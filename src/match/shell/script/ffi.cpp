#include "match/shell/script/script.h"

#ifdef _MSC_VER
    #define GOLDRUSH_FFI extern "C" __declspec(dllexport)
#else
    #define GOLDRUSH_FFI extern "C" __attribute__((visibility("default")))
#endif

GOLDRUSH_FFI const Entity* get_by_id(EntityId entity_id) {
    const MatchShell* shell = script_get_match_shell();
    const uint32_t entity_index = shell->match_state.entities.get_index_of(entity_id);
    if (entity_index == INDEX_INVALID) {
        return nullptr;
    }
    return &shell->match_state.entities[entity_index];
}

GOLDRUSH_FFI const Entity* get_by_index(uint32_t entity_index) {
    const MatchShell* shell = script_get_match_shell();
    return &shell->match_state.entities[entity_index];
}

GOLDRUSH_FFI uint32_t get_count() {
    const MatchShell* shell = script_get_match_shell();
    return shell->match_state.entities.size();
}

GOLDRUSH_FFI EntityId get_id_of(uint32_t entity_index) {
    const MatchShell* shell = script_get_match_shell();
    return shell->match_state.entities.get_id_of(entity_index);
}

GOLDRUSH_FFI uint32_t get_index_of(EntityId entity_id) {
    const MatchShell* shell = script_get_match_shell();
    return shell->match_state.entities.get_index_of(entity_id);
}
