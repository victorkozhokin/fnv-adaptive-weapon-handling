#!/usr/bin/env bash
# Cross-compile AdaptiveWeaponHandling.dll (32-bit Windows) from macOS or Linux.
#
# Requirements: clang (Apple clang is fine) and lld-link from LLVM's lld.
#   brew install lld
#
# The plugin links without a C runtime and without the Windows SDK, so no
# Visual Studio or xwin setup is needed -- the only import library is generated
# from scripts/kernel32.def on the fly.

set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
out="$root/build"
target=i686-pc-windows-msvc

CLANG="${CLANG:-clang}"
LLD_LINK="${LLD_LINK:-lld-link}"

# llvm-dlltool lives in the (keg-only) llvm prefix that lld depends on, so it
# is usually not on PATH.
if [[ -z "${DLLTOOL:-}" ]]; then
    for candidate in llvm-dlltool \
        /opt/homebrew/opt/llvm/bin/llvm-dlltool \
        /usr/local/opt/llvm/bin/llvm-dlltool
    do
        if command -v "$candidate" >/dev/null 2>&1; then
            DLLTOOL="$candidate"
            break
        fi
    done
fi

for tool in "$CLANG" "$LLD_LINK" "${DLLTOOL:-llvm-dlltool}"; do
    command -v "$tool" >/dev/null 2>&1 || {
        echo "error: $tool not found. Try: brew install lld" >&2
        exit 1
    }
done

mkdir -p "$out"

cflags=(
    --target=$target
    -std=c++20
    -O2
    -flto=thin
    -fno-exceptions
    -fno-rtti
    -fno-stack-protector
    -mno-stack-arg-probe
    -ffreestanding
    -fms-extensions
    -fms-compatibility
    -Wall
    -Wno-unused-parameter
    -I"$root/src"
)

sources=(
    src/main.cpp
    src/config.cpp
    src/curve.cpp
    src/util/crt.cpp
    src/util/log.cpp
    src/util/paths.cpp
    src/util/patch.cpp
)

objects=()
for src in "${sources[@]}"; do
    obj="$out/$(basename "${src%.cpp}").obj"
    echo "  CXX $src"
    "$CLANG" "${cflags[@]}" -c "$root/$src" -o "$obj"
    objects+=("$obj")
done

# Import library for the six kernel32 functions the plugin calls.
#
# --kill-at keeps the stdcall decoration on the object-side symbols (which is
# what clang emits) while exporting the plain names kernel32.dll actually has.
echo "  LIB kernel32"
"$DLLTOOL" -m i386 --kill-at \
    -d "$root/scripts/kernel32.def" -l "$out/kernel32.lib"

echo "  LNK AdaptiveWeaponHandling.dll"
"$LLD_LINK" /machine:x86 /dll /nodefaultlib /entry:DllMain \
    /out:"$out/AdaptiveWeaponHandling.dll" \
    "${objects[@]}" "$out/kernel32.lib"

# Stage the mod exactly as it sits under Data/, then zip it.
#
# The staging directories are wiped first: they are build output, and leftovers
# from an earlier layout would otherwise be picked up and shipped.
stage_dll="$root/nvse/Plugins"
stage_ini="$root/config/AWHConfig"
archive="$root/AdaptiveWeaponHandling.zip"
stage_inputs=("$root/scripts-game" "$root/AWHConfig.ini" "$root/MCM" "$root/MCM-RU" "$root/README.md")

# Refuse to touch the staging tree unless every input is present: the wipe
# below is destructive, and a missing source would otherwise leave nothing.
for required in "${stage_inputs[@]}"; do
    [[ -e "$required" ]] || { echo "error: missing $required" >&2; exit 1; }
done

rm -rf "$root/nvse" "$root/config" "$archive"
mkdir -p "$stage_dll" "$stage_ini"

cp "$out/AdaptiveWeaponHandling.dll" "$stage_dll/"
mkdir -p "$root/nvse/Plugins/Scripts"
cp "$root"/scripts-game/*.txt "$root/nvse/Plugins/Scripts/"
cp "$root/AWHConfig.ini" "$stage_ini/"

(cd "$root" && zip -rq "$archive" nvse config MCM MCM-RU README.md -x '*.DS_Store')

echo
echo "built   $out/AdaptiveWeaponHandling.dll"
echo "staged  $stage_dll/AdaptiveWeaponHandling.dll"
echo "        $stage_ini/AWHConfig.ini"
echo "        $root/nvse/Plugins/Scripts/ (script layer)"
echo "        $root/MCM/ (menu)"
echo "        $root/MCM-RU/ (optional Russian menu text)"
echo "package $archive"
