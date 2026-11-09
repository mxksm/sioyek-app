#define main sioyek_application_main
#include "../pdf_viewer/main.cpp"
#undef main
#include "../pdf_viewer/blank_canvas.h"
#include "../pdf_viewer/pdf_picker.h"
#include <QTemporaryDir>
#include <QPainter>
#include <QEventLoop>
#include <QElapsedTimer>
#include <stdexcept>
extern std::wstring PDF_LIBRARY_PATH;
static int result=1;
static QString root;
static void check(bool ok,const char* what){if(!ok)throw std::runtime_error(what);}
static void settle(int ms=80){QEventLoop loop;QTimer::singleShot(ms,&loop,&QEventLoop::quit);loop.exec();}
static void wait(std::function<bool()> done){QElapsedTimer t;t.start();while(!done()&&t.elapsed()<15000)settle();check(done(),"asynchronous request completed");}
static void pdf(QString path,QString text,QString first_page="First page overview"){
    QByteArray bytes("%PDF-1.4\n");std::vector<int> offsets{0};
    auto object=[&](QByteArray body){offsets.push_back(bytes.size());bytes+=QByteArray::number(int(offsets.size())-1)+" 0 obj\n"+body+"\nendobj\n";};
    object("<< /Type /Catalog /Pages 2 0 R >>");
    object("<< /Type /Pages /Kids [3 0 R 4 0 R] /Count 2 >>");
    for(int p=0;p<2;++p)object("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 600 800] /Resources << /Font << /F1 5 0 R >> >> /Contents "+QByteArray::number(6+p)+" 0 R >>");
    object("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>");
    for(auto content:{first_page,text}) {
        const QByteArray fontSize=content.contains('\n') ? "10" : "22";
        QByteArray drawing="BT /F1 "+fontSize+" Tf 30 730 Td ("+content.toUtf8().replace("\n",") Tj 0 -20 Td (")+") Tj ET\n";
        object("<< /Length "+QByteArray::number(drawing.size())+" >>\nstream\n"+drawing+"endstream");
    }
    object("<< /Title (Abstract algebra notes) >>");
    const int xref=bytes.size();bytes+="xref\n0 9\n0000000000 65535 f \n";
    for(size_t i=1;i<offsets.size();++i)bytes+=QByteArray::number(offsets[i]).rightJustified(10,'0')+" 00000 n \n";
    bytes+="trailer\n<< /Size 9 /Root 1 0 R /Info 8 0 R >>\nstartxref\n"+QByteArray::number(xref)+"\n%%EOF\n";
    QFile file(path);check(file.open(QIODevice::WriteOnly),"create PDF fixture");file.write(bytes);
}
static void run(){try{
    auto pane=windows.front();QApplication::setActiveWindow(pane);pane->resize(1200,850);settle();
    const auto first=root+"/algebra.pdf",second=root+"/geometry.pdf";
    pdf(first,"The spectral theorem is useful");pdf(second,"Euclidean geometry");
    PdfPickerWorker worker;QList<PdfContentHit> hits;bool done=false;
    worker.search_ready=[&](auto batch,int,int,bool finished,int){hits.append(batch);done=finished;};
    worker.search({first,second},"spectral theorem");wait([&]{return done;});
    check(hits.size()==1 && hits[0].path==first && hits[0].page==1,"contents found on second page with unrelated filename");
    done=false;hits.clear();worker.search({first,second},"geometry");worker.search({first,second},"spectral theorem");wait([&]{return done;});
    check(hits.size()==1 && hits[0].path==first,"stale cancelled query results are ignored");
    pdf(first,"Replacement text after recompilation");done=false;hits.clear();worker.search({first},"Replacement text");wait([&]{return done;});
    check(hits.size()==1,"text cache invalidates after PDF replacement");
    PdfPreviewResult preview;bool rendered=false;worker.preview_ready=[&](auto value){preview=value;rendered=true;};
    worker.preview(first,1,"Replacement text");wait([&]{return rendered;});
    check(!preview.image.isNull() && !preview.hits.empty() && preview.pages==2,"preview renders matching page and highlight rectangles");
    // Many matching pages must remain one file in the results.
    const auto grouped=root+"/multipage.pdf";
    pdf(grouped,"Matching phrase appears twice: Matching phrase","Matching phrase on page one");
    done=false;hits.clear();worker.search({grouped},"Matching phrase");wait([&]{return done;});
    check(hits.size()==1 && hits[0].matching_pages==QList<int>({0,1}),"one content result retains every matching page of a PDF");
    rendered=false;worker.preview(grouped,1,"Matching phrase");wait([&]{return rendered;});
    check(preview.hits.size()==2,"preview highlights all occurrences on its page");
    {
        PdfPickerPreview cached(pane);cached.resize(500,700);
        cached.select(first,1,"Replacement text");wait([&]{return !cached.is_loading();});
        cached.prefetch({{second,0},{grouped,0}});
        wait([&]{return cached.is_cached(second,0,"Replacement text") && cached.is_cached(grouped,0,"Replacement text");});
        QElapsedTimer timer;timer.start();cached.select(second,0,"Replacement text");
        check(!cached.is_loading() && cached.has_preview(),"prefetched neighbor displays synchronously without a loading frame");
        qInfo("Warm preview switch: %lld ms",timer.elapsed());
        cached.select(first,1,"Replacement text");
        check(!cached.is_loading(),"backtracking reuses the cached preview immediately");
        cached.select(first,0,"an uncached phrase");
        check(cached.has_preview(),"cold selection retains the previous image rather than flashing blank");
        wait([&]{return !cached.is_loading();});
        pdf(second,"A changed document with fresh text");
        check(!cached.is_cached(second,0,"Replacement text"),"PDF replacement invalidates prefetched results");
        check(cached.cached_kib()<=48*1024,"preview cache stays within its memory budget");
    }
    const auto dense=root+"/dense.pdf";
    pdf(dense,(QString("needle ").repeated(20)+"\n").repeated(15));
    rendered=false;worker.preview(dense,1,"needle");wait([&]{return rendered;});
    check(preview.hits.size()==300,"all matches are highlighted beyond the old 256-quad cap");
    auto picker=new LibrarySelector(false,[pane](QString path){pane->open_document(path.toStdWString());},pane,root,true);
    pane->set_current_widget(picker);pane->show_current_widget();
    auto edit=picker->findChild<QLineEdit*>();edit->setText("Replacement text");
    wait([&]{return picker->get_view()->model()->rowCount()>0;});settle(800);
    picker->grab().save("/tmp/sioyek-library-content.png");
    auto previewWidget=picker->findChild<QWidget*>("pdfPickerPreview");
    qInfo("picker width=%d parent=%d preview=%p",picker->width(),picker->parentWidget()->width(),previewWidget);
    check(previewWidget && previewWidget->width()>100 && previewWidget->x()>picker->get_view()->x(),"content picker has side-by-side PDF preview");
    picker->grab().save("/tmp/sioyek-library-content.png");
    QKeyEvent enter(QEvent::KeyPress,Qt::Key_Return,Qt::NoModifier);QApplication::sendEvent(edit,&enter);settle();
    check(pane->doc()->get_path()==first.toStdWString() && pane->main_document_view->get_center_page_number()==1,"content match opens selected page");
    pane->pop_current_widget();pane->setFocus();
    auto groups=new LibrarySelector(false,[pane](QString path){pane->open_document(path.toStdWString());},pane,root,true);
    pane->set_current_widget(groups);pane->show_current_widget();
    auto groupEdit=groups->findChild<QLineEdit*>();groupEdit->setText("Matching phrase");
    wait([&]{return groups->get_view()->model()->rowCount()>0;});settle(500);
    check(groups->get_view()->model()->rowCount()==1,"content picker lists the matching file once, not each matching page");
    auto groupPreview=static_cast<PdfPickerPreview*>(groups->findChild<QWidget*>("pdfPickerPreview"));
    wait([&]{return !groupPreview->is_loading();});
    auto previewKey=[&](int code,Qt::KeyboardModifiers modifiers,const QString& text=QString()) {
        QKeyEvent event(QEvent::KeyPress,code,modifiers,text);QApplication::sendEvent(groupEdit,&event);settle();
    };
    previewKey(Qt::Key_BracketRight,Qt::NoModifier,"]");
    check(groupPreview->current_page()==1,"right bracket advances to the next matching page");
    previewKey(Qt::Key_L,Qt::MetaModifier);
    check(groupPreview->current_page()==0,"physical Control-L goes back one preview page on macOS");
    previewKey(Qt::Key_D,Qt::MetaModifier);
    check(groupPreview->current_page()==1,"physical Control-D goes forward one preview page on macOS");
    groups->grab().save("/tmp/sioyek-grouped-preview.png");
    previewKey(Qt::Key_Return,Qt::NoModifier);
    check(pane->doc()->get_path()==grouped.toStdWString() && pane->main_document_view->get_center_page_number()==1,"Enter opens the page currently shown in grouped search preview");
    pane->pop_current_widget();pane->setFocus();

    auto names=new LibrarySelector(false,[](QString){},pane,root);
    pane->set_current_widget(names);pane->show_current_widget();
    names->findChild<QLineEdit*>()->setText("Abstract algebra");
    wait([&]{return names->get_view()->model()->rowCount()>0;});settle(500);names->grab().save("/tmp/sioyek-library-names.png");pane->pop_current_widget();
    pane->open_document((root+"/blank.pdf").toStdWString());
    auto doc=pane->doc();doc->delete_all_drawings();
    FreehandDrawing a;a.type='r';a.creattion_time=QDateTime::currentDateTime();a.points={{{0,100},5},{{30,120},5},{{60,100},5}};
    doc->add_freehand_drawing(a);auto b=a;b.points={{{200,100},5},{{230,120},5}};b.creattion_time=b.creattion_time.addMSecs(1);doc->add_freehand_drawing(b);
    QPolygonF polygon;polygon<<QPointF(-10,90)<<QPointF(70,90)<<QPointF(70,130)<<QPointF(-10,130);
    auto selected=doc->select_ink(polygon,nullptr);check(selected.size()==1,"lasso selects intersecting ink without selecting unrelated strokes");
    std::vector<InkSelectionItem> moved;check(doc->transform_ink(selected,QTransform::fromTranslate(50,30),&moved),"moving selected ink succeeds");
    check(moved[0].drawing.points[0].pos.x==50 && moved[0].drawing.points[0].pos.y==130,"translation updates coordinates");
    QTransform scale;scale.translate(50,130);scale.scale(2,2);scale.translate(-50,-130);
    check(doc->transform_ink(moved,scale,&selected),"scaling selected ink succeeds");
    check(selected[0].drawing.points[0].thickness==10,"scaling preserves proportional pen width");
    doc->undo_freehand_drawing();doc->undo_freehand_drawing();
    check(doc->get_page_drawings(0)[0].points[0].pos.x==0,"undo restores original ink after scale and move");
    doc->redo_freehand_drawing();doc->redo_freehand_drawing();
    check(doc->get_page_drawings(0).back().points[0].thickness==10,"redo restores transformations");
    doc->persist_drawings();doc->load_drawings();check(doc->get_page_drawings(0).back().points[0].thickness==10,"transformed drawings survive save/reload");
    doc->undo_freehand_drawing();
    check(doc->get_page_drawings(0).size()==1 && doc->get_page_drawings(0)[0].creattion_time==a.creattion_time,"reloaded ink undo follows creation time after lasso reordering");
    doc->redo_freehand_drawing();
    auto cross=doc->select_ink(QPolygonF(QRectF(40,120,170,100)),nullptr);
    check(cross.size()==1 && doc->transform_ink(cross,QTransform::fromTranslate(0,850),&moved) && moved[0].page==1,"lasso moves ink to another page");
    doc->undo_freehand_drawing();check(doc->get_page_drawings(1).empty(),"undo cross-page transform restores both pages");
    check(!doc->transform_ink(moved,QTransform::fromTranslate(1,1)),"stale selection cannot move different ink");
    pane->toggle_ink_lasso();check(pane->lasso_enabled_,"lasso mode enabled");
    pane->ink_lasso_->press(QPointF(40,120));pane->ink_lasso_->move(QPointF(200,120));pane->ink_lasso_->move(QPointF(200,200));pane->ink_lasso_->move(QPointF(40,200));pane->ink_lasso_->release(QPointF(40,120));
    check(pane->ink_lasso_->has_selection(),"interactive lasso creates a selection");
    auto mouse=[&](QEvent::Type type,QPointF position,Qt::MouseButton button,Qt::MouseButtons buttons) {
        auto point=AbsoluteDocumentPos{float(position.x()),float(position.y())}.to_window(pane->main_document_view);
        QPointF local(point.x,point.y);auto global=QPointF(pane->central_widget->mapToGlobal(QPoint(0,0)))+local;
        QMouseEvent event(type,local,global,button,buttons,Qt::NoModifier);QApplication::sendEvent(pane->central_widget,&event);
    };
    mouse(QEvent::MouseButtonPress,{110,150},Qt::LeftButton,Qt::LeftButton);
    mouse(QEvent::MouseMove,{120,160},Qt::NoButton,Qt::LeftButton);
    mouse(QEvent::MouseButtonRelease,{120,160},Qt::LeftButton,Qt::NoButton);
    check(std::abs(doc->get_page_drawings(0).back().points[0].pos.x-60)<2,"mouse drag moves selected ink through pane event routing");
    pane->grab().save("/tmp/sioyek-lasso.png");pane->stop_ink_lasso();
    pane->toggle_split();settle();auto right=pane->split_pane_.data();right->open_document((root+"/blank.pdf").toStdWString());
    pane->main_document_view->goto_page(0);right->main_document_view->goto_page(0);pane->toggle_linked_navigation();
    auto command=[&](MainWidget* target,const char* name){auto c=target->command_manager->get_command_with_name(target,name);c->set_num_repeats(0);target->advance_command(std::move(c));settle();};
    command(pane,"next_page");check(pane->main_document_view->get_center_page_number()==1 && right->main_document_view->get_center_page_number()==1,"linked page movement applies to both panes");
    command(right,"goto_end");check(pane->main_document_view->get_center_page_number()==2 && right->main_document_view->get_center_page_number()==2,"linked G works from either pane");
    command(right,"goto_beginning");check(pane->main_document_view->get_center_page_number()==0 && right->main_document_view->get_center_page_number()==0,"linked gg goes to both first pages");
    auto macro=right->command_manager->create_macro_command(right,"linked_test",L"next_page;next_page");
    macro->set_num_repeats(0);right->advance_command(std::move(macro));settle();
    check(pane->main_document_view->get_center_page_number()==2 && right->main_document_view->get_center_page_number()==2,"navigation macros mirror each primitive exactly once");
    command(right,"goto_beginning");
    pane->toggle_linked_navigation();command(right,"next_page");check(pane->main_document_view->get_center_page_number()==0,"unlinked navigation leaves other PDF alone");
    pane->toggle_split();pane->setFocus();
    PDF_LIBRARY_PATH=root.toStdWString();
    auto key=[&](int code,const QString& text){QKeyEvent event(QEvent::KeyPress,code,Qt::NoModifier,text);pane->key_event(false,&event,false);};
    key(Qt::Key_Space," ");key(Qt::Key_F,"f");key(Qt::Key_F,"f");settle();
    check(!pane->current_widget_stack.empty() && pane->current_widget_stack.back()->objectName()=="documentPicker","Space ff opens the library picker");pane->handle_escape();
    key(Qt::Key_Space," ");key(Qt::Key_F,"f");key(Qt::Key_W,"w");settle();
    check(!pane->current_widget_stack.empty() && pane->current_widget_stack.back()->findChild<QLabel*>()->text()=="Search PDF contents","Space fw opens content search");pane->handle_escape();
    qInfo("PASS: grouped file results, all match highlights, prefetched previews, Control-L/D, contents/cache/cancellation, highlighted PDF preview, lasso move/scale/undo/save and linked split navigation");result=0;QCoreApplication::exit(0);
}catch(const std::exception& e){qCritical("FAIL: %s",e.what());QCoreApplication::exit(1);}}
static void schedule(){QTimer::singleShot(1200,qApp,run);}Q_COREAPP_STARTUP_FUNCTION(schedule)
int main(int argc,char** argv){QStandardPaths::setTestModeEnabled(true);QTemporaryDir dir;root=dir.path();auto path=root+"/blank.pdf";QFile file(path);file.open(QIODevice::WriteOnly);file.write(blank_canvas_pdf(3));file.close();QByteArray first=path.toUtf8();char opt[]="--instance-name",name[]="sioyek-library-tools-test";char* args[]={argv[0],first.data(),opt,name};sioyek_application_main(4,args);return result;}
