#pragma once

#include "core/resource_list.h"
#include <cstddef>

bool resource_init();
void resource_quit();
const char* resource_get_path(ResourceName resource_name);
bool resource_is_text(ResourceName resource_name);
bool resource_begin_bulk_load();
void resource_end_bulk_load();

/*
    Loads the requested resource. If we are in the middle of a bulk session, then the
    bulk-session pack file will be used to load the resource. If we are not in the
    middle of a bulk session, then the pack file will be opened for the sake of reading the resource.

    This function is not thread-safe when used in a bulk session. To make it thread safe, the
    thread_safe flag may be set to true. This will cause the funciton to ignore the bulk session
    and just open up a separate pack file no matter what.
*/
void* resource_load(ResourceName resource_name, size_t* resource_size = NULL, bool thread_safe = false);

void* resource_debug_load_from_file(ResourceName resource_name, size_t* resource_size = NULL);
