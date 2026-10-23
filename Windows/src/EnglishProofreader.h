#pragma once
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>
namespace fuyi {
struct EnglishAdvice { std::wstring corrected; std::vector<std::wstring> notes; };
// Bounded offline spelling + common grammar rules. Suggestions, never a claim
// that arbitrary English is correct. No document reads, network or disk writes.
class EnglishProofreader {
    std::unordered_map<std::wstring, uint64_t> words_;
    static bool letter(wchar_t c) { return (c>=L'a'&&c<=L'z')||(c>=L'A'&&c<=L'Z'); }
    static std::wstring lower(std::wstring s) { for(auto &c:s) if(c>=L'A'&&c<=L'Z') c+=32; return s; }
    std::wstring spelling(const std::wstring &word) const {
        if(words_.count(word)||word.size()<4||word.size()>24) return word;
        // Adjacent letter swaps are a frequent typing error. Prefer a unique
        // common transposition over a more frequent but unrelated substitution.
        std::wstring swapped;
        for(size_t i=0;i+1<word.size();++i){
            auto candidate=word;std::swap(candidate[i],candidate[i+1]);
            auto it=words_.find(candidate);
            if(it!=words_.end()&&it->second>=1000&&candidate!=word){
                if(!swapped.empty()&&swapped!=candidate){swapped.clear();break;}
                swapped=candidate;
            }
        }
        if(!swapped.empty())return swapped;
        std::wstring best=word; uint64_t score=0, runner=0;
        auto consider=[&](const std::wstring &candidate){
            auto it=words_.find(candidate); if(it==words_.end()||candidate==best) return;
            if(it->second>score){runner=score;score=it->second;best=candidate;}
            else runner=std::max(runner,it->second);
        };
        for(size_t i=0;i<word.size();++i){
            auto candidate=word;candidate.erase(i,1);consider(candidate);
            if(i+1<word.size()){candidate=word;std::swap(candidate[i],candidate[i+1]);consider(candidate);}
            for(wchar_t c=L'a';c<=L'z';++c){candidate=word;candidate[i]=c;consider(candidate);}
        }
        for(size_t i=0;i<=word.size();++i) for(wchar_t c=L'a';c<=L'z';++c){auto candidate=word;candidate.insert(i,1,c);consider(candidate);}
        // Ambiguous words and names remain unchanged. User must accept any advice.
        return score>=1000 && (runner==0||score>=runner*3) ? best : word;
    }
  public:
    bool load(const std::filesystem::path &path) {
        std::ifstream file(path);std::string word;uint64_t count;words_.clear();
        while(words_.size()<60000 && file>>word>>count){
            if(word.size()>30||!std::all_of(word.begin(),word.end(),[](unsigned char c){return c>='a'&&c<='z';}))continue;
            words_.emplace(std::wstring(word.begin(),word.end()),count);
        }
        return words_.size()>10000;
    }
    EnglishAdvice check(const std::wstring &source) const {
        EnglishAdvice result{source,{}};
        if(source.size()>1000) return result;
        // Skip URLs, addresses, paths and code-like input rather than rewrite it.
        if(source.find(L"://")!=source.npos||source.find(L'@')!=source.npos||source.find(L'\\')!=source.npos||source.find(L'_')!=source.npos)return result;
        std::wstring text; bool spellingChanged=false;
        for(size_t start=0;start<source.size();){
            if(!letter(source[start])){text+=source[start++];continue;}
            size_t end=start;while(end<source.size()&&(letter(source[end])||source[end]==L'\''))++end;
            auto word=source.substr(start,end-start), normalized=lower(word), replacement=word;
            bool complete=end<source.size() || (!source.empty() && !letter(source.back()));
            // Do not suggest while the last word is still being typed.
            if(complete && word.find(L'\'')==word.npos && word==normalized){replacement=spelling(normalized);}
            if(normalized==L"teh"&&complete)replacement=L"the";
            if(normalized==L"recieve"&&complete)replacement=L"receive";
            if(normalized==L"alot"&&complete)replacement=L"a lot";
            if(replacement!=word)spellingChanged=true;
            text+=replacement;start=end;
        }
        if(spellingChanged)result.notes.push_back(L"拼写候选：请核对专名和原意");
        struct Rule { const wchar_t *pattern,*replacement,*note; };
        const Rule rules[]={
            {LR"(\bi\b)",L"I",L"人称代词 I 大写"},
            {LR"(\b(want|wants|wanted|need|needs|needed|plan|plans|planned|hope|hopes|hoped)\s+(go|buy|see|eat|learn|visit|work|study|get|have|be)\b)",L"$1 to $2",L"动词后补充 to 不定式"},
            {LR"(\b(go|going|went|come|coming)\s+to\s+gym\b)",L"$1 to the gym",L"go to the gym 通常需要冠词 the"},
            {LR"(\b(he|she|it)\s+have\b)",L"$1 has",L"第三人称单数通常使用 has"},
            {LR"(\b(he|she|it)\s+don't\b)",L"$1 doesn't",L"第三人称单数通常使用 doesn't"},
            {LR"(\b(you|we|they)\s+is\b)",L"$1 are",L"主语与 be 动词一致"},
            {LR"(\bI\s+is\b)",L"I am",L"I 通常搭配 am"},
            {LR"(\b(he|she|it)\s+go\b)",L"$1 goes",L"一般现在时第三人称单数"},
            {LR"(\b(did|didn't)\s+(went)\b)",L"$1 go",L"did 后使用动词原形"},
            {LR"(\b(can|could|will|would|should|must)\s+to\s+)",L"$1 ",L"情态动词后通常不加 to"},
            {LR"(\ba\s+(apple|orange|egg|idea|hour|umbrella)\b)",L"an $1",L"元音发音前使用 an"},
            {LR"(\b(the|to|a|an|is|are)\s+\1\b)",L"$1",L"检查重复单词"}
        };
        for(const auto &rule:rules){
            std::wregex pattern(rule.pattern,std::regex_constants::icase);
            auto changed=std::regex_replace(text,pattern,rule.replacement);
            if(changed!=text){result.notes.push_back(rule.note);text=std::move(changed);}
        }
        result.corrected=std::move(text);
        return result;
    }
};
}
