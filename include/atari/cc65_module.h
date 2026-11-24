#ifndef ATARI_CC65_MODULE_H
#define ATARI_CC65_MODULE_H

#include "types.h"

#define CC65_ZP_SIZE 26

struct Cc65Stream
{
	const byte * next;
	word left;
};

struct Cc65Module
{
	const byte * text;
	word code_size, initialized_size, image_size;
	Cc65Stream fixes;
};

struct Cc65Header
{
	byte size, vector_offset, vector_count;
};

extern __zeropage volatile byte cc65_zp[CC65_ZP_SIZE];

word cc65_word(const byte * data);
bool cc65_module_parse(Cc65Module * module, const byte * file, word size, const Cc65Header * header);
bool cc65_module_relocate(const Cc65Module * module, const Cc65Header * header,
	byte * image, word capacity);
word cc65_module_read(const char * path, byte * file, word capacity);

#pragma compile("cc65_module.c")

#endif
