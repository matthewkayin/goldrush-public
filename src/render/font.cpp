#include "font.h"

#include "core/resource.h"
#include "render/ui_color.h"
#include <unordered_map>

static const std::unordered_map<FontName, FontParams> FONT_PARAMS = {
    { FONT_HACK_OFFBLACK, (FontParams) {
        .resource = RESOURCE_FONT_HACK,
        .options = FONT_OPTION_IGNORE_BEARING,
        .size = 10,
        .color = (SDL_Color) { .r = 40, .g = 37, .b = 45, .a = 255 }
    }},
    { FONT_HACK_SHADOW, (FontParams) {
        .resource = RESOURCE_FONT_HACK,
        .options = FONT_OPTION_IGNORE_BEARING,
        .size = 10,
        .color = (SDL_Color) { .r = 40, .g = 37, .b = 45, .a = 200 }
    }},
    { FONT_HACK_WHITE, (FontParams) {
        .resource = RESOURCE_FONT_HACK,
        .options = FONT_OPTION_IGNORE_BEARING,
        .size = 10,
        .color = (SDL_Color) { .r = 255, .g = 255, .b = 255, .a = 255 }
    }},
    { FONT_HACK_GOLD, (FontParams) {
        .resource = RESOURCE_FONT_HACK,
        .options = FONT_OPTION_IGNORE_BEARING,
        .size = 10,
        .color = (SDL_Color) { .r = 238, .g = 209, .b = 158, .a = 255 }
    }},
    { FONT_HACK_GOLD_SATURATED, (FontParams) {
        .resource = RESOURCE_FONT_HACK,
        .options = FONT_OPTION_IGNORE_BEARING,
        .size = 10,
        .color = (SDL_Color) { .r = 237, .g = 200, .b = 135, .a = 255 }
    }},
    { FONT_HACK_PLAYER0, (FontParams) {
        .resource = RESOURCE_FONT_HACK,
        .options = FONT_OPTION_IGNORE_BEARING,
        .size = 10,
        .color = PLAYER_UI_COLOR[0]
    }},
    { FONT_HACK_PLAYER1, (FontParams) {
        .resource = RESOURCE_FONT_HACK,
        .options = FONT_OPTION_IGNORE_BEARING,
        .size = 10,
        .color = PLAYER_UI_COLOR[1]
    }},
    { FONT_HACK_PLAYER2, (FontParams) {
        .resource = RESOURCE_FONT_HACK,
        .options = FONT_OPTION_IGNORE_BEARING,
        .size = 10,
        .color = PLAYER_UI_COLOR[2]
    }},
    { FONT_HACK_PLAYER3, (FontParams) {
        .resource = RESOURCE_FONT_HACK,
        .options = FONT_OPTION_IGNORE_BEARING,
        .size = 10,
        .color = PLAYER_UI_COLOR[3]
    }},
    { FONT_WESTERN16_OFFBLACK, (FontParams) {
        .resource = RESOURCE_FONT_WESTERN,
        .options = 0,
        .size = 16,
        .color = (SDL_Color) { .r = 40, .g = 37, .b = 45, .a = 255 }
    }},
    { FONT_WESTERN8_OFFBLACK, (FontParams) {
        .resource = RESOURCE_FONT_WESTERN,
        .options = 0,
        .size = 8,
        .color = (SDL_Color) { .r = 40, .g = 37, .b = 45, .a = 255 }
    }},
    { FONT_WESTERN8_WHITE, (FontParams) {
        .resource = RESOURCE_FONT_WESTERN,
        .options = 0,
        .size = 8,
        .color = (SDL_Color) { .r = 255, .g = 255, .b = 255, .a = 255 }
    }},
    { FONT_WESTERN8_GOLD, (FontParams) {
        .resource = RESOURCE_FONT_WESTERN,
        .options = 0,
        .size = 8,
        .color = (SDL_Color) { .r = 238, .g = 209, .b = 158, .a = 255 }
    }},
    { FONT_WESTERN8_RED, (FontParams) {
        .resource = RESOURCE_FONT_WESTERN,
        .options = 0,
        .size = 8,
        .color = (SDL_Color) { .r = 186, .g = 97, .b = 95, .a = 255 }
    }},
    { FONT_M3X6_OFFBLACK, (FontParams) {
        .resource = RESOURCE_FONT_M3X6,
        .options = 0,
        .size = 16,
        .color = (SDL_Color) { .r = 40, .g = 37, .b = 45, .a = 255 }
    }},
    { FONT_M3X6_DARKBLACK, (FontParams) {
        .resource = RESOURCE_FONT_M3X6,
        .options = 0,
        .size = 16,
        .color = (SDL_Color) { .r = 16, .g = 15, .b = 18, .a = 255 }
    }},
    { FONT_M3X6_WHITE, (FontParams) {
        .resource = RESOURCE_FONT_M3X6,
        .options = 0,
        .size = 16,
        .color = (SDL_Color) { .r = 255, .g = 255, .b = 255, .a = 255 }
    }}
};

const FontParams& resource_get_font_params(FontName name) {
    return FONT_PARAMS.at(name);
}
