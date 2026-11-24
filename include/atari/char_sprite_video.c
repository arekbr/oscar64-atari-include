#include "char_sprite_video.h"
#include "dlist.h"
#include "irq.h"
#include "gtia.h"
#include "os.h"

static volatile byte first_font, next_font;
static volatile word old_vbi;
#pragma align(old_vbi, 2)
static word old_list, old_screen;
static byte old_chbas, old_dma, old_priority;
static bool active;

__asm sprite_video_dli
{
	pha
	sta $d40a
	lda next_font
	sta $d409
	clc
	adc #4
	sta next_font
	pla
	rti
}

__asm sprite_video_vbi
{
	lda first_font
	sta $d409
	clc
	adc #4
	sta next_font
	jmp (old_vbi)
}

// Two banks hold pending and active row schedules. One byte selects a
// complete schedule so a VBI can never see partially staged data.
static volatile byte pmg_position[2][2 * CHAR_SPRITE_PMG_ROWS];
static volatile byte pmg_color[4][2 * CHAR_SPRITE_PMG_ROWS];
static volatile byte pmg_font[2 * CHAR_SPRITE_PMG_ROWS];
static volatile byte pmg_base[CHAR_SPRITE_PMG_ROWS + 1];
static volatile byte pmg_pending, pmg_current, pmg_next_row;
static byte pmg_columns, pmg_rows, pmg_mode, pmg_top, pmg_rows_per_font;
static bool pmg_active;
static byte old_colors[4];

__asm sprite_video_players
{
	lda pmg_position,x
	sta $d000
	sta $d001
	lda pmg_position + 56,x
	sta $d002
	sta $d003
	lda pmg_color,x
	sta $d012
	lda pmg_color + 56,x
	sta $d013
	lda pmg_color + 112,x
	sta $d014
	lda pmg_color + 168,x
	sta $d015
	rts
}

__asm sprite_video_pmg_dli
{
	pha
	txa
	pha
	ldx pmg_next_row
	lda pmg_font,x
	sta $d40a
	sta $d409
	jsr sprite_video_players
	inc pmg_next_row
	pla
	tax
	pla
	rti
}

__asm sprite_video_pmg_vbi
{
	txa
	pha
	ldx pmg_pending
	cpx #$ff
	beq current
	stx pmg_current
	lda #$ff
	sta pmg_pending
current:
	ldx pmg_current
	lda pmg_base,x
	sta $d407
	lda pmg_font,x
	sta $d409
	jsr sprite_video_players
	lda pmg_color,x
	sta $02c0
	lda pmg_color + 56,x
	sta $02c1
	lda pmg_color + 112,x
	sta $02c2
	lda pmg_color + 168,x
	sta $02c3
	inx
	stx pmg_next_row
	pla
	tax
	jmp (old_vbi)
}

static void set_immediate(word address)
{
	byte lo = address, hi = address >> 8;
	__asm
	{
		ldy lo
		ldx hi
		lda #6
		jsr $e45c
	}
}

static bool range(word address, word size)
{
	return address && size && address <= 0xffffu - (size - 1);
}

static bool overlap(word a, word n, word b, word m)
{
	return a <= b + (m - 1) && b <= a + (n - 1);
}

static word layout(const ACharSpriteVideo * video)
{
	if (!range((word)video, sizeof(ACharSpriteVideo)) ||
		!range((word)video->screen, sizeof(ACharSpriteScreen)))
		return 0;
	const ACharSpriteScreen * screen = video->screen;
	byte mode = video->mode, columns = screen->columns, rows = screen->rows;
	if ((mode != DL_MODE4 && mode != DL_MODE5) || !rows || !screen->rows_per_font ||
		(columns != 32 && columns != 40 && columns != 48) ||
		(word)rows * (mode == DL_MODE4 ? 8 : 16) + video->top > 224)
		return 0;
	word cells = (word)columns * rows;
	word fonts = ((word)rows + screen->rows_per_font - 1) / screen->rows_per_font * 1024;
	word addr = (word)screen->output, font = (word)screen->fonts;
	word fixed = (word)screen->first_slot * 8;
	if (screen->cells < cells || screen->font_size < fonts || (font & 1023) ||
		!range(addr, cells) || !range(font, fonts) || overlap(addr, cells, font, fonts) ||
		!range((word)screen->font, fixed) || !range((word)screen->background, cells))
		return 0;
	word length = ((word)video->top + 7) / 8 + rows + 3;
	for (byte row = 0, last_page = 0xff; row < rows; row++, addr += columns)
	{
		if ((addr & 4095) > 4096u - columns)
			return 0;
		byte page = addr >> 12;
		if (last_page != page)
		{
			length += 2;
			last_page = page;
		}
	}
	word start = (word)video->list;
	if (video->list_size < length || !range(start, length) || length > 1024u - (start & 1023u) ||
		overlap(start, length, (word)video, sizeof(ACharSpriteVideo)) ||
		overlap(start, length, (word)screen, sizeof(ACharSpriteScreen)) ||
		overlap(start, length, (word)screen->output, cells) ||
		overlap(start, length, font, fonts) ||
		overlap(start, length, (word)screen->font, fixed) ||
		overlap(start, length, (word)screen->background, cells))
		return 0;
	return length;
}

static void build(const ACharSpriteVideo * video, bool colored)
{
	const ACharSpriteScreen * screen = video->screen;
	volatile byte * out = dl_blank(video->list, video->top);
	word addr = (word)screen->output;
	byte band_row = 0, last_page = 0xff;
	for (byte row = 0; row < screen->rows; row++, addr += screen->columns)
	{
		byte flags = video->mode;
		if (++band_row == screen->rows_per_font)
		{
			band_row = 0;
			if (row + 1 < screen->rows)
				flags |= DL_DLI;
		}
		if (colored && row + 1 < screen->rows)
			flags |= DL_DLI;
		byte page = addr >> 12;
		if (last_page != page)
		{
			out = dl_lms(out, flags, (const volatile void *)addr);
			last_page = page;
		}
		else
			*out++ = flags;
	}
	dl_jvb(out, video->list);
}

struct Arena
{
	word address, descriptor;
};

static bool arena_layout(Arena * arena, const ACharSpritePmgFrame * frame)
{
	word desc = (word)frame;
	if (!range(desc, sizeof(ACharSpritePmgFrame)))
		return false;
	word address = (word)frame->players;
	if (!range(address, 2048) || (address & 2047) || overlap(address, 2048, desc, sizeof(ACharSpritePmgFrame)))
		return false;
	arena->address = address;
	arena->descriptor = desc;
	return true;
}

static bool arena_overlaps(const Arena * arena, word address, word size)
{
	return overlap(arena->address, 2048, address, size) ||
		overlap(arena->descriptor, sizeof(ACharSpritePmgFrame), address, size);
}

static bool arena_disjoint(const Arena * arena, const ACharSpriteVideo * video, word list_size)
{
	const ACharSpriteScreen * screen = video->screen;
	word cells = (word)screen->columns * screen->rows;
	word fonts = ((word)screen->rows + screen->rows_per_font - 1) / screen->rows_per_font * 1024;
	return !arena_overlaps(arena, (word)video, sizeof(ACharSpriteVideo)) &&
		!arena_overlaps(arena, (word)screen, sizeof(ACharSpriteScreen)) &&
		!arena_overlaps(arena, (word)video->list, list_size) &&
		!arena_overlaps(arena, (word)screen->background, cells) &&
		!arena_overlaps(arena, (word)screen->output, cells) &&
		!arena_overlaps(arena, (word)screen->font, (word)screen->first_slot * 8) &&
		!arena_overlaps(arena, (word)screen->fonts, fonts);
}

struct ActorBox
{
	byte x, y, width, height, source_x, source_y;
};

static bool actor_box(ActorBox * box, const ACharSpritePmgFrame * frame, const ACharSprite * sprite)
{
	int min_x = frame->columns == 48 ? 12 : 0;
	int max_x = frame->columns == 48 ? 190 : (word)frame->columns * 4;
	int max_y = (word)frame->rows * 8;
	int x = sprite->x, y = sprite->y;
	if (x >= max_x || x <= min_x - 8 || y >= max_y || y <= -(int)sprite->rows)
		return false;
	int x1 = x + sprite->columns * 4, y1 = y + sprite->rows;
	int left = x < min_x ? min_x : x;
	int top = y < 0 ? 0 : y;
	int right = x1 > max_x ? max_x : x1;
	int bottom = y1 > max_y ? max_y : y1;
	if (right <= left || bottom <= top)
		return false;
	box->x = left;
	box->y = top;
	box->width = right - left;
	box->height = bottom - top;
	box->source_x = left - x;
	box->source_y = top - y;
	return true;
}

static bool actor_layout(const Arena * arena, const ACharSprite * sprite)
{
	if (!sprite)
		return true;
	if (!range((word)sprite, sizeof(ACharSprite)) || arena_overlaps(arena, (word)sprite, sizeof(ACharSprite)))
		return false;
	if (!sprite->columns || !sprite->rows)
		return true;
	word used = (word)(sprite->rows - 1) * sprite->pitch + sprite->columns;
	return sprite->columns <= 2 && sprite->pitch >= sprite->columns && sprite->size >= used &&
		range((word)sprite->pixels, used) && !arena_overlaps(arena, (word)sprite->pixels, used);
}

static bool draw_actor(ACharSpritePmgFrame * frame, const ACharSpritePmgActor * actor)
{
	const ACharSprite * sprite = actor->sprite;
	ActorBox box;
	if (!sprite || !sprite->columns || !sprite->rows || !actor_box(&box, frame, sprite))
		return true;
	byte first = box.y >> 3;
	if (first)
		first--;
	byte last = (box.y + box.height - 1) >> 3;
	byte pair = 0;
	for (; pair < 2; pair++)
	{
		byte row = first;
		while (row <= last && !frame->position[pair][row])
			row++;
		if (row > last)
			break;
	}
	if (pair == 2)
		return false;
	byte position = 0x80 - 2 * frame->columns + box.x;
	for (byte row = first; row <= last; row++)
	{
		frame->position[pair][row] = position;
		frame->color[pair * 2][row] = actor->first;
		frame->color[pair * 2 + 1][row] = actor->second;
	}

	byte scale = frame->mode == DL_MODE5 ? 2 : 1;
	word start = 0x400u + ((word)pair << 9) + 8 + frame->top + (word)box.y * scale;
	volatile byte * plane = frame->players + start;
	const byte * pixels = sprite->pixels + (word)box.source_y * sprite->pitch;
	for (byte y = 0; y < box.height; y++, pixels += sprite->pitch)
	{
		byte first_bits = 0, second_bits = 0;
		for (byte x = 0; x < box.width; x++)
		{
			byte col = x + box.source_x;
			byte value = pixels[col >> 2] >> ((3 - (col & 3)) << 1);
			if (value & 1)
				first_bits |= 0x80 >> x;
			if (value & 2)
				second_bits |= 0x80 >> x;
		}
		for (byte line = 0; line < scale; line++, plane++)
		{
			plane[0] = first_bits;
			plane[256] = second_bits;
		}
	}
	return true;
}

static byte render_pmg(ACharSpritePmgFrame * frame, const ACharSpriteVideo * video,
	const ACharSpritePmgActor * actors, byte count, byte * next)
{
	Arena arena;
	word size = layout(video);
	if (!size || !arena_layout(&arena, frame) || !arena_disjoint(&arena, video, size))
		return CHAR_SPRITE_BAD;
	if (pmg_active && pmg_base[pmg_current] == arena.address >> 8)
		return CHAR_SPRITE_BAD;
	if (count)
	{
		word used = (word)count * sizeof(ACharSpritePmgActor);
		if (!range((word)actors, used) || arena_overlaps(&arena, (word)actors, used))
			return CHAR_SPRITE_BAD;
		for (byte i = 0; i < count; i++)
			if (!actor_layout(&arena, actors[i].sprite))
				return CHAR_SPRITE_BAD;
	}
	frame->columns = video->screen->columns;
	frame->rows = video->screen->rows;
	frame->mode = video->mode;
	frame->top = video->top;
	for (word i = 0x400; i < 0x800; i++)
		frame->players[i] = 0;
	for (byte row = 0; row < frame->rows; row++)
	{
		frame->position[0][row] = frame->position[1][row] = 0;
		for (byte i = 0; i < 4; i++)
			frame->color[i][row] = 0;
	}
	byte result = CHAR_SPRITE_OK, start = 0, skipped = 0, index = 0;
	if (next && count > 1)
		start = *next && *next < count ? *next : count - 1;
	for (byte i = 0; i < count; i++)
	{
		if (!draw_actor(frame, actors + index))
		{
			if (result == CHAR_SPRITE_OK)
				skipped = index;
			result = CHAR_SPRITE_FULL;
		}
		if (!start)
			index++;
		else if (!i)
			index = start;
		else
			index = index == 1 ? count - 1 : index - 1;
	}
	if (next)
		*next = result == CHAR_SPRITE_FULL ? skipped : start;
	return result;
}

byte char_sprite_pmg_render(ACharSpritePmgFrame * frame, const ACharSpriteVideo * video,
	const ACharSpritePmgActor * actors, byte count)
{
	return render_pmg(frame, video, actors, count, 0);
}

byte char_sprite_pmg_render_rotating(ACharSpritePmgFrame * frame, const ACharSpriteVideo * video,
	const ACharSpritePmgActor * actors, byte count, byte * next)
{
	return next ? render_pmg(frame, video, actors, count, next) : CHAR_SPRITE_BAD;
}

static bool same_geometry(const ACharSpritePmgFrame * frame, byte columns, byte rows, byte mode, byte top)
{
	return frame->columns == columns && frame->rows == rows && frame->mode == mode && frame->top == top;
}

static void stage(const ACharSpritePmgFrame * frame, byte slot)
{
	pmg_base[slot] = (word)frame->players >> 8;
	byte font = first_font, band_row = 0;
	for (byte row = 0; row < frame->rows; row++)
	{
		byte i = slot + row;
		pmg_font[i] = font;
		for (byte player = 0; player < 4; player++)
			pmg_color[player][i] = frame->color[player][row];
		pmg_position[0][i] = frame->position[0][row];
		pmg_position[1][i] = frame->position[1][row];
		if (++band_row == pmg_rows_per_font)
		{
			font += 4;
			band_row = 0;
		}
	}
}

static __noinline bool show(const ACharSpriteVideo * video, const ACharSpritePmgFrame * request)
{
	const ACharSpritePmgFrame * volatile frame = request;
	word size = layout(video);
	if (!size || pmg_active)
		return false;
	const ACharSpriteScreen * screen = video->screen;
	if (frame)
	{
		Arena arena;
		if ((os_sdmctl & (DMACTL_PLAYERS | DMACTL_MISSILES)) || !arena_layout(&arena, frame) ||
			!arena_disjoint(&arena, video, size) || !same_geometry(frame, screen->columns, screen->rows, video->mode, video->top))
			return false;
	}
	os_wait_frame();
	dli_off();
	if (!active)
	{
		old_vbi = (word)os_vvblki;
		old_list = os_sdlst;
		old_screen = os_savmsc;
		old_chbas = os_chbas;
		old_dma = os_sdmctl;
		old_priority = os_gprior;
	}
	os_sdmctl &= (byte)~DMACTL_DLIST;
	os_wait_frame();
	build(video, frame != nullptr);
	first_font = (word)screen->fonts >> 8;
	next_font = first_font + 4;
	os_chbas = first_font;
	os_sdlst = (word)video->list;
	os_savmsc = (word)screen->output;
	os_gprior &= 0x3f;
	os_sdmctl = (old_dma & ~3) | DMACTL_DLIST | ((screen->columns >> 3) - 3);
	if (frame)
	{
		pmg_columns = screen->columns;
		pmg_rows = screen->rows;
		pmg_mode = video->mode;
		pmg_top = video->top;
		pmg_rows_per_font = screen->rows_per_font;
		stage(frame, 0);
		pmg_current = 0;
		pmg_pending = 0xff;
		for (byte i = 0; i < 4; i++)
		{
			old_colors[i] = os_pcolr[i];
			gtia.sizep[i] = 0;
		}
		gtia.vdelay = 0;
		os_gprior = PRIOR_MULTICOLOR | PRIOR_P0123_PF0123;
		gtia.gractl = GRACTL_PLAYERS;
		os_sdmctl |= DMACTL_PM_1LINE | DMACTL_PLAYERS;
		set_immediate((word)sprite_video_pmg_vbi);
	}
	else
		set_immediate((word)sprite_video_vbi);
	os_wait_frame();
	dli_set(frame ? (IRQHandler)sprite_video_pmg_dli : (IRQHandler)sprite_video_dli);
	pmg_active = frame != nullptr;
	active = true;
	return true;
}

bool char_sprite_video_show(const ACharSpriteVideo * video)
{
	return show(video, nullptr);
}

bool char_sprite_video_show_pmg(const ACharSpriteVideo * video, const ACharSpritePmgFrame * frame)
{
	return frame && show(video, frame);
}

bool char_sprite_video_update_pmg(const ACharSpritePmgFrame * frame)
{
	Arena arena;
	if (!pmg_active || !arena_layout(&arena, frame) ||
		!same_geometry(frame, pmg_columns, pmg_rows, pmg_mode, pmg_top) ||
		pmg_base[pmg_current] == arena.address >> 8)
		return false;
	byte slot = CHAR_SPRITE_PMG_ROWS - pmg_current;
	stage(frame, slot);
	pmg_pending = slot;
	while (pmg_pending != 0xff)
		os_wait_frame();
	return true;
}

void char_sprite_video_off(void)
{
	if (!active)
		return;
	os_wait_frame();
	dli_off();
	set_immediate(old_vbi);
	if (pmg_active)
	{
		gtia.gractl = 0;
		for (byte i = 0; i < 4; i++)
		{
			gtia.hposp[i] = 0;
			gtia.grafp[i] = 0;
			gtia.colpm[i] = old_colors[i];
			os_pcolr[i] = old_colors[i];
		}
		pmg_active = false;
	}
	os_sdlst = old_list;
	os_savmsc = old_screen;
	os_chbas = old_chbas;
	os_gprior = old_priority;
	os_sdmctl = old_dma;
	active = false;
}
