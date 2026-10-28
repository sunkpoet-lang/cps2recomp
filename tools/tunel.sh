#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Part of cps2recomp (the user's fork).
#
# The online tunnel (tunel/, Rust with Iroh), built into build/tunel/ and copied beside the launcher:
#   tools/tunel.sh [FOLDER]      FOLDER: where cps-tunel goes (build/collection)
# Needs Rust's cargo (rustup; on Windows its MSVC toolchain, with Visual Studio's Build Tools). Without cargo it says so
# and stops with code 3: the collection is then built without the tunnel, and the launcher shows no ONLINE card.
set -eu
OUT=${1:-build/collection}
if ! command -v cargo > /dev/null 2>&1; then
    # (rustup's folder, also when run from MSYS2, whose HOME is not the Windows one and which may not pass USERPROFILE)
    for dir in "$HOME/.cargo/bin" "${USERPROFILE:-/nowhere}/.cargo/bin" "/c/Users/${USERNAME:-${USER:-nobody}}/.cargo/bin"; do
        if [ -x "$dir/cargo" ] || [ -x "$dir/cargo.exe" ]; then
            PATH="$dir:$PATH"
            break
        fi
    done
fi
if ! command -v cargo > /dev/null 2>&1; then
    echo "tunel: no cargo (install Rust with rustup); built without the online tunnel" >&2
    exit 3
fi
CARGO_PATH=$PATH
set --
case "$(uname -s)" in
MINGW* | MSYS*)
    # (MSYS2's own link, a coreutils program, would come before the Visual Studio linker Rust looks for)
    CARGO_PATH=$(printf %s "$PATH" | tr ':' '\n' | grep -v -x -e /usr/bin -e /bin -e /usr/local/bin | paste -s -d: -)
    # (and a shell started without Windows' environment lacks the variables Rust finds Visual Studio with; sh can't
    # name ProgramFiles(x86), env can)
    if ! env | grep -q '^ProgramFiles(x86)='; then
        set -- "ProgramFiles(x86)=C:\\Program Files (x86)" "ProgramFiles=C:\\Program Files" "ProgramData=C:\\ProgramData"
    fi
    ;;
esac
# The program keeps its sources' paths (for its error messages): this PC's (the user's folder, the repository's) are
# replaced, so the program shared with others names neither
native() { cygpath -w "$1" 2> /dev/null || printf %s "$1"; }
cargo_home=$(cd "$(dirname "$(command -v cargo)")/.." && pwd)
remap="--remap-path-prefix=$(native "$cargo_home")=cargo$(printf '\037')--remap-path-prefix=$(native "$PWD")=cps2recomp"
env "$@" PATH="$CARGO_PATH" CARGO_TARGET_DIR="$PWD/build/tunel" CARGO_ENCODED_RUSTFLAGS="$remap" \
    cargo build --release --quiet --manifest-path tunel/Cargo.toml
mkdir -p "$OUT"
exe=build/tunel/release/cps-tunel
# (Windows: cps-tunel.exe; MSYS2 also takes the name without .exe for it, so .exe is looked for first)
if [ -f "$exe.exe" ]; then
    exe="$exe.exe"
fi
cp -p "$exe" "$OUT/"
echo "built $OUT/$(basename "$exe")"
