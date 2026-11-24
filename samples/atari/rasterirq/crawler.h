#ifndef ATARI_SAMPLE_CRAWLER_H
#define ATARI_SAMPLE_CRAWLER_H

#include <atari/dlist.h>
#include <atari/input.h>

// The scrolled ANTIC row fetches more than the 40 visible columns.
static volatile byte row[48];
#pragma align(row, 64)
static byte list[33];
#pragma align(list, 1024)
static const char message[] =
	"LABORUM RERUM QUO. QUASI IN, SEQUI, TENETUR VOLUPTATEM RERUM "
	"PORRO NON ET MAIORES ALIAS ODIO EST EOS. MAGNAM APERIAM CUM ET "
	"ESSE TEMPORE ITAQUE TEMPORA VOLUPTAS ET IPSAM IPSAM EARUM. ID "
	"SUSCIPIT QUIA RERUM REPREHENDERIT ERROR ET UT. DOLOR ID "
	"CORPORIS, EOS? UNDE VERO ISTE QUIA? EAQUE EAQUE. IN. AUT ID "
	"EXPEDITA ILLUM MOLESTIAS, ";
static byte phase = 3, offset, half_clock;

static void fill_row(void)
{
	row[0] = row[1] = row[2] = row[3] = 0;
	for (byte i = 4; i < sizeof(row); ++i)
		row[i] = message[(word)offset + i - 4] - ' ';
}

static void advance_scroll(void)
{
	// The source advances by a high-resolution pixel.
	if ((++half_clock & 1) == 0)
		return;
	antic.hscrol = phase;
	if (phase)
		--phase;
	else
	{
		++offset;
		fill_row();
		phase = 3;
	}
}

static void show_crawler(void)
{
	volatile byte * p = dl_blank(list, 216);
	p = dl_lms(p, DL_MODE2 | DL_HSCROL, row);
	dl_jvb(p, list);
	os_color[1] = 0x0c;
	os_color[2] = 0x84;
	os_color[4] = 0x8c;
	os_sdmctl = DMACTL_DLIST | DMACTL_PF_NORMAL;
	fill_row();
	dl_set(list);
}

#endif
