#include "result.h"

#include <Windows.h>

#include <cstdio>
#include <string>

#include "log.h"

namespace obc {

namespace {

void AppendEscaped(std::string& out, const std::string& s) {
    out.push_back('"');
    for (char c : s) {
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '"':  out += "\\\""; break;
        case '\n': out += "\\n";  break;
        case '\r': out += "\\r";  break;
        case '\t': out += "\\t";  break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "\\u%04X", static_cast<unsigned>(c));
                out += buf;
            } else {
                out.push_back(c);
            }
        }
    }
    out.push_back('"');
}

void AppendKeyVal(std::string& out, const char* key, const std::string& val) {
    AppendEscaped(out, key);
    out.push_back(':');
    AppendEscaped(out, val);
}

void AppendKeyHex32(std::string& out, const char* key, std::uint32_t val) {
    AppendEscaped(out, key);
    char buf[16];
    std::snprintf(buf, sizeof(buf), ":\"%08X\"", val);
    out += buf;
}

void AppendKeyU64(std::string& out, const char* key, std::uint64_t val) {
    AppendEscaped(out, key);
    char buf[32];
    std::snprintf(buf, sizeof(buf), ":%llu", static_cast<unsigned long long>(val));
    out += buf;
}

void AppendKeyBool(std::string& out, const char* key, bool val) {
    AppendEscaped(out, key);
    out += val ? ":true" : ":false";
}

}  // namespace

void BatchResult::WriteJson(const std::string& path) const {
    if (path.empty()) return;

    std::string out;
    out += "{\n  ";
    AppendKeyVal(out, "status", overallStatus);
    if (!overallError.empty()) {
        out += ",\n  ";
        AppendKeyVal(out, "error", overallError);
    }
    out += ",\n  ";
    AppendKeyVal(out, "plugin", plugin);
    out += ",\n  ";
    AppendKeyBool(out, "saved", saved);
    out += ",\n  \"scripts\": [";
    for (size_t i = 0; i < scripts.size(); ++i) {
        const auto& r = scripts[i];
        out += i ? ",\n    {" : "\n    {";
        out += "\n      ";
        AppendKeyVal(out, "editor_id", r.editorId);
        out += ",\n      ";
        AppendKeyHex32(out, "form_id", r.formId);
        out += ",\n      ";
        AppendKeyBool(out, "compile_ok", r.compileOk);
        if (!r.error.empty()) {
            out += ",\n      ";
            AppendKeyVal(out, "error", r.error);
        }
        out += ",\n      ";
        AppendKeyU64(out, "scda_length_before", r.scdaLengthBefore);
        out += ",\n      ";
        AppendKeyU64(out, "scda_length_after", r.scdaLengthAfter);
        out += ",\n      ";
        AppendKeyVal(out, "scda_sha256_before", r.scdaSha256Before);
        out += ",\n      ";
        AppendKeyVal(out, "scda_sha256_after", r.scdaSha256After);
        out += "\n    }";
    }
    out += "\n  ]\n}\n";

    FILE* fp = nullptr;
    if (fopen_s(&fp, path.c_str(), "wb") != 0 || !fp) {
        OBC_LOG("Result: failed to open '%s' for write (errno=%d)",
                path.c_str(), errno);
        return;
    }
    fwrite(out.data(), 1, out.size(), fp);
    fclose(fp);
    OBC_LOG("Result: wrote %zu bytes to %s", out.size(), path.c_str());
}

}  // namespace obc
