#pragma once
#include "coordinates.h"
#include <map>
#include <deque>
#include <string>
#include <vector>

class Document;

// Stores character values/geometry, never pointers into MuPDF's evictable cache.
class PdfTextCursor {
    struct Position {
        int page = 0, index = 0;
        bool operator==(const Position& other) const { return page == other.page && index == other.index; }
    };
    struct Character { int text; fz_rect rect; int row; };
    struct Page {
        std::vector<Character> chars;
        std::vector<std::vector<int>> rows;
    };
    Document* document_;
    std::map<int, Page> pages_;
    Position cursor_, anchor_;
    float column_ = -1;
    bool selecting_ = true;
    Page& page(int number);
    bool step(Position& pos, int direction);
    bool same_word(Position a, Position b, bool big);
    int kind(Position pos, bool big);
    void vertical(int direction);
    void word(char motion, bool big);
    void trim_pages();
public:
    PdfTextCursor(Document* document, DocumentPos start);
    bool valid();
    bool selecting() const { return selecting_; }
    void toggle_selection();
    std::wstring current_lines(int count = 1);
    bool move(const std::string& motion, int count = 1);
    DocumentRect cursor_rect();
    DocumentRect anchor_rect();
    bool cursor_before_anchor() const;
    void selection(std::deque<AbsoluteRect>& rects, std::wstring& text);
};
