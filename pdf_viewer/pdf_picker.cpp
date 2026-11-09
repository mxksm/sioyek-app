#include "pdf_picker.h"
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

PdfPickerWorker::PdfPickerWorker(QObject* parent) : QObject(parent), thread_([this]{ run(); }) {}
PdfPickerWorker::~PdfPickerWorker() { stopping_ = true; cancel(); ready_.notify_all(); thread_.join(); }
void PdfPickerWorker::cancel() { ++generation_; std::lock_guard<std::mutex> lock(mutex_); pending_.reset(); cookie_.abort=1; }
void PdfPickerWorker::preview(QString path, int page, QString query) {
    std::lock_guard<std::mutex> lock(mutex_);
    cookie_.abort=1;
    pending_ = Job{path, query, {}, page, false, ++generation_}; ready_.notify_one();
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
                pix=fz_new_pixmap_with_bbox(ctx,fz_device_rgb(ctx),fz_round_rect(fz_transform_rect(box,matrix)),nullptr,0);
                fz_clear_pixmap_with_value(ctx,pix,255);
                device=fz_new_draw_device(ctx,matrix,pix);
                fz_run_page(ctx,page,device,fz_identity,&cookie_);
                fz_close_device(ctx,device);fz_drop_device(ctx,device);device=nullptr;
                result.image = QImage(fz_pixmap_samples(ctx,pix),fz_pixmap_width(ctx,pix),fz_pixmap_height(ctx,pix),fz_pixmap_stride(ctx,pix),QImage::Format_RGB888).copy();
                if (!cancelled() && !job.query.trimmed().isEmpty()) {
                    text=fz_new_stext_page(ctx,box);
                    device=fz_new_stext_device(ctx,text,nullptr);
                    fz_run_page(ctx,page,device,fz_identity,&cookie_);
                    fz_close_device(ctx,device);fz_drop_device(ctx,device);device=nullptr;
                    fz_quad hits[256]; int marks[256];
                    const int n = fz_search_stext_page(ctx,text,job.query.toUtf8().constData(),marks,hits,256);
                    for(int i=0;i<n;++i) {
                        const auto r = fz_rect_from_quad(hits[i]);
                        result.hits.append(QRectF(r.x0*scale-fz_pixmap_x(ctx,pix),r.y0*scale-fz_pixmap_y(ctx,pix),(r.x1-r.x0)*scale,(r.y1-r.y0)*scale));
                    }
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
                for(int p=0;p<pages.size() && !cancelled() && total_hits<2000;++p) {
                    const auto text=pages[p].toString(); const int at=text.indexOf(job.query,0,Qt::CaseInsensitive);
                    if(at>=0) { batch.append({path,p,(at>60?"…":"")+text.mid(std::max(0,at-60),180)}); ++total_hits; }
                }
                ++processed;
                // Batch fast cache hits so large libraries cannot flood the UI event queue.
                if(batch.size()>=64 || (!batch.isEmpty() && progress.elapsed()>=80) || progress.elapsed()>=250) publish(false);
            }
            if(!cancelled()) publish(true);
        }
        fz_empty_store(ctx);
    }
    fz_drop_context(ctx);
}
PdfPickerPreview::PdfPickerPreview(QWidget* parent) : QWidget(parent) {
    setObjectName("pdfPickerPreview"); setMinimumWidth(160); setFocusPolicy(Qt::NoFocus);
    debounce_.setSingleShot(true);debounce_.setInterval(110);
    connect(&debounce_,&QTimer::timeout,this,[this]{request();});
    worker_.preview_ready=[this](PdfPreviewResult result){ result_=std::move(result);page_=result_.page;message_=result_.error;update(); };
}
void PdfPickerPreview::select(QString path,int page,QString query) {
    if(path_==path && selection_page_==page && query_==query) return;
    worker_.cancel();path_=path;selection_page_=page;page_=std::max(0,page);query_=query;result_={};
    message_=path.isEmpty()?"Select a PDF to preview":"Loading preview…";update();debounce_.start();
}
void PdfPickerPreview::request() { if(!path_.isEmpty()) worker_.preview(path_,page_,query_); }
void PdfPickerPreview::change_page(int delta) {
    if (result_.pages <= 0) return;
    const int next = std::clamp(page_+delta,0,result_.pages-1);
    if (next==page_) return;
    worker_.cancel(); page_=next; request();
}
void PdfPickerPreview::paintEvent(QPaintEvent*) {
    QPainter p(this);p.fillRect(rect(),palette().color(QPalette::Base));
    p.setPen(palette().color(QPalette::Text));
    if(result_.image.isNull()) {p.drawText(rect().adjusted(12,12,-12,-12),Qt::AlignCenter|Qt::TextWordWrap,message_);return;}
    const QRect area=rect().adjusted(8,8,-8,-30);
    const QSize size=result_.image.size().scaled(area.size(),Qt::KeepAspectRatio);
    const QRect target(QPoint(area.center().x()-size.width()/2,area.center().y()-size.height()/2),size);
    p.setRenderHint(QPainter::SmoothPixmapTransform);p.drawImage(target,result_.image);
    const double scale=double(size.width())/result_.image.width();
    p.setPen(Qt::NoPen);p.setBrush(QColor(240,180,40,100));
    for(const auto& hit:result_.hits) p.drawRect(QRectF(target.x()+hit.x()*scale,target.y()+hit.y()*scale,hit.width()*scale,hit.height()*scale));
    p.setPen(palette().color(QPalette::Text));
    p.drawText(QRect(0,height()-26,width(),24),Qt::AlignCenter,QString("Page %1 / %2  ·  Ctrl+U / Ctrl+D").arg(page_+1).arg(result_.pages));
}
