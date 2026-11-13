#!/bin/bash
# auval only sees third-party Audio Units from a login (Aqua) session, so this file is opened
# with `open`, which runs it in Terminal.app. It writes tests/out/auval.log and a done marker.
# On a Mac that can run Intel programs it then validates the Intel half too (auval-x86_64.log).
cd "$(dirname "$0")/.." || exit 1
mkdir -p tests/out
rm -f tests/out/auval.done tests/out/auval-x86_64.log
{ date; auval -v aumi Kndr Nglw; echo "exit code: $?"; } > tests/out/auval.log 2>&1
tail -3 tests/out/auval.log
if arch -x86_64 /usr/bin/true 2>/dev/null; then
  { date; arch -x86_64 auval -v aumi Kndr Nglw; echo "exit code: $?"; } > tests/out/auval-x86_64.log 2>&1
  tail -3 tests/out/auval-x86_64.log
fi
touch tests/out/auval.done
exit 0
