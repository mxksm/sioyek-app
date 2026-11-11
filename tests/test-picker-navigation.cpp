#define main sioyek_application_main
#include "../pdf_viewer/main.cpp"
#undef main
#include "../pdf_viewer/pdf_picker.h"
#include <QTemporaryDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QEventLoop>
#include <stdexcept>

static QString root;
static int result = 1;
static void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
static void settle(int ms) { QEventLoop loop; QTimer::singleShot(ms, &loop, &QEventLoop::quit); loop.exec(); }
static QByteArray jpx_pdf() {
    QFile image("tests/fixtures/preview-gradient.jp2");
    check(image.open(QIODevice::ReadOnly), "load JPEG 2000 fixture");
    const auto pixels=image.readAll();
    QByteArray bytes("%PDF-1.4\n");
    std::vector<int> offsets{0};
    auto object=[&](const QByteArray& body) {
        offsets.push_back(bytes.size());
        bytes+=QByteArray::number(offsets.size()-1)+" 0 obj\n"+body+"\nendobj\n";
    };
    object("<< /Type /Catalog /Pages 2 0 R >>");
    object("<< /Type /Pages /Kids [3 0 R] /Count 1 >>");
    object("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 512 512] /Resources << /XObject << /Im1 4 0 R >> >> /Contents 5 0 R >>");
    object("<< /Type /XObject /Subtype /Image /Width 512 /Height 512 /ColorSpace /DeviceRGB /BitsPerComponent 8 /Filter /JPXDecode /Length "+QByteArray::number(pixels.size())+" >>\nstream\n"+pixels+"\nendstream");
    const QByteArray paint="q 512 0 0 512 0 0 cm /Im1 Do Q\n";
    object("<< /Length "+QByteArray::number(paint.size())+" >>\nstream\n"+paint+"endstream");
    const int xref=bytes.size();bytes+="xref\n0 6\n0000000000 65535 f \n";
    for(size_t i=1;i<offsets.size();++i) bytes+=QByteArray::number(offsets[i]).rightJustified(10,'0')+" 00000 n \n";
    bytes+="trailer\n<< /Size 6 /Root 1 0 R >>\nstartxref\n"+QByteArray::number(xref)+"\n%%EOF\n";
    return bytes;
}
static void run() {
    try {
        auto pane = windows.front();
        std::vector<DocumentPickerEntry> entries;
        const auto fixture = jpx_pdf();
        for (int i=0; i<96; ++i) {
            QString path=root+QString("/paper-%1.pdf").arg(i,3,10,QChar('0'));
            QFile file(path); check(file.open(QIODevice::WriteOnly), "create navigation fixture");
            file.write(fixture); file.close();
            entries.push_back({path, {}, {}, path});
        }
        const auto real_root = qEnvironmentVariable("SIOYEK_STRESS_PDF_ROOT");
        if (!real_root.isEmpty()) {
            QDirIterator files(real_root, {"*.pdf"}, QDir::Files, QDirIterator::Subdirectories);
            while (files.hasNext() && entries.size()<176) {
                auto path=files.next(); entries.push_back({path, {}, {}, path});
            }
        }
        pane->open_document(entries.front().path.toStdWString());
        auto send = [](DocumentPicker* picker, int code, const QString& text, bool repeat=false) {
            auto edit=picker->findChild<QLineEdit*>();
            QKeyEvent press(QEvent::KeyPress,code,Qt::NoModifier,text,repeat);
            QApplication::sendEvent(edit,&press);
        };
        const int delay = qEnvironmentVariableIntValue("SIOYEK_STRESS_DELAY_MS");
        for (int round=0;round<2;++round) {
            auto picker=new DocumentPicker("Recent documents",true,pane,entries,[](QString){});
            pane->set_current_widget(picker); pane->show_current_widget();
            for (int i=0;i<int(entries.size())+20;++i) {
                send(picker,Qt::Key_J,"j",i!=0);
                settle(delay > 0 ? delay : 8);
                check(picker->get_view()->currentIndex().isValid(),"navigation keeps a valid selection");
            }
            check(picker->get_view()->currentIndex().row()==int(entries.size())-1,"j stops at final result");
            auto preview=dynamic_cast<PdfPickerPreview*>(picker->findChild<QWidget*>("pdfPickerPreview"));
            check(preview, "picker contains a preview");
            QElapsedTimer deadline;deadline.start();
            while(preview->is_loading() && deadline.elapsed()<15000) settle(20);
            check(!preview->is_loading() && preview->has_preview(), "final JPEG 2000 preview renders successfully");
            for(int i=0;i<30;++i) { send(picker,Qt::Key_K,"k",true);settle(8); }
            // Destroy while foreground and speculative preview jobs may still be running.
            pane->pop_all_widgets(); settle(20);
            qInfo("Completed JPEG 2000 navigation pass %d",round+1);
        }
        qInfo("PASS: repeated j/k navigation, asynchronous title refresh, cache turnover and picker destruction");
        result=0;QCoreApplication::exit(0);
    } catch(const std::exception& e) { qCritical("FAIL: %s",e.what());QCoreApplication::exit(1); }
}
static void schedule(){ QTimer::singleShot(1000,qApp,run); }
Q_COREAPP_STARTUP_FUNCTION(schedule)
int main(int, char** argv) {
    QStandardPaths::setTestModeEnabled(true);
    QTemporaryDir temp;root=temp.path();
    char option[]="--instance-name", name[]="sioyek-picker-navigation-test";
    char* args[]={argv[0],argv[1],option,name};
    sioyek_application_main(4,args);return result;
}
