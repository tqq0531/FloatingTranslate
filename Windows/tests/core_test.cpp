#include "../src/Core.h"
#include "../src/PanelLayout.h"
#include <cassert>
#include <iostream>
using namespace fuyi;
int main() {
    Sentence s;
    s.begin(100);
    s.append(L"我", 100);
    s.begin(200);
    s.append(L"今天", 200);
    s.begin(300);
    s.append(L"去北京", 300);
    assert(s.text == L"我今天去北京");
    assert(!s.expired(60300));
    assert(s.expired(60301));
    s.append(L"。", 400);
    assert(s.finished);
    assert(s.expired(8401));
    s.begin(500);
    assert(s.text.empty());
    s.append(L"你好", 600);
    s.begin(700, true);
    assert(s.text.empty());
    ShiftTap shift;
    shift.down(0, false);
    assert(shift.up(200, false));
    shift.down(1000, false);
    shift.key();
    assert(!shift.up(1100, false));
    shift.down(2000, true);
    assert(!shift.up(2100, false));
    shift.down(3000, false);
    assert(!shift.up(4000, false));
    auto r = place({1900, 1000, 1901, 1020}, 500, 150, {0, 0, 1920, 1080}, false);
    assert(r && r->left == 1416 && r->top == 847);
    r = place({-1900, 20, -1899, 40}, 500, 150, {-1920, 0, 0, 1080}, true);
    assert(r && r->left == -1900 && r->top == 43);
    assert(!place({0, 0, 0, 0}, 500, 150, {0, 0, 1920, 1080}, false));
    // Regression: a rich-edit host can echo the same IME edit multiple times,
    // or temporarily return no usable range after confirming each phrase.
    s.clear();
    uint64_t time = 1000;
    for (auto part : {L"你好", L"今天", L"健身", L"了吗"}) {
        s.begin(time);
        s.append(part, time++);
        for (int echo = 0; echo < 3; ++echo) {
            if (breaksSentence(RangeEvidence::Matches, RangeEvidence::Matches)) s.clear();
            if (breaksSentence(RangeEvidence::Unknown, RangeEvidence::Unknown)) s.clear();
        }
    }
    assert(s.text == L"你好今天健身了吗");
    assert(breaksSentence(RangeEvidence::Different, RangeEvidence::Matches));
    assert(breaksSentence(RangeEvidence::Matches, RangeEvidence::Different));
    assert(!breaksSentence(RangeEvidence::Unknown, RangeEvidence::Matches));
    // Exercise the same layout code used by the native GDI panel, at 100–250%.
    for (int dpi : {96, 120, 144, 192, 240}) {
        auto measure = [dpi](const std::wstring &v, size_t a, size_t n, PanelFont font) {
            int width = 0;
            for (size_t i = a; i < a + n; ++i)
                width += v[i] < 128 ? (font == PanelFont::English ? 8 : 10) : 21;
            return (width * dpi + 48) / 96;
        };
        std::vector<Candidate> choices{{L"这个样", 3}, {L"这个", 2}, {L"这歌", 2}, {L"这", 1}, {L"着", 1}};
        auto compact = layoutPanel(L"That's it.", choices, 0, dpi, 1000 * dpi / 96, 700 * dpi / 96, measure);
        assert(compact.width < 500 * dpi / 96 && compact.height <= 100 * dpi / 96);
        auto englishOnly = layoutPanel(L"Looks like", {}, 0, dpi, 1000 * dpi / 96, 700 * dpi / 96, measure);
        assert(englishOnly.width < 250 * dpi / 96 && englishOnly.height <= 46 * dpi / 96);
        choices[0].text = L"今天健身了吗你好今天健身了吗你好今天健身了吗你好今天健身了吗";
        auto full = layoutPanel(L"Hello, did you work out today?", choices, 0, dpi, 320 * dpi / 96, 110 * dpi / 96, measure);
        assert(full.width <= 320 * dpi / 96 && full.height <= 110 * dpi / 96);
        assert(full.contentHeight > full.height);
        for (const auto &choice : full.choices) {
            std::wstring rebuilt;
            for (const auto &line : choice.lines) {
                rebuilt += choices[choice.index].text.substr(line.start, line.length);
                assert(line.rect.right <= full.width);
            }
            assert(rebuilt == choices[choice.index].text); // no omitted characters / ellipsis
        }
        std::wstring longEnglish = L"Hello, did you work out today? This complete sentence wraps across multiple lines without dropping any words.";
        auto wrapped = layoutPanel(longEnglish, {}, 0, dpi, 250 * dpi / 96, 100 * dpi / 96, measure);
        std::wstring rebuilt;
        for (const auto &line : wrapped.english) rebuilt += longEnglish.substr(line.start, line.length);
        assert(rebuilt == longEnglish);
    }
    std::cout << "Sentence continuity, complete candidates, adaptive layout, expiry, Shift and placement checks passed\n";
}
