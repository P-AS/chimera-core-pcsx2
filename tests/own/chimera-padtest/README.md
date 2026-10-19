# chimera-padtest

The PCSX2 core's own PlayStation 2 test program, and the disc that holds it
(`../chimera-padtest.iso`, with `../chimera-padtest.elf` beside it). It is
this repository's, free to copy (see `LICENSE`), so the tests that need a
program can run anywhere, CI included.

For each of the two controller ports it draws 16 cells, one per button of a
DualShock 2, lit while the machine reads that button as held, and under them
a bar that grows a pixel a frame. `cells.py` reads which cells are lit in a
screenshot. It loads ps2sdk's own pad drivers from inside itself, so it reads
the pads on OpenPS2BIOS (the open BIOS the tests use where nobody has a dump)
as on a retail BIOS; `main.c` says what it does differently for each. At the
start it also asks the BIOS for the console's clock, as a game that shows the
date does, so a project's clock setting changes what the machine holds.

Build: `sh build.sh <ps2dev root>` (the ps2dev/ps2dev v2.0.0 image unpacked;
the compiler runs chrooted into it in a user namespace) or with Docker, as
`build.sh` says. Both outputs come out byte for byte the same each time.
