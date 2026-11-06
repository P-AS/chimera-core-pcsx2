#!/bin/sh
# Builds the two discs of chimera-fputest with the ps2dev toolchain:
# ps2dev/ps2dev v2.0.0, image digest
# sha256:8e6dc4564598ecce94d078d3111b8c4803f88faa94a6f183ac60240a0ae010a1
# (GCC 15.2, ps2sdk), the same as chimera-padtest's. Both discs are committed
# beside this folder, so nothing else needs the toolchain.
#
# The discs hold the same program under two names. PCSX2 takes the name of
# the program a disc starts (its SYSTEM.CNF) as the game's serial and applies
# that game's entry in its database, and the Full clamp mode of the EE's FPU
# is only set that way. So:
# - chimera-fputest-full.iso starts SLUS_201.51 (Klonoa 2, Full mode);
# - chimera-fputest-full-round-up.iso starts SLUS_203.12 (Final Fantasy X,
#   Full mode with the FPU rounding up rather than towards zero).
# Nothing of either game is on them.
#
#   build.sh <ps2dev root>     a root holding /usr/local/ps2dev (the image
#                              unpacked); the compiler runs chrooted into it,
#                              in a user namespace, so no root is needed
# or, with Docker:
#   docker run --rm -v "$PWD:/src" -w /src ps2dev/ps2dev@sha256:8e6dc456... sh build.sh --inside
# Needs xorriso (outside the toolchain) for the discs.
# SPDX-License-Identifier: MIT
set -eu
here="$(cd "$(dirname "$0")" && pwd)"
if [ "${1:-}" = "--inside" ]; then
	export PS2DEV=/usr/local/ps2dev PS2SDK=/usr/local/ps2dev/ps2sdk
	PATH="$PATH:$PS2DEV/bin:$PS2DEV/ee/bin:$PS2SDK/bin"
	cd "${2:-/src}"
	mkdir -p build
	mips64r5900el-ps2-elf-gcc -D_EE -G0 -O2 -Wall -I"$PS2SDK/ee/include" -I"$PS2SDK/common/include" \
		-c main.c -o build/main.o
	mips64r5900el-ps2-elf-gcc -T"$PS2SDK/ee/startup/linkfile" -O2 -o build/chimera-fputest.elf build/main.o \
		-L"$PS2SDK/ee/lib" -Wl,-zmax-page-size=128
	mips64r5900el-ps2-elf-strip -s build/chimera-fputest.elf
	exit 0
fi
root="$(cd "$1" && pwd)"
# the sources go into the root for the build, and the result comes back
work="$root/work/chimera-fputest"
rm -rf "${work:?}"
mkdir -p "$work"
cp "$here/main.c" "$here/build.sh" "$work/"
unshare -r chroot "$root" /bin/sh /work/chimera-fputest/build.sh --inside /work/chimera-fputest
for disc in full:SLUS_201.51 full-round-up:SLUS_203.12; do
	name="${disc%%:*}" boot="${disc#*:}"
	dir="$(mktemp -d)"
	cp "$work/build/chimera-fputest.elf" "$dir/$boot"
	printf 'BOOT2 = cdrom0:\\%s;1\r\nVER = 1.00\r\nVMODE = NTSC\r\n' "$boot" > "$dir/SYSTEM.CNF"
	# fixed dates (the files' and the volume's), so that the same program
	# makes the same disc
	touch -d @946684800 "$dir/$boot" "$dir/SYSTEM.CNF" "$dir"
	SOURCE_DATE_EPOCH=946684800 xorriso -as mkisofs -quiet -iso-level 1 -V CHIMERA_FPUTEST \
		--modification-date=2000010100000000 -o "$here/../chimera-fputest-$name.iso" "$dir"
	rm -rf "${dir:?}"
done
echo "built tests/own/chimera-fputest-full.iso and chimera-fputest-full-round-up.iso"
