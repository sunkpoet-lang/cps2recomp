#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# (this fork, ForkAde's discord-community-lobby) Keeps online/site/, the registration and admin pages the Worker
# serves as static assets (/join, /admin), a copy of ForkAde's site/, and checks nobody changed it here.
#
#   tools/sync-site.sh           copy ForkAde's site/ into online/site/
#   tools/sync-site.sh --check   fail if online/site/ is not ForkAde's site/
#
# ForkAde is looked for beside this repository (../ForkAde), or where FORKADE says. Change the pages in ForkAde,
# commit there, and run this.
set -eu

here=$(cd "$(dirname "$0")" && pwd)
root=$(git -C "$here" rev-parse --show-toplevel)
src=${FORKADE:-$root/../ForkAde}/site
dest="$root/online/site"

[ -d "$src" ] || { echo "sync-site: no $src (set FORKADE to ForkAde's folder)" >&2; exit 1; }
if [ "${1:-}" = "--check" ]; then
    if ! diff -r -q "$src" "$dest" > /dev/null 2>&1; then
        echo "sync-site: online/site/ is not ForkAde's site/; run tools/sync-site.sh" >&2
        diff -r -q "$src" "$dest" >&2 || true
        exit 1
    fi
    echo "sync-site: online/site/ is ForkAde's site/"
    exit 0
fi
rm -rf "$dest"
mkdir -p "$dest"
cp -R "$src"/. "$dest"/
echo "sync-site: online/site/ copied from $src"
