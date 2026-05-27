local ivec2 = require("ivec2")
local entities = require("entities")

local entity_util = {}

-- Returns a list of the IDs of entities which match the passed-in filter function
-- @param filter function
-- @return table
function entity_util.find_entities(filter)
    local entity_list = {}
    for entity_index=0,(entities.get_count() - 1) do
        local entity = entities.get_by_index(entity_index)
        local entity_id = entities.get_id_of(entity_index)
        if filter(entity, entity_id) then
            table.insert(entity_list, entity_id)
        end
    end

    return entity_list
end

-- Returns the ID of the first entity which matches the passed-in filter function
-- @param filter function
-- @return number | nil
function entity_util.find_entity(filter)
    for entity_index=0,(entities.get_count() - 1) do
        local entity = entities.get_by_index(entity_index)
        local entity_id = entities.get_id_of(entity_index)
        if filter(entity, entity_id) then
            return entity_id
        end
    end

    return nil
end

-- Returns the ID of the entity who best satisfies the compare function
-- Only entities which pass the filter will be considered
-- @param filter function
-- @param compare function
-- @return number | nil
function entity_util.find_best_entity(filter, compare)
    local best_entity = nil
    local best_entity_id = nil

    for entity_index=0,(entities.get_count() - 1) do
        local entity = entities.get_by_index(entity_index)
        local entity_id = entities.get_id_of(entity_index)
        if not filter(entity, entity_id) then
            goto continue
        end

        if best_entity_id == nil or compare(entity, best_entity) then
            best_entity = entity
            best_entity_id = entity_id
        end

        ::continue::
    end

    return best_entity_id
end

-- Returns true if a player owned entity is near the cell within the specified distance
-- @param cell ivec2
-- @param distance number
-- @return boolean
function entity_util.player_has_entity_near_cell(player_id, cell, distance)
    local entity_near_cell_id = entity_util.find_entity(function (entity)
        return entity.player_id == player_id and
            ivec2.manhattan_distance(entity.cell, cell) <= distance
    end)
    return entity_near_cell_id ~= nil
end

function entity_util.entity_is_building(entity_type)
    return entity_type >= scenario.entity_type.HALL and entity_type <= scenario.entity_type.LANDMINE
end

return entity_util