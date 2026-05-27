#include "action.h"

#ifdef GOLD_DEBUG

void EditorActionBrush::execute(Scenario* scenario, EditorActionMode mode) const {
    for (uint32_t index = 0; index < stroke.size(); index++) {
        uint8_t value = mode == EDITOR_ACTION_MODE_UNDO
            ? stroke[index].previous_value
            : stroke[index].new_value;
        scenario->raw_map->data[stroke[index].index] = value;
    }
}

void EditorActionAddEntity::execute(Scenario* scenario, EditorActionMode mode) const {
    if (mode == EDITOR_ACTION_MODE_DO) {
        ScenarioEntity entity;
        entity.type = type;
        entity.player_id = player_id;
        entity.is_rigged = false;
        entity.cell = cell;
        entity.gold_held = 0;
        if (entity.type == ENTITY_GOLDMINE) {
            entity.gold_held = 7500;
        } else if (entity.type == ENTITY_CRATE) {
            entity.gold_held = 4;
        }
        scenario->entities[scenario->entity_count] = entity;
        scenario->entity_count++;
    } else {
        scenario->entity_count--;
    }
}

void EditorActionEditEntity::execute(Scenario* scenario, EditorActionMode mode) const {
    scenario->entities[index] = mode == EDITOR_ACTION_MODE_DO
        ? new_value
        : previous_value;
}

void EditorActionRemoveEntity::execute(Scenario* scenario, EditorActionMode mode) const {
    if (mode == EDITOR_ACTION_MODE_DO) {
        // Remove entity from squad
        if (squad_index != INDEX_INVALID) {
            ScenarioSquad& squad = scenario->squads[squad_index];

            uint32_t squad_entity_index;
            for (squad_entity_index = 0; squad_entity_index < squad.entity_count; squad_entity_index++) {
                if (squad.entities[squad_entity_index] == index) {
                    break;
                }
            }
            GOLD_ASSERT(squad_entity_index != squad.entity_count);

            squad.entities[squad_entity_index] = squad.entities[squad.entity_count - 1];
            squad.entity_count--;
        }

        // Remove entity from constants
        for (uint32_t constant_index : constant_indices) {
            scenario->constants[constant_index].entity_index = INDEX_INVALID;
        }

        // Remove entity
        uint32_t last_entity_index = scenario->entity_count - 1;
        scenario->entities[index] = scenario->entities[last_entity_index];
        scenario->entity_count--;

        // Update references to the swapped entity in squads
        for (ScenarioSquad& squad : scenario->squads) {
            for (uint32_t squad_entity_index = 0; squad_entity_index < squad.entity_count; squad_entity_index++) {
                if (squad.entities[squad_entity_index] == scenario->entity_count) {
                    squad.entities[squad_entity_index] = index;
                }
            }
        }

        // Update references to the swapped entity in constants
        for (ScenarioConstant& constant : scenario->constants) {
            if (constant.type == SCENARIO_CONSTANT_TYPE_ENTITY && constant.entity_index == scenario->entity_count) {
                constant.entity_index = index;
            }
        }
    } else if (mode == EDITOR_ACTION_MODE_UNDO) {
        // Update references to the swapped entity in constants
        for (ScenarioConstant& constant : scenario->constants) {
            if (constant.type == SCENARIO_CONSTANT_TYPE_ENTITY && constant.entity_index == index) {
                constant.entity_index = scenario->entity_count;
            }
        }

        // Update references to the swapped entity in squads
        for (ScenarioSquad& squad : scenario->squads) {
            for (uint32_t squad_entity_index = 0; squad_entity_index < squad.entity_count; squad_entity_index++) {
                if (squad.entities[squad_entity_index] == index) {
                    squad.entities[squad_entity_index] = scenario->entity_count;
                }
            }
        }

        // Un-remove entity
        scenario->entities[scenario->entity_count] = scenario->entities[index];
        scenario->entities[index] = value;
        scenario->entity_count++;

        // Add entity back to constants
        for (uint32_t constant_index : constant_indices) {
            scenario->constants[constant_index].entity_index = index;
        }

        // Add entity back to squads
        if (squad_index != INDEX_INVALID) {
            ScenarioSquad& squad = scenario->squads[squad_index];
            squad.entities[squad.entity_count] = index;
            squad.entity_count++;
        }
    }
}

void EditorActionAddSquad::execute(Scenario* scenario, EditorActionMode mode) const {
    if (mode == EDITOR_ACTION_MODE_DO) {
        ScenarioSquad squad = scenario_squad_init();
        sprintf(squad.name, "Squad %u", (uint32_t)scenario->squads.size() + 1U);
        scenario->squads.push_back(squad);
    } else if (mode == EDITOR_ACTION_MODE_UNDO) {
        scenario->squads.pop_back();
    }
}

void EditorActionEditSquad::execute(Scenario* scenario, EditorActionMode mode) const {
    scenario->squads[index] = mode == EDITOR_ACTION_MODE_DO
        ? new_value
        : previous_value;
}

void EditorActionRemoveSquad::execute(Scenario* scenario, EditorActionMode mode) const {
    if (mode == EDITOR_ACTION_MODE_DO) {
        scenario->squads.erase(scenario->squads.begin() + index);
    } else if (mode == EDITOR_ACTION_MODE_UNDO) {
        scenario->squads.insert(scenario->squads.begin() + index, value);
    }
}

void EditorActionSetPlayerSpawn::execute(Scenario* scenario, EditorActionMode mode) const {
    scenario->player_spawn = mode == EDITOR_ACTION_MODE_DO
        ? new_value
        : previous_value;
}

void EditorActionAddConstant::execute(Scenario* scenario, EditorActionMode mode) const {
    if (mode == EDITOR_ACTION_MODE_DO) {
        ScenarioConstant new_constant;
        sprintf(new_constant.name, "Constant %u", (uint32_t)scenario->constants.size() + 1U);
        new_constant.type = SCENARIO_CONSTANT_TYPE_ENTITY;
        new_constant.entity_index = INDEX_INVALID;

        scenario->constants.push_back(new_constant);
    } else if (mode == EDITOR_ACTION_MODE_UNDO) {
        scenario->constants.pop_back();
    }
}

void EditorActionEditConstant::execute(Scenario* scenario, EditorActionMode mode) const {
    scenario->constants[index] = mode == EDITOR_ACTION_MODE_DO
        ? new_value
        : previous_value;
}

void EditorActionRemoveConstant::execute(Scenario* scenario, EditorActionMode mode) const {
    if (mode == EDITOR_ACTION_MODE_DO) {
        scenario->constants.erase(scenario->constants.begin() + index);
    } else if (mode == EDITOR_ACTION_MODE_UNDO) {
        scenario->constants.insert(scenario->constants.begin() + index, value);
    }
}

#endif
