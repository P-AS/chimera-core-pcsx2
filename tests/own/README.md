Content this repository may legally ship, and therefore does track. The sibling
`tests/roms/` is ignored wholesale (see the .gitignore, which explains why): a
PlayStation 2 needs a bios dump and the discs are somebody's, so nothing there
can be committed and no per-file rule is trusted to keep it that way. Anything
free to distribute goes here instead, where tracking it needs no exception.

- `padtest.elf` - a PlayStation 2 controller tester by jbit, which draws every
  button and stick of a DualShock 2 as the machine sees them. Its author
  published it for free download at
  https://www.psx-place.com/resources/ps2-controller-tester-by-jbit.670/
  No licence was found that says the file may be passed on (looked
  2026-10-13): it is here as published, not under this repository's licence,
  and goes if its author asks.
  It is what the input gate reads: a core can only claim its buttons arrive in
  the right places if something on the machine says so.
