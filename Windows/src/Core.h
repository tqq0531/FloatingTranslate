#pragma once
#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <utility>
#include <array>
namespace fuyi {
// Each direction keeps an independent default; Tab requests the opposite.
inline bool translationOnCommit(bool englishInput, bool pinyinToEnglish,
                                bool englishToChinese, bool alternative) {
    bool translated = englishInput ? englishToChinese : pinyinToEnglish;
    return alternative ? !translated : translated;
}
struct Candidate {
    std::wstring text;
    size_t consumed = 0;
    size_t rimeIndex = 0;
};
inline std::wstring insertedSentence(const std::wstring &sentence, size_t offset,
                                     const std::wstring &part) {
    offset = std::min(offset, sentence.size());
    return sentence.substr(0, offset) + part + sentence.substr(offset);
}
// An unavailable TSF range is not evidence that the user moved the caret.
enum class RangeEvidence { Unknown, Matches, Different };
inline bool retainEnglishPrefix(bool englishInput, bool blocked, RangeEvidence text,
                                RangeEvidence caret, RangeEvidence prefix) {
    return englishInput && !blocked && text==RangeEvidence::Matches &&
           caret!=RangeEvidence::Different && prefix!=RangeEvidence::Different;
}
inline bool breaksSentence(RangeEvidence caret, RangeEvidence prefix) {
    return caret == RangeEvidence::Different || prefix == RangeEvidence::Different;
}
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
inline std::optional<Rect> searchFallbackAnchor(Rect extent, bool windows11, int dpi) {
    if (dpi < 96 || dpi > 768 || extent.right <= extent.left || extent.bottom <= extent.top ||
        extent.right - extent.left > 10000 || extent.bottom - extent.top > 4000) return {};
    auto d = [dpi](int value) { return (value*dpi+48)/96; };
    if (extent.bottom-extent.top <= d(160))
        return Rect{extent.left, extent.top, extent.left+1, extent.bottom};
    int top = windows11 ? extent.top+d(36) : extent.bottom-d(36);
    return Rect{extent.left+d(24), top, extent.left+d(24)+1, top+d(24)};
}
inline std::optional<std::array<unsigned, 4>> runtimeVersionParts(const std::wstring &value) {
    std::array<unsigned, 4> result{};
    size_t part = 0, digits = 0;
    for (wchar_t c : value) {
        if (c == L'.') {
            if (!digits || ++part >= result.size()) return {};
            digits = 0;
        } else if (c >= L'0' && c <= L'9' && ++digits <= 6) result[part] = result[part]*10 + (c-L'0');
        else return {};
    }
    if (!digits || part < 2) return {};
    return result;
}
inline bool olderRuntimeVersion(const std::wstring &candidate, const std::wstring &current) {
    auto a = runtimeVersionParts(candidate), b = runtimeVersionParts(current);
    return a && b && *a < *b;
}
inline std::optional<uint32_t> rimeCachePid(const std::wstring &name) {
    const std::wstring prefix = L"FuyiRime-";
    if (name.rfind(prefix, 0) != 0 || name.size() <= prefix.size() || name.size() > prefix.size()+10) return {};
    uint64_t pid = 0;
    for (size_t i=prefix.size(); i<name.size(); ++i) {
        if (name[i] < L'0' || name[i] > L'9') return {};
        pid = pid*10 + name[i]-L'0';
        if (pid > UINT32_MAX) return {};
    }
    return pid ? std::optional<uint32_t>((uint32_t)pid) : std::nullopt;
}
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
inline bool backspacePinyin(std::string &raw, std::wstring &locked,
                           std::vector<std::pair<std::string, std::wstring>> &undo) {
    if (!raw.empty()) raw.pop_back();
    else if (!undo.empty()) {
        raw = std::move(undo.back().first);
        locked = std::move(undo.back().second);
        undo.pop_back();
    } else if (!locked.empty()) eraseLast(locked);
    else return false;
    return true;
}
} // namespace fuyi
