#include "pdf_picker.h"
#include "pdf_view_opengl_widget.h"
#include <mupdf/fitz.h>
#include <QPainter>
#include <QFileInfo>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QCryptographicHash>
#include <QDateTime>
#include <QElapsedTimer>
#include <algorithm>
#include <cmath>

extern float DARK_MODE_CONTRAST;

namespace {
QString raster_key(const QString& path, int page) {
    const QFileInfo info(path);
    // Include file metadata so recompilation cannot leave stale previews behind.
    return path + QChar(0x1f) + QString::number(info.size()) + ':' +
        QString::number(info.lastModified().toMSecsSinceEpoch()) + ':' + QString::number(page);
}
QString preview_key(const QString& path, int page, const QString& query) {
    return raster_key(path,page) + QChar(0x1f) + query.simplified();
}
std::mutex raster_mutex;
QCache<QString,QImage> rasters(16 * 1024); // shared, 16 MiB; query changes reuse PDF pixels
}

PdfPickerWorker::PdfPickerWorker(QObject* parent) : QObject(parent), thread_([this]{ run(); }) {}
PdfPickerWorker::~PdfPickerWorker() { stopping_ = true; cancel(); ready_.notify_all(); thread_.join(); }
void PdfPickerWorker::cancel() { ++generation_; std::lock_guard<std::mutex> lock(mutex_); pending_.reset(); cookie_.abort=1; }
void PdfPickerWorker::preview(QString path, int page, QString query) {
    std::lock_guard<std::mutex> lock(mutex_);
    cookie_.abort=1;
    pending_ = Job{path, query.simplified(), {}, page, false, ++generation_}; ready_.notify_one();
}
void PdfPickerWorker::search(QStringList paths, QString query) {
    std::lock_guard<std::mutex> lock(mutex_);
    cookie_.abort=1;
    pending_ = Job{{}, query.simplified(), paths, 0, true, ++generation_}; ready_.notify_one();
}
void PdfPickerWorker::run() {
    fz_context* ctx = fz_new_context(nullptr, nullptr, 16 * 1024 * 1024);
    if (!ctx) return;
    fz_register_document_handlers(ctx);
    const QString cache_dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/library-text-v1";
    QDir().mkpath(cache_dir);
    while (!stopping_) {
        Job job;
        { std::unique_lock<std::mutex> lock(mutex_); ready_.wait(lock,[&]{return stopping_ || pending_.has_value();});
          if (stopping_) break; job = std::move(*pending_); pending_.reset(); cookie_={}; }
        auto cancelled = [&]{ return stopping_ || generation_ != job.generation; };
        if (!job.search) {
            PdfPreviewResult result;
            fz_document* doc = nullptr; fz_page* page = nullptr; fz_pixmap* pix = nullptr;
            fz_device* device=nullptr; fz_stext_page* text=nullptr;
            fz_var(doc); fz_var(page); fz_var(pix); fz_var(device); fz_var(text);
            fz_try(ctx) {
                doc = fz_open_document(ctx, job.path.toUtf8().constData());
                if (fz_needs_password(ctx, doc)) fz_throw(ctx,FZ_ERROR_ARGUMENT,"Password required");
                result.pages = fz_count_pages(ctx, doc);
                if (result.pages < 1) fz_throw(ctx,FZ_ERROR_ARGUMENT,"No pages");
                result.page = std::clamp(job.page, 0, result.pages-1);
                page = fz_load_page(ctx,doc,result.page);
                const auto box = fz_bound_page(ctx,page);
                const float scale = std::min(1200.0f/std::max(1.0f,box.x1-box.x0),1600.0f/std::max(1.0f,box.y1-box.y0));
                const auto matrix=fz_scale(scale,scale);
                const QString key=raster_key(job.path,result.page);
                { std::lock_guard<std::mutex> lock(raster_mutex);
                  if(auto cached=rasters.object(key)) result.image=*cached; }
                const auto pixels=fz_round_rect(fz_transform_rect(box,matrix));
                if(result.image.isNull()) {
                    pix=fz_new_pixmap_with_bbox(ctx,fz_device_rgb(ctx),pixels,nullptr,0);
                    fz_clear_pixmap_with_value(ctx,pix,255);
                    device=fz_new_draw_device(ctx,matrix,pix);
                    fz_run_page(ctx,page,device,fz_identity,&cookie_);
                    fz_close_device(ctx,device);fz_drop_device(ctx,device);device=nullptr;
                    if(!cancelled()) {
                        result.image = QImage(fz_pixmap_samples(ctx,pix),fz_pixmap_width(ctx,pix),fz_pixmap_height(ctx,pix),fz_pixmap_stride(ctx,pix),QImage::Format_RGB888).copy();
                        std::lock_guard<std::mutex> lock(raster_mutex);
                        rasters.insert(key,new QImage(result.image),int(result.image.sizeInBytes()/1024)+1);
                    }
                }
                if (!cancelled() && !job.query.trimmed().isEmpty()) {
                    text=fz_new_stext_page(ctx,box);
                    device=fz_new_stext_device(ctx,text,nullptr);
                    fz_run_page(ctx,page,device,fz_identity,&cookie_);
                    fz_close_device(ctx,device);fz_drop_device(ctx,device);device=nullptr;
                    struct Highlights { PdfPreviewResult* result; float scale; fz_irect pixels; fz_cookie* cookie; } highlights{&result,scale,pixels,&cookie_};
                    fz_search_stext_page_cb(ctx,text,job.query.toUtf8().constData(),
                        [](fz_context*,void* opaque,int count,fz_quad* hits) {
                            auto& h=*static_cast<Highlights*>(opaque);
                            if(h.cookie->abort) return 1;
                            for(int i=0;i<count;++i) {
                                const auto r=fz_rect_from_quad(hits[i]);
                                h.result->hits.append(QRectF(r.x0*h.scale-h.pixels.x0,r.y0*h.scale-h.pixels.y0,
                                    (r.x1-r.x0)*h.scale,(r.y1-r.y0)*h.scale));
                            }
                            return 0;
                        },&highlights);
                }
            } fz_always(ctx) { fz_drop_device(ctx,device);fz_drop_stext_page(ctx,text);fz_drop_pixmap(ctx,pix); fz_drop_page(ctx,page); fz_drop_document(ctx,doc); }
            fz_catch(ctx) { result.error = QString::fromUtf8(fz_caught_message(ctx)); }
            if (!cancelled()) QMetaObject::invokeMethod(this,[this,g=job.generation,result]{ if(generation_==g && preview_ready) preview_ready(result); },Qt::QueuedConnection);
        } else {
            QList<PdfContentHit> batch;
            int processed = 0, total_hits = 0, failures = 0;
            QElapsedTimer progress;progress.start();
            auto publish = [&](bool done) {
                auto hits = std::move(batch); batch.clear();progress.restart();
                QMetaObject::invokeMethod(this,[this,g=job.generation,hits,processed,total=int(job.paths.size()),done,failures]{
                    if(generation_==g && search_ready) search_ready(hits,processed,total,done,failures);
                },Qt::QueuedConnection);
            };
            for (const auto& path : job.paths) {
                if (cancelled() || total_hits >= 2000 || job.query.isEmpty()) break;
                const QFileInfo info(path);
                const QString stamp = QString::number(info.size()) + ':' + QString::number(info.lastModified().toMSecsSinceEpoch());
                const QString cache_path = cache_dir + '/' + QString::fromLatin1(QCryptographicHash::hash(path.toUtf8(),QCryptographicHash::Sha256).toHex());
                QJsonArray pages;
                QFile cache(cache_path);
                bool cached = false;
                if(cache.open(QIODevice::ReadOnly)) {
                    const auto data = QJsonDocument::fromJson(qUncompress(cache.readAll())).object();
                    if(data["stamp"].toString()==stamp) { pages=data["pages"].toArray(); cached=true; }
                }
                if(!cached) {
                    fz_document* doc = nullptr; fz_stext_page* text = nullptr; fz_buffer* buffer = nullptr;
                    fz_page* page=nullptr;fz_device* device=nullptr;
                    fz_var(doc); fz_var(text); fz_var(buffer);fz_var(page);fz_var(device);
                    bool success = true;
                    fz_try(ctx) {
                        doc = fz_open_document(ctx,path.toUtf8().constData());
                        if(fz_needs_password(ctx,doc)) fz_throw(ctx,FZ_ERROR_ARGUMENT,"Password required");
                        const int count=fz_count_pages(ctx,doc);
                        for(int p=0;p<count && !cancelled();++p) {
                            page=fz_load_page(ctx,doc,p);
                            text=fz_new_stext_page(ctx,fz_bound_page(ctx,page));
                            device=fz_new_stext_device(ctx,text,nullptr);
                            fz_run_page(ctx,page,device,fz_identity,&cookie_);
                            fz_close_device(ctx,device);fz_drop_device(ctx,device);device=nullptr;
                            fz_drop_page(ctx,page);page=nullptr;
                            buffer=fz_new_buffer_from_stext_page(ctx,text);
                            unsigned char* bytes=nullptr; const auto length=fz_buffer_storage(ctx,buffer,&bytes);
                            pages.append(QString::fromUtf8(reinterpret_cast<char*>(bytes),int(length)).simplified());
                            fz_drop_buffer(ctx,buffer);buffer=nullptr;fz_drop_stext_page(ctx,text);text=nullptr;
                        }
                    } fz_always(ctx) { fz_drop_device(ctx,device);fz_drop_page(ctx,page);fz_drop_buffer(ctx,buffer);fz_drop_stext_page(ctx,text);fz_drop_document(ctx,doc); }
                    fz_catch(ctx) { success=false; ++failures; }
                    const QFileInfo after(path);
                    if(success && !cancelled() && info.size()==after.size() && info.lastModified()==after.lastModified()) {
                        QSaveFile output(cache_path);
                        if(output.open(QIODevice::WriteOnly)) { output.write(qCompress(QJsonDocument(QJsonObject{{"stamp",stamp},{"pages",pages}}).toJson(QJsonDocument::Compact))); output.commit(); }
                    }
                }
                PdfContentHit hit{path,-1,{}, {}};
                for(int p=0;p<pages.size() && !cancelled();++p) {
                    const auto text=pages[p].toString(); const int at=text.indexOf(job.query,0,Qt::CaseInsensitive);
                    if(at>=0) {
                        if(hit.page<0) { hit.page=p; hit.snippet=(at>60?"…":"")+text.mid(std::max(0,at-60),180); }
                        hit.matching_pages.append(p);
                    }
                }
                if(hit.page>=0 && !cancelled()) { batch.append(std::move(hit)); ++total_hits; }
                ++processed;
                // Batch fast cache hits so large libraries cannot flood the UI event queue.
                if(batch.size()>=64 || (!batch.isEmpty() && progress.elapsed()>=80) || progress.elapsed()>=250) publish(false);
            }
            if(!cancelled()) publish(true);
        }
        if(job.search) fz_empty_store(ctx);
    }
    fz_drop_context(ctx);
}
PdfPickerPreview::PdfPickerPreview(QWidget* parent, PdfViewOpenGLWidget* reader) : QWidget(parent), reader_(reader) {
    setObjectName("pdfPickerPreview"); setMinimumWidth(160); setFocusPolicy(Qt::NoFocus);
    worker_.preview_ready=[this](PdfPreviewResult result) {
        loading_=false;
        result_=std::move(result);page_=result_.page;
        displayed_key_=requested_key_; message_=result_.error;
        remember(displayed_key_,result_);update();schedule_prefetch();
    };
    prefetch_worker_.preview_ready=[this](PdfPreviewResult result) {
        remember(prefetch_key_,result);
        prefetch_key_.clear();prefetch_next();
    };
}
void PdfPickerPreview::remember(const QString& key,const PdfPreviewResult& result) {
    if(!result.image.isNull() && result.error.isEmpty())
        cache_.insert(key,new PdfPreviewResult(result),int((result.image.sizeInBytes()+result.hits.size()*sizeof(QRectF))/1024)+1);
}
bool PdfPickerPreview::is_cached(const QString& path,int page,const QString& query) const {
    return cache_.contains(preview_key(path,page,query));
}
void PdfPickerPreview::select(QString path,int page,QString query,QList<int> matching_pages) {
    query=query.simplified();
    const QString key=preview_key(path,page,query);
    matching_pages_=std::move(matching_pages);
    if(selection_key_==key) return; // model refreshes must not reset preview navigation
    selection_key_=key;
    worker_.cancel();prefetch_worker_.cancel();prefetch_key_.clear();prefetch_queue_.clear();
    path_=path;page_=std::max(0,page);query_=query;
    if(path_.isEmpty()) {
        result_={};displayed_key_.clear();loading_=false;
        message_="Select a PDF to preview";update();return;
    }
    request();
}
void PdfPickerPreview::request() {
    requested_key_=preview_key(path_,page_,query_);
    if(auto cached=cache_.object(requested_key_)) {
        worker_.cancel();result_=*cached;displayed_key_=requested_key_;
        loading_=false;message_=result_.error;update();schedule_prefetch();return;
    }
    // Retain the last image until its replacement arrives instead of flashing blank.
    loading_=true;message_="Loading preview…";
    worker_.preview(path_,page_,query_);update();
}
void PdfPickerPreview::prefetch(QList<PdfPreviewTarget> neighbors) {
    neighbors_=std::move(neighbors);
    if(!loading_) schedule_prefetch();
}
void PdfPickerPreview::schedule_prefetch() {
    if(path_.isEmpty() || loading_) return;
    prefetch_queue_.clear();
    // Nearest files first, in alternating directions, then adjacent preview pages.
    for(const auto& target:neighbors_) prefetch_queue_.push_back(target);
    for(int delta:{1,-1}) if(page_+delta>=0 && page_+delta<result_.pages)
        prefetch_queue_.push_back({path_,page_+delta});
    for(int page:matching_pages_) if(page>page_) { prefetch_queue_.push_back({path_,page});break; }
    if(prefetch_key_.isEmpty()) prefetch_next();
}
void PdfPickerPreview::prefetch_next() {
    while(!prefetch_queue_.empty() && !loading_) {
        auto target=prefetch_queue_.front();prefetch_queue_.pop_front();
        const auto key=preview_key(target.path,target.page,query_);
        if(target.path.isEmpty() || key==requested_key_ || cache_.contains(key)) continue;
        prefetch_key_=key;
        prefetch_worker_.preview(target.path,target.page,query_);return;
    }
}
void PdfPickerPreview::change_page(int delta) {
    if(path_.isEmpty() || result_.pages<=0 || displayed_key_.isEmpty()) return;
    // While replacing a different document, its page count is not known yet.
    if(loading_ && !displayed_key_.startsWith(path_+QChar(0x1f))) return;
    const int next=std::clamp(page_+delta,0,result_.pages-1);
    if(next==page_) return;
    worker_.cancel();prefetch_worker_.cancel();prefetch_key_.clear();page_=next;request();
}
void PdfPickerPreview::change_match(int delta) {
    if(matching_pages_.isEmpty()) return;
    int next=page_;
    if(delta>0) { for(int page:matching_pages_) if(page>page_) {next=page;break;} }
    else { for(auto it=matching_pages_.crbegin();it!=matching_pages_.crend();++it) if(*it<page_) {next=*it;break;} }
    if(next!=page_) { worker_.cancel();prefetch_worker_.cancel();prefetch_key_.clear();page_=next;request(); }
}
const QImage& PdfPickerPreview::display_image() {
    const auto mode = reader_ ? reader_->get_current_color_mode() : PdfViewOpenGLWidget::Normal;
    if (mode == PdfViewOpenGLWidget::Normal || result_.image.isNull()) {
        themed_image_ = {}; themed_source_key_ = 0;
        return result_.image;
    }
    std::array<float, 16> transform{};
    if (mode == PdfViewOpenGLWidget::Custom) reader_->get_custom_color_transform_matrix(transform.data());
    else transform[0] = DARK_MODE_CONTRAST;
    if (themed_source_key_ == result_.image.cacheKey() && themed_mode_ == mode && themed_transform_ == transform)
        return themed_image_;

    // Keep the raw/prefetched rasters shared and theme-independent. Retain only
    // one recolored frame, so repainting never repeats the per-pixel work.
    themed_image_ = result_.image.convertToFormat(QImage::Format_RGB32);
    for (int y = 0; y < themed_image_.height(); ++y) {
        auto row = reinterpret_cast<QRgb*>(themed_image_.scanLine(y));
        for (int x = 0; x < themed_image_.width(); ++x) {
            float rgb[3] = {qRed(row[x]) / 255.0f, qGreen(row[x]) / 255.0f, qBlue(row[x]) / 255.0f};
            float output[3];
            if (mode == PdfViewOpenGLWidget::Custom) {
                for (int c = 0; c < 3; ++c)
                    output[c] = transform[c*4] * rgb[0] + transform[c*4+1] * rgb[1] +
                        transform[c*4+2] * rgb[2] + transform[c*4+3];
            } else {
                // The dark shader inverts brightness, then rotates hue by 180°.
                // That rotation is exactly min + max - channel in RGB space.
                for (float& c : rgb) c = (0.5f - c) * transform[0] + 0.5f;
                const float sum = *std::min_element(rgb, rgb+3) + *std::max_element(rgb, rgb+3);
                for (int c = 0; c < 3; ++c) output[c] = sum - rgb[c];
            }
            auto channel = [&](int c) { return int(std::lround(std::clamp(output[c], 0.0f, 1.0f) * 255.0f)); };
            row[x] = qRgb(channel(0), channel(1), channel(2));
        }
    }
    themed_source_key_ = result_.image.cacheKey(); themed_mode_ = mode; themed_transform_ = transform;
    return themed_image_;
}

void PdfPickerPreview::paintEvent(QPaintEvent*) {
    QColor background = palette().color(QPalette::Window);
    if (reader_) {
        float rgb[3]; reader_->get_empty_background_color(rgb);
        background = QColor::fromRgbF(rgb[0], rgb[1], rgb[2]);
    }
    QPainter p(this);p.fillRect(rect(),background);
    p.setPen(palette().color(QPalette::WindowText));
    if(result_.image.isNull()) {p.drawText(rect().adjusted(12,12,-12,-12),Qt::AlignCenter|Qt::TextWordWrap,message_);return;}
    const QRect area=rect().adjusted(8,8,-8,-30);
    const QSize size=result_.image.size().scaled(area.size(),Qt::KeepAspectRatio);
    const QRect target(QPoint(area.center().x()-size.width()/2,area.center().y()-size.height()/2),size);
    p.setRenderHint(QPainter::SmoothPixmapTransform);p.drawImage(target,display_image());
    const double scale=double(size.width())/result_.image.width();
    p.setPen(Qt::NoPen);p.setBrush(QColor(240,180,40,100));
    for(const auto& hit:result_.hits) p.drawRect(QRectF(target.x()+hit.x()*scale,target.y()+hit.y()*scale,hit.width()*scale,hit.height()*scale));
    p.setPen(palette().color(QPalette::WindowText));
    p.drawText(QRect(0,height()-26,width(),24),Qt::AlignCenter,loading_ ? QString("Loading %1…").arg(QFileInfo(path_).fileName()) :
        QString("Page %1 / %2  ·  Ctrl+L / Ctrl+D%3").arg(result_.page+1).arg(result_.pages)
            .arg(matching_pages_.isEmpty() ? QString() : "  ·  [ / ] matches"));
}
