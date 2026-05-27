#pragma once

#include "menu/types.h"
#include "core/animation.h"

struct MenuAnimationState {
    Animation wagon_animation;
    int wagon_x;
    int parallax_x;
    int parallax_cloud_x;
    int parallax_timer;
    int parallax_cactus_offset;
    uint32_t match_load_countdown_timer;
};

MenuAnimationState menu_animation_init();
void menu_animation_update(MenuAnimationState& state, MenuMode mode);
void menu_animation_render_background(const MenuAnimationState& state);
void menu_animation_render(const MenuAnimationState& state, MenuMode mode);
