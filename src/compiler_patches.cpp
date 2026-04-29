// compiler_patches.cpp
//
// Replicates Construction Set Extender's compiler-error-detour patches
// without requiring CSE itself. These let TESScriptCompiler::CompileScript()
// (at 0x00503450) return success on scripts that vanilla CS would reject —
// e.g., long-line errors, missing-ref errors, etc. — by suppressing the
// failure paths and rerouting error reporting.
//
// All hook addresses, asm bodies, and the PatchCompilerErrorDetours
// orchestration are lifted verbatim from
// shadeMe/Construction-Set-Extender Hooks/Hooks-CompilerErrorDetours.cpp
// (GPL-3.0). Diffs vs. upstream:
//   - BGSEECONSOLE_MESSAGE / BGSEEUI logging removed (we have no UI).
//   - TESScriptCompiler::CompilerMessage / LastCompilationMessages
//     omitted; we only need the result-buffer mechanic, not the per-error
//     logging that CSE wires into its console.
//
// SME-Sundries' MemoryHandler.h supplies the `_DefineHookHdlr` /
// `_DefineNopHdlr` macros and the `MemHdlr` / `NopHdlr` runtime classes.

#include <SME_Prefix.h>
#include <MemoryHandler.h>

#include "compiler_patches.h"
#include "log.h"

#pragma warning(push)
#pragma warning(disable: 4005 4748)

// -----------------------------------------------------------------------
// Minimal TESScriptCompiler shim — exposes the static fields and the
// ShowMessage function pointer that the lifted asm hooks reference.
// -----------------------------------------------------------------------
namespace TESScriptCompiler {
    typedef void (__cdecl *_ShowMessage)(void* Buffer, const char* Format, ...);
    _ShowMessage ShowMessage = (_ShowMessage)0x004FFF40;
    bool         PreventErrorDetours = false;
    bool         PrintErrorsToConsole = false;  // CSE prints to its console; we silence.
}

// -----------------------------------------------------------------------
// Per-compile state. CSE keeps these as namespace globals so the inline
// asm can reference them by name. We do the same.
// -----------------------------------------------------------------------
UInt32 ScriptCompileResultBuffer = 0;
UInt32 MaxScriptSizeExceeded     = 0;

// -----------------------------------------------------------------------
// _hhBegin et al. live in SME::MemoryHandler. We need a typedef so the
// macro-emitted symbol names resolve correctly.
// -----------------------------------------------------------------------
using SME::MemoryHandler::MemHdlr;
using SME::MemoryHandler::NopHdlr;

// -----------------------------------------------------------------------
// Compiler-error override macro (paraphrased from CSE's
// Hooks-CompilerErrorDetours.h). Generates a naked function that:
//   call ShowMessage          ; original error-reporting call
//   mov  ScriptCompileResultBuffer, 0
//   add  esp, stackoffset
//   jmp  jmpaddr               ; clean exit point
// Then registers a MemHdlr to detour `hookaddr` to the new function.
// -----------------------------------------------------------------------
#define DefineCompilerErrorOverrideHook(hookaddr, jmpaddr, stackoffset)                  \
    void CompilerErrorOverrideHandler##hookaddr##Hook(void);                             \
    SME::MemoryHandler::MemHdlr                                                          \
        CompilerErrorOverrideHandler##hookaddr(hookaddr,                                 \
                                               CompilerErrorOverrideHandler##hookaddr##Hook, \
                                               0, 0);                                    \
    void __declspec(naked) CompilerErrorOverrideHandler##hookaddr##Hook(void)            \
    {                                                                                    \
        static UInt32 CompilerErrorOverrideHandler##hookaddr##RetnAddr = jmpaddr;        \
        __asm call    TESScriptCompiler::ShowMessage                                     \
        __asm mov     ScriptCompileResultBuffer, 0                                       \
        __asm add     esp, stackoffset                                                   \
        __asm jmp     CompilerErrorOverrideHandler##hookaddr##RetnAddr                   \
    }

#define GetErrorMemHdlr(hookaddr) CompilerErrorOverrideHandler##hookaddr

// =======================================================================
// Hook definitions — addresses + handlers ported verbatim from CSE.
// =======================================================================
namespace obc {
namespace hooks {

// Forward-declare hook functions so the _DefineHookHdlr expansions below
// can take their address. Definitions emitted by `_hhBegin()` further down.
void RerouteScriptErrorsHook(void);
void CompilerPrologResetHook(void);
void CompilerEpilogCheckHook(void);
void ParseScriptLineOverrideHook(void);
void CheckLineLengthLineCountHook(void);
void ResultScriptErrorNotificationHook(void);
void MaxScriptSizeExceededHook(void);
void PrintCompilerErrorToConsoleOverrideHook(void);

_DefineNopHdlr(RidScriptErrorMessageBox,         0x004FFFEC, 20);
_DefineNopHdlr(RidUnknownFunctionCodeMessage,    0x0050310C, 5);
_DefineHookHdlr(RerouteScriptErrors,             0x004FFF9C);
_DefineHookHdlr(CompilerPrologReset,             0x00503330);
_DefineHookHdlr(CompilerEpilogCheck,             0x0050341F);
_DefineHookHdlr(ParseScriptLineOverride,         0x00503401);
_DefineHookHdlr(CheckLineLengthLineCount,        0x0050013B);
_DefineHookHdlr(ResultScriptErrorNotification,   0x005035EE);
_DefineHookHdlr(MaxScriptSizeExceeded,           0x005031DB);
// PrintCompilerErrorToConsoleOverride: see CSE comment about OBSE's 5-byte shift.
_DefineHookHdlr(PrintCompilerErrorToConsoleOverride, 0x00500001 + 5);

// f_ScriptBuffer__ConstructLineBuffers
DefineCompilerErrorOverrideHook(0x00502781, 0x00502791, 0xC)
DefineCompilerErrorOverrideHook(0x00502813, 0x005027AD, 0xC)
DefineCompilerErrorOverrideHook(0x005027D3, 0x00502824, 0xC)
DefineCompilerErrorOverrideHook(0x005028B5, 0x00502889, 0x8)

// f_ScriptCompiler__CheckSyntax
DefineCompilerErrorOverrideHook(0x00500B44, 0x00500B4C, 0x8)
DefineCompilerErrorOverrideHook(0x00500B5D, 0x00500A7E, 0x8)
DefineCompilerErrorOverrideHook(0x00500B76, 0x00500A8B, 0x8)
DefineCompilerErrorOverrideHook(0x00500B8C, 0x00500AAB, 0xC)
DefineCompilerErrorOverrideHook(0x00500BBE, 0x00500B11, 0x8)

DefineCompilerErrorOverrideHook(0x00500BA5, 0x00500B11, 0x8)
DefineCompilerErrorOverrideHook(0x00500C09, 0x00500C18, 0xC)
DefineCompilerErrorOverrideHook(0x00500C81, 0x00500CB6, 0xC)
DefineCompilerErrorOverrideHook(0x00500CA7, 0x00500CB6, 0x8)

// f_ScriptBuffer__ConstructRefVariables
DefineCompilerErrorOverrideHook(0x00500669, 0x00500676, 0xC)
DefineCompilerErrorOverrideHook(0x0050068F, 0x0050069E, 0xC)

// f_ScriptCompiler__CheckScriptBlockStructure
DefineCompilerErrorOverrideHook(0x00500262, 0x0050024F, 0x8)
DefineCompilerErrorOverrideHook(0x0050027D, 0x0050024F, 0x8)
DefineCompilerErrorOverrideHook(0x00500298, 0x0050024F, 0x8)

// f_ScriptBuffer__CheckReferencedObjects
DefineCompilerErrorOverrideHook(0x005001DC, 0x005001C9, 0xC)

// -----------------------------------------------------------------------
// __stdcall helper called from the RerouteScriptErrors hook. CSE captures
// CompilerMessage objects into a list; we just flag the result buffer
// (the only state Compile() reads back) and optionally log.
// -----------------------------------------------------------------------
void __stdcall DoRerouteScriptErrorsHook(UInt32 Line, const char* Message)
{
    // Mirror CSE: errors flag the compile as failed via ScriptCompileResultBuffer = 0.
    // CSE distinguishes errors from warnings by parsing message prefix; we keep it
    // simple — every reroute path corresponds to a compiler ShowMessage emission.
    if (!TESScriptCompiler::PreventErrorDetours) {
        OBC_LOG("CompilerError: line=%u msg=%s", static_cast<unsigned>(Line),
                Message ? Message : "(null)");
    }
    ScriptCompileResultBuffer = 0;
}

// =======================================================================
// Hook bodies — verbatim from CSE except where noted.
// =======================================================================

#define _hhName     RerouteScriptErrors
_hhBegin()
{
    _hhSetVar(Retn, 0x004FFFA5);
    __asm
    {
        mov     [esp + 0x18], ebx
        mov     [esp + 0x1C], bx

        lea     edx, [esp + 0x20]
        pushad
        push    edx
        push    [esi + 0x1C]
        call    DoRerouteScriptErrorsHook
        popad

        jmp     _hhGetVar(Retn)
    }
}

#define _hhName     CompilerEpilogCheck
_hhBegin()
{
    _hhSetVar(Retn, 0x00503424);
    _hhSetVar(Call, 0x00500190);
    __asm
    {
        call    _hhGetVar(Call)
        mov     eax, ScriptCompileResultBuffer

        jmp     _hhGetVar(Retn)
    }
}

#define _hhName     CompilerPrologReset
_hhBegin()
{
    _hhSetVar(Retn, 0x00503336);
    __asm
    {
        mov     ScriptCompileResultBuffer, 1
        mov     MaxScriptSizeExceeded, 0
        push    ebx
        push    ebp
        mov     ebp, [esp + 0xC]

        jmp     _hhGetVar(Retn)
    }
}

#define _hhName     ParseScriptLineOverride
_hhBegin()
{
    _hhSetVar(Retn, 0x0050340A);
    _hhSetVar(Call, 0x005028D0);
    _hhSetVar(Exit, 0x005033BE);
    __asm
    {
        call    _hhGetVar(Call)
        test    al, al
        jz      FAIL

        jmp     _hhGetVar(Retn)
    FAIL:
        mov     ScriptCompileResultBuffer, 0
        mov     eax, MaxScriptSizeExceeded
        test    eax, eax
        jnz     EXIT

        jmp     _hhGetVar(Retn)
    EXIT:
        jmp     _hhGetVar(Exit)
    }
}

#define _hhName     CheckLineLengthLineCount
_hhBegin()
{
    _hhSetVar(Retn, 0x00500143);
    __asm
    {
        mov     eax, [esp + 0x18]
        add     [eax + 0x1C], 1

        add     dword ptr [esi], 1
        push    0x200

        jmp     _hhGetVar(Retn)
    }
}

#define _hhName     ResultScriptErrorNotification
_hhBegin()
{
    _hhSetVar(Retn, 0x005035F3);
    _hhSetVar(Call, 0x00503330);
    __asm
    {
        mov     TESScriptCompiler::PreventErrorDetours, 1
        call    _hhGetVar(Call)
        mov     TESScriptCompiler::PreventErrorDetours, 0

        jmp     _hhGetVar(Retn)
    }
}

#define _hhName     MaxScriptSizeExceeded
_hhBegin()
{
    _hhSetVar(Retn, 0x00502A7C);
    __asm
    {
        mov     MaxScriptSizeExceeded, 1
        push    0x0094AD6C
        jmp     _hhGetVar(Retn)
    }
}

#define _hhName     PrintCompilerErrorToConsoleOverride
_hhBegin()
{
    _hhSetVar(Retn, 0x00500006 + 5);
    __asm
    {
        // CSE conditionally calls a print routine here. We always skip
        // (PrintErrorsToConsole defaulted to false) to keep the run silent.
        jmp     _hhGetVar(Retn)
    }
}

}  // namespace hooks
}  // namespace obc

// =======================================================================
// Public entry: install all patches. CSE's PatchCompilerErrorDetours
// reproduced.
// =======================================================================
namespace obc::cs {

void PatchCompilerErrorDetours()
{
    using namespace obc::hooks;

    OBC_LOG("CompilerPatches: installing CSE-derived compiler-error detours");

    _MemHdlr(RidScriptErrorMessageBox).WriteNop();
    _MemHdlr(RidUnknownFunctionCodeMessage).WriteNop();
    _MemHdlr(RerouteScriptErrors).WriteJump();
    _MemHdlr(CompilerPrologReset).WriteJump();
    _MemHdlr(CompilerEpilogCheck).WriteJump();
    _MemHdlr(ParseScriptLineOverride).WriteJump();
    _MemHdlr(CheckLineLengthLineCount).WriteJump();
    _MemHdlr(ResultScriptErrorNotification).WriteJump();
    _MemHdlr(MaxScriptSizeExceeded).WriteJump();
    _MemHdlr(PrintCompilerErrorToConsoleOverride).WriteJump();

    GetErrorMemHdlr(0x00502781).WriteJump();
    GetErrorMemHdlr(0x00502813).WriteJump();
    GetErrorMemHdlr(0x005027D3).WriteJump();
    GetErrorMemHdlr(0x005028B5).WriteJump();

    GetErrorMemHdlr(0x00500B44).WriteJump();
    GetErrorMemHdlr(0x00500B5D).WriteJump();
    GetErrorMemHdlr(0x00500B76).WriteJump();
    GetErrorMemHdlr(0x00500B8C).WriteJump();
    GetErrorMemHdlr(0x00500BBE).WriteJump();

    GetErrorMemHdlr(0x00500BA5).WriteJump();
    GetErrorMemHdlr(0x00500C09).WriteJump();
    GetErrorMemHdlr(0x00500C81).WriteJump();
    GetErrorMemHdlr(0x00500CA7).WriteJump();

    GetErrorMemHdlr(0x00500669).WriteJump();
    GetErrorMemHdlr(0x0050068F).WriteJump();

    GetErrorMemHdlr(0x00500262).WriteJump();
    GetErrorMemHdlr(0x0050027D).WriteJump();
    GetErrorMemHdlr(0x00500298).WriteJump();

    GetErrorMemHdlr(0x005001DC).WriteJump();

    OBC_LOG("CompilerPatches: 32 patches installed");
}

}  // namespace obc::cs

#pragma warning(pop)
