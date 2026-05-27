#pragma once

#include "defines.h"

#ifdef GOLD_DEBUG

#include "editor/menu/interface.h"

class EditorMenuNew : public IEditorMenu {
public:
    EditorMenuNew();
protected:
    const char* get_header_text() const override;
    Rect get_rect() const override;
    void child_update(EditorState* state) override;
    void on_submit(EditorState* state) override;
private:
    uint32_t map_type;
    uint32_t map_size;
    uint32_t map_init_style;
};

#endif
