#include "resource.h"

#include "core/logger.h"
#include "core/asserts.h"
#include "util/util.h"
#include "debug/env.h"
#include <cstdio>

static const char* RESOURCE_INDEX_FILE_PATH = "./res.idx";
static const char* RESOURCE_PACK_FILE_PATH = "./res.pak";
static const uint32_t RESOURCE_INDEX_FILE_SIGNATURE = 0x77234639;
static const uint32_t RESOURCE_PACK_FILE_SIGNATURE = 0x77237225;

static const char* RESOURCE_PATHS[] = {
#define X(name, path) path,
    RESOURCE_LIST
#undef X
};

struct ResourceIndexEntry {
    size_t offset;
    size_t size;
};

struct ResourceState {
    FILE* pack_file;
    ResourceIndexEntry index_entries[RESOURCE_COUNT];
};
static ResourceState state;

// MAIN API

bool resource_init() {
    state.pack_file = NULL;

    // Read index file
    FILE* index_file = fopen(RESOURCE_INDEX_FILE_PATH, "rb");
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

const char* resource_get_path(ResourceName resource_name) {
    return RESOURCE_PATHS[resource_name];
}

bool resource_open_pack() {
#ifdef GOLD_DEBUG
    if (!env_get().use_resource_pack) {
        return true;
    }
#endif

    GOLD_ASSERT(state.pack_file == NULL);
    if (state.pack_file != NULL) {
        log_warn("Called resource_open_pack() while pack was already open.");
        return true;
    }

    state.pack_file = fopen(RESOURCE_PACK_FILE_PATH, "rb");
    if (!state.pack_file) {
        log_error("Failed to open resource pack.");
        return false;
    }

    uint32_t signature;
    fread(&signature, 1, sizeof(signature), state.pack_file);
    if (signature != RESOURCE_PACK_FILE_SIGNATURE) {
        log_error("Pack file signature does not match.");
        fclose(state.pack_file);
        state.pack_file = NULL;
        return false;
    }

    log_debug("Opened resource pack.");
    return true;
}

void resource_close_pack() {
#ifdef GOLD_DEBUG
    if (!env_get().use_resource_pack) {
        return;
    }
#endif

    GOLD_ASSERT(state.pack_file != NULL);
    if (state.pack_file == NULL) {
        log_warn("Called resource_close_pack() while pack was not open.");
        return;
    }

    fclose(state.pack_file);
    state.pack_file = NULL;
    log_debug("Closed resource pack.");
}

void* resource_load(ResourceName resource_name, size_t* resource_size) {
    // Debug - load resource directly from file
#ifdef GOLD_DEBUG
    if (!env_get().use_resource_pack) {
        return resource_debug_load_from_file(resource_name, resource_size);
    }
#endif

    // Defined up here to avoid the goto being angry
    size_t bytes_read;

    // Open pack if necessary
    bool was_pack_closed = state.pack_file == NULL;
    if (state.pack_file == NULL) {
        resource_open_pack();
    }

    // Alloc resource buffer
    const ResourceIndexEntry& index_entry = state.index_entries[resource_name];
    void* resource_data = (uint8_t*)malloc(index_entry.size);
    if (!resource_data) {
        log_error("Failed to malloc data for resource %s", RESOURCE_PATHS[resource_name]);
        goto end;
    }

    // Copy into resource buffer
    clearerr(state.pack_file);
    fseek(state.pack_file, index_entry.offset, SEEK_SET);
    bytes_read = fread(resource_data, 1, index_entry.size, state.pack_file);
    if (bytes_read != index_entry.size) {
        log_error("Failed to read data for resource %s. Bytes read %u.", RESOURCE_PATHS[resource_name], bytes_read);
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
    if (was_pack_closed) {
        resource_close_pack();
    }

    return resource_data;
}

#ifdef GOLD_DEBUG

void* resource_debug_load_from_file(ResourceName resource_name, size_t* resource_size) {
    // Open resource file
    const std::string resource_full_path = std::string("../res/") + RESOURCE_PATHS[resource_name];
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
        log_error("Failed to malloc data for resource %s", RESOURCE_PATHS[resource_name]);
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

// PACK

bool resource_is_text(ResourceName resource_name) {
    const char* text_resource_path_suffixes[] = {
        ".glsl",
        ".lua",
        ".txt",
        NULL
    };

    const std::string resource_path = RESOURCE_PATHS[resource_name];
    const char** suffix_ptr = text_resource_path_suffixes;
    while (*suffix_ptr != NULL) {
        if (string_ends_with(resource_path, *suffix_ptr)) {
            return true;
        }
        suffix_ptr++;
    }

    return false;
}

void resource_create_pack() {
    FILE* index_file = NULL;
    FILE* pack_file = NULL;

    // Open the index file
    index_file = fopen(RESOURCE_INDEX_FILE_PATH, "wb");
    if (!index_file) {
        printf("Failed to open index file %s\n", RESOURCE_INDEX_FILE_PATH);
        goto end;
    }

    // Write the index file signature
    fwrite(&RESOURCE_INDEX_FILE_SIGNATURE, 1, sizeof(RESOURCE_INDEX_FILE_SIGNATURE), index_file);

    // Open the pack file
    pack_file = fopen(RESOURCE_PACK_FILE_PATH, "wb");
    if (!pack_file) {
        printf("Failed to open pack file %s\n", RESOURCE_PACK_FILE_PATH);
        goto end;
    }

    // Write the pack file signature
    fwrite(&RESOURCE_PACK_FILE_SIGNATURE, 1, sizeof(RESOURCE_PACK_FILE_SIGNATURE), pack_file);

    for (uint32_t resource = 0; resource < RESOURCE_COUNT; resource++) {
        // Open resource file
        const std::string resource_full_path = std::string("../res/") + RESOURCE_PATHS[resource];
        FILE* resource_file = fopen(resource_full_path.c_str(), "rb");
        if (!resource_file) {
            printf("Failed to open resource file %s\n", RESOURCE_PATHS[resource]);
            goto end;
        }

        // Check the resource file size
        fseek(resource_file, 0L, SEEK_END);
        uint32_t resource_file_size = ftell(resource_file);

        // Check if the resource is text
        const bool is_resource_text = resource_is_text((ResourceName)resource);

        // Write a resource entry into the index file
        ResourceIndexEntry entry;
        entry.offset = ftell(pack_file);
        entry.size = resource_file_size;
        // If the resource is text, include an extra byte for the null terminator
        if (is_resource_text) {
            entry.size++;
        }
        fwrite(&entry, 1, sizeof(entry), index_file);

        // Move the cursor back to the beginning
        fseek(resource_file, 0L, SEEK_SET);

        // Copy the contents of resource file into the pack
        static uint8_t copy_buffer[1024 * 1024];
        size_t bytes_read;
        while ((bytes_read = fread(copy_buffer, 1, sizeof(copy_buffer), resource_file)) != 0) {
            fwrite(copy_buffer, 1, bytes_read, pack_file);
        }

        // If the resource is text, write the null terminator
        if (is_resource_text) {
            char null_terminator = '\0';
            fwrite(&null_terminator, 1, sizeof(null_terminator), pack_file);
        }

        fclose(resource_file);
    }

    printf("Resource packing complete.\n");
end:
    if (pack_file != NULL) {
        fclose(pack_file);
    }
    if (index_file != NULL) {
        fclose(index_file);
    }
}
