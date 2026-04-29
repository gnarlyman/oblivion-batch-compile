#include "cse_interop.h"

#include <Windows.h>

#include <cstdint>

#include "log.h"

namespace obc::cse {

namespace {

// Subset of CSE's CSEInterfaceTable layout (from
// [Common]/ComponentDLLInterface.h). We only use ScriptEditor's
// RecompileAllScriptsInActiveFile and the surrounding pointers; the
// remainder of the struct is opaque to us. Layout MUST match exactly,
// or the function-pointer offsets will be wrong.
//
// Each interface class is non-virtual and contains only function pointers
// (4 bytes each on x86), so we can model it as an array of void* with
// known indices.

struct IEditorAPI_Layout {
    void* fn[20];                    // DebugPrint .. PrintToConsoleContext (20 funcs)
};

struct IScriptEditor_Layout {
    // 0  CreateNewScript
    // 1  DestroyScriptInstance
    // 2  IsUnsavedNewScript
    // 3  CompileScript
    // 4  RecompileAllScriptsInActiveFile         <-- the one we want
    // 5  ToggleScriptCompilation
    // 6  DeleteScript
    // 7  GetPreviousScriptInList
    // 8  GetNextScriptInList
    // 9  RemoveScriptBytecode
    // 10 SaveEditorBoundsToINI
    // 11 GetScriptList
    // 12 GetScriptVarList
    // 13 UpdateScriptVarIndices
    // 14 CompileDependencies
    // 15 GetIntelliSenseUpdateData
    // 16 BindScript
    // 17 SetScriptText
    // 18 UpdateScriptVarNames
    // 19 CanUpdateIntelliSenseDatabase
    // 20 GetDefaultCachePath
    // 21 GetAutoRecoveryCachePath
    // 22 GetPreprocessorBasePath
    // 23 GetPreprocessorStandardPath
    // 24 GetSnippetCachePath
    // 25 AllocateVarRenameData
    // 26 AllocateCompileData
    void* fn[27];

    using FnRecompileAllScriptsInActiveFile = void(__cdecl*)(void);
    FnRecompileAllScriptsInActiveFile RecompileAllScriptsInActiveFile() const {
        return reinterpret_cast<FnRecompileAllScriptsInActiveFile>(fn[4]);
    }
};

struct IUseInfoList_Layout {
    void* fn[3];
};

struct ITagBrowser_Layout {
    void* fn[3];
};

struct CSEInterfaceTable_Layout {
    void* DeleteInterOpData;
    IEditorAPI_Layout       EditorAPI;
    IScriptEditor_Layout    ScriptEditor;
    IUseInfoList_Layout     UseInfoList;
    ITagBrowser_Layout      TagBrowser;
};

// ScriptEditor.dll's QueryInterface returns ScriptEditorInterface. From
// [Common]/ComponentDLLInterface.h: 12 function pointers, with
// PreprocessScript at index 10.
struct ScriptEditorInterface_Layout {
    void* fn[12];
    using FnPreprocessScript = bool (__cdecl*)(const char*, char*, unsigned);
    FnPreprocessScript Preprocess() const {
        return reinterpret_cast<FnPreprocessScript>(fn[10]);
    }
};

using FnQueryInterface = void* (__cdecl*)(void);

CSEInterfaceTable_Layout*       g_table = nullptr;
ScriptEditorInterface_Layout*   g_seTable = nullptr;
bool                            g_attempted = false;
bool                            g_attemptedSE = false;

}  // namespace

bool TryAcquire() {
    if (g_table) return true;
    if (g_attempted) return false;
    g_attempted = true;

    HMODULE mod = GetModuleHandleA("Construction Set Extender.dll");
    if (!mod) {
        OBC_LOG("CSEInterop: 'Construction Set Extender.dll' not loaded — CSE not active");
        return false;
    }
    auto qi = reinterpret_cast<FnQueryInterface>(GetProcAddress(mod, "QueryInterface"));
    if (!qi) {
        OBC_LOG("CSEInterop: 'QueryInterface' export not found on CSE module");
        return false;
    }
    g_table = static_cast<CSEInterfaceTable_Layout*>(qi());
    if (!g_table) {
        OBC_LOG("CSEInterop: QueryInterface() returned null");
        return false;
    }
    OBC_LOG("CSEInterop: acquired CSEInterfaceTable at %p", static_cast<void*>(g_table));
    return true;
}

bool TryAcquirePreprocessor() {
    if (g_seTable) return true;
    if (g_attemptedSE) return false;
    g_attemptedSE = true;

    HMODULE mod = GetModuleHandleA("ScriptEditor.dll");
    if (!mod) {
        OBC_LOG("CSEInterop: 'ScriptEditor.dll' not loaded");
        return false;
    }
    auto qi = reinterpret_cast<FnQueryInterface>(GetProcAddress(mod, "QueryInterface"));
    if (!qi) {
        OBC_LOG("CSEInterop: 'QueryInterface' export not found on ScriptEditor.dll");
        return false;
    }
    g_seTable = static_cast<ScriptEditorInterface_Layout*>(qi());
    if (!g_seTable) {
        OBC_LOG("CSEInterop: ScriptEditor QueryInterface() returned null");
        return false;
    }
    OBC_LOG("CSEInterop: acquired ScriptEditorInterface at %p; PreprocessScript=%p",
            static_cast<void*>(g_seTable),
            reinterpret_cast<void*>(g_seTable->Preprocess()));
    return true;
}

bool Preprocess(const char* scriptText, char* outBuffer, unsigned bufferSize) {
    if (!TryAcquirePreprocessor()) return false;
    auto fn = g_seTable->Preprocess();
    if (!fn) return false;
    return fn(scriptText, outBuffer, bufferSize);
}

bool RecompileAllScriptsInActiveFile() {
    if (!TryAcquire()) return false;

    auto fn = g_table->ScriptEditor.RecompileAllScriptsInActiveFile();
    if (!fn) {
        OBC_LOG("CSEInterop: RecompileAllScriptsInActiveFile pointer is null");
        return false;
    }

    OBC_LOG("CSEInterop: invoking CSE's RecompileAllScriptsInActiveFile");
    fn();
    OBC_LOG("CSEInterop: RecompileAllScriptsInActiveFile returned");
    return true;
}

}  // namespace obc::cse
