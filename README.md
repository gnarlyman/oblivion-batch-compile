# oblivion-batch-compile

A headless OBSE editor plugin that recompiles Oblivion SCPT bytecode
(`SCDA` subrecords) from script source (`SCTX`) without UI interaction.

## Why

Authoring script overrides for Oblivion mods normally requires opening
TESConstructionSet (or CSE) in its GUI, double-clicking each script in
the editor pane, and saving. xEdit cannot recompile SCDA — it only edits
the source text.

This plugin loads inside the CS process (via OBSE editor mode), reads
target plugin and optional FormID filter from environment variables,
calls the CS-internal script compiler at offset `0x00503450`, saves the
plugin, and exits the CS — with zero clicks.

The intended workflow is:

1. **xEdit (or any tool) authors an SCTX-only override** in the target
   `.esp` — copying the master SCPT as override into the active file
   and editing the `SCTX` text. The resulting record has fresh source
   and stale (master-inherited) bytecode.
2. **This plugin runs**, finds the override, recompiles `SCDA` from
   `SCTX`, saves the plugin.

## Build

Requires Visual Studio 2022 (v143 toolset, Win32). No DirectX SDK,
no CrashRpt, no managed/CLI components. Header-only deps:

- [shadeMe/OBSE-Plugin-Build-Dependencies](https://github.com/shadeMe/OBSE-Plugin-Build-Dependencies)
- [shadeMe/SME-Sundries](https://github.com/shadeMe/SME-Sundries)

Setup:

```bash
cp EnvVars.props.template EnvVars.props
# Edit EnvVars.props to point at your local clones and Oblivion install.

"C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe" \
  oblivion-batch-compile.sln /p:Configuration=Release /p:Platform=Win32
```

The post-build step copies `oblivion-batch-compile.dll` to
`$(OblivionPath)\Data\obse\plugins\` if `$(OblivionPath)` is set.

## Install (MO2)

1. Place the built DLL at `<MO2 instance>/mods/CSE Batch Compile/obse/plugins/oblivion-batch-compile.dll`.
2. Activate the "CSE Batch Compile" mod in your profile.
3. Add an MO2 custom executable (Tools → Modify Executables → Add):
   - Title: `CSE Batch Compile`
   - Binary: `<oblivion>/obse_loader.exe`
   - Arguments: `-editor`
   - Working directory: `<oblivion>` (the Oblivion install root)

## Usage

Set environment variables, then launch via MO2:

```bash
export CSE_BATCH_ONE="Reborn Conflicts.esp"        # required: target plugin filename
export CSE_BATCH_FORMID="01012EAF"                  # optional: only recompile this SCPT (active-file FormID, hex)
export CSE_BATCH_RESULT="D:/tmp/cse-result.json"   # optional: write status JSON here

"<MO2 instance>/ModOrganizer.exe" "moshortcut://<profile>:CSE Batch Compile"
```

Result JSON shape:

```json
{
  "status": "ok",
  "plugin": "Reborn Conflicts.esp",
  "scripts": [
    {
      "editor_id": "PSCharactergenStartingEquipmentScript",
      "form_id": "01012EAF",
      "scda_sha256_before": "...",
      "scda_sha256_after": "...",
      "result": "ok"
    }
  ]
}
```

### Multi-plugin batch (job mode)

```bash
export CSE_BATCH_JOB="D:/tmp/jobs.json"
"<MO2 instance>/ModOrganizer.exe" "moshortcut://<profile>:CSE Batch Compile"
```

Where `jobs.json` is:

```json
{
  "jobs": [
    { "plugin": "Reborn Conflicts.esp", "form_id": "01012EAF" },
    { "plugin": "Other Patch.esp" }
  ]
}
```

Each job runs sequentially; the CS launches once and processes all jobs
before exiting.

## Caveats

- The CS main window appears briefly. The plugin issues `WM_CLOSE` when
  done; total runtime is typically 5–15 seconds.
- Only records marked `kFormFlags_FromActiveFile` (i.e., overrides
  authored in the target plugin, not its masters) are eligible for
  recompile. The xEdit-side authoring step is mandatory.
- TESConstructionSet **v1.2** only. Hardcoded offsets are exe-version
  specific.

## Attribution

The CS-internal address table and Script/TESDataHandler struct layouts
are derived from [shadeMe/Construction-Set-Extender](https://github.com/shadeMe/Construction-Set-Extender)
(GPL-3.0). This project is licensed GPL-3.0 accordingly.

## License

GPL-3.0. See `LICENSE`.
