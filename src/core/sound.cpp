#include "sound.h"

#include "SDL3/SDL_audio.h"
#include "SDL3/SDL_iostream.h"
#include "SDL3/SDL_properties.h"
#include "core/logger.h"
#include "core/asserts.h"
#include "core/filesystem.h"
#include "core/options.h"
#include "core/resource.h"
#include "util/bitflag.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_mixer.h>
#include <unordered_map>

#define SOUND_AUDIO_CHANNEL_COUNT 2
#define SOUND_TRACK_COUNT 32
#define SOUND_IS_LOOPING_INDEFINITELY -1

struct SoundParams {
    ResourceName resource;
    uint32_t variants;
};

static const std::unordered_map<SoundName, SoundParams> SOUND_PARAMS = {
    { SOUND_UI_CLICK, (SoundParams) {
        .resource = RESOURCE_SOUND_UI_CLICK,
        .variants = 1
    }},
    { SOUND_DEATH, (SoundParams) {
        .resource = RESOURCE_SOUND_DEATH1,
        .variants = 9
    }},
    { SOUND_MUSKET, (SoundParams) {
        .resource = RESOURCE_SOUND_MUSKET1,
        .variants = 9
    }},
    { SOUND_GUN, (SoundParams) {
        .resource = RESOURCE_SOUND_GUN1,
        .variants = 5
    }},
    { SOUND_EXPLOSION, (SoundParams) {
        .resource = RESOURCE_SOUND_EXPLOSION1,
        .variants = 4
    }},
    { SOUND_PICKAXE, (SoundParams) {
        .resource = RESOURCE_SOUND_PICKAXE1,
        .variants = 3
    }},
    { SOUND_HAMMER, (SoundParams) {
        .resource = RESOURCE_SOUND_HAMMER1,
        .variants = 4
    }},
    { SOUND_BUILDING_PLACE, (SoundParams) {
        .resource = RESOURCE_SOUND_BUILDING_PLACE,
        .variants = 1
    }},
    { SOUND_SWORD, (SoundParams) {
        .resource = RESOURCE_SOUND_SWORD1,
        .variants = 3
    }},
    { SOUND_DEATH_CHICKEN, (SoundParams) {
        .resource = RESOURCE_SOUND_DEATH_CHICKEN1,
        .variants = 4
    }},
    { SOUND_CANNON, (SoundParams) {
        .resource = RESOURCE_SOUND_CANNON1,
        .variants = 4
    }},
    { SOUND_BUILDING_DESTROY, (SoundParams) {
        .resource = RESOURCE_SOUND_BUILDING_DESTROY,
        .variants = 1
    }},
    { SOUND_BUNKER_DESTROY, (SoundParams) {
        .resource = RESOURCE_SOUND_BUNKER_DESTROY,
        .variants = 1
    }},
    { SOUND_MINE_DESTROY, (SoundParams) {
        .resource = RESOURCE_SOUND_MINE_DESTROY,
        .variants = 1
    }},
    { SOUND_MINE_INSERT, (SoundParams) {
        .resource = RESOURCE_SOUND_MINE_INSERT,
        .variants = 1
    }},
    { SOUND_MINE_PRIME, (SoundParams) {
        .resource = RESOURCE_SOUND_MINE_PRIME,
        .variants = 1
    }},
    { SOUND_THROW, (SoundParams) {
        .resource = RESOURCE_SOUND_THROW,
        .variants = 1
    }},
    { SOUND_FLAG_THUMP, (SoundParams) {
        .resource = RESOURCE_SOUND_FLAG_THUMP,
        .variants = 1
    }},
    { SOUND_GARRISON_IN, (SoundParams) {
        .resource = RESOURCE_SOUND_GARRISON_IN,
        .variants = 1
    }},
    { SOUND_GARRISON_OUT, (SoundParams) {
        .resource = RESOURCE_SOUND_GARRISON_OUT,
        .variants = 1
    }},
    { SOUND_ALERT_BELL, (SoundParams) {
        .resource = RESOURCE_SOUND_ALERT_BELL,
        .variants = 1
    }},
    { SOUND_ALERT_BUILDING, (SoundParams) {
        .resource = RESOURCE_SOUND_ALERT_BUILDING,
        .variants = 1
    }},
    { SOUND_ALERT_RESEARCH, (SoundParams) {
        .resource = RESOURCE_SOUND_ALERT_RESEARCH,
        .variants = 1
    }},
    { SOUND_ALERT_UNIT, (SoundParams) {
        .resource = RESOURCE_SOUND_ALERT_UNIT,
        .variants = 1
    }},
    { SOUND_GOLD_MINE_COLLAPSE, (SoundParams) {
        .resource = RESOURCE_SOUND_GOLD_MINE_COLLAPSE,
        .variants = 1
    }},
    { SOUND_MOLOTOV_IMPACT, (SoundParams) {
        .resource = RESOURCE_SOUND_MOLOTOV_IMPACT,
        .variants = 1
    }},
    { SOUND_FIRE_BURN, (SoundParams) {
        .resource = RESOURCE_SOUND_FIRE_BURN,
        .variants = 1
    }},
    { SOUND_PISTOL_SILENCED, (SoundParams) {
        .resource = RESOURCE_SOUND_PISTOL_SILENCED1,
        .variants = 3
    }},
    { SOUND_CAMO_ON, (SoundParams) {
        .resource = RESOURCE_SOUND_CAMO_ON,
        .variants = 1
    }},
    { SOUND_CAMO_OFF, (SoundParams) {
        .resource = RESOURCE_SOUND_CAMO_OFF,
        .variants = 1
    }},
    { SOUND_BALLOON_DEATH, (SoundParams) {
        .resource = RESOURCE_SOUND_BALLOON_DEATH,
        .variants = 1
    }},
    { SOUND_RICOCHET, (SoundParams) {
        .resource = RESOURCE_SOUND_RICOCHET1,
        .variants = 5
    }},
    { SOUND_OBJECTIVE_COMPLETE, (SoundParams) {
        .resource = RESOURCE_SOUND_OBJECTIVE_COMPLETE,
        .variants = 1
    }},
    { SOUND_MATCH_START, (SoundParams) {
        .resource = RESOURCE_SOUND_MATCH_START,
        .variants = 1
    }},
    { SOUND_GOLD_PICKUP, (SoundParams) {
        .resource = RESOURCE_SOUND_GOLD_PICKUP,
        .variants = 1
    }},
    { SOUND_AVALANCHE, (SoundParams) {
        .resource = RESOURCE_SOUND_AVALANCHE,
        .variants = 1
    }},
    { SOUND_PEN_SCRATCH, (SoundParams) {
        .resource = RESOURCE_SOUND_PEN_SCRATCH,
        .variants = 1
    }},
};

struct SoundState {
    MIX_Mixer* mixer;
    MIX_Track* tracks[SOUND_TRACK_COUNT];

    MIX_Audio** sounds;
    uint32_t sound_count;
    uint32_t sound_index[SOUND_COUNT];

    SDL_PropertiesID sound_properties_loop_indefinitely;

    MIX_Audio* music;
    MIX_Track* music_track;
};
static SoundState state;

bool sound_init() {
    // Init SDL Mixer
    if (!MIX_Init()) {
        log_error("Failed to initialize SDL_mixer: %s", SDL_GetError());
        return false;
    }

    // Create mixer
    state.mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
    if (!state.mixer) {
        log_error("Failed to create mixer on default device: %s", SDL_GetError());
        return false;
    }

    // Create tracks
    for (uint32_t track_index = 0; track_index < SOUND_TRACK_COUNT; track_index++) {
        state.tracks[track_index] = MIX_CreateTrack(state.mixer);
        if (!state.tracks[track_index]) {
            log_error("Failed to create mixer track: %s", SDL_GetError());
            return false;
        }
    }

    // Determine number of sounds
    state.sound_count = 0;
    for (uint32_t sound = 0; sound < SOUND_COUNT; sound++) {
        const SoundParams& params = SOUND_PARAMS.at((SoundName)sound);
        state.sound_count += params.variants;
    }

    // Init sounds array
    state.sounds = (MIX_Audio**)malloc(state.sound_count * sizeof(MIX_Audio*));
    if (!state.sounds) {
        log_error("Failed to alloc sounds array.");
        return false;
    }

    // Load sounds
    int sounds_size = 0;
    for (uint32_t sound = 0; sound < SOUND_COUNT; sound++) {
        const SoundParams& params = SOUND_PARAMS.at((SoundName)sound);
        state.sound_index[sound] = sounds_size;

        for (uint32_t variant = 0; variant < params.variants; variant++) {
            // Load sound data
            ResourceName variant_resource = (ResourceName)(params.resource + variant);
            size_t sound_data_size;
            void* sound_data = resource_load(variant_resource, &sound_data_size);
            if (!sound_data) {
                log_error("Failed to load sound %s.", resource_get_path(variant_resource));
                return false;
            }

            // Load the sound
            SDL_IOStream* io_stream = SDL_IOFromMem(sound_data, sound_data_size);
            state.sounds[sounds_size] = MIX_LoadAudio_IO(state.mixer, io_stream, true, true);
            if (!state.sounds[sounds_size]) {
                log_error("Failed to init sound %s: %s", resource_get_path(variant_resource), SDL_GetError());
                return false;
            }
            sounds_size++;

            free(sound_data);
        }
    }

    // Create looping sound property
    state.sound_properties_loop_indefinitely = SDL_CreateProperties();
    if (!state.sound_properties_loop_indefinitely) {
        log_error("Could not create sound properties: %s", SDL_GetError());
        return false;
    }
    SDL_SetNumberProperty(state.sound_properties_loop_indefinitely, MIX_PROP_PLAY_LOOPS_NUMBER, SOUND_IS_LOOPING_INDEFINITELY);

    // Create music track
    state.music_track = MIX_CreateTrack(state.mixer);
    if (!state.music_track) {
        log_error("Failed to create music track: %s", SDL_GetError());
        return false;
    }
    state.music = NULL;

    option_apply(OPTION_SFX_VOLUME);
    option_apply(OPTION_MUSIC_VOLUME);
    log_info("initialized sound system.");

    return true;
}

void sound_quit() {
    // Free properties
    SDL_DestroyProperties(state.sound_properties_loop_indefinitely);

    // Free sound effects
    for (uint32_t sound_index = 0; sound_index < state.sound_count; sound_index++) {
        MIX_DestroyAudio(state.sounds[sound_index]);
    }
    free(state.sounds);

    // Free music
    if (state.music != NULL) {
        MIX_DestroyAudio(state.music);
    }

    // Free tracks
    for (uint32_t track_index = 0; track_index < SOUND_TRACK_COUNT; track_index++) {
        MIX_DestroyTrack(state.tracks[track_index]);
    }
    MIX_DestroyTrack(state.music_track);

    // Quit mixer
    MIX_DestroyMixer(state.mixer);
    MIX_Quit();
}

const char* sound_get_name(SoundName sound) {
    switch (sound) {
        case SOUND_UI_CLICK:
            return "UI_CLICK";
        case SOUND_DEATH:
            return "DEATH";
        case SOUND_MUSKET:
            return "MUSKET";
        case SOUND_GUN:
            return "GUN";
        case SOUND_EXPLOSION:
            return "EXPLOSION";
        case SOUND_PICKAXE:
            return "PICKAXE";
        case SOUND_HAMMER:
            return "HAMMER";
        case SOUND_BUILDING_PLACE:
            return "BUILDING_PLACE";
        case SOUND_SWORD:
            return "SWORD";
        case SOUND_DEATH_CHICKEN:
            return "DEATH_CHICKEN";
        case SOUND_CANNON:
            return "CANNON";
        case SOUND_BUILDING_DESTROY:
            return "BUILDING_DESTROY";
        case SOUND_BUNKER_DESTROY:
            return "BUNKER_DESTROY";
        case SOUND_MINE_DESTROY:
            return "MINE_DESTROY";
        case SOUND_MINE_INSERT:
            return "MINE_INSERT";
        case SOUND_MINE_PRIME:
            return "MINE_PRIME";
        case SOUND_THROW:
            return "THROW";
        case SOUND_FLAG_THUMP:
            return "FLAG_THUMP";
        case SOUND_GARRISON_IN:
            return "GARRISON_IN";
        case SOUND_GARRISON_OUT:
            return "GARRISON_OUT";
        case SOUND_ALERT_BELL:
            return "ALERT_BELL";
        case SOUND_ALERT_BUILDING:
            return "ALERT_BUILDING";
        case SOUND_ALERT_RESEARCH:
            return "ALERT_RESEARCH";
        case SOUND_ALERT_UNIT:
            return "ALERT_UNIT";
        case SOUND_GOLD_MINE_COLLAPSE:
            return "GOLD_MINE_COLLAPSE";
        case SOUND_MOLOTOV_IMPACT:
            return "MOLOTOV_IMPACT";
        case SOUND_FIRE_BURN:
            return "FIRE_BURN";
        case SOUND_PISTOL_SILENCED:
            return "PISTOL_SILENCED";
        case SOUND_CAMO_ON:
            return "CAMO_ON";
        case SOUND_CAMO_OFF:
            return "CAMO_OFF";
        case SOUND_BALLOON_DEATH:
            return "BALLOON_DEATH";
        case SOUND_RICOCHET:
            return "RICOCHET";
        case SOUND_OBJECTIVE_COMPLETE:
            return "OBJECTIVE_COMPLETE";
        case SOUND_MATCH_START:
            return "MATCH_START";
        case SOUND_GOLD_PICKUP:
            return "GOLD_PICKUP";
        case SOUND_AVALANCHE:
            return "AVALANCHE";
        case SOUND_PEN_SCRATCH:
            return "PEN_SCRATCH";
        case SOUND_COUNT:
            GOLD_ASSERT(false);
            return "";
    }
}

void sound_set_sfx_volume(uint32_t volume) {
    for (uint32_t track_index = 0; track_index < SOUND_TRACK_COUNT; track_index++) {
        MIX_SetTrackGain(state.tracks[track_index], (float)volume / 100.0f);
    }
}

void sound_set_music_volume(uint32_t volume) {
    MIX_SetTrackGain(state.music_track, (float)volume / 100.0f);
}

uint32_t sound_play(SoundName sound, bool looping) {
    uint32_t available_track_index = SOUND_TRACK_COUNT;
    for (uint32_t track_index = 0; track_index < SOUND_TRACK_COUNT; track_index++) {
        // If the track is not playing, then use it
        if (!MIX_TrackPlaying(state.tracks[track_index])) {
            available_track_index = track_index;
            break;
        }

        // Never interrupt a looping track
        if (MIX_GetTrackLoops(state.tracks[track_index]) == SOUND_IS_LOOPING_INDEFINITELY) {
            continue;
        }

        // If the track is active but not looping, then use it
        // only if it is closer to finishing than all the others
        if (available_track_index == SOUND_TRACK_COUNT ||
                MIX_GetTrackRemaining(state.tracks[track_index]) <
                MIX_GetTrackRemaining(state.tracks[available_track_index])) {
            available_track_index = track_index;
        }
    }
    GOLD_ASSERT(available_track_index != SOUND_TRACK_COUNT);

    uint32_t variant = SOUND_PARAMS.at(sound).variants == 1
        ? 0
        : (uint32_t)(rand() % SOUND_PARAMS.at(sound).variants);
    uint32_t sound_index = state.sound_index[sound] + variant;
    MIX_SetTrackAudio(state.tracks[available_track_index], state.sounds[sound_index]);
    MIX_PlayTrack(state.tracks[available_track_index], looping ? state.sound_properties_loop_indefinitely : 0);

    return available_track_index;
}

void sound_play_music(ResourceName music_resource, uint32_t options) {
    // Load the music resource
    size_t resource_length;
    void* resource_data = resource_load(music_resource, &resource_length);

    // Init the music audio
    SDL_IOStream* io_stream = SDL_IOFromMem(resource_data, resource_length);
    MIX_Audio* music = MIX_LoadAudio_IO(state.mixer, io_stream, false, true);
    if (!music) {
        log_error("Failed to load music. %s", SDL_GetError());
        return;
    }

    // Configure playback options
    SDL_PropertiesID playback_properties = 0;
    if (options != 0) {
        playback_properties = SDL_CreateProperties();
        if (!playback_properties) {
            log_error("Failed to create sound properties.", SDL_GetError());
            return;
        }

        if (bitflag_check(options, MUSIC_OPTION_FADE_IN)) {
            SDL_SetNumberProperty(playback_properties, MIX_PROP_PLAY_FADE_IN_MILLISECONDS_NUMBER, 3000);
        }
        if (bitflag_check(options, MUSIC_OPTION_LOOP)) {
            SDL_SetNumberProperty(playback_properties, MIX_PROP_PLAY_LOOPS_NUMBER, SOUND_IS_LOOPING_INDEFINITELY);
        }
    }

    // Stop the previous music
    if (state.music != NULL && sound_is_music_playing()) {
        sound_stop_music();
        MIX_DestroyAudio(state.music);
    }

    // Play the music
    state.music = music;
    MIX_SetTrackAudio(state.music_track, state.music);
    MIX_PlayTrack(state.music_track, playback_properties);

    // Clean up
    SDL_DestroyProperties(playback_properties);
    free(resource_data);

    log_info("Started music track %s.", resource_get_path(music_resource));
}

bool sound_is_music_playing() {
    return MIX_TrackPlaying(state.music_track);
}

void sound_stop(uint32_t track_index) {
    MIX_StopTrack(state.tracks[track_index], 0);
}

void sound_stop_music() {
    MIX_StopTrack(state.music_track, 0);
}

void sound_stop_all() {
    for (uint32_t track_index = 0; track_index < SOUND_TRACK_COUNT; track_index++) {
        MIX_StopTrack(state.tracks[track_index], 0);
    }
    MIX_StopTrack(state.music_track, 0);
}
