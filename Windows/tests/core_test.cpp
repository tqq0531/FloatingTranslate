#include "../src/Core.h"
#include "../src/EnglishProofreader.h"
#include "../src/PanelLayout.h"
#include "../src/CandidateRanking.h"
#include <cassert>
#include <iostream>
using namespace fuyi;
int main() {
    EnglishProofreader proof;
    auto advice=proof.check(L"I want go to gym now");
    assert(advice.corrected==L"I want to go to the gym now");
    assert(proof.check(L"I want to go to the gym now").corrected==L"I want to go to the gym now");
    assert(proof.check(L"She have a apple").corrected==L"She has an apple");
    assert(proof.check(L"i recieve teh book ").corrected==L"I receive the book ");
    assert(proof.check(L"https://example.com/teh").corrected==L"https://example.com/teh");
    assert(proof.check(std::wstring(1001,L'x')).corrected==std::wstring(1001,L'x'));
    assert(retainEnglishPrefix(true,false,RangeEvidence::Matches,RangeEvidence::Matches,RangeEvidence::Unknown));
    assert(!retainEnglishPrefix(true,true,RangeEvidence::Matches,RangeEvidence::Matches,RangeEvidence::Matches));
    assert(!retainEnglishPrefix(false,false,RangeEvidence::Matches,RangeEvidence::Matches,RangeEvidence::Matches));
    assert(!retainEnglishPrefix(true,false,RangeEvidence::Different,RangeEvidence::Matches,RangeEvidence::Matches));
    assert(!retainEnglishPrefix(true,false,RangeEvidence::Matches,RangeEvidence::Different,RangeEvidence::Matches));
    assert(!retainEnglishPrefix(true,false,RangeEvidence::Matches,RangeEvidence::Matches,RangeEvidence::Different));
    assert(translationOnCommit(false, true, false, false));
    assert(!translationOnCommit(false, false, true, false));
    assert(translationOnCommit(true, false, true, false));
    assert(!translationOnCommit(true, true, false, false));
    assert(!translationOnCommit(false, true, true, true));
    assert(!translationOnCommit(true, true, true, true));
    assert(translationOnCommit(true, false, false, true));
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
    // Personal ordering follows explicit short selections and must preserve
    // consumed input lengths when an alternate candidate moves to the front.
    CandidateRanking personal;
    std::vector<Candidate> alternatives{{L"是", 3}, {L"时", 3}, {L"事", 3}};
    personal.add("shi", {L"事", 3});
    personal.add("shi", {L"事", 3});
    personal.add("shi", {L"时", 3});
    personal.rank("shi", alternatives);
    assert(alternatives[0].text == L"事" && alternatives[0].consumed == 3);
    std::vector<Candidate> otherSpelling{{L"是", 2}, {L"事", 2}};
    personal.rank("si", otherSpelling);
    assert(otherSpelling.front().text == L"是"); // No frequency borrowed from "shi".
    assert(!memorableCandidate("pass", L"password"));
    assert(!memorableCandidate("123", L"字"));
    assert(!memorableCandidate("nihaojintianjianshenlema", L"你好今天健身了吗"));
    std::vector<Candidate> sentenceChoices{{L"是什么", 7}, {L"事", 3}};
    personal.rank("shishenme", sentenceChoices);
    assert(sentenceChoices.front().text == L"是什么");
    CandidateRanking restored;
    for (const auto &entry : personal.entries())
        restored.add(entry.pinyin, {entry.text, entry.pinyin.size()}, entry.count);
    std::vector<Candidate> again{{L"是", 3}, {L"事", 3}};
    restored.rank("shi", again);
    assert(again.front().text == L"事");
    assert(candidatePage(0, 13, true) == 5);
    assert(candidatePage(8, 13, true) == 10);
    assert(candidatePage(10, 13, true) == 10);
    assert(candidatePage(12, 13, false) == 5);
    assert(candidatePage(0, 13, false) == 0);
    assert(candidatePage(0, 0, true) == 0);
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
    for(int dpi:{96,120,144,192,240}) {
        auto measure=[dpi](const std::wstring &,size_t,size_t n,PanelFont){return int(n)*7*dpi/96;};
        std::wstring advice=L"Basic correction: I want to go to the gym now and then meet my friends after a long workout.";
        auto panel=layoutPanel(L"Chinese translation",{},0,dpi,800*dpi/96,600*dpi/96,measure,advice);
        std::wstring shown;for(const auto &line:panel.proof)shown+=advice.substr(line.start,line.length);
        assert(shown==advice && panel.contentHeight>panel.applyAdvice.top);
        assert(panel.applyAdvice.left>=0 && panel.applyAdvice.right<panel.copyAdvice.left);
        assert(panel.copyAdvice.right<=panel.width && panel.copyAdvice.bottom<=panel.contentHeight);
        assert(panel.height<=600*dpi/96);
    }
    std::cout << "Sentence continuity, complete candidates, adaptive layout, expiry, Shift and placement checks passed\n";
}
