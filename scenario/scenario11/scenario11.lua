local actions = require("actions")
local objectives = require("objectives")
local ivec2 = require("ivec2")
local entities = require("entities")

local OBJECTIVE_DEFEAT_PYRO = "Stop the enemy pyro"
local ENEMY_PLAYER_ID = 1
local AVALANCHE_COUNTDOWN_DURATION = 15 * 60

local is_match_over = false
local avalanche_time = nil

function scenario_init()
    scenario.bot_reserve_entity(ENEMY_PLAYER_ID, scenario.constants.TNT_PYRO)
    scenario.queue_match_input({
        type = scenario.match_input_type.DEFEND,
        player_id = ENEMY_PLAYER_ID,
        entity_id = scenario.constants.TNT_PYRO
    })
    scenario.grant_player_upgrade(ENEMY_PLAYER_ID, scenario.upgrade.BAYONETS)
    scenario.grant_player_upgrade(ENEMY_PLAYER_ID, scenario.upgrade.GETAWAY_BOOTS)
    actions.run(intro_cutscene)
end

function scenario_update()
    if avalanche_time ~= nil and scenario.get_time() >= avalanche_time then
        avalanche_time = nil
        actions.run(avalanche_cutscene)
    end

    local player_hall = entities.get_by_id(scenario.constants.PLAYER_HALL)
    local is_player_hall_alive = player_hall ~= nil and player_hall.health ~= 0
    if not is_match_over and not is_player_hall_alive then
        is_match_over = true
        actions.run(function ()
            actions.wait(2)
            objectives.announce_objectives_failed()
            scenario.set_match_over_defeat()
        end)
    end

    local pyro = entities.get_by_id(scenario.constants.TNT_PYRO)
    local is_pyro_dead = pyro == nil or pyro.health == 0
    if not is_match_over and is_pyro_dead then
        is_match_over = true
        actions.run(function ()
            scenario.complete_objective(0)
            scenario.complete_objective(1)
            objectives.announce_objectives_complete()
            scenario.set_match_over_victory()
        end)
    end

    actions.update()
end

function intro_cutscene()
    scenario.fog_reveal({
        cell = scenario.constants.AVALANCHE_CUTSCENE_CAM1,
        cell_size = 1,
        sight = 17,
        duration = 14
    })
    scenario.hold_camera()
    actions.wait(2)

    scenario.highlight_entity(scenario.constants.RIGGED_GOLDMINE)
    scenario.chat("Those prospectors are fixin' to blow the mine wide open.")
    actions.wait(4)
    scenario.chat("If they succeed, the shock will cause an avalanche that will destroy your town!")
    actions.wait(4)

    scenario.highlight_entity(scenario.constants.TNT_PYRO)
    objectives.announce_new_objective(OBJECTIVE_DEFEAT_PYRO)
    scenario.begin_camera_pan(scenario.constants.AVALANCHE_CUTSCENE_CAM2, 3)

    objectives.add_objective({
        objective = {
            description = "Kill the enemy pyro"
        },
        complete_fn = function()
            return false
        end
    })
    objectives.add_objective({
        objective = {
            description = "Your town hall must survive"
        },
        complete_fn = function()
            return false
        end
    })

    avalanche_time = scenario.get_time() + AVALANCHE_COUNTDOWN_DURATION
    scenario.set_global_objective_counter({
        type = scenario.global_objective_counter_type.COUNTDOWN,
        header_text = "Time Remaining",
        end_time_seconds = avalanche_time
    })
end

function avalanche_cutscene()
    scenario.fog_reveal({
        cell = scenario.constants.AVALANCHE_CUTSCENE_CAM1,
        cell_size = 1,
        sight = 17,
        duration = 7
    })
    actions.camera_pan(scenario.constants.AVALANCHE_CUTSCENE_CAM1, 2)
    scenario.hold_camera()

    actions.wait(0.5)

    scenario.queue_match_input({
        player_id = ENEMY_PLAYER_ID,
        type = scenario.match_input_type.MOVE_ENTITY,
        target_id = scenario.constants.TNT_SWITCH,
        entity_id = scenario.constants.TNT_PYRO
    })

    local tnt_switch = nil
    repeat
        coroutine.yield()
        tnt_switch = entities.get_by_id(scenario.constants.TNT_SWITCH)
        local pyro = entities.get_by_id(scenario.constants.TNT_PYRO)
        local pyro_is_dead = pyro == nil or pyro.health == 0
        if tnt_switch.mode ~= scenario.entity_mode.SWITCH_DOWN and pyro_is_dead then
            return
        end
    until tnt_switch.mode == scenario.entity_mode.SWITCH_DOWN

    actions.wait(0.25)

    scenario.explode_rigged_goldmine(scenario.constants.RIGGED_GOLDMINE)
    actions.wait(0.25)
    scenario.play_sound(scenario.sound.AVALANCHE)

    local blocks = {
        {
            length = 2,
            y_offset = 0
        },
        {
            length = 4,
            y_offset = -1,
        },
        {
            length = 1,
            y_offset = -2,
        },
        {
            length = 9,
            y_offset = -3,
        },
        {
            length = 1,
            y_offset = -2,
        }
    }

    local avalanche_x = 129 * 16
    local avalanche_base_y = 98 * 16
    for block_index = 1,#blocks do
        local block = blocks[block_index]
        local avalanche_y = avalanche_base_y + (block.y_offset * 16)
        for column_index = 1,block.length do
            scenario.create_avalanche_column(ivec2.new(avalanche_x, avalanche_y))
            avalanche_x = avalanche_x + 32
        end
    end

    scenario.begin_camera_pan_and_shake(scenario.constants.AVALANCHE_CUTSCENE_CAM2, 2)
    repeat
        coroutine.yield()
    until scenario.get_camera_mode() == scenario.CAMERA_MODE_FREE
    scenario.begin_camera_shake(1)
end
