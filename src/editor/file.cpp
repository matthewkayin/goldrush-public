#include "editor/state.h"

#ifdef GOLD_DEBUG

#include "util/util.h"
#include <SDL3/SDL.h>

static const SDL_DialogFileFilter EDITOR_FILE_FILTERS[] = {
    { "JSON files", "json" }
};

static const SDL_DialogFileFilter EDITOR_SCN_FILE_FILTERS[] = {
    { "Scenario files", "scn" }
};

std::string editor_get_scenario_folder_path() {
    std::string base_path = SDL_GetBasePath();
    // The substring is to remove the trailing "bin/"
    return base_path.substr(0, base_path.size() - 4) + "scenario" + GOLD_PATH_SEPARATOR;
}

static void SDLCALL editor_save_callback(void* user_data, const char* const* filelist, int /*filter*/) {
    EditorState* state = (EditorState*)user_data;
    state->is_in_file_menu = false;

    if (!filelist) {
        log_error("Error occured while saving files: %s", SDL_GetError());
        return;
    }

    if (!*filelist) {
        return;
    }

    editor_state_save_document(state, *filelist);
}

void editor_state_open_file_save_dialog(EditorState* state) {
    state->is_in_file_menu = true;
    SDL_ShowSaveFileDialog(editor_save_callback, state, state->window, EDITOR_FILE_FILTERS, 1, editor_get_scenario_folder_path().c_str());
}

static void SDLCALL editor_open_callback(void* user_data, const char* const* filelist, int /*filter*/) {
    EditorState* state = (EditorState*)user_data;
    state->is_in_file_menu = false;

    if (!filelist) {
        log_error("Error occured while opening files: %s", SDL_GetError());
        return;
    }

    if (!*filelist) {
        return;
    }

    Scenario* opened_scenario = scenario_open_file(*filelist);
    if (opened_scenario == NULL) {
        return;
    }

    editor_state_free_document(state);
    state->scenario = opened_scenario;
    state->scenario_path = std::string(*filelist);
    state->scenario_is_saved = true;

    int lcg_seed = state->scenario->map_bake_lcg_seed;
    map_init(state->map, state->scenario->map_type, state->scenario->raw_map, &lcg_seed);
    editor_state_update_map(state);
}

void editor_state_open_file_open_dialog(EditorState* state) {
    state->is_in_file_menu = true;
    SDL_ShowOpenFileDialog(editor_open_callback, state, state->window, EDITOR_FILE_FILTERS, 1, editor_get_scenario_folder_path().c_str(), false);
}

void editor_state_save_document(EditorState* state, const char* path) {
    std::string full_path = std::string(path);
    if (!string_ends_with(full_path, ".json")) {
        full_path += ".json";
    }
    bool success = scenario_save_file(state->scenario, full_path.c_str());
    if (success) {
        state->scenario_path = full_path;
        state->scenario_is_saved = true;
    }
}

static void SDLCALL editor_import_callback(void* user_data, const char* const* filelist, int /*filter*/) {
    EditorState* state = (EditorState*)user_data;
    state->is_in_file_menu = false;

    if (!filelist) {
        log_error("Error occured while opening files: %s", SDL_GetError());
        return;
    }

    if (!*filelist) {
        return;
    }

    Scenario* opened_scenario = scenario_import(*filelist);
    if (opened_scenario == NULL) {
        return;
    }

    editor_state_free_document(state);
    state->scenario = opened_scenario;
    state->scenario_path = "";
    state->scenario_is_saved = false;

    int lcg_seed = state->scenario->map_bake_lcg_seed;
    map_init(state->map, state->scenario->map_type, state->scenario->raw_map, &lcg_seed);
    editor_state_update_map(state);
}

void editor_state_open_file_import_dialog(EditorState* state) {
    state->is_in_file_menu = true;
    SDL_ShowOpenFileDialog(editor_import_callback, state, state->window, EDITOR_SCN_FILE_FILTERS, 1, editor_get_scenario_folder_path().c_str(), false);
}

static void SDLCALL editor_export_callback(void* user_data, const char* const* filelist, int /*filter*/) {
    EditorState* state = (EditorState*)user_data;
    state->is_in_file_menu = false;

    if (!filelist) {
        log_error("Error occured while saving files: %s", SDL_GetError());
        return;
    }

    if (!*filelist) {
        return;
    }

    std::string full_path = std::string(*filelist);
    if (!string_ends_with(full_path, ".scn")) {
        full_path += ".scn";
    }
    bool success = scenario_export(state->scenario, full_path.c_str());
    if (success) {
        state->scenario_path = full_path;
        state->scenario_is_saved = true;
    }
}

void editor_state_open_file_export_dialog(EditorState* state) {
    state->is_in_file_menu = true;
    SDL_ShowSaveFileDialog(editor_export_callback, state, state->window, EDITOR_SCN_FILE_FILTERS, 1, editor_get_scenario_folder_path().c_str());
}

#endif
