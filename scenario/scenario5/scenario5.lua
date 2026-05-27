local objectives = require("objectives")
local actions = require("actions")
local ivec2 = require("ivec2")
local entities = require("entities")

local OBJECTIVE_STEAL_IT_BACK = "Steal it Back!"
local objective_crates_index = nil
local cached_crates_stolen = 0

local crates = {}

local is_match_over = false

function scenario_init()
    for entity_index = 0,(entities.get_count() - 1) do
        local entity = entities.get_by_index(entity_index)
        if entity.type == scenario.entity_type.CRATE then
            table.insert(crates, entities.get_id_of(entity_index))
        end
    end

    scenario.set_next_music_track(scenario.music.MATCH2)

    actions.run(function ()
        actions.wait(1.0)

        scenario.chat("Bandits stole all of your gold!")
        actions.wait(4.0)

        objectives.announce_new_objective(OBJECTIVE_STEAL_IT_BACK)
        local target_crates = #crates
        objective_crates_index = objectives.add_objective({
            objective = {
                description = "Steal back your gold",
                counter_target = target_crates
            },
            complete_fn = function ()
                return cached_crates_stolen == target_crates
            end
        })

        actions.wait(5.0)
        scenario.hint("You can now hire Balloons and Sappers.")
        actions.wait(4.0)
        scenario.chat("Ballons are flying scout units.")
        actions.wait(4.0)
        scenario.chat("They can see invisible units, such as landmines.")
    end)
end

function scenario_update()
    objectives.update()

    -- Handle match over
    if not is_match_over and scenario.are_objectives_complete() then
        actions.run(function ()
            objectives.announce_objectives_complete()
            scenario.set_match_over_victory()
        end)
        is_match_over = true
    end

    -- Cache collected
    local crates_stolen = 0
    for index = 1,#crates do
        local crate = entities.get_by_id(crates[index])
        if crate.gold_held == 0 then
            crates_stolen = crates_stolen + 1
        end
    end

    if crates_stolen ~= cached_crates_stolen and objective_crates_index ~= nil then
        scenario.set_objective_variable_counter(objective_crates_index, crates_stolen)
        cached_crates_stolen = crates_stolen
    end

    actions.update()
end
