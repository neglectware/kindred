#!/usr/bin/env bash
# Prints, for each bundle, the minimum macOS version of each architecture in its executable
# (Mach-O load command, read with vtool) and the LSMinimumSystemVersion of its Info.plist.
#   tools/minos.sh <bundle>...
set -euo pipefail
for b in "$@"; do
  exe="$b/Contents/MacOS/$(/usr/libexec/PlistBuddy -c 'Print :CFBundleExecutable' "$b/Contents/Info.plist")"
  ls_min=$(/usr/libexec/PlistBuddy -c 'Print :LSMinimumSystemVersion' "$b/Contents/Info.plist" 2>/dev/null || echo "(not set)")
  for a in $(lipo -archs "$exe"); do
    v=$(xcrun vtool -arch "$a" -show-build "$exe" | awk '/minos|^ *version [0-9]/ {print $2; exit}')
    echo "$(basename "$b")  $a  minos $v  LSMinimumSystemVersion $ls_min"
  done
done
