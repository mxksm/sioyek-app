#include "../pdf_viewer/window_tabs.h"
#include <cassert>
#include <iostream>
int main() {
    WindowTabs<int> first, second;
    first.add(L"a.pdf"); first.add(L"b.pdf"); first.add(L"c.pdf");
    first.save(L"b.pdf", 123);
    first.add(L"b.pdf");
    assert(first.paths().size() == 3 && first.active() == 1);
    second.add(L"b.pdf"); second.save(L"b.pdf", 456);
    assert(first.state(L"b.pdf") == 123 && second.state(L"b.pdf") == 456);
    first.remove(L"a.pdf");
    assert(first.active() == 0 && first.paths()[0] == L"b.pdf");
    first.remove(L"b.pdf");
    assert(first.active() == 0 && first.paths()[0] == L"c.pdf");
    assert(second.paths().size() == 1 && second.state(L"b.pdf") == 456);
    first.add(L"d.pdf"); first.remove(L"d.pdf");
    assert(first.active() == 0);
    first.rename(L"c.pdf", L"renamed.pdf");
    assert(first.paths()[0] == L"renamed.pdf");
    first.remove(L"renamed.pdf");
    assert(first.paths().empty() && first.active() == -1);
    first.add(L"restored.pdf", false); first.add(L"second.pdf", false);
    first.add(L"second.pdf");
    assert(first.active() == 1 && first.paths()[0] == L"restored.pdf");
    WindowTabs<int> reordered;
    assert(!reordered.move(L"missing", 1));
    for (auto path : {L"a", L"b", L"c"}) reordered.add(path);
    reordered.add(L"b");
    reordered.save(L"b", 123);
    assert(reordered.move(L"b", -1));
    assert((reordered.paths() == std::vector<std::wstring>{L"b", L"a", L"c"}));
    assert(reordered.active() == 0 && reordered.state(L"b") == 123);
    assert(!reordered.move(L"b", -1));
    assert(reordered.move(L"b", 1) && reordered.move(L"b", 1));
    assert(reordered.active() == 2 && !reordered.move(L"b", 1));
    assert(reordered.move(L"c", 1)); // Moving a neighbor preserves the active PDF too.
    assert(reordered.active() == 1 && reordered.paths()[1] == L"b");
    assert(reordered.cycle_recent(L"b", false) == L"c");
    assert(reordered.cycle_recent(L"b", false) == L"a");
    reordered.finish_cycle();
    assert(!reordered.move(L"b", 0) && !reordered.move(L"b", 2));
    WindowTabs<int> closing;
    for (auto path : {L"a", L"b", L"c"}) closing.add(path);
    closing.add(L"a");
    closing.add(L"new");
    closing.remove(L"new");
    assert(closing.paths()[closing.active()] == L"a");
    closing.add(L"new");
    closing.remove(L"a"); // Previous tab was closed in the background.
    assert(closing.paths()[closing.active()] == L"new");
    closing.remove(L"new");
    assert(closing.paths()[closing.active()] == L"c");
    closing.move(L"c", -1);
    closing.remove(L"c");
    assert(closing.paths()[closing.active()] == L"b");
    WindowTabs<int> single;
    single.add(L"only");
    assert(!single.move(L"only", -1) && !single.move(L"only", 1));
    WindowTabs<int> mru;
    for (auto path : {L"a", L"b", L"c", L"d"}) mru.add(path);
    mru.add(L"b"); // b, d, c, a (visual order remains a, b, c, d)
    auto step = [&](const wchar_t* current, const wchar_t* expected, bool backwards = false) {
        auto next = mru.cycle_recent(current, backwards);
        assert(next == expected);
        mru.add(next);
    };
    step(L"b", L"d"); step(L"d", L"c"); step(L"c", L"a"); step(L"a", L"b");
    step(L"b", L"a", true); step(L"a", L"c", true);
    mru.finish_cycle(); // c, b, d, a; intermediate previews did not reorder history
    step(L"c", L"b"); mru.finish_cycle();
    step(L"b", L"c"); mru.finish_cycle();
    assert((mru.paths() == std::vector<std::wstring>{L"a", L"b", L"c", L"d"}));
    mru.remove(L"b");
    step(L"c", L"d"); mru.finish_cycle();
    mru.rename(L"c", L"renamed");
    step(L"d", L"renamed"); mru.finish_cycle();
    WindowTabs<int> from_canvas;
    for (auto path : {L"a", L"b", L"c"}) from_canvas.add(path);
    assert(from_canvas.cycle_recent(L"hidden-canvas", false) == L"c");
    assert(from_canvas.cycle_recent(L"hidden-canvas", false) == L"b");
    assert(from_canvas.cycle_recent(L"hidden-canvas", false) == L"a");
    assert(from_canvas.cycle_recent(L"hidden-canvas", false) == L"c");
    from_canvas.finish_cycle();
    assert(from_canvas.cycle_recent(L"hidden-canvas", true) == L"a");
    from_canvas.finish_cycle();
    std::cout << "PASS: tab order, duplicate selection, isolated view states, recent-tab close, rename and restore\n";
    std::cout << "PASS: MRU cycling, wraparound, reverse, commit, closed and renamed tabs\n";
    std::cout << "PASS: tab movement preserves selection, states and MRU, and stops at endpoints\n";
}
