#pragma once
#include <QObject>
#include <mupdf/fitz.h>
#include <QImage>
#include <QRectF>
#include <QStringList>
#include <QWidget>
#include <QTimer>
#include <QCache>
#include <QPointer>
#include <array>
#include <deque>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <optional>

struct PdfContentHit { QString path; int page; QString snippet; QList<int> matching_pages; };
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

struct PdfPreviewTarget {
    QString path;
    int page = 0;
};

class PdfViewOpenGLWidget;

class PdfPickerPreview : public QWidget {
    QPointer<PdfViewOpenGLWidget> reader_;
    QImage themed_image_;
    qint64 themed_source_key_ = 0;
    int themed_mode_ = -1;
    std::array<float, 16> themed_transform_{};
    const QImage& display_image();
    PdfPickerWorker worker_, prefetch_worker_;
    // QImage copies share their pixels; this cache is limited by decoded image size.
    QCache<QString, PdfPreviewResult> cache_{48 * 1024}; // KiB
    PdfPreviewResult result_;
    QString path_, query_, message_ = "Select a PDF to preview";
    QString selection_key_, requested_key_, displayed_key_, prefetch_key_;
    QList<PdfPreviewTarget> neighbors_;
    QList<int> matching_pages_;
    std::deque<PdfPreviewTarget> prefetch_queue_;
    int page_ = 0;
    bool loading_ = false;
    void request();
    void remember(const QString& key, const PdfPreviewResult& result);
    void schedule_prefetch();
    void prefetch_next();
public:
    explicit PdfPickerPreview(QWidget* parent, PdfViewOpenGLWidget* reader = nullptr);
    void select(QString path, int page, QString query, QList<int> matching_pages = {});
    void prefetch(QList<PdfPreviewTarget> neighbors);
    void change_page(int delta);
    void change_match(int delta);
    int current_page() const { return page_; }
    bool is_loading() const { return loading_; }
    bool has_preview() const { return !result_.image.isNull(); }
    bool is_cached(const QString& path, int page, const QString& query) const;
    int cached_kib() const { return cache_.totalCost(); }
    void paintEvent(QPaintEvent*) override;
};
