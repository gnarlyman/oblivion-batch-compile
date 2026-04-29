#include "config.h"

#include <Windows.h>

#include <cstdlib>
#include <string>

#include "log.h"

namespace obc {

namespace {

bool GetEnv(const char* name, std::string& out) {
    char buf[1024];
    DWORD n = GetEnvironmentVariableA(name, buf, sizeof(buf));
    if (n == 0 || n >= sizeof(buf)) {
        out.clear();
        return false;
    }
    out.assign(buf, n);
    return !out.empty();
}

bool ParseHexFormId(const std::string& s, std::uint32_t& out) {
    if (s.empty()) return false;
    const char* p = s.c_str();
    if (s.size() >= 2 && (s[0] == '0') && (s[1] == 'x' || s[1] == 'X')) p += 2;
    char* endp = nullptr;
    auto v = std::strtoul(p, &endp, 16);
    if (endp == p || *endp != '\0') return false;
    out = static_cast<std::uint32_t>(v);
    return true;
}

}  // namespace

bool BatchConfig::LoadFromEnvironment() {
    enabled = false;
    targetPlugin.clear();
    resultPath.clear();
    hasFormId = false;
    formId = 0;

    std::string s;
    if (!GetEnv("CSE_BATCH_ONE", s)) return false;
    targetPlugin = std::move(s);

    if (GetEnv("CSE_BATCH_FORMID", s)) {
        std::uint32_t v = 0;
        if (ParseHexFormId(s, v)) {
            hasFormId = true;
            formId = v;
        } else {
            OBC_LOG("Config: CSE_BATCH_FORMID=%s is not a valid hex value (ignored)",
                    s.c_str());
        }
    }

    if (GetEnv("CSE_BATCH_RESULT", s)) {
        resultPath = std::move(s);
    }

    enabled = true;
    OBC_LOG("Config: enabled, target=\"%s\", formid=%s%08X, result=\"%s\"",
            targetPlugin.c_str(),
            hasFormId ? "" : "(any) 0x", formId,
            resultPath.c_str());
    return true;
}

}  // namespace obc
