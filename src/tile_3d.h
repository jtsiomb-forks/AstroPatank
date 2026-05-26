#ifndef TILE_3D_H
#define TILE_3D_H

#include "types.h"
#include "game.h"
#include "vector.h"

#define TILE_SIZE_BITS 8
#define TILE_SIZE (1 << TILE_SIZE_BITS)
#define TILE_HEIGHT (TILE_SIZE * 1)

#define TILEMAP_WIDTH 64
#define TILEMAP_HEIGHT 64
#define TILEMAP_LAYER_SIZE (TILEMAP_WIDTH * TILEMAP_HEIGHT)
#define TILEMAP_LAYERS 4
#define TILEMAP_SIZE (TILEMAP_LAYER_SIZE * TILEMAP_LAYERS)

#define TILEMAP_POINTS_W (TILEMAP_WIDTH + 1)
#define TILEMAP_POINTS_H (TILEMAP_HEIGHT + 1)
#define TILEMAP_POINTS_LAYER_SIZE (TILEMAP_POINTS_W * TILEMAP_POINTS_H)
#define TILEMAP_POINTS_SIZE (TILEMAP_POINTS_LAYER_SIZE * TILEMAP_LAYERS)

void tilemap3dInit();
void renderTilemap3dLayer(Vec3 *pos, uint8 layer, Screen *screen);

void advTileRenderType(bool inc);

uint8* getTilemap3D();

#endif
