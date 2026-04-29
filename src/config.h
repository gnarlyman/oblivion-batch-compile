#pragma once

#include <cstdint>
#include <string>

namespace obc {

struct BatchConfig {
    bool enabled = false;       // false → plugin idle, no batch run

    std::string targetPlugin;   // CSE_BATCH_ONE — required when enabled
    bool        hasFormId = false;
    std::uint32_t formId = 0;   // CSE_BATCH_FORMID (active-file FormID, hex)
    std::string resultPath;     // CSE_BATCH_RESULT — optional
    bool        hideWindow = false;  // CSE_BATCH_HIDDEN=1 — hide CS main window

    // Load configuration from environment. Returns true if a batch op was
    // requested (CSE_BATCH_ONE present and non-empty); false otherwise.
    bool LoadFromEnvironment();
};

}  // namespace obc
