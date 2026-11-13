#!/usr/bin/env bash
# Builds Kindred for macOS (Release, universal: Apple silicon and Intel, macOS 10.13 and later),
# tests the built plug-in, and only then installs the AU and VST3 into the USER plug-in folders
# (~/Library/Audio/Plug-Ins) and the Standalone into dist/.
# Bump KINDRED_BUILD in src/Product.h before a build you mean to keep.
#   tools/build.sh              build + tests + install + auval
#   tools/build.sh --no-install build + tests only
# JUCE: set JUCE_DIR (environment) or see cmake/Plugin.cmake for where it is looked for.
# If tools/local-prebuild.sh exists (a file of this machine, not in the repository), it runs
# first and can stop the build.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"; cd "$ROOT"
if [[ -x tools/local-prebuild.sh ]]; then pre=$(tools/local-prebuild.sh) || { echo "$pre"; exit 1; }; fi
NAME=$(sed -n 's/#define KINDRED_PRODUCT_NAME "\(.*\)"/\1/p' src/Product.h)
TARGET=$(sed -n 's/#define KINDRED_TARGET_NAME "\(.*\)"/\1/p' src/Product.h)
mkdir -p tests/out
cmake -S . -B build -G Xcode ${JUCE_DIR:+-DJUCE_DIR="$JUCE_DIR"} > tests/out/configure.log 2>&1 || { tail -20 tests/out/configure.log; exit 1; }
cmake --build build --config Release --target ${TARGET}_AU ${TARGET}_VST3 ${TARGET}_Standalone KindredSnapshot KindredHostTest KindredUiTest -- -quiet
A="build/${TARGET}_artefacts/Release"
tools/minos.sh "$A/AU/$NAME.component" "$A/VST3/$NAME.vst3" "$A/Standalone/$NAME.app" | tee tests/out/minos.log
build/KindredHostTest_artefacts/Release/KindredHostTest "$A/VST3/$NAME.vst3" > tests/out/hosttest.log 2>&1 || { tail -30 tests/out/hosttest.log; exit 1; }
tail -2 tests/out/hosttest.log
if arch -x86_64 /usr/bin/true 2>/dev/null; then      # the Intel half, when this Mac can run it
  arch -x86_64 build/KindredHostTest_artefacts/Release/KindredHostTest "$A/VST3/$NAME.vst3" > tests/out/hosttest-x86_64.log 2>&1 || { tail -30 tests/out/hosttest-x86_64.log; exit 1; }
  echo "x86_64: $(tail -1 tests/out/hosttest-x86_64.log)"
fi
build/KindredUiTest_artefacts/Release/KindredUiTest > tests/out/uitest.log 2>&1 || { tail -30 tests/out/uitest.log; exit 1; }
tail -2 tests/out/uitest.log
[[ "${1:-}" == "--no-install" ]] && exit 0
ditto "$A/AU/$NAME.component" "$HOME/Library/Audio/Plug-Ins/Components/$NAME.component"
ditto "$A/VST3/$NAME.vst3" "$HOME/Library/Audio/Plug-Ins/VST3/$NAME.vst3"
rm -rf "dist/$NAME.app"; mkdir -p dist; ditto "$A/Standalone/$NAME.app" "dist/$NAME.app"
echo "installed: ~/Library/Audio/Plug-Ins/Components/$NAME.component, ~/Library/Audio/Plug-Ins/VST3/$NAME.vst3, dist/$NAME.app"
# auval sees third-party Audio Units only from a login session, so it runs in Terminal.app.
rm -f tests/out/auval.done; open tools/auval-in-terminal.command
for _ in $(seq 1 120); do [[ -f tests/out/auval.done ]] && break; sleep 2; done
grep -E "AU VALIDATION|FATAL" tests/out/auval.log || echo "auval did not finish: run 'auval -v aumi Kndr Nglw' in Terminal"
[[ -f tests/out/auval-x86_64.log ]] && echo "x86_64: $(grep -E "AU VALIDATION|FATAL" tests/out/auval-x86_64.log || echo "auval did not finish")"
