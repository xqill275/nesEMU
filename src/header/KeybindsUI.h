#pragma once
#include <string>
#include "Keybinds.h"

namespace KeybindsUI {
    // Call once early
    void Init(const std::string& configPath);

    // Call every frame; draws popup if opened.
    // Returns true if keybinds were changed and saved.
    bool DrawPopup(Keybinds& binds, bool& openPopup);
}
