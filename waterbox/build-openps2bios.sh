#!/bin/sh
# Builds OpenPS2BIOS (extern/openps2bios, a submodule pinned at its v0.2
# release) into build/openps2bios/OpenPS2BIOS.bin: an open, MIT-licensed
# replacement PlayStation 2 BIOS that contains no Sony code. It is for the
# test script only, so that the tests that need a BIOS can run where nobody
# has a dump of their own (CI); it is not in the package.
#
#   waterbox/build-openps2bios.sh
#
# Needs LLVM 23 (clang with the MIPS backend, ld.lld, llvm-objcopy), which is
# what upstream builds its releases with: LLVM_BIN names its bin folder
# (default /usr/lib/llvm-23/bin, where apt.llvm.org's llvm-23 puts it). The
# result is checked against the SHA-256 upstream published for the release
# (tests/openps2bios/OpenPS2BIOS-0.2.sha256): another LLVM compiles it into
# other bytes (LLVM 20 differs in 170,054 of them), and the tests would then
# run on a BIOS nobody released.
set -eu
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/.." && pwd)"
llvm="${LLVM_BIN:-/usr/lib/llvm-23/bin}"
src="$root/extern/openps2bios"
out="$root/build/openps2bios-src"
[ -f "$src/Makefile" ] || { echo "extern/openps2bios is not checked out: git submodule update --init extern/openps2bios" >&2; exit 1; }
[ -x "$llvm/clang" ] || { echo "no LLVM 23 at $llvm (set LLVM_BIN)" >&2; exit 1; }
mkdir -p "$out" "$root/build/openps2bios"
make -s -C "$src" BUILD="$out" CC="$llvm/clang" LD="$llvm/ld.lld" OBJCOPY="$llvm/llvm-objcopy" 2>&1 \
	| grep -v "MIPS-I support is experimental" || true
[ -f "$out/OpenPS2BIOS.bin" ] || { echo "OpenPS2BIOS did not build" >&2; exit 1; }
cp "$out/OpenPS2BIOS.bin" "$root/build/openps2bios/OpenPS2BIOS.bin"
(cd "$root/build/openps2bios" && sha256sum -c --quiet "$root/tests/openps2bios/OpenPS2BIOS-0.2.sha256") || {
	echo "the BIOS built is not the one upstream released (another LLVM than 23?)" >&2
	exit 1
}
echo "built $root/build/openps2bios/OpenPS2BIOS.bin (OpenPS2BIOS 0.2, the released bytes)"
