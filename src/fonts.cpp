#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fonts.h"
#include "game.h"
#include "types.h"

static uint8 bitfonts[] = {0,0,0,0,0,0,0,0,4,12,8,24,16,0,32,0,10,18,20,0,0,0,0,0,0,20,126,40,252,80,
0,0,6,25,124,32,248,34,28,0,4,12,72,24,18,48,32,0,14,18,20,8,21,34,29,0,32,32,64,0,0,0,
0,0,16,32,96,64,64,64,32,0,4,2,2,2,6,4,8,0,8,42,28,127,28,42,8,0,0,4,12,62,24,16,
0,0,0,0,0,0,0,0,32,64,0,0,0,60,0,0,0,0,0,0,0,0,0,0,32,0,4,12,8,24,16,48,
32,0,14,17,35,77,113,66,60,0,12,28,12,8,24,16,16,0,30,50,4,24,48,96,124,0,28,50,6,4,2,98,
60,0,2,18,36,100,126,8,8,0,15,16,24,4,2,50,28,0,14,17,32,76,66,98,60,0,126,6,12,24,16,48,
32,0,56,36,24,100,66,98,60,0,14,17,17,9,2,34,28,0,0,0,16,0,0,16,0,0,0,0,16,0,16,32,
0,0,0,0,0,0,0,0,0,0,0,0,30,0,60,0,0,0,0,0,0,0,0,0,0,0,28,50,6,12,8,0,
16,0,0,0,0,0,0,0,0,0,14,27,51,63,99,65,65,0,28,18,57,38,65,65,62,0,14,25,32,96,64,98,
60,0,12,18,49,33,65,66,60,0,30,32,32,120,64,64,60,0,31,48,32,60,96,64,64,0,14,25,32,96,68,98,
60,0,17,17,50,46,100,68,68,0,8,8,24,16,48,32,32,0,2,2,2,6,68,68,56,0,16,17,54,60,120,76,
66,0,16,48,32,96,64,64,60,0,10,21,49,33,99,66,66,0,17,41,37,101,67,66,66,0,28,50,33,97,67,66,
60,0,28,50,34,36,120,64,64,0,28,50,33,97,77,66,61,0,28,50,34,36,124,70,66,0,14,25,16,12,2,70,
60,0,126,24,16,16,48,32,32,0,17,49,35,98,70,68,56,0,66,102,36,44,40,56,48,0,33,97,67,66,86,84,
40,0,67,36,24,28,36,66,66,0,34,18,22,12,12,8,24,0,31,2,4,4,8,24,62,0};

#define NUM_FONTS 59

#define FONT_W 8
#define FONT_H 8
#define FONT_SIZE (FONT_W * FONT_H)


static uint8 fonts[NUM_FONTS * FONT_SIZE];

static void drawFont(int xp, int yp, int ch, uint8 colOffset, uint8 *vram)
{
	if (xp <0 || xp > SCR_W - FONT_W) return;

	uint8 *src = &fonts[ch * 64];
    uint8 *dst = vram + xp + yp * SCR_W;
    for (int y=0; y<FONT_H; y++) {
		for (int x=0; x<FONT_W; x++) {
			uint8 c = src[x];
			if (c!=0) dst[x] = c + colOffset;
		}
		src+=FONT_W;
        dst+=SCR_W;
    }
}

static void drawFontScaleX2(int xp, int yp, int ch, uint8 colOffset, uint8 *vram)
{
    if (xp <0 || xp > SCR_W - 2 * FONT_W) return;

	uint8 *src = &fonts[ch * 64];
	uint16 *dst16 = (uint16*)(vram + (xp & ~1) + yp * SCR_W);
    for (int y=0; y<FONT_H; y++) {
		for (int x=0; x<FONT_W; x++) {
			uint8 c = *src++;
			if (c!=0) {
				c += colOffset;
				uint16 c16 = (c << 8) | c;
				dst16[x] = c16;
				dst16[x+SCR_W/2] = c16;
			}
		}
        dst16+=SCR_W;
    }
}

static void drawFontScaleX4(int xp, int yp, int ch, uint8 colOffset, uint8 *vram)
{
    if (xp <0 || xp > SCR_W - 4 * FONT_W) return;

	uint8 *src = &fonts[ch * 64];
	uint32 *dst32 = (uint32*)(vram + (xp & ~3) + yp * SCR_W);
    for (int y=0; y<FONT_H; y++) {
		for (int x=0; x<FONT_W; x++) {
			uint8 c = *src++;
			if (c!=0) {
				c += colOffset;
				uint32 c32 = (c << 24) | (c << 16) | (c << 8) | c;
				dst32[x] = c32;
				dst32[x+SCR_W/4] = c32;
				dst32[x+SCR_W/2] = c32;
				dst32[x+3*SCR_W/4] = c32;
			}
		}
        dst32+=SCR_W;
    }
}

void drawText(int xp, int yp, const char *text, uint8 colOffset, int scaleBits, uint8 *vram)
{	
	if (yp < 0 || yp > SCR_H - scaleBits * 8) return;

	while (char c = *text++) {
        if (c>96 && c<123) {
			c-=32;
		}

   		if (c>31 && c<92) {
			switch(scaleBits) {
				default:
				case 0:
					drawFont(xp, yp, c - 32, colOffset, vram);
				break;

				case 1:
					drawFontScaleX2(xp, yp, c - 32, colOffset, vram);
				break;

				case 2:
					drawFontScaleX4(xp, yp, c - 32, colOffset, vram);
				break;
			}
   		}

   		xp+=(FONT_W << scaleBits);
   		if (xp>SCR_W -(FONT_W << scaleBits) -1) break;
	}
}

void fontsInit()
{
    int i = 0;
	for (int n=0; n<NUM_FONTS; n++) {
		for (int y=0; y<FONT_H; y++) {
			int c = bitfonts[i++];
			for (int x=0; x<FONT_W; x++) {
				uint8 shade = y + FONT_H;
				fonts[(n << 6) + x + y * FONT_W] = ((c >>  (7 - x)) & 1) * shade;
			}
		}
	}
}
