#include "animation.h"

#include "menu/types.h"
#include "render/render.h"
#include "network/network.h"

static const int WAGON_X_DEFAULT = 380;
static const int WAGON_X_LOBBY = 480;
static const int PARALLAX_TIMER_DURATION = 2;

static const int MENU_TILE_WIDTH = (SCREEN_WIDTH / TILE_SIZE) * 2;
static const int MENU_TILE_HEIGHT = 2;
static const ivec2 MENU_DECORATION_COORDS[3] = {
    ivec2(680, -8),
    ivec2(920, 30),
    ivec2(1250, -8)
};

static const int CLOUD_COUNT = 6;
static const ivec2 CLOUD_COORDS[CLOUD_COUNT] = {
    ivec2(640, 16), ivec2(950, 64),
    ivec2(1250, 48), ivec2(-30, 48),
    ivec2(320, 32), ivec2(1600, 32)
};
static const int CLOUD_FRAME_X[CLOUD_COUNT] = { 0, 1, 2, 2, 1, 1};

void menu_animation_render_decoration(const MenuAnimationState& state, int index);

MenuAnimationState menu_animation_init() {
    MenuAnimationState state;

    state.wagon_animation = animation_create(ANIMATION_UNIT_MOVE_SLOW);
    state.wagon_x = WAGON_X_DEFAULT;
    state.parallax_x = 0;
    state.parallax_cloud_x = 0;
    state.parallax_timer = PARALLAX_TIMER_DURATION;
    state.parallax_cactus_offset = 0;

    return state;
}

void menu_animation_update(MenuAnimationState& state, MenuMode mode) {
    animation_update(state.wagon_animation);
    state.parallax_timer--;
    state.parallax_x = (state.parallax_x + 1) % (SCREEN_WIDTH * 2);
    if (state.parallax_timer == 0) {
        state.parallax_cloud_x = (state.parallax_cloud_x + 1) % (SCREEN_WIDTH * 2);
        if (state.parallax_x == 0) {
            state.parallax_cactus_offset = (state.parallax_cactus_offset + 1) % 5;
        }
        state.parallax_timer = PARALLAX_TIMER_DURATION;
    }
    int expected_wagon_x;
    if (mode == MENU_MODE_LOBBY || mode == MENU_MODE_SKIRMISH_LOBBY) {
        expected_wagon_x = WAGON_X_LOBBY;
    } else if (mode == MENU_MODE_LOAD_MATCH_COUNTDOWN || mode == MENU_MODE_LOAD_SCENARIO_COUNTDOWN) {
        expected_wagon_x = SCREEN_WIDTH;
    } else {
        expected_wagon_x = WAGON_X_DEFAULT;
    }
    if (state.wagon_x < expected_wagon_x) {
        state.wagon_x++;
    } else if (state.wagon_x > expected_wagon_x) {
        state.wagon_x--;
    }
}

void menu_animation_render_background(const MenuAnimationState& state) {
    // Sky background
    render_fill_rect({ .x = 0, .y = 0, .w = SCREEN_WIDTH, .h = SCREEN_HEIGHT }, RENDER_COLOR_BLUE);

    // Render clouds
    const SpriteInfo& cloud_sprite_info = render_get_sprite_info(SPRITE_UI_CLOUDS);
    for (int index = 0; index < CLOUD_COUNT; index++) {
        Rect src_rect = {
            .x = 0 + (CLOUD_FRAME_X[index] * cloud_sprite_info.frame_width),
            .y = 0,
            .w = cloud_sprite_info.frame_width,
            .h = cloud_sprite_info.frame_height
        };
        Rect dst_rect = {
            .x = CLOUD_COORDS[index].x - state.parallax_cloud_x,
            .y = CLOUD_COORDS[index].y,
            .w = src_rect.w * 2,
            .h = src_rect.h * 2
        };
        render_sprite(SPRITE_UI_CLOUDS, src_rect, dst_rect, 0);
    }
}

void menu_animation_render(const MenuAnimationState& state, MenuMode mode) {
    // Tiles
    for (int y = 0; y < MENU_TILE_HEIGHT; y++) {
        for (int x = 0; x < MENU_TILE_WIDTH; x++) {
            SpriteName sprite = (x + y) % 10 == 0 ? SPRITE_TILE_SAND2 : SPRITE_TILE_SAND1;
            const SpriteInfo& sprite_info = render_get_sprite_info(sprite);
            Rect src_rect = {
                .x = 0,
                .y = 0,
                .w = sprite_info.frame_width,
                .h = sprite_info.frame_height
            };
            Rect dst_rect = {
                .x = (x * TILE_SIZE * 2) - state.parallax_x,
                .y = SCREEN_HEIGHT - ((MENU_TILE_HEIGHT - y) * TILE_SIZE * 2),
                .w = TILE_SIZE * 2,
                .h = TILE_SIZE * 2
            };
            render_sprite(sprite, src_rect, dst_rect, 0);
        }
    }

    // Render decorations behind wagon
    menu_animation_render_decoration(state, 0);
    menu_animation_render_decoration(state, 2);

    // Wagon animation
    const SpriteInfo& sprite_wagon_info = render_get_sprite_info(SPRITE_UNIT_WAGON);
    // Wagon color defaults to blue
    int wagon_recolor_id = 0;
    if (mode == MENU_MODE_LOBBY || mode == MENU_MODE_SKIRMISH_LOBBY || mode == MENU_MODE_LOAD_MATCH_COUNTDOWN) {
        // If we are in lobby, wagon color is player color
        wagon_recolor_id = network_get_player(network_get_player_id()).recolor_id;
    }
    Rect wagon_src_rect = {
        .x = sprite_wagon_info.frame_width * state.wagon_animation.frame.x,
        .y = (wagon_recolor_id * sprite_wagon_info.frame_height * sprite_wagon_info.vframes) + (sprite_wagon_info.frame_height * 2),
        .w = sprite_wagon_info.frame_width,
        .h = sprite_wagon_info.frame_height
    };
    Rect wagon_dst_rect = {
        .x = state.wagon_x,
        .y = SCREEN_HEIGHT - 88,
        .w = wagon_src_rect.w * 2,
        .h = wagon_src_rect.h * 2
    };
    render_sprite(SPRITE_UNIT_WAGON, wagon_src_rect, wagon_dst_rect, RENDER_SPRITE_NO_CULL);

    // Render decorations in front of wagon
    menu_animation_render_decoration(state, 1);
}

void menu_animation_render_decoration(const MenuAnimationState& state, int index) {
    int cactus_index = ((index * 2) + state.parallax_cactus_offset) % 5;
    if (cactus_index == 2) {
        cactus_index = 0;
    }
    const SpriteInfo& sprite_info = render_get_sprite_info(SPRITE_DECORATION_ARIZONA);
    Rect src_rect = {
        .x = 0,
        .y = 0,
        .w = sprite_info.frame_width,
        .h = sprite_info.frame_height
    };
    Rect dst_rect = {
        .x = MENU_DECORATION_COORDS[index].x - state.parallax_x,
        .y = SCREEN_HEIGHT - (MENU_TILE_HEIGHT * TILE_SIZE * 2) + MENU_DECORATION_COORDS[index].y,
        .w = TILE_SIZE * 2,
        .h = TILE_SIZE * 2
    };
    render_sprite(SPRITE_DECORATION_ARIZONA, src_rect, dst_rect, 0);
}
