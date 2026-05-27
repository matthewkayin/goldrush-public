#include "core/ui.h"
#include "menu.h"

#include "core/sound.h"
#include "menu/types.h"
#include "render/sprite.h"

struct CampaignScenarioInfo {
    const char* name;
    const char* description;
};

static const uint32_t CAMPAIGN_MISSION_NONE = UINT32_MAX;
static const uint32_t CAMPAIGN_SCENARIO_COUNT = 12U;
static const ivec2 CAMPAIGN_MAP_ORIGIN = ivec2(16, 8);

static const CampaignScenarioInfo CAMPAIGN_SCENARIO_INFO[CAMPAIGN_SCENARIO_COUNT] = {
    // Scenario 1
    (CampaignScenarioInfo) {
        .name = "First Arrivals",
        .description = "You've traveled miles over barren country to get here. Now that you're finally out west, it's time to find some gold and make your fortune.",
    },
    // Scenario 2
    (CampaignScenarioInfo) {
        .name = "The Bandit Camp",
        .description = "The nearby bandits have been a thorn in our side for long enough. Raise an army and force them out, but don't expect them to go quietly.",
    },
    // Scenario 3
    (CampaignScenarioInfo) {
        .name = "Race to Riches",
        .description = "Rival prospectors are layin' claim to every gold mine in the area. You need to push them back, and fast, or there won't be any gold left for yourself."
    },
    // Scenario 4
    (CampaignScenarioInfo) {
        .name = "King of the Hill",
        .description = "You've got the high ground and two of the best gold mines around. Now the enemy comes to take it from you. Hold them off!"
    },
    // Scenario 5
    (CampaignScenarioInfo) {
        .name = "Stolen Fortune",
        .description = "Your mine has collapsed and bandits have swiped up every ounce of gold you had. Fight your way through twisting tunnels and steal your fortune back."
    },
    // Scenario 6
    (CampaignScenarioInfo) {
        .name = "Sheriff Duty",
        .description = "The settlements in the valley are getting torn apart by raiders. Defend the villagers and drive the bandits out."
    },
    // Scenario 7
    (CampaignScenarioInfo) {
        .name = "Cut the Line",
        .description = "The enemy's sending in hired guns by the wagonload. Cut their reinforcements off before their numbers overwhelm you!"
    },
    // Scenario 8
    (CampaignScenarioInfo) {
        .name = "The Onion Defense",
        .description = "The enemy's tucked in behind layers of defenses. No flanking tricks will save you here. It's time to bring in the big guns."
    },
    // Scenario 9
    (CampaignScenarioInfo) {
        .name = "The Heads of the Snake",
        .description = "Enemy cannons have you boxed in! If you can take out their generals, their position will crumble, but to do that, you'll need to sneak behind enemy lines..."
    },
    // Scenario 10
    (CampaignScenarioInfo) {
        .name = "The Frozen Country",
        .description = "In the cold north, every gold mine has run dry. If you want to survive out here, you'll need to scavenge for the gold the settlers have hoarded away."
    },
    // Scenario 11
    (CampaignScenarioInfo) {
        .name = "Avalanche!",
        .description = "Some prospectors are fixin' to blow open a gold mine, but the blast will cause an avalanche that will destroy your town. Send in a force to stop them before it's too late!"
    },
    // Scenario 12
    (CampaignScenarioInfo) {
        .name = "The Final Frontier",
        .description = "This is your last stand. Rich gold mines lie in the center, along with two rivals bent on taking them from you. Show them who this territory belongs to."
    }
};

static const ivec2 CAMPAIGN_SCENARIO_ORB_POSITION[CAMPAIGN_SCENARIO_COUNT] = {
    ivec2(140, 254),
    ivec2(111, 234),
    ivec2(16, 144),
    ivec2(65, 152),
    ivec2(108, 125),
    ivec2(167, 155),
    ivec2(145, 111),
    ivec2(91, 89),
    ivec2(29, 83),
    ivec2(64, 30),
    ivec2(121, 52),
    ivec2(53, 10)
};

static const Rect CAMPAIGN_SIDEBAR = (Rect) {
    .x = 16 + 229 + 4,
    .y = 8,
    .w = 376,
    .h = 128
};

void menu_campaign_init(MenuState* state) {
    state->campaign_road_reveal_timer = 0;
    state->campaign_mission_selected = CAMPAIGN_MISSION_NONE;
}

void menu_campaign_update(MenuState* state) {
    const uint32_t selected_save_index = menu_campaign_list_get_selected_campaign_save(state);
    CampaignSaveEntry& selected_save = state->campaign_saves[selected_save_index];
    const uint32_t available_scenario_count = std::min(selected_save.missions_completed + 1, CAMPAIGN_SCENARIO_COUNT);
    for (uint32_t index = 0; index < available_scenario_count; index++) {
        ui_element_position(state->ui_context, CAMPAIGN_MAP_ORIGIN + CAMPAIGN_SCENARIO_ORB_POSITION[index]);
        SpriteName button_sprite = index < selected_save.missions_completed ||
                (state->campaign_road_reveal_timer != 0 && index == selected_save.missions_completed)
            ? SPRITE_UI_CAMPAIGN_SCENARIO_ORB_GREEN
            : SPRITE_UI_CAMPAIGN_SCENARIO_ORB_BLUE;
        if (ui_campaign_orb(state->ui_context, button_sprite, state->campaign_road_reveal_timer != 0, state->campaign_mission_selected == index)) {
            state->campaign_mission_selected = index;
        }
    }

    if (state->campaign_road_reveal_timer != 0) {
        state->campaign_road_reveal_timer--;

        if (state->campaign_road_reveal_timer == 0) {
            sound_stop(state->campaign_road_reveal_sound_track_index);
            state->campaign_road_reveal_sound_track_index = SOUND_NOT_PLAYING;
            sound_play(SOUND_FLAG_THUMP);

            selected_save.missions_completed++;
            state->campaign_mission_selected = selected_save.missions_completed;
            menu_save_campaign_saves(state);
        }
    }
    if (state->campaign_road_reveal_timer != 0 && !menu_campaign_is_in_road_reveal_delay(state) && state->campaign_road_reveal_sound_track_index == SOUND_NOT_PLAYING) {
        state->campaign_road_reveal_sound_track_index = sound_play(SOUND_PEN_SCRATCH, true);
    }

    // Sidebar
    ui_frame_rect(state->ui_context, CAMPAIGN_SIDEBAR);

    ivec2 header_text_size = render_get_text_size(FONT_HACK_GOLD, selected_save.name);
    ui_element_position(state->ui_context, ivec2(CAMPAIGN_SIDEBAR.x + (CAMPAIGN_SIDEBAR.w / 2) - (header_text_size.x / 2), 14));
    ui_text(state->ui_context, FONT_HACK_GOLD, selected_save.name);

    if (state->campaign_mission_selected != CAMPAIGN_MISSION_NONE) {
        const CampaignScenarioInfo& scenario_info = CAMPAIGN_SCENARIO_INFO[state->campaign_mission_selected];

        ui_begin_column(state->ui_context, ivec2(CAMPAIGN_SIDEBAR.x + 8, CAMPAIGN_SIDEBAR.y + 28), 4);
        ui_element_size(state->ui_context, ivec2(0, 22));
            char title_text[128];
            sprintf(title_text, "#%u - %s", state->campaign_mission_selected + 1, scenario_info.name);
            ui_text(state->ui_context, FONT_HACK_GOLD, title_text);

            const size_t max_line_length = 56;
            std::string description = std::string(scenario_info.description);
            std::string line;
            while (!description.empty()) {
                size_t space_index = description.find_first_of(' ');
                std::string next_word;
                if (space_index == std::string::npos) {
                    next_word = description;
                    description = "";
                } else {
                    next_word = description.substr(0, space_index);
                    description = description.substr(space_index + 1);
                }

                if (line.size() + next_word.size() > max_line_length) {
                    ui_text(state->ui_context, FONT_HACK_GOLD, line.c_str());
                    line = "";
                }

                if (line.empty()) {
                    line = next_word;
                } else {
                    line += " " + next_word;
                }
            }
            if (!line.empty()) {
                ui_text(state->ui_context, FONT_HACK_GOLD, line.c_str());
            }
        ui_end_container(state->ui_context);
    }

    // Button row
    if (state->campaign_road_reveal_timer == 0) {
        ui_begin_row(state->ui_context, ivec2(CAMPAIGN_SIDEBAR.x + 4, CAMPAIGN_SIDEBAR.y + CAMPAIGN_SIDEBAR.h + 4), 4);
            if (ui_button(state->ui_context, "Back")) {
                menu_set_mode(state, MENU_MODE_CAMPAIGN_LIST);
            }
            if (state->campaign_mission_selected != CAMPAIGN_MISSION_NONE) {
                if (ui_button(state->ui_context, "Play")) {
                    menu_set_mode(state, MENU_MODE_LOAD_SCENARIO_COUNTDOWN);
                }
            }
        ui_end_container(state->ui_context);
    }

#ifdef GOLD_DEBUG
    if (input_is_action_just_pressed(INPUT_ACTION_SPACE) && state->campaign_road_reveal_timer == 0) {
        menu_campaign_begin_road_reveal(state);
    }
#endif
}

void menu_campaign_on_scenario_finished(MenuState* state, uint32_t playtime_seconds, bool victory) {
    uint32_t selected_save_index = menu_campaign_list_get_selected_campaign_save(state);
    CampaignSaveEntry& selected_save = state->campaign_saves[selected_save_index];
    selected_save.playtime_seconds += playtime_seconds;

    if (victory && selected_save.missions_completed == state->campaign_mission_selected) {
        state->mode = MENU_MODE_CAMPAIGN;
        state->music_begin_timer = 15U;
        if (state->campaign_mission_selected == CAMPAIGN_SCENARIO_COUNT - 1) {
            selected_save.missions_completed++;
            menu_save_campaign_saves(state);
            menu_set_mode(state, MENU_MODE_CREDITS);
        } else {
            menu_campaign_begin_road_reveal(state);
        }
        return;
    }

    menu_set_mode(state, MENU_MODE_CAMPAIGN);
}

uint32_t menu_campaign_get_roads_revealed(const MenuState* state) {
    uint32_t selected_save_index = menu_campaign_list_get_selected_campaign_save(state);
    return state->campaign_saves[selected_save_index].missions_completed;
}

void menu_campaign_begin_road_reveal(MenuState* state) {
    uint32_t road_number = menu_campaign_get_roads_revealed(state);

    // Determine the road reveal length
    uint32_t largest_road_length = 0;
    for (uint32_t road_index = 0; road_index < CAMPAIGN_SCENARIO_COUNT - 1; road_index++) {
        largest_road_length = std::max(largest_road_length, render_get_road_length(road_index));
    }
    float percent_of_largest = (float)render_get_road_length(road_number) / (float)largest_road_length;
    uint32_t seconds = std::max((uint32_t)(3.0 * percent_of_largest), 1U);
    state->campaign_road_reveal_duration = 60U * seconds;
    state->campaign_road_reveal_timer = state->campaign_road_reveal_duration + 15U;
}

bool menu_campaign_is_in_road_reveal_delay(const MenuState* state) {
    return state->campaign_road_reveal_timer != 0 && state->campaign_road_reveal_timer > state->campaign_road_reveal_duration;
}

void menu_campaign_render(const MenuState* state) {
    render_sprite_frame(SPRITE_UI_CAMPAIGN_MAP, ivec2(0, 0), CAMPAIGN_MAP_ORIGIN, RENDER_SPRITE_NO_CULL, 0);

    for (uint32_t road_number = 0; road_number < menu_campaign_get_roads_revealed(state); road_number++) {
        render_road(road_number, CAMPAIGN_MAP_ORIGIN, 1.0f);
    }

    if (state->campaign_road_reveal_timer != 0 && !menu_campaign_is_in_road_reveal_delay(state)) {
        float percent = (float)(state->campaign_road_reveal_duration - state->campaign_road_reveal_timer) / (float)state->campaign_road_reveal_duration;
        render_road(menu_campaign_get_roads_revealed(state), CAMPAIGN_MAP_ORIGIN, percent);
    }
}
