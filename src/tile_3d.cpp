#include <math.h>
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "tile_3d.h"
#include "engine.h"
#include "video.h"


static uint8 tilemap3d[TILEMAP_SIZE];

void tilemap3dInit()
{
	memset(tilemap3d, 0, sizeof(tilemap3d));
}

void renderTilemap3dLayer(Vec3 *pos, uint8 layer, Screen *screen)
{
	uint8 *tmap = &tilemap3d[layer * TILEMAP_LAYER_SIZE];
	uint8 *vram = (uint8*)screen->data;

	int pz = pos->z + TILE_SIZE * (TILEMAP_LAYERS - layer);
	int py = -pos->y;
	for (int y=0; y<TILEMAP_HEIGHT; ++y) {
		int yp = (py << PROJ_BITS) / pz + SCR_H / 2;
		if (yp >= 0 && yp < SCR_H) {
			int px = -pos->x;
			for (int x=0; x<TILEMAP_WIDTH; ++x) {
				int xp = (px << PROJ_BITS) / pz + SCR_W / 2;
				if (xp >=0 && xp < SCR_W) {
					uint8 *dst = vram + VRAM_PIXEL_OFFSET((xp>>UNCHAINED_BITS),yp);
					uint8 color = layer*4 + 3;
					*dst = color;
				}
				px += TILE_SIZE;
			}
		}
		py += TILE_SIZE;
	}
}

void renderTilemap3d(Vec3 *pos, Screen *screen)
{
	for (int i=0; i<TILEMAP_LAYERS; ++i) {
		renderTilemap3dLayer(pos, i, screen);
	}
}
