#pragma once
#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
namespace fuyi {
struct Candidate {
    std::wstring text;
    size_t consumed = 0;
};
struct Sentence {
    std::wstring text;
    uint64_t touched = 0;
    bool finished = false;
    void clear() {
        text.clear();
        touched = 0;
        finished = false;
    }
    bool expired(uint64_t now) const {
        return !text.empty() && now >= touched && now - touched > (finished ? 8000 : 60000);
    }
    void begin(uint64_t now, bool moved = false) {
        if (moved || finished || expired(now))
            clear();
        touched = now;
    }
    void append(const std::wstring &part, uint64_t now) {
        if (finished)
            clear();
        text += part;
        if (text.size() > 1000) {
            clear();
            return;
        }
        touched = now;
        finished = !text.empty() && std::wstring(L"。！？.!?\n").find(text.back()) != std::wstring::npos;
    }
    void backspace(uint64_t now) {
        if (!text.empty()) {
            text.pop_back();
            if (!text.empty() && text.back() >= 0xd800 && text.back() <= 0xdbff)
                text.pop_back();
        }
        finished = false;
        touched = now;
    }
};
struct ShiftTap {
    bool armed = false, used = false;
    uint64_t time = 0;
    void down(uint64_t now, bool other) {
        if (!armed) {
            armed = true;
            time = now;
            used = other;
        } else
            used = true;
    }
    void key() {
        if (armed)
            used = true;
    }
    bool up(uint64_t now, bool other) {
        bool toggle = armed && !used && !other && now >= time && now - time < 1000;
        reset();
        return toggle;
    }
    void reset() {
        armed = used = false;
        time = 0;
    }
};
struct Rect {
    int left, top, right, bottom;
};
inline std::optional<Rect> place(Rect anchor, int width, int height, Rect work, bool above) {
    if (anchor.bottom <= anchor.top || anchor.bottom - anchor.top > 500 || anchor.right < anchor.left ||
        anchor.right - anchor.left > 10000 || width <= 0 || height <= 0 || anchor.right < work.left ||
        anchor.left > work.right || anchor.bottom < work.top || anchor.top > work.bottom)
        return {};
    int top = anchor.top - height - 3, bottom = anchor.bottom + 3;
    int y = above ? (top >= work.top ? top : bottom) : (bottom + height <= work.bottom ? bottom : top);
    int x = std::clamp(anchor.left, work.left + 4, std::max(work.left + 4, work.right - width - 4));
    y = std::clamp(y, work.top + 4, std::max(work.top + 4, work.bottom - height - 4));
    return Rect{x, y, x + width, y + height};
}
inline void eraseLast(std::wstring &value) {
    if (value.empty())
        return;
    value.pop_back();
    if (!value.empty() && value.back() >= 0xd800 && value.back() <= 0xdbff)
        value.pop_back();
}
} // namespace fuyi
