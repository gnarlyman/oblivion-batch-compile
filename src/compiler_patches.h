#pragma once

namespace obc::cs {

// Apply CSE's compiler-error-detour patches to the running CS process.
// These let TESScriptCompiler::CompileScript() return success on scripts
// that vanilla CS would reject (e.g., lines >512 chars). Lifted from
// shadeMe/Construction-Set-Extender Hooks/Hooks-CompilerErrorDetours.cpp
// (GPL-3.0). Idempotent — safe to call once per process.
void PatchCompilerErrorDetours();

}  // namespace obc::cs
