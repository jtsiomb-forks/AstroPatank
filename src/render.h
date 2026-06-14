#ifndef RENDER_H
#define RENDER_H

#include "types.h"
#include "game.h"
#include "engine.h"
#include "vector.h"

#define ANTIALIASING

//#define ANTIALIASING_POLY
#ifndef ANTIALIASING_POLY
	#define FILL_SCANLINES_ASM
#endif

#define FP_RAST 8
#define FP_AND ((1 << FP_RAST) - 1)

void drawLine(ScreenPoint *p0, ScreenPoint *p1, uint8 color, uint8 *vram);
void drawLineAntialiased(ScreenPoint *p0, ScreenPoint *p1, uint8 color4, uint8 *vram);
void drawPoly(ScreenPoint *p[], int edgesNum, uint8 color, uint8 *vram);
void drawPolyAntialiased(ScreenPoint *p[], uint8 *edgeAdjacentPolysNum, int edgesNum, uint8 color, uint8 *vram);

void drawQuad(ScreenPoint2D *p[], uint8 color, uint8 *vram);

#endif
