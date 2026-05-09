#include <math.h>
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "tile_3d.h"
#include "engine.h"
#include "render.h"
#include "video.h"

typedef struct TilemapRange
{
	int edgeX;
	int edgeY;
} TilemapRange;

typedef struct Point2D
{
	int x,y;
} Point2D;


static uint8 tilemap3d[TILEMAP_SIZE];
static TilemapRange tilemapRange[TILEMAP_LAYERS];

//static Point2D screenPoints[TILEMAP_SIZE];

static int xsData[TILEMAP_WIDTH];
static int x0,x1;

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

static void cacheScreenXdata(Vec3 *pos, int edgeX, int edgeY, int pz)
{
	x0=0;
	x1=TILEMAP_WIDTH;

	bool xIsIn = false;
	int px = -pos->x;
	for (int x=0; x<TILEMAP_WIDTH; ++x) {
		if (px > -edgeX && px < edgeX) {
			int xs = (px << PROJ_BITS) / pz + SCR_W / 2;
			xsData[x] = xs;
			if (!xIsIn) {
				x0 = x;
				xIsIn = true;
			}
		} else {
			if (xIsIn) {
				x1 = x;
				xIsIn = false;
			}
		}
		px += TILE_SIZE;
	}

	if (x1 > 0 && x1 < TILEMAP_WIDTH) {
		xsData[x1] = xsData[x1-1];	// just clamp the right edge
	}
}

void renderTilemap3dLayer(Vec3 *pos, uint8 layer, Screen *screen)
{
	ScreenPoint p0, p1, p2, p3;

	uint8 *tmap = &tilemap3d[layer * TILEMAP_LAYER_SIZE];
	uint8 *vram = (uint8*)screen->data;

	const int edgeX = tilemapRange[layer].edgeX;
	const int edgeY = tilemapRange[layer].edgeY;

	int pz = pos->z + TILE_SIZE * (TILEMAP_LAYERS - layer);
	p0.z = p1.z = p2.z = p3.z = pz;

	cacheScreenXdata(pos, edgeX, edgeY, pz);
	int diffX = (xsData[x0+1] - xsData[x0]) << SCR_BITS;

	uint8 color = ((layer+1) * 16) / TILEMAP_LAYERS;
	if (color > 15) color = 15;

	int py = -pos->y;
	uint8 *src = &tilemap3d[layer*TILEMAP_LAYER_SIZE];
	//Point2D *pt = &screenPoints[layer*TILEMAP_LAYER_SIZE];
	for (int y=0; y<TILEMAP_HEIGHT; ++y) {
		if (py > -edgeY && py < edgeY) {
			int ys = ((py << PROJ_BITS) / pz + SCR_H / 2) << SCR_BITS;
			for (int x=x0; x<x1; ++x) {
				if (src[x]) {
					//drawDot(xsData[x],ys, color, vram);
					int xs = xsData[x] << SCR_BITS;

					p0.x = xs; p1.x = xs + diffX; p2.x = xs + diffX; p3.x = xs;
					p0.y = ys; p1.y = ys; p2.y = ys + diffX; p3.y = ys + diffX;

					drawLine(&p0, &p1, layer, vram);
					drawLine(&p1, &p2, layer, vram);
					drawLine(&p2, &p3, layer, vram);
					drawLine(&p3, &p0, layer, vram);
				}
				//pt[x].x = src[x];
				//pt[x].y = py;
			}
		}
		py += TILE_SIZE;

		src += TILEMAP_WIDTH;
		//pt += TILEMAP_WIDTH;
	}
}

static void updateTilemapEdges(Vec3 *pos)
{
	for (int i=0; i<TILEMAP_LAYERS; ++i) {
		TilemapRange *tmapRange = &tilemapRange[i];
		int pz = pos->z + TILE_SIZE * (TILEMAP_LAYERS - i);
		tmapRange->edgeX = ((SCR_W/2 + TILE_SIZE) * pz) >> PROJ_BITS;
		tmapRange->edgeY = ((SCR_H/2 + TILE_SIZE) * pz) >> PROJ_BITS;
	}
}

//262-1841 (131072)
//407-2121
/*static void printSomething()
{
	for (int i=0; i<TILEMAP_LAYERS; ++i) {
		printf("%d,%d   ", tilemapRange[i].edgeX, tilemapRange[i].edgeY);
	}
	printf("\n");
}*/

void renderTilemap3d(Vec3 *pos, Screen *screen)
{
	updateTilemapEdges(pos);

	for (int i=0; i<TILEMAP_LAYERS; ++i) {
		renderTilemap3dLayer(pos, i, screen);
	}

	//printSomething();
}
