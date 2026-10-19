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
    std::cout << "PASS: tab order, duplicate selection, isolated view states, adjacent close, rename and restore\n";
}
