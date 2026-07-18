#include "achievements.h"

#include "defines.h"

#ifdef GOLD_STEAM

#include "core/logger.h"

static const char* const ACHIEVEMENT_NAME[ACHIEVEMENT_COUNT] = {
#define X(name, str) str,
    ACHIEVEMENT_LIST
#undef X
};

void achievement_grant(Achievement achievement) {
    log_info("Achievement Get! - %s", ACHIEVEMENT_NAME[achievement]);
}

#else

void achievement_grant(Achievement /*achievement*/) {}

#endif
