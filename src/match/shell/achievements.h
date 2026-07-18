#pragma once

#include "match/state/match.h"

enum AchievementsMatchType {
    ACHIEVEMENTS_MATCH_TYPE_SKIRMISH,
    ACHIEVEMENTS_MATCH_TYPE_SCENARIO
};

enum AchievementsRecordType {
    ACHIEVEMENTS_RECORD_TYPE_UNIT,
    ACHIEVEMENTS_RECORD_TYPE_FIRE
};

const uint32_t ACHIEVEMENT_ARSONIST_TARGET_BUILDINGS_LIT_COUNT = 3U;

struct AchievementsRecordUnit {
    EntityId entity_id;
    uint32_t kills;
};

struct AchievementsRecordFire {
    ivec2 source_cell;
    uint32_t source_player_id;
    EntityId buildings_lit[ACHIEVEMENT_ARSONIST_TARGET_BUILDINGS_LIT_COUNT];
};

struct AchievementsRecord {
    AchievementsRecordType type;
    uint32_t expiration_time;
    union {
        AchievementsRecordUnit unit;
        AchievementsRecordFire fire;
    };
};

struct AchievementsTracker {
    AchievementsMatchType match_type;
    uint32_t goldmines_collapsed;
    uint32_t landmines_killed;
    uint32_t bunkers_busted;
    uint32_t units_killed_with_landmines;
    bool player_has_created_entity[ENTITY_TYPE_COUNT];
    std::vector<AchievementsRecord> records;
};

AchievementsTracker achievements_tracker_init(AchievementsMatchType match_type);
void achievements_tracker_handle_event(AchievementsTracker& tracker, const MatchState& state, const MatchEvent& event, uint32_t match_timer);
void achievements_tracker_update(AchievementsTracker& tracker, const MatchState& state, uint32_t match_timer);

bool achievements_record_is_expired(const AchievementsRecord& record, const MatchState& state, uint32_t match_timer);
bool achievements_record_tracks_unit(const AchievementsRecord& record, EntityId entity_id);
bool achievements_record_tracks_fire(const AchievementsRecord& record, ivec2 source_cell, uint8_t source_player_id);
uint32_t achievements_record_fire_buildings_lit_count(const AchievementsRecord& record);
bool achievements_record_fire_has_lit_building(const AchievementsRecord& record, EntityId building_id);
void achievements_record_fire_add_lit_building(AchievementsRecord& record, EntityId building_id);
