#pragma once

#include "defines.h"

#ifdef GOLD_DEBUG

#include "editor/menu/interface.h"
#include "match/scenario/scenario.h"
#include "core/ui.h"

enum EditorMenuPlayersMode {
    EDITOR_MENU_PLAYERS_MODE_PLAYERS,
    EDITOR_MENU_PLAYERS_MODE_TECH
};

class EditorMenuPlayers : public IEditorMenu {
public:
    EditorMenuPlayers(const Scenario* scenario);
protected:
    const char* get_header_text() const override;
    Rect get_rect() const override;
    void child_update(EditorState* state) override;
    void on_submit(EditorState* state) override;
private:
    void mode_players_update(EditorState* state);
    void mode_players_bot_config_dropdown(UiContext& ui_context, BotConfig& bot_config, uint32_t flag) const;
    void mode_tech_update(EditorState* state);
    bool mode_tech_is_tech_allowed(const Scenario* scenario, uint32_t tech_index) const;
    void mode_tech_set_tech_allowed(Scenario* scenario, uint32_t tech_index, bool value);

    EditorMenuPlayersMode mode;
    uint32_t selected_player_id;
    std::string selected_player_name_string;
    int scroll;
};

#endif
