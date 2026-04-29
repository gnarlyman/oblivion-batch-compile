#pragma once
//
// cse_interop.h
//
// Locates Construction Set Extender's exported interface at runtime so we
// can delegate to CSE's full preprocess+compile pipeline instead of just
// the bare CS-internal Compile() call. CSE.dll is loaded by OBSE before
// our plugin (alphabetical), so by RunBatch time the module is in memory
// and we can resolve its `QueryInterface` export.

namespace obc::cse {

// Returns true if Construction Set Extender's main DLL is loaded and we
// successfully resolved its CSEInterfaceTable. Idempotent.
bool TryAcquire();

// Wrapper for the CSE function `RecompileAllScriptsInActiveFile`. Runs
// CSE's full pipeline: for each active-file SCPT, preprocess SCTX → swap
// in preprocessed text → call Compile() → restore original SCTX. Returns
// false if the CSE interface isn't available.
//
// CAVEAT: tends to crash when invoked outside CSE's normal menu-click
// context (CSE assumes intellisense / dialog state that we don't set up).
// Prefer Preprocess() instead.
bool RecompileAllScriptsInActiveFile();

// Locate ScriptEditor.dll (loaded as a CSE component) and resolve its
// PreprocessScript export. Returns true on success.
bool TryAcquirePreprocessor();

// Run CSE's preprocessor on `scriptText`. Writes expanded source into
// `outBuffer` (must be large enough — CSE uses 2 MiB). Returns true on
// success. Returns false if the preprocessor isn't available or if it
// reported a parse error.
bool Preprocess(const char* scriptText, char* outBuffer, unsigned bufferSize);

}  // namespace obc::cse
