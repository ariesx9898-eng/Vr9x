#!/usr/bin/env bash
# Runs the project's automation tests (Source/MushokuRPG/Private/Tests) headless and summarises them.
#
#   Tools/mac/run_automation_tests.sh                # every MushokuRPG.* test
#   Tools/mac/run_automation_tests.sh MushokuRPG.Orsted
#
# Needs the built editor (Tools/mac/build_and_setup.sh --build-only). The asset checks (meshes, clips) need the
# imported characters too (Tools/mac/build_and_setup.sh). Log: Saved/Logs/mt_tests.log; JSON report: Saved/Automation.
# Exit code 0 only when at least one test ran and none failed.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
UE_ROOT="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
UE_EDITOR_CMD="${UE_EDITOR_CMD:-$UE_ROOT/Engine/Binaries/Mac/UnrealEditor-Cmd}"
FILTER="${1:-MushokuRPG}"
LOG="$REPO_ROOT/Saved/Logs/mt_tests.log"
mkdir -p "$(dirname "$LOG")" "$REPO_ROOT/Saved/Automation"

if [[ ! -x "$UE_EDITOR_CMD" ]]; then
  echo "ERROR: $UE_EDITOR_CMD not found (set UE_ROOT or UE_EDITOR_CMD)" >&2
  exit 2
fi

echo "== tests: Automation RunTests $FILTER (log: Saved/Logs/mt_tests.log)"
set +e
"$UE_EDITOR_CMD" "$REPO_ROOT/MushokuRPG.uproject" -ExecCmds="Automation RunTests $FILTER;Quit" \
  -TestExit="Automation Test Queue Empty" -ReportExportPath="$REPO_ROOT/Saved/Automation" \
  -unattended -nosplash -nullrhi -nosound -nocrashreports -stdout -FullStdOutLogOutput >"$LOG" 2>&1
set -e

PASSED="$(grep -c 'Result={Success}' "$LOG" || true)"
FAILED="$(grep -c 'Result={Fail}' "$LOG" || true)"
grep -E 'Test Completed\. Result=' "$LOG" | sed -E 's/.*Result=\{([A-Za-z]+)\}.*Path=\{([^}]*)\}.*/  \1  \2/' || true
if [[ "$FAILED" -gt 0 ]]; then
  echo "-- failed checks:"
  grep -E 'LogAutomationController: Error:' "$LOG" | sed -E 's/.*LogAutomationController: Error: //' | head -60 || true
fi
echo "AUTOMATION: $PASSED passed, $FAILED failed"
[[ "$PASSED" -gt 0 && "$FAILED" -eq 0 ]]
