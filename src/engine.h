#ifndef ENGINE_H
#define ENGINE_H

#include "types.h"
#include "game.h"
#include "mesh.h"

#define Z_NEAR 32

#define MAX_VERTEX_POINTS 128
#define MAX_POLYS 64

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

#define SHADE_ALPHA_BITS 6
#define SHADE_ALPHA_MAX (1 << SHADE_ALPHA_BITS)


#define ANGLE_BITS 12
#define SINTAB_SIZE (1 << ANGLE_BITS)
#define AMPLITUDE_BITS 12
#define SINTAB_AMPLITUDE (1 << AMPLITUDE_BITS)


enum {
	RENDER_DOTS,
	RENDER_LINES,
	RENDER_POLYS,
	RENDER_METHODS
};

typedef struct ScreenPoint2D
{
	int x,y;
} ScreenPoint2D;

typedef struct ScreenPoint
{
	int x,y,z;
} ScreenPoint;


extern int sinTab[SINTAB_SIZE];

void initEngine();
void renderMesh(Mesh *ms, Screen *screen);

void renderAntialiasedDot(int sx, int sy, int colorBase, int shadeAlpha, uint8 *vram);

void renderMeshHack(Mesh *ms, Screen *screen, bool onlyTransform);	// hack to only transform same repeating cube grid points, so that later for each cube we use the same transformed points but only trans/project

#endif
