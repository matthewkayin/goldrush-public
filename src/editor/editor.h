#pragma once

#include "match/scenario/scenario.h"
#include <SDL3/SDL.h>
#include <string>

void editor_init(SDL_Window* window);
void editor_quit();
void editor_update();

bool editor_requests_playtest();
void editor_begin_playtest();
void editor_end_playtest();
const Scenario* editor_get_scenario();
std::string editor_get_scenario_script_path();

void editor_render();
