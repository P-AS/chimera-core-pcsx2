#!/bin/sh
# Builds chimera-padtest.elf and chimera-padtest.iso (a disc whose SYSTEM.CNF
# boots it) with the ps2dev toolchain: ps2dev/ps2dev v2.0.0, image digest
# sha256:8e6dc4564598ecce94d078d3111b8c4803f88faa94a6f183ac60240a0ae010a1
# (GCC 15.2, ps2sdk, gsKit). Both results are committed beside this script, so
# nothing else needs the toolchain.
#
#   build.sh <ps2dev root>     a root holding /usr/local/ps2dev (the image
#                              unpacked); the compiler runs chrooted into it,
#                              in a user namespace, so no root is needed
# or, with Docker:
#   docker run --rm -v "$PWD:/src" -w /src ps2dev/ps2dev@sha256:8e6dc456... sh build.sh --inside
# Needs xorriso (outside the toolchain) for the disc.
# SPDX-License-Identifier: MIT
set -eu
here="$(cd "$(dirname "$0")" && pwd)"
if [ "${1:-}" = "--inside" ]; then
	export PS2DEV=/usr/local/ps2dev PS2SDK=/usr/local/ps2dev/ps2sdk GSKIT=/usr/local/ps2dev/gsKit
	PATH="$PATH:$PS2DEV/bin:$PS2DEV/ee/bin:$PS2SDK/bin"
	cd "${2:-/src}"
	mkdir -p build
	bin2c "$PS2SDK/iop/irx/freesio2.irx" build/freesio2_irx.c freesio2_irx
	bin2c "$PS2SDK/iop/irx/freepad.irx" build/freepad_irx.c freepad_irx
	cat > build/irx.h <<'H'
extern unsigned char freesio2_irx[]; extern unsigned int size_freesio2_irx;
extern unsigned char freepad_irx[]; extern unsigned int size_freepad_irx;
H
	cflags="-D_EE -G0 -O2 -Wall -I$PS2SDK/ee/include -I$PS2SDK/common/include -I$GSKIT/include -Ibuild"
	for c in main.c build/freesio2_irx.c build/freepad_irx.c; do
		mips64r5900el-ps2-elf-gcc $cflags -c "$c" -o "build/$(basename "$c" .c).o"
	done
	mips64r5900el-ps2-elf-gcc -T"$PS2SDK/ee/startup/linkfile" -O2 -o build/chimera-padtest.elf \
		build/main.o build/freesio2_irx.o build/freepad_irx.o \
		-L"$PS2SDK/ee/lib" -L"$GSKIT/lib" -Wl,-zmax-page-size=128 \
		-lgskit -ldmakit -lpad -lpatches -ldebug
	mips64r5900el-ps2-elf-strip -s build/chimera-padtest.elf
	exit 0
fi
root="$(cd "$1" && pwd)"
# the sources go into the root for the build, and the result comes back
work="$root/work/chimera-padtest"
rm -rf "${work:?}"
mkdir -p "$work"
cp "$here/main.c" "$here/build.sh" "$work/"
unshare -r chroot "$root" /bin/sh /work/chimera-padtest/build.sh --inside /work/chimera-padtest
cp "$work/build/chimera-padtest.elf" "$here/../chimera-padtest.elf"
disc="$(mktemp -d)"
cp "$here/../chimera-padtest.elf" "$disc/CHPAD.ELF"
printf 'BOOT2 = cdrom0:\\CHPAD.ELF;1\r\nVER = 1.00\r\nVMODE = NTSC\r\n' > "$disc/SYSTEM.CNF"
# fixed dates (the files' and the volume's), so that the same program makes
# the same disc
touch -d @946684800 "$disc/CHPAD.ELF" "$disc/SYSTEM.CNF" "$disc"
SOURCE_DATE_EPOCH=946684800 xorriso -as mkisofs -quiet -iso-level 1 -V CHIMERA_PADTEST \
	--modification-date=2000010100000000 -o "$here/../chimera-padtest.iso" "$disc"
rm -rf "${disc:?}"
echo "built tests/own/chimera-padtest.elf and chimera-padtest.iso"
