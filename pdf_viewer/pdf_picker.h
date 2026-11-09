#pragma once
#include <QObject>
#include <mupdf/fitz.h>
#include <QImage>
#include <QRectF>
#include <QStringList>
#include <QWidget>
#include <QTimer>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <optional>

struct PdfContentHit { QString path; int page; QString snippet; };
struct PdfPreviewResult { QImage image; QList<QRectF> hits; int page = 0, pages = 0; QString error; };

// Each worker owns its MuPDF context. Only images and strings cross to the UI.
class PdfPickerWorker : public QObject {
    struct Job { QString path, query; QStringList paths; int page = 0; bool search = false; quint64 generation = 0; };
    std::mutex mutex_;
    std::condition_variable ready_;
    std::optional<Job> pending_;
    std::atomic<quint64> generation_{0};
    std::atomic<bool> stopping_{false};
    fz_cookie cookie_{};
    std::thread thread_;
    void run();
public:
    std::function<void(PdfPreviewResult)> preview_ready;
    std::function<void(QList<PdfContentHit>, int, int, bool, int)> search_ready;
    explicit PdfPickerWorker(QObject* parent = nullptr);
    ~PdfPickerWorker();
    void preview(QString path, int page, QString query);
    void search(QStringList paths, QString query);
    void cancel();
};

class PdfPickerPreview : public QWidget {
    PdfPickerWorker worker_;
    PdfPreviewResult result_;
    QString path_, query_, message_ = "Select a PDF to preview";
    int page_ = 0, selection_page_ = -1;
    QTimer debounce_;
    void request();
public:
    explicit PdfPickerPreview(QWidget* parent);
    void select(QString path, int page, QString query);
    void change_page(int delta);
    void paintEvent(QPaintEvent*) override;
};
