# chimera-fputest

The PCSX2 core's own test of the floating point of the EE, the PS2's main
processor. It runs every FPU instruction (add, subtract, multiply, divide,
square root, reciprocal square root, the multiply-accumulates and the ACC
they add into, the comparisons, MAX, MIN, ABS, NEG and the conversions) on
634,880 sets of operands: 32 chosen values in every combination, and 12,000
pseudo-random sets with exponents mostly near 0, 127 and 255. Each result and
the FPU's status word (FCR31) are kept in memory. Nothing is drawn.

The core's test `fpu:full-mode` runs each disc on PCSX2's recompiler and on
its interpreter, dumps the EE's memory after 600 frames and compares the two
with `compare.py`, which names the instructions that differ. PCSX2's game
database puts 158 titles in the FPU's Full clamp mode, which upstream
implements only in its recompiler; patch 0031 gives the interpreter the same
arithmetic, and without it about 265,000 of the results differ.

The two discs (`../chimera-fputest-full.iso`,
`../chimera-fputest-full-round-up.iso`) hold the same program. PCSX2 takes
the name of the program a disc starts as the game's serial and applies that
game's database entry, and Full mode is only turned on that way. So one disc
starts it as `SLUS_201.51` (Klonoa 2, Full mode) and the other as
`SLUS_203.12` (Final Fantasy X, Full mode with the FPU rounding up rather
than towards zero). Nothing of either game is on them.

It runs on a retail BIOS and on OpenPS2BIOS alike.

Build: `sh build.sh <ps2dev root>` (the ps2dev/ps2dev v2.0.0 image unpacked;
the compiler runs chrooted into it in a user namespace) or with Docker, as
`build.sh` says. Both discs come out byte for byte the same each time.
