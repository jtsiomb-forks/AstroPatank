#include "render.h"

#include <math.h>
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "video.h"

#define ANTIALIAS_LEFT		1
#define ANTIALIAS_RIGHT		2
#define ANTIALIAS_CENTER	4


typedef struct Scanline
{
	int16 x0,x1;
	uint16 a0,a1;
} Scanline;


#define FILL_SCANLINES_ASM

extern "C" {
	void fillScanlinesAsm(uint8 color, uint8 *vram);
	Scanline scanline[SCR_H];
	int yScanlineMin, yScanlineMax;
}


static inline void fillScanlinesUnchained(uint8 color, uint8 *vram)
{
	uint8 *dstY = vram + VRAM_PIXEL_OFFSET(0,yScanlineMin);
	uint32 color32 = (color << 24) | (color << 16) | (color << 8) | color;

	Scanline *s = &scanline[yScanlineMin];
	int countY = yScanlineMax - yScanlineMin;
	while (countY-- > 0) {
		int16 x0 = s->x0;
		int16 x1 = s->x1;
		uint16 a0 = s->a0;
		uint16 a1 = s->a1;
		++s;

		if (x0 < 0) x0 = 0;
		if (x1 > SCR_W - 1) x1 = SCR_W - 1;

		int x0b = x0 >> 2;
		int x1b = x1 >> 2;

		if (x1b>x0b) {
			uint8 xle = x0 & 3;
			if (xle!=0) {
				setPlaneMask(~((16 >> (4-xle)) - 1));
				*(dstY + x0b) = color;
				x0b++;
			}
			if (x1b>=x0b) {
				uint8 xre = (x1-1) & 3;	// 1 pixel less on the right
				if (xre != 3) {
					setPlaneMask((16 >> (3-xre)) - 1);
					*(dstY + x1b) = color;
				}
			}
		} else {
			// mask
			// Need LUTs and more thinking
		}

		setPlaneMask(15);

		int length = x1b - x0b;
		uint8 *dst = dstY + x0b;
		if (length < 5) {			// More optimal (4 should be fine, but 5 is slightly tiny more but insignificant)
			while (length-- > 0) {
				*dst++ = color;
			};
		} else {
			int16 xl = x0b & 3;
			if (xl) {
				int16 l = 4-xl;
				length -= l;
				while (l-- != 0) {
					*dst++ = color;
				};
			}

			uint32 *dst32 = (uint32*)dst;
			while(length > 3) {
				*dst32++ = color32;
				length-=4;
			};

			dst = (uint8*)dst32;
			while(length-- > 0) {
				*dst++ = color;
			};
		}

		#ifdef ANTIALIASING_POLY
			if (a1 & ANTIALIAS_RIGHT) {
				if (length == -1) {
					*dst = color - (a1 >> 8);
				}
			}
		#endif

		dstY += SCR_LINE_BYTES;
	};
}

static inline void fillScanlines(uint8 color, uint8 *vram)
{
	uint8 *dstY = vram + VRAM_PIXEL_OFFSET(0,yScanlineMin);
	uint32 color32 = (color << 24) | (color << 16) | (color << 8) | color;

	Scanline *s = &scanline[yScanlineMin];
	int countY = yScanlineMax - yScanlineMin;
	while (countY-- > 0) {
		int16 x0 = s->x0;
		int16 x1 = s->x1;
		uint16 a0 = s->a0;
		uint16 a1 = s->a1;
		++s;

		if (x0 < 0) x0 = 0;
		if (x1 > SCR_W - 1) x1 = SCR_W - 1;

		#ifdef SCR_UNCHAINED
			x0 >>= 2;
			x1 >>= 2;
		#endif

		uint8 *dst = dstY + x0;
		int16 length = x1 - x0;

		#ifdef ANTIALIASING_POLY
			if (a1 & ANTIALIAS_RIGHT) {
				length--;
			}
			if (a0 & ANTIALIAS_LEFT) {
				if (length > 0) {
					*dst++ = color - (a0 >> 8);
					--length;
				}
			}
		#endif

		if (length < 5) {			// More optimal (4 should be fine, but 5 is slightly tiny more but insignificant)
			while (length-- > 0) {
				*dst++ = color;
			};
		} else {
			int16 xl = x0 & 3;
			if (xl) {
				int16 l = 4-xl;
				length -= l;
				while (l-- != 0) {
					*dst++ = color;
				};
			}

			uint32 *dst32 = (uint32*)dst;
			while(length > 3) {
				*dst32++ = color32;
				length-=4;
			};

			dst = (uint8*)dst32;
			while(length-- > 0) {
				*dst++ = color;
			};
		}

		#ifdef ANTIALIASING_POLY
			if (a1 & ANTIALIAS_RIGHT) {
				if (length == -1) {
					*dst = color - (a1 >> 8);
				}
			}
		#endif

		dstY += SCR_LINE_BYTES;
	};
}

static inline void prepareScanlines(ScreenPoint2D *p0, ScreenPoint2D *p1, uint16 antialiasBits, bool leftSide)
{
	int x0 = p0->x;
	int y0 = p0->y;
	int x1 = p1->x;
	int y1 = p1->y;

	const int dy = y1 - y0;
	const int dx = ((x1 - x0) << SCR_BITS) / dy;

	int xp = x0;

	int py0 = y0 >> SCR_BITS;
	int py1 = y1 >> SCR_BITS;

	if (py0 < 0) {
		xp += (-y0 * dx) >> SCR_BITS;
		y0 = 0;
		py0 = 0;
	}
	if (py1 > SCR_H-1) {
		py1 = SCR_H-1;
		y1 = (SCR_H << SCR_BITS) - 1;
	}

	if (py0 < yScanlineMin) yScanlineMin = py0;
	if (py1 > yScanlineMax) yScanlineMax = py1;

	int frac0 = y0 & SCR_AND;
	if (frac0) {
		frac0 = SCR_AND - frac0;
		xp += ((frac0 * dx) >> SCR_BITS);
	}

	Scanline *s = &scanline[py0];
	int countY = py1 - py0;
	if (leftSide) {
		while (countY-- > 0) {
			s->x0 = (int16)(xp >> SCR_BITS);
			#ifdef ANTIALIASING_POLY
				uint16 fracBits = ((uint16)(xp >> ((SCR_BITS + UNCHAINED_BITS) - SHADE_BITS)) & 15) << 8;
				s->a0 = antialiasBits | fracBits;
			#endif

			xp += dx;
			++s;
		};
	} else {
		while (countY-- > 0) {
			s->x1 = (int16)(xp >> SCR_BITS);
			#ifdef ANTIALIASING_POLY
				uint16 fracBits = (15 - (uint16)(xp >> ((SCR_BITS + UNCHAINED_BITS) - SHADE_BITS)) & 15) << 8;
				s->a1 = antialiasBits | fracBits;
			#endif

			xp += dx;
			++s;
		};
	}
}

static inline void prepareEdgeScanlinesFlatAntialiased(ScreenPoint2D* p0, ScreenPoint2D* p1, uint8 adjacentPolysNum)
{
	int py0 = p0->y >> SCR_BITS;
	int py1 = p1->y >> SCR_BITS;

	if (py0 == py1) return;

	if (py0 < py1) {
		uint16 antialiasBits = ANTIALIAS_LEFT;
		if (adjacentPolysNum==2) antialiasBits = ANTIALIAS_CENTER;
		prepareScanlines(p0, p1, antialiasBits, true);
	} else {
		uint16 antialiasBits = 0;
		if (adjacentPolysNum==1) antialiasBits = ANTIALIAS_RIGHT;
		prepareScanlines(p1, p0, antialiasBits, false);
	}
}

static inline void prepareEdgeScanlinesFlat(ScreenPoint2D* p0, ScreenPoint2D* p1)
{
	int py0 = p0->y >> SCR_BITS;
	int py1 = p1->y >> SCR_BITS;

	if (py0 == py1) return;

	if (py0 < py1) {
		prepareScanlines(p0, p1, 0, true);
	} else {
		prepareScanlines(p1, p0, 0, false);
	}
}

void drawPolyAntialiased(ScreenPoint *p[], uint8 *edgeAdjacentPolysNum, int edgesNum, uint8 color, uint8 *vram)
{
	yScanlineMin = SCR_H;
	yScanlineMax = -1;

	for (int i=0; i<edgesNum-1; ++i) {
		prepareEdgeScanlinesFlatAntialiased((ScreenPoint2D*)p[i], (ScreenPoint2D*)p[i+1], *edgeAdjacentPolysNum++);
	}
	prepareEdgeScanlinesFlatAntialiased((ScreenPoint2D*)p[edgesNum-1], (ScreenPoint2D*)p[0], *edgeAdjacentPolysNum);

	uint8 col = ((color&7)<<4)+15;

#ifdef FILL_SCANLINES_ASM
	fillScanlinesAsm(col, vram);
#else
	#ifdef SCR_UNCHAINED
		fillScanlinesUnchained(col, vram);
	#else
		fillScanlines(col, vram);
	#endif
#endif
}

void drawPoly(ScreenPoint *p[], int edgesNum, uint8 color, uint8 *vram)
{
	yScanlineMin = SCR_H;
	yScanlineMax = -1;

	for (int i=0; i<edgesNum-1; ++i) {
		prepareEdgeScanlinesFlat((ScreenPoint2D*)p[i], (ScreenPoint2D*)p[i+1]);
	}
	prepareEdgeScanlinesFlat((ScreenPoint2D*)p[edgesNum-1], (ScreenPoint2D*)p[0]);

	uint8 col = ((color&7)<<4)+15;

#ifdef FILL_SCANLINES_ASM
	fillScanlinesAsm(col, vram);
#else
	#ifdef SCR_UNCHAINED
		fillScanlinesUnchained(col, vram);
	#else
		fillScanlines(col, vram);
	#endif
#endif
}

void drawQuad(ScreenPoint2D *p[], uint8 color, uint8 *vram)
{
	//if ((p[0]->x - p[1]->x) * (p[2]->y - p[1]->y) - (p[2]->x - p[1]->x) * (p[0]->y - p[1]->y) <= 0) return;
	//Not needed as in the tile top down checks depending on the orientation we don't send the quad

	yScanlineMin = SCR_H;
	yScanlineMax = -1;

	prepareEdgeScanlinesFlat(p[0],p[1]);
	prepareEdgeScanlinesFlat(p[1],p[2]);
	prepareEdgeScanlinesFlat(p[2],p[3]);
	prepareEdgeScanlinesFlat(p[3],p[0]);

#ifdef FILL_SCANLINES_ASM
	fillScanlinesAsm(color, vram);
#else
	#ifdef SCR_UNCHAINED
		fillScanlinesUnchained(color, vram);
	#else
		fillScanlines(color, vram);
	#endif
#endif
}
