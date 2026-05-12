#include "render.h"

#include <math.h>
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "demo.h"
#include "engine.h"
#include "mathutil.h"


static inline void drawLineBressenhamSlopeX(int x0, int y0, int x1, int y1, uint8 color, uint8 *vram)
{
	uint8 *dst;

    int dx = x1 - x0;
    int dy = y1 - y0;

    int vy = SCR_LINE_BYTES;
    if (dy < 0) {
        dy = -dy;
        vy = -vy;
    }
    int d = 2 * dy - dx;
    const int dd = 2 * (dy - dx);

	// I don't know if this can correct for subpixel (still too shaky), but we will go for smooth antialias anyway
	int frac0 = y0 & SCR_AND;
	if (d > 0) {
		d -= ((frac0 * dd) >> SCR_BITS);
	} else {
		d -= ((frac0 * 2 * dy) >> SCR_BITS);
	}

	int xp0 = x0 >> SCR_BITS;
	int xp1 = x1 >> SCR_BITS;
	int yp0 = y0 >> SCR_BITS;
	
	CLAMP(xp0, 0, SCR_W-1);
	CLAMP(xp1, 0, SCR_W-1);
	CLAMP(yp0, 0, SCR_H-1);

    vram += VRAM_PIXEL_OFFSET(xp0,yp0);

    for (int x=xp0; x<=xp1; ++x) {
		*vram++ = color;

        if (d > 0) {
            vram += vy;
            d += dd;
        } else {
            d += 2 * dy;
        }
    }
}

static inline void drawLineBressenhamSlopeY(int x0, int y0, int x1, int y1, uint8 color, uint8 *vram)
{
	uint8 *dst;

    int dx = x1 - x0;
    int dy = y1 - y0;

    int vx = 1, vy = SCR_LINE_BYTES;
    if (dx < 0) {
        dx = -dx;
        vx = -1;
    }
    if (dy < 0) {
        vy = -vy;
    }

    int d = 2 * dx - dy;
    const int dd = 2 * (dx - dy);

	// I don't know if this can correct for subpixel (still too shaky), but we will go for smooth antialias anyway
	int frac0 = x0 & SCR_AND;
	if (d > 0) {
		d -= ((frac0 * dd) >> SCR_BITS);
	} else {
		d -= ((frac0 * 2 * dx) >> SCR_BITS);
	}

	int yp0 = y0 >> SCR_BITS;
	int yp1 = y1 >> SCR_BITS;
	int xp0 = x0 >> SCR_BITS;
	
	CLAMP(yp0, 0, SCR_H-1);
	CLAMP(yp1, 0, SCR_H-1);
	CLAMP(xp0, 0, SCR_W-1);

    vram += VRAM_PIXEL_OFFSET(xp0,yp0);

    for (int y=yp0; y<=yp1; ++y) {
		*vram = color;

        if (d > 0) {
			vram+=vx;
            d += dd;
        } else {
            d += 2 * dx;
        }
        vram += vy;
    }
}

void drawLine(ScreenPoint *p0, ScreenPoint *p1, uint8 color, uint8 *vram)
{
	uint8 *dst;

    const int x0 = p0->x >> UNCHAINED_BITS;
    const int x1 = p1->x >> UNCHAINED_BITS;
    const int y0 = p0->y;
    const int y1 = p1->y;

    const int dx = x1 - x0;
    const int dy = y1 - y0;

    if (dx == 0 && dy == 0) return;

	color = (color << SHADE_BITS) + SHADE_AND;
    if (abs(dy) < abs(dx)) {
        if (x0 > x1) {
            drawLineBressenhamSlopeX(x1,y1,x0,y0,color,vram);
        } else {
            drawLineBressenhamSlopeX(x0,y0,x1,y1,color,vram);
        }
    } else {
        if (y0 > y1) {
            drawLineBressenhamSlopeY(x1,y1,x0,y0,color,vram);
        } else {
            drawLineBressenhamSlopeY(x0,y0,x1,y1,color,vram);
        }
    }
}

static inline void drawAntialiasedLineSlopeX(int i0, int i1, int p0, int d, uint8 color, uint8 *vram)
{
	uint8 *dst;
	const uint8 colBase = (color & 7) << SHADE_BITS;

	int x0 = i0 >> SCR_BITS;
	int x1 = i1 >> SCR_BITS;

	int frac0 = i0 & SCR_AND;
	int frac1 = (i1 + 1) & SCR_AND;
	int y = p0;

	if (frac0) {
		frac0 = SCR_AND - frac0;

		uint8 c1 = (y & SCR_AND) >> (SCR_BITS - SHADE_BITS);
		uint8 c0 = SHADE_AND - c1;

		const int yp = y >> SCR_BITS;
		uint8 *dst = vram + VRAM_PIXEL_OFFSET(x0,yp);
		*dst |= colBase + ((c0 * frac0) >> SCR_BITS);
		if (yp < SCR_H - 1) {
			*(dst + SCR_LINE_BYTES) |= colBase + ((c1 * frac0) >> SCR_BITS);
		}

		y += ((frac0 * d) >> SCR_BITS);
		x0++;
	}
	if (frac1) x1--;

	for (int x = x0; x <= x1; x++) {
		const int yp = y >> SCR_BITS;
		uint8 *dst = vram + VRAM_PIXEL_OFFSET(x,yp);

		int c1 = (y & SCR_AND) >> (SCR_BITS - SHADE_BITS);
		int c0 = SHADE_AND - c1;

		*dst = colBase + c0;
		if (yp < SCR_H - 1) {
			*(dst + SCR_LINE_BYTES) = colBase + c1;
		}

		y += d;
	}

	if (frac1) {
		y += ((frac1 * d) >> SCR_BITS) - d;

		uint8 c1 = (y & SCR_AND) >> (SCR_BITS - SHADE_BITS);
		uint8 c0 = SHADE_AND - c1;

		const int yp = y >> SCR_BITS;
		uint8 *dst = vram + VRAM_PIXEL_OFFSET(x1+1,yp);
		*dst |= colBase + ((c0 * frac1) >> SCR_BITS);
		if (yp > 0 && yp < SCR_H - 1) {
			*(dst + SCR_LINE_BYTES) |= colBase + ((c1 * frac1) >> SCR_BITS);
		}
	}
}

static inline void drawAntialiasedLineSlopeY(int i0, int i1, int p0, int d, uint8 color, uint8 *vram)
{
	uint8 *dst;
	const uint8 colBase = (color & 7) << SHADE_BITS;

	int y0 = i0 >> SCR_BITS;
	int y1 = i1 >> SCR_BITS;

	int frac0 = i0 & SCR_AND;
	int frac1 = (i1 + 1) & SCR_AND;
	int x = p0;

	vram += VRAM_PIXEL_OFFSET(0,y0);
	if (frac0) {
		frac0 = SCR_AND - frac0;

		uint8 c1 = (x & SCR_AND) >> (SCR_BITS - SHADE_BITS);
		uint8 c0 = SHADE_AND - c1;

		const int xp = x >> SCR_BITS;
		*(vram + xp) |= colBase + ((c0 * frac0) >> SCR_BITS);
		if (xp < SCR_W - 1) {
			*(vram + xp + 1) |= colBase + ((c1 * frac0) >> SCR_BITS);
		}

		x += ((frac0 * d) >> SCR_BITS);
		y0++;
		vram += SCR_LINE_BYTES;
	}
	if (frac1) y1--;

	for (int y = y0; y <= y1; y++) {
		uint8 c1 = (x & SCR_AND) >> (SCR_BITS - SHADE_BITS);
		uint8 c0 = SHADE_AND - c1;

		const int xp = x >> SCR_BITS;
		*(vram + xp) = colBase + c0;
		if (xp > 0 && xp < SCR_W - 1) {
			*(vram + xp + 1) = colBase + c1;
		}

		vram += SCR_LINE_BYTES;
		x += d;
	}

	if (frac1) {
		x += ((frac1 * d) >> SCR_BITS) - d;

		uint8 c1 = (x & SCR_AND) >> (SCR_BITS - SHADE_BITS);
		uint8 c0 = SHADE_AND - c1;

		const int xp = x >> SCR_BITS;
		*(vram + xp) |= colBase + ((c0 * frac1) >> SCR_BITS);
		if (xp < SCR_W - 1) {
			*(vram + xp + 1) |= colBase + ((c1 * frac1) >> SCR_BITS);
		}
	}
}

void drawLineAntialiased(ScreenPoint *p0, ScreenPoint *p1, uint8 color4, uint8 *vram)
{
    const int x0 = p0->x >> UNCHAINED_BITS;
    const int x1 = p1->x >> UNCHAINED_BITS;
    const int y0 = p0->y;
    const int y1 = p1->y;

	int dx = x1 - x0;
	int dy = y1 - y0;

	// I will do this the laziest way for now as I don't properly clamp lines outside, just so that it doesn't crash.
	// I leave the proper clamping and interpolation to the edges for the future.
	if (x0 < (1 << SCR_BITS) || x0 > ((SCR_W - 2) << SCR_BITS) || x1 < (1 << SCR_BITS) || x1 > ((SCR_W - 2) << SCR_BITS) || 
		y0 < (1 << SCR_BITS) || y0 > ((SCR_H - 2) << SCR_BITS) || y1 < (1 << SCR_BITS) || y1 > ((SCR_H - 2) << SCR_BITS)) return;

    if (dx == 0 && dy == 0) return;

	int d = 0;
	if (abs(dy) < abs(dx)) {
		if (dx != 0) {
			d = (dy << SCR_BITS) / dx;
		}

		if (x0 < x1) {
			drawAntialiasedLineSlopeX(x0, x1, y0, d, color4, vram);
		} else {
			drawAntialiasedLineSlopeX(x1, x0, y1, d, color4, vram);
		}
	} else {
		if (dy != 0) {
			d = (dx << SCR_BITS) / dy;
		}

		if (y0 < y1) {
			drawAntialiasedLineSlopeY(y0, y1, x0, d, color4, vram);
		} else {
			drawAntialiasedLineSlopeY(y1, y0, x1, d, color4, vram);
		}
	}
}
