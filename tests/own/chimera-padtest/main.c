/* chimera-padtest - the PCSX2 core's own PlayStation 2 test program.
 *
 * Draws, for each of the two controller ports, a grid of 16 cells, one per
 * button of a DualShock 2, lit while the machine reads that button as held,
 * and under them a bar that grows by one pixel a frame (it wraps at 512).
 * Nothing it shows depends on anything but the pads and the frame count.
 *
 * It draws into one framebuffer, the whole picture once and then only what
 * changes: a cell whose button changed, the bar's new pixel. So the picture
 * lives in the GS's memory from frame to frame, as most games' does, and a
 * renderer that lost that memory across a savestate load shows it (the
 * core's gl/vk:picture-after-load tests depend on that).
 *
 * It loads ps2sdk's own pad drivers (freesio2, freepad) from inside itself
 * rather than the BIOS's, so that it reads the pads the same way on a retail
 * BIOS and on OpenPS2BIOS, whose pad module answers nothing. Three things
 * are left out or tried in order so that it runs on both:
 * - ps2sdk's libc start-up, which opens the IOP's file services over SIF
 *   before main: nothing here uses files, and on OpenPS2BIOS it never
 *   returns;
 * - ps2sdk's replacements of kernel functions (DISABLE_PATCHED_FUNCTIONS);
 * - ps2sdk's patch that lets a retail BIOS load a module from memory
 *   (sbv_patch_enable_lmb): OpenPS2BIOS can already, and never returns from
 *   the patch, while a retail BIOS never answers a load without it. So the
 *   program looks for OpenPS2BIOS's name in the first 128 KB of the ROM and
 *   applies the patch only when it is not there.
 * The pad state comes through libpad every frame, as a game reads it.
 *
 * It also reads the console's clock once, at the start, through the BIOS's
 * CDVD service (libcdvd's sceCdReadClock), as a game that shows the date or
 * stamps its saves does. The clock lives in the drive's controller, which
 * only the IOP reaches, so the answer passes through the IOP's memory: a
 * project's clock setting then changes what the machine holds (the core's
 * frontend test settings:clock depends on that). Nothing drawn depends on it.
 *
 * The layout (640x448, NTSC; tests/own/chimera-padtest/cells.py reads it):
 * port p's cells start at y = 40 + 200 * p; button i (in the order of
 * kButtons below) is the 64x40 cell at x = 40 + 72 * (i % 8),
 * y = 40 + 200 * p + 56 * (i / 8). The bar is at y 400-416.
 *
 * SPDX-License-Identifier: MIT
 */
#include <kernel.h>
#include <loadfile.h>
#include <sifrpc.h>
#include <sbv_patches.h>
#include <libpad.h>
#include <libcdvd.h>
#include <gsKit.h>
#include <dmaKit.h>

#include "irx.h" /* freesio2_irx, freepad_irx (build.sh writes it) */

void _libcglue_init(void) { }
void _libcglue_deinit(void) { }
DISABLE_PATCHED_FUNCTIONS();

static char padArea[2][256] __attribute__((aligned(64)));
/* the clock as the BIOS answered it (seconds to year, in BCD) */
static sceCdCLOCK rtc;

/* the button bit libpad reports (active low), in the cells' order */
static const unsigned short kButtons[16] = {
	PAD_UP, PAD_DOWN, PAD_LEFT, PAD_RIGHT, PAD_START, PAD_SELECT,
	PAD_SQUARE, PAD_CROSS, PAD_CIRCLE, PAD_TRIANGLE,
	PAD_L1, PAD_R1, PAD_L2, PAD_R2, PAD_L3, PAD_R3,
};

/* is the BIOS OpenPS2BIOS: its name in the first 128 KB of the ROM, read
 * through the kernel (kCopyBytes) a page at a time - a program may not have
 * the right to read the ROM itself */
static int open_bios(void)
{
	static const char name[] = "OpenPS2BIOS";
	static unsigned char page[4096 + 16] __attribute__((aligned(64)));
	for (int at = 0; at < 0x20000; at += 4096) {
		kCopyBytes(page, (const void *)(0xbfc00000 + at), sizeof page);
		for (int i = 0; i < 4096; i++) {
			int k = 0;
			while (k < (int)sizeof name - 1 && page[i + k] == (unsigned char)name[k])
				k++;
			if (k == (int)sizeof name - 1)
				return 1;
		}
	}
	return 0;
}

static void load_drivers(void)
{
	int ret;
	SifInitRpc(0);
	if (!open_bios())
		sbv_patch_enable_lmb();
	SifExecModuleBuffer(freesio2_irx, size_freesio2_irx, 0, NULL, &ret);
	SifExecModuleBuffer(freepad_irx, size_freepad_irx, 0, NULL, &ret);
	padInit(0);
	padPortOpen(0, 0, padArea[0]);
	padPortOpen(1, 0, padArea[1]);
	sceCdInit(SCECdINoD);
	sceCdReadClock(&rtc);
}

/* the buttons held on a port, a bit each (1 = held), or 0 */
static unsigned short held(int port)
{
	struct padButtonStatus b;
	int state = padGetState(port, 0);
	if (state != PAD_STATE_STABLE && state != PAD_STATE_FINDCTP1)
		return 0;
	if (padRead(port, 0, &b) == 0)
		return 0;
	return (unsigned short)(0xffff ^ b.btns);
}

int main(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	load_drivers();

	GSGLOBAL *gs = gsKit_init_global();
	gs->DoubleBuffering = GS_SETTING_OFF;
	gs->ZBuffering = GS_SETTING_OFF;
	dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC, D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);
	dmaKit_chan_init(DMA_CHANNEL_GIF);
	gs->PrimAlphaEnable = GS_SETTING_OFF;
	gsKit_init_screen(gs);
	gsKit_mode_switch(gs, GS_ONESHOT);

	const u64 back = GS_SETREG_RGBAQ(0x10, 0x18, 0x30, 0x80, 0x00);
	const u64 up = GS_SETREG_RGBAQ(0x40, 0x40, 0x48, 0x80, 0x00);
	const u64 down = GS_SETREG_RGBAQ(0xf0, 0xc0, 0x20, 0x80, 0x00);
	const u64 bar = GS_SETREG_RGBAQ(0xe0, 0xe0, 0xe0, 0x80, 0x00);
	unsigned short shown[2] = { 0, 0 };
	unsigned frame = 0;
	for (;;) {
		const int whole = frame == 0;
		if (whole)
			gsKit_clear(gs, back);
		for (int p = 0; p < 2; p++) {
			const unsigned short h = held(p);
			for (int i = 0; i < 16; i++) {
				if (!whole && ((h ^ shown[p]) & kButtons[i]) == 0)
					continue;
				const float x = 40 + 72 * (i % 8), y = 40 + 200 * p + 56 * (i / 8);
				gsKit_prim_sprite(gs, x, y, x + 64, y + 40, 1, (h & kButtons[i]) ? down : up);
			}
			shown[p] = h;
		}
		/* the bar: its new pixel, or, when it wraps, the empty bar again */
		const unsigned at = frame % 512;
		if (at == 0)
			gsKit_prim_sprite(gs, 40, 400, 40 + 513, 416, 1, back);
		gsKit_prim_sprite(gs, 40 + at, 400, 40 + at + 1, 416, 1, bar);
		gsKit_queue_exec(gs);
		gsKit_sync_flip(gs);
		frame++;
	}
	return 0;
}
