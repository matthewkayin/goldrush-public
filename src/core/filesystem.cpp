#include "filesystem.h"

#include "defines.h"
#include <SDL3/SDL.h>
#include <ctime>
#include <vector>

#ifdef GOLD_STEAM
    #include <steam/steam_api.h>
#endif

std::string filesystem_get_timestamp_str() {
    time_t _time = time(NULL);
    tm _tm = *localtime(&_time);
    char buffer[32];
    sprintf(buffer, "%d-%02d-%02dT%02d%02d%02d", _tm.tm_year + 1900, _tm.tm_mon + 1, _tm.tm_mday, _tm.tm_hour, _tm.tm_min, _tm.tm_sec);
    return std::string(buffer);
}

std::string filesystem_get_data_path() {
    #ifdef GOLD_DEBUG
        return std::string("./");
    #else
        char* pref_path = SDL_GetPrefPath(NULL, APP_NAME);
        std::string path = std::string(pref_path);
        SDL_free(pref_path);
        return path;
    #endif
}

std::string filesystem_get_saves_folder_path() {
    std::string path = filesystem_get_data_path() + "saves/";
    #ifdef GOLD_STEAM
        path += std::to_string(SteamUser()->GetSteamID().ConvertToUint64()) + "/";
    #endif
    return path;
}

std::string filesystem_get_scenario_path() {
    #ifdef GOLD_DEBUG
        return std::string("../scenario/");
    #else
        return std::string(SDL_GetBasePath()) + "scenario/";
    #endif
}

void filesystem_create_required_folders() {
    std::vector<std::string> folder_paths;
    folder_paths.push_back(filesystem_get_data_path() + FILESYSTEM_LOG_FOLDER_NAME);
    folder_paths.push_back(filesystem_get_data_path() + FILESYSTEM_REPLAY_FOLDER_NAME);

    for (const std::string& path : folder_paths) {
        SDL_CreateDirectory(path.c_str());
    }
}

std::string filesystem_get_path_folder(const char* path) {
    std::string folder_path = std::string(path);

    size_t last_slash_index = folder_path.size();
    for (size_t index = 0; index < folder_path.size(); index++) {
        if (folder_path[index] == '\\' || folder_path[index] == '/') {
            last_slash_index = index;
        }
    }

    if (last_slash_index == folder_path.size()) {
        return std::string() + GOLD_PATH_SEPARATOR;
    }

    return folder_path.substr(0, last_slash_index) + GOLD_PATH_SEPARATOR;
}
