#include "hcm.h"
#include "dlist.h"
#include "gtia.h"
#include "irq.h"
#include "os.h"
#include "pmg.h"

#define HCM_TOP 28

static volatile byte saved_sp;

__asm hcm_raster
{
	pha
	txa
	pha
	tya
	pha
	tsx
	stx saved_sp
	ldx #0
	txs
row:
	sta $d40a
	lda #$48
	sta $d001
	sta $d000
	nop
	nop
	nop
	nop
	nop
left_bits:
	ldy $811c,x
right_bits:
	lda $821c,x
	tax
	lda #$98
	sta $d000
	sty $d00d
	sta $d001
	stx $d00e
	tsx
	inx
	txs
	cpx #200
	bne row
	ldx saved_sp
	txs
	pla
	tay
	pla
	tax
	pla
	rti
}

static bool intersects(word a, word n, word b, word m)
{
	return a <= b + (m - 1) && b <= a + (n - 1);
}

static volatile byte unused_row_color;

static bool address_range(word address, word count)
{
	return address && address <= 0xffffu - (count - 1);
}

static bool row_program_ok(const AHcm * screen, const AHcmRows * program, const byte * pmg, word list_size)
{
	word code = (word)program->code, colors = (word)program->colors;
	word count = pmg ? HCM_PMG_ROW_PROGRAM_SIZE : HCM_ROW_PROGRAM_SIZE;
	word table = sizeof(AHcmRowColor) * HCM_ROWS;
	if (program->capacity < count || !address_range(code, count) || !address_range(colors, table) ||
		intersects(code, count, colors, table) ||
		(pmg && (!address_range((word)pmg, HCM_ROWS) || intersects(code, count, (word)pmg, HCM_ROWS))) ||
		intersects(code, count, (word)program, sizeof(AHcmRows)) ||
		intersects(code, count, (word)screen, sizeof(AHcm)) ||
		intersects(code, count, (word)screen->pixels, HCM_SCANLINES * 32) ||
		intersects(code, count, (word)screen->list, list_size) ||
		intersects(code, count, (word)screen->players, 2048))
		return false;
	for (byte i = 0; i < HCM_ROWS; i++)
	{
		byte reg = program->colors[i].reg;
		if (reg && (reg < 0x12 || reg > 0x1a))
			return false;
	}
	return true;
}

static void put_address(volatile byte * dest, word address)
{
	dest[0] = address;
	dest[1] = address >> 8;
}

static __noinline void build_rows(const AHcmRows * program, word players, const byte * pmg)
{
	static const byte start[] =
	{
		0x48, 0x8a, 0x48, 0x98, 0x48, 0xba, 0x8e, 0, 0, 0xa2, 0, 0x9a
	};
	// Each block loops for eight or four scanlines. The embedded color write
	// takes the same ten cycles as the five NOPs in hcm_raster.
	static const byte scan[] =
	{
		0x8d, 0x0a, 0xd4,                   // STA WSYNC
		0xa9, 0x48, 0x8d, 1, 0xd0, 0x8d, 0, 0xd0,
		0xa0, 0, 0x8c, 0, 0, 0xea, 0xea,    // LDY #color, STY reg, NOP, NOP
		0xbc, 0, 0, 0xbd, 0, 0,             // LDY plane1,X, LDA plane2,X
		0xaa, 0xa9, 0x98,
		0x8d, 0, 0xd0, 0x8c, 0x0d, 0xd0,
		0x8d, 1, 0xd0, 0x8e, 0x0e, 0xd0,
		0xba, 0xe8, 0x9a, 0xe0, 0, 0xd0, 0xd2
	};
	static const byte end[] =
	{
		0xae, 0, 0, 0x9a, 0x68, 0xa8, 0x68, 0xaa, 0x68, 0x40
	};
	volatile byte * code = program->code;
	for (byte i = 0; i < sizeof(start); i++)
		code[i] = start[i];
	put_address(code + 7, (word)&saved_sp);
	code += sizeof(start);
	for (byte row = 0; row < HCM_ROWS; row++)
	{
		byte last = (row + 1) << 3;
		for (byte i = 0; i < sizeof(scan); i++)
			code[i] = scan[i];
		byte reg = program->colors[row].reg;
		code[12] = program->colors[row].value;
		put_address(code + 14, reg ? 0xd000u + reg : (word)&unused_row_color);
		put_address(code + 19, players + 0x100 + HCM_TOP);
		put_address(code + 22, players + 0x200 + HCM_TOP);
		code[43] = pmg ? last - 4 : last;
		if (!pmg)
		{
			code += sizeof(scan);
			continue;
		}

		// Both color stores fit in the same ten-cycle slot as the
		// ordinary color write. Avoid any extra cycles before WSYNC.
		volatile byte * mid = code + sizeof(scan);
		for (byte i = 0; i < 11; i++)
			mid[i] = code[i];
		mid[11] = 0xa0; // LDY #color
		mid[12] = pmg[row];
		mid[13] = 0x8c; // STY $D012
		put_address(mid + 14, 0xd012);
		mid[16] = 0x8c; // STY $D014
		put_address(mid + 17, 0xd014);
		for (byte i = 18; i < sizeof(scan); i++)
			mid[i + 1] = code[i];
		mid[44] = last;
		mid[46] = (byte)(0u - (sizeof(scan) + 1));
		code = mid + sizeof(scan) + 1;
	}
	for (byte i = 0; i < sizeof(end); i++)
		code[i] = end[i];
	put_address(code + 1, (word)&saved_sp);
}

static __noinline bool show(const AHcm * screen_request, const AHcmRows * request, const byte * pmg_request)
{
	// Keep the linker addresses used by the overlap checks until runtime.
	const AHcm * volatile screen = screen_request;
	const AHcmRows * volatile program = request;
	const byte * volatile pmg = pmg_request;
	if (program && !address_range((word)program, sizeof(AHcmRows)))
		return false;
	if (!screen || !screen->players || ((word)screen->players & 2047u) ||
		(screen->mode != DL_MODEE && screen->mode != DL_MODEF))
		return false;
	ABitmap view;
	if (!abm_init_width(&view, screen->pixels, screen->pixel_size, 32, HCM_SCANLINES,
		screen->mode, DMACTL_PF_NARROW))
		return false;
	word size = abm_display_list_size(screen->list, screen->list_size, &view, screen->mode, 20);
	word players = (word)screen->players, descriptor = (word)screen;
	if (!size || descriptor > 0xffffu - (sizeof(AHcm) - 1) ||
		intersects(players, 2048, (word)screen->pixels, HCM_SCANLINES * 32) ||
		intersects(players, 2048, (word)screen->list, size) ||
		intersects(players, 2048, descriptor, sizeof(AHcm)) ||
		intersects((word)screen->list, size, descriptor, sizeof(AHcm)) ||
		(program && !row_program_ok(screen, program, pmg, size)))
		return false;

	os_wait_frame();
	dli_off();
	os_sdmctl &= (byte)~(DMACTL_DLIST | DMACTL_PLAYERS | DMACTL_MISSILES);
	os_wait_frame();
	if (program)
		build_rows(program, players, pmg);
	abm_display_list(screen->list, size, &view, screen->mode, 20);
	screen->list[2] |= DL_DLI;
	for (word i = 256; i < 768; i++)
		screen->players[i] = 0;
	pmg_init(screen->players, PMG_1LINE);
	byte base = players >> 8;
	__asm {
		lda base
		clc
		adc #1
		sta hcm_raster.left_bits + 2
		clc
		adc #1
		sta hcm_raster.right_bits + 2
	}
	gtia.sizep[0] = 3;
	gtia.sizep[1] = 3;
	gtia.sizep[2] = 3;
	gtia.sizep[3] = 3;
	gtia.sizem = 255;
	gtia.hposp[0] = 0x48;
	gtia.hposp[1] = 0x48;
	gtia.hposp[2] = 0x68;
	gtia.hposp[3] = 0x68;
	gtia.hposm[0] = 0x88;
	gtia.hposm[1] = 0x88;
	gtia.hposm[2] = 0x90;
	gtia.hposm[3] = 0x90;
	os_gprior = PRIOR_MULTICOLOR | (screen->mode == DL_MODEF ? PRIOR_P0123_PF0123 : PRIOR_PF0123_P0123);
	os_savmsc = (word)screen->pixels;
	os_sdlst = (word)screen->list;
	os_sdmctl = (os_sdmctl & ~3) | DMACTL_DLIST | DMACTL_PF_NARROW;
	os_wait_frame();
	if (program)
		dli_set((IRQHandler)(word)program->code);
	else
		dli_set((IRQHandler)hcm_raster);
	return true;
}

bool hcm_show(const AHcm * screen)
{
	return show(screen, 0, 0);
}

bool hcm_show_rows(const AHcm * screen, const AHcmRows * program)
{
	return program && show(screen, program, 0);
}

bool hcm_show_pmg_rows(const AHcm * screen, const AHcmRows * program, const byte * pmg)
{
	return program && pmg && show(screen, program, pmg);
}

struct HcmCell
{
	volatile byte * first, * second;
	byte first_mask, second_mask;
};

static void cell(HcmCell * result, const AHcm * screen, byte column, byte line)
{
	static const byte first_missile[4] = {2, 1, 32, 16};
	static const byte second_missile[4] = {8, 4, 128, 64};
	byte page = 4;
	byte mask;
	if (column < 8)
		mask = 0x80 >> column;
	else if (column < 16)
	{
		page = 6;
		mask = 0x80 >> (column - 8);
	}
	else if (column < 20)
	{
		result->first = screen->players + 0x300 + HCM_TOP + line;
		result->second = result->first;
		result->first_mask = first_missile[column - 16];
		result->second_mask = second_missile[column - 16];
		return;
	}
	else
	{
		page = 1;
		mask = 0x80 >> (column - 20);
	}
	result->first = screen->players + ((word)page << 8) + HCM_TOP + line;
	result->second = result->first + 256;
	result->first_mask = mask;
	result->second_mask = mask;
}

bool hcm_span(const AHcm * screen, byte column, byte line, byte height, byte color)
{
	if (!screen || !screen->players || column >= HCM_COLUMNS || line >= HCM_SCANLINES ||
		height > HCM_SCANLINES - line || color > 3)
		return false;
	HcmCell bits;
	cell(&bits, screen, column, line);
	byte first = color & 2 ? bits.first_mask : 0;
	byte second = color & 1 ? bits.second_mask : 0;
	for (byte i = 0; i < height; i++)
	{
		bits.first[i] = (bits.first[i] & ~bits.first_mask) | first;
		bits.second[i] = (bits.second[i] & ~bits.second_mask) | second;
	}
	return true;
}

bool hcm_color(const AHcm * screen, byte column, byte row, byte color)
{
	return row < HCM_ROWS && hcm_span(screen, column, row << 3, 8, color);
}

static __forceinline byte read_index(const AHcm * screen, byte column, byte line)
{
	if (!screen || !screen->players || column >= HCM_COLUMNS || line >= HCM_SCANLINES)
		return 0;
	HcmCell bits;
	cell(&bits, screen, column, line);
	// Keep the first result out of the scratch used for the second pointer.
	volatile byte color = bits.first[0] & bits.first_mask ? 2 : 0;
	return color | (bits.second[0] & bits.second_mask ? 1 : 0);
}

byte hcm_read_scanline(const AHcm * screen, byte column, byte line)
{
	return read_index(screen, column, line);
}

byte hcm_read(const AHcm * screen, byte column, byte row)
{
	return row < HCM_ROWS ? read_index(screen, column, row << 3) : 0;
}

void hcm_colors(byte first, byte second)
{
	os_pcolr[0] = first;
	os_pcolr[2] = first;
	os_pcolr[1] = second;
	os_pcolr[3] = second;
}

void hcm_off(void)
{
	os_wait_frame();
	dli_off();
	pmg_off();
}
