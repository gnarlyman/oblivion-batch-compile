#include "cs_internals.h"

#include <cstring>

#include "log.h"

namespace obc::cs {

namespace {

// Inline helpers for invoking native functions inside the CS exe at fixed
// addresses. We cast through __thiscall function-pointer types so MSVC
// emits the correct prologue (ecx = this) and stack handling.

template <class R, class... Args>
inline R CdeclAt(std::uintptr_t addr, Args... args) {
    using Fn = R(__cdecl*)(Args...);
    return reinterpret_cast<Fn>(addr)(args...);
}

template <class R, class... Args>
inline R ThisCallAt(std::uintptr_t addr, void* self, Args... args) {
    using Fn = R(__thiscall*)(void*, Args...);
    return reinterpret_cast<Fn>(addr)(self, args...);
}

void SafeWriteBytes(std::uintptr_t addr, const UInt8* bytes, std::size_t len) {
    DWORD oldProt = 0;
    auto target = reinterpret_cast<LPVOID>(addr);
    if (!VirtualProtect(target, len, PAGE_EXECUTE_READWRITE, &oldProt)) {
        OBC_LOG("SafeWriteBytes: VirtualProtect RW failed at 0x%08X (err=%lu)",
                static_cast<unsigned>(addr), GetLastError());
        return;
    }
    std::memcpy(target, bytes, len);
    DWORD restored = 0;
    VirtualProtect(target, len, oldProt, &restored);
    FlushInstructionCache(GetCurrentProcess(), target, len);
}

}  // namespace

bool ScriptCompile(Script* script, bool asResultScript) {
    if (!script || !script->text) return false;
    if (asResultScript) {
        // CSE: Script.cpp:47
        //   thisCall<bool>(0x005034E0, 0x00A0B128, this, 0, 0)
        // The "this" passed is actually the global compiler context at
        // 0x00A0B128, with the script and two zero flags as args.
        using Fn = bool(__thiscall*)(void*, Script*, int, int);
        return reinterpret_cast<Fn>(kAddr_Script_Compile_Result)(
            reinterpret_cast<void*>(kAddr_ScriptCompiler_Context), script, 0, 0);
    } else {
        // CSE: Script.cpp:49
        //   thisCall<bool>(0x00503450, 0x00A0B128, this, 0)
        using Fn = bool(__thiscall*)(void*, Script*, int);
        return reinterpret_cast<Fn>(kAddr_Script_Compile_Object)(
            reinterpret_cast<void*>(kAddr_ScriptCompiler_Context), script, 0);
    }
}

void ScriptSetText(Script* script, const char* text) {
    // CSE: Script.cpp:54 — thisCall<UInt32>(0x004FC6C0, this, Text)
    ThisCallAt<UInt32, const char*>(kAddr_Script_SetText, script, text);
}

bool FileSetActive(TESFile* file, bool state) {
    return ThisCallAt<bool, bool>(kAddr_TESFile_SetActive, file, state);
}

bool FileSetLoaded(TESFile* file, bool state) {
    return ThisCallAt<bool, bool>(kAddr_TESFile_SetLoaded, file, state);
}

bool DataHandlerSavePlugin(TESDataHandler* dh, const char* fileName, bool asESM) {
    return ThisCallAt<bool, const char*, bool>(kAddr_TESDataHandler_SavePlugin,
                                               dh, fileName, asESM);
}

TESFile* LookupPluginByName(TESDataHandler* dh, const char* fileName) {
    if (!dh || !fileName) return nullptr;
    // Walk fileList. tList in the CS keeps the first item in head.item;
    // additional items chain via head.next->item.
    auto* node = &dh->fileList.head;
    while (node) {
        TESFile* f = node->item;
        if (f && _stricmp(f->fileName, fileName) == 0) return f;
        node = node->next;
    }
    return nullptr;
}

void InstallAutoLoadBypass() {
    SafeWriteBytes(kAddr_AutoLoadHook_Site, kAutoLoadHook_PatchBytes, kAutoLoadHook_Size);
}

void RemoveAutoLoadBypass() {
    SafeWriteBytes(kAddr_AutoLoadHook_Site, kAutoLoadHook_OriginalBytes, kAutoLoadHook_Size);
}

}  // namespace obc::cs
