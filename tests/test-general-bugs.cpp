#define main sioyek_application_main
#include "../pdf_viewer/main.cpp"
#undef main
#include "../pdf_viewer/blank_canvas.h"
#include <QTemporaryDir>
#include <set>
#include <QMessageBox>
#include <type_traits>

static_assert(std::has_virtual_destructor_v<Annotation>);
static_assert(std::has_virtual_destructor_v<TextToSpeechHandler>);
static_assert(std::has_virtual_destructor_v<DocumentView>);

extern bool NUMERIC_TAGS;
extern bool ALPHABETIC_LINK_TAGS;
extern std::wstring ANNOTATIONS_DIR_PATH;
static QString directory;
static int failures = 0;
static void expect(bool ok, const char* message) {
    qInfo("%s: %s", ok ? "PASS" : "FAIL", message);
    if (!ok) ++failures;
}
static void run() {
    ANNOTATIONS_DIR_PATH.clear();
    expect(file_size_to_human_readable_string(999) == "999" &&
        file_size_to_human_readable_string(1000) == "1K" &&
        file_size_to_human_readable_string(1000000) == "1M" &&
        file_size_to_human_readable_string(1000000000) == "1G" &&
        file_size_to_human_readable_string(5000000000LL) == "5G" &&
        file_size_to_human_readable_string(999999999999LL) == "999G",
        "download file sizes do not overflow at gigabyte boundaries");
    Config unknown_config;
    unknown_config.config_type = static_cast<ConfigType>(13);
    expect(unknown_config.get_type_string() == L"unknown", "unknown config type has a defined result");
    const std::string unicode = "aé𝛼";
    utf8::iterator<std::string::const_iterator> cursor(unicode.begin(), unicode.begin(), unicode.end());
    expect(*cursor == 'a' && *++cursor == 0xe9 && *++cursor == 0x1d6fc && *--cursor == 0xe9,
        "UTF-8 iterator traits modernization preserves bidirectional decoding");
    const auto sensitive = SearchCaseSensitivity::CaseSensitive;
    auto search = [&](std::wstring text, std::vector<int> pages, std::wstring query, int start=0, int low=0, int high=2) {
        return search_regex_with_index(text, pages, query, sensitive, start, low, high);
    };
    auto matches = search(L"catcatcat", {0,3,6}, L"cat", 1);
    expect(matches.size()==3 && matches[0].page==1 && matches[0].begin_index_in_page==0 &&
        matches[1].page==2 && matches[2].page==0, "regex page boundaries and wrap order");
    matches = search(L"abc", {0}, L"z*|b", 0,0,0);
    expect(matches.size()==1 && matches[0].begin_index_in_page==1, "regex continues after empty matches");
    matches = search_regex_with_index(L"Cat cat", {0}, L"Cat", SearchCaseSensitivity::SmartCase,0,0,0);
    expect(matches.size()==1, "regex respects uppercase smart-case query");
    matches = search_text_with_index(L"abcDEF", {0,3}, L"missing",sensitive,0,0,0);
    expect(matches.empty(), "range-limited text search does not invent a match at range end");
    expect(search(L"abc", {}, L"a").empty() && search(L"abc", {0}, L"a",0,2,3).empty(),
        "regex handles empty indexes and invalid page ranges");
    expect(search_text_with_index(L"abc", {}, L"a",sensitive,0,0,0).empty() &&
        search_text_with_index(L"abc", {0}, L"",sensitive,0,0,0).empty(),
        "text search handles empty indexes and queries");
    expect(search(L"abc",{0},L"[",0,0,0).empty() && search(L"abc",{0},L"^",0,0,0).empty(),
        "invalid and zero-width-only regexes return no highlights");
    const bool numeric = NUMERIC_TAGS; NUMERIC_TAGS=false;
    for (int count : {26,27,676,677,702,703}) {
        auto tags=get_tags(count); std::set<std::string> unique(tags.begin(),tags.end());
        bool roundtrip=unique.size()==size_t(count);
        for (int i=0;i<count;++i) roundtrip &= get_index_from_tag(tags[i])==i;
        expect(roundtrip,"keyboard labels remain unique and round-trip at alphabet-size boundaries");
    }
    NUMERIC_TAGS=numeric;
    auto widget=windows.front(); auto doc=widget->doc();
    doc->fill_index_highlight_rects(-1);
    const auto invalid_page_rect = doc->get_page_absolute_rect(-1);
    expect(invalid_page_rect.x0 == 0 && invalid_page_rect.x1 == 1,
        "negative page and annotation indices remain rejected");
    widget->opengl_widget->set_selected_rectangle(DocumentRect(fz_rect{10,20,30,40}, 1).to_absolute(doc));
    const auto selection_state = widget->get_json_state()["selected_rect_document"].toObject();
    expect(selection_state["page"].toInt(-1) == 1,
        "selection state reports the selected page rather than uninitialized data");
    widget->clear_selected_rect();
    const bool alphabetic = ALPHABETIC_LINK_TAGS; ALPHABETIC_LINK_TAGS=false;
    expect(!widget->get_selected_link(L"99999999999999999999999999999").has_value(),
        "oversized numeric link input is rejected without an exception");
    ALPHABETIC_LINK_TAGS=alphabetic;
    doc->delete_all_drawings();
    const auto stamp=QDateTime::fromString("2026-11-08T12:00:00.100Z",Qt::ISODate);
    FreehandDrawing ink; ink.type='r'; ink.alpha=1; ink.creattion_time=stamp;
    ink.points.push_back({DocumentPos{0,100,100}.to_absolute(doc),5}); doc->add_freehand_drawing(ink);
    ink.creattion_time=stamp.addMSecs(300); ink.points[0].pos=DocumentPos{1,100,100}.to_absolute(doc);
    doc->add_freehand_drawing(ink); doc->persist_drawings(); doc->load_drawings();
    expect(doc->get_page_drawings(1).front().creattion_time==stamp.addMSecs(300),"drawing save retains subsecond creation time");
    doc->undo_freehand_drawing();
    expect(doc->get_page_drawings(1).empty() && doc->get_page_drawings(0).size()==1,"cross-page undo order survives reopening");
    auto helper=widget->helper_document_view(); bool invalid=false;
    helper->open_document(doc->get_path(), &invalid);
    std::vector<DocumentView*> views; std::vector<MainWidget*> widgets; std::vector<bool> helpers;
    widget->get_document_views_referencing_doc(doc->get_path(), views, widgets, helpers);
    expect(views.size()==2 && views[0]==widget->main_document_view && views[1]==helper && helpers[1],
        "rename tracks the actual helper view rather than main view twice");
    // Exercise actual rename with a helper and a split pane holding this PDF.
    widget->toggle_split();
    auto right=widget->split_pane_.data();
    expect(right != nullptr,"rename fixture has a split pane");
    if (right) right->open_document(doc->get_path());
    auto write = [](const QString& path,const QByteArray& data) {
        QFile file(path); expect(file.open(QIODevice::WriteOnly),"open companion fixture");
        expect(file.write(data)==data.size(),"write companion fixture");
    };
    auto read = [](const QString& path) { QFile file(path); file.open(QIODevice::ReadOnly); return file.readAll(); };
    QTimer dismiss;
    QObject::connect(&dismiss,&QTimer::timeout,[] {
        for (auto window : QApplication::topLevelWidgets())
            if (auto box=qobject_cast<QMessageBox*>(window)) box->accept();
    });
    dismiss.start(100);
    for (int round=0; round<2; ++round) {
        doc=widget->doc();
        if (round) ANNOTATIONS_DIR_PATH=directory.toStdWString();
        // An unsaved edit must also survive freeing/reopening the Document.
        ink.creattion_time=stamp.addMSecs(500+round); doc->add_freehand_drawing(ink);
        const auto count=doc->num_freehand_drawings();
        const auto old=doc->get_path();
        const auto target=directory+QString("/renamed%1.pdf").arg(round);
        const auto old_ink=QString::fromStdWString(doc->get_drawings_file_path());
        const auto old_scratch=QString::fromStdWString(doc->get_scratchpad_file_path());
        write(old_scratch,"preserve scratch text");
        doc->set_extra("regression_marker",17); doc->persist_extras();
        const auto conflict=QString::fromStdWString(doc->get_addtional_sioyek_file_path("drawings",target.toStdWString()));
        write(conflict,"existing unrelated data");
        widget->handle_rename(QString("renamed%1").arg(round).toStdWString());
        expect(widget->doc()->get_path()==old && read(conflict)=="existing unrelated data" && QFileInfo::exists(old_scratch),
            "rename refuses companion-file collisions without losing either file");
        QFile::remove(conflict);
        widget->handle_rename(QString("renamed%1").arg(round).toStdWString());
        const auto renamed=widget->doc();
        expect(renamed && renamed->get_path()==target.toStdWString() && !QFileInfo::exists(QString::fromStdWString(old)),
            "PDF renamed successfully");
        expect(renamed && renamed->num_freehand_drawings()==count && !QFileInfo::exists(old_ink),
            "rename preserves unsaved drawings and moves their storage");
        expect(helper->get_document()==renamed && right && right->doc()==renamed,
            "rename restores helper and split references to the new Document");
        expect(read(QString::fromStdWString(renamed->get_scratchpad_file_path()))=="preserve scratch text" &&
            renamed->get_extra("regression_marker").value_or(QVariant()).toInt()==17,
            "rename preserves scratch text and PDF settings");
    }
    QCoreApplication::exit(failures ? 1 : 0);
}
static void schedule() { QTimer::singleShot(1200,qApp,run); }
Q_COREAPP_STARTUP_FUNCTION(schedule)
int main(int /*argc*/,char** argv) {
    QStandardPaths::setTestModeEnabled(true); QTemporaryDir temporary; directory=temporary.path();
    ANNOTATIONS_DIR_PATH=directory.toStdWString();
    const auto path=directory+"/general.pdf";
    QFile file(path); file.open(QIODevice::WriteOnly); file.write(blank_canvas_pdf(3)); file.close();
    QByteArray first=path.toUtf8(); char option[]="--instance-name", name[]="sioyek-general-regression";
    char* args[]={argv[0],first.data(),option,name}; sioyek_application_main(4,args);
    return failures ? 1 : 0;
}
