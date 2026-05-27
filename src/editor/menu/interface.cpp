#include "interface.h"

#ifdef GOLD_DEBUG

#include "core/input.h"
#include "core/ui.h"
#include "editor/state.h"

IEditorMenu::IEditorMenu() {
    _is_open = true;
}

void IEditorMenu::update(EditorState* state) {
    // Frame
    state->ui_context.input_enabled = true;
    const Rect frame_rect = get_rect();
    ui_frame_rect(state->ui_context, get_rect());

    // Header
    ivec2 header_text_size = render_get_text_size(FONT_HACK_GOLD, get_header_text());
    ui_element_position(state->ui_context, ivec2(frame_rect.x + (frame_rect.w / 2) - (header_text_size.x / 2), frame_rect.y + 6));
    ui_text(state->ui_context, FONT_HACK_GOLD, get_header_text());

    // Child update
    child_update(state);

    // Back button
    ui_element_position(state->ui_context, ui_button_position_frame_bottom_left(frame_rect));
    if (ui_button(state->ui_context, "Back")) {
        _is_open = false;
    }

    // Save button
    ui_element_position(state->ui_context, ui_button_position_frame_bottom_right(frame_rect, "Save"));
    if (ui_button(state->ui_context, "Save")) {
        _is_open = false;
        on_submit(state);
    }

    // Stop text input
    if (input_is_text_input_active() && !_is_open) {
        input_stop_text_input();
    }
}

bool IEditorMenu::is_open() const {
    return _is_open;
}

#endif
