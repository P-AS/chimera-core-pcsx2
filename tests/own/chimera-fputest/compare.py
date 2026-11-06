#!/usr/bin/env python3
# compare.py <EE RAM A> <EE RAM B>: are chimera-fputest's results the same in
# two dumps of the EE's memory? Prints one line and exits 0 when they are; 1,
# with the instructions that differ and an example of each, when they are
# not; 2 when a dump does not hold finished results.
#
# Bits 0 and 24 of FCR31 are left out of the comparison. PCSX2's recompiler
# sets them whenever a program writes FCR31 and its interpreter does not, for
# every program and every clamp mode, which is not what the test is about.
#
# A result is named by its place, in the order main.c runs them, so the two
# change together.
# SPDX-License-Identifier: MIT
import collections
import struct
import sys

MAGIC = b"CHIMERA FPUTEST1"
DONE = 0x600DF00D
IGNORED_FCR31 = 0x01000001

V = [0x00000000, 0x80000000, 0x00000001, 0x807fffff, 0x00800000, 0x80800000, 0x00800001, 0x01000000,
     0x3f800000, 0xbf800000, 0x3f800001, 0x3fffffff, 0x3effffff, 0x40490fdb, 0x3e800000, 0x4b000001,
     0x7f000000, 0x7f7fffff, 0xff7fffff, 0x7f800000, 0xff800000, 0x7fc00000, 0x7fffffff, 0xffffffff,
     0x7f400000, 0x34000000, 0x33ffffff, 0x0c000000, 0x00ffffff, 0x3f7fffff, 0x5f800000, 0x1f800000]
UNARY = ["sqrt", "abs", "neg", "cvt.w", "cvt.s"]
PAIR = ["add", "sub", "mul", "div", "max", "min", "rsqrt", "c.eq", "c.lt", "c.le",
        "adda (FCR31)", "adda (ACC)", "suba (FCR31)", "suba (ACC)", "mula (FCR31)", "mula (ACC)"]
MAC = ["adda, madd", "adda, msub", "adda, madda", "adda, msuba",
       "mula, madd", "mula, msub", "mula, madda", "mula, msuba"]


def labels():
    out = []
    for i in range(32):
        out += [(n, (V[i],)) for n in UNARY]
        for j in range(32):
            out += [(n, (V[i], V[j])) for n in PAIR]
    for i in range(33):
        c, d = (V[i], 0) if i < 32 else (0x7fffffff, 0x7fffffff)
        for j in range(32):
            for k in range(32):
                out += [(n, (c, d, V[j], V[k])) for n in MAC]
    seed = 12345

    def rnd():
        nonlocal seed
        seed = (seed * 1103515245 + 12345) & 0xffffffff
        return seed

    def rnd_float():
        k = rnd() >> 16
        m = rnd() >> 9
        s = rnd() & 0x80000000
        e = [(k >> 2) & 3, 255 - ((k >> 2) & 3), 115 + ((k >> 2) % 26), (k >> 2) & 255][k & 3]
        return s | (e << 23) | (m & 0x7fffff)

    for _ in range(12000):
        a, b, c, d = rnd_float(), rnd_float(), rnd_float(), rnd_float()
        out += [(n, (a,)) for n in UNARY] + [(n, (a, b)) for n in PAIR] + [(n, (c, d, a, b)) for n in MAC]
    return out


def results(path, want):
    ram = open(path, "rb").read()
    at = ram.find(MAGIC)
    if at < 0 or ram.find(MAGIC, at + 1) >= 0:
        sys.exit(f"{path}: the results are not in it (no single header)")
    count, done = struct.unpack_from("<II", ram, at + 16)
    if done != DONE:
        sys.exit(f"{path}: the program had not finished")
    if count != want:
        sys.exit(f"{path}: {count} results, want {want}")
    return struct.unpack_from(f"<{2 * count}I", ram, at + 32)


def main():
    names = labels()
    a = results(sys.argv[1], len(names))
    b = results(sys.argv[2], len(names))
    differ = collections.Counter()
    example = {}
    for i, (n, ops) in enumerate(names):
        va, fa = a[2 * i], a[2 * i + 1] & ~IGNORED_FCR31
        vb, fb = b[2 * i], b[2 * i + 1] & ~IGNORED_FCR31
        if (va, fa) != (vb, fb):
            what = " and ".join(x for x, d in (("result", va != vb), ("FCR31", fa != fb)) if d)
            differ[(n, what)] += 1
            example.setdefault((n, what), (ops, va, fa, vb, fb))
    if not differ:
        print(f"{len(names)} results, the same")
        return 0
    print(f"{sum(differ.values())} of {len(names)} results differ:")
    for (n, what), count in differ.most_common():
        ops, va, fa, vb, fb = example[(n, what)]
        print(f"  {n}: {what}, {count} times; e.g. {' '.join(f'{o:08x}' for o in ops)}"
              f" gives {va:08x} (FCR31 {fa:08x}) vs {vb:08x} (FCR31 {fb:08x})")
    return 1


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__ or "compare.py <EE RAM A> <EE RAM B>")
    try:
        sys.exit(main())
    except SystemExit as e:
        if isinstance(e.code, str):
            print(e.code)
            sys.exit(2)
        raise
