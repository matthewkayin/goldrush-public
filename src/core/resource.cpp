#include "resource.h"

#include "core/logger.h"
#include "core/asserts.h"
#include "debug/env.h"
#include <SDL3/SDL.h>
#include <string>
#include <cstdio>
#include <cstdlib>

struct ResourceState {
    FILE* pack_file;
    ResourceIndexEntry index_entries[RESOURCE_COUNT];
};
static ResourceState state;

// MAIN API

bool resource_init() {
    state.pack_file = NULL;

#ifdef GOLD_DEBUG
    if (!env_get().use_resource_pack) {
        log_info("Initialized resource system in raw files mode.");
        return true;
    }
#endif

    // Read index file
    const std::string pack_file_path = std::string(SDL_GetBasePath()) + RESOURCE_INDEX_FILE_PATH;
    FILE* index_file = fopen(pack_file_path.c_str(), "rb");
    if (!index_file) {
        log_error("Failed to open resource file index %s", RESOURCE_INDEX_FILE_PATH);
        return false;
    }

    // Check signature
    uint32_t signature;
    fread(&signature, 1, sizeof(signature), index_file);
    if (signature != RESOURCE_INDEX_FILE_SIGNATURE) {
        log_error("Resource file signature is invalid.");
        fclose(index_file);
        return false;
    }

    // Read resource file entries
    for (uint32_t resource = 0; resource < RESOURCE_COUNT; resource++) {
        fread(&state.index_entries[resource], 1, sizeof(state.index_entries[resource]), index_file);
    }

    log_info("Initialized resource system.");
    return true;
}

void resource_quit() {
    if (state.pack_file != NULL) {
        log_warn("Called resource_quit() while pack was open.");
        fclose(state.pack_file);
    }
}

FILE* resource_open_pack_file() {
    const std::string index_file_path = std::string(SDL_GetBasePath()) + RESOURCE_PACK_FILE_PATH;
    FILE* pack_file = fopen(index_file_path.c_str(), "rb");
    if (!pack_file) {
        log_error("Failed to open resource pack.");
        return NULL;
    }

    uint32_t signature;
    fread(&signature, 1, sizeof(signature), pack_file);
    if (signature != RESOURCE_PACK_FILE_SIGNATURE) {
        log_error("Pack file signature does not match.");
        fclose(pack_file);
        pack_file = NULL;
        return NULL;
    }

    return pack_file;
}

bool resource_begin_bulk_load() {
    // In debug mode, if we are not using the resource pack, then do nothing
    #ifdef GOLD_DEBUG
        if (!env_get().use_resource_pack) {
            return true;
        }
    #endif

    // If the pack file is already open, then do nothing
    GOLD_ASSERT(state.pack_file == NULL);
    if (state.pack_file != NULL) {
        log_warn("Called resource_begin_bulk_load() while pack was already open.");
        return true;
    }

    state.pack_file = resource_open_pack_file();
    if (!state.pack_file) {
        return false;
    }

    log_debug("Resource began bulk load.");
    return true;
}

void resource_end_bulk_load() {
#ifdef GOLD_DEBUG
    if (!env_get().use_resource_pack) {
        return;
    }
#endif

    GOLD_ASSERT(state.pack_file != NULL);
    if (state.pack_file == NULL) {
        log_warn("Called resource_end_bulk_load() while pack was not open.");
        return;
    }

    fclose(state.pack_file);
    state.pack_file = NULL;
    log_debug("Resource ended bulk load.");
}

void* resource_load(ResourceName resource_name, size_t* resource_size, bool thread_safe) {
    // Debug - load resource directly from file
#ifdef GOLD_DEBUG
    if (!env_get().use_resource_pack) {
        return resource_debug_load_from_file(resource_name, resource_size);
    }
#endif

    // Defined up here to avoid the goto being angry
    size_t bytes_read;

    // Get a pack file handle
    FILE* pack_file = state.pack_file && !thread_safe
        ? state.pack_file
        : resource_open_pack_file();

    // Alloc resource buffer
    const ResourceIndexEntry& index_entry = state.index_entries[resource_name];
    void* resource_data = (uint8_t*)malloc(index_entry.size);
    if (!resource_data) {
        log_error("Failed to malloc data for resource %s", resource_get_path(resource_name));
        goto end;
    }

    // Copy into resource buffer
    clearerr(pack_file);
    fseek(pack_file, index_entry.offset, SEEK_SET);
    bytes_read = fread(resource_data, 1, index_entry.size, pack_file);
    if (bytes_read != index_entry.size) {
        log_error("Failed to read data for resource %s. Bytes read %u.", resource_get_path(resource_name), bytes_read);
        free(resource_data);
        resource_data = NULL;
        goto end;
    }

    // Copy the resource size
    if (resource_size != NULL) {
        *resource_size = index_entry.size;
    }

end:
    // If we opened the pack before, close it
    if (pack_file != state.pack_file) {
        fclose(pack_file);
    }

    return resource_data;
}

#ifdef GOLD_DEBUG

void* resource_debug_load_from_file(ResourceName resource_name, size_t* resource_size) {
    // Open resource file
    const std::string resource_full_path = std::string("../res/") + resource_get_path(resource_name);
    FILE* file = fopen(resource_full_path.c_str(), "rb");
    if (!file) {
        log_error("Failed to open resource file %s", resource_full_path.c_str());
        return NULL;
    }

    // Check the resource file size
    fseek(file, 0L, SEEK_END);
    uint32_t resource_file_size = ftell(file);
    uint32_t resource_data_size = resource_file_size;

    if (resource_is_text(resource_name)) {
        resource_data_size++;
    }

    // Alloc buffer for resource
    void* resource_data = (uint8_t*)malloc(resource_data_size);
    if (!resource_data) {
        log_error("Failed to malloc data for resource %s", resource_get_path(resource_name));
        fclose(file);
        return NULL;
    }

    // Copy resource into buffer
    clearerr(file);
    fseek(file, 0L, SEEK_SET);
    size_t bytes_read = fread(resource_data, 1, resource_file_size, file);
    if (bytes_read != resource_file_size) {
        log_error("Failed to read resource file. Bytes read %u vs size %u", bytes_read, resource_file_size);
        fclose(file);
        free(resource_data);
        return NULL;
    }

    if (resource_is_text(resource_name)) {
        char* resource_data_str = (char*)resource_data;
        resource_data_str[resource_data_size - 1] = '\0';
    }

    if (resource_size != NULL) {
        *resource_size = resource_data_size;
    }

    fclose(file);
    return resource_data;
}

#else

void* resource_debug_load_from_file(ResourceName /*resource_name*/, size_t* /*resource_size*/) {
    return NULL;
}

#endif
