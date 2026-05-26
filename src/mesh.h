#ifndef MESH_H
#define MESH_H

#include "types.h"
#include "game.h"
#include "vector.h"

typedef struct Mesh
{
	int numVerts, numLines, numPolys;

	int8 *gridPoints;	// x,y,z grid position
	Vec3 *vertexPoints; // x,y,z vertex position
	int *lineIndices;
	int *polyIndices;

	int *lineToPolyIndices;
	int *edgeToPolyIndices;

	uint8 *pointColor;
	uint8 *lineColor;
	uint8 *polyColor;

	int renderMode;
	int gridScale;
	int gridRange;

	Vec3 pos, rot;
}Mesh;


Mesh* initMesh(int numVerts, int numLines, int numTriangles, int numQuads, bool gridBased, int gridRange, int gridScale);
Mesh* initMeshFromCPCdata(int8 *data);
void reversePolygonOrder(Mesh *ms);

#endif
