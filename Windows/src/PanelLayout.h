#pragma once
#include "Core.h"
#include <functional>
namespace fuyi {
enum class PanelFont { English, Candidate };
struct TextLine {
    size_t start, length;
    Rect rect;
};
struct ChoiceLayout {
    size_t index;
    Rect rect;
    std::vector<TextLine> lines;
};
struct PanelLayout {
    int width = 0, height = 0, contentHeight = 0, divider = 0;
    Rect copy{};
    std::vector<TextLine> english;
    std::vector<ChoiceLayout> choices;
};
using MeasureText = std::function<int(const std::wstring &, size_t, size_t, PanelFont)>;
inline size_t nextCharacter(const std::wstring &text, size_t i) {
    if (i + 1 < text.size() && text[i] >= 0xd800 && text[i] <= 0xdbff &&
        text[i + 1] >= 0xdc00 && text[i + 1] <= 0xdfff)
        return i + 2;
    return i + 1;
}
inline std::vector<TextLine> wrapPanelText(const std::wstring &text, int width, int lineHeight,
                                          PanelFont font, const MeasureText &measure) {
    std::vector<TextLine> lines;
    for (size_t start = 0; start < text.size();) {
        size_t end = start, space = start;
        while (end < text.size() && text[end] != L'\n') {
            size_t next = nextCharacter(text, end);
            if (end > start && measure(text, start, next - start, font) > width)
                break;
            if (text[end] == L' ')
                space = next;
            end = next;
        }
        if (end < text.size() && text[end] != L'\n' && font == PanelFont::English && space > start)
            end = space;
        lines.push_back({start, end - start,
                         {0, (int)lines.size() * lineHeight,
                          measure(text, start, end - start, font), ((int)lines.size() + 1) * lineHeight}});
        start = end < text.size() && text[end] == L'\n' ? end + 1 : end;
    }
    return lines;
}
inline PanelLayout layoutPanel(const std::wstring &text, const std::vector<Candidate> &candidates,
                               size_t selected, int dpi, int maxWidth, int maxHeight,
                               const MeasureText &measure) {
    const auto d = [dpi](int v) { return (v * dpi + 48) / 96; };
    PanelLayout out;
    maxWidth = std::max(d(80), std::min(d(560), maxWidth));
    int desired = d(160), total = 2 * d(10);
    for (size_t start = 0; start < text.size();) {
        auto end = text.find(L'\n', start);
        if (end == std::wstring::npos) end = text.size();
        desired = std::max(desired, measure(text, start, end - start, PanelFont::English) + d(76));
        start = end + 1;
    }
    size_t first = selected / 5 * 5, last = std::min(first + 5, candidates.size());
    for (size_t i = first; i < last; ++i)
        total += std::max(d(40), measure(candidates[i].text, 0, candidates[i].text.size(), PanelFont::Candidate) + d(34)) + d(6);
    if (last > first) desired = std::max(desired, total - d(6));
    out.width = std::min(desired, maxWidth);
    out.copy = {out.width - d(33), d(8), out.width - d(9), d(32)};
    out.english = wrapPanelText(text, std::max(d(8), out.width - d(76)), d(22), PanelFont::English, measure);
    for (auto &line : out.english) {
        line.rect.left += d(38); line.rect.right += d(38);
        line.rect.top += d(11); line.rect.bottom += d(11);
    }
    int y = d(11) + std::max(1, (int)out.english.size()) * d(22) + d(11);
    if (last > first) {
        out.divider = y;
        y += d(8);
        int x = d(10), rowHeight = 0;
        for (size_t i = first; i < last; ++i) {
            int boxWidth = std::min(out.width - 2 * d(10),
                measure(candidates[i].text, 0, candidates[i].text.size(), PanelFont::Candidate) + d(34));
            boxWidth = std::max(d(40), boxWidth);
            if (x > d(10) && x + boxWidth > out.width - d(10)) {
                y += rowHeight + d(6); x = d(10); rowHeight = 0;
            }
            auto lines = wrapPanelText(candidates[i].text, std::max(d(8), boxWidth - d(34)),
                                       d(26), PanelFont::Candidate, measure);
            int boxHeight = std::max(d(34), (int)lines.size() * d(26) + d(8));
            for (auto &line : lines) {
                line.rect.left += x + d(24); line.rect.right += x + d(24);
                line.rect.top += y + d(4); line.rect.bottom += y + d(4);
            }
            out.choices.push_back({i, {x, y, x + boxWidth, y + boxHeight}, std::move(lines)});
            rowHeight = std::max(rowHeight, boxHeight); x += boxWidth + d(6);
        }
        y += rowHeight + d(8);
    }
    out.contentHeight = y;
    out.height = std::min(y, maxHeight);
    return out;
}
} // namespace fuyi
