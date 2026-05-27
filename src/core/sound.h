#pragma once

#include "core/resource.h"
#include <cstdint>

#define SOUND_NOT_PLAYING UINT32_MAX

enum SoundName {
    SOUND_UI_CLICK,
    SOUND_DEATH,
    SOUND_MUSKET,
    SOUND_GUN,
    SOUND_EXPLOSION,
    SOUND_PICKAXE,
    SOUND_HAMMER,
    SOUND_BUILDING_PLACE,
    SOUND_SWORD,
    SOUND_DEATH_CHICKEN,
    SOUND_CANNON,
    SOUND_BUILDING_DESTROY,
    SOUND_BUNKER_DESTROY,
    SOUND_MINE_DESTROY,
    SOUND_MINE_INSERT,
    SOUND_MINE_PRIME,
    SOUND_THROW,
    SOUND_FLAG_THUMP,
    SOUND_GARRISON_IN,
    SOUND_GARRISON_OUT,
    SOUND_ALERT_BELL,
    SOUND_ALERT_BUILDING,
    SOUND_ALERT_RESEARCH,
    SOUND_ALERT_UNIT,
    SOUND_GOLD_MINE_COLLAPSE,
    SOUND_MOLOTOV_IMPACT,
    SOUND_FIRE_BURN,
    SOUND_PISTOL_SILENCED,
    SOUND_CAMO_ON,
    SOUND_CAMO_OFF,
    SOUND_BALLOON_DEATH,
    SOUND_RICOCHET,
    SOUND_OBJECTIVE_COMPLETE,
    SOUND_MATCH_START,
    SOUND_GOLD_PICKUP,
    SOUND_AVALANCHE,
    SOUND_PEN_SCRATCH,
    SOUND_COUNT
};

const uint32_t MUSIC_OPTION_FADE_IN = 1U;
const uint32_t MUSIC_OPTION_LOOP = 1U << 1U;

bool sound_init();
void sound_quit();
const char* sound_get_name(SoundName sound);
void sound_set_sfx_volume(uint32_t volume);
void sound_set_music_volume(uint32_t volume);

uint32_t sound_play(SoundName sound, bool looping = false);
void sound_play_music(ResourceName music_resource, uint32_t options);
bool sound_is_music_playing();
void sound_stop(uint32_t track_index);
void sound_stop_music();
void sound_stop_all();
