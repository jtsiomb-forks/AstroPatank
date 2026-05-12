#include <math.h>
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine.h"

#include "mathutil.h"
#include "render.h"
#include "vector.h"

#define NUM_POINTS_GRID_HALF 128
#define NUM_POINTS_AXIS (2*NUM_POINTS_GRID_HALF+1)

#define ANGLE_BITS 12
#define SINTAB_SIZE (1 << ANGLE_BITS)
#define AMPLITUDE_BITS 12
#define SINTAB_AMPLITUDE (1 << AMPLITUDE_BITS)

static int sinTab[SINTAB_SIZE];

static int rotMat[9];

static Vec3 axisX, axisY, axisZ;
static Vec3 gridAxisX[NUM_POINTS_AXIS];
static Vec3 gridAxisY[NUM_POINTS_AXIS];
static Vec3 gridAxisZ[NUM_POINTS_AXIS];

static Vec3 transVertex[MAX_VERTEX_POINTS];
static ScreenPoint scrPoints[MAX_VERTEX_POINTS];

static char scrPolyVis[MAX_POLYS];


#define Z_BUCKET_RANGE 64
#define Z_BUCKETS_NUM (262144 / Z_BUCKET_RANGE)
#define Z_BUCKET_MAX_POLYS 32

typedef struct ZBucket
{
	uint8 count;
	uint8 polyIndex[Z_BUCKET_MAX_POLYS];
	int polyIndexOffset[Z_BUCKET_MAX_POLYS];
} ZBucket;

ZBucket zBucket[Z_BUCKETS_NUM];
static int zBucketIndexMin, zBucketIndexMax;


static void calcRotMatrix(Vec3 *rot)
{
	const int cosxr = sinTab[(rot->x + (SINTAB_SIZE / 4)) & (SINTAB_SIZE - 1)];
	const int cosyr = sinTab[(rot->y + (SINTAB_SIZE / 4)) & (SINTAB_SIZE - 1)];
	const int coszr = sinTab[(rot->z + (SINTAB_SIZE / 4)) & (SINTAB_SIZE - 1)];
	const int sinxr = sinTab[rot->x & (SINTAB_SIZE - 1)];
	const int sinyr = sinTab[rot->y & (SINTAB_SIZE - 1)];
	const int sinzr = sinTab[rot->z & (SINTAB_SIZE - 1)];

	rotMat[0] = ((cosyr * coszr) >> AMPLITUDE_BITS);
	rotMat[1] = ((((sinxr * sinyr) >> AMPLITUDE_BITS) * coszr) >> AMPLITUDE_BITS) - ((cosxr * sinzr) >> AMPLITUDE_BITS);
	rotMat[2] = ((((cosxr * sinyr) >> AMPLITUDE_BITS) * coszr) >> AMPLITUDE_BITS) + ((sinxr * sinzr) >> AMPLITUDE_BITS);
	rotMat[3] = ((cosyr * sinzr) >> AMPLITUDE_BITS);
	rotMat[4] = ((cosxr * coszr) >> AMPLITUDE_BITS) + ((((sinxr * sinyr) >> AMPLITUDE_BITS) * sinzr) >> AMPLITUDE_BITS);
	rotMat[5] = ((-sinxr * coszr) >> AMPLITUDE_BITS) + ((((cosxr * sinyr) >> AMPLITUDE_BITS) * sinzr) >> AMPLITUDE_BITS);
	rotMat[6] = (-sinyr);
	rotMat[7] = ((sinxr * cosyr) >> AMPLITUDE_BITS);
	rotMat[8] = ((cosxr * cosyr) >> AMPLITUDE_BITS);
}

static void genRotAxes(Mesh *ms)
{
	int gridScale = ms->gridScale;

    axisX.x = (rotMat[0] * gridScale) >> AMPLITUDE_BITS;
	axisX.y = (rotMat[1] * gridScale) >> AMPLITUDE_BITS;
	axisX.z = (rotMat[2] * gridScale) >> AMPLITUDE_BITS;

    axisY.x = (rotMat[3] * gridScale) >> AMPLITUDE_BITS;
	axisY.y = (rotMat[4] * gridScale) >> AMPLITUDE_BITS;
	axisY.z = (rotMat[5] * gridScale) >> AMPLITUDE_BITS;

    axisZ.x = (rotMat[6] * gridScale) >> AMPLITUDE_BITS;
	axisZ.y = (rotMat[7] * gridScale) >> AMPLITUDE_BITS;
	axisZ.z = (rotMat[8] * gridScale) >> AMPLITUDE_BITS;
}

static void genRotGridAxes(Mesh *ms)
{
	const int d = ms->gridRange;
	Vec3 b_ax = Vec3(-d * axisX.x, -d * axisX.y, -d * axisX.z);
	Vec3 b_ay = Vec3(-d * axisY.x, -d * axisY.y, -d * axisY.z);
	Vec3 b_az = Vec3(-d * axisZ.x, -d * axisZ.y, -d * axisZ.z);

	int count = 2*d + 1;
	for (int i=0; i < count; ++i) {
		gridAxisX[i] = b_ax;
		gridAxisY[i] = b_ay;
		gridAxisZ[i] = b_az;
		b_ax += axisX;
		b_ay += axisY;
		b_az += axisZ;
	}
}

static void transformGridMesh(Mesh *ms)
{
	int count = ms->numVerts;
	int8 *src = ms->gridPoints;
	Vec3 *dst = transVertex;

    calcRotMatrix(&ms->rot);
    genRotAxes(ms);
	genRotGridAxes(ms);

	Vec3 *vX = &gridAxisX[ms->gridRange];
	Vec3 *vY = &gridAxisY[ms->gridRange];
	Vec3 *vZ = &gridAxisZ[ms->gridRange];
	do {
		Vec3 *gx = &vX[*src++];
		Vec3 *gy = &vY[*src++];
		Vec3 *gz = &vZ[*src++];

		dst->x = gx->x + gy->x + gz->x;
		dst->y = gx->y + gy->y + gz->y;
		dst->z = gx->z + gy->z + gz->z;
		++dst;
	} while(--count != 0);
}

static void transformMesh(Mesh *ms)
{
	int count = ms->numVerts;
	Vec3 *src = ms->vertexPoints;
	Vec3 *dst = transVertex;

    calcRotMatrix(&ms->rot);

	do {
        int xo = src->x;
        int yo = src->y;
        int zo = src->z;
		++src;

		dst->x = ((xo * rotMat[0]) >> VERTEX_BITS) + ((yo * rotMat[3]) >> VERTEX_BITS) + ((zo * rotMat[6]) >> VERTEX_BITS);
		dst->y = ((xo * rotMat[1]) >> VERTEX_BITS) + ((yo * rotMat[4]) >> VERTEX_BITS) + ((zo * rotMat[7]) >> VERTEX_BITS);
		dst->z = ((xo * rotMat[2]) >> VERTEX_BITS) + ((yo * rotMat[5]) >> VERTEX_BITS) + ((zo * rotMat[8]) >> VERTEX_BITS);
		++dst;
	} while(--count != 0);
}

static void translateAndProjectMesh(Mesh *ms)
{
	const int posX = ms->pos.x;
	const int posY = ms->pos.y;
	const int posZ = ms->pos.z;

	int count = ms->numVerts;
	Vec3 *src = transVertex;
	ScreenPoint *dst = scrPoints;

    do {
		int z = src->z + posZ;

		if (z > 0) {
            const int x = src->x + posX;
            const int y = src->y + posY;

			int xp = (x << (PROJ_BITS + SCR_BITS)) / z + (SCR_W << SCR_BITS) / 2;
			int yp = (SCR_H << SCR_BITS) / 2 - (y << (PROJ_BITS + SCR_BITS)) / z;

			// Commented that out, it's proper, for many cube in grid to work although we are not gonna use it. Seems proper polygon clipping works there, no need to exclude polygons here
			// It's not proper precise clipping with special cases anyway (I've done it on 3DO no need here for now)

			// We want center subpixel? Can change it later here globally, not everywhere else on the renderers
			dst->x = (xp + SCR_RANGE / 2);
			dst->y = (yp + SCR_RANGE / 2);

			if (xp < 0 || xp >= ((SCR_W-1) << SCR_BITS) || yp < 0 || yp >= ((SCR_H-1) << SCR_BITS)) {
				z = 0;	// z<=0 denotes skip this point later for the renderer
			}
		}
		dst->z = z;
		++src;
		++dst;
	} while(--count != 0);
}

static void renderSortedMeshPolys(Mesh *ms, uint8 *vram)
{
	static ScreenPoint *p[16];
	static uint8 edgeAdjacentPolysNum[16];

	for (int j=zBucketIndexMax; j>=zBucketIndexMin; --j) {
		ZBucket *zb = &zBucket[j];
		int *polyIndexOffsetPtr = zb->polyIndexOffset;
		uint8 *polyIndexPtr = zb->polyIndex;

		uint8 count = zb->count;
		for (uint8 i=0; i<count; ++i) {
			int polyIndexOffset = *polyIndexOffsetPtr++;
			uint8 polyIndex = *polyIndexPtr++;

			int *edgeData = &ms->polyIndices[polyIndexOffset];
			int edgesNum = *edgeData++;
			if (edgesNum < 3) continue;	// something wrong, skip

			for (int n=0; n<edgesNum; ++n) {
				p[n] = &scrPoints[*edgeData++];
				if (p[n]->z <= 0) continue;	// one point negative z? Not handling clip with near yet..
			}

			uint8 color = ms->polyColor[polyIndex] - 1;

			#ifndef ANTIALIASING_POLY
				drawPoly(p, edgesNum, color, vram);
			#else
				int *edgeToAdjacentPoly = &ms->edgeToPolyIndices[2*(polyIndexOffset - polyIndex)];
				for (int n=0; n<edgesNum; ++n) {
					int p0 = *edgeToAdjacentPoly++;
					int p1 = *edgeToAdjacentPoly++;

					uint8 adjacentPolys = 0;
					if (scrPolyVis[p0]!=0) ++adjacentPolys;
					if (scrPolyVis[p1]!=0) ++adjacentPolys;
					edgeAdjacentPolysNum[n] = adjacentPolys;
				}
				drawPolyAntialiased(p, edgeAdjacentPolysNum, edgesNum, color, vram);
			#endif
		}
	}
}

static void updateFaceOrderPolysVis(Mesh *ms)
{
	int *src = ms->polyIndices;
	const int count = ms->numPolys;
	for (int i=0; i<count; ++i) {
		int polyIsVis = 0;

		int edgesNum = *src++;
		if (edgesNum >= 3) {
			ScreenPoint *p0 = &scrPoints[src[0]];
			ScreenPoint *p1 = &scrPoints[src[1]];
			ScreenPoint *p2 = &scrPoints[src[2]];

			int x0 = p0->x >> SCR_BITS;
			int y0 = p0->y >> SCR_BITS;
			int x1 = p1->x >> SCR_BITS;
			int y1 = p1->y >> SCR_BITS;
			int x2 = p2->x >> SCR_BITS;
			int y2 = p2->y >> SCR_BITS;

			int faceOrder = (x0 - x1) * (y2 - y1) - (x2 - x1) * (y0 - y1);
			if (faceOrder >= 0) {
				polyIsVis = 1;
			}
		}
		scrPolyVis[i] = polyIsVis;

		src += edgesNum;
	}
}

static void renderMeshPolys(Mesh *ms, uint8 *vram)
{
	updateFaceOrderPolysVis(ms);

	// clear all entries from previously
	for (int i=zBucketIndexMin; i<=zBucketIndexMax; ++i) {
		zBucket[i].count = 0;
	}
	zBucketIndexMin = Z_BUCKETS_NUM;
	zBucketIndexMax = 0;

	int *src = ms->polyIndices;
	int polyIndexBase = 0;

	const int count = ms->numPolys;
	for (int i=0; i<count; ++i) {
		int edgesNum = *src++;
		if (edgesNum >= 3) {
			ScreenPoint *p0 = &scrPoints[src[0]];
			ScreenPoint *p1 = &scrPoints[src[1]];
			ScreenPoint *p2 = &scrPoints[src[2]];

			if (scrPolyVis[i] != 0) {
				const int p0z = scrPoints[src[0]].z;
				const int p1z = scrPoints[src[1]].z;
				const int p2z = scrPoints[src[2]].z;

				int avgZ = (p0z + p1z + 2 * p2z) >> 2; // adding p2 twice as hack, enough for crude innacurate z average

				if (avgZ < 0) avgZ = 0;

				// find bucket index
				int zBucketIndex = avgZ / Z_BUCKET_RANGE;
				if (zBucketIndex < zBucketIndexMin) zBucketIndexMin = zBucketIndex;
				if (zBucketIndex > zBucketIndexMax) zBucketIndexMax = zBucketIndex;

				// add it
				ZBucket *zb = &zBucket[zBucketIndex];
				const int zbCount = zb->count;
				if (zbCount < Z_BUCKET_MAX_POLYS) {
					zb->polyIndex[zbCount] = i;
					zb->polyIndexOffset[zbCount] = polyIndexBase;
					zb->count++;
				}
			}
		}

		src += edgesNum;
		polyIndexBase += edgesNum + 1;
	}

	renderSortedMeshPolys(ms, vram);
}

static void renderMeshLines(Mesh *ms, uint8 *vram)
{
	updateFaceOrderPolysVis(ms);

	int *src = ms->lineIndices;
	int *srcNearPolys = ms->lineToPolyIndices;

	int count = ms->numLines;
    do {
		int i0 = *srcNearPolys++;
		int i1 = *srcNearPolys++;
		if (scrPolyVis[i0]!=0 || scrPolyVis[i1]!=0) {
			ScreenPoint *p0 = &scrPoints[*src];
			ScreenPoint *p1 = &scrPoints[*(src+1)];
			uint8 color = count & 7;

			if (p0->z > 0 && p1->z > 0) {
				#ifndef ANTIALIASING
					drawLine(p0, p1, color, vram);
				#else
					drawLineAntialiased(p0, p1, color, vram);
				#endif
			}
		}
		src += 2;
	} while(--count != 0);
}

static void renderMeshDots(Mesh *ms, uint8 *vram)
{
	ScreenPoint *src = scrPoints;
	const int colorBase = 240;
	const int color = colorBase + 15;

	int count = ms->numVerts;
    do {
		int z = src->z;
		if (z > 0) {
			// Should it be centered pixel or upper left corner?
			const int sx = src->x;
			const int sy = src->y;

            const int xp = sx >> SCR_BITS;
            const int yp = sy >> SCR_BITS;

			uint8 *dst = vram + VRAM_PIXEL_OFFSET((xp>>UNCHAINED_BITS),yp);
			#ifndef ANTIALIASING
				*dst = color;
			#else
				const int fracX1 = (sx & AND_BITS(SCR_BITS + UNCHAINED_BITS)) >> (SCR_BITS + UNCHAINED_BITS - SHADE_BITS);
				const int fracY1 = (sy & AND_BITS(SCR_BITS + UNCHAINED_BITS)) >> (SCR_BITS + UNCHAINED_BITS - SHADE_BITS);
				const int fracX0 = SHADE_AND - fracX1;
				const int fracY0 = SHADE_AND - fracY1;

				*dst = colorBase + ((fracX0 * fracY0) >> SHADE_BITS);
				if (xp < SCR_W - 1) {
					*(dst+1) = colorBase + ((fracX1 * fracY0) >> SHADE_BITS);
				}
				if (yp < SCR_H - 1) {
					dst += SCR_LINE_BYTES;
					*dst = colorBase + ((fracX0 * fracY1) >> SHADE_BITS);
					if (xp < SCR_W - 1) {
						*(dst + 1) = colorBase + ((fracX1 * fracY1) >> SHADE_BITS);
					}
				}
			#endif
		}
		++src;
	} while(--count != 0);
}

void renderMeshHack(Mesh *ms, Screen *screen, bool onlyTransform)
{
	if (onlyTransform) {
		transformGridMesh(ms);
	} else {
		translateAndProjectMesh(ms);
		renderMeshPolys(ms, (uint8*)screen->data);
	}
}

void renderMesh(Mesh *ms, Screen *screen)
{
	uint8 *vram = (uint8*)screen->data;

	if (ms->gridPoints) {
		transformGridMesh(ms);
	} else if (ms->vertexPoints) {
		transformMesh(ms);
	} else {
		return;	// something wrong
	}

	translateAndProjectMesh(ms);

	switch (ms->renderMode) {
		case RENDER_DOTS:
			renderMeshDots(ms, vram);
		break;

		case RENDER_LINES:
			renderMeshLines(ms, vram);
		break;

		case RENDER_POLYS:
			renderMeshPolys(ms, vram);
		break;

		default:
		break;
	}
}

void initEngine()
{
	initSinTab(SINTAB_SIZE, 1, SINTAB_AMPLITUDE, sinTab);

	zBucketIndexMin = 0;
	zBucketIndexMax = Z_BUCKETS_NUM;
}
