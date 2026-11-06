/* chimera-fputest - the PCSX2 core's own test of the EE's floating point.
 *
 * Runs every floating-point instruction of the EE (the PS2's main processor)
 * on chosen and on pseudo-random operands, and keeps each result and the
 * FPU's status word (FCR31) in memory. Nothing is drawn. Two runs of the same
 * disc, one on PCSX2's recompiler and one on its interpreter, must leave the
 * same results: the core's test fpu:full-mode dumps the EE's memory after
 * each run and compare.py, beside this file, compares them.
 *
 * The operands are 32 chosen values (zeroes, denormals, the smallest and
 * largest numbers, exponent 255, which the PS2 treats as an ordinary number,
 * and values next to them) in every pair, and 12000 pseudo-random sets whose
 * exponents are mostly near 0, near 255 and near 127. The ACC, the register
 * the multiply-accumulate instructions add into, is set up first by adda.s
 * or mula.s; it has no instruction to read it, so an ACC result is read out
 * through madd.s with a zero product.
 *
 * The order of the results is the order main() runs them in; compare.py
 * names a result by the same order, so the two change together.
 *
 * The results start with a header that marks where they are (kMagic, put
 * together at run time so that the program's own code does not hold it), the
 * number of results, and a word set when the program has finished.
 *
 * SPDX-License-Identifier: MIT
 */
#include <kernel.h>

void _libcglue_init(void) { }
void _libcglue_deinit(void) { }
DISABLE_PATCHED_FUNCTIONS();

typedef unsigned int u32;

#define MAXR (640 * 1024)
#define DONE 0x600df00du
static struct
{
	u32 magic[4], count, done, pad[2];
	u32 r[MAXR][2]; /* result, FCR31 */
} out __attribute__((aligned(64)));

/* "CHIMERA FPUTEST1", XORed with a word the compiler cannot see */
static const u32 kMagic[4] = {0x4d494843 ^ 0xa5a5a5a5, 0x20415245 ^ 0xa5a5a5a5, 0x54555046 ^ 0xa5a5a5a5, 0x31545345 ^ 0xa5a5a5a5};
static volatile u32 key = 0xa5a5a5a5;

static void put(u32 v, u32 f)
{
	if (out.count < MAXR)
	{
		out.r[out.count][0] = v;
		out.r[out.count][1] = f;
	}
	out.count++;
}

/* FCR31 is cleared, $f1 and $f2 get a and b, the instruction runs, and the
 * result ($f0) and FCR31 are kept. */
#define OP(name, insn) \
	static void name(u32 a, u32 b) \
	{ \
		u32 r, f; \
		__asm__ volatile("ctc1 $0, $31\n\tmtc1 %2, $f1\n\tmtc1 %3, $f2\n\t" insn "\n\tmfc1 %0, $f0\n\tcfc1 %1, $31" \
			: "=r"(r), "=r"(f) : "r"(a), "r"(b) : "$f0", "$f1", "$f2", "memory"); \
		put(r, f); \
	}
OP(t_add, "add.s $f0, $f1, $f2")
OP(t_sub, "sub.s $f0, $f1, $f2")
OP(t_mul, "mul.s $f0, $f1, $f2")
OP(t_div, "div.s $f0, $f1, $f2")
OP(t_max, "max.s $f0, $f1, $f2")
OP(t_min, "min.s $f0, $f1, $f2")
OP(t_rsqrt, "rsqrt.s $f0, $f1, $f2")
OP(t_sqrt, "sqrt.s $f0, $f2")
OP(t_abs, "abs.s $f0, $f1")
OP(t_neg, "neg.s $f0, $f1")
OP(t_cvtw, "cvt.w.s $f0, $f1")
OP(t_cvts, "cvt.s.w $f0, $f1")
OP(t_ceq, "mtc1 $0, $f0\n\tc.eq.s $f1, $f2")
OP(t_clt, "mtc1 $0, $f0\n\tc.lt.s $f1, $f2")
OP(t_cle, "mtc1 $0, $f0\n\tc.le.s $f1, $f2")

/* An instruction that writes the ACC: FCR31 after it, then the ACC read out
 * and FCR31 after that. */
#define ACC(name, insn) \
	static void name(u32 a, u32 b) \
	{ \
		u32 f1, r, f2; \
		__asm__ volatile("ctc1 $0, $31\n\tmtc1 %3, $f1\n\tmtc1 %4, $f2\n\t" insn "\n\tcfc1 %0, $31\n\t" \
			"mtc1 $0, $f3\n\tmadd.s $f0, $f3, $f3\n\tmfc1 %1, $f0\n\tcfc1 %2, $31" \
			: "=&r"(f1), "=&r"(r), "=r"(f2) : "r"(a), "r"(b) : "$f0", "$f1", "$f2", "$f3", "memory"); \
		put(0, f1); \
		put(r, f2); \
	}
ACC(t_adda, "adda.s $f1, $f2")
ACC(t_suba, "suba.s $f1, $f2")
ACC(t_mula, "mula.s $f1, $f2")

/* The ACC set up by `setup` from (c, d), then the multiply-accumulate on
 * (a, b); for one that writes the ACC, the ACC is read out after it. */
#define MAC(name, setup, insn, readout) \
	static void name(u32 c, u32 d, u32 a, u32 b) \
	{ \
		u32 r, f; \
		__asm__ volatile("ctc1 $0, $31\n\tmtc1 %2, $f3\n\tmtc1 %3, $f4\n\tmtc1 %4, $f1\n\tmtc1 %5, $f2\n\t" \
			setup " $f3, $f4\n\t" insn "\n\t" readout "\n\tmfc1 %0, $f0\n\tcfc1 %1, $31" \
			: "=r"(r), "=r"(f) : "r"(c), "r"(d), "r"(a), "r"(b) : "$f0", "$f1", "$f2", "$f3", "$f4", "memory"); \
		put(r, f); \
	}
#define READ "mtc1 $0, $f3\n\tmadd.s $f0, $f3, $f3"
MAC(t_add_madd, "adda.s", "madd.s $f0, $f1, $f2", "nop")
MAC(t_add_msub, "adda.s", "msub.s $f0, $f1, $f2", "nop")
MAC(t_add_madda, "adda.s", "madda.s $f1, $f2", READ)
MAC(t_add_msuba, "adda.s", "msuba.s $f1, $f2", READ)
MAC(t_mul_madd, "mula.s", "madd.s $f0, $f1, $f2", "nop")
MAC(t_mul_msub, "mula.s", "msub.s $f0, $f1, $f2", "nop")
MAC(t_mul_madda, "mula.s", "madda.s $f1, $f2", READ)
MAC(t_mul_msuba, "mula.s", "msuba.s $f1, $f2", READ)

static const u32 kValues[] = {
	0x00000000, 0x80000000, 0x00000001, 0x807fffff, 0x00800000, 0x80800000, 0x00800001, 0x01000000,
	0x3f800000, 0xbf800000, 0x3f800001, 0x3fffffff, 0x3effffff, 0x40490fdb, 0x3e800000, 0x4b000001,
	0x7f000000, 0x7f7fffff, 0xff7fffff, 0x7f800000, 0xff800000, 0x7fc00000, 0x7fffffff, 0xffffffff,
	0x7f400000, 0x34000000, 0x33ffffff, 0x0c000000, 0x00ffffff, 0x3f7fffff, 0x5f800000, 0x1f800000,
};
#define NV (sizeof(kValues) / sizeof(kValues[0]))

static u32 seed = 12345;
static u32 rnd(void)
{
	seed = seed * 1103515245u + 12345u;
	return seed;
}
/* an exponent near 0, near 255, near 127 or anywhere; a sign; a mantissa */
static u32 rndFloat(void)
{
	const u32 k = rnd() >> 16, m = rnd() >> 9, s = rnd() & 0x80000000;
	u32 e;
	switch (k & 3)
	{
		case 0: e = (k >> 2) & 3; break;
		case 1: e = 255 - ((k >> 2) & 3); break;
		case 2: e = 115 + ((k >> 2) % 26); break;
		default: e = (k >> 2) & 255; break;
	}
	return s | (e << 23) | (m & 0x7fffff);
}

static void unary(u32 a)
{
	t_sqrt(0, a); t_abs(a, 0); t_neg(a, 0); t_cvtw(a, 0); t_cvts(a, 0);
}

static void pair(u32 a, u32 b)
{
	t_add(a, b); t_sub(a, b); t_mul(a, b); t_div(a, b); t_max(a, b); t_min(a, b);
	t_rsqrt(a, b); t_ceq(a, b); t_clt(a, b); t_cle(a, b);
	t_adda(a, b); t_suba(a, b); t_mula(a, b);
}

static void mac(u32 c, u32 d, u32 a, u32 b)
{
	t_add_madd(c, d, a, b); t_add_msub(c, d, a, b); t_add_madda(c, d, a, b); t_add_msuba(c, d, a, b);
	t_mul_madd(c, d, a, b); t_mul_msub(c, d, a, b); t_mul_madda(c, d, a, b); t_mul_msuba(c, d, a, b);
}

int main(void)
{
	for (u32 i = 0; i < 4; i++)
		out.magic[i] = kMagic[i] ^ key;
	for (u32 i = 0; i < NV; i++)
	{
		unary(kValues[i]);
		for (u32 j = 0; j < NV; j++)
			pair(kValues[i], kValues[j]);
	}
	/* the ACC from each value, and from MAX + MAX (which overflows), with every pair */
	for (u32 i = 0; i <= NV; i++)
	{
		const u32 c = i < NV ? kValues[i] : 0x7fffffff, d = i < NV ? 0 : 0x7fffffff;
		for (u32 j = 0; j < NV; j++)
			for (u32 k = 0; k < NV; k++)
				mac(c, d, kValues[j], kValues[k]);
	}
	for (u32 n = 0; n < 12000; n++)
	{
		const u32 a = rndFloat(), b = rndFloat(), c = rndFloat(), d = rndFloat();
		unary(a);
		pair(a, b);
		mac(c, d, a, b);
	}
	out.done = DONE;
	for (;;)
		;
	return 0;
}
