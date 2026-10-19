#pragma once
#include <algorithm>
#include <optional>
#include <string>
#include <vector>

// Each window owns its order and view states; documents/annotations remain shared.
template<class State> class WindowTabs {
    struct Tab { std::wstring path; std::optional<State> state; };
    std::vector<Tab> entries;
    int selected = -1;
public:
    int index(const std::wstring& path) const {
        for (int i = 0; i < int(entries.size()); ++i) if (entries[i].path == path) return i;
        return -1;
    }
    int active() const { return selected; }
    std::vector<std::wstring> paths() const {
        std::vector<std::wstring> result;
        for (const auto& tab : entries) result.push_back(tab.path);
        return result;
    }
    void add(const std::wstring& path, bool select = true) {
        if (path.empty()) return;
        int i = index(path);
        if (i < 0) { i = int(entries.size()); entries.push_back({path, {}}); }
        if (select) selected = i;
    }
    void save(const std::wstring& path, const State& state) {
        int i = index(path);
        if (i >= 0) entries[i].state = state;
    }
    std::optional<State> state(const std::wstring& path) const {
        int i = index(path);
        return i < 0 ? std::optional<State>{} : entries[i].state;
    }
    void remove(const std::wstring& path) {
        int i = index(path);
        if (i < 0) return;
        entries.erase(entries.begin() + i);
        if (i < selected) --selected;
        else if (i == selected) selected = std::min(i, int(entries.size()) - 1);
    }
    void rename(const std::wstring& old_path, const std::wstring& new_path) {
        int i = index(old_path);
        if (i >= 0) entries[i].path = new_path;
    }
};
