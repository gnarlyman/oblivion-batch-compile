#!/usr/bin/env bash
# Wrapper that launches obse_loader.exe -editor under MO2's USVFS, with the
# CSE_BATCH_* env vars exported for our OBSE plugin to pick up.
#
# Usage:
#   run-batch-compile.sh --plugin "Reborn Conflicts.esp"
#   run-batch-compile.sh --plugin "Reborn Conflicts.esp" --formid 01012EAF
#   run-batch-compile.sh --plugin "Reborn Conflicts.esp" --result "/d/tmp/out.json"
#
# Required env (or override via CLI):
#   MO2_PATH       — path to ModOrganizer.exe
#   MO2_PROFILE    — e.g. "Reborn-Base"
#   MO2_EXEC_NAME  — e.g. "CSE Batch Compile"

set -euo pipefail

MO2_PATH="${MO2_PATH:-D:/Modlists/Reborn/ModOrganizer.exe}"
MO2_PROFILE="${MO2_PROFILE:-Reborn-Base}"
MO2_EXEC_NAME="${MO2_EXEC_NAME:-CSE Batch Compile}"

PLUGIN=""
FORMID=""
RESULT=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --plugin)  PLUGIN="$2"; shift 2;;
        --formid)  FORMID="$2"; shift 2;;
        --result)  RESULT="$2"; shift 2;;
        -h|--help) sed -n '2,/^$/p' "$0" | sed 's/^# //;s/^#//'; exit 0;;
        *) echo "unknown arg: $1" >&2; exit 2;;
    esac
done

if [[ -z "$PLUGIN" ]]; then
    echo "error: --plugin is required" >&2
    exit 2
fi

# Default result path so a failed/successful run still leaves a status file.
if [[ -z "$RESULT" ]]; then
    RESULT="$(pwd)/.batch-compile-result.json"
fi

# Clean any stale result so we can detect run completion by mtime.
rm -f "$RESULT" 2>/dev/null || true

export CSE_BATCH_ONE="$PLUGIN"
[[ -n "$FORMID" ]] && export CSE_BATCH_FORMID="$FORMID"
export CSE_BATCH_RESULT="$RESULT"

echo ">> CSE_BATCH_ONE     = $CSE_BATCH_ONE"
[[ -n "${CSE_BATCH_FORMID:-}" ]] && echo ">> CSE_BATCH_FORMID  = $CSE_BATCH_FORMID"
echo ">> CSE_BATCH_RESULT  = $CSE_BATCH_RESULT"
echo ">> launching MO2: $MO2_PROFILE / $MO2_EXEC_NAME"

# Pattern A — moshortcut:// (args from ModOrganizer.ini, env-vars inherited).
"$MO2_PATH" "moshortcut://${MO2_PROFILE}:${MO2_EXEC_NAME}"

if [[ -f "$RESULT" ]]; then
    echo ">> result:"
    cat "$RESULT"
else
    echo ">> warning: no result file written at $RESULT" >&2
    exit 1
fi
