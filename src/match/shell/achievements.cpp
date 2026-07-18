#include "achievements.h"

#include "core/achievements.h"
#include "defines.h"
#include "match/state/entity_data.h"
#include "match/state/match.h"
#include "match/state/upgrade.h"
#include "network/network.h"

static const uint32_t ACHIEVEMENT_CLAIM_JUMPER_COLLAPSED_MINE_COUNT = 3U;
static const uint32_t ACHIEVEMENT_MIND_YOUR_STEP_TARGET_KILL_COUNT = 10U;
static const uint32_t ACHIEVEMENT_MINESWEEPER_TARGET_LANDMINES_COUNT = 20U;
static const uint32_t ACHIEVEMENT_BUNKER_BUSTER_TARGET_BUNKER_COUNT = 4U;
static const uint32_t ACHIEVEMENT_UNDERCOVER_TARGET_KILL_COUNT = 8U;
static const uint32_t ACHIEVEMENT_DRIVE_BY_SHOOTING_TARGET_KILL_COUNT = 3U;

static const uint32_t ACHIEVEMENTS_RECORD_DOES_NOT_EXPIRE = UINT32_MAX;
static const uint32_t ACHIEVEMENTS_RECORD_SPECIAL_DELIVERY_EXPIRATION_DURATION = 15U * UPDATES_PER_SECOND;
static const uint32_t ACHIEVEMENTS_RECORD_DRIVE_BY_WAGON_EXPIRATION_DURATION = 15U * UPDATES_PER_SECOND;
static const uint32_t ACHIEVEMENTS_RECORD_FIRE_EXPIRATION_DURATION = 5U * UPDATES_PER_SECOND;

AchievementsTracker achievements_tracker_init(AchievementsMatchType match_type) {
    AchievementsTracker tracker;

    tracker.match_type = match_type;
    tracker.goldmines_collapsed = 0;
    tracker.landmines_killed = 0;
    tracker.bunkers_busted = 0;
    tracker.units_killed_with_landmines = 0;
    memset(tracker.player_has_created_entity, 0, sizeof(tracker.player_has_created_entity));

    return tracker;
}

void achievements_tracker_handle_event(AchievementsTracker& tracker, const MatchState& state, const MatchEvent& event, uint32_t match_timer) {
    // Don't grant achievements unless there is an opponent
    if (!match_player_has_at_least_one_active_opponent(state, network_get_player_id())) {
        return;
    }

    switch (event.type) {
        case MATCH_EVENT_ALERT: {
            switch (event.alert.type) {
                // Claim Jumper - Get 3 goldmines
                case MATCH_ALERT_TYPE_MINE_COLLAPSE: {
                    if (event.alert.player_id == network_get_player_id()) {
                        tracker.goldmines_collapsed++;
                        if (tracker.goldmines_collapsed == ACHIEVEMENT_CLAIM_JUMPER_COLLAPSED_MINE_COUNT) {
                            achievement_grant(ACHIEVEMENT_CLAIM_JUMPER);
                        }
                    }
                    break;
                }
                // Best of the Best - Research all available upgrades
                case MATCH_ALERT_TYPE_RESEARCH: {
                    if (event.alert.player_id != network_get_player_id()) {
                        break;
                    }

                    // Get all upgrades into a single bitset
                    uint32_t all_upgrades = 0U;
                    for (uint32_t upgrade_index = 0; upgrade_index < UPGRADE_COUNT; upgrade_index++) {
                        uint32_t upgrade_flag = 1U << upgrade_index;
                        all_upgrades |= upgrade_flag;
                    }

                    // Compare the players upgrades to all upgrades
                    if (state.players[network_get_player_id()].upgrades == all_upgrades) {
                        achievement_grant(ACHIEVEMENT_BEST_OF_THE_BEST);
                    }
                    break;
                }
                case MATCH_ALERT_TYPE_UNIT:
                case MATCH_ALERT_TYPE_BUILDING: {
                    if (event.alert.player_id != network_get_player_id()) {
                        break;
                    }

                    tracker.player_has_created_entity[event.alert.entity_type] = true;

                    if (event.alert.type == MATCH_ALERT_TYPE_UNIT) {

                        // Ragtag Outfit - Train one of every unit type
                        bool player_has_trained_all_unit_types = true;
                        for (uint32_t entity_type = ENTITY_MINER; entity_type < ENTITY_HALL; entity_type++) {
                            if (!tracker.player_has_created_entity[entity_type]) {
                                player_has_trained_all_unit_types = false;
                                break;
                            }
                        }
                        if (player_has_trained_all_unit_types) {
                            achievement_grant(ACHIEVEMENT_RAGTAG_OUTFIT);
                        }

                        // Maxed Out - Reach max population
                        if (match_get_player_population(state, network_get_player_id()) == MATCH_MAX_POPULATION) {
                            achievement_grant(ACHIEVEMENT_MAXED_OUT);
                        }
                    }

                    // Greedy Opener - Player built Hall before Saloon
                    if (event.alert.entity_type == ENTITY_HALL && !tracker.player_has_created_entity[ENTITY_SALOON]) {
                        achievement_grant(ACHIEVEMENT_GREEDY_OPENER);
                    }

                    break;
                }
                default: {
                    break;
                }
            }
            break;
        }
        case MATCH_EVENT_PLAYER_DEFEATED: {
            // High Noon - Defeat AI on hard
            if (tracker.match_type == ACHIEVEMENTS_MATCH_TYPE_SKIRMISH &&
                    network_get_player(event.player_defeated.player_id).status == NETWORK_PLAYER_STATUS_BOT &&
                    network_get_match_setting(MATCH_SETTING_DIFFICULTY) == DIFFICULTY_HARD) {
                achievement_grant(ACHIEVEMENT_HIGH_NOON);
            }
            break;
        }
        case MATCH_EVENT_ENTITY_KILLED: {
            const Entity& attacker = state.entities.get_by_id(event.entity_killed.attacker_id);
            const Entity& defender = state.entities.get_by_id(event.entity_killed.defender_id);
            // The attack must be player-controlled and the defender must have been an enemy unit
            if (attacker.player_id != network_get_player_id() ||
                    state.players[attacker.player_id].team == state.players[defender.player_id].team) {
                break;
            }

            // Hold Your Horses - Kill a Wagon or Jockey with a Bandit
            if (attacker.type == ENTITY_BANDIT && (defender.type == ENTITY_WAGON || defender.type == ENTITY_JOCKEY)) {
                achievement_grant(ACHIEVEMENT_HOLD_YOUR_HORSES);
                break;
            }

            // Bunker Buster - Kill X bunkers with sappers in a match
            if (attacker.type == ENTITY_SAPPER && defender.type == ENTITY_BUNKER) {
                tracker.bunkers_busted++;
                if (tracker.bunkers_busted == ACHIEVEMENT_BUNKER_BUSTER_TARGET_BUNKER_COUNT) {
                    achievement_grant(ACHIEVEMENT_BUNKER_BUSTER);
                }

                break;
            }

            // Minesweeper - Kill X landmines in a match
            if (defender.type == ENTITY_LANDMINE) {
                tracker.landmines_killed++;
                if (tracker.landmines_killed == ACHIEVEMENT_MINESWEEPER_TARGET_LANDMINES_COUNT) {
                    achievement_grant(ACHIEVEMENT_MINESWEEPER);
                }

                break;
            }

            // Watch your step - Kill X units with landmines in a match
            if (attacker.type == ENTITY_LANDMINE) {
                tracker.units_killed_with_landmines++;
                if (tracker.units_killed_with_landmines == ACHIEVEMENT_MIND_YOUR_STEP_TARGET_KILL_COUNT) {
                    achievement_grant(ACHIEVEMENT_MIND_YOUR_STEP);
                }

                break;
            }

            // Undercover - Kill X miners with detectives in a match
            if (attacker.type == ENTITY_DETECTIVE && defender.type == ENTITY_MINER) {
                // Find an existing unit record
                size_t record_index = 0;
                while (record_index < tracker.records.size() &&
                        !achievements_record_tracks_unit(
                            tracker.records[record_index],
                            event.entity_killed.attacker_id)) {
                    record_index++;
                }

                // If we don't find a unit track, then create one
                if (record_index == tracker.records.size()) {
                    AchievementsRecord record;
                    record.type = ACHIEVEMENTS_RECORD_TYPE_UNIT;
                    record.expiration_time = ACHIEVEMENTS_RECORD_DOES_NOT_EXPIRE;
                    record.unit.entity_id = event.entity_killed.attacker_id;
                    record.unit.kills = 0;
                    tracker.records.push_back(record);
                }

                AchievementsRecord& record = tracker.records[record_index];
                record.unit.kills++;

                if (record.unit.kills == ACHIEVEMENT_UNDERCOVER_TARGET_KILL_COUNT) {
                    achievement_grant(ACHIEVEMENT_UNDERCOVER);

                    // Mark the unit track to be cleaned up
                    // This is not super necessary, but there's no need to keep
                    // this in memory once we've gotten the achievement
                    tracker.records[record_index].expiration_time = match_timer;
                }

                break;
            }

            // Drive By Shooting - Kill X units with the same wagon in short succession
            if (attacker.garrison_id != ID_NULL &&
                    state.entities.get_by_id(attacker.garrison_id).type == ENTITY_WAGON &&
                    entity_is_unit(defender.type)) {
                // Find an existing unit track
                size_t record_index = 0;
                while (record_index < tracker.records.size() &&
                        !achievements_record_tracks_unit(tracker.records[record_index], attacker.garrison_id)) {
                    record_index++;
                }

                // If we don't find a unit track, then create one
                if (record_index == tracker.records.size()) {
                    AchievementsRecord record;
                    record.type = ACHIEVEMENTS_RECORD_TYPE_UNIT;
                    record.expiration_time = match_timer + ACHIEVEMENTS_RECORD_DRIVE_BY_WAGON_EXPIRATION_DURATION;
                    record.unit.entity_id = attacker.garrison_id;
                    record.unit.kills = 0;
                    tracker.records.push_back(record);
                }

                AchievementsRecord& record = tracker.records[record_index];
                record.unit.kills++;

                if (record.unit.kills == ACHIEVEMENT_DRIVE_BY_SHOOTING_TARGET_KILL_COUNT) {
                    achievement_grant(ACHIEVEMENT_DRIVE_BY_SHOOTING);

                    // Mark the unit track to be cleaned up
                    // This is not super necessary, but there's no need to keep
                    // this in memory once we've gotten the achievement
                    tracker.records[record_index].expiration_time = match_timer;
                }

                break;
            }

            // Special delivery - Kill an enemy unit or building using a sapper that has recently been unloaded
            if (attacker.type == ENTITY_SAPPER) {
                // Find an existing unit track
                size_t record_index = 0;
                while (record_index < tracker.records.size() &&
                        !achievements_record_tracks_unit(
                            tracker.records[record_index],
                            event.entity_killed.attacker_id)) {
                    record_index++;
                }

                if (record_index < tracker.records.size()) {
                    achievement_grant(ACHIEVEMENT_SPECIAL_DELIVERY);

                    // Mark the unit track to be cleaned up
                    // This is not super necessary, but there's no need to keep
                    // this in memory once we've gotten the achievement
                    tracker.records[record_index].expiration_time = match_timer;
                }

                break;
            }

            break;
        }
        case MATCH_EVENT_BUILDING_CANCELLED: {
            const Entity& building = state.entities.get_by_id(event.building_cancelled.building_id);

            // Expansion Denied - Force a cancel on a town hall
            if (building.type == ENTITY_HALL &&
                    state.players[network_get_player_id()].team !=
                    state.players[building.player_id].team) {
                // Check if there is an allied unit nearby
                EntityId allied_unit = match_find_entity(state, [&building](const Entity& entity, EntityId /*entity_id */) {
                    return entity.player_id == network_get_player_id() &&
                        entity_is_unit(entity.type) &&
                        entity.health != 0 &&
                        ivec2::manhattan_distance(entity.cell, building.cell) <= 16;
                });
                if (allied_unit != ID_NULL) {
                    achievement_grant(ACHIEVEMENT_EXPANSION_DENIED);
                }
            }

            break;
        }
        case MATCH_EVENT_UNIT_UNLOADED: {
            const Entity& unit = state.entities.get_by_id(event.unit_unloaded.unit_id);

            // Ignore units that aren't allied sappers
            if (unit.player_id != network_get_player_id() || unit.type != ENTITY_SAPPER) {
                break;
            }

            // Find an existing unit track
            // (we will be creating a new one anyways, but if there is an
            // existing one for this unit, we will replace it)
            size_t record_index = 0;
            while (record_index < tracker.records.size() &&
                    !achievements_record_tracks_unit(
                        tracker.records[record_index],
                        event.unit_unloaded.unit_id)) {
                record_index++;
            }

            // If there is no existing unit track, create one
            // but don't modify it yet, it will be newed up below
            if (record_index == tracker.records.size()) {
                AchievementsRecord record;
                tracker.records.push_back(record);
            }

            AchievementsRecord& record = tracker.records[record_index];
            record.type = ACHIEVEMENTS_RECORD_TYPE_UNIT;
            record.expiration_time = match_timer + ACHIEVEMENTS_RECORD_SPECIAL_DELIVERY_EXPIRATION_DURATION;
            record.unit.entity_id = event.unit_unloaded.unit_id;
            record.unit.kills = 0; // Not used, but doesn't hurt to zero it

            break;
        }
        case MATCH_EVENT_CELL_SET_ON_FIRE: {
            // Ignore this event if it's not a player-created fire
            if (event.cell_set_on_fire.source_player_id != network_get_player_id()) {
                break;
            }

            // Check if the fire cell set a building on fire
            Cell map_cell = map_get_cell(state.map, CELL_LAYER_GROUND, event.cell_set_on_fire.cell);
            if (map_cell.type != CELL_BUILDING) {
                log_debug("ACHIEVEMENTS - Cell <%i, %i> source <%i, %i> %u is not a building.", event.cell_set_on_fire.cell.x, event.cell_set_on_fire.cell.y, event.cell_set_on_fire.source_cell.x, event.cell_set_on_fire.source_cell.y, event.cell_set_on_fire.source_player_id);
                break;
            }

            // Check if the on-fire building is an enemy, and check that it is even on fire
            // (it might not be on fire, if it's a bunker)
            EntityId building_id = map_cell.id;
            const Entity& building = state.entities.get_by_id(building_id);
            if (!entity_check_flag(building, ENTITY_FLAG_ON_FIRE) ||
                    state.players[building.player_id].team ==
                    state.players[network_get_player_id()].team) {
                log_debug("ACHIEVEMENTS - Cell is a building but either the building is not on fire or the building is allied.");
                if (!entity_check_flag(building, ENTITY_FLAG_ON_FIRE)) {
                    log_debug("ACHIEVEMENTS - Cell <%i, %i> source <%i, %i> %u. Building %u is not on fire.", event.cell_set_on_fire.cell.x, event.cell_set_on_fire.cell.y, event.cell_set_on_fire.source_cell.x, event.cell_set_on_fire.source_cell.y, event.cell_set_on_fire.source_player_id, building_id);
                } else {
                    log_debug("ACHIEVEMENTS - Cell <%i, %i> source <%i, %i> %u. Building %u is not enemy.", event.cell_set_on_fire.cell.x, event.cell_set_on_fire.cell.y, event.cell_set_on_fire.source_cell.x, event.cell_set_on_fire.source_cell.y, event.cell_set_on_fire.source_player_id, building_id);
                }
                break;
            }

            // Find an existing record
            size_t record_index = 0;
            while (record_index < tracker.records.size() &&
                        !achievements_record_tracks_fire(
                            tracker.records[record_index],
                            event.cell_set_on_fire.source_cell,
                            event.cell_set_on_fire.source_player_id)) {
                record_index++;
            }

            // If no record found, create a new one
            if (record_index == tracker.records.size()) {
                AchievementsRecord record;
                record.type = ACHIEVEMENTS_RECORD_TYPE_FIRE;
                record.expiration_time = match_timer + ACHIEVEMENTS_RECORD_FIRE_EXPIRATION_DURATION;
                record.fire.source_cell = event.cell_set_on_fire.source_cell;
                record.fire.source_player_id = event.cell_set_on_fire.source_player_id;
                for (uint32_t index = 0; index < ACHIEVEMENT_ARSONIST_TARGET_BUILDINGS_LIT_COUNT; index++) {
                    record.fire.buildings_lit[index] = ID_NULL;
                }

                tracker.records.push_back(record);
            }

            AchievementsRecord& record = tracker.records[record_index];

            // Don't count the same building twice
            if (achievements_record_fire_has_lit_building(record, building_id)) {
                log_debug("ACHIEVEMENTS - Cell <%i, %i> source <%i, %i> %u. Building %u Record %u. Building is already accounted for.", event.cell_set_on_fire.cell.x, event.cell_set_on_fire.cell.y, event.cell_set_on_fire.source_cell.x, event.cell_set_on_fire.source_cell.y, event.cell_set_on_fire.source_player_id, building_id, record_index);
                break;
            }

            achievements_record_fire_add_lit_building(record, building_id);
            log_debug("ACHIEVEMENTS - Cell <%i, %i> source <%i, %i> %u. Building %u Record %u. Building added. Count is now %u.", event.cell_set_on_fire.cell.x, event.cell_set_on_fire.cell.y, event.cell_set_on_fire.source_cell.x, event.cell_set_on_fire.source_cell.y, event.cell_set_on_fire.source_player_id, building_id, record_index, achievements_record_fire_buildings_lit_count(record));
            if (achievements_record_fire_buildings_lit_count(record) == ACHIEVEMENT_ARSONIST_TARGET_BUILDINGS_LIT_COUNT) {
                achievement_grant(ACHIEVEMENT_ARSONIST);

                // Mark the unit track to be cleaned up
                // This is not super necessary, but there's no need to keep
                // this in memory once we've gotten the achievement
                tracker.records[record_index].expiration_time = match_timer;
            }

            break;
        }
        default: {
            break;
        }
    }
}

void achievements_tracker_update(AchievementsTracker& tracker, const MatchState& state, uint32_t match_timer) {
    size_t record_index = 0;
    while (record_index < tracker.records.size()) {
        const AchievementsRecord& track = tracker.records[record_index];
        if (achievements_record_is_expired(track, state, match_timer)) {
            tracker.records[record_index] = tracker.records.back();
            tracker.records.pop_back();
        } else {
            record_index++;
        }
    }
}

bool achievements_record_is_expired(const AchievementsRecord& record, const MatchState& state, uint32_t match_timer) {
    // If the tracked entity is dead, then this is expired
    if (record.type == ACHIEVEMENTS_RECORD_TYPE_UNIT) {
        uint32_t entity_index = state.entities.get_index_of(record.unit.entity_id);
        if (entity_index == INDEX_INVALID || state.entities[entity_index].health == 0) {
            return true;
        }
    }

    // Otherwise, this has expired only if the expiration time has passed
    return record.expiration_time < match_timer;
}

bool achievements_record_tracks_unit(const AchievementsRecord& record, EntityId entity_id) {
    if (record.type != ACHIEVEMENTS_RECORD_TYPE_UNIT) {
        return false;
    }
    return record.unit.entity_id == entity_id;
}

bool achievements_record_tracks_fire(const AchievementsRecord& record, ivec2 source_cell, uint8_t source_player_id) {
    if (record.type != ACHIEVEMENTS_RECORD_TYPE_FIRE) {
        return false;
    }
    return record.fire.source_cell == source_cell && record.fire.source_player_id == source_player_id;
}

uint32_t achievements_record_fire_buildings_lit_count(const AchievementsRecord& record) {
    GOLD_ASSERT(record.type == ACHIEVEMENTS_RECORD_TYPE_FIRE);
    uint32_t count = 0;
    while (count < ACHIEVEMENT_ARSONIST_TARGET_BUILDINGS_LIT_COUNT && record.fire.buildings_lit[count] != ID_NULL) {
        count++;
    }

    return count;
}

bool achievements_record_fire_has_lit_building(const AchievementsRecord& record, EntityId building_id) {
    GOLD_ASSERT(record.type == ACHIEVEMENTS_RECORD_TYPE_FIRE);

    for (uint32_t index = 0; index < ACHIEVEMENT_ARSONIST_TARGET_BUILDINGS_LIT_COUNT; index++) {
        if (record.fire.buildings_lit[index] == building_id) {
            return true;
        }
    }

    return false;
}

void achievements_record_fire_add_lit_building(AchievementsRecord& record, EntityId building_id) {
    uint32_t index = 0;
    while (index < ACHIEVEMENT_ARSONIST_TARGET_BUILDINGS_LIT_COUNT && record.fire.buildings_lit[index] != ID_NULL) {
        index++;
    }

    GOLD_ASSERT(index < ACHIEVEMENT_ARSONIST_TARGET_BUILDINGS_LIT_COUNT);
    if (index == ACHIEVEMENT_ARSONIST_TARGET_BUILDINGS_LIT_COUNT) {
        log_warn("Cannot add building ID %u to fire record because record is already full.");
        return;
    }

    record.fire.buildings_lit[index] = building_id;
}
