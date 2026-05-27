#include "defines.h"

#include "gold.h"

// APP ENTRY

#if defined PLATFORM_WIN32 and not defined GOLD_DEBUG

#include <windows.h>
#define MAX_ARGS 16

int WINAPI WinMain(HINSTANCE /*h_instance*/, HINSTANCE /*h_prev_instance*/, LPSTR /*lp_cmd_line*/, int /*n_cmd_show*/) {
    char arg_buffer[MAX_ARGS][256];
    char* argv[MAX_ARGS];
    int argv_index = 0;
    int argc = 0;

    char* command_line = GetCommandLineA();
    int command_line_index = 0;
    bool is_in_quotes = false;
    while (command_line[command_line_index] != '\0') {
        if (command_line[command_line_index] == '"') {
            is_in_quotes = !is_in_quotes;
        } else {
            arg_buffer[argc][argv_index] = command_line[command_line_index];
            argv_index++;
        }
        command_line_index++;

        if ((command_line[command_line_index] == ' ' && !is_in_quotes) || command_line[command_line_index] == '\0') {
            command_line_index++;
            arg_buffer[argc][argv_index] = '\0';
            argv[argc] = arg_buffer[argc];
            argc++;
            argv_index = 0;
        }
    }

    return gold_main(argc, argv);
}

#else

int main(int argc, char** argv) {
    return gold_main(argc, argv);
}

#endif
