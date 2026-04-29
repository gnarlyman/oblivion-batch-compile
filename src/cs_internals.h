#pragma once
//
// cs_internals.h
//
// CS-internal struct layouts and hardcoded function/variable addresses for
// TESConstructionSet.exe v1.2 (Oblivion 1.2.0416 Construction Set).
//
// All offsets, member layouts, and type signatures are derived from
// shadeMe/Construction-Set-Extender (GPL-3.0). Cited paths reference the
// upstream CSE repo.
//
// Comments format: `// CSE: <path>:<line>`

#include <Windows.h>
#include <cstdint>
#include <cstddef>

namespace obc::cs {

using UInt8 = std::uint8_t;
using UInt16 = std::uint16_t;
using UInt32 = std::uint32_t;

// -----------------------------------------------------------------------
// Address table
// -----------------------------------------------------------------------

// CSE: EditorAPI/Core.cpp:7
inline constexpr std::uintptr_t kAddr_TESDataHandler_Singleton = 0x00A0E064;

// CSE: EditorAPI/TESDialog.cpp:19
inline constexpr std::uintptr_t kAddr_TESCSMain_WindowHandle = 0x00A0AF20;

// CSE: EditorAPI/TESDialog.cpp:24
inline constexpr std::uintptr_t kAddr_TESCSMain_ExittingCSFlag    = 0x00A0B63C;

// CSE: EditorAPI/TESDialog.cpp:25 — clear before WM_CLOSE to skip save-prompt.
inline constexpr std::uintptr_t kAddr_TESCSMain_UnsavedChangesFlag = 0x00A0B13C;

// CSE: EditorAPI/Script.cpp:6 (TESScriptCompiler::ShowMessage; not used directly).
// Function offsets used via thiscall:
inline constexpr std::uintptr_t kAddr_Script_Compile_Object   = 0x00503450;  // CSE: Script.cpp:49
inline constexpr std::uintptr_t kAddr_Script_Compile_Result   = 0x005034E0;  // CSE: Script.cpp:47
inline constexpr std::uintptr_t kAddr_ScriptCompiler_Context  = 0x00A0B128;  // CSE: Script.cpp:47/49 (passed as first arg)

inline constexpr std::uintptr_t kAddr_Script_SetText          = 0x004FC6C0;  // CSE: Script.cpp:54

inline constexpr std::uintptr_t kAddr_TESFile_SetLoaded       = 0x00485B70;  // CSE: TESFile.cpp:34
inline constexpr std::uintptr_t kAddr_TESFile_SetActive       = 0x00485BB0;  // CSE: TESFile.cpp:39

inline constexpr std::uintptr_t kAddr_TESDataHandler_SavePlugin = 0x0047E9B0;  // CSE: Core.cpp:204

// CSE: EditorAPI/TESDialog.h:702-703
inline constexpr int kToolbar_DataFiles = 40145;
inline constexpr int kToolbar_Save      = 40146;

// AutoLoad bypass patch — see CSE: Hooks/Hooks-Plugins.cpp:25, hook fn line 224.
// The hook replaces 6 bytes at 0x0041A26A so the CS skips the Data Files
// dialog and proceeds with whatever plugin is already marked active+loaded.
//
// Equivalent inline patch: `mov eax, 1; jmp 0x0041A284; nop`.
inline constexpr std::uintptr_t kAddr_AutoLoadHook_Site = 0x0041A26A;
inline constexpr std::size_t    kAutoLoadHook_Size = 6;
inline constexpr UInt8 kAutoLoadHook_PatchBytes[kAutoLoadHook_Size]   = { 0x33, 0xC0, 0x40, 0xEB, 0x15, 0x90 };
inline constexpr UInt8 kAutoLoadHook_OriginalBytes[kAutoLoadHook_Size] = { 0x8B, 0x0D, 0x44, 0xB6, 0xA0, 0x00 };

// -----------------------------------------------------------------------
// tList<T> — CS's intrusive linked list. CSE: many headers.
// We only need begin/iterate semantics.
// -----------------------------------------------------------------------
template <class T>
struct tListNode {
    T*           item;
    tListNode<T>* next;
};

template <class T>
struct tList {
    tListNode<T> head;
    // CS appends new nodes; the head's `item` is the first element (or null
    // if list is empty), and `next` chains additional nodes.
};

// -----------------------------------------------------------------------
// BSString. CSE: EditorAPI/BSString.h
// -----------------------------------------------------------------------
struct BSString {
    char*  m_data;
    UInt16 m_dataLen;
    UInt16 m_bufLen;

    const char* c_str() const { return m_data ? m_data : ""; }
};

// -----------------------------------------------------------------------
// TESForm. CSE: EditorAPI/TESForm.h:59
// We only model the prefix we need.
// -----------------------------------------------------------------------
enum FormFlags : UInt32 {
    kFormFlags_FromMaster     = 0x00000001,  // CSE: TESForm.h:142
    kFormFlags_FromActiveFile = 0x00000002,  // CSE: TESForm.h:143
    kFormFlags_Deleted        = 0x00000020,  // CSE: TESForm.h:145
};

// TrackingData. CSE: TESForm.h:51 — sizeof == 4.
struct TrackingData {
    UInt16 date;
    UInt8  lastUser;
    UInt8  currentUser;
};
static_assert(sizeof(TrackingData) == 4, "TrackingData layout");

// TESForm. CSE: TESForm.h:59 — sizeof == 0x24.
struct TESForm {
    void*        vtbl;            // 0x00 (BaseFormComponent vtable)
    UInt8        formType;        // 0x04
    UInt8        pad05[3];        // 0x05
    UInt32       formFlags;       // 0x08
    UInt32       formID;          // 0x0C
    BSString     editorID;        // 0x10
    TrackingData trackingData;    // 0x18
    UInt32       fileListHead[2]; // 0x1C — tList<TESFile> opaque
};
static_assert(sizeof(TESForm) == 0x24, "TESForm size mismatch");
static_assert(offsetof(TESForm, formFlags) == 0x08, "TESForm::formFlags offset");
static_assert(offsetof(TESForm, formID)    == 0x0C, "TESForm::formID offset");
static_assert(offsetof(TESForm, editorID)  == 0x10, "TESForm::editorID offset");

// -----------------------------------------------------------------------
// Script. CSE: EditorAPI/Script.h:14, sizeof == 0x54.
// -----------------------------------------------------------------------
struct ScriptInfo {
    UInt32 unusedVarCount;  // 00
    UInt32 refCount;        // 04
    UInt32 dataLength;      // 08 — size of `data` (SCDA byte count)
    UInt32 lastVarIdx;      // 0C
    UInt32 type;            // 10
};
static_assert(sizeof(ScriptInfo) == 0x14, "ScriptInfo layout");

struct ScriptVariableList { UInt32 head[2]; };  // sized for tList — opaque
struct ScriptRefVarList   { UInt32 head[2]; };  // sized for tList — opaque

struct Script {
    // 00 — TESForm base (must match TESForm layout exactly).
    void*        vtbl;
    UInt8        formType;
    UInt8        pad05[3];
    UInt32       formFlags;
    UInt32       formID;
    BSString     editorID;
    TrackingData trackingData;
    UInt32       fileListHead[2];
    // 24 — Script-specific.
    ScriptInfo         info;          // /*24*/
    char*              text;          // /*38*/ — SCTX
    void*              data;          // /*3C*/ — SCDA bytecode
    ScriptRefVarList   refList;       // /*40*/
    ScriptVariableList varList;       // /*48*/
    UInt8              compileResult; // /*50*/
    UInt8              pad51[3];
};
static_assert(sizeof(Script) == 0x54, "Script size mismatch with CSE");
static_assert(offsetof(Script, info)  == 0x24, "Script::info offset");
static_assert(offsetof(Script, text)  == 0x38, "Script::text offset");
static_assert(offsetof(Script, data)  == 0x3C, "Script::data offset");

// -----------------------------------------------------------------------
// TESFile. CSE: EditorAPI/TESFile.h
// We only need fileName, filePath, and the fileFlags bit testing surface.
// -----------------------------------------------------------------------
constexpr std::size_t kMAX_PATH_FILE = 0x104;

struct TESFile {
    UInt8 prefix[0x01C];
    char  fileName[kMAX_PATH_FILE];   // /*01C*/ CSE: TESFile.h:120
    char  filePath[kMAX_PATH_FILE];   // /*120*/ CSE: TESFile.h:121
    // … remainder elided.
};

// -----------------------------------------------------------------------
// TESDataHandler. CSE: EditorAPI/Core.h:75, sizeof == 0x1220.
// We need: scripts list, activeFile, file list, and the SavePlugin method.
// -----------------------------------------------------------------------
struct TESDataHandler {
    UInt8 prefix0064[0x0064];                          // CSE: Core.h:79-91
    tList<Script> scripts;                              // /*0064*/ CSE: Core.h:92
    UInt8 mid[0x0E04 - 0x0064 - sizeof(tList<Script>)]; // CSE: Core.h:93-107
    TESFile* activeFile;                                // /*0E04*/ CSE: Core.h:108
    tList<TESFile> fileList;                            // /*0E08*/ CSE: Core.h:109
    UInt8 tail[0x1220 - 0x0E10];                        // CSE: Core.h:110+
};
static_assert(offsetof(TESDataHandler, scripts)    == 0x0064, "TESDataHandler::scripts offset");
static_assert(offsetof(TESDataHandler, activeFile) == 0x0E04, "TESDataHandler::activeFile offset");
static_assert(offsetof(TESDataHandler, fileList)   == 0x0E08, "TESDataHandler::fileList offset");
static_assert(sizeof(TESDataHandler) == 0x1220, "TESDataHandler size mismatch with CSE");

// -----------------------------------------------------------------------
// Accessors
// -----------------------------------------------------------------------
inline TESDataHandler* DataHandler() {
    return *reinterpret_cast<TESDataHandler**>(kAddr_TESDataHandler_Singleton);
}
inline HWND CSWindow() {
    return *reinterpret_cast<HWND*>(kAddr_TESCSMain_WindowHandle);
}

// -----------------------------------------------------------------------
// Function call helpers
// -----------------------------------------------------------------------
//
// MSVC __thiscall does not support free-function pointer types directly;
// we use a stub that calls through the address with the correct calling
// convention. The CS exe is x86, so __thiscall = ecx-this + cdecl-args.

bool   ScriptCompile(Script* script, bool asResultScript);
void   ScriptSetText(Script* script, const char* text);

bool   FileSetActive(TESFile* file, bool state);
bool   FileSetLoaded(TESFile* file, bool state);

bool   DataHandlerSavePlugin(TESDataHandler* dh, const char* fileName, bool asESM);

TESFile* LookupPluginByName(TESDataHandler* dh, const char* fileName);

// -----------------------------------------------------------------------
// Auto-load bypass: install / remove the 6-byte patch around a load op.
// -----------------------------------------------------------------------
void InstallAutoLoadBypass();
void RemoveAutoLoadBypass();

}  // namespace obc::cs
