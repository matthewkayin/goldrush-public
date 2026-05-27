local objectives = require("objectives")
local actions = require("actions")
local squad_util = require("squad_util")
local entities = require("entities")
local ivec2 = require("ivec2")
local list_util = require("list_util")

local OBJECTIVE_PROTECT_VILLAGERS = "Protect the Villagers"

local villager_states = {
    {
        hall_id = scenario.constants.VILLAGER_HALL1,
        goldmine_id = scenario.constants.VILLAGER_MINE1,
        starting_miner_list = scenario.constants.VILLAGER_MINERS1,
        miners = {},
        is_dead = false,
        micro_cooldown = nil
    },
    {
        hall_id = scenario.constants.VILLAGER_HALL2,
        goldmine_id = scenario.constants.VILLAGER_MINE2,
        starting_miner_list = scenario.constants.VILLAGER_MINERS2,
        miners = {},
        is_dead = false,
        micro_cooldown = nil
    },
    {
        hall_id = scenario.constants.VILLAGER_HALL3,
        goldmine_id = scenario.constants.VILLAGER_MINE3,
        starting_miner_list = scenario.constants.VILLAGER_MINERS3,
        miners = {},
        is_dead = false,
        micro_cooldown = nil
    }
}

local HARASS_ATTACK_TYPES = {
    {
        rate = 0.6,
        entity_types = {
            scenario.entity_type.BANDIT,
            scenario.entity_type.BANDIT,
            scenario.entity_type.BANDIT,
            scenario.entity_type.COWBOY,
            scenario.entity_type.COWBOY,
        }
    },
    {
        rate = 0.25,
        entity_types = {
            scenario.entity_type.SAPPER,
            scenario.entity_type.SAPPER,
            scenario.entity_type.COWBOY,
            scenario.entity_type.COWBOY,
        }
    },
    {
        rate = 0.15,
        entity_types = {
            scenario.entity_type.PYRO,
            scenario.entity_type.COWBOY,
            scenario.entity_type.COWBOY,
        }
    }
}

local HARASS_EXTRA_UNIT_RATES = {
    {
        entity_type = scenario.entity_type.BANDIT,
        rate = 0.4
    },
    {
        entity_type = scenario.entity_type.COWBOY,
        rate = 0.4
    },
    {
        entity_type = scenario.entity_type.SAPPER,
        rate = 0.2
    }
}

local VILLAGERS_PLAYER_ID = 1
local BANDITS_PLAYER_ID = 2

local ATTACK_INTERVAL = 90

local is_match_over = false
local next_attack_time
local player_expansion_hall_id = nil
local cached_villager_count = nil

function scenario_init()
    villager_states_init()
    actions.run(function ()
        actions.wait(1.0)
        local previous_camera_cell = scenario.get_camera_centered_cell()

        -- Determine camera pan cells
        local villager_hall_cells = {}
        for index=1,#villager_states do
            local hall = entities.get_by_id(villager_states[index].hall_id)
            if hall == nil then
                error("Villager hall not found")
            end

            table.insert(villager_hall_cells, ivec2.from_cdata(hall.cell))
        end

        -- Camera pan
        local camera_pan_durations = {
            1.5, 2.0, 1.5
        }
        for index=1,#villager_states do
            scenario.fog_reveal({
                cell = villager_hall_cells[index],
                cell_size = 4,
                sight = 13,
                duration = 6.0
            })
            actions.camera_pan(villager_hall_cells[index], camera_pan_durations[index])
            scenario.hold_camera()

            -- Slight pause before sending message
            actions.wait(0.2)

            if index == 1 then
                scenario.chat("Bandits have been hitting these settlements hard.")
            elseif index == 2 then
                scenario.chat("If their towns are destroyed, the villagers will have nowhere else to go.")
            elseif index == 3 then
                -- Announce new objective
                scenario.play_sound(scenario.sound.UI_CLICK)
                scenario.chat_prefixed(scenario.CHAT_COLOR_GOLD, "New Objective:", OBJECTIVE_PROTECT_VILLAGERS)
                objectives.current_objective = OBJECTIVE_PROTECT_VILLAGERS
            end
            actions.wait(1.8)
        end

        actions.camera_pan(previous_camera_cell, 1.5)
        actions.wait(1.0)

        objectives.add_objective({
            objective = {
                description = "Destroy the bandit's base",
            },
            complete_fn = function ()
                return false
            end
        })
        objectives.add_objective({
            objective = {
                description = "The villagers must survive"
            },
            complete_fn = function ()
                return false
            end
        })

        local villager_count = 0
        for index = 1,#villager_states do
            villager_count = villager_count + #(villager_states[index].miners)
        end
        scenario.set_global_objective_counter({
            type = scenario.global_objective_counter_type.VARIABLE,
            header_text = "Villagers Remaining",
            initial_value = villager_count
        })
        cached_villager_count = villager_count

        actions.wait(5.0)
        scenario.hint("You can how hire Wagons, which are fast transport units.")

        actions.wait(15)

        -- Send first harass
        local target_halls = choose_random_harass_targets()
        for index = 1,#target_halls do
            local hall = entities.get_by_id(target_halls[index])
            local spawn_cell
            if target_halls[index] == scenario.constants.VILLAGER_HALL1 then
                spawn_cell = scenario.constants.BANDIT_SPAWN_WOODS
            elseif target_halls[index] == scenario.constants.VILLAGER_HALL2 then
                spawn_cell = scenario.constants.BANDIT_SPAWN_STAIRS
            elseif target_halls[index] == scenario.constants.VILLAGER_HALL3 then
                spawn_cell = scenario.constants.BANDIT_SPAWN_BEAN
            end
            squad_util.spawn_harass_squad({
                player_id = BANDITS_PLAYER_ID,
                target_cell = ivec2.from_cdata(hall.cell),
                spawn_cell = spawn_cell,
                entity_types = {
                    scenario.entity_type.BANDIT,
                    scenario.entity_type.BANDIT,
                    scenario.entity_type.BANDIT
                }
            })
        end

        scenario.bot_set_config_flag(VILLAGERS_PLAYER_ID, scenario.bot_config_flag.SHOULD_PRODUCE, true)

        next_attack_time = scenario.get_time() + ATTACK_INTERVAL
    end)
end

function scenario_update()
    objectives.update()

    -- Villager state update
    local villager_count = 0
    local villager_state_index = 1
    while villager_state_index < #villager_states do
        villager_state_update(villager_states[villager_state_index])
        if villager_states[villager_state_index].is_dead then
            table.remove(villager_states, villager_state_index)
        else
            villager_count = villager_count + #(villager_states[villager_state_index].miners)
            villager_state_index = villager_state_index + 1
        end
    end

    -- Villager count global objective counter update
    if cached_villager_count ~= nil and villager_count ~= cached_villager_count then
        scenario.set_global_objective_counter_variable_value(villager_count)
        cached_villager_count = villager_count
    end

    -- Check for defeat
    if not is_match_over and villager_count == 0 then
        actions.run(function ()
            objectives.announce_objectives_failed()
            scenario.set_match_over_defeat()
        end)
        is_match_over = true
    end

    -- Check for victory
    if not is_match_over and scenario.is_player_defeated(BANDITS_PLAYER_ID) then
        actions.run(function ()
            scenario.complete_objective(0)
            scenario.complete_objective(1)
            objectives.announce_objectives_complete()
            scenario.set_match_over_victory()
        end)
        is_match_over = true
    end

    -- Check for player expansion death
    if player_expansion_hall_id ~= nil then
        local hall = entities.get_by_id(player_expansion_hall_id)
        if hall == nil or hall.health == 0 then
            player_expansion_hall_id = nil
        end
    end

    -- Check for player expansion
    if player_expansion_hall_id == nil then
        player_expansion_hall_id = scenario.get_hall_surrounding_goldmine(scenario.constants.UNOCCUPIED_GOLDMINE)
    end

    -- Spawn harass squads
    if not is_match_over and next_attack_time ~= nil and scenario.get_time() >= next_attack_time then
        local target_hall_ids = choose_random_harass_targets()
        for index = 1,#target_hall_ids do
            harass_villager(target_hall_ids[index])
        end

        next_attack_time = next_attack_time + ATTACK_INTERVAL
    end

    actions.update()
end

function villager_states_init()
    for index = 1,#villager_states do
        scenario.queue_match_input({
            player_id = VILLAGERS_PLAYER_ID,
            type = scenario.match_input_type.MOVE_ENTITY,
            target_id = villager_states[index].goldmine_id,
            entity_ids = villager_states[index].starting_miner_list
        })
    end

    local hall_cells = {}
    for index = 1,#villager_states do
        local hall = entities.get_by_id(villager_states[index].hall_id)
        table.insert(hall_cells, ivec2.from_cdata(hall.cell))
    end

    for entity_index = 0,(entities.get_count()) do
        local entity = entities.get_by_index(entity_index)
        if entity.player_id ~= VILLAGERS_PLAYER_ID or entity.type ~= scenario.entity_type.MINER then
            goto continue
        end

        local entity_cell = ivec2.from_cdata(entity.cell)

        local nearest_index = nil
        for index = 1,#hall_cells do
            if nearest_index == nil or
                    ivec2.manhattan_distance(entity_cell, hall_cells[index]) <
                    ivec2.manhattan_distance(entity_cell, hall_cells[nearest_index]) then
                nearest_index = index
            end
        end

        table.insert(villager_states[nearest_index].miners, entities.get_id_of(entity_index))

        ::continue::
    end
end

function villager_state_update(state)
    -- If dead, do nothing
    if state.is_dead then
        return
    end

    -- If on micro cooldown, do nothing
    if state.micro_cooldown ~= nil and scenario.get_time() < state.micro_cooldown then
        return
    end

    -- Check for dead miners
    local miner_index = 1
    while miner_index < #state.miners do
        local miner = entities.get_by_id(state.miners[miner_index])
        if miner == nil or miner.health == 0 then
            table.remove(state.miners, miner_index)
        else
            miner_index = miner_index + 1
        end
    end

    -- If no more miners, then is_dead
    if #state.miners == 0 then
        state.is_dead = true
        return
    end

    -- Hall is dead, then is dead
    local hall = entities.get_by_id(state.hall_id)
    if hall == nil or hall.health == 0 then
        state.is_dead = true
        return
    end
end

function choose_random_harass_targets()
    local harass_targets = {}
    for index = 1,#villager_states do
        table.insert(harass_targets, villager_states[index].hall_id)
    end
    if player_expansion_hall_id ~= nil then
        table.insert(harass_targets, player_expansion_hall_id)
    end

    if #harass_targets == 1 then
        return { harass_targets[1], harass_targets[1] }
    end

    local target_index1 = math.random(1, #harass_targets)
    local target_index2 = math.random(1, #harass_targets)
    if target_index2 == target_index1 then
        if target_index2 == #harass_targets then
            target_index2 = 1
        else
            target_index2 = target_index2 + 1
        end
    end

    return { harass_targets[target_index1], harass_targets[target_index2] }
end

function harass_choose_spawn_location(hall_id)
    local spawn_cells = {}
    if hall_id == scenario.constants.VILLAGER_HALL1 then
        spawn_cells =  { scenario.constants.BANDIT_SPAWN_CLIFF, scenario.constants.BANDIT_SPAWN_WOODS }
    elseif hall_id == scenario.constants.VILLAGER_HALL2 then
        spawn_cells = { scenario.constants.BANDIT_SPAWN_STAIRS, scenario.constants.BANDIT_SPAWN_BEAN }
    elseif hall_id == scenario.constants.VILLAGER_HALL3 then
        spawn_cells = { scenario.constants.BANDIT_SPAWN_BEAN, scenario.constants.BANDIT_SPAWN_WOODS }
    elseif hall_id == player_expansion_hall_id then
        spawn_cells = { scenario.constants.BANDIT_SPAWN_BEAN, scenario.constants.BANDIT_SPAWN_STAIRS }
    else
        scenario.log("Unrecognized harass hall_id", hall_id)
        error("Unrecognized hall_id")
    end

    local spawn_roll = math.random(1, #spawn_cells)
    return spawn_cells[spawn_roll]
end

function harass_villager(hall_id)
    -- Determine spawn location
    local spawn_cell = harass_choose_spawn_location(hall_id)

    -- Determine attack target based on hall
    local hall = entities.get_by_id(hall_id)
    if hall == nil then
        scenario.log("Getting hall ", hall_id, " by ID failed in spawn_harass_squad().")
        return
    end
    local target_cell = ivec2.from_cdata(hall.cell)

    -- Determine entity types
    local entity_types
    local harass_roll = math.random()
    local harass_dc = 0.0
    for harass_type_index = 1,#HARASS_ATTACK_TYPES do
        harass_dc = harass_dc + HARASS_ATTACK_TYPES[harass_type_index].rate
        if harass_roll < harass_dc then
            entity_types = list_util.copy(HARASS_ATTACK_TYPES[harass_type_index].entity_types)
            break
        end
    end

    -- Add extra units
    local extra_units = 0
    if scenario.get_player_entity_count(scenario.PLAYER_ID, scenario.entity_type.MINER) > 16 then
        extra_units = 2
    end
    if #villager_states < 3 then
        extra_units = 4
    end
    while extra_units > 0 do
        local extra_unit_roll = math.random()
        local extra_unit_dc = 0.0
        for extra_rates_index = 1,#HARASS_EXTRA_UNIT_RATES do
            extra_unit_dc = extra_unit_dc + HARASS_EXTRA_UNIT_RATES[extra_rates_index].rate
            if extra_unit_roll < extra_unit_dc then
                table.insert(entity_types, HARASS_EXTRA_UNIT_RATES[extra_rates_index].entity_type)
                extra_units = extra_units - 1
                break
            end
        end
    end

    -- Spawn harass squad
    squad_util.spawn_harass_squad({
        player_id = BANDITS_PLAYER_ID,
        target_cell = target_cell,
        spawn_cell = spawn_cell,
        entity_types = entity_types
    })
end
