local actions = require("actions")
local objectives = require("objectives")
local entities = require("entities")
local ivec2 = require("ivec2")
local squad_util = require("squad_util")

local ENEMY1 = 1
local ENEMY2 = 2

local TARGET_GOLD_COUNT = 10000
local OBJECTIVE_MINE_GOLD = "Mine 10,000 gold before your opponents"

local is_match_over = false

local CENTER_HALL_MODE_BUILD_BUNKER = 1
local CENTER_HALL_MODE_BUILDING_BUNKER = 2
local CENTER_HALL_MODE_BUILD_HALL = 3
local CENTER_HALL_MODE_FINISHED = 4

local CENTER_HALL_MICRO_COOLDOWN_DURATION = 1.0

local enemy_center_hall_states = {
    {
        player_id = ENEMY1,
        mode = CENTER_HALL_MODE_BUILD_BUNKER,
        builder_id = scenario.constants.ENEMY1_CENTER_HALL_BUILDER,
        cowboy_ids = scenario.constants.ENEMY1_COWBOYS,
        bunker_cell = scenario.constants.ENEMY1_CENTER_BUNKER_CELL,
        hall_cell = scenario.constants.ENEMY1_CENTER_HALL_CELL
    },
    {
        player_id = ENEMY2,
        mode = CENTER_HALL_MODE_BUILD_BUNKER,
        builder_id = scenario.constants.ENEMY2_CENTER_HALL_BUILDER,
        cowboy_ids = scenario.constants.ENEMY2_COWBOYS,
        bunker_cell = scenario.constants.ENEMY2_CENTER_BUNKER_CELL,
        hall_cell = scenario.constants.ENEMY2_CENTER_HALL_CELL
    }
}

local enemy_harass_states = {
    {
        player_id = ENEMY1,
        unit_spawn_rates = {
            { type = scenario.entity_type.COWBOY, rate = 0.4 },
            { type = scenario.entity_type.BANDIT, rate = 0.3 },
            { type = scenario.entity_type.PYRO, rate = 0.15 },
            { type = scenario.entity_type.SAPPER, rate = 0.15 },
        },
        unit_count_per_level = 2,
        next_harass_time = nil,
        initial_harass_interval = 90,
        harass_interval = 120
    },
    {
        player_id = ENEMY2,
        unit_spawn_rates = {
            { type = scenario.entity_type.SOLDIER, rate = 0.8 },
            { type = scenario.entity_type.CANNON, rate = 0.2 },
        },
        unit_count_per_level = 3,
        next_harass_time = nil,
        initial_harass_interval = 120,
        harass_interval = 150
    }
}
local goldmine_ids = {}

function scenario_init()
    -- Hold camera and move player miners
    scenario.hold_camera()
    scenario.queue_match_input({
        player_id = scenario.PLAYER_ID,
        type = scenario.match_input_type.MOVE_ENTITY,
        target_id = scenario.constants.GOLDMINE1,
        entity_ids = scenario.constants.PLAYER_MINERS
    })

    -- Init goldmine list and enemy starting halls
    for entity_index = 0,(entities.get_count() - 1) do
        local entity = entities.get_by_index(entity_index)
        if entity.type == scenario.entity_type.GOLDMINE then
            table.insert(goldmine_ids, entities.get_id_of(entity_index))
        end

        for index = 1,#enemy_harass_states do
            if entity.type == scenario.entity_type.HALL and
                    entity.player_id == enemy_harass_states[index].player_id then
                enemy_harass_states[index].starting_hall_id = entities.get_id_of(entity_index)
            end
        end
    end

    -- Init center hall states
    for index = 1,#enemy_center_hall_states do
        enemy_center_hall_state_init(enemy_center_hall_states[index])
    end

    -- Intro cutscene
    actions.run(intro_cutscene)
end

function scenario_update()
    objectives.update()

    -- Check for victory
    if not is_match_over and scenario.get_player_gold_mined_total(scenario.PLAYER_ID) >= TARGET_GOLD_COUNT then
        is_match_over = true
        actions.run(function ()
            scenario.complete_objective(0)
            objectives.announce_objectives_complete()
            scenario.set_match_over_victory()
        end)
    end

    -- Check for defeat
    local an_enemy_has_mined_target_gold =
        scenario.get_player_gold_mined_total(ENEMY1) >= TARGET_GOLD_COUNT or
        scenario.get_player_gold_mined_total(ENEMY2) >= TARGET_GOLD_COUNT
    if not is_match_over and an_enemy_has_mined_target_gold then
        is_match_over = true
        actions.run(function ()
            objectives.announce_objectives_failed()
            scenario.set_match_over_defeat()
        end)
    end

    -- Update enemy hall state
    for index = 1,#enemy_center_hall_states do
        enemy_center_hall_state_update(enemy_center_hall_states[index])
    end

    -- Update harassment
    for index = 1,#enemy_harass_states do
        enemy_harass_update(enemy_harass_states[index])
    end

    actions.update()
end

function intro_cutscene()
    actions.wait(1)
    local camera_start_cell = scenario.get_camera_centered_cell()

    -- Intro
    scenario.highlight_entity(scenario.constants.GOLDMINE1)
    scenario.chat("This gold mine won't last much longer.")
    actions.wait(3)

    -- Center goldmine pans
    for center_goldmine_index = 1,#scenario.constants.CENTER_GOLDMINES do
        local goldmine_id = scenario.constants.CENTER_GOLDMINES[center_goldmine_index]
        local goldmine = entities.get_by_id(goldmine_id)
        local goldmine_cell = ivec2.from_cdata(goldmine.cell)

        scenario.fog_reveal({
            cell = goldmine_cell,
            cell_size = 3,
            sight = 13,
            duration = 7
        })
        actions.camera_pan(ivec2.add(goldmine_cell, ivec2.new(1, 1)), 2)
        scenario.hold_camera()
        scenario.highlight_entity(goldmine_id)
        if center_goldmine_index == 1 then
            scenario.chat("The mines in the center have plenty of gold.")
        elseif center_goldmine_index == 3 then
            scenario.chat("If you can hold the center, you can secure your fortune.")
        end
        actions.wait(1)
    end

    -- Player reveals
    scenario.fog_reveal({
        cell = scenario.constants.INTRO_CAM1,
        cell_size = 1,
        sight = 17,
        duration = 7
    })
    actions.camera_pan(scenario.constants.INTRO_CAM1, 2)
    scenario.hold_camera()
    scenario.chat("Just make sure to bring some guns.")
    actions.wait(2)

    scenario.fog_reveal({
        cell = scenario.constants.INTRO_CAM2,
        cell_size = 1,
        sight = 17,
        duration = 7
    })
    actions.camera_pan(scenario.constants.INTRO_CAM2, 2)
    scenario.hold_camera()
    scenario.chat("You ain't the only one tryin to strike it rich out here.")
    actions.wait(2)

    actions.camera_pan(camera_start_cell, 2)
    actions.wait(1)

    objectives.announce_new_objective(OBJECTIVE_MINE_GOLD)
    objectives.add_objective({
        objective = {
            description = OBJECTIVE_MINE_GOLD
        },
        complete_fn = function ()
            return false
        end
    })
    scenario.set_global_objective_counter({
        type = scenario.global_objective_counter_type.GOLD,
        max_value = TARGET_GOLD_COUNT
    })
end

function enemy_center_hall_state_init(state)
    scenario.bot_reserve_entity(state.player_id, state.builder_id)
    for index = 1,#state.cowboy_ids do
        scenario.bot_reserve_entity(state.player_id, state.cowboy_ids[index])
    end

    state.previous_allowed_entities = scenario.bot_get_allowed_entities(state.player_id)
    scenario.bot_set_allowed_entities(state.player_id, { scenario.entity_type.MINER })

    scenario.queue_match_input({
        player_id = state.player_id,
        type = scenario.match_input_type.BUILD,
        building_type = scenario.entity_type.BUNKER,
        building_cell = state.bunker_cell,
        entity_id = state.builder_id
    })
    state.micro_cooldown = scenario.get_time() + CENTER_HALL_MICRO_COOLDOWN_DURATION

    actions.run(function ()
        actions.wait(5)
        scenario.queue_match_input({
            player_id = state.player_id,
            type = scenario.match_input_type.MOVE_CELL,
            target_cell = state.hall_cell,
            entity_ids = state.cowboy_ids
        })
    end)
end

function enemy_center_hall_state_update(state)
    if state.mode == CENTER_HALL_MODE_FINISHED then
        return
    end

    if state.micro_cooldown ~= nil and scenario.get_time() < state.micro_cooldown then
        return
    end

    if scenario.is_player_defeated(state.player_id) then
        enemy_center_hall_state_finish(state)
        return
    end

    local builder = entities.get_by_id(state.builder_id)
    local is_builder_dead = builder == nil or builder.health == 0
    if is_builder_dead then
        enemy_center_hall_state_finish(state)
        return
    end

    if state.mode == CENTER_HALL_MODE_BUILD_BUNKER then
        -- If we are not building the bunker, try again
        if builder.target.type == scenario.target_type.NONE then
            scenario.queue_match_input({
                player_id = state.player_id,
                type = scenario.match_input_type.BUILD,
                building_type = scenario.entity_type.BUNKER,
                building_cell = state.bunker_cell,
                entity_id = state.builder_id
            })
            state.micro_cooldown = scenario.get_time() + CENTER_HALL_MICRO_COOLDOWN_DURATION
        end

        -- If we are building the bunker, then switch to bunker mode
        if builder.mode == scenario.entity_mode.UNIT_BUILD then
            state.mode = CENTER_HALL_MODE_BUILDING_BUNKER
            state.bunker_id = builder.target.id
            return
        end
    end

    if state.mode == CENTER_HALL_MODE_BUILDING_BUNKER then
        -- Check for bunker death
        local bunker = entities.get_by_id(state.bunker_id)
        local bunker_is_dead = bunker == nil or bunker.health == 0
        if bunker_is_dead then
            enemy_center_hall_state_finish(state)
            return
        end

        -- If bunker finished, create squad and move into hall mode
        if bunker.mode == scenario.entity_mode.BUILDING_FINISHED then
            local entity_list = state.cowboy_ids
            table.insert(entity_list, state.bunker_id)

            scenario.bot_add_squad({
                player_id = state.player_id,
                type = scenario.bot_squad_type.DEFEND,
                target_cell = ivec2.from_cdata(bunker.cell),
                entity_list = entity_list
            })

            -- Clear the cowboy_ids so that we don't release them on finish
            state.cowboy_ids = {}
            state.mode = CENTER_HALL_MODE_BUILD_HALL
        end
    end

    if state.mode == CENTER_HALL_MODE_BUILD_HALL then
        -- If we are not building the hall, try again
        local enemy_can_afford_hall =
            scenario.get_player_gold(state.player_id) >=
            scenario.get_entity_gold_cost(scenario.entity_type.HALL)
        if builder.target.type == scenario.target_type.NONE and enemy_can_afford_hall then
            scenario.queue_match_input({
                player_id = state.player_id,
                type = scenario.match_input_type.BUILD,
                building_type = scenario.entity_type.HALL,
                building_cell = state.hall_cell,
                entity_id = state.builder_id
            })
            state.micro_cooldown = scenario.get_time() + CENTER_HALL_MICRO_COOLDOWN_DURATION
        end

        -- If we are building a hall, then release the builder and consider this state finished
        if builder.mode == scenario.entity_mode.UNIT_BUILD then
            enemy_center_hall_state_finish(state)
        end
    end
end

function enemy_center_hall_state_finish(state)
    state.mode = CENTER_HALL_MODE_FINISHED
    scenario.bot_release_entity(state.player_id, state.builder_id)
    scenario.bot_set_allowed_entities(state.player_id, state.previous_allowed_entities)
    for index = 1,#state.cowboy_ids do
        scenario.bot_release_entity(state.player_id, state.cowboy_ids[index])
    end
end

function is_goldmine_in_center(goldmine_id)
    for index = 1,#scenario.constants.CENTER_GOLDMINES do
        if scenario.constants.CENTER_GOLDMINES[index] == goldmine_id then
            return true
        end
    end

    return false
end

function enemy_harass_update(state)
    -- Determine how many bases everyone has
    local enemy_halls = {}
    local player_halls = {}
    local player_center_halls = {}
    for index = 1,#goldmine_ids do
        local goldmine_id = goldmine_ids[index]
        local hall_id = scenario.get_hall_surrounding_goldmine(goldmine_id)
        if hall_id == nil then
            goto continue
        end

        local hall = entities.get_by_id(hall_id)
        if hall.player_id == scenario.PLAYER_ID then
            table.insert(player_halls, goldmine_id)
            if is_goldmine_in_center(goldmine_id) then
                table.insert(player_center_halls, goldmine_id)
            end
        end

        if hall.player_id == state.player_id then
            table.insert(enemy_halls, goldmine_id)
        end

        ::continue::
    end

    if state.next_harass_time == nil and #player_halls > 1 then
        state.next_harass_time = scenario.get_time() + state.harass_interval
    end

    if state.next_harass_time == nil or state.next_harass_time > scenario.get_time() then
        return
    end

    -- If they have no bases, don't harass
    if #enemy_halls == 0 or (#player_halls + #player_center_halls == 0) then
        state.next_harass_time = nil
        return
    end

    -- Determine squad spawn position
    -- Tries to spawn from enemy starting hall first and if that fails chooses and random hall
    local spawn_hall_id
    local starting_hall = entities.get_by_id(state.starting_hall_id)
    local starting_hall_is_alive = starting_hall ~= nil and starting_hall.health ~= 0
    if starting_hall_is_alive then
        spawn_hall_id = state.starting_hall_id
    else
        local spawn_hall_index = math.random(1, #enemy_halls)
        spawn_hall_id = enemy_halls[spawn_hall_index]
    end
    local enemy_hall = entities.get_by_id(spawn_hall_id)
    local spawn_cell = ivec2.add(ivec2.from_cdata(enemy_hall.cell), ivec2.new(-1, 0))

    -- Determine attack target cell
    -- Prefers to attack player center bases first, otherwise chooses a random base
    local target_hall_id
    if #player_center_halls ~= 0 then
        local player_hall_index = math.random(1, #player_center_halls)
        target_hall_id = player_center_halls[player_hall_index]
    else
        local player_hall_index = math.random(1, #player_halls)
        target_hall_id = player_halls[player_hall_index]
    end
    local player_hall = entities.get_by_id(target_hall_id)
    local target_cell = ivec2.from_cdata(player_hall.cell)

    -- Determine the harass squad unit count
    -- Enemy level is from 1 to max_level and scales as their gold_mined reaches the target
    -- The equation is gold / target = level / max_level => level = (gold / target) * max_level
    local max_level = 3
    local enemy_gold_mined = math.min(scenario.get_player_gold_mined_total(state.player_id), TARGET_GOLD_COUNT)
    local enemy_gold_mined_percent = enemy_gold_mined / TARGET_GOLD_COUNT
    local enemy_level = math.ceil(enemy_gold_mined_percent * max_level)
    local unit_count = enemy_level * state.unit_count_per_level

    -- Determine unit types to choose from
    local unit_spawn_rates
    if enemy_level == 1 then
        unit_spawn_rates = {
            { type = scenario.entity_type.COWBOY, rate = 0.5 },
            { type = scenario.entity_type.BANDIT, rate = 0.5 },
        }
    else
        unit_spawn_rates = state.unit_spawn_rates
    end

    -- Select units to spawn
    local unit_types = {}
    while #unit_types < unit_count do
        local unit_type = scenario.entity_type.COWBOY
        local unit_roll = math.random()
        local rate_dc = 0.0
        for index = 1,#unit_spawn_rates do
            rate_dc = rate_dc + unit_spawn_rates[index].rate
            if unit_roll < rate_dc then
                unit_type = unit_spawn_rates[index].type
                goto insert_unit
            end
        end

        ::insert_unit::
        table.insert(unit_types, unit_type)
    end

    squad_util.spawn_harass_squad({
        player_id = state.player_id,
        target_cell = target_cell,
        spawn_cell = spawn_cell,
        entity_types = unit_types
    })
    state.next_harass_time = scenario.get_time() + state.harass_interval
end
