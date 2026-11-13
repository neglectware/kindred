#!/bin/bash
# Copies this folder into the neglectware/kindred git repository
# (default ~/Developer/neglectware/kindred), leaving out what .gitignore
# names, then refuses to finish if a private detail slipped in. The deny
# patterns live outside this folder so the list itself is never exported.
#
#   tools/export-public.sh [repo-dir]      then review, commit, push in repo-dir
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEST="${1:-$HOME/Developer/neglectware/kindred}"
DENY="$HERE/../private/export-deny-patterns.txt"
[ -f "$DENY" ] || { echo "export-public.sh: $DENY missing" >&2; exit 1; }
mkdir -p "$DEST"
rsync -a --delete --exclude '.git/' --filter=':- .gitignore' "$HERE/" "$DEST/"
if HITS=$(grep -rIl -i -E -f "$DENY" "$DEST" --exclude-dir=.git); then
  echo "export-public.sh: PRIVATE DETAILS FOUND -- do not commit:" >&2
  echo "$HITS" >&2
  exit 1
fi
echo "exported to $DEST"
