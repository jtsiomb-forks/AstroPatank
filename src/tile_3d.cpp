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

typedef struct TilemapGridInfo
{
	int x0,x1,y0,y1;
	int xs0, ys0;
	int tileStep;
} TilemapGridInfo;

typedef struct TileMeshInfo
{
	int numQuadsX, numQuadsY, numQuadsZ;
	ScreenPoint **spStartX;
	ScreenPoint **spStartY;
	ScreenPoint **spStartZ;
} TileMeshInfo;


static uint8 tilemap3d[TILEMAP_SIZE];
static TilemapGridInfo tmapGridInfo;

static int tileRenderType = TILE_RENDER_DOTS;

static ScreenPoint tileScrPt[TILEMAP_SIZE];
static TileMeshInfo tileMeshInfo[TILEMAP_SIZE];
static ScreenPoint *scrPlist[TILEMAP_SIZE * (4 / 2) * 6];		// good theoritical maximum? Will reduce..
static ScreenPoint **spNext = scrPlist;

// scrPlist 4 ScreenPoint* per quad point at &tileScrPtr[n]
// tileMeshInfo per tile, tells you number of quads (can be 0 to 6 (but 5 max in our case as the bottom is always out of view)), then pointer to scrPlist start of the sequence of points

//New tileMeshInfo[numQuads, spNext]
//==================================
//spStart = spNext
//From tile look 6 directions, inc numQuads each time
//add four ScreenPoint* to *spNext = sp0/1/2/3


uint8* getTilemap3D()
{
	return tilemap3d;
}

static void buildTilemapMesh()
{
	uint8 *src = tilemap3d;
	ScreenPoint *srcPt = tileScrPt;
	TileMeshInfo *dst = tileMeshInfo;

	for (int i=0; i<TILEMAP_LAYERS; ++i) {
		for (int y=0; y<TILEMAP_HEIGHT; ++y) {
			for (int x=0; x<TILEMAP_WIDTH; ++x) {
				int numQuadsX = 0;
				int numQuadsY = 0;
				int numQuadsZ = 0;
				if (x<TILEMAP_WIDTH-1 && y<TILEMAP_HEIGHT-1) {
					uint8 c = *src;
					if (c != 0) {
						if (i > 0) {
							dst->spStartX = spNext;
							if (x==0 || *(src - 1)==0) {
								*spNext++ = &srcPt[0];
								*spNext++ = &srcPt[-TILEMAP_LAYER_SIZE];
								*spNext++ = &srcPt[-TILEMAP_LAYER_SIZE+TILEMAP_WIDTH];
								*spNext++ = &srcPt[TILEMAP_WIDTH];
								++numQuadsX;
							}
							if (*(src + 1)==0) {
								*spNext++ = &srcPt[1+TILEMAP_WIDTH];
								*spNext++ = &srcPt[-TILEMAP_LAYER_SIZE+1+TILEMAP_WIDTH];
								*spNext++ = &srcPt[-TILEMAP_LAYER_SIZE+1];
								*spNext++ = &srcPt[1];
								++numQuadsX;
							}

							dst->spStartY = spNext;
							if (y==0 || *(src - TILEMAP_WIDTH)==0) {
								*spNext++ = &srcPt[1];
								*spNext++ = &srcPt[-TILEMAP_LAYER_SIZE+1];
								*spNext++ = &srcPt[-TILEMAP_LAYER_SIZE];
								*spNext++ = &srcPt[0];
								++numQuadsY;
							}
							if (*(src + TILEMAP_WIDTH)==0) {
								*spNext++ = &srcPt[TILEMAP_WIDTH];
								*spNext++ = &srcPt[-TILEMAP_LAYER_SIZE+TILEMAP_WIDTH];
								*spNext++ = &srcPt[-TILEMAP_LAYER_SIZE+TILEMAP_WIDTH+1];
								*spNext++ = &srcPt[TILEMAP_WIDTH+1];
								++numQuadsY;
							}
						}

						dst->spStartZ = spNext;
						if (i==TILEMAP_LAYERS-1 || *(src + TILEMAP_LAYER_SIZE)==0) {
							*spNext++ = &srcPt[TILEMAP_WIDTH];
							*spNext++ = &srcPt[1+TILEMAP_WIDTH];
							*spNext++ = &srcPt[1];
							*spNext++ = &srcPt[0];
							++numQuadsZ;
						}
					}
				}
				srcPt++;
				dst->numQuadsX = numQuadsX;
				dst->numQuadsY = numQuadsY;
				dst->numQuadsZ = numQuadsZ;
				dst++;
				src++;
			}
		}
	}
}

static void fillLayerRect(int x0, int y0, int x1, int y1, int layer)
{
	uint8 *dst = &tilemap3d[layer * TILEMAP_LAYER_SIZE + y0 * TILEMAP_WIDTH];
	for (int y=y0; y<=y1; ++y) {
		for (int x=x0; x<=x1; ++x) {
			dst[x] = 1;
		}
		dst += TILEMAP_WIDTH;
	}
}

void tilemap3dInit()
{
	memset(tilemap3d, 0, sizeof(tilemap3d));

	uint8 *dst = tilemap3d;
	for (int i=0; i<TILEMAP_LAYERS; ++i) {
		uint8 n = ((1 << (2+i)) - 1) & 15;
		for (int y=0; y<TILEMAP_HEIGHT; ++y) {
			for (int x=0; x<TILEMAP_WIDTH; ++x) {
				uint8 c = 0;
				if (i==2) {
					if (!(x & n) || !(y & n)) c = 1;
				} else {
					if (!(x & n) && !(y & n)) c = 1;
				}
				*dst++ = c;
			}
		}
	}

	for (int i=0; i<TILEMAP_LAYERS; ++i) {
		int x0 = 36+i;
		int x1 = 42-i;
		int y0 = 36+i;
		int y1 = 42-i;
		fillLayerRect(x0,y0,x1,y1,i);

		fillLayerRect(28,34,29,35,i);
	}

	buildTilemapMesh();
}

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

static void drawDot(int xs, int ys, uint8 color, uint8 *vram)
{
	xs >>= SCR_BITS;
	ys >>= SCR_BITS;
	if (ys >= 0 && ys < SCR_H && xs >=0 && xs < SCR_W) {
		uint8 *dst = vram + VRAM_PIXEL_OFFSET((xs>>UNCHAINED_BITS),ys);
		*dst = color;
	}
}

static void drawRectangleLines(int x0, int y0, int x1, int y1, uint8 color, uint8 *vram)
{
	x0 >>= SCR_BITS;
	y0 >>= SCR_BITS;
	x1 >>= SCR_BITS;
	y1 >>= SCR_BITS;

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

static void drawRectangle(int x0, int y0, int x1, int y1, uint8 color, uint8 *vram)
{
	x0 >>= SCR_BITS;
	y0 >>= SCR_BITS;
	x1 >>= SCR_BITS;
	y1 >>= SCR_BITS;

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
	int layerZ = pos->z + TILE_SIZE * (TILEMAP_LAYERS - layer);
	int edgeZ = layerZ;
	if (layer > 0) {
		edgeZ += TILE_SIZE;
	}
	int edgeX = (SCR_W/2 * edgeZ) >> PROJ_BITS;
	int edgeY = (SCR_H/2 * edgeZ) >> PROJ_BITS;

	findTilemapExtends(-pos->x, TILEMAP_WIDTH, edgeX, &tmapGridInfo.x0, &tmapGridInfo.x1);
	findTilemapExtends(-pos->y, TILEMAP_HEIGHT, edgeY, &tmapGridInfo.y0, &tmapGridInfo.y1);

	tmapGridInfo.xs0 = ((-pos->x + tmapGridInfo.x0 * TILE_SIZE) << (SCR_BITS + PROJ_BITS)) / layerZ + ((SCR_W / 2) << SCR_BITS);
	tmapGridInfo.ys0 = ((-pos->y + tmapGridInfo.y0 * TILE_SIZE) << (SCR_BITS + PROJ_BITS)) / layerZ + ((SCR_H / 2) << SCR_BITS);
	tmapGridInfo.tileStep = (TILE_SIZE << (SCR_BITS + PROJ_BITS)) / layerZ;
}
//116,878
//124,890
static bool checkFaceOrder(ScreenPoint *p[])
{
	int x0 = p[0]->x;
	int y0 = p[0]->y;
	int x1 = p[1]->x;
	int y1 = p[1]->y;
	int x2 = p[2]->x;
	int y2 = p[2]->y;

	int faceOrder = (x0 - x1) * (y2 - y1) - (x2 - x1) * (y0 - y1);

	return faceOrder >= 0;
}

static void renderTilemap3DLayerMesh(uint8 layer, uint8 *vram)
{
	const int tileRowOffset = layer * TILEMAP_LAYER_SIZE + tmapGridInfo.y0 * TILEMAP_WIDTH;
	ScreenPoint *sp = &tileScrPt[tileRowOffset];
	TileMeshInfo *tmi = &tileMeshInfo[tileRowOffset];

	int x0 = tmapGridInfo.x0;
	int y0 = tmapGridInfo.y0;
	int x1 = tmapGridInfo.x1;
	int y1 = tmapGridInfo.y1;

	int x1b = x1;
	int y1b = y1;
	if (x1 < TILEMAP_WIDTH-1) ++x1b;
	if (y1 < TILEMAP_HEIGHT-1) ++y1b;

	int step = tmapGridInfo.tileStep;
	int ys = tmapGridInfo.ys0;
	for (int y=y0; y<y1b; ++y) {
		int xs = tmapGridInfo.xs0;
		for (int x=x0; x<x1b; ++x) {
			sp[x].x = xs;
			sp[x].y = ys;
			xs += step;
		}
		ys += step;
		sp += TILEMAP_WIDTH;
	}

	uint8 color = 2 + 2 * layer;
	for (int y=y0; y<y1; ++y) {
		for (int x=x0; x<x1; ++x) {
			int numQuads = tmi[x].numQuadsX;
			if (numQuads != 0) {
				ScreenPoint **spQuad = tmi[x].spStartX;
				for (int n=0; n<numQuads; ++n) {
					if (checkFaceOrder(spQuad)) {
					//if (spQuad[0]->x < spQuad[1]->x) {
						drawQuad(spQuad, color + 2 * n, vram);
					}
					spQuad+=4;
				}
			}
		}
		tmi += TILEMAP_WIDTH;
	}

	tmi = &tileMeshInfo[tileRowOffset];
	color = 6 + 2 * layer;
	for (int y=y0; y<y1; ++y) {
		for (int x=x0; x<x1; ++x) {
			int numQuads = tmi[x].numQuadsY;
			if (numQuads != 0) {
				ScreenPoint **spQuad = tmi[x].spStartY;
				for (int n=0; n<numQuads; ++n) {
					if (checkFaceOrder(spQuad)) {
					//if (spQuad[0]->y < spQuad[1]->y) {
						drawQuad(spQuad, color + 2 * n, vram);
					}
					spQuad+=4;
				}
			}
		}
		tmi += TILEMAP_WIDTH;
	}

	tmi = &tileMeshInfo[tileRowOffset];
	color = (layer + 1) * 3;
	for (int y=y0; y<y1; ++y) {
		for (int x=x0; x<x1; ++x) {
			int numQuads = tmi[x].numQuadsZ;
			if (numQuads != 0) {
				ScreenPoint **spQuad = tmi[x].spStartZ;
				//for (int n=0; n<numQuads; ++n) {
					//if (checkFaceOrder(spQuad)) {
						drawQuad(spQuad, color, vram);
					//}
					//spQuad += 4;
				//}
			}
		}
		tmi += TILEMAP_WIDTH;
	}
}

static void renderTilemap3dLayerQuads(uint8 color, uint8 *tmap, uint8 *vram)
{
	int x0 = tmapGridInfo.x0;
	int y0 = tmapGridInfo.y0;
	int x1 = tmapGridInfo.x1;
	int y1 = tmapGridInfo.y1;

	int step = tmapGridInfo.tileStep;
	int ys = tmapGridInfo.ys0;
	for (int y=y0; y<y1; ++y) {
		int xs = tmapGridInfo.xs0;
		for (int x=x0; x<x1; ++x) {
			if (tmap[x]) {
				drawRectangle(xs,ys, xs+step, ys+step, color, vram);
			}
			xs += step;
		}
		ys += step;
		tmap += TILEMAP_WIDTH;
	}
}

static void renderTilemap3dLayerLines(uint8 color, uint8 *tmap, uint8 *vram)
{
	int x0 = tmapGridInfo.x0;
	int y0 = tmapGridInfo.y0;
	int x1 = tmapGridInfo.x1;
	int y1 = tmapGridInfo.y1;

	int step = tmapGridInfo.tileStep;
	int ys = tmapGridInfo.ys0;
	for (int y=y0; y<y1; ++y) {
		int xs = tmapGridInfo.xs0;
		for (int x=x0; x<x1; ++x) {
			if (tmap[x]) {
				drawRectangleLines(xs,ys, xs+step, ys+step, color, vram);
			}
			xs += step;
		}
		ys += step;
		tmap += TILEMAP_WIDTH;
	}
}

static void renderTilemap3dLayerDots(uint8 color, uint8 *tmap, uint8 *vram)
{
	int x0 = tmapGridInfo.x0;
	int y0 = tmapGridInfo.y0;
	int x1 = tmapGridInfo.x1;
	int y1 = tmapGridInfo.y1;

	int step = tmapGridInfo.tileStep;
	int ys = tmapGridInfo.ys0;
	for (int y=y0; y<y1; ++y) {
		int xs = tmapGridInfo.xs0;
		for (int x=x0; x<x1; ++x) {
			if (tmap[x]) {
				drawDot(xs, ys, color, vram);
			}
			xs += step;
		}
		ys += step;
		tmap += TILEMAP_WIDTH;
	}
}

void renderTilemap3dLayer(Vec3 *pos, uint8 layer, Screen *screen)
{
	updateTilemapEdges(pos, layer);

	uint8 color = ((layer+1) * 16) / TILEMAP_LAYERS;
	if (color > 15) color = 15;

	uint8 *tmap = &tilemap3d[layer*TILEMAP_LAYER_SIZE + tmapGridInfo.y0 * TILEMAP_WIDTH];
	uint8 *vram = (uint8*)screen->data;

	switch(tileRenderType) {
		case TILE_RENDER_DOTS:
			renderTilemap3dLayerDots(color,tmap,vram);
		break;

		case TILE_RENDER_LINES:
			renderTilemap3dLayerLines(color,tmap,vram);
		break;

		case TILE_RENDER_QUADS:
			renderTilemap3dLayerQuads(color,tmap,vram);
		break;

		case TILE_RENDER_MESH:
			renderTilemap3DLayerMesh(layer, vram);
		break;
	}
}

// start   : 2013, 1595, 866 (2037, 1594, 833)
// zoom out: 621, 230, 250 (613, 206, 198)