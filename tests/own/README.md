Content this repository may legally ship, and therefore does track. The sibling
`tests/roms/` is ignored wholesale (see the .gitignore, which explains why): a
PlayStation 2 needs a bios dump and the discs are somebody's, so nothing there
can be committed and no per-file rule is trusted to keep it that way. Anything
free to distribute goes here instead, where tracking it needs no exception.

- `chimera-padtest.iso` and `chimera-padtest.elf` - this repository's own
  test program, on a disc of its own and as a bare executable: a cell per
  button for both ports, lit while held, and a bar that grows a pixel a
  frame, drawn once and then only where it changes; it reads the console's
  clock once at the start. Source, build script,
  cell reader and licences (MIT for the program; AFL-2.0 for the ps2sdk and
  gsKit code it is linked with; newlib's) are in `chimera-padtest/`. It reads
  the pads on a retail BIOS and on OpenPS2BIOS alike, and is what the gate's
  pad, gun, column and renderer checks run, and the disc the frontend test
  runs when there is no game.
