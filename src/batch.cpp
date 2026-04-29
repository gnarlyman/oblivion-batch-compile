#include "batch.h"

#include <Windows.h>

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "compiler_patches.h"
#include "cs_internals.h"
#include "cse_interop.h"
#include "log.h"
#include "result.h"
#include "sha256.h"

namespace obc {

namespace {

std::string ToHex32(std::uint32_t v) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%08X", v);
    return buf;
}

// Capture (length, sha256) of a script's SCDA bytes.
void CaptureScda(cs::Script* s, std::size_t& outLen, std::string& outHash) {
    outLen = s->info.dataLength;
    if (s->data && outLen > 0) {
        outHash = Sha256Hex(s->data, outLen);
    } else {
        outHash.clear();
    }
}

// Iterate active-file SCPTs from the data handler, optionally filtering by formId.
void CollectTargets(cs::TESDataHandler* dh, bool hasFormId, std::uint32_t formId,
                    std::vector<cs::Script*>& out) {
    auto* node = &dh->scripts.head;
    while (node) {
        cs::Script* s = node->item;
        if (s) {
            const bool fromActive = (s->formFlags & cs::kFormFlags_FromActiveFile) != 0;
            const bool notDeleted = (s->formFlags & cs::kFormFlags_Deleted) == 0;
            const bool formIdMatches = !hasFormId || s->formID == formId;
            if (fromActive && notDeleted && formIdMatches) {
                out.push_back(s);
            }
        }
        node = node->next;
    }
}

bool LoadTargetPlugin(const std::string& target, cs::TESFile*& outFile,
                      std::string& errorOut) {
    auto* dh = cs::DataHandler();
    if (!dh) {
        errorOut = "TESDataHandler singleton unavailable";
        return false;
    }
    cs::TESFile* file = cs::LookupPluginByName(dh, target.c_str());
    if (!file) {
        errorOut = "plugin '" + target + "' not found in Data folder";
        return false;
    }
    OBC_LOG("Load: found plugin '%s' at %p", target.c_str(), static_cast<void*>(file));

    cs::FileSetActive(file, true);
    cs::FileSetLoaded(file, true);

    HWND hwnd = cs::CSWindow();
    if (!hwnd) {
        errorOut = "CS main window unavailable";
        return false;
    }

    OBC_LOG("Load: installing AutoLoad bypass + sending WM_COMMAND kToolbar_DataFiles");
    cs::InstallAutoLoadBypass();
    SendMessageA(hwnd, WM_COMMAND, cs::kToolbar_DataFiles, 0);
    cs::RemoveAutoLoadBypass();
    OBC_LOG("Load: data-files load returned (synchronous SendMessage)");

    outFile = file;
    return true;
}

void RecompileScripts(const std::vector<cs::Script*>& targets, BatchResult& result) {
    for (auto* s : targets) {
        ScriptResult r;
        r.editorId = s->editorID.c_str();
        r.formId   = s->formID;

        CaptureScda(s, r.scdaLengthBefore, r.scdaSha256Before);
        OBC_LOG("Compile: %s (FormID %08X) — SCDA before: %zu bytes",
                r.editorId.c_str(), r.formId, r.scdaLengthBefore);

        bool ok = false;
        try {
            ok = cs::ScriptCompile(s, /*asResultScript=*/false);
        } catch (...) {
            r.error = "exception during Compile()";
        }

        CaptureScda(s, r.scdaLengthAfter, r.scdaSha256After);
        r.compileOk = ok && (s->compileResult != 0);
        if (!r.compileOk && r.error.empty()) {
            r.error = ok ? "Compile() returned true but compileResult flag is zero"
                         : "Compile() returned false";
        }
        OBC_LOG("Compile: %s — ok=%d, SCDA after: %zu bytes",
                r.editorId.c_str(), r.compileOk ? 1 : 0, r.scdaLengthAfter);

        result.scripts.push_back(std::move(r));
    }
}

void SaveAndClose(BatchResult& result) {
    HWND hwnd = cs::CSWindow();
    if (!hwnd) {
        OBC_LOG("Save: CS window unavailable; skipping save");
        return;
    }

    OBC_LOG("Save: sending WM_COMMAND kToolbar_Save");
    SendMessageA(hwnd, WM_COMMAND, cs::kToolbar_Save, 0);
    result.saved = true;
    OBC_LOG("Save: returned");

    // Suppress the "Save changes before exiting?" prompt.
    *reinterpret_cast<std::uint8_t*>(cs::kAddr_TESCSMain_UnsavedChangesFlag) = 0;
    OBC_LOG("Exit: posting WM_CLOSE");
    PostMessageA(hwnd, WM_CLOSE, 0, 0);
}

}  // namespace

void RunBatch(const BatchConfig& cfg) {
    BatchResult result;
    result.plugin = cfg.targetPlugin;
    result.overallStatus = "ok";

    OBC_LOG("RunBatch: starting on plugin '%s'", cfg.targetPlugin.c_str());

    // Compiler-error detours: if CSE is loaded, its patches are already
    // active at the same byte addresses we'd patch. Overwriting them
    // with ours breaks CSE's pipeline because our handlers reference
    // different globals than CSE's internal code expects. Apply ours
    // only as a fallback when CSE is absent.
    if (cse::TryAcquire()) {
        OBC_LOG("RunBatch: CSE detected; deferring to its compiler-error patches");
    } else {
        cs::PatchCompilerErrorDetours();
    }

    cs::TESFile* file = nullptr;
    if (!LoadTargetPlugin(cfg.targetPlugin, file, result.overallError)) {
        result.overallStatus = "error";
        OBC_LOG("RunBatch: load failed — %s", result.overallError.c_str());
        result.WriteJson(cfg.resultPath);
        // Still close the CS so the wrapper script terminates.
        if (HWND h = cs::CSWindow()) PostMessageA(h, WM_CLOSE, 0, 0);
        return;
    }

    auto* dh = cs::DataHandler();
    std::vector<cs::Script*> targets;
    CollectTargets(dh, cfg.hasFormId, cfg.formId, targets);
    OBC_LOG("RunBatch: %zu eligible SCPT record(s) found", targets.size());

    if (targets.empty()) {
        result.overallStatus = "error";
        result.overallError = cfg.hasFormId
            ? "no active-file SCPT matched FormID filter " + ToHex32(cfg.formId)
            : "no active-file SCPT records found in target plugin";
        OBC_LOG("RunBatch: %s", result.overallError.c_str());
        result.WriteJson(cfg.resultPath);
        if (HWND h = cs::CSWindow()) PostMessageA(h, WM_CLOSE, 0, 0);
        return;
    }

    // Capture SCDA-before for each target so we can compare regardless of
    // which compile path we take.
    std::vector<ScriptResult> rs;
    rs.reserve(targets.size());
    for (auto* s : targets) {
        ScriptResult r;
        r.editorId = s->editorID.c_str();
        r.formId   = s->formID;
        CaptureScda(s, r.scdaLengthBefore, r.scdaSha256Before);
        rs.push_back(std::move(r));
    }

    // If CSE's preprocessor is loaded, mirror CSE's RecompileScripts
    // pattern (preprocess SCTX → SetText(preprocessed) → Compile →
    // SetText(original)) so macros / long lines are handled before the
    // bare CS compiler sees the source.
    constexpr unsigned kPreprocBufSize = 2u * 1024u * 1024u;
    char* preprocBuf = nullptr;
    bool havePreprocessor = cse::TryAcquirePreprocessor();
    if (havePreprocessor) {
        preprocBuf = static_cast<char*>(std::malloc(kPreprocBufSize));
        if (!preprocBuf) havePreprocessor = false;
    }

    for (size_t i = 0; i < targets.size(); ++i) {
        cs::Script* s = targets[i];
        ScriptResult& r = rs[i];

        std::string originalText = s->text ? s->text : "";

        if (havePreprocessor) {
            std::memset(preprocBuf, 0, kPreprocBufSize);
            bool ppOk = cse::Preprocess(originalText.c_str(), preprocBuf, kPreprocBufSize);
            if (ppOk) {
                cs::ScriptSetText(s, preprocBuf);
                OBC_LOG("Compile: %s — preprocessor produced %zu bytes",
                        r.editorId.c_str(), std::strlen(preprocBuf));
            } else {
                OBC_LOG("Compile: %s — preprocessor returned false; compiling raw SCTX",
                        r.editorId.c_str());
            }
        }

        bool ok = false;
        try {
            ok = cs::ScriptCompile(s, /*asResultScript=*/false);
        } catch (...) {
            r.error = "exception during Compile()";
        }

        if (havePreprocessor) {
            cs::ScriptSetText(s, originalText.c_str());
        }

        CaptureScda(s, r.scdaLengthAfter, r.scdaSha256After);
        r.compileOk = ok && (s->compileResult != 0);
        if (!r.compileOk && r.error.empty()) {
            r.error = ok ? "Compile() returned true but compileResult flag is zero"
                         : "Compile() returned false";
        }
        OBC_LOG("Compile: %s — ok=%d, SCDA after: %zu bytes",
                r.editorId.c_str(), r.compileOk ? 1 : 0, r.scdaLengthAfter);

        result.scripts.push_back(std::move(r));
    }

    if (preprocBuf) std::free(preprocBuf);

    SaveAndClose(result);
    result.WriteJson(cfg.resultPath);
    OBC_LOG("RunBatch: complete");
}

}  // namespace obc
