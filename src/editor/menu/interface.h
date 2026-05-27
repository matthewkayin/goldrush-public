#pragma once

#include "defines.h"

#ifdef GOLD_DEBUG

#include "util/math.h"

// Forward-declare
struct EditorState;

class IEditorMenu {
public:
    IEditorMenu();
    virtual ~IEditorMenu() = default;

    void update(EditorState* state);
    bool is_open() const;

protected:
    virtual const char* get_header_text() const = 0;
    virtual Rect get_rect() const = 0;
    virtual void child_update(EditorState* state) = 0;
    virtual void on_submit(EditorState* state) = 0;

    bool _is_open;
};

#endif
