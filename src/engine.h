#ifndef ENGINE_H
#define ENGINE_H

#include "types.h"
#include "demo.h"
#include "mesh.h"

#define MAX_VERTEX_POINTS 4096
#define MAX_POLYS 1024

#define RANGE_BITS(b) (1 << (b))
#define AND_BITS(b) (RANGE_BITS(b) - 1)

#define VERTEX_BITS 16
#define PROJ_BITS 8

#define SCR_BITS 8
#define SCR_RANGE RANGE_BITS(SCR_BITS)
#define SCR_AND AND_BITS(SCR_BITS)

#define SHADE_BITS 4
#define SHADE_RANGE RANGE_BITS(SHADE_BITS)
#define SHADE_AND AND_BITS(SHADE_BITS)


enum {
	RENDER_DOTS,
	RENDER_LINES,
	RENDER_POLYS,
	RENDER_METHODS
};

typedef struct ScreenPoint
{
	int x,y,z;
} ScreenPoint;


void initEngine();
void renderMesh(Mesh *ms, Screen *screen);

void renderMeshHack(Mesh *ms, Screen *screen, bool onlyTransform);	// hack to only transform same repeating cube grid points, so that later for each cube we use the same transformed points but only trans/project

#endif
