local actions = require("actions")
local objectives = require("objectives")
local entities = require("entities")
local entity_util = require("entity_util")
local squad_util = require("squad_util")
local ivec2 = require("ivec2")

local ENEMY_PLAYER_ID = 1
local OBJECTIVE_CLAIM_CRATES = "Secure the Gold Crates"

local crates_total = 0
local cached_crates_claimed = 0
local is_match_over = false

function scenario_init()
    scenario.hold_camera()

    local crates = entity_util.find_entities(function (entity)
        return entity.type == scenario.entity_type.CRATE
    end)
    scenario.log("Crate total", #crates)
    crates_total = #crates

    actions.run(intro_cutscene)
end

function scenario_update()
    objectives.update()

    if not is_match_over and objectives.current_objective == OBJECTIVE_CLAIM_CRATES and scenario.are_objectives_complete() then
        is_match_over = true
        actions.run(function()
            objectives.announce_objectives_complete()
            scenario.set_match_over_victory()
        end)
    end

    -- Check crates
    local crates = entity_util.find_entities(function (entity)
        return entity.type == scenario.entity_type.CRATE
    end)
    local crates_claimed = 0
    for crate_index = 1,#crates do
        local crate = entities.get_by_id(crates[crate_index])
        if crate.gold_held == 0 then
            crates_claimed = crates_claimed + 1
        end
    end
    if crates_claimed ~= cached_crates_claimed then
        scenario.set_objective_variable_counter(0, crates_claimed)
        cached_crates_claimed = crates_claimed
    end

    enemy_miner_defense_update()

    actions.update()
end

function intro_cutscene()
    -- Fog reveal the empty goldmine
    local empty_goldmine = entities.get_by_id(scenario.constants.EMPTY_GOLDMINE)
    scenario.fog_reveal({
        cell = empty_goldmine.cell,
        cell_size = 3,
        sight = 9,
        duration = 7
    })

    -- Move the wagon
    local wagon = entities.get_by_id(scenario.constants.INTRO_PLAYER_WAGON)
    scenario.queue_match_input({
        player_id = scenario.PLAYER_ID,
        type = scenario.match_input_type.MOVE_CELL,
        target_cell = { x = wagon.cell.x - 20, y = wagon.cell.y },
        entity_id = scenario.constants.INTRO_PLAYER_WAGON
    })

    -- Wait a moment for the queued inputs to go through
    actions.wait(1.0)

    -- Camera pan to player base
    actions.camera_pan(scenario.constants.INTRO_CAM0, 2.0)
    scenario.hold_camera()
    actions.wait(2.0)

    -- Dialog 1
    actions.wait(0.25)
    scenario.chat("You've scouted the whole valley. Every mine out here has been picked clean.")

    actions.wait(3.0)

    -- Reveal and camera pan to center town
    scenario.fog_reveal({
        cell = scenario.constants.INTRO_FOG1,
        cell_size = 1,
        sight = 23,
        duration = 13
    })
    actions.camera_pan(scenario.constants.INTRO_CAM1, 2.0)
    scenario.hold_camera()

    -- Begin camera pan down through the town
    actions.wait(0.25)
    scenario.begin_camera_pan(scenario.constants.INTRO_CAM2, 5.0)
    actions.wait(0.25)
    for index = 1, #scenario.constants.INTRO_GOLD_CRATES do
        scenario.highlight_entity(scenario.constants.INTRO_GOLD_CRATES[index])
    end
    actions.wait(0.25)
    scenario.chat("Looks like these prospectors still have some gold stockpiled, though.")
    actions.wait(3.0)

    repeat
        coroutine.yield()
    until scenario.get_camera_mode() == scenario.CAMERA_MODE_FREE

    actions.wait(0.25)
    scenario.chat("If you're gonna survive out here, you'll have to take it.")
    actions.wait(1.0)

    actions.camera_pan(scenario.constants.INTRO_CAM0, 2.0)
    scenario.release_camera()

    actions.wait(1.0)
    objectives.announce_new_objective(OBJECTIVE_CLAIM_CRATES)
    objectives.add_objective({
        objective = {
            description = OBJECTIVE_CLAIM_CRATES,
            counter_target = crates_total
        },
        complete_fn = function()
            return cached_crates_claimed >= crates_total
        end
    })

    actions.wait(5.0)
    scenario.hint("You can now research upgrades from the Blacksmith")
end

function enemy_miner_defense_update()
    for miner_index = 1,#scenario.constants.ENEMY_MINERS do
        local miner_id = scenario.constants.ENEMY_MINERS[miner_index]
        local miner = entities.get_by_id(miner_id)
        if miner == nil or miner.health == 0 or miner.target.type ~= scenario.target_type.NONE then
            goto continue
        end

        for entity_index = 0,(entities.get_count() - 1) do
            local entity = entities.get_by_index(entity_index)
            if entity.player_id ~= scenario.PLAYER_ID or entity_util.entity_is_building(entity.type) then
                goto entity_continue
            end
            if ivec2.manhattan_distance(entity.cell, miner.cell) > 16 then
                goto entity_continue
            end

            scenario.queue_match_input({
                player_id = ENEMY_PLAYER_ID,
                type = scenario.match_input_type.MOVE_ATTACK_CELL,
                target_cell = ivec2.from_cdata(entity.cell),
                entity_id = miner_id
            })
            goto continue

            ::entity_continue::
        end

        ::continue::
    end
end
