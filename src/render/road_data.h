#pragma once

#include "util/math.h"
#include <vector>

void render_generate_road_data();
bool render_load_road_data(std::vector<std::vector<ivec2>>* points);
