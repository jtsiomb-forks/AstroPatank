#include <math.h>
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "types.h"
#include "mathutil.h"

#include "game.h"
#include "tile_3d.h"
#include "video.h"
#include "input.h"
#include "sound.h"
#include "musplay.h"
#include "mathutil.h"
#include "vector.h"
#include "engine.h"
#include "render.h"
#include "gfxtools.h"
#include "mesh.h"

#include "meshdata.h"


#define NUM_OBJECTS 17
#define PPOS_BITS 8

enum {
	OBJ_QUAD, OBJ_TRIPOD, OBJ_PYRAMID, OBJ_ROMBUS, OBJ_CUBE, 
	OBJ_GLENZ, OBJ_UFO, OBJ_UFO2,
	OBJ_DRUM, OBJ_CROSS,
	OBJ_SPACESHIP,
	OBJ_TORUS, OBJ_CUBESTAR,
	OBJ_TEST3, OBJ_ROMBUS_RING, OBJ_TORUS2, OBJ_EIGHT_CUBES
};

static int8 *objMeshData[NUM_OBJECTS] = {	objQuadData, objTripodData, objPyramidData, objRombusData, objCubeData, objGlenzData, objUfoData, objUfo2Data, objDrumData, objSquareCrossData, 
									objSpaceship1Data, objTorusData, objCubeStarData, objTest3Data, objRombusRingData, objTorus2Data, objEightCubesData };

static Mesh *objectMesh[NUM_OBJECTS];

static int renderMethod = RENDER_POLYS;

static Vec3 playerPos;
static int playerAngle = 0;
static int playerThrustX = 0;
static int playerThrustY = 0;
static int playerMoveSpeed = 4;
static int playerAngleSpeed = 2;
static int playerZoomSpeed = 4;

static int soundDuration = 32;


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

#define THRUST_BITS 6
#define THRUST_MAX (1 << THRUST_BITS)

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

	int tAng = (dt*playerAngleSpeed) << PPOS_BITS;
	int tZoom = (dt*playerZoomSpeed) << PPOS_BITS;

	int pposX = playerPos.x << PPOS_BITS;
	int pposY = playerPos.y << PPOS_BITS;
	int pposZ = playerPos.z << PPOS_BITS;
	int pAngle = playerAngle << PPOS_BITS;

	if (buttonsHeld.down) {
		tAng = -tAng;
	}
	
	if (buttonsHeld.left) {
		pAngle += tAng;
	}
	if (buttonsHeld.right) {
		pAngle -= tAng;
	}
	playerAngle = pAngle >> PPOS_BITS;

	if (buttonsHeld.up) {
		if (playerThrustX < THRUST_MAX) {
			playerThrustX++;
		}
		if (playerThrustY < THRUST_MAX) {
			playerThrustY++;
		}
	} else if (buttonsHeld.down) {
		if (playerThrustX > -(THRUST_MAX / 2)) {
			playerThrustX--;
		}
		if (playerThrustY > -(THRUST_MAX / 2)) {
			playerThrustY--;
		}
	} else {
		if (playerThrustX < 0) {
			playerThrustX++;
		} else if (playerThrustX > 0) {
			playerThrustX--;
		}

		if (playerThrustY < 0) {
			playerThrustY++;
		} else if (playerThrustY > 0) {
			playerThrustY--;
		}
	}

	if (playerThrustX != 0) {
		int tMov = (dt*playerMoveSpeed*playerThrustX) << (PPOS_BITS - THRUST_BITS);
		int tMovX = (tMov * sinTab[playerAngle & (SINTAB_SIZE - 1)]) >> AMPLITUDE_BITS;

		pposX -= tMovX;
	}

	if (playerThrustY != 0) {
		int tMov = (dt*playerMoveSpeed*playerThrustY) << (PPOS_BITS - THRUST_BITS);
		int tMovY = (tMov * sinTab[(playerAngle - (SINTAB_SIZE / 4)) & (SINTAB_SIZE - 1)]) >> AMPLITUDE_BITS;

		pposY += tMovY;
	}

	playerPos.x = pposX >> PPOS_BITS;
	if (checkPlayerCollision(tmap)) {
		playerPos.x = prevPlayerPosX;
		playerThrustX = -(playerThrustX * 12) >> 4;
	}

	playerPos.y = pposY >> PPOS_BITS;
	if (checkPlayerCollision(tmap)) {
		playerPos.y = prevPlayerPosY;
		playerThrustY = -(playerThrustY * 12) >> 4;
	}


	if (buttonsHeld.zoomIn) {
		if (pposZ > (2048 << PPOS_BITS)) {
			pposZ -= tZoom;
		}
	}

	if (buttonsHeld.zoomOut) {
		pposZ += tZoom;
	}
	playerPos.z = pposZ >> PPOS_BITS;

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

static void scriptSpaceship3D(Mesh *ms)
{
	Vec3 *rot = &ms->rot;
	rot->x = 1024;
    rot->y = playerAngle;
    rot->z = 0;

	ms->pos.x = 0;
	ms->pos.y = 0;
	ms->pos.z = playerPos.z;

	ms->renderMode = renderMethod;
}

static void scriptCube3D(Mesh *ms, int t)
{
	Vec3 *rot = &ms->rot;
	rot->x = t;
    rot->y = 2*t;
    rot->z = 3*t;

	int vx = sinTab[playerAngle & (SINTAB_SIZE - 1)];
	int vy = sinTab[(playerAngle - (SINTAB_SIZE / 4)) & (SINTAB_SIZE - 1)];

	ms->pos.x = (vx * (8 + playerThrustX)) >> (4 + THRUST_BITS);
	ms->pos.y = (vy * (8 + playerThrustY)) >> (4 + THRUST_BITS);
	ms->pos.z = playerPos.z;

	ms->renderMode = renderMethod;
}

static void updateScene3D(Screen *screen, int t)
{
	Mesh *ms;

	renderTilemap3dLayer(&playerPos, 0, screen);

	ms = objectMesh[OBJ_TRIPOD];
	scriptCube3D(ms, t);
	renderMesh(ms, screen);

	ms = objectMesh[OBJ_SPACESHIP];
	scriptSpaceship3D(ms);
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

//#define SHOW_PALETTE

static void drawPalette(uint8 *vram)
{
	for (int y=0; y<16; ++y) {
		for (int x=0; x<256; ++x) {
			*(vram + x) = x;
		}
		vram += SCR_LINE_BYTES;
	}
}

static void soundRun()
{
	if (soundDuration-- > 0) {
		soundTest();
	} else {
		stopSoundTest();
	}
}

void gameInit()
{
	initEngine();

	for (int i=0; i<NUM_OBJECTS; ++i) {
		objectMesh[i] = initMeshFromCPCdata(objMeshData[i]);
		objectMesh[i]->gridScale >>= 3;
		//reversePolygonOrder(objectMesh[i]); // Why did this work on EGA but here we shouldn't be doing it?
	}

	playerPos.x = (TILEMAP_WIDTH / 2) * TILE_SIZE - 3*TILE_SIZE;
	playerPos.y = (TILEMAP_HEIGHT / 2) * TILE_SIZE - 2*TILE_SIZE;
	playerPos.z = 2048+1024;

	tilemap3dInit();

	setupPalette3D();

	runMusPlayTest();
}

void gameRun(Screen *screen, int t)
{
	static int t0 = 0;

	clearScreen(screen);

	input3D(t - t0);

	updateScene3D(screen, t);

	//soundRun();

#ifdef SHOW_PALETTE
	drawPalette((uint8*)screen->data);
#endif

	t0 = t;
}
