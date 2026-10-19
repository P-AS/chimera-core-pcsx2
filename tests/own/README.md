Content this repository may legally ship, and therefore does track. The sibling
`tests/roms/` is ignored wholesale (see the .gitignore, which explains why): a
PlayStation 2 needs a bios dump and the discs are somebody's, so nothing there
can be committed and no per-file rule is trusted to keep it that way. Anything
free to distribute goes here instead, where tracking it needs no exception.

- `chimera-padtest.iso` and `chimera-padtest.elf` - this repository's own
  test program, on a disc of its own and as a bare executable: a cell per
  button for both ports, lit while held, and a bar that grows a pixel a
  frame, drawn once and then only where it changes. Source, build script,
  cell reader and licences (MIT for the program; AFL-2.0 for the ps2sdk and
  gsKit code it is linked with; newlib's) are in `chimera-padtest/`. It reads
  the pads on a retail BIOS and on OpenPS2BIOS alike, and is what the gate's
  pad, gun, column and renderer checks run.
- `padtest.elf` - a PlayStation 2 controller tester by jbit, which draws
  every button and stick of a DualShock 2 as the machine sees them. No longer
  used by the gate (chimera-padtest replaced it on 2026-10-19); kept until
  the owner decides. Its author published it for free download at
  https://www.psx-place.com/resources/ps2-controller-tester-by-jbit.670/
  No licence was found that says the file may be passed on (looked
  2026-10-13): it is here as published, not under this repository's licence,
  and goes if its author asks.
