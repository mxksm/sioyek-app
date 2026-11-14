#include "pdf_text_cursor.h"
#include "document.h"
#include "utils.h"
#include <QChar>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <limits>
#include <numeric>

PdfTextCursor::Page& PdfTextCursor::page(int number) {
    auto found = pages_.find(number);
    if (found != pages_.end()) return found->second;
    auto& result = pages_[number];
    auto text = document_->get_stext_with_page_number(number);
    if (!text) return result;
    auto rows = get_source_line_rects(text);
    result.rows.resize(rows.size());
    std::vector<fz_stext_char*> chars;
    get_flat_chars_from_stext_page(text, chars);
    for (auto ch : chars) {
        auto rect = fz_rect_from_quad(ch->quad);
        if (rect.y1 - rect.y0 < 0.3f * ch->size) {
            rect.y0 = ch->origin.y - 0.8f * ch->size;
            rect.y1 = ch->origin.y + 0.2f * ch->size;
        }
        if (fz_is_empty_rect(rect) || rows.empty()) continue;
        const float x = (rect.x0 + rect.x1) / 2, y = (rect.y0 + rect.y1) / 2;
        int row = 0;
        float best = std::numeric_limits<float>::max();
        for (size_t i = 0; i < rows.size(); ++i) {
            const auto r = rows[i];
            const float dx = std::max({r.x0-x, x-r.x1, 0.0f});
            const float dy = std::max({r.y0-y, y-r.y1, 0.0f});
            const float score = dx*dx + dy*dy + 0.001f*std::abs(y-(r.y0+r.y1)/2);
            if (score < best) { best = score; row = static_cast<int>(i); }
        }
        result.rows[row].push_back(static_cast<int>(result.chars.size()));
        result.chars.push_back({ch->c, rect, row});
    }
    // Preserve the PDF's reading order (including columns), while treating
    // detached mathematical fragments as part of their visual line.
    result.rows.erase(std::remove_if(result.rows.begin(), result.rows.end(),
        [](const auto& row) { return row.empty(); }), result.rows.end());
    std::sort(result.rows.begin(), result.rows.end(), [](const auto& a, const auto& b) { return a.front() < b.front(); });
    for (size_t r = 0; r < result.rows.size(); ++r)
        for (int index : result.rows[r]) result.chars[index].row = static_cast<int>(r);
    return result;
}

PdfTextCursor::PdfTextCursor(Document* document, DocumentPos start) : document_(document) {
    cursor_.page = std::clamp(start.page, 0, document_->num_pages()-1);
    auto& chars = page(cursor_.page).chars;
    float best = std::numeric_limits<float>::max();
    for (size_t i = 0; i < chars.size(); ++i) {
        auto r = chars[i].rect;
        const float dx = std::max({r.x0-start.x, start.x-r.x1, 0.0f});
        const float dy = std::max({r.y0-start.y, start.y-r.y1, 0.0f});
        const float score = dx*dx + dy*dy;
        if (score < best) { best = score; cursor_.index = static_cast<int>(i); }
    }
    anchor_ = cursor_;
}

bool PdfTextCursor::valid() { return !page(cursor_.page).chars.empty(); }
DocumentRect PdfTextCursor::cursor_rect() { return {page(cursor_.page).chars[cursor_.index].rect, cursor_.page}; }
DocumentRect PdfTextCursor::anchor_rect() { return {page(anchor_.page).chars[anchor_.index].rect, anchor_.page}; }
void PdfTextCursor::toggle_selection() { selecting_ = !selecting_; anchor_ = cursor_; }
std::wstring PdfTextCursor::current_lines(int count) {
    if (!valid()) return {};
    std::wstring text;
    int p = cursor_.page;
    int row = page(p).chars[cursor_.index].row;
    for (int i = 0; i < std::clamp(count, 1, 9999); ++i) {
        while (p < document_->num_pages() && row >= static_cast<int>(page(p).rows.size())) {
            ++p;
            row = 0;
        }
        if (p >= document_->num_pages()) break;
        const auto& data = page(p);
        for (int index : data.rows[row]) text.push_back(data.chars[index].text);
        text.push_back(L'\n');
        ++row;
    }
    trim_pages();
    return text;
}
bool PdfTextCursor::cursor_before_anchor() const {
    return cursor_.page < anchor_.page || (cursor_.page == anchor_.page && cursor_.index < anchor_.index);
}

void PdfTextCursor::selection(std::deque<AbsoluteRect>& rects, std::wstring& text) {
    rects.clear(); text.clear();
    if (!selecting_) return;
    const auto begin = cursor_before_anchor() ? cursor_ : anchor_;
    const auto end = cursor_before_anchor() ? anchor_ : cursor_;
    for (int p = begin.page; p <= end.page; ++p) {
        const auto& chars = page(p).chars;
        const int first = p == begin.page ? begin.index : 0;
        const int last = p == end.page ? end.index : static_cast<int>(chars.size())-1;
        for (int i = first; i <= last; ++i) {
            const auto& ch = chars[i];
            if (i > first && chars[i-1].row != ch.row && !text.empty() && !QChar::isSpace(static_cast<uint>(text.back()))) {
                if (text.back() == L'-') text.pop_back();
                else text.push_back(L' ');
            }
            text.push_back(ch.text);
            rects.push_back(DocumentRect(ch.rect,p).to_absolute(document_));
        }
        if (p < end.page && !text.empty() && !QChar::isSpace(static_cast<uint>(text.back()))) text.push_back(L' ');
    }
    trim_pages();
}

void PdfTextCursor::trim_pages() {
    for (auto it = pages_.begin(); it != pages_.end();)
        if (it->first != cursor_.page && it->first != anchor_.page) it = pages_.erase(it); else ++it;
}

bool PdfTextCursor::step(Position& pos, int direction) {
    const int index = pos.index + direction;
    if (index >= 0 && index < static_cast<int>(page(pos.page).chars.size())) { pos.index = index; return true; }
    for (int p = pos.page + direction; p >= 0 && p < document_->num_pages(); p += direction) {
        const auto& chars = page(p).chars;
        if (!chars.empty()) { pos = {p, direction > 0 ? 0 : static_cast<int>(chars.size())-1}; return true; }
    }
    return false;
}

int PdfTextCursor::kind(Position pos, bool big) {
    const auto c = static_cast<uint>(page(pos.page).chars[pos.index].text);
    if (QChar::isSpace(c)) return 0;
    if (big || QChar::isLetterOrNumber(c) || QChar::isMark(c) || c == '_') return 1;
    return 2;
}

bool PdfTextCursor::same_word(Position a, Position b, bool big) {
    return a.page == b.page && page(a.page).chars[a.index].row == page(b.page).chars[b.index].row &&
        kind(a, big) != 0 && kind(a, big) == kind(b, big);
}

void PdfTextCursor::word(char motion, bool big) {
    Position next = cursor_;
    if (motion == 'w') {
        while (step(next, 1) && same_word(cursor_, next, big)) cursor_ = next;
        if (!(next == cursor_)) cursor_ = next;
        while (kind(cursor_, big) == 0) { next = cursor_; if (!step(next, 1)) break; cursor_ = next; }
    } else if (motion == 'b') {
        if (!step(next, -1)) return;
        cursor_ = next;
        while (kind(cursor_, big) == 0) { next = cursor_; if (!step(next, -1)) break; cursor_ = next; }
        next = cursor_;
        while (step(next, -1) && same_word(cursor_, next, big)) cursor_ = next;
    } else {
        if (!step(next, 1)) return;
        cursor_ = next;
        while (kind(cursor_, big) == 0) { next = cursor_; if (!step(next, 1)) break; cursor_ = next; }
        next = cursor_;
        while (step(next, 1) && same_word(cursor_, next, big)) cursor_ = next;
    }
}

void PdfTextCursor::vertical(int direction) {
    auto& current = page(cursor_.page);
    if (column_ < 0) { auto r = current.chars[cursor_.index].rect; column_ = (r.x0+r.x1)/2; }
    int p = cursor_.page, row = current.chars[cursor_.index].row + direction;
    if (row < 0 || row >= static_cast<int>(current.rows.size())) {
        Position next{p, direction > 0 ? static_cast<int>(current.chars.size())-1 : 0};
        if (!step(next, direction)) return;
        p = next.page;
        row = direction > 0 ? 0 : static_cast<int>(page(p).rows.size())-1;
    }
    float best = std::numeric_limits<float>::max();
    for (int index : page(p).rows[row]) {
        auto r = page(p).chars[index].rect;
        const float distance = std::abs((r.x0+r.x1)/2 - column_);
        if (distance < best) { best = distance; cursor_ = {p,index}; }
    }
}

bool PdfTextCursor::move(const std::string& motion, int count) {
    if (!valid()) return false;
    if (motion != "j" && motion != "k") column_ = -1;
    count = std::clamp(count, 1, 9999);
    if (motion == "o") { std::swap(cursor_, anchor_); return true; }
    if (motion == "iw" || motion == "aw" || motion == "iW" || motion == "aW") {
        const bool big = motion[1] == 'W';
        Position begin = cursor_, end = cursor_, next = cursor_;
        while (step(next, -1) && same_word(begin, next, big)) begin = next;
        next = end;
        while (step(next, 1) && same_word(end, next, big)) end = next;
        anchor_ = begin; cursor_ = end; selecting_ = true;
        for (int i = 1; i < count; ++i) word('e', big);
        if (motion[0] == 'a') {
            next = cursor_;
            while (step(next, 1) && kind(next, big) == 0 && next.page == cursor_.page &&
                page(next.page).chars[next.index].row == page(cursor_.page).chars[cursor_.index].row) cursor_ = next;
            if (cursor_ == end) {
                next = anchor_;
                while (step(next, -1) && kind(next, big) == 0 && next.page == anchor_.page &&
                    page(next.page).chars[next.index].row == page(anchor_.page).chars[anchor_.index].row) anchor_ = next;
            }
        }
    } else if (motion == "gg" || motion == "G") {
        const int direction = motion == "gg" ? 1 : -1;
        for (int p = direction > 0 ? 0 : document_->num_pages()-1;
            p >= 0 && p < document_->num_pages(); p += direction) {
            auto& chars = page(p).chars;
            if (!chars.empty()) { cursor_ = {p, direction > 0 ? 0 : static_cast<int>(chars.size())-1}; break; }
        }
    } else for (int i = 0; i < count; ++i) {
        if (motion == "h" || motion == "l") step(cursor_, motion == "h" ? -1 : 1);
        else if (motion == "j" || motion == "k") vertical(motion == "j" ? 1 : -1);
        else if (motion == "w" || motion == "b" || motion == "e" || motion == "W" || motion == "B" || motion == "E")
            word(static_cast<char>(std::tolower(motion[0])), std::isupper(motion[0]));
        else if (motion == "0" || motion == "^" || motion == "$") {
            auto& data = page(cursor_.page);
            const auto& row = data.rows[data.chars[cursor_.index].row];
            cursor_.index = motion == "$" ? row.back() : row.front();
            if (motion == "^") for (int index : row) if (kind({cursor_.page,index}, false) != 0) { cursor_.index = index; break; }
        } else return false;
    }
    if (!selecting_) anchor_ = cursor_;
    // Selection geometry is maintained by the view. Keep only the endpoint
    // pages here, so visiting a long document does not retain another text cache.
    trim_pages();
    return true;
}
