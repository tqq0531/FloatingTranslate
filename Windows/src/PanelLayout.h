#pragma once
#include "Core.h"
#include <functional>
namespace fuyi {
// Shared typography keeps native painting and measured wrapping in step.
struct PanelStyle {
    static constexpr int englishSize = 14, candidateSize = 17, labelSize = 9;
    static constexpr int cornerDiameter = 20;
};
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
    std::vector<TextLine> english, proof;
    Rect applyAdvice{}, copyAdvice{};
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
                               const MeasureText &measure, const std::wstring &advice=L"") {
    const auto d = [dpi](int v) { return (v * dpi + 48) / 96; };
    PanelLayout out;
    maxWidth = std::max(d(80), std::min(d(440), maxWidth));
    int desired = d(136), total = 2 * d(8);
    for (size_t start = 0; start < text.size();) {
        auto end = text.find(L'\n', start);
        if (end == std::wstring::npos) end = text.size();
        desired = std::max(desired, measure(text, start, end - start, PanelFont::English) + d(64));
        start = end + 1;
    }
    size_t first = selected / 5 * 5, last = std::min(first + 5, candidates.size());
    for (size_t i = first; i < last; ++i)
        total += std::max(d(36), measure(candidates[i].text, 0, candidates[i].text.size(), PanelFont::Candidate) + d(26)) + d(4);
    if (last > first) desired = std::max(desired, total - d(4));
    if(!advice.empty())desired=std::max(desired,d(204));
    out.width = std::min(desired, maxWidth);
    out.copy = {out.width - d(29), d(6), out.width - d(7), d(28)};
    out.english = wrapPanelText(text, std::max(d(8), out.width - d(64)), d(18), PanelFont::English, measure);
    for (auto &line : out.english) {
        line.rect.left += d(32); line.rect.right += d(32);
        line.rect.top += d(8); line.rect.bottom += d(8);
    }
    int y = d(8) + std::max(1, (int)out.english.size()) * d(18) + d(8);
    if (last > first) {
        out.divider = y;
        y += d(6);
        int x = d(8), rowHeight = 0;
        for (size_t i = first; i < last; ++i) {
            int boxWidth = std::min(out.width - 2 * d(8),
                measure(candidates[i].text, 0, candidates[i].text.size(), PanelFont::Candidate) + d(26));
            boxWidth = std::max(d(36), boxWidth);
            if (x > d(8) && x + boxWidth > out.width - d(8)) {
                y += rowHeight + d(4); x = d(8); rowHeight = 0;
            }
            auto lines = wrapPanelText(candidates[i].text, std::max(d(8), boxWidth - d(26)),
                                       d(22), PanelFont::Candidate, measure);
            int boxHeight = std::max(d(28), (int)lines.size() * d(22) + d(6));
            for (auto &line : lines) {
                line.rect.left += x + d(20); line.rect.right += x + d(20);
                line.rect.top += y + d(3); line.rect.bottom += y + d(3);
            }
            out.choices.push_back({i, {x, y, x + boxWidth, y + boxHeight}, std::move(lines)});
            rowHeight = std::max(rowHeight, boxHeight); x += boxWidth + d(4);
        }
        y += rowHeight + d(6);
    }
    if(!advice.empty()) {
        y+=d(7);
        out.proof=wrapPanelText(advice,std::max(d(20),out.width-d(24)),d(18),PanelFont::English,measure);
        for(auto &line:out.proof){line.rect.left+=d(12);line.rect.right+=d(12);line.rect.top+=y;line.rect.bottom+=y;}
        y+=int(out.proof.size())*d(18)+d(6);
        int half=std::max(d(24),(out.width-d(28))/2);
        out.applyAdvice={d(10),y,d(10)+half,y+d(25)};
        out.copyAdvice={d(18)+half,y,out.width-d(10),y+d(25)};
        y+=d(32);
    }
    out.contentHeight = y;
    out.height = std::min(y, maxHeight);
    return out;
}
} // namespace fuyi
