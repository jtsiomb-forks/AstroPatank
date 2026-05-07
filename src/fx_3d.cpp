#include <math.h>
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "types.h"
#include "mathutil.h"

#include "demo.h"
#include "tile_3d.h"
#include "video.h"
#include "input.h"
#include "vector.h"
#include "engine.h"
#include "render.h"
#include "gfxtools.h"
#include "mesh.h"

#include "meshdata.h"


#define NUM_OBJECTS 17

int8 *objMeshData[NUM_OBJECTS] = {	objQuadData, objTripodData, objPyramidData, objRombusData, objCubeData, objGlenzData, objUfoData, objUfo2Data, objDrumData, objSquareCrossData, 
									objSpaceship1Data, objTorusData, objCubeStarData, objTest3Data, objRombusRingData, objTorus2Data, objEightCubesData };

static Mesh *objectMesh[NUM_OBJECTS];

static int renderMethod = RENDER_POLYS;
static int objectMeshIndex = 4;

static Vec3 playerPos;
static int playerSpeed = 8;

static void script3D(Mesh *ms, int t)
{
	static int runAndStop = 0;
	static int rx = 0;
	static int ry = 0;
	static int rz = 0;

	Vec3 *rot = &ms->rot;
	rot->x = rx << 0;
    rot->y = ry << 0;
    rot->z = rz << 0;

	ms->pos.x = 0;
	ms->pos.y = 0;
	ms->pos.z = playerPos.z;

	if (runAndStop < 96) {
		rx += 16;
		ry += 14;
		rz -= 18;
		//runAndStop++;
	}

	/*t >>= 2;
	rx = t;
	ry = 2*t;
	rz = 3*t;*/

	//playerPos.x = (TILEMAP_WIDTH / 2) * TILE_SIZE + sin((float)t / 2150.0f) * (4 * TILE_SIZE);
	//playerPos.y = (TILEMAP_HEIGHT / 2) * TILE_SIZE + sin((float)t / 1610.0f) * (3 * TILE_SIZE);
	//playerPos.z = 1024;

	ms->renderMode = renderMethod;
}

static void input3D()
{
	//static bool leftPressed = false;
	//static bool rightPressed = false;
	//static bool upPressed = false;
	//static bool downPressed = false;

	if (buttonsHeld.left) {// & !leftPressed) {
		playerPos.x -= playerSpeed;
	}
	if (buttonsHeld.right) {// & !rightPressed) {
		playerPos.x += playerSpeed;
	}
	if (buttonsHeld.up) {// & !upPressed) {
		playerPos.y -= playerSpeed;
	}
	if (buttonsHeld.down) {// & !downPressed) {
		playerPos.y += playerSpeed;
	}

	if (buttonsHeld.zoomIn) {
		playerPos.z -= 4*playerSpeed;
	}

	if (buttonsHeld.zoomOut) {
		playerPos.z += 4*playerSpeed;
	}

	//leftPressed = buttonsHeld.left;
	//rightPressed = buttonsHeld.right;
	//upPressed = buttonsHeld.up;
	//downPressed = buttonsHeld.down;
}

static void setupPalette3D()
{
	const int sh1 = 3;
	const int sh2 = 5;
	int i = 0;
	for (int r=1; r<3; ++r) {
		for (int g=1; g<3; ++g) {
			for (int b=1; b<3; ++b) {
				const int c0 = i << 4;
				const int c1 = ((i+1) << 4) - 1;

				const int r0 = (r << sh1) - 1; int r1 = (r << sh2) - 1;
				const int g0 = (g << sh1) - 1; int g1 = (g << sh2) - 1;
				const int b0 = (b << sh1) - 1; int b1 = (b << sh2) - 1;

				makeAndSetPal(c0,c1, r0,g0,b0, r1,g1,b1);
				++i;
			}
		}
	}
	makeAndSetPal(0,15, 0,0,0, 63,47,31);
	makeAndSetPal(240,255, 0,0,0, 63,63,63);
}

static void updateScene3D(Screen *screen, int t)
{
	Mesh *ms = objectMesh[objectMeshIndex];

	renderTilemap3d(&playerPos, screen);

	script3D(ms, t);
	renderMesh(ms, screen);
}

static void clearScreen(Screen *screen)
{
	const int screenSize = (screen->width * screen->height * screen->bpp) >> (3 + UNCHAINED_BITS);
	memset(screen->data, 0, screenSize);
}

void fx3dInit(bool onlySetup)
{
	if (!onlySetup) {
		initEngine();

		for (int i=0; i<NUM_OBJECTS; ++i) {
			objectMesh[i] = initMeshFromCPCdata(objMeshData[i]);
			//reversePolygonOrder(objectMesh[i]); // Why did this work on EGA but here we shouldn't be doing it?
		}
	}

	playerPos.x = (TILEMAP_WIDTH / 2) * TILE_SIZE;
	playerPos.y = (TILEMAP_HEIGHT / 2) * TILE_SIZE;
	playerPos.z = 1024;

	tilemap3dInit();

	setupPalette3D();
}

void fx3dRun(Screen *screen, int t)
{
	clearScreen(screen);

	input3D();

	updateScene3D(screen, t);
}
