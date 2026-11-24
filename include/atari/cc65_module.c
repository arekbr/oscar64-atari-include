#include "cc65_module.h"
#include "cio.h"

__zeropage volatile byte cc65_zp[CC65_ZP_SIZE];

__noinline word cc65_word(const byte * data)
{
	// Keep both byte loads behind one call. Inlining can misaddress the
	// upper byte when the result is compared with a word constant.
	return data[0] | (word)data[1] << 8;
}

static bool options(Cc65Stream * stream)
{
	bool system = false;
	while (stream->left)
	{
		byte length = stream->next[0];
		if (!length)
		{
			stream->next++;
			stream->left--;
			return system;
		}
		if (length < 2 || length > stream->left)
			return false;
		if (stream->next[1] == 1)
		{
			if (length != 6 || stream->next[2] != 3 || stream->next[3] != 0)
				return false;
			system = true;
		}
		stream->next += length;
		stream->left -= length;
	}
	return false;
}

static bool relocate(Cc65Stream * input, byte * segment, word size, word image, const Cc65Header * header)
{
	// The delta starts one byte before the segment. A 255 entry advances 254.
	word offset = 0;
	byte vectors = 0;
	while (input->left)
	{
		byte step = *input->next++;
		input->left--;
		if (!step)
			return !header || vectors == header->vector_count;
		word delta = step == 255 ? 254 : step;
		if (offset > size || delta > size - offset)
			return false;
		offset += delta;
		if (step == 255)
			continue;
		if (!input->left)
			return false;
		byte code = *input->next++;
		input->left--;
		byte type = code & 0xe0, id = code & 7;
		if (id < 2 || id > 5 || (code & 0x18))
			return false;
		if (header && offset <= header->size)
		{
			if (vectors >= header->vector_count || code != 0x82 ||
				offset != header->vector_offset + 1 + 2 * vectors)
				return false;
			vectors++;
		}
		word base = id == 5 ? (word)&cc65_zp[0] : image;
		switch (type)
		{
		case 0x20:
			if (segment)
				segment[offset - 1] += (byte)base;
			break;
		case 0x40:
			if (!input->left)
				return false;
			base += *input->next++;
			input->left--;
			if (segment)
				segment[offset - 1] += base >> 8;
			break;
		case 0x80:
			if (offset == size)
				return false;
			if (segment)
			{
				byte * dest = segment + (offset - 1);
				base += cc65_word(dest);
				dest[0] = base;
				dest[1] = base >> 8;
			}
			break;
		default:
			return false;
		}
	}
	return false;
}

bool cc65_module_parse(Cc65Module * module, const byte * file, word size, const Cc65Header * header)
{
	if (!file || size < 26 || (word)file > 0xffffu - (size - 1) || file[0] != 1 || file[1] ||
		file[2] != 'o' || file[3] != '6' || file[4] != '5' || file[5] ||
		cc65_word(file + 6) != 0x0800 || cc65_word(file + 8) ||
		cc65_word(file + 20) || cc65_word(file + 22) != CC65_ZP_SIZE ||
		cc65_word(file + 24))
		return false;
	word code = cc65_word(file + 10);
	word data = cc65_word(file + 14);
	word bss = cc65_word(file + 18);
	if (code < header->size || data > 0xffffu - code ||
		cc65_word(file + 12) != code || cc65_word(file + 16) != code + data)
		return false;
	word initialized = code + data;
	if (bss > 0xffffu - initialized)
		return false;

	Cc65Stream stream = {file + 26, size - 26};
	if (!options(&stream) || initialized > stream.left || stream.left - initialized < 2)
		return false;
	const byte * raw = stream.next;
	stream.next += initialized;
	stream.left -= initialized;
	if (cc65_word(stream.next))
		return false;
	stream.next += 2;
	stream.left -= 2;

	for (byte i = 0; i < header->vector_count; ++i)
	{
		word address = cc65_word(raw + header->vector_offset + 2 * i);
		if (address < header->size || address >= code)
			return false;
	}
	module->text = raw;
	module->code_size = code;
	module->initialized_size = initialized;
	module->image_size = initialized + bss;
	module->fixes = stream;
	return true;
}

bool cc65_module_relocate(const Cc65Module * module, const Cc65Header * header,
	byte * image, word capacity)
{
	word used = module->image_size, code = module->code_size;
	word initialized = module->initialized_size;
	if (!image || used > capacity || (word)image > 0xffffu - (used - 1))
		return false;
	Cc65Stream stream = module->fixes, checked = stream;
	word base = (word)image;
	if (!relocate(&checked, nullptr, code, base, header) ||
		!relocate(&checked, nullptr, initialized - code, base, nullptr) ||
		checked.left != 2 || cc65_word(checked.next))
		return false;
	for (word i = 0; i < initialized; ++i)
		image[i] = module->text[i];
	for (word i = initialized; i < used; ++i)
		image[i] = 0;
	relocate(&stream, image, code, base, header);
	relocate(&stream, image + code, initialized - code, base, nullptr);
	return true;
}

// Keep the buffer arguments in RAM across the CIO call sequence. Older
// Oscar64 can copy overlapping caller/callee argument registers backwards.
static byte * volatile read_buffer;
static volatile word read_capacity;

// Keep buffer metadata independent of constant caller descriptors.
#pragma optimize(push, 2, noinline, noconstparams)
word cc65_module_read(const char * path, byte * file, word capacity)
{
	if (!path || !file || !capacity)
		return 0;
	read_buffer = file;
	read_capacity = capacity;
	byte ch = cio_free();
	if (ch == 255 || cio_open(ch, path, CIO_READ, 0) != CIO_OK)
		return 0;
	word done;
	byte status = cio_read(ch, read_buffer, read_capacity, &done);
	if ((status == CIO_OK || status == CIO_EOF_NEXT) && done == read_capacity)
	{
		byte extra;
		status = cio_getc(ch, &extra);
	}
	byte close = cio_close(ch);
	if (status != CIO_EOF || close != CIO_OK)
		return 0;
	return done;
}
#pragma optimize(pop)
