#include <math.h>
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "tile_3d.h"
#include "engine.h"
#include "video.h"

typedef struct TilemapRange
{
	int edgeX;
	int edgeY;
} TilemapRange;


static uint8 tilemap3d[TILEMAP_SIZE];
static TilemapRange tilemapRange[TILEMAP_LAYERS];

void tilemap3dInit()
{
	memset(tilemap3d, 0, sizeof(tilemap3d));
}

static void drawDot(int xs, int ys, uint8 color, uint8 *vram)
{
	if (ys >= 0 && ys < SCR_H && xs >=0 && xs < SCR_W) {
		uint8 *dst = vram + VRAM_PIXEL_OFFSET((xs>>UNCHAINED_BITS),ys);
		*dst = color;
	}
}

void renderTilemap3dLayer(Vec3 *pos, uint8 layer, Screen *screen)
{
	uint8 *tmap = &tilemap3d[layer * TILEMAP_LAYER_SIZE];
	uint8 *vram = (uint8*)screen->data;

	const int edgeX = tilemapRange[layer].edgeX;
	const int edgeY = tilemapRange[layer].edgeY;

	int pz = pos->z + TILE_SIZE * (TILEMAP_LAYERS - layer);
	int py = -pos->y;
	for (int y=0; y<TILEMAP_HEIGHT; ++y) {
		if (py > -edgeY && py < edgeY) {
			int ys = (py << PROJ_BITS) / pz + SCR_H / 2;
			int px = -pos->x;
			for (int x=0; x<TILEMAP_WIDTH; ++x) {
				if (px > -edgeX && px < edgeX) {
					int xs = (px << PROJ_BITS) / pz + SCR_W / 2;
					drawDot(xs,ys, layer*4+3, vram);
				}
				px += TILE_SIZE;
			}
		}
		py += TILE_SIZE;
	}
}

static void updateTilemapEdges(Vec3 *pos)
{
	for (int i=0; i<TILEMAP_LAYERS; ++i) {
		TilemapRange *tmapRange = &tilemapRange[i];
		int pz = pos->z + TILE_SIZE * (TILEMAP_LAYERS - i);
		tmapRange->edgeX = ((SCR_W/2) * pz) >> PROJ_BITS;
		tmapRange->edgeY = ((SCR_H/2) * pz) >> PROJ_BITS;
	}
}

void renderTilemap3d(Vec3 *pos, Screen *screen)
{
	updateTilemapEdges(pos);

	for (int i=0; i<TILEMAP_LAYERS; ++i) {
		renderTilemap3dLayer(pos, i, screen);
	}
}
