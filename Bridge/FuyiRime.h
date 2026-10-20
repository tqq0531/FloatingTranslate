#ifndef FUYI_RIME_H
#define FUYI_RIME_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
int FuyiRimeInitialize(const char *shared_dir, const char *user_dir);
int FuyiRimeQuery(const char *raw, char **texts, int *consumed, int capacity);
void FuyiRimeFreeTexts(char **texts, int count);
void FuyiRimeClear(void);
void FuyiRimeShutdown(void);
const char *FuyiRimeVersion(void);
#ifdef __cplusplus
}
#endif
#endif
