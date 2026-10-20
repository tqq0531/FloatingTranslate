#include "FuyiRime.h"
#include <rime_api.h>
#include <string.h>
#include <stdlib.h>
static RimeApi *api;
static RimeSessionId session;
int FuyiRimeInitialize(const char *shared_dir, const char *user_dir) {
    if (session) return 1;
    api = rime_get_api();
    RIME_STRUCT(RimeTraits, traits);
    traits.shared_data_dir = shared_dir;
    traits.user_data_dir = user_dir;
    traits.distribution_name = "Fuyi";
    traits.distribution_code_name = "fuyi";
    traits.distribution_version = "0.5.0";
    traits.app_name = "rime.fuyi";
    traits.min_log_level = 3;
    traits.log_dir = "";
    api->setup(&traits);
    api->initialize(&traits);
    session = api->create_session();
    if (!session || !api->select_schema(session, "fuyi")) { FuyiRimeShutdown(); return 0; }
    api->set_option(session, "ascii_mode", 0);
    api->set_option(session, "_no_learning", 1);
    atexit(FuyiRimeShutdown);
    return 1;
}
void FuyiRimeShutdown(void) { if (session) { api->destroy_session(session); session = 0; api->finalize(); } }
void FuyiRimeClear(void) {
    if (!session) return;
    api->clear_composition(session);
    RIME_STRUCT(RimeCommit, pending);
    if (api->get_commit(session, &pending)) api->free_commit(&pending);
}
const char *FuyiRimeVersion(void) { return api ? api->get_version() : "unavailable"; }
static void input(const char *raw) {
    api->clear_composition(session);
    RIME_STRUCT(RimeCommit, pending);
    if (api->get_commit(session, &pending)) api->free_commit(&pending);
    api->set_input(session, raw);
}
static int remaining_letters(const char *text) {
    int count = 0;
    if (!text) return 0;
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
        if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z')) ++count;
        else if (*p == 0xC3 && p[1] == 0xBC) { ++count; ++p; } // ü is one typed v
    }
    return count;
}
int FuyiRimeQuery(const char *raw, char **texts, int *consumed, int capacity) {
    if (!session || !raw || !*raw || capacity <= 0) return 0;
    input(raw);
    RIME_STRUCT(RimeContext, context);
    if (!api->get_context(session, &context)) return 0;
    int total = context.menu.num_candidates < capacity ? context.menu.num_candidates : capacity;
    for (int i = 0; i < total; ++i) texts[i] = strdup(context.menu.candidates[i].text);
    api->free_context(&context);
    int length = (int)strlen(raw);
    for (int i = 0; i < total; ++i) {
        input(raw);
        if (!api->select_candidate(session, (size_t)i)) { consumed[i] = 0; continue; }
        RIME_STRUCT(RimeContext, selected);
        int tail = 0;
        if (api->get_context(session, &selected)) {
            tail = remaining_letters(selected.composition.preedit);
            api->free_context(&selected);
        }
        int position = length;
        while (position > 0 && tail > 0) {
            --position;
            if (raw[position] >= 'a' && raw[position] <= 'z') --tail;
        }
        while (position < length && raw[position] == '\'') ++position;
        consumed[i] = position;
    }
    FuyiRimeClear();
    return total;
}
void FuyiRimeFreeTexts(char **texts, int count) { for (int i = 0; i < count; ++i) free(texts[i]); }
