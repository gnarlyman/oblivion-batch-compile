#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace obc {

struct ScriptResult {
    std::string editorId;
    std::uint32_t formId = 0;
    std::string scdaSha256Before;
    std::string scdaSha256After;
    std::size_t scdaLengthBefore = 0;
    std::size_t scdaLengthAfter = 0;
    bool        compileOk = false;
    std::string error;          // empty if compileOk
};

struct BatchResult {
    std::string overallStatus;  // "ok" | "error"
    std::string overallError;   // populated when overallStatus == "error"
    std::string plugin;
    std::vector<ScriptResult> scripts;
    bool        saved = false;

    // Write to path as JSON. If path empty, no-op.
    void WriteJson(const std::string& path) const;
};

}  // namespace obc
