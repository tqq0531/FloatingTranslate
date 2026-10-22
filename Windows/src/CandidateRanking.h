#pragma once
#include "Core.h"
#include <map>
namespace fuyi {
struct CandidateUse {
    std::string pinyin;
    std::wstring text;
    uint32_t count = 0;
};
inline bool memorableCandidate(const std::string &pinyin, const std::wstring &text) {
    if (pinyin.empty() || pinyin.size() > 32 || text.empty() || text.size() > 4) return false;
    for (char c : pinyin) if ((c < 'a' || c > 'z') && c != '\'') return false;
    // Store short Chinese selections only, never Latin strings, numbers or
    // arbitrary committed text (e.g. an identifier/password or a full sentence).
    for (wchar_t c : text) if (c < 0x3400 || c > 0x9fff) return false;
    return true;
}
class CandidateRanking {
    std::map<std::pair<std::string, std::wstring>, uint32_t> counts_;
  public:
    static constexpr size_t limit = 2048;
    void add(const std::string &raw, const Candidate &candidate, uint32_t increment = 1) {
        if (candidate.consumed > raw.size() || !candidate.consumed) return;
        auto prefix = raw.substr(0, candidate.consumed);
        if (!memorableCandidate(prefix, candidate.text)) return;
        auto key = std::make_pair(prefix, candidate.text);
        if (!counts_.count(key) && counts_.size() >= limit) {
            auto least = std::min_element(counts_.begin(), counts_.end(),
                [](const auto &a, const auto &b) { return a.second < b.second; });
            counts_.erase(least);
        }
        auto &count = counts_[key];
        count = (uint32_t)std::min<uint64_t>(1000000, (uint64_t)count + increment);
    }
    std::vector<CandidateUse> entries() const {
        std::vector<CandidateUse> out;
        for (const auto &[key, count] : counts_) out.push_back({key.first, key.second, count});
        return out;
    }
    void rank(const std::string &raw, std::vector<Candidate> &candidates) const {
        size_t longest = 0;
        for (const auto &c : candidates) longest = std::max(longest, c.consumed);
        auto score = [&](const Candidate &candidate) -> uint32_t {
            // A favourite prefix must not displace a complete phrase/sentence.
            if (candidate.consumed != longest || candidate.consumed > raw.size()) return 0;
            auto found = counts_.find({raw.substr(0, candidate.consumed), candidate.text});
            return found == counts_.end() ? 0 : found->second;
        };
        std::stable_sort(candidates.begin(), candidates.end(),
            [&](const auto &a, const auto &b) { return score(a) > score(b); });
    }
};
inline size_t candidatePage(size_t selected, size_t count, bool next) {
    if (!count) return 0;
    size_t page = std::min(selected, count - 1) / 5;
    size_t last = (count - 1) / 5;
    return (next ? std::min(page + 1, last) : (page ? page - 1 : 0)) * 5;
}
} // namespace fuyi
