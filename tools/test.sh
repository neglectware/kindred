#!/usr/bin/env bash
# Engine tests (no JUCE, no plug-in): golden equivalence with the JavaScript prototype, the ported
# rule checks, and the MIDI scheduler stress test. Needs node for the fixtures.
#   tools/test.sh [projects=3000]
# On a Mac that can run Intel programs, the golden test also runs as x86_64.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"; cd "$ROOT"
if [[ -x tools/local-prebuild.sh ]]; then pre=$(tools/local-prebuild.sh) || { echo "$pre"; exit 1; }; fi
B=build-engine
cmake -S . -B $B -G Ninja -DKINDRED_ENGINE_ONLY=ON -DCMAKE_BUILD_TYPE=Release > /dev/null
cmake --build $B | tail -1
node tests/golden/gen-fixtures.js $B/fixtures.jsonl "${1:-3000}" 20261112
$B/KindredGolden styles/warm-ambient.json $B/fixtures.jsonl | tail -3
$B/KindredGolden styles/warm-ambient.json $B/fixtures.jsonl --shipped | tail -1
if [[ "$(uname)" == Darwin ]] && arch -x86_64 /usr/bin/true 2>/dev/null && lipo -archs $B/KindredGolden | grep -q x86_64; then
  echo "x86_64: $(arch -x86_64 $B/KindredGolden styles/warm-ambient.json $B/fixtures.jsonl | tail -1)"
fi
for style in styles/*.json; do $B/KindredRules "$style" 1800 20261111 | grep -v "^note:" ; done
$B/KindredPlayerTest styles/warm-ambient.json 2000 7
