#include "road_data.h"

#include "defines.h"
#include "core/resource.h"
#include "core/logger.h"

static const uint32_t ROAD_DATA_SIGNATURE = 0x76233282;

#ifdef GOLD_DEBUG

#include "util/math.h"
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstdio>
#include <vector>
#include <queue>

void render_generate_road_data() {
    SDL_Surface* roads_surface = SDL_LoadPNG("../res/sprite/ui/roads.png");
    if (roads_surface == NULL) {
        printf("Failed to load roads image: %s\n", SDL_GetError());
    }

    const SDL_PixelFormatDetails* format_details = SDL_GetPixelFormatDetails(roads_surface->format);
    const uint32_t PIXEL_NO_ROAD = SDL_MapRGBA(format_details, NULL, 0, 0, 0, 0);

    const uint32_t* pixels = (uint32_t*)roads_surface->pixels;
    const int width = roads_surface->w;
    const int height = roads_surface->h;

    // Find all the road starts
    std::vector<ivec2> road_starts;
    for (int y = 0; y < height - 1; y++) {
        for (int x = 0; x < width - 1; x++) {
            uint8_t r, g, b, a;
            SDL_GetRGBA(pixels[x + (y * width)], format_details, NULL, &r, &g, &b, &a);

            // Only choose road starts
            if (r != 255) {
                continue;
            }

            // Only choose the top-left of the road start
            const bool is_pixel_the_top_left_of_the_start =
                pixels[(x + 1) + (y * width)] == pixels[x + (y * width)] &&
                pixels[x + ((y + 1) * width)] == pixels[x + (y * width)] &&
                pixels[(x + 1) + ((y + 1) * width)] == pixels[x + (y * width)];
            if (!is_pixel_the_top_left_of_the_start) {
                continue;
            }

            road_starts.push_back(ivec2(x, y));
        }
    }

    // Create the roads
    // Roads array is initialized with road_starts.size(),
    // this way we can insert into the array in order of road number
    std::vector<std::vector<ivec2>> roads(road_starts.size());
    for (const ivec2 road_start : road_starts) {
        uint8_t r, g, b, a;
        SDL_GetRGBA(pixels[road_start.x + (road_start.y * width)], format_details, NULL, &r, &g, &b, &a);
        const uint32_t road_number = (uint32_t)g;

        std::queue<ivec2> frontier;
        std::vector<bool> explored(width * height, false);
        frontier.push(road_start);

        printf("Exploring road %u start <%i, %i>\n", road_number, road_start.x, road_start.y);

        while (!frontier.empty()) {
            ivec2 next = frontier.front();
            frontier.pop();

            if (explored[next.x + (next.y * width)]) {
                // printf("Next <%i, %i> is explored\n", next.x, next.y);
                continue;
            }
            if (pixels[next.x + (next.y * width)] == PIXEL_NO_ROAD) {
                // printf("Next <%i, %i> is not transparent\n", next.x, next.y);
                continue;
            }

            const bool is_2x2 =
                pixels[next.x + (next.y * width)] != PIXEL_NO_ROAD &&
                pixels[(next.x + 1) + (next.y * width)] != PIXEL_NO_ROAD &&
                pixels[next.x + ((next.y + 1) * width)] != PIXEL_NO_ROAD &&
                pixels[(next.x + 1) + ((next.y + 1) * width)] != PIXEL_NO_ROAD;
            if (is_2x2) {
                roads[road_number].push_back(next);
                printf("Next <%i, %i> added to road\n", next.x, next.y);
            }

            explored[next.x + (next.y * width)] = true;

            const Direction directions_to_search[] = {
                DIRECTION_NORTH, DIRECTION_EAST, DIRECTION_SOUTH, DIRECTION_WEST,
                DIRECTION_NORTHEAST, DIRECTION_SOUTHEAST, DIRECTION_SOUTHWEST, DIRECTION_NORTHWEST
            };
            for (int index = 0; index < DIRECTION_COUNT; index++) {
                Direction direction = directions_to_search[index];
                ivec2 child = next + DIRECTION_IVEC2[direction];
                if (child.x < 0 || child.y < 0 || child.x >= width - 1 || child.y >= height - 1) {
                    continue;
                }
                frontier.push(child);
            }
        } // end while frontier not empty
    } // end for each road_start

    SDL_DestroySurface(roads_surface);

    // Write roads data file
    FILE* file = fopen("../res/sprite/ui/roads.dat", "wb");
    if (!file) {
        printf("Failed to open roads.dat for writing.\n");
        return;
    }

    // Signature
    fwrite(&ROAD_DATA_SIGNATURE, 1, sizeof(ROAD_DATA_SIGNATURE), file);

    // Road count
    uint32_t road_count = roads.size();
    fwrite(&road_count, 1, sizeof(road_count), file);
    printf("Road count %u\n", road_count);

    // Roads
    for (uint32_t road_index = 0; road_index < road_count; road_index++) {
        // Road size
        uint32_t road_size = roads[road_index].size();
        fwrite(&road_size, 1, sizeof(road_size), file);
        printf("Road %u has size %u\n", road_index, road_size);

        // Road points
        for (uint32_t index = 0; index < roads[road_index].size(); index++) {
            fwrite(&roads[road_index][index], 1, sizeof(roads[road_index][index]), file);
        }
    }

    fclose(file);
    printf("Wrote ../res/sprite/ui/roads.dat.\n");
}

#else

void menu_generate_road_data();

#endif

bool render_load_road_data(std::vector<std::vector<ivec2>>* points) {
    void* road_data = resource_load(RESOURCE_DATA_CAMPAIGN_ROADS);
    if (!road_data) {
        GOLD_ASSERT(false);
        return false;
    }

    uint8_t* road_data_ptr = (uint8_t*)road_data;

    // Signature
    if (*(uint32_t*)road_data_ptr != ROAD_DATA_SIGNATURE) {
        log_error("Road data signature of %u is invalid.", *(uint32_t*)road_data_ptr);
        return false;
    }
    road_data_ptr += sizeof(uint32_t);

    // Road count
    uint32_t road_count = *(uint32_t*)road_data_ptr;
    road_data_ptr += sizeof(road_count);

    *points = std::vector<std::vector<ivec2>>(road_count);

    for (uint32_t road_index = 0; road_index < road_count; road_index++) {
        // Road size
        uint32_t road_size = *(uint32_t*)road_data_ptr;
        road_data_ptr += sizeof(road_size);

        // Road points
        for (uint32_t index = 0; index < road_size; index++) {
            (*points)[road_index].push_back(*(ivec2*)road_data_ptr);
            road_data_ptr += sizeof(ivec2);
        }
    }

    free(road_data);
    return true;
}
