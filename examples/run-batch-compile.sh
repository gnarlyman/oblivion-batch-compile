#!/usr/bin/env bash
# Wrapper that launches obse_loader.exe -editor under MO2's USVFS, with
# the CSE_BATCH_* env vars exported for our OBSE plugin to pick up.
#
# Usage:
#   run-batch-compile.sh --plugin "Reborn Conflicts.esp"
#   run-batch-compile.sh --plugin "Reborn Conflicts.esp" --formid 01012EAF
#   run-batch-compile.sh --plugin "Reborn Conflicts.esp" --result "/d/tmp/out.json"
#   run-batch-compile.sh --plugin "..." --hidden     # opt in to invisible CS
#
# MO2 dispatches the moshortcut and then exits — the CS runs detached.
# So we don't wait for ModOrganizer.exe; we poll for the result JSON and
# treat its appearance as completion. The plugin writes the JSON just
# before posting WM_CLOSE, so it's a tight signal.
#
# Optional env (or override via CLI):
#   MO2_PATH       — path to ModOrganizer.exe (default: Reborn instance)
#   MO2_INSTANCE   — MO2 instance name (default: empty = current)
#   MO2_EXEC_NAME  — moshortcut target (default: "CSE Batch Compile")
#   OBC_TIMEOUT    — max seconds to wait for the result file (default 120)

set -euo pipefail

MO2_PATH="${MO2_PATH:-D:/Modlists/Reborn/ModOrganizer.exe}"
MO2_INSTANCE="${MO2_INSTANCE:-}"
MO2_EXEC_NAME="${MO2_EXEC_NAME:-CSE Batch Compile}"
TIMEOUT="${OBC_TIMEOUT:-120}"

PLUGIN=""
FORMID=""
RESULT=""
HIDDEN=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --plugin)  PLUGIN="$2"; shift 2;;
        --formid)  FORMID="$2"; shift 2;;
        --result)  RESULT="$2"; shift 2;;
        --hidden)  HIDDEN="1"; shift;;
        -h|--help) sed -n '2,/^$/p' "$0" | sed 's/^# //;s/^#//'; exit 0;;
        *) echo "unknown arg: $1" >&2; exit 2;;
    esac
done

if [[ -z "$PLUGIN" ]]; then
    echo "error: --plugin is required" >&2
    exit 2
fi

# Default result path lives outside the MO2 USVFS overlay so a wrapper
# script in any cwd can read it.
if [[ -z "$RESULT" ]]; then
    RESULT="/d/tmp/cse-result.json"
    mkdir -p /d/tmp 2>/dev/null || true
fi

# Clean stale result so freshness is unambiguous.
rm -f "$RESULT" 2>/dev/null || true

export CSE_BATCH_ONE="$PLUGIN"
[[ -n "$FORMID" ]] && export CSE_BATCH_FORMID="$FORMID"
export CSE_BATCH_RESULT="$RESULT"
[[ -n "$HIDDEN" ]] && export CSE_BATCH_HIDDEN=1

echo ">> CSE_BATCH_ONE     = $CSE_BATCH_ONE"
[[ -n "${CSE_BATCH_FORMID:-}" ]] && echo ">> CSE_BATCH_FORMID  = $CSE_BATCH_FORMID"
echo ">> CSE_BATCH_RESULT  = $CSE_BATCH_RESULT"
[[ -n "$HIDDEN" ]] && echo ">> CSE_BATCH_HIDDEN  = 1 (CS will run invisibly)"
echo ">> launching: moshortcut://${MO2_INSTANCE}:${MO2_EXEC_NAME}"

# Fire and forget — MO2 exits immediately after dispatch.
"$MO2_PATH" "moshortcut://${MO2_INSTANCE}:${MO2_EXEC_NAME}" &
MO2_PID=$!

# Poll for the result file. Plugin writes it just before WM_CLOSE.
echo ">> waiting up to ${TIMEOUT}s for $RESULT"
ELAPSED=0
while [[ ! -f "$RESULT" ]] && [[ $ELAPSED -lt $TIMEOUT ]]; do
    sleep 1
    ELAPSED=$((ELAPSED + 1))
done

# Reap the MO2 dispatcher process (it should already be done).
wait "$MO2_PID" 2>/dev/null || true

if [[ -f "$RESULT" ]]; then
    echo ">> done in ${ELAPSED}s; result:"
    cat "$RESULT"
    # Exit code reflects whether *all* scripts compiled — plain grep keeps
    # the wrapper free of jq dependencies.
    if grep -q '"compile_ok":false' "$RESULT"; then
        exit 3
    fi
    exit 0
else
    echo ">> ERROR: no result file written within ${TIMEOUT}s" >&2
    exit 1
fi
