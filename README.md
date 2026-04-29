# oblivion-batch-compile

Headless OBSE editor plugin that recompiles Oblivion SCPT bytecode
(`SCDA` subrecords) from script source (`SCTX`) without UI interaction.

## Why

Authoring script overrides for Oblivion mods normally requires opening
the Construction Set GUI, double-clicking each script in the editor pane,
and saving. xEdit can edit `SCTX` (source) but cannot recompile `SCDA`
(bytecode) — and the engine runs the bytecode, so xEdit-only overrides
are runtime-inert.

This plugin loads inside the CS process via OBSE editor mode, reads
target plugin + optional FormID from environment variables, drives the
CS through the load → recompile → save → close cycle, and exits. With
[Construction Set Extender][cse] enabled, it uses CSE's preprocessor for
macro/long-line handling; without CSE, it falls back to a port of CSE's
compiler-error detours (handles simpler scripts only).

## Workflow

```
xEdit Pascal authoring (headless) → SCTX-only override in target .esp
                                  ↓
              oblivion-batch-compile (env-var driven)
                                  ↓
        loads .esp → CSE preprocessor → CS compiler → save → exit
                                  ↓
                     status JSON with sha256-before/after
```

End-to-end: ~5–15 seconds, zero clicks.

## Build

Visual Studio 2022 (v143 toolset, Win32). No DirectX SDK, no CrashRpt,
no managed/CLI components.

Header-only deps:
- [shadeMe/SME-Sundries][sme] — only used by `compiler_patches.cpp`
  (the no-CSE fallback path); MemoryHandler.h supplies the hook macros.

```bash
cp EnvVars.props.template EnvVars.props
# Edit OblivionPath / MO2DeployPath / SMESundriesIncludePath.

MSBuild oblivion-batch-compile.sln /p:Configuration=Release /p:Platform=Win32
```

Post-build copies the DLL to `$(MO2DeployPath)` if set.

## Install (MO2)

1. **DLL location:** place at
   `<MO2 instance>/mods/CSE Batch Compile/OBSE/Plugins/oblivion-batch-compile.dll`.
   **Case matters** — USVFS overlays match path segments case-sensitively
   on Windows. `obse/plugins/` will not be visible to OBSE; use exactly
   `OBSE/Plugins/`.
2. **Activate** the "CSE Batch Compile" mod in your profile and check the
   plugin entry in `loadorder.txt`.
3. **MO2 custom-exec entry** (`ModOrganizer.ini` `[customExecutables]`):
   ```
   <n>\title=CSE Batch Compile
   <n>\binary=<oblivion>/Stock Game/obse_loader.exe
   <n>\arguments=-editor -notimeout
   <n>\workingDirectory=<oblivion>/Stock Game
   ```
   **Critical:** point `binary` at the *staged* `Stock Game/obse_loader.exe`,
   **not** the mod-folder original (`mods/.../Root/obse_loader.exe`). MO2's
   `adjustForVirtualized` rewrites mod-folder binary paths to
   `<gameDataDir>/<rest>` — but Root Builder content lives at the game
   *root*, not under `Data/`, so the rewritten path doesn't exist and the
   launch silently no-ops.
4. **CSE + admin requirement.** Construction Set Extender requires admin
   privileges to initialize (CrashRpt). Without it our plugin still loads,
   but only the no-CSE fallback compile path is available — sufficient
   only for simple scripts. For the full CSE preprocessor pipeline, run
   MO2 with "Run as administrator" enabled.
5. **UAC/launching.** When MO2 is set to run as admin, you can't launch
   it via `moshortcut://` from a non-admin shell (Windows refuses to
   bridge the IL boundary). Either run from a privileged shell, or use a
   Task Scheduler task with saved credentials to invoke MO2.

## Usage

```bash
export CSE_BATCH_ONE="Reborn Conflicts.esp"     # required: target .esp
export CSE_BATCH_FORMID="01012EAF"               # optional: only this SCPT
export CSE_BATCH_RESULT="D:/tmp/cse-result.json" # optional: status JSON path
export CSE_BATCH_HIDDEN=1                        # optional: hide CS window

"<MO2 instance>/ModOrganizer.exe" "moshortcut://:CSE Batch Compile"
```

Or use the bundled wrapper that polls for the result:

```bash
./examples/run-batch-compile.sh --plugin "Reborn Conflicts.esp" --formid 01012EAF
```

The leading-`:` form uses the current MO2 instance; the field before `:`
is an MO2 *instance* name, not a profile.

### Result JSON

```json
{
  "status": "ok",
  "plugin": "Reborn Conflicts.esp",
  "saved": true,
  "scripts": [
    {
      "editor_id": "PSCharactergenStartingEquipmentScript",
      "form_id": "01012EAF",
      "compile_ok": true,
      "scda_length_before": 19470,
      "scda_length_after": 19507,
      "scda_sha256_before": "dcaa12744b1aed95...",
      "scda_sha256_after":  "5d1b08488a91804f..."
    }
  ]
}
```

If `compile_ok` is true and the sha256 hashes differ, the override
is runtime-effective: the engine will execute the new bytecode.

### Authoring helper

`scripts/author_test_override.pas` is a companion xEdit Pascal script
that copies a master SCPT as override into a target plugin and applies
a `StringReplace` to its SCTX. Default behavior is the PSMQD DLC-guard
fix; pass `--sctx-find` and `--sctx-replace` for arbitrary edits:

```
TES4Edit_patched -script:scripts/author_test_override.pas \
  --target-plugin="Reborn Conflicts.esp" \
  --master-formid=1A012EAF \
  --sctx-find='IsModLoaded "Foo.esp"' \
  --sctx-replace='IsModLoaded "Bar.esp"'
```

## Caveats

- **TESConstructionSet v1.2 only.** Hardcoded offsets are exe-version
  specific.
- **Active-file records only.** The plugin recompiles records flagged
  `kFormFlags_FromActiveFile` — i.e., authored in the target plugin
  (not master records). The xEdit-side authoring step is mandatory.
- **First-run popups.** The CS shows reference-resolution warnings on
  first plugin load. Click "Yes to all" once; it's sticky for the
  session. (Future: add a watcher thread that auto-dismisses.)
- **CSE conflicts.** Don't apply our compiler-error patches when CSE
  is loaded — CSE's own patches are at the same byte addresses, and
  double-patching crashed CSE. The plugin auto-detects CSE and skips
  its own patches accordingly.

## Attribution

CS-internal address table, struct layouts, the compiler-error-detour
port, and the recompile pipeline pattern are derived from
[shadeMe/Construction-Set-Extender][cse] (GPL-3.0). This project is
GPL-3.0 accordingly.

## License

GPL-3.0. See `LICENSE`.

[cse]: https://github.com/shadeMe/Construction-Set-Extender
[sme]: https://github.com/shadeMe/SME-Sundries
