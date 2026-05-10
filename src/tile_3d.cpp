#include <math.h>
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "tile_3d.h"
#include "engine.h"
#include "render.h"
#include "vector.h"
#include "video.h"
#include "mathutil.h"

enum {
	TILE_RENDER_DOTS,
	TILE_RENDER_LINES,
	TILE_RENDER_QUADS,
	TILE_RENDER_MESH,
	TILE_RENDER_COUNT
};

typedef struct TilemapPos
{
	int x,y;
	int xs,ys;
} TilemapPos;

typedef struct TilemapRange
{
	int edgeX, edgeY;
	int x0,x1,y0,y1;
} TilemapRange;


static uint8 tilemap3d[TILEMAP_SIZE];
static TilemapRange tilemapRange[TILEMAP_LAYERS];

static Vec3 tilePos[TILEMAP_LAYER_SIZE];

static TilemapPos tmapPos[TILEMAP_WIDTH];



static int tileRenderType = TILE_RENDER_DOTS;

void advTileRenderType(bool inc)
{
	if (inc) {
		tileRenderType++;
		if (tileRenderType >= TILE_RENDER_COUNT) tileRenderType = 0;
	} else {
		tileRenderType--;
		if (tileRenderType < 0) tileRenderType = TILE_RENDER_COUNT - 1;
	}
}

void tilemap3dInit()
{
	memset(tilemap3d, 0, sizeof(tilemap3d));

	for (int i=0; i<TILEMAP_LAYERS; ++i) {
		uint8 n = (1 << (1+i)) - 1;
		uint8 *dst = &tilemap3d[i*TILEMAP_LAYER_SIZE];
		for (int y=0; y<TILEMAP_HEIGHT; ++y) {
			for (int x=0; x<TILEMAP_WIDTH; ++x) {
				uint8 c = 0;
				if (!(x & n) && !(y & n)) c = 1;
				*dst++ = c;
			}
		}
	}
}

static void drawDot(int xs, int ys, uint8 color, uint8 *vram)
{
	if (ys >= 0 && ys < SCR_H && xs >=0 && xs < SCR_W) {
		uint8 *dst = vram + VRAM_PIXEL_OFFSET((xs>>UNCHAINED_BITS),ys);
		*dst = color;
	}
}

static void drawQuadLines(int x0, int y0, int x1, int y1, uint8 color, uint8 *vram)
{
	CLAMP(x0,0,SCR_W-1);
	CLAMP(x1,0,SCR_W-1);
	CLAMP(y0,0,SCR_H-1);
	CLAMP(y1,0,SCR_H-1);

	uint32 color32 = (color << 24) | (color << 16) | (color << 8) | color;

	if (x1 <= x0 || y1 <= y0) return;

	int countY = y1 - y0;
	for (int y = y0; y<=y1; y+=countY) {
		uint8 *dst = vram + VRAM_PIXEL_OFFSET((x0>>UNCHAINED_BITS),y);
		int16 length = x1 - x0;

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
	};

	uint8 *dstX0 = vram + VRAM_PIXEL_OFFSET((x0>>UNCHAINED_BITS),y0);
	uint8 *dstX1 = vram + VRAM_PIXEL_OFFSET((x1>>UNCHAINED_BITS),y0);
	for (int y = y0; y<y1; ++y) {
		*dstX0 = color;
		*dstX1 = color;
		dstX0 += SCR_LINE_BYTES;
		dstX1 += SCR_LINE_BYTES;
	}
}

static void drawQuad(int x0, int y0, int x1, int y1, uint8 color, uint8 *vram)
{
	CLAMP(x0,0,SCR_W-1);
	CLAMP(x1,0,SCR_W-1);
	CLAMP(y0,0,SCR_H-1);
	CLAMP(y1,0,SCR_H-1);

	uint8 *dstY = vram + VRAM_PIXEL_OFFSET(0,y0);
	uint32 color32 = (color << 24) | (color << 16) | (color << 8) | color;

	int countY = y1 - y0;
	while (countY-- > 0) {
		uint8 *dst = dstY + (x0>>UNCHAINED_BITS);
		int16 length = x1 - x0;

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

		dstY += SCR_LINE_BYTES;
	};
}

static void cacheScreenData(Vec3 *pos, TilemapRange *tmapRange, int layerZ)
{
	int x0 = tmapRange->x0;
	int x1 = tmapRange->x1;
	int y0 = tmapRange->y0;
	int y1 = tmapRange->y1;

	int px = -pos->x + x0 * TILE_SIZE;
	for (int x=x0; x<x1; ++x) {
		tmapPos[x].x = px;
		tmapPos[x].xs = (px << PROJ_BITS) / layerZ + SCR_W / 2;
		px += TILE_SIZE;
	}

	int py = -pos->y + y0 * TILE_SIZE;
	for (int y=y0; y<y1; ++y) {
		tmapPos[y].y = py;
		tmapPos[y].ys = (py << PROJ_BITS) / layerZ + SCR_H / 2;
		py += TILE_SIZE;
	}
}

static void findTilemapExtends(int posI, int iRange, int edgeI, int *tmapI0, int *tmapI1)
{
	int i0=0;
	int i1=iRange;

	bool isIn = false;
	for (int i=0; i<iRange; ++i) {
		if (posI > -edgeI && posI < edgeI) {
			if (!isIn) {
				i0 = i;
				isIn = true;
			}
		} else {
			if (isIn) {
				i1 = i;
				isIn = false;
			}
		}
		posI += TILE_SIZE;
	}

	if (i0 > 0) i0--;

	*tmapI0 = i0;
	*tmapI1 = i1;
}

static void updateTilemapEdges(Vec3 *pos, uint8 layer)
{
	TilemapRange *tmapRange = &tilemapRange[layer];
	int layerZ = pos->z + TILE_SIZE * (TILEMAP_LAYERS - layer);
	tmapRange->edgeX = (SCR_W/2 * layerZ) >> PROJ_BITS;
	tmapRange->edgeY = (SCR_H/2 * layerZ) >> PROJ_BITS;

	findTilemapExtends(-pos->x, TILEMAP_WIDTH, tmapRange->edgeX, &tmapRange->x0, &tmapRange->x1);
	findTilemapExtends(-pos->y, TILEMAP_HEIGHT, tmapRange->edgeY, &tmapRange->y0, &tmapRange->y1);

	cacheScreenData(pos, tmapRange, layerZ);
}

static void renderTilemap3dLayerQuads(int x0, int y0, int x1, int y1, uint8 color, int tileScrSize, uint8 *tmap, uint8 *vram)
{
	for (int y=y0; y<y1; ++y) {
		const int ys = tmapPos[y].ys + 1;
		for (int x=x0; x<x1; ++x) {
			if (tmap[x]) {
				const int xs = tmapPos[x].xs + 1;
				drawQuad(xs,ys, xs+tileScrSize, ys+tileScrSize, color, vram);
			}
		}
		tmap += TILEMAP_WIDTH;
	}
}

static void renderTilemap3dLayerLines(int x0, int y0, int x1, int y1, uint8 color, int tileScrSize, uint8 *tmap, uint8 *vram)
{
	for (int y=y0; y<y1; ++y) {
		const int ys = tmapPos[y].ys + 8;
		for (int x=x0; x<x1; ++x) {
			if (tmap[x]) {
				const int xs = tmapPos[x].xs + 8;
				drawQuadLines(xs,ys, xs+tileScrSize, ys+tileScrSize, color, vram);
			}
		}
		tmap += TILEMAP_WIDTH;
	}
}

static void renderTilemap3dLayerDots(int x0, int y0, int x1, int y1, uint8 color, uint8 *tmap, uint8 *vram)
{
	for (int y=y0; y<y1; ++y) {
		const int ys = tmapPos[y].ys;
		for (int x=x0; x<x1; ++x) {
			if (tmap[x]) {
				drawDot(tmapPos[x].xs, ys, color, vram);
			}
		}
		tmap += TILEMAP_WIDTH;
	}
}

static void renderTilemap3dLayer(Vec3 *pos, uint8 layer, Screen *screen)
{
	TilemapRange *tmapRange = &tilemapRange[layer];
	const int x0 = tmapRange->x0;
	const int x1 = tmapRange->x1;
	const int y0 = tmapRange->y0;
	const int y1 = tmapRange->y1;

	uint8 color = ((layer+1) * 16) / TILEMAP_LAYERS;
	if (color > 15) color = 15;

	uint8 *tmap = &tilemap3d[layer*TILEMAP_LAYER_SIZE + y0*TILEMAP_WIDTH];
	uint8 *vram = (uint8*)screen->data;

	int layerZ, tileScrSize;
	if (tileRenderType > TILE_RENDER_DOTS) {
		layerZ = pos->z + TILE_SIZE * (TILEMAP_LAYERS - layer);
		tileScrSize = (TILE_SIZE << PROJ_BITS) / layerZ - 2;
	}

	switch(tileRenderType) {
		case TILE_RENDER_DOTS:
			renderTilemap3dLayerDots(x0,y0,x1,y1,color,tmap,vram);
		break;

		case TILE_RENDER_LINES:
			renderTilemap3dLayerLines(x0,y0,x1,y1,color,tileScrSize,tmap,vram);
		break;

		case TILE_RENDER_QUADS:
			renderTilemap3dLayerQuads(x0,y0,x1,y1,color,tileScrSize,tmap,vram);
		break;

		case TILE_RENDER_MESH:
		break;
	}
}

//262-1841 (131072)
//407-2121
//395-2096

void renderTilemap3d(Vec3 *pos, Screen *screen)
{
	for (int i=0; i<TILEMAP_LAYERS; ++i) {
		updateTilemapEdges(pos, i);
		renderTilemap3dLayer(pos, i, screen);
	}
}
