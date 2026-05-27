#pragma once

#include "util/math.h"
#include "render/sprite.h"
#include "profile/profile.h"
#include <vector>

struct RenderSpriteParams {
    SpriteName sprite;
    ivec2 frame;
    ivec2 position;
    int ysort_position;
    uint32_t options;
    int recolor_id;
};

// This is the actual function that does the work
void _ysort_render_params(std::vector<RenderSpriteParams>& params, int low, int high);

// This is a wrapper function that profiles the ysort
// We need the wrapper function because ysort is recursive
#define ysort_render_params(params, low, high)           \
    {                                                    \
        ZoneScopedN("ysort_render_params");  \
        _ysort_render_params(params, low, high);         \
    }
