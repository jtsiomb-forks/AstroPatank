#include <math.h>
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "types.h"
#include "mathutil.h"

#include "game.h"
#include "fonts.h"
#include "menu.h"
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

#define GROUND_Z 3072

#define PPOS_BITS 8

#define NUM_THINGS 64

typedef struct GameThing
{
	Vec3 pos, rot;
	Mesh *mesh;
	bool alive;
} GameThing;

static GameThing thing[NUM_THINGS];

enum {
	OBJ_QUAD, OBJ_TRIPOD, OBJ_PYRAMID, OBJ_ROMBUS, OBJ_CUBE, 
	OBJ_GLENZ, OBJ_UFO, OBJ_UFO2,
	OBJ_DRUM, OBJ_CROSS,
	OBJ_SPACESHIP,
	OBJ_TORUS, OBJ_CUBESTAR,
	OBJ_TEST3, OBJ_ROMBUS_RING, OBJ_TORUS2, OBJ_EIGHT_CUBES,
	OBJ_LASER,
	OBJ_LETTER_A,
	OBJ_LETTER_S,
	OBJ_LETTER_T,
	OBJ_LETTER_R,
	OBJ_LETTER_O,
	OBJ_LETTER_P,
	OBJ_LETTER_N,
	OBJ_LETTER_K,
	NUM_MESHES
};

static int8 *objMeshData[NUM_MESHES] =	{ 	objQuadData, objTripodData, objPyramidData, objRombusData, objCubeData, objGlenzData, objUfoData, objUfo2Data, objDrumData, objSquareCrossData, 
											objSpaceship1Data, objTorusData, objCubeStarData, objTest3Data, objRombusRingData, objTorus2Data, objEightCubesData, 
											objLaserData, 
											objLetterAData, objLetterSData, objLetterTData, objLetterRData, objLetterOData, objLetterPData, objLetterNData, objLetterKData
										};

static Mesh *objectMesh[NUM_MESHES];

static int objsInLayer[TILEMAP_LAYERS+1][NUM_THINGS];
static int layerObjCount[TILEMAP_LAYERS+1];

static Vec3 playerPos;

static int playerAngle = 0;
static int playerThrustX = 0;
static int playerThrustY = 0;
static int playerMoveSpeed = 4;
static int playerAngleSpeed = 2;

static Vec3 centeredViewPos;
static int viewZoomSpeed = 4;

static bool isInGame = true;
static bool gameQuit = false;


void switchGameMusic()
{
	stopMusPlay();

	if (isInGame) {
		loadMusFile(MUS_GAME);
		playMusFile(MUS_GAME);
	} else {
		loadMusFile(MUS_INTRO);
		playMusFile(MUS_INTRO);
	}
}

bool isGameQuit()
{
	return gameQuit;
}

void setGameQuit(bool quit)
{
	gameQuit = quit;
}

void setIsInGame(bool inGame)
{
	if (isInGame==inGame) return;
	isInGame = inGame;

	switchGameMusic();
}

static void initThings()
{
	memset(thing, 0, sizeof(GameThing) * NUM_THINGS);

	thing[0].mesh = objectMesh[OBJ_SPACESHIP];
	thing[0].alive = true;

	for (int i=1; i<4; ++i) {
		thing[i].mesh = objectMesh[OBJ_LASER];
		thing[i].alive = true;
	}
}

static bool checkPlayerCollision(uint8 *tmap)
{
	int playerLayer = playerPos.z / TILE_HEIGHT;

	if (playerPos.x < TILE_SIZE || playerPos.x >= (TILEMAP_WIDTH - 1) * TILE_SIZE || playerPos.y < TILE_SIZE || playerPos.y >= (TILEMAP_HEIGHT - 1) * TILE_SIZE) return true;
	if (playerLayer < 0  || playerLayer >= TILEMAP_LAYERS-1) return false;

	const int playerSize = TILE_SIZE / 4;
	int tx0 = (playerPos.x - playerSize) / TILE_SIZE;
	int ty0 = (playerPos.y - playerSize) / TILE_SIZE;
	int tx1 = (playerPos.x + playerSize) / TILE_SIZE;
	int ty1 = (playerPos.y + playerSize) / TILE_SIZE;

	CLAMP(tx0,0,TILEMAP_WIDTH-1)
	CLAMP(tx1,0,TILEMAP_WIDTH-1)
	CLAMP(ty0,0,TILEMAP_HEIGHT-1)
	CLAMP(ty1,0,TILEMAP_HEIGHT-1)

	tmap = &tmap[(playerLayer + 1) * TILEMAP_LAYER_SIZE];

	return (tmap[ty0*TILEMAP_WIDTH+tx0] || tmap[ty0*TILEMAP_WIDTH+tx1] || tmap[ty1*TILEMAP_WIDTH+tx0] || tmap[ty1*TILEMAP_WIDTH+tx1]);
}

#define THRUST_BITS 6
#define THRUST_MAX (1 << THRUST_BITS)

static void input3D(int dt)
{
	uint8* tmap = getTilemap3D();

	//static bool leftPressed = false;
	//static bool rightPressed = false;
	//static bool upPressed = false;
	//static bool downPressed = false;
	
	static bool rPrevPressed = false;
	static bool rNextPressed = false;

	int prevPlayerPosX = playerPos.x;
	int prevPlayerPosY = playerPos.y;

	int tAng = (dt*playerAngleSpeed) << PPOS_BITS;
	int tZoom = (dt*viewZoomSpeed) << PPOS_BITS;

	int pposX = playerPos.x << PPOS_BITS;
	int pposY = playerPos.y << PPOS_BITS;
	int pposZ = playerPos.z << PPOS_BITS;
	int pAngle = playerAngle << PPOS_BITS;

	int cposZ = centeredViewPos.z << PPOS_BITS;

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

	if (buttonsHeld.fire) {
	}

	if (buttonsHeld.escape) {
		setIsInGame(false);
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

	playerPos.z = pposZ >> PPOS_BITS;

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
		if (cposZ > (2048 << PPOS_BITS)) {
			cposZ -= tZoom;
		}
	}
	if (buttonsHeld.zoomOut) {
		cposZ += tZoom;
	}
	centeredViewPos.z = cposZ >> PPOS_BITS;

	if (buttonsHeld.renderPrev & !rPrevPressed) {
		playerPos.z -= TILE_HEIGHT;
		CLAMP(playerPos.z, 0, 3 * TILE_HEIGHT);
		//advTileRenderType(false);
	}
	if (buttonsHeld.renderNext & !rNextPressed) {
		playerPos.z += TILE_HEIGHT;
		CLAMP(playerPos.z, 0, 3 * TILE_HEIGHT);
		//advTileRenderType(true);
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

static void renderObject(int i, Screen *screen)
{
	GameThing *gt = &thing[i];
	Mesh *ms = gt->mesh;
	if (!ms) return;

	ms->pos.x = gt->pos.x - centeredViewPos.x;
	ms->pos.y = centeredViewPos.y - gt->pos.y;
	ms->pos.z = centeredViewPos.z - gt->pos.z;

	ms->rot = gt->rot;

	renderMesh(ms, screen);
}

static void scriptObject(int i, int t)
{
	if (i < 0 || i >= NUM_THINGS) return;

	GameThing *gt = &thing[i];

	if (!gt->alive) return;

	switch(i) {
		case 0:
		{
			gt->rot.x = SINTAB_SIZE >> 2;
			gt->rot.y = playerAngle;
			gt->rot.z = 0;

			gt->pos.x = playerPos.x;
			gt->pos.y = playerPos.y;
			gt->pos.z = playerPos.z;
		}
		break;

		default:
		{
			int vx = sinTab[(t + i * (SINTAB_SIZE / 3))& (SINTAB_SIZE - 1)];
			int vy = sinTab[(t + i * (SINTAB_SIZE / 3) - (SINTAB_SIZE / 4)) & (SINTAB_SIZE - 1)];

			gt->rot.x = SINTAB_SIZE >> 2;
			gt->rot.y = t;
			gt->rot.z = 0;

			gt->pos.x = playerPos.x + ((vx * 64) >> (4 + THRUST_BITS));
			gt->pos.y = playerPos.y - ((vy * 64) >> (4 + THRUST_BITS));
			gt->pos.z = playerPos.z;
		}
		break;
	}

	int layerIndex = gt->pos.z / TILE_HEIGHT;
	CLAMP(layerIndex, 0, TILEMAP_LAYERS);
	objsInLayer[layerIndex][layerObjCount[layerIndex]++] = i;
}

static void updateScene3D(Screen *screen, int t)
{
	Mesh *ms;

	centeredViewPos.x = playerPos.x;
	centeredViewPos.y = playerPos.y;

	memset(layerObjCount, 0, (TILEMAP_LAYERS+1) * sizeof(int));
	for (int i=0; i<NUM_THINGS; ++i) {
		scriptObject(i, t);
	}

	for (int n=0; n<TILEMAP_LAYERS+1; ++n) {
		if (n < TILEMAP_LAYERS) {
			renderTilemap3dLayer(&centeredViewPos, n, screen);
		}

		const int layerCount = layerObjCount[n];
		int *layerSrc = objsInLayer[n];
		for (int i=0; i<layerCount; ++i) {
			renderObject(layerSrc[i], screen);
		}
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

/*static int soundDuration = 32;

static void soundRun()
{
	if (soundDuration-- > 0) {
		soundTest();
	} else {
		stopSoundTest();
	}
}*/

void gameInit()
{
	initEngine();

	for (int i=0; i<NUM_MESHES; ++i) {
		int gridScaleMulX = 40;
		int gridScaleMulY = 40;
		int gridScaleMulZ = 40;
		if (i==OBJ_LASER) {
			gridScaleMulX = 12;
			gridScaleMulY = 16;
			gridScaleMulZ = 32;
		}

		objectMesh[i] = initMeshFromCPCdata(objMeshData[i]);
		objectMesh[i]->gridScaleX = (objectMesh[i]->gridScaleX * gridScaleMulX) >> 8;
		objectMesh[i]->gridScaleY = (objectMesh[i]->gridScaleY * gridScaleMulY) >> 8;
		objectMesh[i]->gridScaleZ = (objectMesh[i]->gridScaleZ * gridScaleMulZ) >> 8;

		//reversePolygonOrder(objectMesh[i]); // Why did this work on EGA but here we shouldn't be doing it?
	}

	playerPos.x = (TILEMAP_WIDTH / 2) * TILE_SIZE + TILE_SIZE / 2;
	playerPos.y = (TILEMAP_HEIGHT / 2) * TILE_SIZE + TILE_SIZE / 2;
	playerPos.z = 0;

	centeredViewPos.z = GROUND_Z;

	tilemap3dInit();

	initThings();

	setupPalette3D();

	fontsInit();

	menuInit();

	switchGameMusic();
}

void gameRun(Screen *screen, int t)
{
	static int t0 = 0;

	clearScreen(screen);

	if (isInGame) {
		input3D(t - t0);
		updateScene3D(screen, t);
	} else {
		menuRun(screen, t);
	}

	//soundRun();

#ifdef SHOW_PALETTE
	drawPalette((uint8*)screen->data);
#endif

	t0 = t;
}
