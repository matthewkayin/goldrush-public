local objectives = require("objectives")
local actions = require("actions")
local ivec2 = require("ivec2")
local entity_util = require("entity_util")
local squad_util = require("squad_util")
local entities = require("entities")

local OBJECTIVE_DEFEAT_ENEMY = "Destroy the enemy base"

local ENEMY_PLAYER_ID = 1

local WAGON_ROUTES = {
    -- spawn 1 above
    { scenario.constants.WAGON_SPAWN1, scenario.constants.WAYPOINT1, scenario.constants.WAYPOINT2, scenario.constants.WAYPOINT3, scenario.constants.DROPOFF1 },
    { scenario.constants.WAGON_SPAWN1, scenario.constants.WAYPOINT1, scenario.constants.WAYPOINT2, scenario.constants.WAYPOINT3, scenario.constants.DROPOFF2 },
    -- spawn 1 below
    { scenario.constants.WAGON_SPAWN1, scenario.constants.WAYPOINT3, scenario.constants.DROPOFF1 },
    { scenario.constants.WAGON_SPAWN1, scenario.constants.WAYPOINT3, scenario.constants.DROPOFF2 },
    -- spawn 2 above
    { scenario.constants.WAGON_SPAWN2, scenario.constants.WAYPOINT4, scenario.constants.WAYPOINT3, scenario.constants.DROPOFF1 },
    { scenario.constants.WAGON_SPAWN2, scenario.constants.WAYPOINT4, scenario.constants.WAYPOINT3, scenario.constants.DROPOFF2 },
    -- spawn 2 below
    { scenario.constants.WAGON_SPAWN2, scenario.constants.WAYPOINT5, scenario.constants.WAYPOINT3, scenario.constants.DROPOFF1 },
    { scenario.constants.WAGON_SPAWN2, scenario.constants.WAYPOINT5, scenario.constants.WAYPOINT3, scenario.constants.DROPOFF2 },
    -- spawn 3
    { scenario.constants.WAGON_SPAWN3, scenario.constants.WAYPOINT7, scenario.constants.DROPOFF1 },
    { scenario.constants.WAGON_SPAWN3, scenario.constants.WAYPOINT7, scenario.constants.DROPOFF2 },
    -- spawn 4
    { scenario.constants.WAGON_SPAWN4, scenario.constants.WAYPOINT7, scenario.constants.DROPOFF1 },
    { scenario.constants.WAGON_SPAWN4, scenario.constants.WAYPOINT7, scenario.constants.DROPOFF2 },
    -- spawn 5
    { scenario.constants.WAGON_SPAWN5, scenario.constants.WAYPOINT6, scenario.constants.WAYPOINT6, scenario.constants.DROPOFF1 },
    { scenario.constants.WAGON_SPAWN5, scenario.constants.WAYPOINT6, scenario.constants.WAYPOINT6, scenario.constants.DROPOFF2 },
}

local WAGON_POINT_DISTANCE = 4

local WAGON_STATE_GARRISON = 1
local WAGON_STATE_MOVE = 2
local WAGON_STATE_UNLOAD = 3
local WAGON_STATE_EXIT = 4
local WAGON_STATE_FINISHED = 5

local WAGON_STATE_FN = {}
local WAGON_SPAWN_INTERVAL = 60

local HARASS_TRIGGERS = {
    scenario.constants.HARASS_TRIGGER1,
    scenario.constants.HARASS_TRIGGER2,
    scenario.constants.HARASS_TRIGGER3,
    scenario.constants.HARASS_TRIGGER4,
    scenario.constants.WAYPOINT3,
    scenario.constants.WAYPOINT7
}
local HARASS_COOLDOWN_DURATION = 120
local next_harass_time = nil
local harass_squad_id = nil

local next_wagon_spawn_time = nil
local wagon_states = {}
local defense_squad_id = nil

local wagons_killed = 0

local intro_cutscene_over = false
local is_match_over = false

function scenario_init()
    WAGON_STATE_FN = {
        wagon_state_garrison,
        wagon_state_move,
        wagon_state_unload,
        wagon_state_exit
    }

    actions.run(function ()
        local COWBOY_IDS = {
            scenario.constants.INTRO_COWBOY1,
            scenario.constants.INTRO_COWBOY2,
            scenario.constants.INTRO_COWBOY3,
            scenario.constants.INTRO_COWBOY4
        }

        -- Reserve entities for intro scene
        scenario.bot_reserve_entity(ENEMY_PLAYER_ID, scenario.constants.INTRO_WAGON)
        for index = 1,#COWBOY_IDS do
            scenario.bot_reserve_entity(ENEMY_PLAYER_ID, COWBOY_IDS[index])
        end

        scenario.hold_camera()

        -- Garrison into wagon
        scenario.queue_match_input({
            player_id = ENEMY_PLAYER_ID,
            type = scenario.match_input_type.MOVE_ENTITY,
            target_id = scenario.constants.INTRO_WAGON,
            entity_ids = COWBOY_IDS
        })

        -- Wait until units are in wagon
        local wagon = nil
        repeat
            coroutine.yield()
            wagon = entities.get_by_id(scenario.constants.INTRO_WAGON)
        until wagon.garrisoned_units_size == 4

        -- Pause to let the players get acclimated
        actions.wait(0.25)

        -- Fog reveal for the scene
        scenario.fog_reveal({
            cell = scenario.constants.INTRO_FOG_REVEAL1,
            cell_size = 2,
            sight = 11,
            duration = 12
        })
        scenario.fog_reveal({
            cell = scenario.constants.DROPOFF1,
            cell_size = 2,
            sight = 13,
            duration = 12
        })

        -- Order wagon to move to dropoff cell
        scenario.queue_match_input({
            player_id = ENEMY_PLAYER_ID,
            type = scenario.match_input_type.MOVE_CELL,
            target_cell = scenario.constants.DROPOFF1,
            entity_id = scenario.constants.INTRO_WAGON
        })

        -- Camera pan
        scenario.begin_camera_pan(scenario.constants.DROPOFF1, 5)

        actions.wait(1)
        scenario.chat("The enemy is sending in reinforcements!")

        actions.wait(4)
        scenario.hold_camera()

        -- Wait until wagon has reached dropoff cell
        repeat
            coroutine.yield()
            wagon = entities.get_by_id(scenario.constants.INTRO_WAGON)
        until wagon.mode == scenario.entity_mode.UNIT_IDLE and wagon.target.type == scenario.target_type.NONE

        scenario.chat("You've got to stop those wagons, or you'll never take them down.")

        -- Unload cowboys one by one
        for cowboy_index = 1,#COWBOY_IDS do
            -- Queue unload input
            scenario.queue_match_input({
                player_id = ENEMY_PLAYER_ID,
                type = scenario.match_input_type.SINGLE_UNLOAD,
                entity_id = COWBOY_IDS[cowboy_index]
            })

            -- Wait until cowboy has unloaded
            local cowboy = nil
            repeat
                coroutine.yield()
                cowboy = entities.get_by_id(COWBOY_IDS[cowboy_index])
            until cowboy.garrison_id == scenario.ID_NULL

            -- Queue cowboy move input
            scenario.queue_match_input({
                player_id = ENEMY_PLAYER_ID,
                type = scenario.match_input_type.MOVE_CELL,
                target_cell = scenario.constants.INTRO_COWBOY_CELL,
                entity_id = COWBOY_IDS[cowboy_index]
            })

            -- Pause for effect
            actions.wait(1)
        end

        -- Pan to player base
        actions.camera_pan(scenario.constants.INTRO_PLAYER_BASE_CELL, 2)
        actions.wait(1)

        -- Announce objectives
        objectives.announce_new_objective(OBJECTIVE_DEFEAT_ENEMY)
        objectives.add_objective({
            objective = {
                description = "Destroy the enemy base"
            },
            complete_fn = function ()
                return scenario.is_player_defeated(ENEMY_PLAYER_ID)
            end
        })
        set_wagon_spawn_time()

        -- Remove the wagon
        scenario.remove_entity(scenario.constants.INTRO_WAGON)

        -- Add the cowboys to the defense squad
        add_cowboys_to_defense_squad(COWBOY_IDS)

        scenario.bot_set_config_flag(ENEMY_PLAYER_ID, scenario.bot_config_flag.SHOULD_PRODUCE, true)

        -- Give jockey hint
        actions.wait(5)
        scenario.hint("You can now hire Jockeys, which are fast, ranged cavalry.")

        intro_cutscene_over = true
    end)
end

function scenario_update()
    objectives.update()

    -- Check for victory
    if not is_match_over and scenario.are_objectives_complete() then
        is_match_over = true
        actions.run(function ()
            objectives.announce_objectives_complete()
            scenario.set_match_over_victory()
        end)
    end

    -- Spawn wagon
    if next_wagon_spawn_time ~= nil and scenario.get_time() >= next_wagon_spawn_time then
        scenario.set_global_objective_counter({
            type = scenario.global_objective_counter_type.OFF
        })

        local wagon_index = math.random(1, #WAGON_ROUTES)
        spawn_wagon(wagon_index)
        next_wagon_spawn_time = nil
    end

    -- Update wagon states
    local wagon_state_index = 1
    while wagon_state_index <= #wagon_states do
        update_wagon_state(wagon_states[wagon_state_index])
        if wagon_states[wagon_state_index].mode == WAGON_STATE_FINISHED then
            table.remove(wagon_states, wagon_state_index)
        else
            wagon_state_index = wagon_state_index + 1
        end
    end

    -- Reset wagon spawn
    if intro_cutscene_over and
            #wagon_states == 0 and
            next_wagon_spawn_time == nil and
            scenario.get_player_population(ENEMY_PLAYER_ID) <= 90 then
        set_wagon_spawn_time()
    end

    -- Harassment
    local harassable_building_id = get_harassable_player_building()
    if harassable_building_id ~= nil and next_harass_time == nil and harass_squad_id == nil then
        next_harass_time = scenario.get_time() + HARASS_COOLDOWN_DURATION
    end
    if not scenario.is_player_defeated(ENEMY_PLAYER_ID) and harassable_building_id ~= nil and next_harass_time ~= nil and scenario.get_time() >= next_harass_time then
        harass_squad_id = spawn_harass_squad(harassable_building_id)
        next_harass_time = nil
    end

    actions.update()
end

function set_wagon_spawn_time()
    next_wagon_spawn_time = scenario.get_time() + WAGON_SPAWN_INTERVAL
    scenario.set_global_objective_counter({
        type = scenario.global_objective_counter_type.COUNTDOWN,
        header_text = "Enemy reinforcements arrive in",
        end_time_seconds = next_wagon_spawn_time
    })
end

function spawn_wagon(route_index)
    scenario.log("spawn wagon route_index ", route_index, " route ", WAGON_ROUTES[route_index])

    local spawn_cells = scenario.find_entity_spawn_cells(WAGON_ROUTES[route_index][1], {
        scenario.entity_type.WAGON,
        scenario.entity_type.COWBOY,
        scenario.entity_type.BANDIT,
        scenario.entity_type.COWBOY,
        scenario.entity_type.BANDIT,
        scenario.entity_type.JOCKEY,
        scenario.entity_type.JOCKEY
    })
    for spawn_cell_index = 1,#spawn_cells do
        if spawn_cells[spawn_cell_index] == nil then
            scenario.log("Script spawn_wagon - One of the spawn cells is nil. Abandoning wagon spawn.")
            return
        end
    end

    -- Spawn wagon
    local wagon_id = scenario.create_entity(scenario.entity_type.WAGON, spawn_cells[1], ENEMY_PLAYER_ID)
    scenario.bot_reserve_entity(ENEMY_PLAYER_ID, wagon_id)

    -- Spawn cowboys
    local cowboy_ids = {}
    for index = 1,4 do
        local entity_type
        if index < 3 then
            entity_type = scenario.entity_type.COWBOY
        else
            entity_type = scenario.entity_type.BANDIT
        end
        local cowboy_id = scenario.create_entity(entity_type, spawn_cells[index + 1], ENEMY_PLAYER_ID)
        scenario.bot_reserve_entity(ENEMY_PLAYER_ID, cowboy_id)
        table.insert(cowboy_ids, cowboy_id)
    end

    -- Spawn jockeys
    local jockey_ids = {}
    if wagons_killed >= 2 then
        for index = 1,2 do
            local jockey_id = scenario.create_entity(scenario.entity_type.JOCKEY, spawn_cells[5 + index], ENEMY_PLAYER_ID)
            scenario.bot_reserve_entity(ENEMY_PLAYER_ID, jockey_id)
            table.insert(jockey_ids, jockey_id)
        end
    end

    -- Save wagon state
    local wagon_state = {
        route_index = route_index,
        route_points_index = 2,
        mode = WAGON_STATE_GARRISON,
        wagon_id = wagon_id,
        cowboy_ids = cowboy_ids,
        jockey_ids = jockey_ids,
        wagon = {},
        resume_at_time = scenario.get_time()
    }
    wagon_state.wagon = entities.get_by_id(wagon_state.wagon_id)

    -- Announce arrival
    scenario.chat("Enemy reinforcements are inbound!")
    scenario.play_sound(scenario.sound.ALERT_BELL)
    scenario.highlight_entity(wagon_id)
    scenario.fog_reveal({
        cell = ivec2.from_cdata(wagon_state.wagon.cell),
        cell_size = 2,
        sight = 9,
        duration = 1
    })
    scenario.create_alert(scenario.ALERT_COLOR_WHITE, ivec2.from_cdata(wagon_state.wagon.cell), 2)

    wagon_state_garrison(wagon_state)

    table.insert(wagon_states, wagon_state)
end

function update_wagon_state(state)
    if scenario.is_player_defeated(ENEMY_PLAYER_ID) then
        return
    end

    -- Check for dead cowboys
    -- If all the cowboys die somehow before the wagon, we can still have
    -- the wagon do its route and then despawn
    local cowboy_index = 1
    while cowboy_index <= #state.cowboy_ids do
        local cowboy = entities.get_by_id(state.cowboy_ids[cowboy_index])
        if cowboy == nil or cowboy.health == 0 then
            -- Release cowboy and remove from list
            scenario.bot_release_entity(ENEMY_PLAYER_ID, state.cowboy_ids[cowboy_index])
            table.remove(state.cowboy_ids, cowboy_index)
        else
            cowboy_index = cowboy_index + 1
        end
    end

    -- Check for dead jockeys
    local jockey_index = 1
    while jockey_index <= #state.jockey_ids do
        local jockey = entities.get_by_id(state.jockey_ids[jockey_index])
        if jockey == nil or jockey.health == 0 then
            -- Release jockey and remove from list
            scenario.bot_release_entity(ENEMY_PLAYER_ID, state.jockey_ids[jockey_index])
            table.remove(state.jockey_ids, jockey_index)
        else
            jockey_index = jockey_index + 1
        end
    end

    -- Check for dead wagon
    state.wagon = entities.get_by_id(state.wagon_id)
    if state.wagon == nil or state.wagon.health == 0 then
        -- Release wagon
        scenario.bot_release_entity(ENEMY_PLAYER_ID, state.wagon_id)

        if #state.cowboy_ids ~= 0 then
            add_cowboys_to_defense_squad(state.cowboy_ids)
        end

        if #state.jockey_ids ~= 0 then
            add_cowboys_to_defense_squad(state.jockey_ids)
        end

        -- Update wagons killed
        if wagons_killed < 2 then
            wagons_killed = wagons_killed + 1
        end

        -- Set mode to finished
        state.mode = WAGON_STATE_FINISHED
        scenario.log("wagon is dead, wagon state finished")
        return
    end

    if state.resume_at_time > scenario.get_time() then
        return
    end

    -- Tell jockeys to follow wagon
    for index = 1,#state.jockey_ids do
        local jockey = entities.get_by_id(state.jockey_ids[index])
        if jockey.mode == scenario.entity_mode.UNIT_IDLE and
                jockey.target.type == scenario.target_type.NONE and
                ivec2.manhattan_distance(ivec2.from_cdata(jockey.cell), ivec2.from_cdata(state.wagon.cell)) > 4 then
            scenario.queue_match_input({
                player_id = ENEMY_PLAYER_ID,
                type = scenario.match_input_type.MOVE_ENTITY,
                target_id = state.wagon_id,
                entity_id = state.jockey_ids[index]
            })
        end
    end

    local is_finished = false
    while not is_finished do
        is_finished = WAGON_STATE_FN[state.mode](state)
    end
end

function wagon_state_set_delay_for_match_input(state)
    state.resume_at_time = scenario.get_time() + 0.5
end

function wagon_state_garrison(state)
    -- Wait for cowboys to garrison
    scenario.log("wagon mode is garrison. #garrisoned_units ", state.wagon.garrisoned_units_size, " #cowboys ", #state.cowboy_ids)
    if state.wagon.garrisoned_units_size == #state.cowboy_ids then
        state.mode = WAGON_STATE_MOVE
        scenario.log("wagon is full, set to move")
        return false
    end

    -- Queue garrison into wagon
    scenario.log("wagon state queue garrison into wagon")
    scenario.queue_match_input({
        player_id = ENEMY_PLAYER_ID,
        type = scenario.match_input_type.MOVE_ENTITY,
        target_id = state.wagon_id,
        entity_ids = state.cowboy_ids
    })
    wagon_state_set_delay_for_match_input(state)

    return true
end

function wagon_state_move(state)
    -- If we're moving, keep moving
    if state.wagon.mode ~= scenario.entity_mode.UNIT_IDLE and
            state.wagon.target.type ~= scenario.target_type.NONE then
        return true
    end

    -- If we're not moving and we're close to the next point,
    -- then increment the route
    local route_points = WAGON_ROUTES[state.route_index]
    if ivec2.manhattan_distance(ivec2.from_cdata(state.wagon.cell), route_points[state.route_points_index]) < WAGON_POINT_DISTANCE then
        scenario.log("wagon is move and is close to next point. wagon points index ", state.route_points_index, " and #route_points ", #route_points)
        if state.route_points_index == #route_points then
            scenario.log("wagon state set to unload")
            state.mode = WAGON_STATE_UNLOAD
        else
            state.route_points_index = state.route_points_index + 1
            scenario.log("wagon state increments route points index ", state.route_points_index)
        end

        return false
    end

    -- If we're not moving, and we're not close to the next point,
    -- then move to the next point
    scenario.queue_match_input({
        player_id = ENEMY_PLAYER_ID,
        type = scenario.match_input_type.MOVE_CELL,
        target_cell = route_points[state.route_points_index],
        entity_id = state.wagon_id
    })
    wagon_state_set_delay_for_match_input(state)
    scenario.log("wagon queue move input")

    return true
end

function wagon_state_unload(state)
    -- If the wagon is empty, then we're finished
    if state.wagon.garrisoned_units_size == 0 then
        if #state.cowboy_ids ~= 0 then
            add_cowboys_to_defense_squad(state.cowboy_ids)
        end
        scenario.chat("Enemy reinforcements have arrived at their base!")
        state.mode = WAGON_STATE_EXIT
        return false
    end

    -- Unload the wagon
    if state.wagon.target.type == scenario.target_type.NONE then
        scenario.queue_match_input({
            player_id = ENEMY_PLAYER_ID,
            type = scenario.match_input_type.MOVE_UNLOAD,
            target_cell = ivec2.from_cdata(state.wagon.cell),
            entity_id = state.wagon_id
        })
    end
    wagon_state_set_delay_for_match_input(state)
    scenario.log("wagon queue unload input")

    return true
end

function wagon_state_exit(state)
    -- If we're moving, keep moving
    if state.wagon.mode ~= scenario.entity_mode.UNIT_IDLE and
            state.wagon.target.type ~= scenario.target_type.NONE then
        return true
    end

    -- Determine which despawn cell we should be using
    local despawn_cell
    local route_points = WAGON_ROUTES[state.route_index]
    if route_points[#route_points] == scenario.constants.DROPOFF1 then
        despawn_cell = scenario.constants.DESPAWN1
    else
        despawn_cell = scenario.constants.DESPAWN2
    end

    -- If we're not moving, and we're far away from the despawn cell,
    -- then move to the despawn cell
    if ivec2.manhattan_distance(ivec2.from_cdata(state.wagon.cell), despawn_cell) > WAGON_POINT_DISTANCE then
        scenario.queue_match_input({
            player_id = ENEMY_PLAYER_ID,
            type = scenario.match_input_type.MOVE_CELL,
            target_cell = despawn_cell,
            entity_id = state.wagon_id
        })
        wagon_state_set_delay_for_match_input(state)
        return true
    end

    -- If we are close to the despawn cell,
    -- then despawn the wagon and finish the wagon state
    scenario.bot_release_entity(ENEMY_PLAYER_ID, state.wagon_id)
    scenario.remove_entity(state.wagon_id)

    for jockey_index = 1,#state.jockey_ids do
        scenario.bot_release_entity(ENEMY_PLAYER_ID, state.jockey_ids[jockey_index])
        scenario.remove_entity(state.jockey_ids[jockey_index])
    end

    state.mode = WAGON_STATE_FINISHED
    return true
end

function add_cowboys_to_defense_squad(cowboy_ids)
    if defense_squad_id == nil or not scenario.bot_squad_exists(ENEMY_PLAYER_ID, defense_squad_id) then
        defense_squad_id = scenario.bot_add_squad({
            player_id = ENEMY_PLAYER_ID,
            type = scenario.bot_squad_type.DEFEND,
            target_cell = scenario.constants.INTRO_COWBOY_CELL,
            entity_list = cowboy_ids
        })
        return
    end

    scenario.bot_add_entities_to_squad(ENEMY_PLAYER_ID, defense_squad_id, cowboy_ids)
end

function get_harassable_player_building()
    local filter_fn = function (entity)
        -- Fitler down to player-owned units
        if entity.player_id ~= scenario.PLAYER_ID then
            return false
        end
        -- Filter down to buildings only
        if entity.type < scenario.entity_type.HALL or entity.type >= scenario.entity_type.LANDMINE then
            return false
        end
        -- Fitler out dead entities
        if entity.health == 0 then
            return false
        end

        -- Choose this entity only if it's close to a trigger cell
        local is_near_harass_triggers = false
        for trigger_index = 1,#HARASS_TRIGGERS do
            if ivec2.manhattan_distance(ivec2.from_cdata(entity.cell), HARASS_TRIGGERS[trigger_index]) <= 16 then
                is_near_harass_triggers = true
                break
            end
        end
        if not is_near_harass_triggers then
            return false
        end
    end
    local score_entity = function (entity)
        local score = 1

        -- Plus score for being near the triggers outside the opponent's base
        if is_entity_near_harass_triggers_outside_of_opponents_base(entity) then
            score = score + 2
        end

        -- Plus score for being a bunker
        if entity.type == scenario.entity_type.BUNKER then
            score = score + 1
        end

        return score
    end
    local compare_fn = function (entity, best_entity)
        return score_entity(entity) > score_entity(best_entity)
    end

    return entity_util.find_best_entity(filter_fn, compare_fn)
end

function is_entity_near_harass_triggers_outside_of_opponents_base(entity)
    return ivec2.manhattan_distance(ivec2.from_cdata(entity.cell), scenario.constants.HARASS_TRIGGER1) <= 16 or
        ivec2.manhattan_distance(ivec2.from_cdata(entity.cell), scenario.constants.HARASS_TRIGGER2) <= 16
end

function spawn_harass_squad(harassable_building_id)
    local building = entities.get_by_id(harassable_building_id)

    local entity_types = {
        scenario.entity_type.COWBOY,
        scenario.entity_type.COWBOY,
        scenario.entity_type.COWBOY,
        scenario.entity_type.COWBOY
    }

    -- Check if we should use sappers in harass squad
    if building.type == scenario.entity_type.BUNKER and
            is_entity_near_harass_triggers_outside_of_opponents_base(building) then
        table.insert(entity_types, scenario.entity_type.SAPPER)
    end

    -- Chose where to spawn
    local spawn_cell = nil
    if ivec2.manhattan_distance(ivec2.from_cdata(building.cell), scenario.constants.HARASS_SPAWN1) <
            ivec2.manhattan_distance(ivec2.from_cdata(building.cell), scenario.constants.HARASS_SPAWN2) then
        spawn_cell = scenario.constants.HARASS_SPAWN1
    else
        spawn_cell = scenario.constants.HARASS_SPAWN2
    end

    return squad_util.spawn_harass_squad({
        player_id = ENEMY_PLAYER_ID,
        spawn_cell = spawn_cell,
        target_cell = ivec2.from_cdata(building.cell),
        entity_types = entity_types
    })
end
