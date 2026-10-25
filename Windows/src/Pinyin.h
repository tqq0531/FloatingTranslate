#pragma once
#include "Core.h"
#include "WinUtil.h"
#include "PersonalDictionary.h"
#include <mutex>
#include <rime_api.h>
namespace fuyi {
class Pinyin {
    std::recursive_mutex mutex_;
    RimeApi *api_ = nullptr;
    std::atomic<RimeSessionId> session_{0};
    HMODULE dll_ = nullptr;
    PersonalDictionary personal_;
    void input(const std::string &raw) {
        api_->clear_composition(session_);
        RIME_STRUCT(RimeCommit, c);
        if (api_->get_commit(session_, &c))
            api_->free_commit(&c);
        api_->set_input(session_, raw.c_str());
    }

  public:
    bool ready() const {
        return session_ != 0;
    }
    bool start() {
        std::lock_guard lock(mutex_);
        if (session_)
            return true;
        dll_ = loadLocal(L"librime.dll");
        if (!dll_)
            return false;
        auto get = (RimeApi * (*)()) GetProcAddress(dll_, "rime_get_api");
        if (!get)
            return false;
        api_ = get();
        auto shared = utf8((directory() / L"RimeData").wstring());
        auto runtime = pinyinRuntime();
        auto user = utf8(runtime.wstring());
        RIME_STRUCT(RimeTraits, traits);
        traits.shared_data_dir = shared.c_str();
        traits.user_data_dir = user.c_str();
        traits.distribution_name = "FloatingTranslate";
        traits.distribution_code_name = "fuyi";
        traits.distribution_version = "0.1.18";
        traits.app_name = "rime.fuyi";
        traits.min_log_level = 3;
        traits.log_dir = "";
        api_->setup(&traits);
        api_->initialize(&traits);
        session_ = api_->create_session();
        if (!session_ || !api_->select_schema(session_, "fuyi")) {
            session_ = 0;
            return false;
        }
        api_->set_option(session_, "ascii_mode", false);
        api_->set_option(session_, "_no_learning", true);
        return true;
    }
    std::vector<Candidate> query(const std::string &raw, bool lazy = false, size_t limit = 100) {
        std::lock_guard lock(mutex_);
        std::vector<Candidate> out;
        if (!session_ || raw.empty() || raw.size() > 100)
            return out;
        input(raw);
        RimeCandidateListIterator iterator{};
        if (!api_->candidate_list_begin(session_, &iterator)) return out;
        while (out.size() < limit && api_->candidate_list_next(&iterator)) {
            auto text = wide(iterator.candidate.text ? iterator.candidate.text : "");
            if (!text.empty() && std::none_of(text.begin(), text.end(), [](wchar_t ch) {
                    return (ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z');
                }))
                out.push_back({text, 0, (size_t)iterator.index});
            if (iterator.index >= 199) break; // Bound work even when candidates are filtered.
        }
        api_->candidate_list_end(&iterator);
        if (!lazy) for (auto &candidate : out) resolve(raw, candidate);
        if (!lazy) out.erase(std::remove_if(out.begin(), out.end(), [&](const Candidate &candidate) {
            return !candidate.consumed || candidate.consumed > raw.size();
        }), out.end());
        api_->clear_composition(session_);
        personal_.rank(raw, out, [&](Candidate &candidate) { resolve(raw, candidate); });
        return out;
    }
    void resolve(const std::string &raw, Candidate &candidate) {
        std::lock_guard lock(mutex_);
        if (candidate.consumed || !session_) return;
        input(raw);
        if (!api_->select_candidate(session_, candidate.rimeIndex)) {
            candidate.consumed = 0;
            api_->clear_composition(session_);
            return;
        }
        RIME_STRUCT(RimeContext, selected);
        int tail = 0;
        if (api_->get_context(session_, &selected)) {
            auto text = wide(selected.composition.preedit ? selected.composition.preedit : "");
            for (wchar_t ch : text)
                if ((ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z') || ch == L'ü')
                    tail++;
            api_->free_context(&selected);
        }
        size_t pos = raw.size();
        while (pos > 0 && tail > 0) {
            --pos;
            if (raw[pos] >= 'a' && raw[pos] <= 'z')
                --tail;
        }
        while (pos < raw.size() && raw[pos] == '\'')
            ++pos;
        candidate.consumed = pos;
        api_->clear_composition(session_);
    }
    void remember(const std::string &raw, const Candidate &candidate) {
        std::lock_guard lock(mutex_);
        personal_.remember(raw, candidate);
    }
    std::optional<std::wstring> preview(std::string raw) {
        std::wstring result;
        while (!raw.empty()) {
            auto list = query(raw, true);
            if (list.empty())
                return {};
            resolve(raw, list[0]);
            if (!list[0].consumed) return {};
            result += list[0].text;
            raw.erase(0, list[0].consumed);
        }
        return result;
    }
    // A process-wide Rime instance is intentionally retained; no finalization while TSF objects exist.
};
inline Pinyin &pinyin() {
    static auto *value = new Pinyin;
    return *value;
}
} // namespace fuyi
