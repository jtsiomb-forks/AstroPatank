#include "mesh.h"

#include <math.h>
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "input.h"
#include "render.h"
#include "engine.h"
#include "mathutil.h"

#include "meshdata.h"

#define VEC_SIZE (1 << 8)

#define DEFAULT_CPC_GRID_RANGE 4
#define DEFAULT_CPC_GRID_SCALE 256

static void prepareEdgeToPolyIndices(Mesh *ms)
{
	const int numLines = ms->numLines;

	int *src = ms->polyIndices;
	int *dst = ms->edgeToPolyIndices;

	const int numPolys = ms->numPolys;	
	for (int j=0; j<numPolys; ++j) {
		int vCount = *src++;

		for (int n=0; n<vCount; ++n) {
			int v0 = src[n];
			int v1 = src[(n+1) % vCount];

			int *srcLineIndices = ms->lineIndices;
			for (int i=0; i<numLines; ++i) {
				int i0 = *srcLineIndices++;
				int i1 = *srcLineIndices++;
				if ((i0==v0 && i1==v1) || (i0==v1 && i1==v0)) {
					int *adjacentPolys = &ms->lineToPolyIndices[2*i];
					memcpy(dst, adjacentPolys, 2 * sizeof(int));
				}
			}
			dst += 2;
		}
		src += vCount;
	}
}

static void prepareLineToPolyIndices(Mesh *ms)
{
	int *src = ms->lineIndices;
	int *dst = ms->lineToPolyIndices;

	const int numLines = ms->numLines;
	const int numPolys = ms->numPolys;

	for (int i=0; i<numLines; ++i) {
		int v0 = *src++;
		int v1 = *src++;

		int *srcPolyData = ms->polyIndices;
		int pCount = 0;
		int pFound = 0;
		while(pCount < numPolys) {
			int vCount = *srcPolyData++;
			int found = 0;
			for (int n=0; n<vCount; ++n) {
				int vIndex = *srcPolyData++;
				if (vIndex==v0 || vIndex==v1) {
					found++;
				}
			}
			if (found==2) {
				if (++pFound <= 2) {
					*dst++ = pCount;
				}
			}
			pCount++;
		}
	}
}

static void generateLinesFromPolys(Mesh *ms)
{
	int *tempLineIndices = (int*)malloc(MAX_POLYS * 8 * sizeof(int));
	int tempLineNext = 0;

	const int numPolys = ms->numPolys;	
	int *srcPolyData = ms->polyIndices;

	for (int i=0; i<numPolys; ++i) {
		int vCount = *srcPolyData++;
		for (int n=0; n<vCount; ++n) {
			int v0 = srcPolyData[n];
			int v1 = srcPolyData[(n+1) % vCount];

			bool found = false;
			for (int j=0; j<tempLineNext; j+=2) {
				int i0 = tempLineIndices[j];
				int i1 = tempLineIndices[j+1];
				if ((i0==v0 && i1==v1) || (i0==v1 && i1==v0)) {
					found = true;
					break;
				}
			}
			if (!found) {
				tempLineIndices[tempLineNext++] = v0;
				tempLineIndices[tempLineNext++] = v1;
			}
		}
		srcPolyData += vCount;
	}

	ms->numLines = tempLineNext / 2;
	ms->lineIndices = (int*)malloc(tempLineNext * sizeof(int));
	ms->lineToPolyIndices = (int*)malloc(2 * ms->numLines * sizeof(int));	// assuming a line will always connect 2 polygons, no less, no more.
	memcpy(ms->lineIndices, tempLineIndices, tempLineNext * sizeof(int));

	free(tempLineIndices);
}

static int getElementsSizeForCPCpolyData(int8 *data, int numPolys)
{
	int numElements = 0;
	for (int i=0; i<numPolys; ++i) {
		int edges = *data++;
		numElements += edges + 1;
		data += edges;
	}
	return numElements;
}

Mesh* initMeshFromCPCdata(int8 *data)
{
	Mesh *ms = (Mesh*)malloc(sizeof(Mesh));

	int elementsSize;
	int numVerts = *data++;
	int numLines = *data++;
	int numPolys = *data++;

	if (numVerts<=0) return NULL;	// something wrong?

	ms->numVerts = numVerts;
	ms->numLines = numLines;
	ms->numPolys = numPolys;

	ms->pointColor = (uint8*)malloc(numVerts);

	elementsSize = numVerts * 3;
	ms->gridPoints = (int8*)malloc(elementsSize);
	memcpy(ms->gridPoints, data, elementsSize);
	data += elementsSize;

	ms->vertexPoints = NULL;	// only grid points for the CPC objects

	elementsSize = 2 * numLines;
	ms->lineIndices = (int*)malloc(elementsSize * sizeof(int));
	for (int i=0; i<elementsSize; ++i) {
		ms->lineIndices[i] = (int)*data++;
	}
	ms->lineColor = (uint8*)malloc(numLines);
	ms->lineToPolyIndices = (int*)malloc(2 * numLines * sizeof(int));	// assuming a line will always connect 2 polygons, no less, no more.

	elementsSize = getElementsSizeForCPCpolyData(data, numPolys);
	ms->polyIndices = (int*)malloc(elementsSize * sizeof(int));
	for (int i=0; i<elementsSize; ++i) {
		ms->polyIndices[i] = (int)*data++;
	}

	ms->edgeToPolyIndices = (int*)malloc(2 * (elementsSize - numPolys) * sizeof(int));

	ms->polyColor = (uint8*)malloc(numPolys);
	memcpy(ms->polyColor, data, numPolys);

	ms->pos = Vec3(0,0,0);
	ms->rot = Vec3(0,0,0);

	ms->renderMode = RENDER_POLYS;
	ms->gridRange = DEFAULT_CPC_GRID_RANGE;
	ms->gridScale = DEFAULT_CPC_GRID_SCALE;

	if (numLines==1) {
		free(ms->lineIndices);
		free(ms->lineToPolyIndices);
		generateLinesFromPolys(ms);
	}

	prepareLineToPolyIndices(ms);
	prepareEdgeToPolyIndices(ms);

	return ms;
}

void reversePolygonOrder(Mesh *ms)
{
	int count = ms->numPolys;
	int *src = ms->polyIndices;

	do {
		uint8 edgesNum = (uint8)*src++;
		uint8 halfEdgesNum = edgesNum / 2;
		for (uint8 i=0; i<halfEdgesNum; ++i) {
			int *elementL = src + i;
			int *elementR = src + edgesNum - 1 - i;
			int temp = *elementL;
			*elementL = *elementR;
			*elementR = temp;
		}
		src += edgesNum;
	} while(--count !=0);
}

Mesh* initMesh(int numVerts, int numLines, int numTriangles, int numQuads, bool gridBased, int gridRange, int gridScale)
{
	Mesh *ms = (Mesh*)malloc(sizeof(Mesh));
	int numPolys = numTriangles * numQuads;

	if (numVerts<=0) return NULL;	// you can't have mesh without points

	ms->numVerts = numVerts;
	ms->numLines = numLines;
	ms->numPolys = numPolys;

	ms->pointColor = (uint8*)malloc(numVerts);

	if (gridBased) {
		ms->gridPoints = (int8*)malloc(numVerts * 3);
		ms->vertexPoints = NULL;
	} else {
		ms->gridPoints = NULL;
		ms->vertexPoints = (Vec3*)malloc(numVerts * sizeof(Vec3));
	}

	if (numLines > 0) {
		ms->lineIndices = (int*)malloc(2 * numLines * sizeof(int));
		ms->lineColor = (uint8*)malloc(numLines);
		ms->lineToPolyIndices = (int*)malloc(2 * numLines * sizeof(int));	// assuming a line will always connect 2 polygons, no less, no more.
	} else {
		ms->lineIndices = NULL;
		ms->lineColor =  NULL;
	}

	if (numPolys > 0) {
		const int numElements = ((3+1) * numTriangles + (4+1) * numQuads);
		ms->polyIndices = (int*)malloc(numElements * sizeof(int));	// n+1 to store poly edges num
		ms->polyColor = (uint8*)malloc(numPolys);
		ms->edgeToPolyIndices = (int*)malloc(2 * (numElements - numPolys) * sizeof(int));
		if (numLines > 0) {
			prepareLineToPolyIndices(ms);
			prepareEdgeToPolyIndices(ms);
		}
	} else {
		ms->polyIndices = NULL;
		ms->polyColor = NULL;
	}

	ms->renderMode = RENDER_POLYS;
	ms->gridRange = gridRange;
	ms->gridScale = gridScale;

	return ms;
}
