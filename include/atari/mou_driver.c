#include "mou_driver.h"
#include "cc65_module.h"
#include "os.h"
#include "pokey.h"
#include "ptimer.h"

#define AMOU_INSTALL   0
#define AMOU_UNINSTALL 1
#define AMOU_SETBOX    4
#define AMOU_GETBOX    5
#define AMOU_MOVE      6
#define AMOU_INFO      9
#define AMOU_IRQ      11
#define AMOU_CALLBACKS 31

static const Cc65Header format = {49, 6, 12};
static const AMouDriver * active;
static AMouSound restore_sound;
static volatile byte reg_a, reg_x;
static volatile word target;
#pragma align(target, 2)
static volatile byte move_x[2], exchange[8];

// The XL module passes its timer routine in A/X, with carry set for install.
// It is already in ordinary RAM, so the bridge leaves the OS ROM mapped.
static volatile word timer_target;
#pragma align(timer_target, 2)
__asm mou_timer_call
{
	jmp (timer_target)
}

__asm mou_timer_entry
{
	jsr mou_timer_call
	pla
	rti
}

__asm mou_timer_config
{
	// ptimer_detach restores the original vector after UNINSTALL.
	bcc done
	sta timer_target
	stx timer_target + 1
	lda #<mou_timer_entry
	sta $0212
	lda #>mou_timer_entry
	sta $0213
done:
	rts
}

__asm mou_cursor_noop
{
	rts
}

__asm mou_jump
{
	jmp (target)
}

static __noinline word invoke(byte vector, word ax)
{
	target = cc65_word(active->image + 6 + 2 * vector);
	reg_a = (byte)ax;
	reg_x = (byte)(ax >> 8);
	__asm volatile {
		php
		sei
		cld
		lda reg_a
		ldx reg_x
		jsr mou_jump
		sta reg_a
		stx reg_x
		plp
	}
	return reg_a | (word)reg_x << 8;
}

bool amou_load(AMouDriver * driver, const byte * file, word size, byte * image, word capacity)
{
	Cc65Module module;
	if (!driver || active || !cc65_module_parse(&module, file, size, &format))
		return false;
	const byte * raw = module.text;
	if (raw[0] != 'm' || raw[1] != 'o' || raw[2] != 'u' || raw[3] != 6 ||
		raw[4] || raw[5] || raw[30] != 0x80)
		return false;
	for (byte i = AMOU_CALLBACKS; i < format.size; i += 3)
		if (raw[i] != 0x4c || raw[i + 1] || raw[i + 2])
			return false;
	word start = (word)image, from = (word)file;
	if (start > 0xffffu - (module.image_size - 1) || from > 0xffffu - (size - 1) ||
		(start <= from + (size - 1) && from <= start + (module.image_size - 1)) ||
		!cc65_module_relocate(&module, &format, image, capacity))
		return false;
	AMouDriver result = {image, module.code_size, module.initialized_size, module.image_size};
	*driver = result;
	return true;
}

#pragma optimize(push, 2, noinline, noconstparams)
bool amou_load_file(AMouDriver * driver, const char * path, const AMouStorage * memory)
{
	if (!driver || active || !memory)
		return false;
	word size = cc65_module_read(path, memory->file, memory->file_capacity);
	return size && amou_load(driver, memory->file, size, memory->image, memory->image_capacity);
}
#pragma optimize(pop)

static __interrupt void unused_timer(void)
{
}

static void release_timer(void)
{
	ptimer_detach(PTIMER_2);
	pokey.audctl = restore_sound.audctl;
	pokey.aud[1].audf = restore_sound.audf2;
	pokey.aud[1].audc = restore_sound.audc2;
	active = nullptr;
}

bool amou_open(const AMouDriver * driver, const AMouSound * sound)
{
	if (!driver || !sound || active || !driver->image || driver->code_size < format.size ||
		driver->initialized_size < driver->code_size || driver->image_size < driver->initialized_size ||
		(word)driver->image < 0x0600u || (word)driver->image >= 0xc000u ||
		driver->image_size > 0xc000u - (word)driver->image ||
		!ptimer_claim(PTIMER_2, unused_timer))
		return false;

	restore_sound = *sound;
	active = driver;
	byte * image = driver->image;
	word callback = (word)mou_cursor_noop, config = (word)mou_timer_config;
	image[4] = (byte)config;
	image[5] = (byte)(config >> 8);
	for (byte i = AMOU_CALLBACKS; i < format.size; i += 3)
	{
		image[i + 1] = (byte)callback;
		image[i + 2] = (byte)(callback >> 8);
	}
	for (word i = driver->initialized_size; i < driver->image_size; ++i)
		image[i] = 0;
	if ((byte)invoke(AMOU_INSTALL, 0))
	{
		invoke(AMOU_UNINSTALL, 0);
		release_timer();
		return false;
	}
	return true;
}

void amou_close(void)
{
	if (active)
	{
		invoke(AMOU_UNINSTALL, 0);
		release_timer();
	}
}

bool amou_poll(void)
{
	if (!active || *(volatile byte *)0x0042)
		return false;
	invoke(AMOU_IRQ, 0);
	return true;
}

static __noinline word read_word(byte offset)
{
	byte low = exchange[offset];
	byte high = exchange[offset + 1];
	return low | (word)high << 8;
}

static void write_word(byte offset, word value)
{
	exchange[offset] = (byte)value;
	exchange[offset + 1] = (byte)(value >> 8);
}

bool amou_info(AMouInfo * info)
{
	if (!active || !info)
		return false;
	word address = (word)&exchange[0];
	cc65_zp[8] = (byte)address;
	cc65_zp[9] = (byte)(address >> 8);
	invoke(AMOU_INFO, address);
	info->x = read_word(0);
	info->y = read_word(2);
	info->buttons = exchange[4];
	return true;
}

bool amou_getbox(AMouBox * box)
{
	if (!active || !box)
		return false;
	invoke(AMOU_GETBOX, (word)&exchange[0]);
	box->left = read_word(0);
	box->top = read_word(2);
	box->right = read_word(4);
	box->bottom = read_word(6);
	return true;
}

bool amou_setbox(const AMouBox * box)
{
	if (!active || !box || box->left > box->right || box->top > box->bottom ||
		box->right > 319 || box->bottom > 191)
		return false;
	write_word(0, box->left);
	write_word(2, box->top);
	write_word(4, box->right);
	write_word(6, box->bottom);
	invoke(AMOU_SETBOX, (word)&exchange[0]);
	return true;
}

bool amou_move(int x, int y)
{
	AMouBox box;
	if (!amou_getbox(&box))
		return false;
	word clamped_x = x < (int)box.left ? box.left : x > (int)box.right ? box.right : x;
	word clamped_y = y < (int)box.top ? box.top : y > (int)box.bottom ? box.bottom : y;
	move_x[0] = (byte)clamped_x;
	move_x[1] = (byte)(clamped_x >> 8);
	word address = (word)&move_x[0];
	cc65_zp[0] = (byte)address;
	cc65_zp[1] = (byte)(address >> 8);
	invoke(AMOU_MOVE, clamped_y);
	return true;
}
