#include "match/state/entity_data.h"
#include "match/state/match.h"
#include "shell.h"

#include "render/sprite.h"
#include "render/render.h"
#include "render/ysort.h"
#include "network/network.h"
#include "match/state/upgrade.h"
#include "profile/profile.h"

void match_shell_render(const MatchShell* shell) {
    ZoneScoped;

    std::vector<RenderSpriteParams> above_fog_sprite_params;
    std::vector<RenderSpriteParams> ysort_params;

    const ivec2 base_pos = ivec2(-(shell->camera_offset.x % TILE_SIZE), -(shell->camera_offset.y % TILE_SIZE));
    const ivec2 base_coords = ivec2(shell->camera_offset.x / TILE_SIZE, shell->camera_offset.y / TILE_SIZE);
    ivec2 max_visible_tiles = ivec2(SCREEN_WIDTH / TILE_SIZE, (SCREEN_HEIGHT - MATCH_SHELL_UI_HEIGHT) / TILE_SIZE);
    if (base_pos.x != 0) {
        max_visible_tiles.x++;
    }
    if (base_pos.y != 0) {
        max_visible_tiles.y++;
    }

    // Elevation passes
    {
        ZoneScopedN("elevation passes");

        // Begin elevation passes
        static const int ELEVATION_COUNT = 2;
        for (uint32_t elevation = 0; elevation < ELEVATION_COUNT; elevation++) {
            // Render map
            for (int y = 0; y < max_visible_tiles.y; y++) {
                for (int x = 0; x < max_visible_tiles.x; x++) {
                    int map_index = (base_coords.x + x) + ((base_coords.y + y) * shell->match_state.map.width);
                    Tile tile = shell->match_state.map.tiles[map_index];

                    ivec2 tile_params_position = base_pos + ivec2(x * TILE_SIZE, y * TILE_SIZE);
                    RenderSpriteParams tile_params = (RenderSpriteParams) {
                        .sprite = (SpriteName)tile.sprite,
                        .frame = ivec2((int)tile.frame_x, (int)tile.frame_y),
                        .position = tile_params_position,
                        .ysort_position = tile_params_position.y,
                        .options = RENDER_SPRITE_NO_CULL,
                        .recolor_id = 0
                    };

                    bool should_render_on_ground_level =
                        map_is_tile_ground(shell->match_state.map, base_coords + ivec2(x, y)) ||
                        map_is_tile_ramp(shell->match_state.map, base_coords + ivec2(x, y));
                    if (elevation == 0 &&
                            !map_is_tile_ground(shell->match_state.map, base_coords + ivec2(x, y)) &&
                            !map_is_tile_water(shell->match_state.map, base_coords + ivec2(x, y))) {
                        render_sprite_frame(map_get_plain_ground_tile_sprite(shell->match_state.map.type), ivec2(0, 0), base_pos + ivec2(x * TILE_SIZE, y * TILE_SIZE), RENDER_SPRITE_NO_CULL, 0);
                    }
                    if ((should_render_on_ground_level && elevation == 0) ||
                            (!should_render_on_ground_level && elevation == tile.elevation)) {
                        render_sprite_frame(tile_params.sprite, tile_params.frame, tile_params.position, tile_params.options, tile_params.recolor_id);
                    }

                    // Decorations
                    Cell cell = shell->match_state.map.cells[CELL_LAYER_GROUND][map_index];
                    if (cell.type == CELL_DECORATION && tile.elevation == elevation) {
                        SpriteName decoration_sprite = map_get_decoration_sprite(shell->match_state.map.type);
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

                    match_shell_debug_render_cell_region_lines(shell, base_coords, base_pos, elevation, ivec2(x, y));
                }  // End for each x
            } // End for each y

            // For each cell layer
            for (int cell_layer = CELL_LAYER_UNDERGROUND; cell_layer < CELL_LAYER_GROUND + 1; cell_layer++) {
                // Dead entities
                for (uint32_t entity_index = 0; entity_index < shell->match_state.entities.size(); entity_index++) {
                    const Entity& entity = shell->match_state.entities[entity_index];
                    const EntityData& entity_data = entity_get_data(entity.type);
                    if (!(entity.mode == MODE_UNIT_DEATH_FADE || entity.mode == MODE_BUILDING_DESTROYED)) {
                        continue;
                    }
                    if (entity_data.cell_layer != cell_layer ||
                            entity_get_elevation(entity, shell->match_state.map) != elevation) {
                        continue;
                    }
                    if (!match_shell_is_entity_visible(shell, entity)) {
                        continue;
                    }

                    RenderSpriteParams params = match_shell_create_entity_render_params(shell, entity);
                    render_sprite_frame(params.sprite, params.frame, params.position, params.options, params.recolor_id);
                }

                // Select rings and healthbars
                for (EntityId id : shell->selection) {
                    const Entity& entity = shell->match_state.entities.get_by_id(id);
                    const EntityData& entity_data = entity_get_data(entity.type);
                    if (entity_data.cell_layer != cell_layer ||
                            entity_get_elevation(entity, shell->match_state.map) != elevation) {
                        continue;
                    }
                    if (entity_is_in_mine(shell->match_state, entity)) {
                        continue;
                    }
                    match_shell_render_entity_select_rings_and_healthbars(shell, entity);
                }

                // Move animation
                if (animation_is_playing(shell->move_animation) &&
                        map_get_tile(shell->match_state.map, shell->move_animation_position / TILE_SIZE).elevation == elevation) {
                    if (shell->move_animation.name == ANIMATION_UI_MOVE_CELL && cell_layer == CELL_LAYER_GROUND) {
                        ivec2 params_position = shell->move_animation_position - shell->camera_offset;
                        RenderSpriteParams params = (RenderSpriteParams) {
                            .sprite = SPRITE_UI_MOVE,
                            .frame = shell->move_animation.frame,
                            .position = params_position,
                            .ysort_position = params_position.y,
                            .options = RENDER_SPRITE_CENTERED,
                            .recolor_id = 0
                        };
                        ivec2 ui_move_cell = shell->move_animation_position / TILE_SIZE;
                        if (match_get_fog(shell->match_state, shell->match_state.players[network_get_player_id()].team, ui_move_cell) > 0) {
                            render_sprite_frame(params.sprite, params.frame, params.position, params.options, params.recolor_id);
                        } else {
                            above_fog_sprite_params.push_back(params);
                        }
                    } else if (shell->move_animation.frame.x % 2 == 0) {
                        uint32_t entity_index = shell->match_state.entities.get_index_of(shell->move_animation_entity_id);
                        if (entity_index != INDEX_INVALID) {
                            const Entity& entity = shell->match_state.entities[entity_index];
                            if (entity_get_data(entity.type).cell_layer == cell_layer) {
                                match_shell_render_entity_move_animation(shell, entity, shell->move_animation);
                            }
                        } else if (cell_layer == CELL_LAYER_GROUND) {
                            uint8_t player_team = shell->match_state.players[network_get_player_id()].team;
                            uint32_t remembered_entity_index = match_team_find_remembered_entity_index(shell->match_state, player_team, shell->move_animation_entity_id);
                            if (remembered_entity_index != MATCH_ENTITY_NOT_REMEMBERED) {
                                const RememberedEntity& remembered_entity = shell->match_state.remembered_entities[player_team][remembered_entity_index];
                                const int cell_size = entity_get_data(remembered_entity.type).cell_size;
                                ivec2 entity_center_position = (remembered_entity.cell * TILE_SIZE) + ((ivec2(cell_size, cell_size) * TILE_SIZE) / 2);

                                render_sprite_frame(match_shell_get_entity_select_ring(remembered_entity.type, shell->move_animation.name == ANIMATION_UI_MOVE_ATTACK_ENTITY), ivec2(0, 0), entity_center_position, RENDER_SPRITE_CENTERED, 0);
                            }
                        }
                    }
                }

                // Highlight animation
                for (const EntityHighlight& entity_highlight : shell->entity_highlights) {
                    if (!animation_is_playing(entity_highlight.animation) || entity_highlight.animation.frame.x % 2 != 0) {
                        continue;
                    }

                    const uint32_t entity_index = shell->match_state.entities.get_index_of(entity_highlight.entity_id);
                    if (entity_index == INDEX_INVALID || !entity_is_selectable(shell->match_state.entities[entity_index])) {
                        continue;
                    }

                    const Entity& entity = shell->match_state.entities[entity_index];
                    if (entity_get_data(entity.type).cell_layer == cell_layer) {
                        match_shell_render_entity_move_animation(shell, entity, entity_highlight.animation);
                    }
                }

                // Underground entities
                if (cell_layer == CELL_LAYER_UNDERGROUND) {
                    for (uint32_t entity_index = 0; entity_index < shell->match_state.entities.size(); entity_index++) {
                        const Entity& entity = shell->match_state.entities[entity_index];
                        const EntityData& entity_data = entity_get_data(entity.type);
                        if (entity_data.cell_layer != CELL_LAYER_UNDERGROUND ||
                                entity_get_elevation(entity, shell->match_state.map) != elevation) {
                            continue;
                        }
                        if (entity.mode == MODE_UNIT_DEATH_FADE || entity.mode == MODE_BUILDING_DESTROYED) {
                            continue;
                        }
                        if (!match_shell_is_entity_visible(shell, entity)) {
                            continue;
                        }

                        RenderSpriteParams params = match_shell_create_entity_render_params(shell, entity);
                        render_sprite_frame(params.sprite, params.frame, params.position, params.options, params.recolor_id);
                    }
                }
            } // End for each cell layer
        } // End for each elevation
    }

    // Fires
    for (uint32_t fire_index = 0; fire_index < shell->match_state.fires.size(); fire_index++) {
        const Fire& fire = shell->match_state.fires[fire_index];
        if (match_shell_get_fire_cell_render(shell, fire) == FIRE_CELL_RENDER_BELOW) {
            render_sprite_frame(SPRITE_PARTICLE_FIRE, fire.animation.frame, (fire.cell * TILE_SIZE) - shell->camera_offset, 0, 0);
        }
    }

    // Entities
    {
        ZoneScopedN("entities");

        for (uint32_t entity_index = 0; entity_index < shell->match_state.entities.size(); entity_index++) {
            const Entity& entity = shell->match_state.entities[entity_index];
            const EntityData& entity_data = entity_get_data(entity.type);
            if (entity.mode == MODE_UNIT_DEATH_FADE ||
                    entity.mode == MODE_BUILDING_DESTROYED ||
                    entity_data.cell_layer != CELL_LAYER_GROUND) {
                continue;
            }
            if (!match_shell_is_entity_visible(shell, entity)) {
                continue;
            }

            RenderSpriteParams params = match_shell_create_entity_render_params(shell, entity);
            const SpriteInfo& sprite_info = render_get_sprite_info(entity_get_sprite(shell->match_state, entity));
            Rect render_rect = (Rect) {
                .x = params.position.x, .y = params.position.y,
                .w = sprite_info.frame_width, .h = sprite_info.frame_height
            };

            if (!render_rect.intersects(SCREEN_RECT)) {
                continue;
            }
            params.options |= RENDER_SPRITE_NO_CULL;

            ysort_params.push_back(params);
        }

        // Remembered entities
        for (uint8_t team = 0; team < MAX_PLAYERS; team++) {
            if (!match_shell_should_render_remembered_entities_for_team(shell, team)) {
                continue;
            }

            for (uint32_t remembered_entity_index = 0; remembered_entity_index < shell->match_state.remembered_entities[team].size(); remembered_entity_index++) {
                const RememberedEntity& remembered_entity = shell->match_state.remembered_entities[team][remembered_entity_index];
                const EntityData& entity_data = entity_get_data(remembered_entity.type);
                // Don't draw the remembered entity if we can see it, otherwise we will double draw them
                if (match_shell_is_cell_rect_revealed(shell, remembered_entity.cell, entity_data.cell_size)) {
                    continue;
                }

                const SpriteInfo& sprite_info = render_get_sprite_info(entity_data.sprite);

                ivec2 params_position = (remembered_entity.cell * TILE_SIZE) - shell->camera_offset;
                RenderSpriteParams params = (RenderSpriteParams) {
                    .sprite = entity_data.sprite,
                    .frame = remembered_entity.frame,
                    .position = params_position,
                    .ysort_position = params_position.y,
                    .options = RENDER_SPRITE_NO_CULL,
                    .recolor_id = remembered_entity.recolor_id
                };

                Rect render_rect = (Rect) {
                    .x = params.position.x, .y = params.position.y,
                    .w = sprite_info.frame_width * 2, .h = sprite_info.frame_height * 2
                };
                if (!render_rect.intersects(SCREEN_RECT)) {
                    continue;
                }

                ysort_params.push_back(params);
            }
        }
    }

    // Define rally flag enqueue function
    const std::function<void(ivec2, uint8_t)> queue_render_rally_flag = [shell, &ysort_params, &above_fog_sprite_params](ivec2 rally_point, uint8_t player_id) {
        const ivec2 RALLY_FLAG_OFFSET = ivec2(-4, -15);
        ivec2 params_position = rally_point + RALLY_FLAG_OFFSET - shell->camera_offset;
        RenderSpriteParams params = (RenderSpriteParams) {
            .sprite = SPRITE_RALLY_FLAG,
            .frame = shell->rally_flag_animation.frame,
            .position = params_position,
            .ysort_position = params_position.y,
            .options = 0,
            .recolor_id = shell->match_state.players[player_id].recolor_id
        };

        ivec2 rally_cell = rally_point / TILE_SIZE;
        if (match_shell_get_fog(shell, rally_cell) > 0) {
            ysort_params.push_back(params);
        } else {
            above_fog_sprite_params.push_back(params);
        }
    };

    // Rally points
    uint32_t selection_type = match_shell_get_selection_type(shell, shell->selection);
    if (selection_type == MATCH_SHELL_SELECTION_BUILDINGS || (shell->replay_mode && selection_type == MATCH_SHELL_SELECTION_ENEMY_BUILDING)) {
        for (EntityId id : shell->selection) {
            const Entity& building = shell->match_state.entities.get_by_id(id);
            if (building.mode == MODE_BUILDING_DESTROYED || building.rally_point.x == -1) {
                continue;
            }

            queue_render_rally_flag(building.rally_point, building.player_id);
        }
    }

    // Queued entity target rally points
    if (shell->selection.size() == 1) {
        ZoneScopedN("queued entity targets");

        const Entity& entity = shell->match_state.entities.get_by_id(shell->selection[0]);
        // Check if it's an allied unit
        if (entity_is_unit(entity.type) &&
                (shell->replay_mode || entity.player_id == network_get_player_id())) {
            if (entity.target_queue_index != ENTITY_TARGET_QUEUE_INDEX_NONE) {
                // Render flag for entity's current target
                ivec2 target_position = match_shell_get_queued_target_position(shell, entity.target);
                if (target_position.x != -1) {
                    queue_render_rally_flag(target_position, entity.player_id);
                }

                // Render flag for queued targets
                const TargetQueue* entity_target_queue = shell->match_state.entity_target_queues.get(entity.target_queue_index);
                for (uint32_t target_queue_index = 0; target_queue_index < entity_target_queue->size(); target_queue_index++) {
                    const Target& target = (*entity_target_queue)[target_queue_index];
                    ivec2 target_queue_target_position = match_shell_get_queued_target_position(shell, target);
                    if (target_queue_target_position.x != -1) {
                        queue_render_rally_flag(target_queue_target_position, entity.player_id);
                    }
                }
            }
        }
    }

    // Avalanche
    for (const AvalancheColumn& avalanche : shell->scenario_avalanche_columns) {
        const int AVALANCHE_ROW_COUNT = 3U;
        for (int row = 0; row < AVALANCHE_ROW_COUNT; row++) {
            ysort_params.push_back((RenderSpriteParams) {
                .sprite = SPRITE_PARTICLE_AVALANCHE,
                .frame = avalanche.animation.frame,
                .position = avalanche.position + ivec2(0, -20 * row) - shell->camera_offset,
                .ysort_position = avalanche.position.y - (20 * row) - shell->camera_offset.y,
                .options = RENDER_SPRITE_CENTERED | RENDER_SPRITE_NO_CULL
            });
        }
    }

    // Render ysort params
    {
        ZoneScopedN("render ysort params");

        ysort_render_params(ysort_params, 0, ysort_params.size() - 1);
        for (const RenderSpriteParams& params : ysort_params) {
            render_sprite_frame(params.sprite, params.frame, params.position, params.options, params.recolor_id);
        }
    }

    // Balloon shadows
    for (uint32_t entity_index = 0; entity_index < shell->match_state.entities.size(); entity_index++) {
        const Entity& entity = shell->match_state.entities[entity_index];
        if (entity.type == ENTITY_BALLOON && entity.mode != MODE_UNIT_DEATH_FADE &&
                match_shell_is_entity_visible(shell, entity)) {
            render_sprite_frame(SPRITE_UNIT_BALLOON_SHADOW, ivec2(0, 0), entity.position.to_ivec2() + ivec2(-5, 3) - shell->camera_offset, 0, 0);
        }
    }

    // Building fires
    for (uint32_t entity_index = 0; entity_index < shell->match_state.entities.size(); entity_index++) {
        const Entity& entity = shell->match_state.entities[entity_index];
        if (entity.mode == MODE_UNIT_DEATH_FADE || entity.mode == MODE_BUILDING_DESTROYED) {
            continue;
        }
        if (!entity_is_building(entity.type) || !entity_check_flag(entity, ENTITY_FLAG_ON_FIRE)) {
            continue;
        }
        if (!match_shell_is_entity_visible(shell, entity)) {
            continue;
        }

        const EntityData& entity_data = entity_get_data(entity.type);
        for (int x = entity.cell.x; x < entity.cell.x + entity_data.cell_size; x++) {
            ivec2 fire_position = (ivec2(x, entity.cell.y + entity_data.cell_size - 1) * TILE_SIZE) - shell->camera_offset;
            render_sprite_frame(SPRITE_PARTICLE_FIRE, shell->building_fire_animation.frame, fire_position, 0, 0);
        }
    }

    // Fires above units
    for (uint32_t fire_index = 0; fire_index < shell->match_state.fires.size(); fire_index++) {
        const Fire& fire = shell->match_state.fires[fire_index];
        if (match_shell_get_fire_cell_render(shell, fire) == FIRE_CELL_RENDER_ABOVE) {
            render_sprite_frame(SPRITE_PARTICLE_FIRE, fire.animation.frame, (fire.cell * TILE_SIZE) - shell->camera_offset, 0, 0);
        }
    }

    // Smith and workshop smoke animations
    for (uint32_t entity_index = 0; entity_index < shell->match_state.entities.size(); entity_index++) {
        const Entity& entity = shell->match_state.entities[entity_index];
        if (!match_shell_is_entity_visible(shell, entity)) {
            continue;
        }
        if (entity.animation.name == ANIMATION_WORKSHOP) {
            // Haha this is awful
            int hframe = -1;
            switch (entity.animation.frame.x) {
                case 5:
                    hframe = 0;
                    break;
                case 6:
                case 8:
                case 10:
                    hframe = 1;
                    break;
                case 7:
                case 9:
                case 11:
                    hframe = 2;
                    break;
                case 12:
                    hframe = 3;
                    break;
                case 13:
                    hframe = 4;
                    break;
                default:
                    break;
            }
            if (hframe != -1) {
                Rect building_rect = entity_get_rect(entity);
                ivec2 steam_position = ivec2(building_rect.x, building_rect.y) + (ivec2(building_rect.w, building_rect.h) / 2) - shell->camera_offset;
                render_sprite_frame(SPRITE_WORKSHOP_STEAM, ivec2(hframe, 0), steam_position, RENDER_SPRITE_CENTERED, 0);
            }
        } else if (entity.type == ENTITY_SMITH && entity.animation.name != ANIMATION_UNIT_IDLE) {
            render_sprite_frame(SPRITE_BUILDING_SMITH_ANIMATION, entity.animation.frame, entity.position.to_ivec2() - ivec2(0, 16) - shell->camera_offset, 0, 0);
        }
    }

    // Ground particles
    for (uint32_t particle_index = 0; particle_index < shell->match_state.particles.size(); particle_index++) {
        const Particle& particle = shell->match_state.particles[particle_index];
        if (particle.layer != PARTICLE_LAYER_GROUND) {
            continue;
        }
        match_shell_render_particle(shell, particle);
    }

    // Projectiles
    for (uint32_t projectile_index = 0; projectile_index < shell->match_state.projectiles.size(); projectile_index++) {
        const Projectile& projectile = shell->match_state.projectiles[projectile_index];
        if (!match_shell_is_cell_rect_revealed(shell, projectile.position.to_ivec2() / TILE_SIZE, 1)) {
            continue;
        }
        uint32_t options = RENDER_SPRITE_CENTERED;
        if (projectile.position.x > projectile.target.x) {
            options |= RENDER_SPRITE_FLIP_H;
        }
        render_sprite_frame(SPRITE_PROJECTILE_MOLOTOV, ivec2(0, 0), projectile.position.to_ivec2() - shell->camera_offset, options, 0);
    }

    // Miners on gold counter
    for (uint32_t entity_index = 0; entity_index < shell->match_state.entities.size(); entity_index++) {
        const Entity& entity = shell->match_state.entities[entity_index];
        // Make sure the entity is actually a goldmine
        if (entity.type != ENTITY_GOLDMINE || entity.mode != MODE_GOLDMINE) {
            continue;
        }
        // Make sure the player can see the goldmine
        if (!match_shell_is_cell_rect_revealed(shell, entity.cell, entity_get_data(entity.type).cell_size)) {
            continue;
        }
        // Make sure the goldmine is actually on screen
        Rect entity_rect = entity_get_rect(entity);
        entity_rect.x -= shell->camera_offset.x;
        entity_rect.y -= shell->camera_offset.y;
        if (!SCREEN_RECT.intersects(entity_rect)) {
            continue;
        }

        const SpriteInfo& miner_icon_info = render_get_sprite_info(SPRITE_UI_MINER_ICON);
        int icon_y_offset = 0;
        for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
            if (!shell->replay_mode && player_id != network_get_player_id()) {
                continue;
            }

            // Count how many miners are mining from this mine
            EntityId entity_id = shell->match_state.entities.get_id_of(entity_index);
            if (player_id == PLAYER_NONE) {
                continue;
            }
            uint32_t miner_count = match_get_miners_on_gold(shell->match_state, entity_id, player_id);
            if (miner_count == 0) {
                continue;
            }

            char counter_text[4];
            sprintf(counter_text, "%u", miner_count);
            ivec2 text_size = render_get_text_size(FONT_HACK_WHITE, counter_text);
            text_size.x += miner_icon_info.frame_width;
            ivec2 text_pos = ivec2(entity_rect.x + (entity_rect.w / 2) - (text_size.x / 2), entity_rect.y + 6) + ivec2(0, icon_y_offset);
            render_text(miner_count > MATCH_MAX_MINERS_ON_GOLD ? FONT_HACK_PLAYER1 : FONT_HACK_WHITE, counter_text, text_pos + ivec2(miner_icon_info.frame_width + 2, 0));
            render_sprite_frame(SPRITE_UI_MINER_ICON, ivec2(0, 0), text_pos, RENDER_SPRITE_NO_CULL, shell->match_state.players[player_id].recolor_id);
            icon_y_offset -= miner_icon_info.frame_height + 1;
        }
    }

    // Sky
    {
        ZoneScopedN("sky");
        // Sky entities select rings
        for (EntityId entity_id : shell->selection) {
            const Entity& entity = shell->match_state.entities.get_by_id(entity_id);
            const EntityData& entity_data = entity_get_data(entity.type);
            if (entity_data.cell_layer != CELL_LAYER_SKY) {
                continue;
            }
            match_shell_render_entity_select_rings_and_healthbars(shell, entity);
        }

        // Sky entity move animation
        if (animation_is_playing(shell->move_animation) &&
                shell->move_animation.name != ANIMATION_UI_MOVE_CELL &&
                shell->move_animation.frame.x % 2 == 0) {
            uint32_t entity_index = shell->match_state.entities.get_index_of(shell->move_animation_entity_id);
            if (entity_index != INDEX_INVALID) {
                const Entity& entity = shell->match_state.entities[entity_index];
                if (entity_get_data(entity.type).cell_layer == CELL_LAYER_SKY) {
                    match_shell_render_entity_move_animation(shell, entity, shell->move_animation);
                }
            }
        }

        // Sky entities
        ysort_params.clear();
        for (uint32_t entity_index = 0; entity_index < shell->match_state.entities.size(); entity_index++) {
            const Entity& entity = shell->match_state.entities[entity_index];
            const EntityData& entity_data = entity_get_data(entity.type);
            if (entity.mode == MODE_UNIT_DEATH_FADE ||
                    entity.mode == MODE_BUILDING_DESTROYED ||
                    entity_data.cell_layer != CELL_LAYER_SKY) {
                continue;
            }
            if (!match_shell_is_entity_visible(shell, entity)) {
                continue;
            }

            RenderSpriteParams params = match_shell_create_entity_render_params(shell, entity);
            const SpriteInfo& sprite_info = render_get_sprite_info(entity_get_sprite(shell->match_state, entity));
            Rect render_rect = (Rect) {
                .x = params.position.x, .y = params.position.y,
                .w = sprite_info.frame_width, .h = sprite_info.frame_height
            };
            if (!render_rect.intersects(SCREEN_RECT)) {
                continue;
            }
            params.options |= RENDER_SPRITE_NO_CULL;

            ysort_params.push_back(params);
        }
        ysort_render_params(ysort_params, 0, ysort_params.size() - 1);
        for (const RenderSpriteParams& params : ysort_params) {
            render_sprite_frame(params.sprite, params.frame, params.position, params.options, params.recolor_id);
            if (params.sprite == SPRITE_UNIT_BALLOON) {
                if ((params.frame.x == 3 || params.frame.x == 4) && params.frame.y != 22) {
                    int steam_hframe = params.frame.x - 3;
                    uint32_t options = RENDER_SPRITE_NO_CULL;
                    if (params.frame.y == 1) {
                        options |= RENDER_SPRITE_FLIP_H;
                    }
                    render_sprite_frame(SPRITE_UNIT_BALLOON_STEAM, ivec2(steam_hframe, 0), params.position, options, 0);
                }
            }
        }

        // Sky particles
        for (uint32_t particle_index = 0; particle_index < shell->match_state.particles.size(); particle_index++) {
            const Particle& particle = shell->match_state.particles[particle_index];
            if (particle.layer != PARTICLE_LAYER_SKY) {
                continue;
            }
            match_shell_render_particle(shell, particle);
        }
    }

    // Fog of War
    {
        ZoneScopedN("fog of war");
        for (int fog_pass = 0; fog_pass < 2; fog_pass++) {
            for (int y = 0; y < max_visible_tiles.y; y++) {
                for (int x = 0; x < max_visible_tiles.x; x++) {
                    ivec2 fog_cell = base_coords + ivec2(x, y);
                    int fog = match_shell_get_fog(shell, fog_cell);
                    if (fog > 0) {
                        continue;
                    }
                    if (fog_pass == 1 && fog == FOG_EXPLORED) {
                        continue;
                    }

                    uint32_t neighbors = 0;
                    for (int direction = 0; direction < DIRECTION_COUNT; direction += 2) {
                        ivec2 neighbor_cell = fog_cell + DIRECTION_IVEC2[direction];
                        if (!map_is_cell_in_bounds(shell->match_state.map, neighbor_cell)) {
                            neighbors += DIRECTION_MASK[direction];
                            continue;
                        }
                        if ((fog_pass == 0 && match_shell_get_fog(shell, neighbor_cell) < 1) ||
                            (fog_pass != 0 && match_shell_get_fog(shell, neighbor_cell) == FOG_HIDDEN)) {
                            neighbors += DIRECTION_MASK[direction];
                        }
                    }

                    for (int direction = 1; direction < DIRECTION_COUNT; direction += 2) {
                        ivec2 neighbor_cell = fog_cell + DIRECTION_IVEC2[direction];
                        int prev_direction = direction - 1;
                        int next_direction = (direction + 1) % DIRECTION_COUNT;
                        if ((neighbors & DIRECTION_MASK[prev_direction]) != DIRECTION_MASK[prev_direction] ||
                            (neighbors & DIRECTION_MASK[next_direction]) != DIRECTION_MASK[next_direction]) {
                            continue;
                        }
                        if (!map_is_cell_in_bounds(shell->match_state.map, neighbor_cell)) {
                            neighbors += DIRECTION_MASK[direction];
                            continue;
                        }
                        if ((fog_pass == 0 && match_shell_get_fog(shell, neighbor_cell) < 1) ||
                            (fog_pass != 0 && match_shell_get_fog(shell, neighbor_cell) == FOG_HIDDEN)) {
                            neighbors += DIRECTION_MASK[direction];
                        }
                    }
                    int autotile_index = map_neighbors_to_autotile_index(neighbors);
                    #ifdef GOLD_DEBUG
                        if (shell->debug_fog_level == MATCH_SHELL_FOG_DISABLED) {
                            continue;
                        }
                    #endif
                    render_sprite_frame(fog_pass == 0 ? SPRITE_FOG_EXPLORED : SPRITE_FOG_HIDDEN,
                            ivec2(autotile_index % AUTOTILE_HFRAMES, autotile_index / AUTOTILE_HFRAMES),
                            base_pos + ivec2(x * TILE_SIZE, y * TILE_SIZE), RENDER_SPRITE_NO_CULL, 0);
                }
            }
        }
    }

    // Above fog params
    for (const RenderSpriteParams& params : above_fog_sprite_params) {
        render_sprite_frame(params.sprite, params.frame, params.position, params.options, params.recolor_id);
    }

    // UI
    {
        ZoneScopedN("ui");

        // UI Building Placement
        if (shell->mode == MATCH_SHELL_MODE_BUILDING_PLACE && !match_shell_is_mouse_in_ui()) {
            const EntityData& building_data = entity_get_data(shell->building_type);

            // First draw the building
            ivec2 building_cell = match_shell_get_building_cell(building_data.cell_size, shell->camera_offset);
            render_sprite_frame(building_data.sprite, ivec2(3, 0), (building_cell * TILE_SIZE) - shell->camera_offset, RENDER_SPRITE_NO_CULL, shell->match_state.players[network_get_player_id()].recolor_id);

            // Then draw the green / red squares
            ivec2 miner_cell = shell->match_state.entities.get_by_id(match_get_nearest_builder(shell->match_state, shell->selection, building_cell)).cell;
            for (int y = building_cell.y; y < building_cell.y + building_data.cell_size; y++) {
                for (int x = building_cell.x; x < building_cell.x + building_data.cell_size; x++) {
                    ivec2 cell = ivec2(x, y);
                    bool is_cell_red = !match_shell_is_building_place_cell_valid(shell, miner_cell, cell);

                    // Don't allow buildings too close to goldmines
                    if (shell->building_type == ENTITY_HALL) {
                    for (uint32_t goldmine_index = 0; goldmine_index < shell->match_state.entities.size(); goldmine_index++) {
                        const Entity& goldmine = shell->match_state.entities[goldmine_index];
                            if (goldmine.type == ENTITY_GOLDMINE && entity_goldmine_get_block_building_rect(goldmine.cell).has_point(cell)) {
                                is_cell_red = true;
                                break;
                            }
                        }
                    }

                    RenderColor cell_color = is_cell_red ? RENDER_COLOR_RED_TRANSPARENT : RENDER_COLOR_GREEN_TRANSPARENT;
                    Rect cell_rect = (Rect) {
                        .x = (x * TILE_SIZE) - shell->camera_offset.x,
                        .y = (y * TILE_SIZE) - shell->camera_offset.y,
                        .w = TILE_SIZE,
                        .h = TILE_SIZE
                    };
                    render_fill_rect(cell_rect, cell_color);
                }
            }
        }
        // End UI building placement

        // UI queued building placements
        for (EntityId entity_id : shell->selection) {
            const Entity& entity = shell->match_state.entities.get_by_id(entity_id);
            // If it's not an allied unit, then we can break out of this whole loop, since the rest of the selection won't be either
            if (!entity_is_unit(entity.type) ||
                    (!shell->replay_mode && entity.player_id != network_get_player_id())) {
                break;
            }

            if (entity.target.type == TARGET_BUILD && entity.target.id == ID_NULL) {
                match_shell_render_target_build(shell, entity.target, entity.player_id);
            }
            if (entity.target_queue_index != ENTITY_TARGET_QUEUE_INDEX_NONE) {
                const TargetQueue* entity_target_queue = shell->match_state.entity_target_queues.get(entity.target_queue_index);
                for (uint32_t target_queue_index = 0; target_queue_index < entity_target_queue->size(); target_queue_index++) {
                    const Target& target = (*entity_target_queue)[target_queue_index];
                    if (target.type == TARGET_BUILD) {
                        match_shell_render_target_build(shell, target, entity.player_id);
                    }
                }
            }
        }

        // UI Chat
        static const ivec2 CHAT_PROMPT_POSITION = ivec2(32 + 12, MINIMAP_RECT.y - 20);
        for (uint32_t chat_index = 0; chat_index < shell->chat.size(); chat_index++) {
            const ChatMessage& message = shell->chat[shell->chat.size() - chat_index - 1];
            ivec2 message_pos = CHAT_PROMPT_POSITION + ivec2(0, -((chat_index + 1) * 16));
            if (strlen(message.prefix) != 0) {
                render_text(FONT_HACK_SHADOW, message.prefix, message_pos + ivec2(1, 1));
                render_text(message.prefix_font, message.prefix, message_pos);
                message_pos.x += render_get_text_size(FONT_HACK_WHITE, message.prefix).x + render_get_text_size(FONT_HACK_WHITE, " ").x;
            }
            render_text(FONT_HACK_SHADOW, message.message, message_pos + ivec2(1, 1));
            render_text(FONT_HACK_WHITE, message.message, message_pos);
        }
        if (input_is_text_input_active()) {
            char prompt_str[128];
            sprintf(prompt_str, "Chat: %s", shell->chat_message.c_str());
            render_text(FONT_HACK_SHADOW, prompt_str, CHAT_PROMPT_POSITION + ivec2(1, 1));
            render_text(FONT_HACK_WHITE, prompt_str, CHAT_PROMPT_POSITION);
            if (shell->chat_cursor_visible) {
                int prompt_width = render_get_text_size(FONT_HACK_WHITE, prompt_str).x;
                ivec2 cursor_pos = CHAT_PROMPT_POSITION + ivec2(prompt_width - 1, -1);
                render_text(FONT_HACK_SHADOW, "|", cursor_pos + ivec2(1, 1));
                render_text(FONT_HACK_WHITE, "|", cursor_pos);
            }
        }

        // Select rect
        if (match_shell_is_selecting(shell)) {
            ivec2 mouse_world_pos = ivec2(input_get_mouse_position().x, std::min(input_get_mouse_position().y, SCREEN_HEIGHT - MATCH_SHELL_UI_HEIGHT)) + shell->camera_offset;
            Rect select_rect = (Rect) {
                .x = std::min(shell->select_origin.x, mouse_world_pos.x) - shell->camera_offset.x,
                .y = std::min(shell->select_origin.y, mouse_world_pos.y) - shell->camera_offset.y,
                .w = std::abs(shell->select_origin.x - mouse_world_pos.x),
                .h = std::abs(shell->select_origin.y - mouse_world_pos.y)
            };
            select_rect.x++;
            select_rect.y++;
            render_draw_rect(select_rect, RENDER_COLOR_OFFBLACK_A200);
            select_rect.x--;
            select_rect.y--;
            render_draw_rect(select_rect, RENDER_COLOR_WHITE);
        }

        // UI frames
        const SpriteInfo& minimap_sprite_info = render_get_sprite_info(SPRITE_UI_MINIMAP);
        render_sprite_frame(SPRITE_UI_MINIMAP, ivec2(0, 0), ivec2(0, SCREEN_HEIGHT - minimap_sprite_info.frame_height), 0, 0);
        render_ninepatch(SPRITE_UI_FRAME_BOLTS, BOTTOM_PANEL_RECT);
        if (shell->replay_mode) {
            render_ninepatch(SPRITE_UI_FRAME_BOLTS, REPLAY_PANEL_RECT);
        } else {
            render_ninepatch(SPRITE_UI_FRAME_BOLTS, BUTTON_PANEL_RECT);
            render_sprite_frame(SPRITE_UI_WANTED_SIGN, ivec2(0, 0), WANTED_SIGN_POSITION, 0, 0);
        }

        // UI Control groups
        for (uint32_t control_group_index = 0; control_group_index < MATCH_SHELL_CONTROL_GROUP_COUNT; control_group_index++) {
            // Count the entities in this control group and determine which is the most common
            uint32_t entity_count = 0;
            std::unordered_map<EntityType, uint32_t> entity_occurances;
            EntityType most_common_entity_type = ENTITY_MINER;
            entity_occurances[ENTITY_MINER] = 0;

            for (EntityId id : shell->control_groups[control_group_index]) {
                uint32_t entity_index = shell->match_state.entities.get_index_of(id);
                if (entity_index == INDEX_INVALID || shell->match_state.entities[entity_index].health == 0) {
                    continue;
                }

                EntityType entity_type = shell->match_state.entities[entity_index].type;
                auto occurances_it = entity_occurances.find(entity_type);
                if (occurances_it == entity_occurances.end()) {
                    entity_occurances[entity_type] = 1;
                } else {
                    occurances_it->second++;
                }
                if (entity_occurances[entity_type] > entity_occurances[most_common_entity_type]) {
                    most_common_entity_type = entity_type;
                }
                entity_count++;
            }

            if (entity_count == 0) {
                continue;
            }

            int button_frame = 0;
            FontName font = FONT_M3X6_OFFBLACK;
            if (shell->control_group_selected != control_group_index) {
                font = FONT_M3X6_DARKBLACK;
                button_frame = 2;
            }

            SpriteName button_icon = entity_get_icon(shell->match_state, most_common_entity_type, network_get_player_id());
            const SpriteInfo& sprite_info = render_get_sprite_info(SPRITE_UI_CONTROL_GROUP);
            ivec2 render_pos = ivec2(BOTTOM_PANEL_RECT.x, BOTTOM_PANEL_RECT.y) + ivec2(2, 0) + ivec2((3 + sprite_info.frame_width) * control_group_index, -32);
            render_sprite_frame(SPRITE_UI_CONTROL_GROUP, ivec2(button_frame, 0), render_pos, RENDER_SPRITE_NO_CULL, 0);
            render_sprite_frame(button_icon, ivec2(button_frame, 0), render_pos + ivec2(2, 0), RENDER_SPRITE_NO_CULL, 0);
            char control_group_number_text[4];
            sprintf(control_group_number_text, "%u", control_group_index == 9 ? 0 : control_group_index + 1);
            render_text(font, control_group_number_text, render_pos + ivec2(3, -9));
            char control_group_count_text[4];
            sprintf(control_group_count_text, "%u", entity_count);
            ivec2 count_text_size = render_get_text_size(font, control_group_count_text);
            render_text(font, control_group_count_text, render_pos + ivec2(32 - count_text_size.x, 23 - count_text_size.y));
        }

        // Idle miner
        if (!shell->replay_mode) {
            const EntityList idle_miners = match_shell_find_idle_miners(shell);
            if (!idle_miners.empty()) {
                const Rect idle_miner_button_rect = match_shell_get_idle_miner_button_rect();
                const bool is_hovered = idle_miner_button_rect.has_point(input_get_mouse_position());
                const ivec2 render_pos = ivec2(idle_miner_button_rect.x, idle_miner_button_rect.y) - ivec2(0, (int)is_hovered);
                const FontName font = is_hovered ? FONT_M3X6_WHITE : FONT_M3X6_OFFBLACK;
                render_sprite_frame(SPRITE_UI_CONTROL_GROUP, ivec2((int)is_hovered, 0), render_pos, RENDER_SPRITE_NO_CULL, 0);
                render_sprite_frame(SPRITE_BUTTON_ICON_MINER, ivec2((int)is_hovered, 0), render_pos + ivec2(2, 0), RENDER_SPRITE_NO_CULL, 0);
                char idle_miner_count_text[4];
                sprintf(idle_miner_count_text, "%u", (uint32_t)idle_miners.size());
                ivec2 idle_miner_count_text_size = render_get_text_size(font, idle_miner_count_text);
                render_text(font, idle_miner_count_text, render_pos + ivec2(32 - idle_miner_count_text_size.x, 23 - idle_miner_count_text_size.y));
            }
        }

        // UI Status message
        if (shell->status_timer != 0) {
            int status_message_width = render_get_text_size(FONT_HACK_WHITE, shell->status_message.c_str()).x;
            ivec2 status_message_position = ivec2((SCREEN_WIDTH / 2) - (status_message_width / 2), SCREEN_HEIGHT - 148);
            render_text(FONT_HACK_SHADOW, shell->status_message.c_str(), status_message_position + ivec2(1, 1));
            render_text(FONT_HACK_WHITE, shell->status_message.c_str(), status_message_position);
        }

        // Hotkeys
        for (uint32_t hotkey_index = 0; hotkey_index < HOTKEY_GROUP_SIZE; hotkey_index++) {
            InputHotkey hotkey = shell->hotkey_group[hotkey_index];
            if (hotkey == INPUT_HOTKEY_NONE) {
                continue;
            }

            const SpriteInfo& sprite_info = render_get_sprite_info(SPRITE_UI_ICON_BUTTON);
            ivec2 hotkey_position = HOTKEY_BUTTON_POSITIONS[hotkey_index];
            Rect hotkey_rect = (Rect) {
                .x = hotkey_position.x, .y = hotkey_position.y,
                .w = sprite_info.frame_width, .h = sprite_info.frame_height
            };
            int hframe = 0;
            if (!match_shell_does_player_meet_hotkey_requirements(shell->match_state, hotkey)) {
                hframe = 2;
            } else if (match_shell_is_camera_free(shell) && !match_shell_is_selecting(shell) && hotkey_rect.has_point(input_get_mouse_position())) {
                hframe = 1;
                hotkey_position.y--;
            }

            render_sprite_frame(SPRITE_UI_ICON_BUTTON, ivec2(hframe, 0), hotkey_position, RENDER_SPRITE_NO_CULL, 0);
            render_sprite_frame(match_shell_hotkey_get_sprite(shell, hotkey, match_shell_should_render_hotkey_toggled(shell, hotkey)), ivec2(hframe, 0), hotkey_position, RENDER_SPRITE_NO_CULL, 0);
        }

        // UI Tooltip
        if (match_shell_is_camera_free(shell) &&
                !(match_shell_is_selecting(shell) || match_shell_is_in_menu(shell))) {
            uint32_t hotkey_hovered_index;
            for (hotkey_hovered_index = 0; hotkey_hovered_index < HOTKEY_GROUP_SIZE; hotkey_hovered_index++) {
                Rect hotkey_rect = (Rect) {
                    .x = HOTKEY_BUTTON_POSITIONS[hotkey_hovered_index].x,
                    .y = HOTKEY_BUTTON_POSITIONS[hotkey_hovered_index].y,
                    .w = 32,
                    .h = 32
                };
                if (hotkey_rect.has_point(input_get_mouse_position())) {
                    break;
                }
            }

            InputHotkey hotkey = INPUT_HOTKEY_NONE;
            if (hotkey_hovered_index != HOTKEY_GROUP_SIZE) {
                hotkey = shell->hotkey_group[hotkey_hovered_index];
            }

            const Rect idle_miner_rect = match_shell_get_idle_miner_button_rect();
            if (idle_miner_rect.has_point(input_get_mouse_position())) {
                // Determine if we actually have idle miners before rendering the tooltip
                bool has_idle_miners = false;
                for (uint32_t entity_index = 0; entity_index < shell->match_state.entities.size(); entity_index++) {
                    const Entity& entity = shell->match_state.entities[entity_index];
                    if (entity.player_id == network_get_player_id() && entity_is_idle_miner(entity)) {
                        has_idle_miners = true;
                        break;
                    }
                }
                if (has_idle_miners) {
                    hotkey = INPUT_HOTKEY_IDLE_MINER;
                }
            }

            if (hotkey != INPUT_HOTKEY_NONE) {
                match_shell_render_tooltip(shell, hotkey);
            }
        }
        // End render tooltip

        // UI Selection list
        if (shell->selection.size() == 1) {
            const Entity& entity = shell->match_state.entities.get_by_id(shell->selection[0]);
            const EntityData& entity_data = entity_get_data(entity.type);

            // Entity name
            const SpriteInfo& frame_sprite_info = render_get_sprite_info(SPRITE_UI_TEXT_FRAME);
            ivec2 text_size = render_get_text_size(FONT_WESTERN8_OFFBLACK, entity_data.name);
            int frame_count = (text_size.x / frame_sprite_info.frame_width) + 1;
            if (text_size.x % frame_sprite_info.frame_width != 0) {
                frame_count++;
            }
            for (int frame = 0; frame < frame_count; frame++) {
                int hframe = 1;
                if (frame == 0) {
                    hframe = 0;
                } else if (frame == frame_count - 1) {
                    hframe = 2;
                }
                render_sprite_frame(SPRITE_UI_TEXT_FRAME, ivec2(hframe, 0), SELECTION_LIST_TOP_LEFT + ivec2(frame * frame_sprite_info.frame_width, 0), RENDER_SPRITE_NO_CULL, 0);
            }
            ivec2 frame_size = ivec2(frame_count * frame_sprite_info.frame_width, frame_sprite_info.frame_height);
            render_text(FONT_WESTERN8_OFFBLACK, entity_data.name, SELECTION_LIST_TOP_LEFT + ivec2((frame_size.x / 2) - (text_size.x / 2), 0));

            // Entity icon
            render_sprite_frame(SPRITE_UI_ICON_BUTTON, ivec2(0, 0), SELECTION_LIST_TOP_LEFT + ivec2(0, 18), RENDER_SPRITE_NO_CULL, 0);
            render_sprite_frame(entity_get_icon(shell->match_state, entity.type, entity.player_id), ivec2(0, 0), SELECTION_LIST_TOP_LEFT + ivec2(0, 18), RENDER_SPRITE_NO_CULL, 0);

            if (entity_is_misc(entity.type)) {
                if (entity.mode == MODE_GOLDMINE_COLLAPSED) {
                    render_text(FONT_HACK_WHITE, "Collapsed!", SELECTION_LIST_TOP_LEFT + ivec2(36, 20));
                } else if (entity.type != ENTITY_SWITCH && entity.mode != MODE_GOLDMINE_RIGGED) {
                    char gold_left_str[8];
                    sprintf(gold_left_str, "%u", entity.gold_held);
                    render_sprite_frame(SPRITE_UI_GOLD_ICON, ivec2(0, 0), SELECTION_LIST_TOP_LEFT + ivec2(36, 20), RENDER_SPRITE_NO_CULL, 0);
                    render_text(FONT_HACK_WHITE, gold_left_str, SELECTION_LIST_TOP_LEFT + ivec2(36 + render_get_sprite_info(SPRITE_UI_GOLD_ICON).frame_width + 2, 21));
                }
            } else {
                ivec2 healthbar_position = SELECTION_LIST_TOP_LEFT + ivec2(34, 18 + 2);
                ivec2 healthbar_size = ivec2(64, 12);
                match_shell_render_healthbar(RENDER_HEALTHBAR, healthbar_position, healthbar_size, entity.health, entity_data.max_health);

                char health_text[16];
                sprintf(health_text, "%i/%i", entity.health, entity_data.max_health);
                ivec2 health_text_size = render_get_text_size(FONT_HACK_WHITE, health_text);
                ivec2 health_text_position = healthbar_position + (healthbar_size / 2) - (health_text_size / 2);
                render_text(FONT_HACK_WHITE, health_text, health_text_position);

                if (entity_is_unit(entity.type) && entity_data.unit_data.max_energy != 0)  {
                    healthbar_position += ivec2(0, healthbar_size.y + 1);
                    match_shell_render_healthbar(RENDER_ENERGY_BAR, healthbar_position, healthbar_size, entity.energy, entity_data.unit_data.max_energy);
                    sprintf(health_text, "%i/%i", entity.energy, entity_data.unit_data.max_energy);
                    health_text_size = render_get_text_size(FONT_HACK_WHITE, health_text);
                    health_text_position = healthbar_position + (healthbar_size / 2) - (health_text_size / 2);
                    render_text(FONT_HACK_WHITE, health_text, health_text_position);
                }

                SpriteName stat_icons[4];
                int stat_count = 0;

                // Detection
                if (entity_has_detection(shell->match_state, entity)) {
                    stat_icons[stat_count] = SPRITE_UI_STAT_ICON_DETECTION;
                    stat_count++;
                }

                ivec2 stat_position = SELECTION_LIST_TOP_LEFT + ivec2(0, 18 + 34);
                ivec2 stat_positions[2];
                const int STAT_ICON_SIZE = render_get_sprite_info(SPRITE_UI_STAT_ICON_DETECTION).frame_width;
                int STAT_TEXT_PADDING = 1;

                for (int stat_index = 0; stat_index < stat_count; stat_index++) {
                    render_sprite_frame(stat_icons[stat_index], ivec2(0, 0), stat_position, RENDER_SPRITE_NO_CULL, 0);
                    stat_positions[stat_index] = stat_position;
                    stat_position.y += STAT_ICON_SIZE + STAT_TEXT_PADDING;
                }

                // Determine if a stat is being hovered
                int stat_hovered;
                for (stat_hovered = 0; stat_hovered < stat_count; stat_hovered++) {
                    Rect stat_rect = (Rect) {
                        .x = stat_positions[stat_hovered].x,
                        .y = stat_positions[stat_hovered].y,
                        .w = STAT_ICON_SIZE, .h = STAT_ICON_SIZE
                    };
                    if (stat_rect.has_point(input_get_mouse_position())) {
                        break;
                    }
                }

                // Render stat tooltip
                if (stat_hovered != stat_count) {
                    const SpriteInfo& tooltip_info = render_get_sprite_info(SPRITE_UI_TOOLTIP_FRAME);
                    const char* tooltip_text = match_shell_render_get_stat_tooltip(stat_icons[stat_hovered]);

                    // Determine tooltip size
                    ivec2 tooltip_text_size = render_get_text_size(FONT_HACK_OFFBLACK, tooltip_text);
                    int tooltip_width = tooltip_text_size.x + tooltip_info.frame_width;
                    int tooltip_frame_count = tooltip_width / tooltip_info.frame_width;
                    if (tooltip_width % tooltip_info.frame_width != 0) {
                        tooltip_frame_count++;
                    }

                    // Determine tooltip position
                    ivec2 tooltip_pos = stat_positions[stat_hovered] + ivec2(STAT_ICON_SIZE - 1, -((tooltip_info.frame_height * 2) - 2));

                    // Render tooltip background
                    for (int frame_x = 0; frame_x < tooltip_frame_count; frame_x++) {
                        int hframe = 1;
                        if (frame_x == 0) {
                            hframe = 0;
                        } else if (frame_x == tooltip_frame_count - 1) {
                            hframe = 2;
                        }

                        render_sprite_frame(SPRITE_UI_TOOLTIP_FRAME, ivec2(hframe, 0), tooltip_pos + ivec2(tooltip_info.frame_width * frame_x, 0), RENDER_SPRITE_NO_CULL, 0);
                        render_sprite_frame(SPRITE_UI_TOOLTIP_FRAME, ivec2(hframe, 2), tooltip_pos + ivec2(tooltip_info.frame_width * frame_x, tooltip_info.frame_height), RENDER_SPRITE_NO_CULL, 0);
                    }

                    // Render tooltip text
                    render_text(FONT_HACK_OFFBLACK, tooltip_text, tooltip_pos + ivec2(4, 2));
                }
            }
        } else {
            for (uint32_t selection_index = 0; selection_index < shell->selection.size(); selection_index++) {
                match_shell_render_entity_icon(
                    shell,
                    shell->match_state.entities.get_by_id(shell->selection[selection_index]),
                    match_shell_get_selection_list_item_rect(selection_index));
            }
        }
        // End UI Selection List

        // UI Building queues
        if (shell->selection.size() == 1) {
            const Entity& building = shell->match_state.entities.get_by_id(shell->selection[0]);
            if (entity_is_building(building.type) &&
                    !building.queue.empty() &&
                    (shell->replay_mode || building.player_id == network_get_player_id())) {
                // Render building queue icon buttons
                const SpriteInfo& icon_sprite_info = render_get_sprite_info(SPRITE_UI_ICON_BUTTON);
                for (uint32_t building_queue_index = 0; building_queue_index < building.queue.size(); building_queue_index++) {
                    Rect icon_rect = (Rect) {
                        .x = BUILDING_QUEUE_POSITIONS[building_queue_index].x,
                        .y = BUILDING_QUEUE_POSITIONS[building_queue_index].y,
                        .w = icon_sprite_info.frame_width,
                        .h = icon_sprite_info.frame_height
                    };
                    SpriteName item_sprite;
                    switch (building.queue[building_queue_index].type) {
                        case BUILDING_QUEUE_ITEM_UNIT: {
                            item_sprite = entity_get_icon(shell->match_state, building.queue[building_queue_index].unit_type, building.player_id);
                            break;
                        }
                        case BUILDING_QUEUE_ITEM_UPGRADE: {
                            item_sprite = upgrade_get_data(building.queue[building_queue_index].upgrade).icon;
                            break;
                        }
                    }
                    bool hovered =
                        match_shell_is_camera_free(shell) &&
                        !match_shell_is_selecting(shell) &&
                        icon_rect.has_point(input_get_mouse_position());
                    render_sprite_frame(SPRITE_UI_ICON_BUTTON, ivec2(hovered ? 1 : 0, 0), ivec2(icon_rect.x, icon_rect.y - (int)hovered), RENDER_SPRITE_NO_CULL, 0);
                    render_sprite_frame(item_sprite, ivec2(hovered ? 1 : 0, 0), ivec2(icon_rect.x, icon_rect.y - (int)hovered), RENDER_SPRITE_NO_CULL, 0);
                }

                // Render building queue progress bar
                if (building.timer == BUILDING_QUEUE_BLOCKED) {
                    const char* message = shell->match_state.entities.is_full() || match_get_player_max_population(shell->match_state, network_get_player_id()) == MATCH_MAX_POPULATION
                        ? "Unit limit reached."
                        : "Build more houses.";
                    render_text(FONT_WESTERN8_GOLD, message, ivec2(BUILDING_QUEUE_PROGRESS_BAR_RECT.x + 2, BUILDING_QUEUE_PROGRESS_BAR_RECT.y - 14));
                } else if (building.timer == BUILDING_QUEUE_EXIT_BLOCKED) {
                    render_text(FONT_WESTERN8_GOLD, "Exit is blocked.", ivec2(BUILDING_QUEUE_PROGRESS_BAR_RECT.x + 2, BUILDING_QUEUE_PROGRESS_BAR_RECT.y - 14));
                } else {
                    int item_duration = (int)building_queue_item_duration(building.queue[0]);
                    Rect building_queue_progress_bar_subrect = BUILDING_QUEUE_PROGRESS_BAR_RECT;
                    building_queue_progress_bar_subrect.w = (BUILDING_QUEUE_PROGRESS_BAR_RECT.w * (item_duration - (int)building.timer) / item_duration);
                    render_fill_rect(building_queue_progress_bar_subrect, RENDER_COLOR_WHITE);
                    render_draw_rect(BUILDING_QUEUE_PROGRESS_BAR_RECT, RENDER_COLOR_OFFBLACK);
                }
            }
        }

        // UI Garrisoned units
        if (shell->selection.size() == 1) {
            const Entity& carrier = shell->match_state.entities.get_by_id(shell->selection[0]);
            if (carrier.type == ENTITY_GOLDMINE ||
                    shell->replay_mode ||
                    carrier.player_id == network_get_player_id()) {
                const SpriteInfo& icon_sprite_info = render_get_sprite_info(SPRITE_UI_ICON_BUTTON);
                int index = 0;
                for (uint32_t garrisoned_units_index = 0; garrisoned_units_index < carrier.garrisoned_units.size(); garrisoned_units_index++) {
                    EntityId entity_id = carrier.garrisoned_units[garrisoned_units_index];
                    const Entity& garrisoned_unit = shell->match_state.entities.get_by_id(entity_id);
                    // We have to make this check here because goldmines might have both allied and enemy units in them
                    if (!shell->replay_mode && garrisoned_unit.player_id != network_get_player_id()) {
                        continue;
                    }

                    Rect icon_rect = (Rect) {
                        .x = GARRISON_ICON_POSITIONS[index].x,
                        .y = GARRISON_ICON_POSITIONS[index].y,
                        .w = icon_sprite_info.frame_width,
                        .h = icon_sprite_info.frame_height
                    };
                    match_shell_render_entity_icon(shell, garrisoned_unit, icon_rect);
                    index++;
                }
            }
        }

        // Resource counters
        const SpriteInfo& population_icon_sprite_info = render_get_sprite_info(SPRITE_UI_HOUSE_ICON);
        const SpriteInfo& gold_icon_sprite_info = render_get_sprite_info(SPRITE_UI_GOLD_ICON);
        int resource_base_y = 0;
        for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
            if (!shell->replay_mode && player_id != network_get_player_id()) {
                continue;
            }
            if (shell->replay_mode && shell->match_state.players[player_id].mode == PLAYER_MODE_INACTIVE) {
                continue;
            }

            int render_x = SCREEN_WIDTH;
            const ivec2 text_shadow_offset = ivec2(1, 1);

            // Rendering from right to left here

            // Population text
            render_x -= (render_get_text_size(FONT_HACK_WHITE, "200/200").x + 2);
            {
                char population_text[8];
                sprintf(population_text, "%u/%u", match_get_player_population(shell->match_state, player_id), match_get_player_max_population(shell->match_state, player_id));
                render_text(FONT_HACK_SHADOW, population_text, ivec2(render_x, resource_base_y + 3) + text_shadow_offset);
                render_text(FONT_HACK_WHITE, population_text, ivec2(render_x, resource_base_y + 3));
            }

            // Population icon
            render_x -= (population_icon_sprite_info.frame_width + 2);
            {
                render_sprite_frame(SPRITE_UI_HOUSE_ICON, ivec2(0, 0), ivec2(render_x, resource_base_y), RENDER_SPRITE_NO_CULL, 0);
            }

            // Gold text
            render_x -= (render_get_text_size(FONT_HACK_WHITE, "99999").x + 2);
            {
                char gold_text[8];
                sprintf(gold_text, "%u", shell->displayed_gold_amounts[player_id]);
                render_text(FONT_HACK_SHADOW, gold_text, ivec2(render_x, resource_base_y + 3) + text_shadow_offset);
                render_text(FONT_HACK_WHITE, gold_text, ivec2(render_x, resource_base_y + 3));
            }

            // Gold icon
            render_x -= (gold_icon_sprite_info.frame_width + 2);
            {
                render_sprite_frame(SPRITE_UI_GOLD_ICON, ivec2(0, 0), ivec2(render_x, resource_base_y + 2), RENDER_SPRITE_NO_CULL, 0);
            }

            // Player name
            if (shell->replay_mode) {
                render_x -= (render_get_text_size(FONT_HACK_WHITE, shell->match_state.players[player_id].name).x + 16);
                render_text(FONT_HACK_SHADOW, shell->match_state.players[player_id].name, ivec2(render_x, resource_base_y + 3) + text_shadow_offset);
                render_text((FontName)(FONT_HACK_PLAYER0 + shell->match_state.players[player_id].recolor_id), shell->match_state.players[player_id].name, ivec2(render_x, resource_base_y + 3));
            }

            resource_base_y += population_icon_sprite_info.frame_height;
        }

        // Objectives
        if (!shell->scenario_objectives.empty()) {
            const SpriteInfo& menu_button_sprite_info = render_get_sprite_info(SPRITE_UI_BUTTON_BURGER);
            ivec2 objectives_text_pos = MENU_BUTTON_POSITION + ivec2(1, menu_button_sprite_info.frame_height + 4);
            render_text(FONT_HACK_SHADOW, "Objectives:", objectives_text_pos + ivec2(1, 1));
            render_text(FONT_HACK_GOLD_SATURATED, "Objectives:", objectives_text_pos);

            objectives_text_pos += ivec2(4, 20);
            const SpriteInfo& checkbox_sprite_info = render_get_sprite_info(SPRITE_UI_OBJECTIVE_CHECKBOX);
            for (const Objective& objective : shell->scenario_objectives) {
                render_sprite_frame(SPRITE_UI_OBJECTIVE_CHECKBOX, ivec2((int)objective.is_complete, 0), objectives_text_pos, RENDER_SPRITE_NO_CULL, 0);

                // Determine objective text
                char objective_text[128];
                char* objective_text_ptr = objective_text;
                objective_text_ptr += sprintf(objective_text_ptr, "%s", objective.description.c_str());
                if (!objective.is_complete && objective.counter_type != OBJECTIVE_COUNTER_TYPE_NONE) {
                    uint32_t counter_value = 0;
                    if (objective.counter_type == OBJECTIVE_COUNTER_TYPE_ENTITY) {
                        counter_value = match_shell_get_player_entity_count(shell, network_get_player_id(), (EntityType)objective.counter_value);
                    } else if (objective.counter_type == OBJECTIVE_COUNTER_TYPE_VARIABLE) {
                        counter_value = objective.counter_value;
                    }

                    objective_text_ptr += sprintf(objective_text_ptr, " (%u/%u)", counter_value, objective.counter_target);
                }

                // Render objective text
                ivec2 text_pos = objectives_text_pos + ivec2(checkbox_sprite_info.frame_width + 2, 1);
                render_text(FONT_HACK_SHADOW, objective_text, text_pos + ivec2(1, 1));
                render_text(FONT_HACK_GOLD_SATURATED, objective_text, text_pos);

                objectives_text_pos.y += checkbox_sprite_info.frame_height + 4;
            }

            if (shell->scenario_global_objective_counter.type == GLOBAL_OBJECTIVE_COUNTER_GOLD) {
                objectives_text_pos.x = 1;
                render_text(FONT_HACK_SHADOW, "Gold Mined:", objectives_text_pos + ivec2(1, 1));
                render_text(FONT_HACK_GOLD_SATURATED, "Gold Mined:", objectives_text_pos);
                objectives_text_pos += ivec2(4, 20);

                for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
                    if (shell->match_state.players[player_id].mode == PLAYER_MODE_INACTIVE) {
                        continue;
                    }

                    // Determine text
                    char text[64];
                    sprintf(text, "%s: %u", shell->match_state.players[player_id].name, shell->scenario_global_objective_counter.gold.values[player_id]);

                    // Render text
                    ivec2 text_pos = objectives_text_pos + ivec2(checkbox_sprite_info.frame_width + 2, 1);
                    render_text(FONT_HACK_SHADOW, text, text_pos + ivec2(1, 1));
                    render_text(FONT_HACK_GOLD_SATURATED, text, text_pos);

                    objectives_text_pos.y += checkbox_sprite_info.frame_height + 4;
                }
            } else if (shell->scenario_global_objective_counter.type == GLOBAL_OBJECTIVE_COUNTER_COUNTDOWN) {
                const uint32_t frames_remaining = shell->match_timer < shell->scenario_global_objective_counter.countdown.end_frame
                    ? shell->scenario_global_objective_counter.countdown.end_frame - shell->match_timer
                    : 0;
                const uint32_t minutes_remaining = (frames_remaining / UPDATES_PER_SECOND) / 60;
                const uint32_t seconds_remaining = (frames_remaining - (minutes_remaining * 60 * UPDATES_PER_SECOND)) / UPDATES_PER_SECOND;

                char timer_text[64];
                char* timer_text_ptr = timer_text;
                timer_text_ptr += sprintf(timer_text_ptr, "%s: %u:", shell->scenario_global_objective_counter.countdown.header_text, minutes_remaining);
                if (seconds_remaining < 10) {
                    timer_text_ptr += sprintf(timer_text_ptr, "0");
                }
                timer_text_ptr += sprintf(timer_text_ptr, "%u", seconds_remaining);

                // objectives_text_pos.x = 1;
                render_text(FONT_HACK_SHADOW, timer_text, objectives_text_pos + ivec2(1, 1));
                render_text(FONT_HACK_GOLD_SATURATED, timer_text, objectives_text_pos);
            } else if (shell->scenario_global_objective_counter.type == GLOBAL_OBJECTIVE_COUNTER_VARIABLE) {
                objectives_text_pos.x = 1;

                char text[64];
                sprintf(text, "%s: %u", shell->scenario_global_objective_counter.variable.header_text, shell->scenario_global_objective_counter.variable.value);
                render_text(FONT_HACK_SHADOW, text, objectives_text_pos + ivec2(1, 1));
                render_text(FONT_HACK_GOLD_SATURATED, text, objectives_text_pos);
                objectives_text_pos += ivec2(4, 20);
            }
        }

        // Menu button icon
        {
            const SpriteInfo& sprite_info = render_get_sprite_info(SPRITE_UI_BUTTON_BURGER);
            Rect menu_button_rect = (Rect) {
                .x = MENU_BUTTON_POSITION.x, .y = MENU_BUTTON_POSITION.y,
                .w = sprite_info.frame_width, .h = sprite_info.frame_height
            };
            bool hovered =
                shell->mode == MATCH_SHELL_MODE_MENU ||
                shell->mode == MATCH_SHELL_MODE_MENU_SURRENDER ||
                    (match_shell_is_camera_free(shell) &&
                    !(match_shell_is_selecting(shell)) &&
                    menu_button_rect.has_point(input_get_mouse_position()));
            render_sprite_frame(SPRITE_UI_BUTTON_BURGER, ivec2((int)hovered, 0), MENU_BUTTON_POSITION, RENDER_SPRITE_NO_CULL, 0);
        }

        // UI Disconnect frame
        const int DISCONNECT_FRAME_WIDTH = 200;
        const Rect DISCONNECT_FRAME_RECT = (Rect) {
            .x = (SCREEN_WIDTH / 2) - (DISCONNECT_FRAME_WIDTH / 2), .y = 32,
            .w = DISCONNECT_FRAME_WIDTH, .h = 150
        };
        if (shell->disconnect_timer > DISCONNECT_GRACE) {
            render_ninepatch(SPRITE_UI_FRAME, DISCONNECT_FRAME_RECT);
            ivec2 text_size = render_get_text_size(FONT_HACK_GOLD, "Waiting for players...");
            render_text(FONT_HACK_GOLD, "Waiting for players...", ivec2(DISCONNECT_FRAME_RECT.x + (DISCONNECT_FRAME_RECT.w / 2) - (text_size.x / 2), DISCONNECT_FRAME_RECT.y + 8));
            int player_text_y = 32;
            for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
                if (network_get_player(player_id).status == NETWORK_PLAYER_STATUS_NONE || !(shell->inputs[player_id].empty() || shell->inputs[player_id].front().empty())) {
                    continue;
                }

                render_text(FONT_HACK_GOLD, network_get_player(player_id).name, ivec2(DISCONNECT_FRAME_RECT.x + 8, DISCONNECT_FRAME_RECT.y + player_text_y));
                player_text_y += 16;
            }
        }
    }

    // MINIMAP
    // Minimap tiles
    {
        ZoneScopedN("minimap");
        for (int y = 0; y < shell->match_state.map.height; y++) {
            for (int x = 0; x < shell->match_state.map.width; x++) {
                render_minimap_putpixel(MINIMAP_LAYER_TILE, ivec2(x, y), match_shell_get_minimap_pixel_for_cell(shell, ivec2(x, y)));
            }
        }
        // Minimap entities
        for (uint32_t entity_index = 0; entity_index < shell->match_state.entities.size(); entity_index++) {
            const Entity& entity = shell->match_state.entities[entity_index];
            if (!entity_is_selectable(entity) || !match_shell_is_entity_visible(shell, entity)) {
                continue;
            }

            int entity_cell_size = entity_get_data(entity.type).cell_size;
            Rect entity_rect = (Rect) {
                .x = entity.cell.x, .y = entity.cell.y,
                .w = entity_cell_size, .h = entity_cell_size
            };
            render_minimap_fill_rect(MINIMAP_LAYER_TILE, entity_rect, match_shell_get_minimap_pixel_for_entity(shell, entity));
        }
        // Minimap remembered entities
        for (uint8_t team = 0; team < MAX_PLAYERS; team++) {
            if (!match_shell_should_render_remembered_entities_for_team(shell, team)) {
                continue;
            }

            for (uint32_t remembered_entity_index = 0; remembered_entity_index < shell->match_state.remembered_entities[team].size(); remembered_entity_index++) {
                const RememberedEntity& remembered_entity = shell->match_state.remembered_entities[team][remembered_entity_index];
                const EntityData& entity_data = entity_get_data(remembered_entity.type);
                Rect entity_rect = (Rect) {
                    .x = remembered_entity.cell.x, .y = remembered_entity.cell.y,
                    .w = entity_data.cell_size, .h = entity_data.cell_size
                };
                MinimapPixel pixel = entity_is_misc(remembered_entity.type) ? MINIMAP_PIXEL_GOLD : (MinimapPixel)(MINIMAP_PIXEL_PLAYER0 + remembered_entity.recolor_id);
                render_minimap_fill_rect(MINIMAP_LAYER_TILE, entity_rect, pixel);
            }
        }
        // Minimap fog of war
        for (int y = 0; y < shell->match_state.map.height; y++) {
            for (int x = 0; x < shell->match_state.map.width; x++) {
                int fog_value = match_shell_get_fog(shell, ivec2(x, y));
                MinimapPixel pixel;
                if (fog_value > 0) {
                    pixel = MINIMAP_PIXEL_TRANSPARENT;
                } else if (fog_value == 0) {
                    pixel = MINIMAP_PIXEL_OFFBLACK_TRANSPARENT;
                } else {
                    pixel = MINIMAP_PIXEL_OFFBLACK;
                }

                render_minimap_putpixel(MINIMAP_LAYER_FOG, ivec2(x, y), pixel);
            }
        }
        // Minimap alerts
        for (const Alert& alert : shell->alerts) {
            if (alert.timer <= ALERT_LINGER_DURATION) {
                continue;
            }

            int alert_timer = alert.timer - ALERT_LINGER_DURATION;
            int alert_rect_margin = 3 + (alert_timer <= 60
                                            ? 0
                                            : ((alert_timer - 60) / 3));
            Rect alert_rect = (Rect) {
                .x = alert.cell.x - alert_rect_margin,
                .y = alert.cell.y - alert_rect_margin,
                .w = alert.cell_size + 1 + (alert_rect_margin * 2),
                .h = alert.cell_size + 1 + (alert_rect_margin * 2),
            };
            // We want this on the fog layer because the minimap rect might go into the fog
            render_minimap_draw_rect(MINIMAP_LAYER_FOG, alert_rect, alert.pixel);
        }
        // Minimap camera rect
        Rect camera_rect = (Rect) {
            .x = shell->camera_offset.x / TILE_SIZE,
            .y = shell->camera_offset.y / TILE_SIZE,
            .w = (SCREEN_WIDTH / TILE_SIZE) - 1,
            .h = ((SCREEN_HEIGHT - MATCH_SHELL_UI_HEIGHT) / TILE_SIZE)
        };
        render_minimap_draw_rect(MINIMAP_LAYER_FOG, camera_rect, MINIMAP_PIXEL_WHITE);
        render_minimap_queue_render(ivec2(MINIMAP_RECT.x, MINIMAP_RECT.y), ivec2(shell->match_state.map.width, shell->match_state.map.height), ivec2(MINIMAP_RECT.w, MINIMAP_RECT.h));
    }

    ui_render(shell->ui_context);
    if (shell->replay_mode) {
        ui_render(shell->replay_ui_context);
    }
}

bool match_shell_should_render_remembered_entities_for_team(const MatchShell* shell, uint8_t team) {
    // If using replay fog none or everyone, then there is no need to render remembered buildings because we can just see all entities normally
    if (shell->replay_mode && (shell->replay_fog_index == REPLAY_FOG_NONE || shell->replay_fog_index == REPLAY_FOG_EVERYONE)) {
        return false;
    }
    // If we are in replay mode and looking at the fog for a specific player, then skip this team if it is not that player's team
    if (shell->replay_mode && shell->match_state.players[shell->replay_fog_player_ids[shell->replay_fog_index]].team != team) {
        return false;
    }
    // If we are not in replay mode, then skip this team if it is not the network player's team
    if (!shell->replay_mode && shell->match_state.players[network_get_player_id()].team != team) {
        return false;
    }

    return true;
}

bool match_shell_use_yellow_rings(const MatchShell* shell) {
    return shell->match_state.map.type == MAP_TYPE_KLONDIKE;
}

SpriteName match_shell_get_entity_select_ring(EntityType type, bool attacking) {
    if (type == ENTITY_GOLDMINE) {
        return SPRITE_SELECT_RING_GOLDMINE;
    }
    if (type == ENTITY_CRATE || type == ENTITY_SWITCH) {
        return SPRITE_SELECT_RING_CRATE;
    }
    if (type == ENTITY_LANDMINE) {
        if (attacking) {
            return SPRITE_SELECT_RING_LANDMINE_ATTACK;
        } else {
            return SPRITE_SELECT_RING_LANDMINE;
        }
    }

    SpriteName select_ring;
    int entity_cell_size = entity_get_data(type).cell_size;
    if (entity_is_unit(type)) {
        select_ring = (SpriteName)(SPRITE_SELECT_RING_UNIT + ((entity_cell_size - 1) * 2));
    } else {
        select_ring = (SpriteName)(SPRITE_SELECT_RING_BUILDING_SIZE2 + ((entity_cell_size - 2) * 2));
    }
    if (attacking) {
        select_ring = (SpriteName)(select_ring + 1);
    }
    return select_ring;
}

SpriteName match_shell_hotkey_get_sprite(const MatchShell* shell, InputHotkey hotkey, bool show_toggled) {
    const HotkeyButtonInfo& info = hotkey_get_button_info(hotkey);
    if (info.type == HOTKEY_BUTTON_BUILD || info.type == HOTKEY_BUTTON_TRAIN) {
        return entity_get_icon(shell->match_state, info.entity_type, network_get_player_id());
    } else {
        return hotkey_get_sprite(hotkey, show_toggled);
    }
}

void match_shell_render_healthbar(RenderHealthbarType type, ivec2 position, ivec2 size, int amount, int max) {
    Rect healthbar_rect = (Rect) {
        .x = position.x,
        .y = position.y,
        .w = size.x,
        .h = size.y
    };

    // Cull the healthbar
    if (!healthbar_rect.intersects(SCREEN_RECT)) {
        return;
    }

    Rect healthbar_subrect = healthbar_rect;
    healthbar_subrect.w = (healthbar_subrect.w * amount) / max;

    RenderColor healthbar_color;
    if (type == RENDER_GARRISON_BAR) {
        healthbar_color = RENDER_COLOR_WHITE;
    } else if (type == RENDER_ENERGY_BAR) {
        healthbar_color = RENDER_COLOR_BLUE;
    } else if (healthbar_subrect.w <= healthbar_rect.w / 3) {
        healthbar_color = RENDER_COLOR_RED;
    } else {
        healthbar_color = RENDER_COLOR_GREEN;
    }

    render_fill_rect(healthbar_subrect, healthbar_color);
    render_draw_rect(healthbar_rect, RENDER_COLOR_OFFBLACK);

    if (type == RENDER_GARRISON_BAR) {
        for (int line_index = 1; line_index < max; line_index++) {
            int line_x = healthbar_rect.x + ((healthbar_rect.w * line_index) / max);
            render_vertical_line(line_x, healthbar_rect.y, healthbar_rect.y + healthbar_rect.h, RENDER_COLOR_OFFBLACK);
        }
    }
}

void match_shell_render_target_build(const MatchShell* shell, const Target& target, uint8_t player_id) {
    const EntityData& building_data = entity_get_data(target.build.building_type);
    Rect building_rect = (Rect) {
        .x = (target.build.building_cell.x * TILE_SIZE) - shell->camera_offset.x,
        .y = (target.build.building_cell.y * TILE_SIZE) - shell->camera_offset.y,
        .w = building_data.cell_size * TILE_SIZE,
        .h = building_data.cell_size * TILE_SIZE
    };
    render_sprite_frame(building_data.sprite, ivec2(3, 0), ivec2(building_rect.x, building_rect.y), 0, shell->match_state.players[player_id].recolor_id);
    render_fill_rect(building_rect, RENDER_COLOR_GREEN_TRANSPARENT);
}

RenderSpriteParams match_shell_create_entity_render_params(const MatchShell* shell, const Entity& entity) {
    ivec2 params_position = entity.position.to_ivec2() - shell->camera_offset;
    RenderSpriteParams params = (RenderSpriteParams) {
        .sprite = entity_get_sprite(shell->match_state, entity),
        .frame = entity_get_animation_frame(entity),
        .position = params_position,
        .ysort_position = params_position.y,
        .options = 0,
        .recolor_id = entity_is_misc(entity.type) || entity.mode == MODE_BUILDING_DESTROYED ? 0 : shell->match_state.players[entity.player_id].recolor_id
    };
    const SpriteInfo& sprite_info = render_get_sprite_info(entity_get_sprite(shell->match_state, entity));
    if (entity_is_unit(entity.type)) {
        if (entity.mode == MODE_UNIT_BUILD) {
            const Entity& building = shell->match_state.entities.get_by_id(entity.target.id);
            const EntityData& building_data = entity_get_data(building.type);
            int building_hframe = entity_get_animation_frame(building).x;
            params.position = building.position.to_ivec2() +
                                ivec2(building_data.building_data.builder_positions_x[building_hframe],
                                    building_data.building_data.builder_positions_y[building_hframe])
                                - shell->camera_offset;
            if (building_data.building_data.builder_flip_h[building_hframe]) {
                params.options |= RENDER_SPRITE_FLIP_H;
            }
        } else {
            params.position.x -= sprite_info.frame_width / 2;
            params.position.y -= sprite_info.frame_height / 2;
            if (entity_get_data(entity.type).cell_layer == CELL_LAYER_SKY) {
                if (entity.mode == MODE_UNIT_BALLOON_DEATH) {
                    params.position.y += ((int)entity.timer * ENTITY_SKY_POSITION_Y_OFFSET) / (int)ENTITY_BALLOON_DEATH_DURATION;
                } else {
                    params.position.y += ENTITY_SKY_POSITION_Y_OFFSET;
                }
            }
            if (entity.direction > DIRECTION_SOUTH) {
                params.options |= RENDER_SPRITE_FLIP_H;
            }
        }
    }
    if (entity.type == ENTITY_SWITCH) {
        params.position.y += ENTITY_SWITCH_POSITION_Y_OFFSET;
    }

    return params;
}

void match_shell_render_entity_select_rings_and_healthbars(const MatchShell* shell, const Entity& entity) {
    const int HEALTHBAR_HEIGHT = 4;
    const int HEALTHBAR_PADDING = 4;

    const EntityData& entity_data = entity_get_data(entity.type);
    Rect entity_rect = entity_get_rect(entity);

    // Render select ring
    bool use_red_select_ring = shell->replay_mode || entity_is_misc(entity.type)
                                    ? false
                                    : shell->match_state.players[entity.player_id].team != shell->match_state.players[network_get_player_id()].team;
    SpriteName select_ring_sprite = match_shell_get_entity_select_ring(entity.type, use_red_select_ring);
    ivec2 entity_center_position = entity_is_unit(entity.type)
            ? entity.position.to_ivec2()
            : ivec2(entity_rect.x + (entity_rect.w / 2), entity_rect.y + (entity_rect.h / 2));
    entity_center_position -= shell->camera_offset;
    if (entity_data.cell_layer == CELL_LAYER_SKY) {
        entity_center_position.y += ENTITY_SKY_POSITION_Y_OFFSET;
    }
    render_sprite_frame(select_ring_sprite, ivec2(0, 0), entity_center_position, RENDER_SPRITE_CENTERED, 0);

    // Render healthbar
    ivec2 healthbar_position = ivec2(entity_rect.x, entity_rect.y + entity_rect.h + HEALTHBAR_PADDING) - shell->camera_offset;
    if (entity_data.max_health != 0) {
        match_shell_render_healthbar(RENDER_HEALTHBAR, healthbar_position, ivec2(entity_rect.w, HEALTHBAR_HEIGHT), entity.health, entity_data.max_health);
        healthbar_position.y += HEALTHBAR_HEIGHT + 1;
    }

    // Render garrison bar
    if (entity_data.garrison_capacity != 0 && (entity.type == ENTITY_GOLDMINE || shell->replay_mode ||
            shell->match_state.players[entity.player_id].team == shell->match_state.players[network_get_player_id()].team)) {
        match_shell_render_healthbar(RENDER_GARRISON_BAR, healthbar_position, ivec2(entity_rect.w, HEALTHBAR_HEIGHT), (int)entity.garrisoned_units.size(), (int)entity_data.garrison_capacity);
        healthbar_position.y += HEALTHBAR_HEIGHT + 1;
    }
    // Render energy bar
    if (entity_is_unit(entity.type) && entity_data.unit_data.max_energy != 0) {
        match_shell_render_healthbar(RENDER_ENERGY_BAR, healthbar_position, ivec2(entity_rect.w, HEALTHBAR_HEIGHT), (int)entity.energy, (int)entity_data.unit_data.max_energy);
    }
}

void match_shell_render_entity_icon(const MatchShell* shell, const Entity& entity, Rect icon_rect) {
    const EntityData& entity_data = entity_get_data(entity.type);
    bool icon_hovered =
        match_shell_is_camera_free(shell) &&
        !match_shell_is_selecting(shell) &&
        icon_rect.has_point(input_get_mouse_position());

    render_sprite_frame(SPRITE_UI_ICON_BUTTON, ivec2(icon_hovered ? 1 : 0, 0), ivec2(icon_rect.x, icon_rect.y - (int)icon_hovered), RENDER_SPRITE_NO_CULL, 0);
    render_sprite_frame(entity_get_icon(shell->match_state, entity.type, entity.player_id), ivec2(icon_hovered ? 1 : 0, 0), ivec2(icon_rect.x, icon_rect.y - (int)icon_hovered), RENDER_SPRITE_NO_CULL, 0);
    ivec2 healthbar_position = ivec2(icon_rect.x + 1, icon_rect.y + 27 - (int)icon_hovered);
    ivec2 healthbar_size = ivec2(30, 4);
    if (entity_is_unit(entity.type) && entity_data.unit_data.max_energy != 0) {
        match_shell_render_healthbar(RENDER_ENERGY_BAR, healthbar_position, healthbar_size, entity.energy, entity_data.unit_data.max_energy);
        healthbar_position -= ivec2(0, healthbar_size.y + 1);
    }
    match_shell_render_healthbar(RENDER_HEALTHBAR, healthbar_position, healthbar_size, entity.health, entity_data.max_health);
}

void match_shell_render_entity_move_animation(const MatchShell* shell, const Entity& entity, Animation move_animation) {
    Rect entity_rect = entity_get_rect(entity);
    ivec2 entity_center_position = entity_is_unit(entity.type)
            ? entity.position.to_ivec2()
            : ivec2(entity_rect.x + (entity_rect.w / 2), entity_rect.y + (entity_rect.h / 2));
    entity_center_position -= shell->camera_offset;
    if (entity_get_data(entity.type).cell_layer == CELL_LAYER_SKY) {
        entity_center_position.y += ENTITY_SKY_POSITION_Y_OFFSET;
    }

    render_sprite_frame(match_shell_get_entity_select_ring(entity.type, move_animation.name == ANIMATION_UI_MOVE_ATTACK_ENTITY), ivec2(0, 0), entity_center_position, RENDER_SPRITE_CENTERED, 0);
}

void match_shell_render_particle(const MatchShell* shell, const Particle& particle) {
    // Check if particle can be seen by player
    if (!match_shell_is_cell_rect_revealed(shell, particle.position / TILE_SIZE, 1)) {
        return;
    }

    render_sprite_frame(particle.sprite, ivec2(particle.animation.frame.x, particle.vframe), particle.position - shell->camera_offset, RENDER_SPRITE_CENTERED, 0);
}

bool match_shell_should_render_hotkey_toggled(const MatchShell* shell, InputHotkey hotkey) {
    const HotkeyButtonInfo& hotkey_info = hotkey_get_button_info(hotkey);
    if (hotkey_info.type != HOTKEY_BUTTON_TOGGLED_ACTION) {
        return false;
    }

    const Entity& entity = shell->match_state.entities.get_by_id(shell->selection[0]);
    if (hotkey == INPUT_HOTKEY_CAMO && entity_check_flag(entity, ENTITY_FLAG_INVISIBLE)) {
        return true;
    }

    return false;
}

const char* match_shell_render_get_stat_tooltip(SpriteName sprite) {
    switch (sprite) {
        case SPRITE_UI_STAT_ICON_DETECTION:
            return "Detection";
        default:
            log_warn("Unhandled stat tooltip icon of %u", sprite);
            return "";
    }
}

void match_shell_render_tooltip(const MatchShell* shell, InputHotkey hotkey) {
    const HotkeyButtonInfo& hotkey_info = hotkey_get_button_info(hotkey);
    const bool show_toggle = match_shell_should_render_hotkey_toggled(shell, hotkey);

    // Write tooltip text
    char tooltip_text[64];
    char* tooltip_text_ptr = tooltip_text;
    tooltip_text_ptr += hotkey_get_name(tooltip_text, hotkey, show_toggle);
    tooltip_text_ptr += sprintf(tooltip_text_ptr, " (");
    tooltip_text_ptr += input_sprintf_sdl_scancode_str(tooltip_text_ptr, input_get_hotkey_mapping(hotkey));
    tooltip_text_ptr += sprintf(tooltip_text_ptr, ")");

    // Write tooltip desc
    char tooltip_desc[128];
    sprintf(tooltip_desc, "%s", hotkey_get_desc(hotkey));

    // Determine gold, population, and energy cost
    uint32_t tooltip_gold_cost = 0;
    uint32_t tooltip_population_cost = 0;
    uint32_t tooltip_energy_cost = 0;
    switch (hotkey_info.type) {
        case HOTKEY_BUTTON_ACTION:
        case HOTKEY_BUTTON_TOGGLED_ACTION: {
            if (hotkey == INPUT_HOTKEY_MOLOTOV) {
                tooltip_energy_cost = MOLOTOV_ENERGY_COST;
            } else if (hotkey == INPUT_HOTKEY_CAMO) {
                tooltip_energy_cost = CAMO_ENERGY_COST;
            }
            break;
        }
        case HOTKEY_BUTTON_TRAIN:
        case HOTKEY_BUTTON_BUILD: {
            const EntityData& entity_data = entity_get_data(hotkey_info.entity_type);
            bool costs_energy = entity_is_building(hotkey_info.entity_type)
                                    ? (entity_data.building_data.options & BUILDING_COSTS_ENERGY) == BUILDING_COSTS_ENERGY
                                    : false;
            if (costs_energy) {
                tooltip_energy_cost = entity_data.gold_cost;
            } else {
                tooltip_gold_cost = entity_data.gold_cost;
            }
            tooltip_population_cost = hotkey_info.type == HOTKEY_BUTTON_TRAIN
                ? entity_data.unit_data.population_cost : 0;
            break;
        }
        case HOTKEY_BUTTON_RESEARCH: {
            const UpgradeData& upgrade_data = upgrade_get_data(hotkey_info.upgrade);
            tooltip_gold_cost = upgrade_data.gold_cost;
            break;
        }
    }

    // If requirements not met, overwrite tooltip desc with requirements text
    if (!match_shell_does_player_meet_hotkey_requirements(shell->match_state, hotkey)) {
        switch (hotkey_info.requirements.type) {
            case HOTKEY_REQUIRES_NONE: {
                GOLD_ASSERT(false);
                break;
            }
            case HOTKEY_REQUIRES_BUILDING: {
                sprintf(tooltip_desc, "Requires %s", entity_get_data(hotkey_info.requirements.building).name);
                break;
            }
            case HOTKEY_REQUIRES_UPGRADE: {
                sprintf(tooltip_desc, "Requires %s", upgrade_get_data(hotkey_info.requirements.upgrade).name);
                break;
            }
        }
        tooltip_gold_cost = 0;
        tooltip_population_cost = 0;
        tooltip_energy_cost = 0;
    }

    // Determine tooltip size
    const bool tooltip_has_desc = tooltip_desc[0] != '\0';
    int tooltip_text_width = render_get_text_size(FONT_WESTERN8_OFFBLACK, tooltip_text).x;
    if (tooltip_has_desc) {
        tooltip_text_width = std::max(tooltip_text_width, render_get_text_size(FONT_HACK_OFFBLACK, tooltip_desc).x);
    }
    int tooltip_min_width = 10 + tooltip_text_width;
    int tooltip_cell_width = tooltip_min_width / 8;
    int tooltip_cell_height = 3;
    if (tooltip_gold_cost != 0 || tooltip_energy_cost != 0) {
        tooltip_cell_height += 2;
    }
    if (tooltip_has_desc) {
        tooltip_cell_height += 2;
    }
    if (tooltip_min_width % 8 != 0) {
        tooltip_cell_width++;
    }

    // Render tooptip background
    const ivec2 tooltip_top_left = ivec2(
        SCREEN_WIDTH - (tooltip_cell_width * 8) - 2,
        BUTTON_PANEL_RECT.y - (tooltip_cell_height * 8) - 2
    );
    for (int y = 0; y < tooltip_cell_height; y++) {
        for (int x = 0; x < tooltip_cell_width; x++) {
            ivec2 frame;
            if (x == 0) {
                frame.x = 0;
            } else if (x == tooltip_cell_width - 1) {
                frame.x = 2;
            } else {
                frame.x = 1;
            }
            if (y == 0) {
                frame.y = 0;
            } else if (y == tooltip_cell_height - 1) {
                frame.y = 2;
            } else {
                frame.y = 1;
            }

            render_sprite_frame(SPRITE_UI_TOOLTIP_FRAME, frame, tooltip_top_left + (ivec2(x, y) * 8), RENDER_SPRITE_NO_CULL, 0);
        }
    }

    // Render tooltip text
    int tooltip_item_y = 5;
    render_text(FONT_WESTERN8_OFFBLACK, tooltip_text, tooltip_top_left + ivec2(5, tooltip_item_y));
    tooltip_item_y = 21;
    if (tooltip_has_desc) {
        render_text(FONT_HACK_OFFBLACK, tooltip_desc, tooltip_top_left + ivec2(5, 5 + 16));
        tooltip_item_y = 35;
    }

    // Render gold icon and text
    if (tooltip_gold_cost != 0) {
        render_sprite_frame(SPRITE_UI_GOLD_ICON, ivec2(0, 0), tooltip_top_left + ivec2(5, tooltip_item_y), RENDER_SPRITE_NO_CULL, 0);
        char gold_text[4];
        sprintf(gold_text, "%u", tooltip_gold_cost);
        render_text(FONT_WESTERN8_OFFBLACK, gold_text, tooltip_top_left + ivec2(23, tooltip_item_y));
    }

    // Render population icon and text
    if (tooltip_population_cost != 0) {
        render_sprite_frame(SPRITE_UI_HOUSE_ICON, ivec2(0, 0), tooltip_top_left + ivec2(5 + 18 + 32, tooltip_item_y - 2), RENDER_SPRITE_NO_CULL, 0);
        char population_text[4];
        sprintf(population_text, "%u", tooltip_population_cost);
        render_text(FONT_WESTERN8_OFFBLACK, population_text, tooltip_top_left + ivec2(5 + 18 + 32 + 22, tooltip_item_y + 2));
    }

    // Render energy text
    if (tooltip_energy_cost != 0) {
        render_sprite_frame(SPRITE_UI_ENERGY_ICON, ivec2(0, 0), tooltip_top_left + ivec2(5, tooltip_item_y), RENDER_SPRITE_NO_CULL, 0);
        char energy_text[4];
        sprintf(energy_text, "%u", tooltip_energy_cost);
        render_text(FONT_WESTERN8_OFFBLACK, energy_text, tooltip_top_left + ivec2(22, tooltip_item_y));
    }
}

ivec2 match_shell_get_queued_target_position(const MatchShell* shell, const Target& target) {
    switch (target.type) {
        case TARGET_CELL:
        case TARGET_ATTACK_CELL:
        case TARGET_UNLOAD:
        case TARGET_MOLOTOV:
            return (target.cell * TILE_SIZE) + ivec2(TILE_SIZE / 2, TILE_SIZE / 2);
        case TARGET_ENTITY:
        case TARGET_ATTACK_ENTITY:
        case TARGET_REPAIR: {
            uint32_t target_index = shell->match_state.entities.get_index_of(target.id);
            if (target_index == INDEX_INVALID) {
                return ivec2(-1, -1);
            }

            const Entity& target_entity = shell->match_state.entities[target_index];
            if (entity_is_unit(target_entity.type)) {
                return target_entity.position.to_ivec2();
            }
            const EntityData& target_data = entity_get_data(target_entity.type);
            return (target_entity.cell * TILE_SIZE) + (ivec2(target_data.cell_size, target_data.cell_size) * TILE_SIZE / 2);
        }
        case TARGET_NONE:
        case TARGET_BUILD:
        case TARGET_BUILD_ASSIST:
        case TARGET_PATROL:
            return ivec2(-1, -1);
        case TARGET_TYPE_COUNT:
            GOLD_ASSERT(false);
            return ivec2(-1, -1);
    }
}

FireCellRender match_shell_get_fire_cell_render(const MatchShell* shell, const Fire& fire) {
    if (!match_shell_is_cell_rect_revealed(shell, fire.cell, 1)) {
        return FIRE_CELL_DO_NOT_RENDER;
    }
    Cell map_fire_cell = map_get_cell(shell->match_state.map, CELL_LAYER_GROUND, fire.cell);
    if (map_fire_cell.type == CELL_EMPTY) {
        return FIRE_CELL_RENDER_BELOW;
    }
    if (map_fire_cell.type == CELL_BUILDING) {
        return FIRE_CELL_DO_NOT_RENDER;
    }
    if (map_fire_cell.type == CELL_DECORATION) {
        return FIRE_CELL_RENDER_ABOVE;
    }
    if (!(map_fire_cell.type == CELL_UNIT || map_fire_cell.type == CELL_MINER)) {
        return FIRE_CELL_DO_NOT_RENDER;
    }

    const Entity& unit = shell->match_state.entities.get_by_id(map_fire_cell.id);

    if (unit.mode == MODE_UNIT_DEATH_FADE) {
        return FIRE_CELL_RENDER_ABOVE;
    }
    bool is_bottom_row = fire.cell.y == unit.cell.y + (entity_get_data(unit.type).cell_size - 1);
    if (!is_bottom_row) {
        return FIRE_CELL_RENDER_BELOW;
    }
    if (unit.mode == MODE_UNIT_MOVE && DIRECTION_IVEC2[unit.direction].y == -1) {
        return FIRE_CELL_RENDER_BELOW;
    }
    return FIRE_CELL_RENDER_ABOVE;
}

MinimapPixel match_shell_get_minimap_pixel_for_cell(const MatchShell* shell, ivec2 cell) {
    switch (map_get_tile(shell->match_state.map, cell).sprite) {
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

MinimapPixel match_shell_get_minimap_pixel_for_entity(const MatchShell* shell, const Entity& entity) {
    if (entity_is_misc(entity.type)) {
        return MINIMAP_PIXEL_GOLD;
    }
    if (entity_check_flag(entity, ENTITY_FLAG_DAMAGE_FLICKER)) {
        return MINIMAP_PIXEL_WHITE;
    }
    return (MinimapPixel)(MINIMAP_PIXEL_PLAYER0 + shell->match_state.players[entity.player_id].recolor_id);
}

// Render debug

#ifdef GOLD_DEBUG

void match_shell_debug_render_cell_region_lines(const MatchShell* shell, ivec2 base_coords, ivec2 base_pos, uint32_t render_elevation, ivec2 cell) {
    const int map_index = (base_coords.x + cell.x) + ((base_coords.y + cell.y) * shell->match_state.map.width);
    if (!shell->debug_show_region_lines || shell->match_state.map.tiles[map_index].elevation != render_elevation) {
        return;
    }

    for (int direction = 0; direction < DIRECTION_COUNT; direction++) {
        ivec2 neighbor = base_coords + cell + DIRECTION_IVEC2[direction];
        if (!map_is_cell_in_bounds(shell->match_state.map, neighbor) || map_get_region(shell->match_state.map, neighbor) == shell->match_state.map.regions[map_index]) {
            continue;
        }
        ivec2 tile_pos = base_pos + ivec2(cell.x * TILE_SIZE, cell.y * TILE_SIZE);
        if (direction == DIRECTION_NORTH) {
            render_draw_rect((Rect) {
                .x = tile_pos.x, .y = tile_pos.y,
                .w = TILE_SIZE, .h = 1
            }, RENDER_COLOR_WHITE);
        } else if (direction == DIRECTION_SOUTH) {
            render_draw_rect((Rect) {
                .x = tile_pos.x, .y = tile_pos.y + TILE_SIZE - 1,
                .w = TILE_SIZE, .h = 1
            }, RENDER_COLOR_WHITE);
        } else if (direction == DIRECTION_WEST) {
            render_draw_rect((Rect) {
                .x = tile_pos.x, .y = tile_pos.y,
                .w = 1, .h = TILE_SIZE
            }, RENDER_COLOR_WHITE);
        } else if (direction == DIRECTION_EAST) {
            render_draw_rect((Rect) {
                .x = tile_pos.x + TILE_SIZE - 1, .y = tile_pos.y,
                .w = 1, .h = TILE_SIZE
            }, RENDER_COLOR_WHITE);
        }
    }
}

#else

void match_shell_debug_render_cell_region_lines(const MatchShell* shell, ivec2 base_coords, ivec2 base_pos, uint32_t render_elevation, ivec2 cell) {}

#endif
