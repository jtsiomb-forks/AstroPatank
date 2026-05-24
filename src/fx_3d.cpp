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
#define PPOS_BITS 8

static int8 *objMeshData[NUM_OBJECTS] = {	objQuadData, objTripodData, objPyramidData, objRombusData, objCubeData, objGlenzData, objUfoData, objUfo2Data, objDrumData, objSquareCrossData, 
									objSpaceship1Data, objTorusData, objCubeStarData, objTest3Data, objRombusRingData, objTorus2Data, objEightCubesData };

static Mesh *objectMesh[NUM_OBJECTS];

static int renderMethod = RENDER_POLYS;
static int objectMeshIndex = 10;

static Vec3 playerPos;
static int playerMoveSpeed = 1;
static int playerZoomSpeed = 32;

static void script3D(Mesh *ms, int t)
{
	static int rx = 1024;
	static int ry = 0;
	static int rz = 0;

	Vec3 *rot = &ms->rot;
	rot->x = rx << 0;
    rot->y = ry << 0;
    rot->z = rz << 0;

	ms->pos.x = 0;
	ms->pos.y = 0;
	ms->pos.z = playerPos.z;

	//ry -= 16;

	ms->renderMode = renderMethod;
}

static bool checkPlayerCollision(uint8 *tmap)
{
	const int playerSize = TILE_SIZE / 6;
	int tx0 = (playerPos.x - playerSize) / TILE_SIZE;
	int ty0 = (playerPos.y - playerSize) / TILE_SIZE;
	int tx1 = (playerPos.x + playerSize) / TILE_SIZE;
	int ty1 = (playerPos.y + playerSize) / TILE_SIZE;

	CLAMP(tx0,0,TILEMAP_WIDTH-1)
	CLAMP(tx1,0,TILEMAP_WIDTH-1)
	CLAMP(ty0,0,TILEMAP_HEIGHT-1)
	CLAMP(ty1,0,TILEMAP_HEIGHT-1)

	return (tmap[ty0*TILEMAP_WIDTH+tx0] || tmap[ty0*TILEMAP_WIDTH+tx1] || tmap[ty1*TILEMAP_WIDTH+tx0] || tmap[ty1*TILEMAP_WIDTH+tx1]);
}

static void input3D(int dt)
{
	uint8* tmap = &getTilemap3D()[TILEMAP_LAYER_SIZE];

	//static bool leftPressed = false;
	//static bool rightPressed = false;
	//static bool upPressed = false;
	//static bool downPressed = false;
	
	static bool rPrevPressed = false;
	static bool rNextPressed = false;

	int prevPlayerPosX = playerPos.x;
	int prevPlayerPosY = playerPos.y;


	int tMov = ((dt*playerMoveSpeed) << PPOS_BITS);

	int pposX = playerPos.x << PPOS_BITS;
	int pposY = playerPos.y << PPOS_BITS;

	if (buttonsHeld.left) {
		pposX -= tMov;
	}
	if (buttonsHeld.right) {
		pposX += tMov;
	}
	playerPos.x = pposX >> PPOS_BITS;

	if (checkPlayerCollision(tmap)) {
		playerPos.x = prevPlayerPosX;
	}


	if (buttonsHeld.up) {
		pposY -= tMov;
	}
	if (buttonsHeld.down) {
		pposY += tMov;
	}
	playerPos.y = pposY >> PPOS_BITS;

	if (checkPlayerCollision(tmap)) {
		playerPos.y = prevPlayerPosY;
	}


	if (buttonsHeld.zoomIn) {
		if (playerPos.z > 2048) {
			playerPos.z -= playerZoomSpeed;
		}
	}

	if (buttonsHeld.zoomOut) {
		playerPos.z += playerZoomSpeed;
	}

	if (buttonsHeld.renderPrev & !rPrevPressed) {
		advTileRenderType(false);
	}
	if (buttonsHeld.renderNext & !rNextPressed) {
		advTileRenderType(true);
	}

	rPrevPressed = buttonsHeld.renderPrev;
	rNextPressed = buttonsHeld.renderNext;
	

	//leftPressed = buttonsHeld.left;
	//rightPressed = buttonsHeld.right;
	//upPressed = buttonsHeld.up;
	//downPressed = buttonsHeld.down;
}

static void setupPalette3D()
{
	const int sh1 = 1;
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
	renderTilemap3dLayer(&playerPos, 0, screen);

	Mesh *ms = objectMesh[objectMeshIndex];

	script3D(ms, t);
	renderMesh(ms, screen);

	for (int i=1; i<TILEMAP_LAYERS; ++i) {
		renderTilemap3dLayer(&playerPos, i, screen);
	}
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
			objectMesh[i]->gridScale >>= 3;
			//reversePolygonOrder(objectMesh[i]); // Why did this work on EGA but here we shouldn't be doing it?
		}
	}

	playerPos.x = (TILEMAP_WIDTH / 2) * TILE_SIZE - 3*TILE_SIZE;
	playerPos.y = (TILEMAP_HEIGHT / 2) * TILE_SIZE - 2*TILE_SIZE;
	playerPos.z = 6144;

	tilemap3dInit();

	setupPalette3D();
}

void fx3dRun(Screen *screen, int t)
{
	static int t0 = 0;

	clearScreen(screen);

	input3D(t - t0);

	updateScene3D(screen, t);

	t0 = t;
}
