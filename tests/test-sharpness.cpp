#define main sioyek_application_main
#include "../pdf_viewer/main.cpp"
#undef main
#include "../pdf_viewer/blank_canvas.h"
#include <QTemporaryDir>
#include <QPdfWriter>
#include <QEventLoop>
#include <QOpenGLFunctions>
#include <stdexcept>
extern bool LINEAR_TEXTURE_FILTERING;
extern bool SLICED_RENDERING;
static int result=1;
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static void settle(int ms=40){QEventLoop loop;QTimer::singleShot(ms,&loop,&QEventLoop::quit);loop.exec();}
static void bind(GLuint texture){QOpenGLContext::currentContext()->functions()->glBindTexture(GL_TEXTURE_2D,texture);}
static GLuint request(PdfRenderer& renderer, MainWidget* owner,const std::wstring& path,float zoom,float scale,bool wait=true){
 GLuint texture=0;for(int i=0;i<(wait?100:1);++i){owner->opengl_widget->makeCurrent();int w=0,h=0;
 texture=renderer.find_rendered_page(path,0,true,-1,1,1,zoom,scale,&w,&h);GLint tw=0;if(texture){bind(texture);glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_WIDTH,&tw);}owner->opengl_widget->doneCurrent();
 if(texture && (!wait || tw==w))return texture;settle();}return texture;
}
static void run(){try{
 auto owner=windows.front()->tab_owner();
 const QString target=qEnvironmentVariable("SIOYEK_TEST_SCREEN");
 if(!target.isEmpty()){
  QScreen* selected=nullptr;
  for(auto screen:QGuiApplication::screens())if(screen->name().contains(target,Qt::CaseInsensitive))selected=screen;
  check(selected,"requested test screen connected");
  owner->windowHandle()->setScreen(selected);
  owner->setGeometry(selected->availableGeometry().adjusted(40,40,-40,-40));settle(600);
  check(owner->screen()==selected,"test window moved to requested display");
  qInfo()<<"Testing screen"<<selected->name()<<selected->geometry()<<"DPR"<<owner->opengl_widget->devicePixelRatioF();
 }
 QTemporaryDir dir;auto file=dir.filePath("sharpness.pdf");
 {QPdfWriter writer(file);writer.setResolution(72);writer.setPageSize(QPageSize(QSizeF(612,792),QPageSize::Point));writer.setPageMargins({});QPainter p(&writer);p.fillRect(0,0,612,792,Qt::white);p.setPen(Qt::black);p.setFont(QFont("Times New Roman",12));for(int y=70;y<740;y+=28)p.drawText(50,y,"Theorem 1. For every x in V, rank(A) + nullity(A) = n.");}
 owner->open_document(file.toStdWString());owner->set_light_mode();SLICED_RENDERING=false;LINEAR_TEXTURE_FILTERING=false;
 owner->dv()->set_zoom_level(0.827f,true);owner->dv()->set_offset_y(396);owner->dv()->set_offset_x(0);owner->validate_render();settle(500);
 check(fz_text_aa_level(owner->mupdf_context)==8,"maximum text antialiasing already active");
 const DocumentRect rect(PagelessDocumentRect({0,0,612,792}),0);
 for(float scale:{1.f,1.25f,1.5f,2.f}){
  auto r=owner->dv()->document_to_window_rect_pixel_perfect(rect,1013/scale,1311/scale,false,scale);
  check(abs((r.x1-r.x0)*std::round(owner->dv()->get_view_width()*scale)/2-1013)<0.002,"odd physical width preserved");
  check(abs((r.y0-r.y1)*std::round(owner->dv()->get_view_height()*scale)/2-1311)<0.002,"odd physical height preserved");
  float x=(r.x0+1)*std::round(owner->dv()->get_view_width()*scale)/2;
  check(abs(x-std::round(x))<0.002,"origin aligned to physical pixels");
 }
 bool quit=false;PdfRenderer renderer(1,&quit,owner->mupdf_context);renderer.set_num_cached_pages(20);renderer.start_threads();
 auto low=request(renderer,owner,file.toStdWString(),0.5,1),medium=request(renderer,owner,file.toStdWString(),1,1),high=request(renderer,owner,file.toStdWString(),2,1);
 check(low && medium && high && low!=medium && medium!=high,"multiple exact cache resolutions");
 check(request(renderer,owner,file.toStdWString(),1.6,1,false)==high,"fallback picks closest scale instead of smallest");
 owner->opengl_widget->makeCurrent();bind(high);GLint filter=0;glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,&filter);check(filter==GL_LINEAR,"temporary scaled texture uses smooth filtering");owner->opengl_widget->doneCurrent();
 check(request(renderer,owner,file.toStdWString(),2,1)==high,"exact render reused");owner->opengl_widget->makeCurrent();bind(high);glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,&filter);check(filter==GL_NEAREST,"native size restores crisp filtering");owner->opengl_widget->doneCurrent();
 quit=true;renderer.join_threads();owner->opengl_widget->makeCurrent();renderer.free_all_resources_for_document(file.toStdWString());owner->opengl_widget->doneCurrent();
 const float dpr=owner->opengl_widget->devicePixelRatioF();auto exact=request(*owner->pdf_renderer,owner,file.toStdWString(),0.827f,dpr);check(exact,"native display texture ready");
 owner->invalidate_render();owner->validate_render();settle(200);auto image=owner->opengl_widget->grabFramebuffer();
 owner->opengl_widget->makeCurrent();bind(exact);GLint w=0,h=0;glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_WIDTH,&w);glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_HEIGHT,&h);
 std::vector<unsigned char> pixels(w*h*3);glPixelStorei(GL_PACK_ALIGNMENT,1);glGetTexImage(GL_TEXTURE_2D,0,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());glPixelStorei(GL_PACK_ALIGNMENT,4);owner->opengl_widget->doneCurrent();
 auto location=owner->dv()->document_to_window_rect_pixel_perfect(rect,w/dpr,h/dpr,false,dpr);
 int left=std::lround((location.x0+1)*image.width()/2),top=std::lround((1-location.y0)*image.height()/2);
 int mismatches=0,samples=0,ink=0;for(int y=80;y<h-80;++y)for(int x=70;x<w-70;++x){if(!image.rect().contains(left+x,top+y))continue;int source=pixels[(y*w+x)*3];auto actual=image.pixelColor(left+x,top+y).red();++samples;if(source<230)++ink;if(abs(source-actual)>2)++mismatches;}
 qInfo("Pixel comparison: %d mismatches / %d samples, %d ink pixels, DPR %.2f, texture %dx%d",mismatches,samples,ink,dpr,w,h);
 image.save("/tmp/sioyek-sharpness.png");check(ink>1000,"comparison includes real glyphs");check(mismatches<samples/1000,"native page pixels match rendered glyphs without resampling");
 qInfo("PASS: physical pixel dimensions, fractional scale alignment, closest cache selection, temporary filtering, native framebuffer fidelity, 8-bit antialiasing");result=0;QCoreApplication::exit(0);
 }catch(const std::exception& e){qCritical("FAIL: %s",e.what());QCoreApplication::exit(1);}}
static void schedule(){QTimer::singleShot(1200,qApp,run);}Q_COREAPP_STARTUP_FUNCTION(schedule)
int main(int,char** argv){QStandardPaths::setTestModeEnabled(true);QTemporaryDir dir;auto path=dir.filePath("blank.pdf");QFile f(path);f.open(QIODevice::WriteOnly);f.write(blank_canvas_pdf(1));f.close();auto bytes=path.toUtf8();char option[]="--instance-name",name[]="sioyek-sharpness-test";char* args[]={argv[0],bytes.data(),option,name};sioyek_application_main(4,args);return result;}
