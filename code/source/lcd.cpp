#include "lcd.h"
#include "cpu.h"
#include "interrupt.h"
#include "sdl.h"
#include "mem.h"

#include <Hall/Hall.h>
#include "Halib/Graphic.h"

#include <assert.h>
#include <string.h>
#include <cstdlib>

static int leftover;
static int lcd_cycles;
static int lcd_line, prev_line;
static int lcd_ly_compare;

/* LCD STAT */
static int ly_int;	/* LYC = LY coincidence interrupt enable */
static int oam_int;
static int vblank_int;
static int hblank_int;
static int lcd_mode;

static int irq_level, new_level;

/* LCD Control */
static int lcd_enabled;
static int window_tilemap_select;
static int window_enabled;
static int tilemap_select;
static int bg_tiledata_select;
static int sprite_size;
static int sprites_enabled;
static int bg_enabled;
static int scroll_x, scroll_y;
static int window_x, window_y;

static int bgpalette[] = {0, 3, 3, 3};
static int sprpalette1[] = {0, 1, 2, 3};
static int sprpalette2[] = {0, 1, 2, 3};
static unsigned long colours[4] = {
    0b11101'11111'11101'1,
    0b10111'11001'10111'1,
    0b01111'10011'01111'1,
    0b00000'00010'00000'1
};

struct sprite {
	int y, x, tile, flags;
};

enum {
	PRIO  = 0x80,
	VFLIP = 0x40,
	HFLIP = 0x20,
	PNUM  = 0x10
};

enum LCD_INT {
	LCD_HBLANK = 1,
	LCD_VBLANK = 2,
	LCD_OAM    = 4,
	LCD_LY     = 8
};

void lcd_write_bg_palette(unsigned char n)
{
	bgpalette[0] = (n>>0)&3;
	bgpalette[1] = (n>>2)&3;
	bgpalette[2] = (n>>4)&3;
	bgpalette[3] = (n>>6)&3;
}

void lcd_write_spr_palette1(unsigned char n)
{
	sprpalette1[0] = 0;
	sprpalette1[1] = (n>>2)&3;
	sprpalette1[2] = (n>>4)&3;
	sprpalette1[3] = (n>>6)&3;
}

void lcd_write_spr_palette2(unsigned char n)
{
	sprpalette2[0] = 0;
	sprpalette2[1] = (n>>2)&3;
	sprpalette2[2] = (n>>4)&3;
	sprpalette2[3] = (n>>6)&3;
}

void lcd_write_scroll_x(unsigned char n)
{
	scroll_x = n;
}

void lcd_write_scroll_y(unsigned char n)
{
	scroll_y = n;
}

int lcd_get_line(void)
{
#ifdef DEBUG
	return 0x90;
#else
	if(lcd_line == 153 && (leftover % 456) >= 4)
		return 0;
	else
		return lcd_line;
#endif
}

unsigned char lcd_get_stat(void)
{
	unsigned char coincidence = (lcd_line == lcd_ly_compare) << 2;
	return 0x80 | ly_int | oam_int | vblank_int | hblank_int | coincidence | lcd_mode;
}

void lcd_write_stat(unsigned char c)
{
	ly_int     = c&0x40;
	oam_int    = c&0x20;
	vblank_int = c&0x10;
	hblank_int = c&0x08;
}

static unsigned short *b;

void lcd_write_control(unsigned char c)
{
	/* LCD just got turned on */
	if(!lcd_enabled && (c & 0x80))
		lcd_cycles = 0;

	bg_enabled            = !!(c & 0x01);
	sprites_enabled       = !!(c & 0x02);
	sprite_size           = !!(c & 0x04);
	tilemap_select        = !!(c & 0x08);
	bg_tiledata_select    = !!(c & 0x10);
	window_enabled        = !!(c & 0x20);
	window_tilemap_select = !!(c & 0x40);
	lcd_enabled           = !!(c & 0x80);
}

unsigned char lcd_get_ly_compare(void)
{
	return lcd_ly_compare;
}

void lcd_set_ly_compare(unsigned char c)
{
	lcd_ly_compare = c;
}

void lcd_set_window_y(unsigned char n) {
	window_y = n;
}

void lcd_set_window_x(unsigned char n) {
	window_x = n;
}

static void POKE(unsigned int x, int y, int c)
{
	b[y*160 + x] = c;
}

static void swap(struct sprite *a, struct sprite *b)
{
	struct sprite c;

	 c = *a;
	*a = *b;
	*b =  c;
}

static void sort_sprites(struct sprite *s, int n)
{
	int swapped, i;

	do
	{
		swapped = 0;
		for(i = 0; i < n-1; i++)
		{
			if(s[i].x > s[i+1].x)
			{
				swap(&s[i], &s[i+1]);
				swapped = 1;
			}
		}
	}
	while(swapped);
}

static int sprites_update(int line, struct sprite *spr)
{
	int i, c = 0;

	for(i = 0; i < 40; i++)
	{
		int y;

		y = mem_get_raw(0xFE00 + (i*4) + 0) - 16;

		if(line < y || line >= y + 8 + (sprite_size*8))
			continue;

		spr[c].y     = y;
		spr[c].x     = mem_get_raw(0xFE00 + (i*4) + 1) - 8;
		spr[c].tile  = mem_get_raw(0xFE00 + (i*4) + 2);
		spr[c].flags = mem_get_raw(0xFE00 + (i*4) + 3);
		c++;

		if(c == 10)
			break;
	}

	if(c)
		sort_sprites(spr, c);

	return c;
}

struct oam_cache
{
	char colour;
	char prio;
	char pal;
};

static void sprite_fetch(int line, struct oam_cache *o)
{
	struct sprite spr[10];
	int i, x, sprite_count;

	/* Fetch up to 10 in-range sprites for this scanline */
	sprite_count = sprites_update(line, spr);

	memset(o, 0, sizeof (struct oam_cache[160]));

	/* Copy sprite pixels to oam_cache */
	for(i = 0; i < sprite_count; i++)
	{
		int sprite_line;
		unsigned short tile_addr;
		unsigned char b1, b2, mask;

		/* Sprite is too far right to ever render anything */
		if(spr[i].x >= 160)
			continue;

		if(spr[i].flags & VFLIP)
			sprite_line = (sprite_size ? 15 : 7) - (line - spr[i].y);
		else
			sprite_line = line - spr[i].y;

		if(sprite_size)
			tile_addr = 0x8000 + (spr[i].tile & 0xFE) * 16 + sprite_line * 2;
		else
			tile_addr = 0x8000 + spr[i].tile * 16 + sprite_line * 2;

		b1 = mem_get_raw(tile_addr);
		b2 = mem_get_raw(tile_addr + 1);

		for(x = spr[i].x; x < spr[i].x + 8; x++)
		{
			int relx, new_col;

			/* Sprite pixel is off the left hand side of the screen */
			if(x < 0)
				continue;

			/* This pixel is offscreen, we're done with this sprite */
			if(x >= 160)
				break;

			relx = x - spr[i].x;

			mask = spr[i].flags & HFLIP ? 128>>(7-relx) : 128>>relx;
			new_col = (!!(b2&mask))<<1 | !!(b1&mask);

			/* We've already drawn this pixel, don't overwrite */
			if(o[x].colour)
				continue;

			/* Pixel was blank, put our pixel's details in */
			o[x].colour = new_col;
			o[x].prio = spr[i].flags & PRIO;
			o[x].pal = spr[i].flags & PNUM;
		}
	}
}

/* Process scanline 'line', cycle 'cycle' within that line */
static void lcd_do_line(int line, int cycle)
{
	static struct oam_cache o[160];
	static int line_fill, fetch_delay, window_lines, window_used_line, window_used_frame;
	static unsigned char scx_low_latch;

	/*
	if(fetch_delay)
	{
		fetch_delay--;
		return;
	}*/

	if(line >= 144)
	{
		lcd_mode = 1;
		window_lines = 0;
		window_used_frame = 0;
		return;
	}

	if(lcd_mode != 2 /*&& cycle < 80*/)
	{
		lcd_mode = 2;
	}
	else
		if(lcd_mode == 2 /*&& cycle >= 80*/)
		{
			scx_low_latch = scroll_x & 7;
			sprite_fetch(line, o);
			lcd_mode = 3;
		}

		/*
	if(cycle < 93)
		return;
*/
	if(lcd_mode == 3)
	{
		struct oam_cache *oc;
		int colour = 0, bgcol;
		unsigned int map_select, map_offset, tile_num, tile_addr, xm, ym;
		unsigned char b1, b2, mask;

		if(line >= window_y && window_enabled && line - window_y < 144 && (window_x - 7) <= line_fill)
		{
			if(!window_used_frame && line == window_y)
				window_used_frame = 1;

			if(!window_used_frame)
				goto bg;
			xm = line_fill - (window_x-7);
			ym = window_lines;
			map_select = window_tilemap_select;
			window_used_line = 1;
		}
		else
		{
			bg:
			if(!bg_enabled)
			{
				bgcol = 0;
				goto skip_bg;
			}

			xm = (line_fill + (scroll_x & 0xF8) + scx_low_latch)%256;
			ym = (line + scroll_y)%256;
			map_select = tilemap_select;
		}

		map_offset = (ym/8)*32 + xm/8;

		tile_num = mem_get_raw(0x9800 + map_select*0x400 + map_offset);
		if(bg_tiledata_select)
			tile_addr = 0x8000 + tile_num*16;
		else
			tile_addr = 0x9000 + ((signed char)tile_num)*16;

		b1 = mem_get_raw(tile_addr+(ym%8)*2);
		b2 = mem_get_raw(tile_addr+(ym%8)*2+1);

		mask = 128>>(xm%8);

		bgcol = (!!(b2&mask)<<1) | !!(b1&mask);

skip_bg:
		oc = &o[line_fill];

		if(sprites_enabled && oc->colour && ((oc->prio && !bgcol) || (!oc->prio)))
		{
			int *pal = oc->pal ? sprpalette2 : sprpalette1;
			colour = colours[pal[(int)oc->colour]];
		}
		else
		{
			colour = colours[bgpalette[bgcol]];
		}

		POKE(line_fill, line, colour);

		if(line_fill++ == 159)
		{
			lcd_mode = 0;
			line_fill = 0;
			if(window_used_line)
				window_lines++;
			window_used_line = 0;
			scx_low_latch = 0;
		}
	}
}

static void lcd_interrupt(enum LCD_INT src)
{
	new_level |= src;

	if(!!irq_level != !!new_level)
		interrupt(INTR_LCDSTAT);
}
#include <cstdio>

int lcd_cycle(void)
{
	/* Amount of cycles left over since the last full frame */
	leftover = lcd_cycles % (456 * 154);

	/* Each scanline is 456 cycles */
	lcd_line = leftover / 456;

	if(lcd_enabled)
	{
		if(ly_int)
		{
			if(lcd_line == lcd_ly_compare || (lcd_line == 153 && lcd_ly_compare == 0 && (leftover % 456) >= 4))
				lcd_interrupt(LCD_LY);
		}
		if(oam_int && lcd_mode == 2)
			lcd_interrupt(LCD_OAM);
		if(vblank_int && lcd_line >= 144)
			lcd_interrupt(LCD_VBLANK);
		if(hblank_int && lcd_mode == 0)
			lcd_interrupt(LCD_HBLANK);

		lcd_do_line(lcd_line, leftover % 456);
	}

	if(lcd_line == 144 && prev_line == 143)
	{
        // Process Keys:
		if(sdl_update()) 
			return 0;

        // Draw Frame:
        Hall::SetImage((Hall::Color*)b, 160, 144);
        Hall::SetExcerpt(0, 0, 160, 144);
        Hall::SetScale(1, 1);
        Hall::SetFlip(false, false);
        Hall::SetColorTable(Hall::NONE);
        Hall::SetColorSource(Hall::MEMORY);
        Hall::SetShape(Hall::RECTANGLE);
        Hall::SetScreenPosition(0, 0);
        Hall::Draw();
        #ifdef DESKTOP
            Hall::UpdateRaylibTexture((Hall::Color*)b, 160, 144);
        #endif
		Halib::Show();

		if(lcd_enabled)
		{
			interrupt(INTR_VBLANK);
		}
	}

	prev_line = lcd_line;

	lcd_cycles++;

	irq_level = new_level;
	new_level =  0;

	return 1;
}

int lcd_init(void)
{
    b = (unsigned short*)malloc(160*144*sizeof(Hall::Color));

    #ifdef DEBUG
        lcd_write_control(91);
    #endif

	return 1;
}
