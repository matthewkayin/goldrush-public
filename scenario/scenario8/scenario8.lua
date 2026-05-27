local objectives = require("objectives")
local actions = require("actions")
local entity_util = require("entity_util")
local squad_util = require("squad_util")
local entities = require("entities")
local ivec2 = require("ivec2")

local OBJECTIVE_DEFEAT_ENEMY = "Destroy the enemy base"

local ENEMY_PLAYER_ID = 1
local HARASS_INTERVAL = 90

local is_match_over = false

local next_harass_time = nil

local snake_bunker_states = {
    {
        bunker_id = scenario.constants.SNAKE_BUNKER1,
        has_fallen_back = false
    },
    {
        bunker_id = scenario.constants.SNAKE_BUNKER2,
        has_fallen_back = false
    }
}

function scenario_init()
    scenario.grant_player_upgrade(ENEMY_PLAYER_ID, scenario.upgrade.IRON_SIGHTS)
    scenario.grant_player_upgrade(ENEMY_PLAYER_ID, scenario.upgrade.BAYONETS)

    scenario.hold_camera()

    actions.run(function ()
        actions.wait(0.25)

        local fog_reveal_duration = 17
        scenario.fog_reveal({
            player_id = scenario.PLAYER_ID,
            cell = scenario.constants.INTRO_FOG1,
            cell_size = 1,
            sight = 17,
            duration = fog_reveal_duration
        })
        scenario.fog_reveal({
            player_id = scenario.PLAYER_ID,
            cell = scenario.constants.INTRO_CAM1,
            cell_size = 1,
            sight = 13,
            duration = fog_reveal_duration
        })
        scenario.fog_reveal({
            player_id = scenario.PLAYER_ID,
            cell = scenario.constants.INTRO_CAM2,
            cell_size = 1,
            sight = 19,
            duration = fog_reveal_duration
        })
        scenario.fog_reveal({
            player_id = scenario.PLAYER_ID,
            cell = scenario.constants.INTRO_FOG2,
            cell_size = 1,
            sight = 15,
            duration = fog_reveal_duration
        })
        scenario.fog_reveal({
            player_id = scenario.PLAYER_ID,
            cell = scenario.constants.INTRO_FOG3,
            cell_size = 1,
            sight = 15,
            duration = fog_reveal_duration
        })
        scenario.fog_reveal({
            player_id = scenario.PLAYER_ID,
            cell = scenario.constants.INTRO_FOG4,
            cell_size = 1,
            sight = 17,
            duration = fog_reveal_duration
        })
        scenario.fog_reveal({
            player_id = scenario.PLAYER_ID,
            cell = scenario.constants.INTRO_CAM3,
            cell_size = 1,
            sight = 13,
            duration = fog_reveal_duration
        })

        scenario.begin_camera_pan(scenario.constants.INTRO_CAM1, 5)
        actions.wait(0.5)
        scenario.chat("The enemy's position is heavily fortified.")

        repeat
            coroutine.yield()
        until scenario.get_camera_mode() == scenario.CAMERA_MODE_FREE

        scenario.begin_camera_pan(scenario.constants.INTRO_CAM2, 3)

        repeat
            coroutine.yield()
        until scenario.get_camera_mode() == scenario.CAMERA_MODE_FREE

        scenario.begin_camera_pan(scenario.constants.INTRO_CAM3, 8)
        actions.wait(0.5)
        scenario.chat("You'll have to push your way through in order to win.")

        repeat
            coroutine.yield()
        until scenario.get_camera_mode() == scenario.CAMERA_MODE_FREE

        actions.camera_pan(scenario.constants.INTRO_CAM4, 2)
        scenario.release_camera()

        actions.wait(0.5)
        objectives.announce_new_objective(OBJECTIVE_DEFEAT_ENEMY)
        objectives.add_objective({
            objective = {
                description = OBJECTIVE_DEFEAT_ENEMY
            },
            complete_fn = function ()
                return scenario.is_player_defeated(ENEMY_PLAYER_ID)
            end
        })

        actions.wait(5)
        scenario.hint("You can now hire soldiers and cannonneers, which are long-ranged siege units.")

        scenario.queue_match_input({
            player_id = ENEMY_PLAYER_ID,
            type = scenario.match_input_type.MOVE_ENTITY,
            target_id = scenario.constants.HARASS_GOLDMINE,
            entity_ids = scenario.constants.H1_MINERS,
        })
        scenario.queue_match_input({
            player_id = ENEMY_PLAYER_ID,
            type = scenario.match_input_type.DEFEND,
            entity_ids = scenario.constants.B3_CANNONS
        })
    end)
end

function scenario_update()
    objectives.update()

    -- Objectives complete
    if not is_match_over and objectives.current_objective == OBJECTIVE_DEFEAT_ENEMY and scenario.are_objectives_complete() then
        is_match_over = true
        actions.run(function ()
            objectives.announce_objectives_complete()
            scenario.set_match_over_victory()
        end)
    end

    -- Harassment
    if next_harass_time == nil and should_harass_player() then
        next_harass_time = scenario.get_time() + HARASS_INTERVAL
        scenario.bot_set_config_flag(ENEMY_PLAYER_ID, scenario.bot_config_flag.SHOULD_PRODUCE, true)
    end
    if next_harass_time ~= nil and scenario.get_time() >= next_harass_time then
        spawn_harass_squad()
        next_harass_time = next_harass_time + HARASS_INTERVAL
    end

    -- Snake bunker
    for snake_bunker_index = 1,#snake_bunker_states do
        update_snake_bunker_state(snake_bunker_states[snake_bunker_index])

    end

    actions.update()
end

function should_harass_player()
    local hall_id = scenario.get_hall_surrounding_goldmine(scenario.constants.HARASS_GOLDMINE)
    if hall_id == nil then
        return false
    end

    local hall = entities.get_by_id(hall_id)
    if hall == nil or hall.health == 0 then
        return false
    end

    return hall.player_id == scenario.PLAYER_ID
end

function spawn_harass_squad()
    local spawn_cell
    local hall_ids = {
        scenario.constants.HARASS_HALL1,
        scenario.constants.HARASS_HALL2
    }
    for hall_id_index = 1,#hall_ids do
        local hall_id = hall_ids[hall_id_index]
        local hall = entities.get_by_id(hall_id)
        if hall ~= nil and
                hall.health ~= 0 and
                not entity_util.player_has_entity_near_cell(scenario.PLAYER_ID, { x = hall.cell.x - 1, y = hall.cell.y }, 8) then
            spawn_cell = { x = hall.cell.x - 1, y = hall.cell.y }
            break
        end
    end

    if spawn_cell == nil then
        return
    end

    local goldmine = entities.get_by_id(scenario.constants.HARASS_GOLDMINE)

    squad_util.spawn_harass_squad({
        player_id = ENEMY_PLAYER_ID,
        spawn_cell = spawn_cell,
        target_cell = ivec2.from_cdata(goldmine.cell),
        entity_types = {
            scenario.entity_type.PYRO,
            scenario.entity_type.BANDIT,
            scenario.entity_type.BANDIT,
            scenario.entity_type.BANDIT
        }
    })
end

function update_snake_bunker_state(snake_bunker_state)
    if snake_bunker_state.has_fallen_back then
        return
    end

    local bunker = entities.get_by_id(snake_bunker_state.bunker_id)
    if bunker ~= nil and bunker.health ~= 0 then
        return
    end

    snake_bunker_state.has_fallen_back = true
    if bunker == nil then
        return
    end

    local squad_id = scenario.bot_get_entity_squad_id(snake_bunker_state.bunker_id)
    if squad_id == nil then
        return
    end

    scenario.bot_set_squad_target_cell(ENEMY_PLAYER_ID, squad_id, scenario.constants.SNAKE_BUNKER_FALLBACK_CELL)
end
