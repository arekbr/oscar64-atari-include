#include "tgi_driver.h"
#include "cc65_module.h"
#include "cio.h"
#include "os.h"

#define TGI_VECTORS 19
#define TGI_HEADER_SIZE (17 + 2 * TGI_VECTORS)
#define OS_APPMHI (*(volatile word *)0x000e)

static const Cc65Header format = {TGI_HEADER_SIZE, 17, TGI_VECTORS};
static const ATgiDriver * active;
static word old_appmhi;
static bool graphics;
static volatile word target;
#pragma align(target, 2)
static volatile byte reg_a, reg_x, reg_y;

bool atgi_load(ATgiDriver * driver, const byte * file, word size, byte * image, word capacity)
{
	Cc65Module module;
	if (!driver || active || !cc65_module_parse(&module, file, size, &format))
		return false;
	const byte * raw = module.text;
	word start = (word)image, from = (word)file;
	if (start > 0xffffu - (module.image_size - 1) || from > 0xffffu - (size - 1) ||
		(start <= from + (size - 1) && from <= start + (module.image_size - 1)) ||
		raw[0] != 't' || raw[1] != 'g' || raw[2] != 'i' || raw[3] != 6 ||
		raw[11] < 1 || raw[11] > 2 || cc65_word(raw + 6) > 320 || cc65_word(raw + 8) > 240 ||
		!cc65_word(raw + 6) || !cc65_word(raw + 8) ||
		!cc65_module_relocate(&module, &format, image, capacity))
		return false;
	image[4] = 0x56;
	image[5] = 0xe4;
	ATgiDriver result = {image, module.code_size, module.image_size,
		cc65_word(image + 6), cc65_word(image + 8), image[10], image[11]};
	*driver = result;
	return true;
}

#pragma optimize(push, 2, noinline, noconstparams)
bool atgi_load_file(ATgiDriver * driver, const char * path, const ATgiStorage * memory)
{
	if (!driver || active || !memory)
		return false;
	word size = cc65_module_read(path, memory->file, memory->file_capacity);
	return size && atgi_load(driver, memory->file, size, memory->image, memory->image_capacity);
}
#pragma optimize(pop)

__asm tgi_jump
{
	jmp (target)
}

static void invoke(const ATgiDriver * driver, byte operation)
{
	target = cc65_word(driver->image + 17 + 2 * operation);
	__asm volatile {
		php
		cld
		lda reg_a
		ldx reg_x
		ldy reg_y
		jsr tgi_jump
		sta reg_a
		stx reg_x
		sty reg_y
		plp
	}
}

bool atgi_attach(const ATgiDriver * driver)
{
	if (active || !driver || !driver->image || driver->pages < 1 || driver->pages > 2)
		return false;
	active = driver;
	invoke(active, 0);
	return true;
}

static bool screen_fits(const ATgiDriver * driver, word program_end)
{
	word top = (word)os_ramtop << 8;
	word screen = (word)driver->pages * 8192u;
	word image = (word)driver->image;
	return cio_free() != 255 && program_end && top >= screen &&
		program_end <= top - screen && OS_APPMHI <= top - screen &&
		image < program_end && driver->image_size <= program_end - image;
}

byte atgi_begin(word program_end)
{
	if (!active)
		return 1;
	if (graphics)
		return 9;
	if (!screen_fits(active, program_end))
		return 8;
	old_appmhi = OS_APPMHI;
	if (old_appmhi < program_end)
		OS_APPMHI = program_end;
	invoke(active, 2);
	invoke(active, 4);
	byte status = reg_a;
	if (status)
		OS_APPMHI = old_appmhi;
	else
		graphics = true;
	return status;
}

void atgi_end(void)
{
	if (graphics)
	{
		invoke(active, 3);
		OS_APPMHI = old_appmhi;
		graphics = false;
	}
}

void atgi_detach(void)
{
	if (active)
	{
		atgi_end();
		invoke(active, 1);
		active = nullptr;
	}
}

bool atgi_open(const ATgiDriver * driver, word program_end)
{
	if (active || !driver || !driver->image || driver->pages < 1 || driver->pages > 2 ||
		!screen_fits(driver, program_end) || !atgi_attach(driver))
		return false;
	if (!atgi_begin(program_end))
		return true;
	atgi_detach();
	return false;
}

void atgi_close(void)
{
	atgi_detach();
}

byte atgi_call(ATgiRequest * request)
{
	if (!active || !request || request->operation < 5 || request->operation >= TGI_VECTORS)
		return 0xff;
	cc65_zp[8] = request->p1;
	cc65_zp[9] = request->p1 >> 8;
	cc65_zp[10] = request->p2;
	cc65_zp[11] = request->p2 >> 8;
	cc65_zp[12] = request->p3;
	cc65_zp[13] = request->p3 >> 8;
	cc65_zp[14] = request->p4;
	cc65_zp[15] = request->p4 >> 8;
	reg_a = request->a;
	reg_x = request->x;
	reg_y = request->y;
	invoke(active, request->operation);
	request->a = reg_a;
	request->x = reg_x;
	request->y = reg_y;
	invoke(active, 4);
	return reg_a;
}
