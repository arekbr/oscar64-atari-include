#ifndef OSCAR_TGI_H
#define OSCAR_TGI_H

#define TGI_ERR_OK          0
#define TGI_ERR_NO_DRIVER   1
#define TGI_ERR_CANNOT_LOAD 2
#define TGI_ERR_INV_DRIVER  3
#define TGI_ERR_INV_MODE    4
#define TGI_ERR_INV_ARG     5
#define TGI_ERR_INV_FUNC    6
#define TGI_ERR_INV_FONT    7
#define TGI_ERR_NO_RES      8
#define TGI_ERR_INSTALLED   9

#define TGI_FONT_BITMAP     0
#define TGI_FONT_VECTOR     1
#define TGI_TEXT_HORIZONTAL 0
#define TGI_TEXT_VERTICAL   1

// Atari hardware palette values. Use palette indices with tgi_setcolor().
#define TGI_COLOR_BLACK      0x00
#define TGI_COLOR_WHITE      0x0e
#define TGI_COLOR_RED        0x32
#define TGI_COLOR_CYAN       0x96
#define TGI_COLOR_PURPLE     0x68
#define TGI_COLOR_GREEN      0xc4
#define TGI_COLOR_BLUE       0x74
#define TGI_COLOR_YELLOW     0xee
#define TGI_COLOR_ORANGE     0x4a
#define TGI_COLOR_BROWN      0xe4
#define TGI_COLOR_LIGHTRED   0x3c
#define TGI_COLOR_GRAY1      0x04
#define TGI_COLOR_GRAY2      0x06
#define TGI_COLOR_LIGHTGREEN 0xcc
#define TGI_COLOR_LIGHTBLUE  0x7c
#define TGI_COLOR_GRAY3      0x0a

extern const char tgi_stddrv[];

// Oscar64 representation of a static driver: a read-only embedded o65 file.
// Define tgi_static_stddrv in the application to provide the default module.
struct ATgiModule
{
	const unsigned char * file;
	unsigned size;
};
extern const ATgiModule tgi_static_stddrv[];

typedef struct tgi_vectorfont tgi_vectorfont;

void tgi_load_driver(const char * name);
void tgi_install(const void * driver);
void tgi_unload(void);
void tgi_uninstall(void);
void tgi_init(void);
void tgi_done(void);

unsigned char tgi_geterror(void);
const char * tgi_geterrormsg(unsigned char code);
unsigned tgi_ioctl(unsigned char code, void * data);

void tgi_clear(void);
unsigned tgi_getpagecount(void);
void tgi_setviewpage(unsigned char page);
void tgi_setdrawpage(unsigned char page);
unsigned char tgi_getcolorcount(void);
unsigned char tgi_getmaxcolor(void);
void tgi_setcolor(unsigned char color);
unsigned char tgi_getcolor(void);
void tgi_setpalette(const unsigned char * palette);
const unsigned char * tgi_getpalette(void);
const unsigned char * tgi_getdefpalette(void);
unsigned tgi_getxres(void);
unsigned tgi_getmaxx(void);
unsigned tgi_getyres(void);
unsigned tgi_getmaxy(void);
unsigned tgi_getaspectratio(void);
void tgi_setaspectratio(unsigned aspectratio);

unsigned char tgi_getpixel(int x, int y);
void tgi_setpixel(int x, int y);
void tgi_gotoxy(int x, int y);
void tgi_line(int x1, int y1, int x2, int y2);
void tgi_lineto(int x2, int y2);
void tgi_bar(int x1, int y1, int x2, int y2);
void tgi_circle(int x, int y, unsigned char radius);
void tgi_ellipse(int x, int y, unsigned char rx, unsigned char ry);
void tgi_arc(int x, int y, unsigned char rx, unsigned char ry, unsigned sa, unsigned ea);
void tgi_pieslice(int x, int y, unsigned char rx, unsigned char ry, unsigned sa, unsigned ea);
int tgi_imulround(int rhs, int lhs);

const tgi_vectorfont * tgi_load_vectorfont(const char * name);
void tgi_install_vectorfont(const tgi_vectorfont * font);
void tgi_free_vectorfont(const tgi_vectorfont * font);
void tgi_settextdir(unsigned char dir);
void tgi_settextscale(unsigned width, unsigned height);
void tgi_settextstyle(unsigned width, unsigned height, unsigned char dir, unsigned char font);
unsigned tgi_gettextwidth(const char * s);
unsigned tgi_gettextheight(const char * s);
void tgi_outtext(const char * s);
void tgi_outtextxy(int x, int y, const char * s);

#pragma compile("atari/tgi.c")

#endif
