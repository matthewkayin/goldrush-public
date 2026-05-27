#include "match/bot/bot.h"
#include "match/scenario/scenario.h"
#include "match/state/entity_data.h"
#include "match/state/map_gen.h"
#include "match/state/match.h"
#include "render/sprite.h"
#include "state.h"

#ifdef GOLD_DEBUG

#include "core/input.h"
#include "render/ysort.h"
#include "render/render.h"
#include <algorithm>

void editor_state_render(const EditorState* state) {
    // Scenario
    if (state->scenario != NULL) {
        editor_state_render_scenario(state);
    }

    // Entity preview
    if (editor_state_should_render_entity_preview(state)) {
        editor_state_render_entity_preview(state);
    }

    // Hover rect
    if (editor_state_should_render_mouse_hover_rect(state)) {
        editor_state_render_mouse_hover_rect(state);
    }

    // Constant value
    if (editor_state_should_render_constant_cell_value(state)) {
        const ScenarioConstant& constant = state->scenario->constants[state->tool.constants.constant_index];
        Rect rect = (Rect) {
            .x = ((constant.cell.x * TILE_SIZE) - state->camera_offset.x) + CANVAS_RECT.x,
            .y = ((constant.cell.y * TILE_SIZE) - state->camera_offset.y) + CANVAS_RECT.y,
            .w = TILE_SIZE, .h = TILE_SIZE
        };
        render_draw_rect(rect, RENDER_COLOR_WHITE);
    }

    // Rect preview
    if (state->tool.type == EDITOR_TOOL_RECT && state->tool.rect.is_painting) {
        const Rect rect = editor_state_tool_rect_get_rect(state);
        for (int y = rect.y; y < rect.y + rect.h; y++) {
            for (int x = rect.x; x < rect.x + rect.w; x++) {
                const ivec2 cell = ivec2(x, y);
                const ivec2 cell_position = ivec2(CANVAS_RECT.x, CANVAS_RECT.y) + (cell * TILE_SIZE) - state->camera_offset;
                render_sprite_frame(editor_state_get_raw_map_preview_sprite(state, state->tool.rect.value), ivec2(0, 0), cell_position, RENDER_SPRITE_NO_CULL, 0);
            }
        }
    }

    // Paste preview
    if (editor_state_should_render_paste_preview(state)) {
        editor_state_render_paste_preview(state);
    }

    // Squad patrol cell
    if (editor_state_should_render_squad_patrol_cell(state)) {
        const ivec2 patrol_cell = state->scenario->squads[state->tool.squads.squad_index].patrol_cell;
        const Rect rect = (Rect) {
            .x = ((patrol_cell.x * TILE_SIZE) - state->camera_offset.x) + CANVAS_RECT.x,
            .y = ((patrol_cell.y * TILE_SIZE) - state->camera_offset.y) + CANVAS_RECT.y,
            .w = TILE_SIZE, .h = TILE_SIZE
        };
        render_draw_rect(rect, RENDER_COLOR_WHITE);
    }

    // Draw rects surrounding selected entities
    const std::vector<uint32_t> selection = editor_state_get_selected_entities(state);
    for (uint32_t entity_index : selection) {
        const ScenarioEntity& entity = state->scenario->entities[entity_index];
        const int entity_cell_size = entity_get_data(entity.type).cell_size;
        const Rect& entity_rect = (Rect) {
            .x = entity.cell.x, .y = entity.cell.y,
            .w = entity_cell_size, .h = entity_cell_size
        };
        Rect render_rect = editor_state_cell_rect_to_world_space(state, entity_rect);
        render_rect.x += CANVAS_RECT.x;
        render_rect.y += CANVAS_RECT.y;

        if (CANVAS_RECT.intersects(render_rect)) {
            render_draw_rect(render_rect, RENDER_COLOR_WHITE);
        }
    }

    // Entity vision preview
    if (state->tool.type == EDITOR_TOOL_EDIT_ENTITY && !selection.empty() && input_is_action_pressed(INPUT_ACTION_SHIFT)) {
        const uint32_t entity_index = selection[0];
        const ScenarioEntity& entity = state->scenario->entities[entity_index];
        const std::vector<ivec2> vision_cells = editor_state_get_entity_vision_cells(state, entity.cell, entity_get_data(entity.type).cell_size, entity_get_data(entity.type).sight);

        for (const ivec2 cell : vision_cells) {
            const Rect cell_rect = (Rect) {
                .x = cell.x, .y = cell.y,
                .w = 1, .h = 1
            };
            Rect render_rect = editor_state_cell_rect_to_world_space(state, cell_rect);
            render_rect.x += CANVAS_RECT.x;
            render_rect.y += CANVAS_RECT.y;

            if (CANVAS_RECT.intersects(render_rect)) {
                render_draw_rect(render_rect, RENDER_COLOR_WHITE);
            }
        }
    }

    // Select rect
    if (state->tool.type == EDITOR_TOOL_SELECT && editor_state_tool_select_has_select_rect(state)) {
        const Rect rect = editor_state_tool_select_get_select_rect(state);
        Rect rendered_rect = editor_state_cell_rect_to_world_space(state, rect);
        rendered_rect.x += CANVAS_RECT.x;
        rendered_rect.y += CANVAS_RECT.y;
        if (CANVAS_RECT.intersects(rendered_rect)) {
            render_draw_rect(rendered_rect, RENDER_COLOR_WHITE);
        }
    }

    // Player spawn rect
    if (state->tool.type == EDITOR_TOOL_PLAYER_SPAWN) {
        editor_state_render_camera_rect(state, state->scenario->player_spawn);
    }

    // Tool constants camera preview rect
    if (editor_state_should_render_tool_constants_camera_preview_rect(state)) {
        const ScenarioConstant& constant = state->scenario->constants[state->tool.constants.constant_index];
        editor_state_render_camera_rect(state, constant.cell);
    }

    // UI covers
    const Rect SRC_RECT = (Rect) {
        .x = RENDER_COLOR_OFFBLACK, .y = 0, .w = 1, .h = 1
    };
    render_sprite(SPRITE_UI_SWATCH, SRC_RECT, TOOLBAR_RECT, RENDER_SPRITE_NO_CULL);
    render_sprite(SPRITE_UI_SWATCH, SRC_RECT, SIDEBAR_RECT, RENDER_SPRITE_NO_CULL);
    render_sprite(SPRITE_UI_SWATCH, SRC_RECT, STATUS_RECT, RENDER_SPRITE_NO_CULL);

    // UI context
    ui_render(state->ui_context);

    // Minimap frame
    const SpriteInfo& minimap_frame_sprite_info = render_get_sprite_info(SPRITE_UI_MINIMAP);
    const Rect MINIMAP_SRC_RECT = (Rect) {
        .x = 0, .y = 0,
        .w = minimap_frame_sprite_info.frame_width,
        .h = minimap_frame_sprite_info.frame_width
    };
    const Rect MINIMAP_DST_RECT = (Rect) {
        .x = MINIMAP_RECT.x - 4, .y = MINIMAP_RECT.y - 4,
        .w = minimap_frame_sprite_info.frame_width,
        .h = minimap_frame_sprite_info.frame_width
    };
    render_sprite(SPRITE_UI_MINIMAP, MINIMAP_SRC_RECT, MINIMAP_DST_RECT, RENDER_SPRITE_NO_CULL);

    // Minimap
    if (state->scenario != NULL) {
        // Minimap tiles
        for (int y = 0; y < state->map.height; y++) {
            for (int x = 0; x < state->map.width; x++) {
                render_minimap_putpixel(MINIMAP_LAYER_TILE, ivec2(x, y), editor_state_get_minimap_pixel_for_cell(state, ivec2(x, y)));
            }
        }

        // Minimap paste preview
        if (editor_state_should_render_paste_preview(state)) {
            const ivec2 hovered_cell = editor_state_get_hovered_cell(state);

            // Draw preview cells
            for (int y = 0; y < state->clipboard.height; y++) {
                for (int x = 0; x < state->clipboard.width; x++) {
                    uint8_t clipboard_value = state->clipboard.values[x + (y * state->clipboard.width)];
                    SpriteName sprite = editor_state_get_raw_map_preview_sprite(state, clipboard_value);
                    ivec2 cell = hovered_cell + ivec2(x, y);
                    render_minimap_putpixel(MINIMAP_LAYER_TILE, cell, editor_state_get_minimap_pixel_for_tile_sprite(sprite));
                }
            }
        }

        // Minimap entities
        for (uint32_t entity_index = 0; entity_index < state->scenario->entity_count; entity_index++) {
            const ScenarioEntity& entity = state->scenario->entities[entity_index];
            int entity_cell_size = entity_get_data(entity.type).cell_size;
            Rect entity_rect = (Rect) {
                .x = entity.cell.x, .y = entity.cell.y,
                .w = entity_cell_size, .h = entity_cell_size
            };
            render_minimap_fill_rect(MINIMAP_LAYER_TILE, entity_rect, editor_state_get_minimap_pixel_for_entity(state, selection, entity_index));
        }

        // Minimap cell-constant
        if (editor_state_should_render_constant_cell_value(state)) {
            const ScenarioConstant& constant = state->scenario->constants[state->tool.constants.constant_index];
            const Rect rect = (Rect) {
                .x = constant.cell.x,
                .y = constant.cell.y,
                .w = 1,
                .h = 1
            };
            render_minimap_fill_rect(MINIMAP_LAYER_TILE, rect, MINIMAP_PIXEL_WHITE);
        }

        // Clear fog layer
        for (int y = 0; y < state->map.height; y++) {
            for (int x = 0; x < state->map.width; x++) {
                render_minimap_putpixel(MINIMAP_LAYER_FOG, ivec2(x, y), MINIMAP_PIXEL_TRANSPARENT);
            }
        }

        // Player spawn rect
        if (state->tool.type == EDITOR_TOOL_PLAYER_SPAWN) {
            ivec2 camera_offset = editor_state_get_player_spawn_camera_offset(state, state->scenario->player_spawn);
            Rect spawn_rect = (Rect) {
                .x = camera_offset.x / TILE_SIZE,
                .y = camera_offset.y / TILE_SIZE,
                .w = (SCREEN_WIDTH / TILE_SIZE) - 1,
                .h = ((SCREEN_HEIGHT - 86) / TILE_SIZE)
            };
            render_minimap_draw_rect(MINIMAP_LAYER_FOG, spawn_rect, MINIMAP_PIXEL_PLAYER0);
        }

        // Minimap selection rect
        if (state->tool.type == EDITOR_TOOL_SELECT && editor_state_tool_select_has_select_rect(state)) {
            const Rect rect = editor_state_tool_select_get_select_rect(state);
            render_minimap_draw_rect(MINIMAP_LAYER_FOG, rect, MINIMAP_PIXEL_WHITE);
        }

        // Minimap paste preview rect
        if (editor_state_should_render_paste_preview(state)) {
            const ivec2 hovered_cell = editor_state_get_hovered_cell(state);
            const Rect paste_rect = (Rect) {
                .x = hovered_cell.x,
                .y = hovered_cell.y,
                .w = state->clipboard.width,
                .h = state->clipboard.height
            };
            render_minimap_draw_rect(MINIMAP_LAYER_FOG, paste_rect, MINIMAP_PIXEL_WHITE);
        }

        // Minimap camera rect
        Rect camera_rect = (Rect) {
            .x = state->camera_offset.x / TILE_SIZE,
            .y = state->camera_offset.y / TILE_SIZE,
            .w = (SCREEN_WIDTH / TILE_SIZE) - 1,
            .h = (SCREEN_HEIGHT / TILE_SIZE)
        };
        render_minimap_draw_rect(MINIMAP_LAYER_FOG, camera_rect, MINIMAP_PIXEL_WHITE);

        // Final render
        render_minimap_queue_render(ivec2(MINIMAP_RECT.x, MINIMAP_RECT.y), ivec2(state->map.width, state->map.height), ivec2(MINIMAP_RECT.w, MINIMAP_RECT.h));
    }
}

void editor_state_render_scenario(const EditorState* state) {
    std::vector<RenderSpriteParams> ysort_params;

    ivec2 base_pos = ivec2(-(state->camera_offset.x % TILE_SIZE), -(state->camera_offset.y % TILE_SIZE));
    ivec2 base_coords = ivec2(state->camera_offset.x / TILE_SIZE, state->camera_offset.y / TILE_SIZE);
    ivec2 max_visible_tiles = ivec2(CANVAS_RECT.w / TILE_SIZE, CANVAS_RECT.h / TILE_SIZE);
    if (base_pos.x != 0) {
        max_visible_tiles.x++;
    }
    if (base_pos.y != 0) {
        max_visible_tiles.y++;
    }

    // Begin elevation passes
    static const int ELEVATION_COUNT = 2;
    for (uint32_t elevation = 0; elevation < ELEVATION_COUNT; elevation++) {
        // Render map
        for (int y = 0; y < max_visible_tiles.y; y++) {
            for (int x = 0; x < max_visible_tiles.x; x++) {
                if (base_coords.x + x >= state->map.width || base_coords.y + y >= state->map.height) {
                    continue;
                }

                int map_index = (base_coords.x + x) + ((base_coords.y + y) * state->map.width);
                Tile tile = state->map.tiles[map_index];

                ivec2 tile_params_position = ivec2(CANVAS_RECT.x, CANVAS_RECT.y) + base_pos + ivec2(x * TILE_SIZE, y * TILE_SIZE);
                RenderSpriteParams tile_params = (RenderSpriteParams) {
                    .sprite = (SpriteName)tile.sprite,
                    .frame = ivec2((int)tile.frame_x, (int)tile.frame_y),
                    .position = tile_params_position,
                    .ysort_position = tile_params_position.y,
                    .options = RENDER_SPRITE_NO_CULL,
                    .recolor_id = 0
                };

                bool should_render_on_ground_level =
                    map_is_tile_ground(state->map, base_coords + ivec2(x, y)) ||
                    map_is_tile_ramp(state->map, base_coords + ivec2(x, y));
                if (elevation == 0 &&
                        !map_is_tile_ground(state->map, base_coords + ivec2(x, y)) &&
                        !map_is_tile_water(state->map, base_coords + ivec2(x, y))) {
                    render_sprite_frame(map_get_plain_ground_tile_sprite(state->map.type), ivec2(0, 0), tile_params_position, RENDER_SPRITE_NO_CULL, 0);
                }
                if ((should_render_on_ground_level && elevation == 0) ||
                        (!should_render_on_ground_level && elevation == tile.elevation)) {
                    render_sprite_frame(tile_params.sprite, tile_params.frame, tile_params.position, tile_params.options, tile_params.recolor_id);
                }

                // Decorations
                Cell cell = state->map.cells[CELL_LAYER_GROUND][map_index];
                if (cell.type == CELL_DECORATION && tile.elevation == elevation) {
                    SpriteName decoration_sprite = map_get_decoration_sprite(state->map.type);
                    const SpriteInfo& decoration_sprite_info = render_get_sprite_info(decoration_sprite);
                    const int decoration_extra_height = decoration_sprite_info.frame_height - TILE_SIZE;
                    ysort_params.push_back((RenderSpriteParams) {
                        .sprite = decoration_sprite,
                        .frame = ivec2(cell.decoration_hframe, 0),
                        .position = ivec2(tile_params_position.x, tile_params_position.y - decoration_extra_height),
                        .ysort_position = tile_params_position.y,
                        .options = RENDER_SPRITE_NO_CULL,
                        .recolor_id = 0
                    });
                }
            }  // End for each x
        } // End for each y

        // For each cell layer
        for (int cell_layer = CELL_LAYER_UNDERGROUND; cell_layer < CELL_LAYER_GROUND + 1; cell_layer++) {
            // Underground entities
            if (cell_layer == CELL_LAYER_UNDERGROUND) {
                for (uint32_t entity_index = 0; entity_index < state->scenario->entity_count; entity_index++) {
                    const ScenarioEntity& entity = state->scenario->entities[entity_index];
                    const EntityData& entity_data = entity_get_data(entity.type);
                    if (entity_data.cell_layer != CELL_LAYER_UNDERGROUND ||
                            map_get_tile(state->map, entity.cell).elevation != elevation) {
                        continue;
                    }

                    RenderSpriteParams params = editor_state_create_entity_render_params(state, entity);
                    render_sprite_frame(params.sprite, params.frame, params.position, params.options, params.recolor_id);
                }
            }
        } // End for each cell layer
    }

    // Entities
    for (uint32_t entity_index = 0; entity_index < state->scenario->entity_count; entity_index++) {
        // Don't render entity while we are moving it because
        // it will be rendered by the move preview
        if (editor_state_is_dragging_entity(state, entity_index)) {
            continue;
        }

        const ScenarioEntity& entity = state->scenario->entities[entity_index];
        const EntityData& entity_data = entity_get_data(entity.type);
        if (entity_data.cell_layer != CELL_LAYER_GROUND) {
            continue;
        }

        RenderSpriteParams params = editor_state_create_entity_render_params(state, entity);
        const SpriteInfo& sprite_info = render_get_sprite_info(entity_get_data(entity.type).sprite);
        const Rect render_rect = (Rect) {
            .x = params.position.x, .y = params.position.y,
            .w = sprite_info.frame_width, .h = sprite_info.frame_height
        };

        if (!CANVAS_RECT.intersects(render_rect)) {
            continue;
        }
        params.options |= RENDER_SPRITE_NO_CULL;

        ysort_params.push_back(params);
    }

    // Render ysort params
    ysort_render_params(ysort_params, 0, ysort_params.size() - 1);
    for (const RenderSpriteParams& params : ysort_params) {
        render_sprite_frame(params.sprite, params.frame, params.position, params.options, params.recolor_id);
    }

    // Balloon shadows
    for (uint32_t entity_index = 0; entity_index < state->scenario->entity_count; entity_index++) {
        const ScenarioEntity& entity = state->scenario->entities[entity_index];
        if (entity.type != ENTITY_BALLOON) {
            continue;
        }
        const EntityData& entity_data = entity_get_data(entity.type);
        ivec2 balloon_position = (entity.cell * TILE_SIZE) + ((ivec2(entity_data.cell_size, entity_data.cell_size) * TILE_SIZE) / 2);
        render_sprite_frame(SPRITE_UNIT_BALLOON_SHADOW, ivec2(0, 0), ivec2(CANVAS_RECT.x, CANVAS_RECT.y) + balloon_position + ivec2(-5, 3) - state->camera_offset, 0, 0);
    }

    // Sky entities
    ysort_params.clear();
    for (uint32_t entity_index = 0; entity_index < state->scenario->entity_count; entity_index++) {
        // Don't render entity while we are moving it because
        // it will be rendered by the move preview
        if (editor_state_is_dragging_entity(state, entity_index)) {
            continue;
        }

        const ScenarioEntity& entity = state->scenario->entities[entity_index];
        const EntityData& entity_data = entity_get_data(entity.type);
        if (entity_data.cell_layer != CELL_LAYER_SKY) {
            continue;
        }

        RenderSpriteParams params = editor_state_create_entity_render_params(state, entity);
        const SpriteInfo& sprite_info = render_get_sprite_info(entity_get_data(entity.type).sprite);
        const Rect render_rect = (Rect) {
            .x = params.position.x, .y = params.position.y,
            .w = sprite_info.frame_width, .h = sprite_info.frame_height
        };

        if (!CANVAS_RECT.intersects(render_rect)) {
            continue;
        }
        params.options |= RENDER_SPRITE_NO_CULL;

        ysort_params.push_back(params);
    }

    ysort_render_params(ysort_params, 0, ysort_params.size() - 1);
    for (const RenderSpriteParams& params : ysort_params) {
        render_sprite_frame(params.sprite, params.frame, params.position, params.options, params.recolor_id);
    }
}

RenderSpriteParams editor_state_create_entity_render_params(const EditorState* state, const ScenarioEntity& entity) {
    ivec2 params_position = editor_state_get_entity_render_position(state, entity.type, entity.cell);
    RenderSpriteParams params = (RenderSpriteParams) {
        .sprite = entity_get_data(entity.type).sprite,
        .frame = editor_state_get_entity_animation_frame(entity.type),
        .position = params_position,
        .ysort_position = params_position.y,
        .options = 0,
        .recolor_id = entity_is_misc(entity.type) ? 0 : state->scenario->players[entity.player_id].recolor_id
    };

    return params;
}

ivec2 editor_state_get_entity_animation_frame(EntityType type) {
    if (type == ENTITY_GOLDMINE) {
        return ivec2(0, 0);
    }
    if (entity_is_building(type)) {
        return ivec2(3, 0);
    }
    return ivec2(0, 0);
}

ivec2 editor_state_get_entity_render_position(const EditorState* state, EntityType type, ivec2 cell) {
    const EntityData& entity_data = entity_get_data(type);
    ivec2 render_pos = ivec2(CANVAS_RECT.x, CANVAS_RECT.y) + (cell * TILE_SIZE) - state->camera_offset;
    if (entity_is_unit(type)) {
        const SpriteInfo& sprite_info = render_get_sprite_info(entity_data.sprite);
        render_pos += ivec2(entity_data.cell_size, entity_data.cell_size) * (TILE_SIZE / 2);
        render_pos -= ivec2(sprite_info.frame_width / 2, sprite_info.frame_height / 2);
    }
    if (type == ENTITY_BALLOON) {
        render_pos.y += ENTITY_SKY_POSITION_Y_OFFSET;
    }
    if (type == ENTITY_SWITCH) {
        render_pos.y += ENTITY_SWITCH_POSITION_Y_OFFSET;
    }
    return render_pos;
}

MinimapPixel editor_state_get_minimap_pixel_for_tile_sprite(SpriteName sprite) {
    switch (sprite) {
        case SPRITE_TILE_SAND1:
        case SPRITE_TILE_SAND2:
        case SPRITE_TILE_SAND3:
            return MINIMAP_PIXEL_SAND;
        case SPRITE_TILE_SAND_WATER:
        case SPRITE_TILE_GRASS_WATER:
            return MINIMAP_PIXEL_WATER;
        case SPRITE_TILE_GRASS1:
        case SPRITE_TILE_GRASS2:
        case SPRITE_TILE_GRASS3:
        case SPRITE_TILE_GRASS4:
        case SPRITE_TILE_GRASS5:
            return MINIMAP_PIXEL_GRASS;
        case SPRITE_TILE_SNOW1:
        case SPRITE_TILE_SNOW2:
        case SPRITE_TILE_SNOW3:
            return MINIMAP_PIXEL_SNOW;
        case SPRITE_TILE_SNOW_WATER:
            return MINIMAP_PIXEL_SNOW_WATER;
        default:
            return MINIMAP_PIXEL_WALL;
    }
}

MinimapPixel editor_state_get_minimap_pixel_for_cell(const EditorState* state, ivec2 cell) {
    return editor_state_get_minimap_pixel_for_tile_sprite((SpriteName)map_get_tile(state->map, cell).sprite);
}

MinimapPixel editor_state_get_minimap_pixel_for_entity(const EditorState* state, const std::vector<uint32_t>& selection, uint32_t entity_index) {
    for (uint32_t index : selection) {
        if (entity_index == index) {
            return MINIMAP_PIXEL_WHITE;
        }
    }

    const ScenarioEntity& entity = state->scenario->entities[entity_index];
    if (entity_is_misc(entity.type)) {
        return MINIMAP_PIXEL_GOLD;
    }
    return (MinimapPixel)(MINIMAP_PIXEL_PLAYER0 + state->scenario->players[entity.player_id].recolor_id);
}

bool editor_state_should_render_mouse_hover_rect(const EditorState* state) {
    return CANVAS_RECT.has_point(input_get_mouse_position()) &&
        !state->is_minimap_dragging &&
        !editor_state_is_camera_dragging(state) &&
        !editor_state_is_in_menu(state) &&
        !editor_state_is_toolbar_open(state);
}

void editor_state_render_mouse_hover_rect(const EditorState* state) {
    // Determine hover rect cell and cell size
    ivec2 hovered_cell = editor_state_get_hovered_cell(state);
    int cell_size = 1;
    if (state->tool.type == EDITOR_TOOL_CONSTANTS && !state->scenario->constants.empty()) {
        const ScenarioConstant& constant = state->scenario->constants[state->tool.constants.constant_index];
        if ((constant.type == SCENARIO_CONSTANT_TYPE_ENTITY || constant.type == SCENARIO_CONSTANT_TYPE_ENTITY_LIST) && editor_state_is_hovered_cell_valid(state)) {
            uint32_t entity_index = editor_state_get_hovered_entity(state);
            const ScenarioEntity& entity = state->scenario->entities[entity_index];
            hovered_cell = entity.cell;
            cell_size = entity_get_data(entity.type).cell_size;
        }
    }

    // Render the rect
    const Rect rect = (Rect) {
        .x = ((hovered_cell.x * TILE_SIZE) - state->camera_offset.x) + CANVAS_RECT.x,
        .y = ((hovered_cell.y * TILE_SIZE) - state->camera_offset.y) + CANVAS_RECT.y,
        .w = cell_size * TILE_SIZE, .h = cell_size * TILE_SIZE
    };
    const RenderColor rect_color = state->scenario == NULL || editor_state_is_hovered_cell_valid(state)
        ? RENDER_COLOR_WHITE
        : RENDER_COLOR_RED;
    render_draw_rect(rect, rect_color);
}

bool editor_state_should_render_entity_preview(const EditorState* state) {
    if (!editor_state_should_render_mouse_hover_rect(state)) {
        return false;
    }

    return state->tool.type == EDITOR_TOOL_ADD_ENTITY ||
        (state->tool.type == EDITOR_TOOL_EDIT_ENTITY && state->tool.edit_entity.is_dragging);
}

void editor_state_render_entity_preview(const EditorState* state) {
    // Determine preview entity type
    EntityType entity_type = state->tool.type == EDITOR_TOOL_ADD_ENTITY
        ? state->tool.add_entity.entity_type
        : state->scenario->entities[state->tool.edit_entity.entity_index].type;
    const EntityData& entity_data = entity_get_data(entity_type);

    // Determine cell
    ivec2 hovered_cell = editor_state_get_hovered_cell(state);
    if (state->tool.type == EDITOR_TOOL_EDIT_ENTITY) {
        hovered_cell -= state->tool.edit_entity.drag_offset;
    }

    // Determine preview position
    ivec2 entity_position = editor_state_get_entity_render_position(state, entity_type, hovered_cell);

    // Determine preview recolor ID
    uint8_t recolor_id;
    if (entity_is_misc(entity_type)) {
        recolor_id = 0;
    } else if (state->tool.type == EDITOR_TOOL_ADD_ENTITY) {
        recolor_id = state->scenario->players[state->tool.add_entity.player_id].recolor_id;
    } else {
        recolor_id = state->scenario->players[state->scenario->entities[state->tool.edit_entity.entity_index].player_id].recolor_id;
    }

    render_sprite_frame(entity_data.sprite, editor_state_get_entity_animation_frame(entity_type), entity_position, RENDER_SPRITE_NO_CULL, recolor_id);

    // Render blocked rects for goldmines
    for (uint32_t entity_index = 0; entity_index < state->scenario->entity_count; entity_index++) {
        const ScenarioEntity& entity = state->scenario->entities[entity_index];
        if (entity.type != ENTITY_GOLDMINE) {
            continue;
        }

        Rect block_rect = entity_goldmine_get_block_building_rect(entity.cell);
        Rect render_rect = editor_state_cell_rect_to_world_space(state, block_rect);
        render_rect.x += CANVAS_RECT.x;
        render_rect.y += CANVAS_RECT.y;

        if (CANVAS_RECT.intersects(render_rect)) {
            render_draw_rect(render_rect, RENDER_COLOR_GOLD);
        }
    }
}

bool editor_state_should_render_constant_cell_value(const EditorState* state) {
    if (state->tool.type != EDITOR_TOOL_CONSTANTS || state->scenario->constants.empty()) {
        return false;
    }

    const ScenarioConstant& constant = state->scenario->constants[state->tool.constants.constant_index];
    return constant.type == SCENARIO_CONSTANT_TYPE_CELL && constant.cell.x != -1;
}

bool editor_state_should_render_paste_preview(const EditorState* state) {
    return state->tool.type == EDITOR_TOOL_SELECT &&
        state->tool.select.is_pasting &&
        CANVAS_RECT.has_point(input_get_mouse_position());
}

void editor_state_render_paste_preview(const EditorState* state) {
    const ivec2 hovered_cell = editor_state_get_hovered_cell(state);

    // Draw preview cells
    for (int y = 0; y < state->clipboard.height; y++) {
        for (int x = 0; x < state->clipboard.width; x++) {
            uint8_t clipboard_value = state->clipboard.values[x + (y * state->clipboard.width)];
            SpriteName sprite = editor_state_get_raw_map_preview_sprite(state, clipboard_value);
            ivec2 cell = hovered_cell + ivec2(x, y);
            ivec2 cell_position = ivec2(CANVAS_RECT.x, CANVAS_RECT.y) + (cell * TILE_SIZE) - state->camera_offset;
            render_sprite_frame(sprite, ivec2(0, 0), cell_position, 0, 0);
        }
    }

    // Draw a rect around the paste area
    const Rect paste_rect = (Rect) {
        .x = hovered_cell.x,
        .y = hovered_cell.y,
        .w = state->clipboard.width,
        .h = state->clipboard.height
    };
    Rect rendered_rect = editor_state_cell_rect_to_world_space(state, paste_rect);
    rendered_rect.x += CANVAS_RECT.x;
    rendered_rect.y += CANVAS_RECT.y;
    render_draw_rect(rendered_rect, RENDER_COLOR_WHITE);
}

SpriteName editor_state_get_raw_map_preview_sprite(const EditorState* state, uint8_t value) {
    switch (value) {
        case MAP_VALUE_WATER:
            return map_choose_water_tile_sprite(state->map.type);
        case MAP_VALUE_LOWGROUND:
        case MAP_VALUE_DECORATION_LOWGROUND:
        case MAP_VALUE_DECORATION_HIGHGROUND:
            return map_get_plain_ground_tile_sprite(state->map.type);
        case MAP_VALUE_HIGHGROUND:
            return SPRITE_TILE_WALL_SOUTH_EDGE;
        case MAP_VALUE_STAIR:
            return SPRITE_TILE_WALL_SOUTH_STAIR_CENTER;
        default:
            GOLD_ASSERT(false);
            return SPRITE_TILE_NULL;
    }
}

bool editor_state_should_render_squad_patrol_cell(const EditorState* state) {
    if (state->tool.type != EDITOR_TOOL_SQUADS || state->scenario->squads.empty()) {
        return false;
    }

    const ScenarioSquad& squad = state->scenario->squads[state->tool.squads.squad_index];
    return squad.type == BOT_SQUAD_TYPE_PATROL && squad.patrol_cell.x != -1;
}

std::vector<ivec2> editor_state_get_entity_vision_cells(const EditorState* state, ivec2 cell, int cell_size, int sight) {
    std::vector<ivec2> vision_cells;

    ivec2 search_corners[4] = {
        cell - ivec2(sight, sight),
        cell + ivec2((cell_size - 1) + sight, -sight),
        cell + ivec2((cell_size - 1) + sight, (cell_size - 1) + sight),
        cell + ivec2(-sight, (cell_size - 1) + sight)
    };
    for (int search_index = 0; search_index < 4; search_index++) {
        ivec2 search_goal = search_corners[search_index + 1 == 4 ? 0 : search_index + 1];
        ivec2 search_step = DIRECTION_IVEC2[(search_index * 2) + 2 == DIRECTION_COUNT
                                            ? DIRECTION_NORTH
                                            : (search_index * 2) + 2];
        for (ivec2 line_end = search_corners[search_index]; line_end != search_goal; line_end += search_step) {
            ivec2 line_start;
            switch (cell_size) {
                case 1:
                    line_start = cell;
                    break;
                case 3:
                    line_start = cell + ivec2(1, 1);
                    break;
                case 2:
                case 4: {
                    ivec2 center_cell = cell_size == 2 ? cell : cell + ivec2(1, 1);
                    if (line_end.x < center_cell.x) {
                        line_start.x = center_cell.x;
                    } else if (line_end.x > center_cell.x + 1) {
                        line_start.x = center_cell.x + 1;
                    } else {
                        line_start.x = line_end.x;
                    }
                    if (line_end.y < center_cell.y) {
                        line_start.y = center_cell.y;
                    } else if (line_end.y > center_cell.y + 1) {
                        line_start.y = center_cell.y + 1;
                    } else {
                        line_start.y = line_end.y;
                    }
                    break;
                }
                default:
                    log_warn("cell size of %i not handled in map_fog_update", cell_size);
                    line_start = cell;
                    break;
            }

            // we want slope to be between 0 and 1
            // if "run" is greater than "rise" then m is naturally between 0 and 1, we will step with x in increments of 1 and handle y increments that are less than 1
            // if "rise" is greater than "run" (use_x_step is false) then we will swap x and y so that we can step with y in increments of 1 and handle x increments that are less than 1
            bool use_x_step = std::abs(line_end.x - line_start.x) >= std::abs(line_end.y - line_start.y);
            int slope = std::abs(2 * (use_x_step ? (line_end.y - line_start.y) : (line_end.x - line_start.x)));
            int slope_error = slope - std::abs((use_x_step ? (line_end.x - line_start.x) : (line_end.y - line_start.y)));
            ivec2 line_step;
            ivec2 line_opposite_step;
            if (use_x_step) {
                line_step = ivec2(1, 0) * (line_end.x >= line_start.x ? 1 : -1);
                line_opposite_step = ivec2(0, 1) * (line_end.y >= line_start.y ? 1 : -1);
            } else {
                line_step = ivec2(0, 1) * (line_end.y >= line_start.y ? 1 : -1);
                line_opposite_step = ivec2(1, 0) * (line_end.x >= line_start.x ? 1 : -1);
            }
            for (ivec2 line_cell = line_start; line_cell != line_end; line_cell += line_step) {
                if (!map_is_cell_in_bounds(state->map, line_cell) || ivec2::euclidean_distance_squared(line_start, line_cell) > sight * sight) {
                    break;
                }

                vision_cells.push_back(line_cell);

                slope_error += slope;
                if (slope_error >= 0) {
                    line_cell += line_opposite_step;
                    slope_error -= 2 * std::abs((use_x_step ? (line_end.x - line_start.x) : (line_end.y - line_start.y)));
                }
            } // End for each line cell in line
        } // End for each line end from corner to corner
    } // End for each search index

    return vision_cells;
}

void editor_state_render_camera_rect(const EditorState* state, ivec2 camera_cell) {
    ivec2 camera_offset = editor_state_get_player_spawn_camera_offset(state, camera_cell);
    const Rect rendered_rect = (Rect) {
        .x = camera_offset.x + CANVAS_RECT.x - state->camera_offset.x,
        .y = camera_offset.y + CANVAS_RECT.y - state->camera_offset.y,
        .w = SCREEN_WIDTH,
        .h = SCREEN_HEIGHT - 86
    };
    if (CANVAS_RECT.intersects(rendered_rect)) {
        render_draw_rect(rendered_rect, RENDER_COLOR_PLAYER_UI0);
    }
}

ivec2 editor_state_get_player_spawn_camera_offset(const EditorState* state, ivec2 cell) {
    ivec2 camera_offset = ivec2(
        (cell.x * TILE_SIZE) + (TILE_SIZE / 2) - (SCREEN_WIDTH / 2),
        (cell.y * TILE_SIZE) + (TILE_SIZE / 2) - ((SCREEN_HEIGHT - MATCH_SHELL_UI_HEIGHT) / 2));
    camera_offset.x = std::clamp(camera_offset.x, 0, (state->map.width * TILE_SIZE) - SCREEN_WIDTH);
    camera_offset.y = std::clamp(camera_offset.y, 0, (state->map.height * TILE_SIZE) - SCREEN_HEIGHT + MATCH_SHELL_UI_HEIGHT);

    return camera_offset;
}

bool editor_state_should_render_tool_constants_camera_preview_rect(const EditorState* state) {
    return state->tool.type == EDITOR_TOOL_CONSTANTS &&
            !state->scenario->constants.empty() &&
            state->scenario->constants[state->tool.constants.constant_index].type == SCENARIO_CONSTANT_TYPE_CELL &&
            input_is_action_pressed(INPUT_ACTION_SHIFT);
}

#endif
