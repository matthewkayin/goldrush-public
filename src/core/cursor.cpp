#include "cursor.h"

#include "logger.h"
#include "core/resource.h"
#include <SDL3/SDL.h>
#include <cstdio>
#include <unordered_map>

struct CursorParams {
    ResourceName resource;
    int hot_x;
    int hot_y;
};

static const std::unordered_map<CursorName, CursorParams> CURSOR_PARAMS = {
    { CURSOR_DEFAULT, (CursorParams) {
        .resource = RESOURCE_CURSOR_DEFAULT,
        .hot_x = 0,
        .hot_y = 0
    }},
    { CURSOR_TARGET, (CursorParams) {
        .resource = RESOURCE_CURSOR_TARGET,
        .hot_x = 9,
        .hot_y = 9
    }}
};

static SDL_Cursor* cursors[CURSOR_COUNT];
static CursorName current_cursor;

bool cursor_init() {
    for (int cursor = 0; cursor < CURSOR_COUNT; cursor++) {
        const CursorParams& params = CURSOR_PARAMS.at((CursorName)cursor);

        // Load the cursor resource data
        size_t cursor_data_length;
        void* cursor_data = resource_load(params.resource, &cursor_data_length);
        if (!cursor_data) {
            log_error("Unable to load cursor %s.", resource_get_path(params.resource));
        }

        SDL_IOStream* io_stream = SDL_IOFromMem(cursor_data, cursor_data_length);
        SDL_Surface* cursor_surface = SDL_LoadPNG_IO(io_stream, true);
        if (cursor_surface == NULL) {
            log_error("Unable to init surface %s for cursor: %s", resource_get_path(params.resource), SDL_GetError());
            return false;
        }

        cursors[cursor] = SDL_CreateColorCursor(cursor_surface, params.hot_x, params.hot_y);
        if (cursors[cursor] == NULL) {
            log_error("Unable to create cursor with path %s: %s", resource_get_path(params.resource), SDL_GetError());
            return false;
        }

        SDL_DestroySurface(cursor_surface);
    }

    current_cursor = CURSOR_DEFAULT;
    SDL_SetCursor(cursors[current_cursor]);

    return true;
}

void cursor_quit() {
    for (int cursor = 0; cursor < CURSOR_COUNT; cursor++) {
        SDL_DestroyCursor(cursors[cursor]);
    }
}

void cursor_set(CursorName cursor) {
    if (current_cursor == cursor) {
        return;
    }

    current_cursor = cursor;
    SDL_SetCursor(cursors[current_cursor]);
}
