#pragma once

#include "defines.h"

#ifdef GOLD_DEBUG

#include "editor/menu/interface.h"
#include "match/scenario/scenario.h"
#include <string>

class EditorMenuSquad : public IEditorMenu {
public:
    EditorMenuSquad(const ScenarioSquad& squad);
protected:
    const char* get_header_text() const override;
    Rect get_rect() const override;
    void child_update(EditorState* state) override;
    void on_submit(EditorState* state) override;
private:
    std::string squad_name;
    uint32_t squad_player;
    uint32_t squad_type_index;
};

#endif
