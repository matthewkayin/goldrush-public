#include "constant.h"

#ifdef GOLD_DEBUG

#include "editor/state.h"

static const Rect MENU_RECT = (Rect) {
     .x = (SCREEN_WIDTH / 2) - (300 / 2),
     .y = 64,
     .w = 300,
     .h = 112
 };

EditorMenuConstant::EditorMenuConstant(const char* initial_constant_name) {
    constant_name = std::string(initial_constant_name);
}

const char* EditorMenuConstant::get_header_text() const {
    return "Rename Constant";
}

Rect EditorMenuConstant::get_rect() const {
    return MENU_RECT;
}

void EditorMenuConstant::child_update(EditorState* state) {
    ui_element_position(state->ui_context, ivec2(MENU_RECT.x + 8, MENU_RECT.y + 30));
    ui_text_input(state->ui_context, "Name: ", ivec2(MENU_RECT.w - 32, 24), &constant_name, SCENARIO_CONSTANT_NAME_BUFFER_LENGTH - 1);
}

void EditorMenuConstant::on_submit(EditorState* state) {
    const ScenarioConstant previous_constant = state->scenario->constants[state->tool.constants.constant_index];

    // If the constant name has not changed, then don't create an action
    if (strcmp(constant_name.c_str(), previous_constant.name) == 0) {
        return;
    }

    ScenarioConstant edited_constant = previous_constant;
    strncpy(edited_constant.name, constant_name.c_str(), SCENARIO_CONSTANT_NAME_BUFFER_LENGTH);

    editor_state_do_action(state, (EditorActionEditConstant) {
        .index = state->tool.constants.constant_index,
        .previous_value = previous_constant,
        .new_value = edited_constant
    });
}

#endif
