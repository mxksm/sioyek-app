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
    std::vector<std::wstring> recent;
    std::vector<std::wstring> cycle_order;
    int cycle_index = 0;
    void promote(const std::wstring& path) {
        recent.erase(std::remove(recent.begin(), recent.end(), path), recent.end());
        recent.insert(recent.begin(), path);
    }
public:
    const std::vector<std::wstring>& cycling_paths() const { return cycle_order; }
    int cycling_index() const { return cycle_order.empty() ? -1 : cycle_index; }
    void finish_cycle() {
        if (cycle_order.empty()) return;
        cycle_order.clear();
        if (selected >= 0) promote(entries[selected].path);
    }
    std::wstring cycle_recent(const std::wstring& current, bool backwards) {
        if (entries.empty()) return {};
        if (cycle_order.empty()) {
            if (index(current) >= 0) promote(current);
            cycle_order = recent;
            cycle_index = 0;
        }
        const int count = int(cycle_order.size());
        cycle_index = (cycle_index + (backwards ? count - 1 : 1)) % count;
        return cycle_order[cycle_index];
    }
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
        if (i < 0) {
            finish_cycle();
            i = int(entries.size()); entries.push_back({path, {}});
            recent.push_back(path);
        }
        if (select) {
            if (!cycle_order.empty() && cycle_order[cycle_index] != path) finish_cycle();
            selected = i;
            if (cycle_order.empty()) promote(path);
        }
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
        finish_cycle();
        recent.erase(std::remove(recent.begin(), recent.end(), path), recent.end());
        entries.erase(entries.begin() + i);
        if (i < selected) --selected;
        else if (i == selected) selected = std::min(i, int(entries.size()) - 1);
    }
    void rename(const std::wstring& old_path, const std::wstring& new_path) {
        finish_cycle();
        int i = index(old_path);
        if (i >= 0) entries[i].path = new_path;
        for (auto& path : recent) if (path == old_path) path = new_path;
    }
};
