#pragma once

#include "defines.h"

#ifdef GOLD_DEBUG

#include "core/ui.h"

bool editor_ui_dropdown(UiContext& ui_context, const char* prompt, uint32_t* selection, const std::vector<std::string>& items, const Rect& rect, int scroll_max_visible_items = -1);
bool editor_ui_slider(UiContext& ui_context, const char* prompt, uint32_t* value, const UiSliderParams& params, const Rect& rect);
bool editor_ui_prompt_and_button(UiContext& ui, const char* prompt, const char* button, Rect rect);

#endif
