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

#include "maps.h"

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


static uint8 tilemap3d[TILEMAP_SIZE];
static TilemapGridInfo tmapGridInfo;

static int tileRenderType = TILE_RENDER_DOTS;

static ScreenPoint tileScrPt[TILEMAP_SIZE];
static ScreenPoint **tileMeshScreenPointPtr[TILEMAP_SIZE];
static ScreenPoint *scrPlist[TILEMAP_SIZE * (4 / 2) * 6];		// good theoritical maximum? Will reduce..
static ScreenPoint **spNext = scrPlist;


#define FILL_SHAPES_ASM

extern "C" {
	void drawRectangleAsm(ScreenPoint *p0, ScreenPoint *p1, uint8 color, uint8 *vram);
}


uint8* getTilemap3D()
{
	return tilemap3d;
}

static void buildTilemapMesh()
{
	uint8 *src = tilemap3d;
	ScreenPoint *srcPt = tileScrPt;
	ScreenPoint ***dst = tileMeshScreenPointPtr;

	for (int i=0; i<TILEMAP_LAYERS; ++i) {
		for (int y=0; y<TILEMAP_HEIGHT; ++y) {
			for (int x=0; x<TILEMAP_WIDTH; ++x) {
				*dst = NULL; // if it finds zero quads later, NULL to know to skip
				if (x<TILEMAP_WIDTH-1 && y<TILEMAP_HEIGHT-1) {
					uint8 c = *src;
					if (c != 0) {
						*dst = spNext;
						if (i > 0) {
							if (x==0 || *(src - 1)==0) {
								spNext[0] = &srcPt[0];
								spNext[1] = &srcPt[-TILEMAP_LAYER_SIZE];
								spNext[2] = &srcPt[-TILEMAP_LAYER_SIZE+TILEMAP_WIDTH];
								spNext[3] = &srcPt[TILEMAP_WIDTH];
							} else {
								spNext[0] = NULL;
							}
							spNext += 4;

							if (*(src + 1)==0) {
								spNext[0] = &srcPt[1+TILEMAP_WIDTH];
								spNext[1] = &srcPt[-TILEMAP_LAYER_SIZE+1+TILEMAP_WIDTH];
								spNext[2] = &srcPt[-TILEMAP_LAYER_SIZE+1];
								spNext[3] = &srcPt[1];
							} else {
								spNext[0] = NULL;
							}
							spNext += 4;

							if (y==0 || *(src - TILEMAP_WIDTH)==0) {
								spNext[0] = &srcPt[1];
								spNext[1] = &srcPt[-TILEMAP_LAYER_SIZE+1];
								spNext[2] = &srcPt[-TILEMAP_LAYER_SIZE];
								spNext[3] = &srcPt[0];
							} else {
								spNext[0] = NULL;
							}
							spNext += 4;

							if (*(src + TILEMAP_WIDTH)==0) {
								spNext[0] = &srcPt[TILEMAP_WIDTH];
								spNext[1] = &srcPt[-TILEMAP_LAYER_SIZE+TILEMAP_WIDTH];
								spNext[2] = &srcPt[-TILEMAP_LAYER_SIZE+TILEMAP_WIDTH+1];
								spNext[3] = &srcPt[TILEMAP_WIDTH+1];
							}
							else {
								spNext[0] = NULL;
							}
							spNext += 4;
						} else {
							spNext[0] = NULL; spNext += 4;
							spNext[0] = NULL; spNext += 4;
							spNext[0] = NULL; spNext += 4;
							spNext[0] = NULL; spNext += 4;
						}

						if (i==TILEMAP_LAYERS-1 || *(src + TILEMAP_LAYER_SIZE)==0) {
							spNext[0] = &srcPt[TILEMAP_WIDTH];
							spNext[1] = &srcPt[1+TILEMAP_WIDTH];
							spNext[2] = &srcPt[1];
							spNext[3] = &srcPt[0];
						} else {
							spNext[0] = NULL;
						}
						spNext += 4;

						spNext[0] = NULL;
						spNext += 4;
					}
				}
				srcPt++;
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

static void loadMapTest()
{
	uint8 *src = map1;
	uint8 *csrc = cmap1;
	uint8 *dst = tilemap3d;
	for (int y=0; y<TILEMAP_HEIGHT; ++y) {
		for (int x=0; x<TILEMAP_WIDTH; ++x) {
			uint8 b = *src++;
			uint8 c = *csrc++;
			for (int i=0; i<3; ++i) {
				if (b & (1<<i)) *(dst + (i+1)*TILEMAP_LAYER_SIZE) = c;
			}
			dst++;
		}
	}
}

static void genMapFloor()
{
	uint8 *dst = tilemap3d;
	uint8 n = 3;
	for (int y=0; y<TILEMAP_HEIGHT; ++y) {
		for (int x=0; x<TILEMAP_WIDTH; ++x) {
			uint8 c = 0;
			if (!(x & n) && !(y & n)) c = 1;
			*dst++ = c;
		}
	}
}

static void genMapTest()
{
	uint8 *dst = &tilemap3d[TILEMAP_LAYER_SIZE];
	for (int i=1; i<TILEMAP_LAYERS; ++i) {
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
}

void tilemap3dInit()
{
	memset(tilemap3d, 0, sizeof(tilemap3d));

	genMapFloor();
	//genMapTest();
	loadMapTest();

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

static void drawRectangle(ScreenPoint *p0, ScreenPoint *p1, uint8 color, uint8 *vram)
{
	int x0 = p0->x >> SCR_BITS;
	int y0 = p0->y >> SCR_BITS;
	int x1 = p1->x >> SCR_BITS;
	int y1 = p1->y >> SCR_BITS;

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
	int layerZ = pos->z - TILE_SIZE * layer;
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

// 1080,194
// 1038,192
// 1027,190
// 931,180
// 919,155

static void renderTilemap3DLayerMesh(uint8 layer, uint8 *vram)
{
	const int tileRowOffset = layer * TILEMAP_LAYER_SIZE + tmapGridInfo.y0 * TILEMAP_WIDTH;
	ScreenPoint *tileSp = &tileScrPt[tileRowOffset];

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
			tileSp[x].x = xs;
			tileSp[x].y = ys;
			xs += step;
		}
		ys += step;
		tileSp += TILEMAP_WIDTH;
	}

	uint8 colStart = ((layer+1) * 16) / TILEMAP_LAYERS;
	if (colStart > 15) colStart = 15;

	ScreenPoint ***tileMeshSpPtr = &tileMeshScreenPointPtr[tileRowOffset];
	uint8 *tmap = &tilemap3d[tileRowOffset];
	for (int y=y0; y<y1; ++y) {
		for (int x=x0; x<x1; ++x) {
			ScreenPoint **spQuad = tileMeshSpPtr[x];
			if (spQuad) {
				uint8 color = colStart + (tmap[x] << 4);

				// X
				if (spQuad[0] && spQuad[0]->x > spQuad[1]->x) {
					drawQuad(spQuad, color, vram);
				}
				color--; spQuad+=4;
				if (spQuad[0] && spQuad[0]->x < spQuad[1]->x) {
					drawQuad(spQuad, color, vram);
				}
				color--; spQuad+=4;

				// Y
				if (spQuad[0] && spQuad[0]->y > spQuad[1]->y) {
					drawQuad(spQuad, color, vram);
				}
				color--; spQuad+=4;
				if (spQuad[0] && spQuad[0]->y < spQuad[1]->y) {
					drawQuad(spQuad, color, vram);
				}
			}
		}
		tileMeshSpPtr += TILEMAP_WIDTH;
		tmap += TILEMAP_WIDTH;
	}

	tileMeshSpPtr = &tileMeshScreenPointPtr[tileRowOffset];
	tmap = &tilemap3d[tileRowOffset];
	for (int y=y0; y<y1; ++y) {
		for (int x=x0; x<x1; ++x) {
			ScreenPoint **spQuad = tileMeshSpPtr[x];
			if (spQuad) {
				if (spQuad[16]) {
					uint8 color = colStart;
					if (layer>0) color += (tmap[x] << 4);
					#ifdef FILL_SHAPES_ASM
						drawRectangleAsm(spQuad[16+3], spQuad[16+1], color, vram);
					#else
						drawRectangle(spQuad[16+3], spQuad[16+1], color, vram);
					#endif
				}
			}
		}
		tileMeshSpPtr += TILEMAP_WIDTH;
		tmap += TILEMAP_WIDTH;
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
				ScreenPoint p0, p1;
				p0.x = xs; p1.x = xs+step;
				p0.y = ys; p1.y = ys+step;
				#ifdef FILL_SHAPES_ASM
					drawRectangleAsm(&p0, &p1, color, vram);
				#else
					drawRectangle(&p0, &p1, color, vram);
				#endif
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