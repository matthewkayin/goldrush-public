#pragma once

#include "defines.h"

#ifdef GOLD_DEBUG

#include "match/scenario/scenario.h"
#include <vector>
#include <variant>

enum EditorActionMode {
    EDITOR_ACTION_MODE_DO,
    EDITOR_ACTION_MODE_UNDO
};

struct EditorActionBrushStroke {
    int index;
    uint8_t previous_value;
    uint8_t new_value;
};

struct EditorActionBrush {
    std::vector<EditorActionBrushStroke> stroke;

    void execute(Scenario* scenario, EditorActionMode mode) const;
};

struct EditorActionAddEntity {
    EntityType type;
    uint8_t player_id;
    ivec2 cell;

    void execute(Scenario* scenario, EditorActionMode mode) const;
};

struct EditorActionEditEntity {
    uint32_t index;
    ScenarioEntity previous_value;
    ScenarioEntity new_value;

    void execute(Scenario* scenario, EditorActionMode mode) const;
};

struct EditorActionRemoveEntity {
    uint32_t index;
    ScenarioEntity value;
    uint32_t squad_index;
    std::vector<uint32_t> constant_indices;

    void execute(Scenario* scenario, EditorActionMode mode) const;
};

struct EditorActionAddSquad {
    void execute(Scenario* scenario, EditorActionMode mode) const;
};

struct EditorActionEditSquad {
    uint32_t index;
    ScenarioSquad previous_value;
    ScenarioSquad new_value;

    void execute(Scenario* scenario, EditorActionMode mode) const;
};

struct EditorActionRemoveSquad {
    uint32_t index;
    ScenarioSquad value;

    void execute(Scenario* scenario, EditorActionMode mode) const;
};

struct EditorActionSetPlayerSpawn {
    ivec2 previous_value;
    ivec2 new_value;

    void execute(Scenario* scenario, EditorActionMode mode) const;
};

struct EditorActionAddConstant {
    void execute(Scenario* scenario, EditorActionMode mode) const;
};

struct EditorActionEditConstant {
    uint32_t index;
    ScenarioConstant previous_value;
    ScenarioConstant new_value;

    void execute(Scenario* scenario, EditorActionMode mode) const;
};

struct EditorActionRemoveConstant {
    uint32_t index;
    ScenarioConstant value;

    void execute(Scenario* scenario, EditorActionMode mode) const;
};

using EditorAction = std::variant<
    EditorActionBrush,
    EditorActionAddEntity,
    EditorActionEditEntity,
    EditorActionRemoveEntity,
    EditorActionAddSquad,
    EditorActionEditSquad,
    EditorActionRemoveSquad,
    EditorActionSetPlayerSpawn,
    EditorActionAddConstant,
    EditorActionEditConstant,
    EditorActionRemoveConstant
>;

// Action

inline void editor_action_execute(Scenario* scenario, const EditorAction& action, EditorActionMode mode) {
    std::visit([scenario, mode](const auto& sub_action) {
        sub_action.execute(scenario, mode);
    }, action);
}

#endif
