local objectives = require("objectives")
local actions = require("actions")
local entities = require("entities")
local ivec2 = require("ivec2")

local OBJECTIVE_KILL_GENERALS = "Assassinate the generals"

local ENEMY_PLAYER_ID = 1
local GENERALS_PLAYER_ID = 2

local generals = {
    scenario.constants.GENERAL1,
    scenario.constants.GENERAL2,
    scenario.constants.GENERAL3
}
local cached_generals_killed = 0
local generals_killed_target = #generals

local is_match_over = false

function scenario_init()
    scenario.grant_player_upgrade(ENEMY_PLAYER_ID, scenario.upgrade.BAYONETS)

    -- Reserve contain units
    for index = 1,#scenario.constants.CONTAIN_UNITS do
        scenario.bot_reserve_entity(ENEMY_PLAYER_ID, scenario.constants.CONTAIN_UNITS[index])
    end

    -- Reserve contain balloons
    for index = 1,#scenario.constants.CONTAIN_BALLOONS do
        scenario.bot_reserve_entity(ENEMY_PLAYER_ID, scenario.constants.CONTAIN_BALLOONS[index])
    end

    -- Contain units hold position
    scenario.queue_match_input({
        player_id = ENEMY_PLAYER_ID,
        type = scenario.match_input_type.DEFEND,
        entity_ids = scenario.constants.CONTAIN_UNITS
    })

    -- Contain balloons
    scenario.queue_match_input({
        player_id = ENEMY_PLAYER_ID,
        type = scenario.match_input_type.DEFEND,
        entity_ids = scenario.constants.CONTAIN_BALLOONS
    })

    -- Player soldiers into bunker
    scenario.queue_match_input({
        player_id = scenario.PLAYER_ID,
        type = scenario.match_input_type.MOVE_ENTITY,
        target_id = scenario.constants.PLAYER_BUNKER,
        entity_ids = scenario.constants.PLAYER_SOLDIERS
    })

    -- Get general info for cutscenes
    local general_cells = {}
    for index = 1,#generals do
        local general = entities.get_by_id(generals[index])
        general_cells[index] = ivec2.from_cdata(general.cell)
    end

    -- Generals hold position
    scenario.queue_match_input({
        player_id = GENERALS_PLAYER_ID,
        type = scenario.match_input_type.DEFEND,
        entity_ids = generals
    })

    actions.run(function ()
        local camera_start_cell = scenario.get_camera_centered_cell()

        actions.wait(0.5)

        scenario.fog_reveal({
            cell = scenario.constants.CONTAIN_REVEAL_CELL,
            cell_size = 2,
            sight = 13,
            duration = 7
        })
        actions.camera_pan(scenario.constants.CONTAIN_REVEAL_CELL, 1.5)
        scenario.hold_camera()

        actions.wait(0.5)
        scenario.chat("The enemy has you contained!")
        actions.wait(2)

        scenario.fog_reveal({
            cell = general_cells[1],
            cell_size = 1,
            sight = 13,
            duration = 7
        })
        actions.camera_pan(general_cells[1], 2)
        scenario.hold_camera()

        actions.wait(0.5)
        scenario.highlight_entity(generals[1])
        scenario.chat("If you take out their generals, their army will lose composure.")
        actions.wait(2)

        for index = 2,#generals do
            scenario.fog_reveal({
                cell = general_cells[index],
                cell_size = 1,
                sight = 13,
                duration = 7
            })
            actions.camera_pan(general_cells[index], 2)
            scenario.highlight_entity(generals[index])
            actions.hold_camera(2)
        end

        actions.camera_pan(camera_start_cell, 3)

        actions.wait(0.5)
        objectives.announce_new_objective(OBJECTIVE_KILL_GENERALS)
        objectives.add_objective({
            objective = {
                description = OBJECTIVE_KILL_GENERALS,
                counter_target = generals_killed_target
            },
            complete_fn = function ()
                return #generals == 0
            end
        })

        actions.wait(5)

        scenario.chat("You can now hire Detectives from the Sheriff's Office.")
        actions.wait(4)
        scenario.chat("Detectives can turn invisible, but they reveal themselves once they attack.")
    end)
end

function scenario_update()
    objectives.update()

    -- Check contain units alive status
    local contain_unit_index = 1
    while contain_unit_index < #scenario.constants.CONTAIN_UNITS do
        local unit = entities.get_by_id(scenario.constants.CONTAIN_UNITS[contain_unit_index])
        if unit == nil or unit.health == 0 then
            scenario.bot_release_entity(ENEMY_PLAYER_ID, scenario.constants.CONTAIN_UNITS[contain_unit_index])
            table.remove(scenario.constants.CONTAIN_UNITS, contain_unit_index)
        else
            contain_unit_index = contain_unit_index + 1
        end
    end

    -- Check contain balloon alive status
    contain_unit_index = 1
    while contain_unit_index < #scenario.constants.CONTAIN_BALLOONS do
        local unit = entities.get_by_id(scenario.constants.CONTAIN_BALLOONS[contain_unit_index])
        if unit == nil or unit.health == 0 then
            scenario.bot_release_entity(ENEMY_PLAYER_ID, scenario.constants.CONTAIN_BALLOONS[contain_unit_index])
            table.remove(scenario.constants.CONTAIN_BALLOONS, contain_unit_index)
        else
            contain_unit_index = contain_unit_index + 1
        end
    end

    -- Check status of generals
    local general_index = 1
    while general_index <= #generals do
        local general = entities.get_by_id(generals[general_index])
        if general == nil or general.health == 0 then
            table.remove(generals, general_index)
        else
            general_index = general_index + 1
        end
    end
    local generals_killed = generals_killed_target - #generals
    -- Containment retreat
    if generals_killed ~= cached_generals_killed and #scenario.constants.CONTAIN_UNITS >= 2 then
        actions.run(function ()
            actions.wait(3)
            scenario.fog_reveal({
                cell = scenario.constants.CONTAIN_REVEAL_CELL,
                cell_size = 2,
                sight = 13,
                duration = 7
            })
            scenario.create_alert(scenario.ALERT_COLOR_WHITE, scenario.constants.CONTAIN_REVEAL_CELL, 2)
            if generals_killed == 1 then
                scenario.chat("Nicely done. Look, some of their army is retreating!")
            elseif generals_killed == 2 then
                scenario.chat("Nice work. The rest of their containment is retreating now.")
            end

            local retreat_units = {}
            if generals_killed == 1 then
                local contain_units_index = 1
                local should_retreat_flag = true
                while contain_units_index <= #scenario.constants.CONTAIN_UNITS do
                    if should_retreat_flag then
                        table.insert(retreat_units, scenario.constants.CONTAIN_UNITS[contain_units_index])
                        table.remove(scenario.constants.CONTAIN_UNITS, contain_units_index)
                    else
                        contain_units_index = contain_units_index + 1
                    end
                    should_retreat_flag = not should_retreat_flag
                end
            else
                retreat_units = scenario.constants.CONTAIN_UNITS
                scenario.constants.CONTAIN_UNITS = {}
            end

            for index = 1,#retreat_units do
                scenario.bot_release_entity(ENEMY_PLAYER_ID, retreat_units[index])
            end

            local retreat_cell
            if generals_killed == 1 then
                retreat_cell = scenario.constants.CONTAIN_RETREAT_CELL1
            else
                retreat_cell = scenario.constants.CONTAIN_RETREAT_CELL2
            end

            -- Retreat units
            scenario.queue_match_input({
                player_id = ENEMY_PLAYER_ID,
                type = scenario.match_input_type.MOVE_CELL,
                target_cell = retreat_cell,
                entity_ids = retreat_units
            })

            -- Retreat balloons
            if generals_killed == 2 then
                for index = 1,#scenario.constants.CONTAIN_BALLOONS do
                    scenario.bot_release_entity(ENEMY_PLAYER_ID, scenario.constants.CONTAIN_BALLOONS[index])
                end

                scenario.queue_match_input({
                    player_id = ENEMY_PLAYER_ID,
                    type = scenario.match_input_type.MOVE_CELL,
                    target_cell = retreat_cell,
                    entity_ids = scenario.constants.CONTAIN_BALLOONS
                })
            end
        end)
    end
    -- Update objective counter
    if objectives.current_objective == OBJECTIVE_KILL_GENERALS and generals_killed ~= cached_generals_killed then
        scenario.log("Generals killed ", generals_killed, " #generals ", #generals, " cached_generals_killed ", cached_generals_killed, " target ", generals_killed_target)
        scenario.set_objective_variable_counter(0, generals_killed)
        cached_generals_killed = generals_killed
    end

    if not is_match_over and scenario.are_objectives_complete() then
        is_match_over = true
        actions.run(function ()
            objectives.announce_objectives_complete()
            scenario.set_match_over_victory()
        end)
    end

    actions.update()
end
