#pragma once

#include "defines.h"

#ifdef GOLD_DEBUG

#include "editor/menu/interface.h"
#include <string>

class EditorMenuConstant : public IEditorMenu {
public:
    EditorMenuConstant(const char* initial_constant_name);
protected:
    const char* get_header_text() const override;
    Rect get_rect() const override;
    void child_update(EditorState* state) override;
    void on_submit(EditorState* state) override;
private:
    std::string constant_name;
};

#endif
