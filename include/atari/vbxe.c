#include "vbxe.h"
#include "os.h"
#include <string.h>

#define CONTROL 0x40
#define REVISION 0x41
#define XDL 0x41
#define COLOR_INDEX 0x44
#define PALETTE_INDEX 0x45
#define COLOR_RED 0x46
#define COLOR_GREEN 0x47
#define COLOR_BLUE 0x48
#define BLITTER_ADDR 0x50
#define BLITTER_STATUS 0x53
#define MEMAC_CONTROL 0x5e
#define MEMAC_BANK 0x5f

static byte revision(volatile byte * regs)
{
	if (regs[CONTROL] != 0x10)
		return 0;
	return regs[REVISION];
}

bool vbxe_detect(AVbxe * board)
{
	if (!board)
		return false;
	volatile byte * regs = (volatile byte *)0xd600;
	byte rev = revision(regs);
	if (!(rev & 0x7f))
	{
		regs = (volatile byte *)0xd700;
		rev = revision(regs);
		if (!(rev & 0x7f))
			return false;
	}
	board->regs = regs;
	board->revision = rev;
	board->palette = nullptr;
	return true;
}

static bool fits_window(const byte * data, word size)
{
	word start = (word)data;
	return start && start <= 0xffffu - (size - 1) &&
		(start > VBXE_MEMAC_WINDOW + VBXE_MEMAC_SIZE - 1 || start + (size - 1) < VBXE_MEMAC_WINDOW);
}

enum Transfer {READ_VRAM, WRITE_VRAM, FILL_VRAM};

static bool transfer(const AVbxe * board, unsigned long addr, byte * data, word size, byte value, byte operation)
{
	if (!board || addr >= VBXE_VRAM_SIZE || size > VBXE_VRAM_SIZE - addr)
		return false;
	if (!size)
		return true;
	if (operation != FILL_VRAM && !fits_window(data, size))
		return false;

	volatile byte * regs = board->regs;
	byte control = regs[MEMAC_CONTROL];
	byte bank = regs[MEMAC_BANK];
	regs[MEMAC_BANK] = 0;
	regs[MEMAC_CONTROL] = 0xa8;

	while (size)
	{
		word offset = (word)addr & 0x0fffu;
		word count = VBXE_MEMAC_SIZE - offset;
		if (count > size)
			count = size;
		regs[MEMAC_BANK] = (byte)(addr >> 12) | 0x80;
		volatile byte * window = (volatile byte *)(VBXE_MEMAC_WINDOW + offset);
		switch (operation)
		{
		case READ_VRAM:
			for (word i = 0; i < count; i++)
				data[i] = window[i];
			data += count;
			break;
		case WRITE_VRAM:
			for (word i = 0; i < count; i++)
				window[i] = data[i];
			data += count;
			break;
		default:
			for (word i = 0; i < count; i++)
				window[i] = value;
			break;
		}
		size -= count;
		addr += count;
	}

	regs[MEMAC_BANK] = 0;
	regs[MEMAC_CONTROL] = control;
	regs[MEMAC_BANK] = bank;
	return true;
}

bool vbxe_read(const AVbxe * board, unsigned long addr, byte * dest, word size)
{
	return transfer(board, addr, dest, size, 0, READ_VRAM);
}

bool vbxe_write(const AVbxe * board, unsigned long addr, const byte * data, word size)
{
	return transfer(board, addr, (byte *)data, size, 0, WRITE_VRAM);
}

bool vbxe_fill(const AVbxe * board, unsigned long addr, byte value, word size)
{
	return transfer(board, addr, 0, size, value, FILL_VRAM);
}

// Keep the three ordered hardware writes and the shadow update visible
// across optimization profiles, including when the palette is constant.
#pragma optimize(push, 0, noinline, noconstparams)
__noinline bool vbxe_palette(const AVbxe * board, byte palette, byte first, const AVbxeColor * colors, word count)
{
	if (!board || palette >= 4 || count > 256u - first || (count && !colors))
		return false;
	volatile byte * regs = board->regs;
	regs[PALETTE_INDEX] = palette;
	regs[COLOR_INDEX] = first;
	for (word i = 0; i < count; i++)
	{
		regs[COLOR_RED] = colors[i].red;
		regs[COLOR_GREEN] = colors[i].green;
		regs[COLOR_BLUE] = colors[i].blue;
	}
	AVbxePalette * state = board->palette;
	if (state && state->bank == palette && count)
	{
		memmove(state->colors + first, colors, count * sizeof(AVbxeColor));
		word end = first + count;
		for (word i = first; i < end; i++)
			state->known[i >> 3] |= 1 << (i & 7);
	}
	return true;
}

bool vbxe_palette_track(AVbxe * board, byte bank, AVbxePalette * state)
{
	if (!board || (state && bank >= 4))
		return false;
	if (state)
	{
		state->bank = bank;
		for (byte i = 0; i < sizeof(state->known); i++)
			state->known[i] = 0;
	}
	board->palette = state;
	return true;
}

const AVbxeColor * vbxe_palette_entries(const AVbxe * board, byte bank, byte first, word count)
{
	if (!board || count > 256u - first)
		return 0;
	const AVbxePalette * state = board->palette;
	if (!state || state->bank != bank)
		return 0;
	word end = first + count;
	for (word i = first; i < end; i++)
		if (!(state->known[i >> 3] & (1 << (i & 7))))
			return 0;
	return state->colors + first;
}

#pragma optimize(pop)

static void write_addr(volatile byte * dest, unsigned long addr)
{
	dest[0] = addr;
	dest[1] = addr >> 8;
	dest[2] = addr >> 16;
}

void vbxe_control(const AVbxe * board, byte control)
{
	board->regs[CONTROL] = control;
}

bool vbxe_xdl(const AVbxe * board, unsigned long list, byte control)
{
	if (!board || list >= VBXE_VRAM_SIZE)
		return false;
	os_wait_frame();
	board->regs[CONTROL] = 0;
	write_addr(board->regs + XDL, list);
	board->regs[CONTROL] = control;
	return true;
}

bool vbxe_blitter_busy(const AVbxe * board)
{
	return (board->regs[BLITTER_STATUS] & 3) != 0;
}

bool vbxe_blitter_start(const AVbxe * board, unsigned long list)
{
	if (!board || list >= VBXE_VRAM_SIZE || vbxe_blitter_busy(board))
		return false;
	write_addr(board->regs + BLITTER_ADDR, list);
	board->regs[BLITTER_STATUS] = 1;
	return true;
}

bool vbxe_bcb(volatile byte * encoded, const AVbxeBlit * request)
{
	// Preserve the descriptor pointer on Oscar64. Folding a const
	// aggregate through this pointer can reject valid VRAM addresses.
	const AVbxeBlit * volatile blit = request;
	if (!encoded || !blit || blit->src >= VBXE_VRAM_SIZE || blit->dst >= VBXE_VRAM_SIZE ||
		!blit->width || blit->width > 512 || !blit->height || blit->height > 256 ||
		blit->src_pitch < -2048 || blit->src_pitch > 2047 ||
		blit->dst_pitch < -2048 || blit->dst_pitch > 2047)
		return false;
	write_addr(encoded, blit->src);
	encoded[3] = blit->src_pitch;
	encoded[4] = blit->src_pitch >> 8;
	encoded[5] = blit->src_step;
	write_addr(encoded + 6, blit->dst);
	encoded[9] = blit->dst_pitch;
	encoded[10] = blit->dst_pitch >> 8;
	encoded[11] = blit->dst_step;
	word width = blit->width - 1;
	encoded[12] = width;
	encoded[13] = width >> 8;
	encoded[14] = blit->height - 1;
	encoded[15] = blit->and_mask;
	encoded[16] = blit->xor_mask;
	encoded[17] = blit->collision_mask;
	encoded[18] = blit->zoom;
	encoded[19] = blit->pattern;
	encoded[20] = blit->control;
	return true;
}

bool vbxe_blit(const AVbxe * board, unsigned long command, const AVbxeBlit * blit)
{
	if (!board || vbxe_blitter_busy(board))
		return false;
	byte encoded[VBXE_BCB_SIZE];
	if (!vbxe_bcb(encoded, blit) || !vbxe_write(board, command, encoded, sizeof(encoded)))
		return false;
	return vbxe_blitter_start(board, command);
}

// Oscar64's optimizer can crash when folding the temporary XDL
// buffer into a caller. This setup runs once per display change.
#pragma optimize(push, 0, noinline, noconstparams)
bool vbxe_show_indexed(const AVbxe * board, const AVbxeOverlay * request)
{
	const AVbxeOverlay * volatile overlay = request;
	if (!board || !overlay || overlay->pitch < 320 || overlay->pitch > 4095 ||
		!overlay->height || (word)overlay->top + overlay->height > 240 || overlay->palette >= 4 ||
		overlay->list > VBXE_VRAM_SIZE - 15 || overlay->pixels >= VBXE_VRAM_SIZE)
		return false;
	unsigned long extent = (unsigned long)(overlay->height - 1) * overlay->pitch + 320;
	if (extent > VBXE_VRAM_SIZE - overlay->pixels ||
		(overlay->list < overlay->pixels + extent && overlay->pixels < overlay->list + 15))
		return false;

	byte list[15];
	byte at = 0;
	if (overlay->top)
	{
		list[at++] = 0x24; // Disable overlay and repeat blank rows.
		list[at++] = 0;
		list[at++] = overlay->top - 1;
	}
	list[at++] = 0x62; // Graphics overlay, repeated rows and an address.
	list[at++] = 8;    // Standard resolution, with overlay attributes.
	list[at++] = overlay->height - 1;
	list[at++] = overlay->pixels;
	list[at++] = overlay->pixels >> 8;
	list[at++] = overlay->pixels >> 16;
	list[at++] = overlay->pitch;
	list[at++] = overlay->pitch >> 8;
	list[at++] = 1 | (overlay->palette << 4); // Normal width.
	list[at++] = 0xff; // Overlay in front of ANTIC and PMG.
	list[at++] = 4;
	list[at++] = 0x80; // End the XDL and remove the overlay.
	if (!vbxe_write(board, overlay->list, list, at))
		return false;
	return vbxe_xdl(board, overlay->list, VBXE_XDL_ENABLED | VBXE_OPAQUE);
}
#pragma optimize(pop)
