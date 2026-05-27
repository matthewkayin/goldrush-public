#include "doc.h"

#include "defines.h"

#ifdef GOLD_DEBUG

#include "match/shell/script/script.h"
#include "core/filesystem.h"
#include <fstream>

static const char* MODULE_CPP_PATH = "../src/match/shell/script/module.cpp";

void script_generate_doc() {
    std::string doc_path = filesystem_get_scenario_path() + "modules" + GOLD_PATH_SEPARATOR + "scenario.d.lua";
    FILE* file = fopen(doc_path.c_str(), "w");
    if (!file) {
        printf("Error: could not open %s.\n", doc_path.c_str());
        return;
    }

    fprintf(file, "--- @meta\n\n");

    fprintf(file, "--- @class scenario\n");
    fprintf(file, "--- @field constants table\n");
    fprintf(file, "%s = {}\n\n", MODULE_NAME);

    fprintf(file, "--- @class ivec2\n");
    fprintf(file, "--- @field x number\n");
    fprintf(file, "--- @field y number\n\n");

    // Create and populate the scenario table with all of the constants
    lua_State* lua_state = luaL_newstate();
    lua_newtable(lua_state);
    lua_setglobal(lua_state, MODULE_NAME);
    script_register_scenario_constants(lua_state);

    // Now that we have the constants, we can read them to create documentation
    lua_getglobal(lua_state, MODULE_NAME);
    lua_pushnil(lua_state);
    while (lua_next(lua_state, -2) != 0) {
        const char* const_name = lua_tostring(lua_state, -2);
        int type = lua_type(lua_state, -1);

        if (type == LUA_TTABLE) {
            fprintf(file, "%s.%s = {}\n", MODULE_NAME, const_name);
            lua_pushnil(lua_state);
            while (lua_next(lua_state, -2) != 0) {
                const char* table_const_name = lua_tostring(lua_state, -2);
                int table_const_value = lua_tonumber(lua_state, -1);

                fprintf(file, "%s.%s.%s = %i\n", MODULE_NAME, const_name, table_const_name, table_const_value);

                lua_pop(lua_state, 1);
            }
            fprintf(file, "\n");
        } else if (type == LUA_TNUMBER) {
            fprintf(file, "%s.%s = %i\n", MODULE_NAME, const_name, (int)lua_tonumber(lua_state, -1));
        }
        lua_pop(lua_state, 1);
    }
    lua_pop(lua_state, 1);
    fprintf(file, "\n");

    lua_close(lua_state);

    std::ifstream script_cpp_ifstream(MODULE_CPP_PATH);
    if (!script_cpp_ifstream.is_open()) {
        printf("ERROR: Failed to open %s\n", MODULE_CPP_PATH);
        return;
    }

    std::vector<std::string> comments;
    std::unordered_map<std::string, std::vector<std::string>> function_comments;

    std::string line;
    while (std::getline(script_cpp_ifstream, line)) {
        if (line.rfind("// ", 0) == 0) {
            comments.push_back(line.substr(3));
            continue;
        }

        if (line.rfind("int script_", 0) == 0 && line.rfind("{") != std::string::npos) {
            size_t function_name_start = strlen("int script_");
            size_t parameter_list_start = line.find('(');
            size_t function_name_length = parameter_list_start - function_name_start;
            std::string function_name = line.substr(function_name_start, function_name_length);
            function_comments[function_name] = comments;
        }

        // If line not a comment, clear the comments list
        comments.clear();
    }

    size_t index = 0;
    while (GOLD_FUNCS[index].name != NULL) {
        std::vector<std::string> param_names;
        for (const std::string& comment : function_comments[GOLD_FUNCS[index].name]) {
            fprintf(file, "--- %s\n", comment.c_str());
            if (comment.rfind("@param", 0) == 0) {
                size_t space_index = comment.find(' ');
                if (space_index == std::string::npos) {
                    printf("ERROR: Invalid @param for function %s\n", GOLD_FUNCS[index].name);
                    continue;
                }

                std::string param_name = comment.substr(space_index + 1);
                space_index = param_name.find(' ');
                if (space_index != std::string::npos) {
                    param_name = param_name.substr(0, space_index);
                }

                param_names.push_back(param_name);
            }
        }

        std::string param_string = "";
        for (size_t param_index = 0; param_index < param_names.size(); param_index++) {
            param_string += param_names[param_index];
            if (param_index != param_names.size() - 1) {
                param_string += ", ";
            }
        }

        fprintf(file, "function %s.%s(%s) end\n\n", MODULE_NAME, GOLD_FUNCS[index].name, param_string.c_str());

        index++;
    }

    fclose(file);
    printf("Generated Lua doc at %s\n", doc_path.c_str());
}

#else

void script_generate_doc() {}

#endif
