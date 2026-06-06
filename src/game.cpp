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

#define PLAYER_THING_BASE 0
#define NUM_PLAYERS 1

#define BULLET_TIME_MAX 48
#define BULLET_THING_BASE (PLAYER_THING_BASE + NUM_PLAYERS)
#define MAX_BULLETS 5

#define NARC_THING_BASE (BULLET_THING_BASE + MAX_BULLETS)
#define MAX_NARCS 8

#define NUM_PARTICLES 256

#define MAX_SHIELD 8
#define MAX_ENERGY 8

static int energy = MAX_ENERGY;
static int shield = MAX_SHIELD;


typedef struct GameThing
{
	Vec3 pos, rot, vel;
	Mesh *mesh;
	bool alive;
} GameThing;

static GameThing thing[NUM_THINGS];


typedef struct Particle
{
	Vec3 pos, vel;
	uint8 life, color;
} Particle;

static Particle particle[NUM_PARTICLES];
static uint32 currParticleIndex = 0;


enum {
	OBJ_QUAD, OBJ_TRIPOD, OBJ_PYRAMID, OBJ_ROMBUS, OBJ_CUBE, 
	OBJ_GLENZ, OBJ_UFO, OBJ_UFO2,
	OBJ_DRUM, OBJ_CROSS,
	OBJ_SPACESHIP,
	OBJ_TORUS, OBJ_CUBESTAR,
	OBJ_TEST3, OBJ_ROMBUS_RING, OBJ_TORUS2, OBJ_EIGHT_CUBES,
	OBJ_LASER,
	NUM_MESHES
};

static int8 *objMeshData[NUM_MESHES] =	{ 	objQuadData, objTripodData, objPyramidData, objRombusData, objCubeData, objGlenzData, objUfoData, objUfo2Data, objDrumData, objSquareCrossData, 
											objSpaceship1Data, objTorusData, objCubeStarData, objTest3Data, objRombusRingData, objTorus2Data, objEightCubesData, objLaserData
										};

static Mesh *objectMesh[NUM_MESHES];

static int objsInLayer[TILEMAP_LAYERS+1][NUM_THINGS];
static int layerObjCount[TILEMAP_LAYERS+1];

static int playerThrustX = 0;
static int playerThrustY = 0;
static int playerMoveSpeed = 4;
static int playerAngleSpeed = 2;

static int currentBullet = 0;
static int playerBulletTime = 0;

static Vec3 centeredViewPos;
static int viewZoomSpeed = 4;

static bool isInGame = false;
static bool gameQuit = false;


void switchGameMusic()
{
#ifdef SOUND_ON
	stopMusPlay();

	if (isInGame) {
		loadMusFile(MUS_GAME);
		playMusFile(MUS_GAME);
	} else {
		loadMusFile(MUS_INTRO);
		playMusFile(MUS_INTRO);
	}
#endif
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

static void spawnParticle(Vec3 &pos, Vec3 &vel, uint8 color, uint8 life)
{
	Particle *p = &particle[currParticleIndex];

	p->pos = pos;
	p->vel = vel;
	p->color = color;
	p->life = life;

	currParticleIndex = (currParticleIndex + 1) % NUM_PARTICLES;
}

static Vec3 getVelocityFromAngle(int angle, int scale)
{
	Vec3 vel;

	vel.x = -(scale * sinTab[angle & (SINTAB_SIZE - 1)]) >> AMPLITUDE_BITS;
	vel.y = (scale * sinTab[(angle - (SINTAB_SIZE / 4)) & (SINTAB_SIZE - 1)]) >> AMPLITUDE_BITS;
	vel.z = 0;

	return vel;
}

static bool checkThingMapCollision(GameThing *gt)
{
	Vec3 pos = gt->pos;

	pos.x >>= PPOS_BITS;
	pos.y >>= PPOS_BITS;
	pos.z >>= PPOS_BITS;

	int layer = pos.z / TILE_HEIGHT;

	if (pos.x < TILE_SIZE || pos.x >= (TILEMAP_WIDTH - 1) * TILE_SIZE || pos.y < TILE_SIZE || pos.y >= (TILEMAP_HEIGHT - 1) * TILE_SIZE) return true;
	if (layer < 0  || layer >= TILEMAP_LAYERS-1) return false;

	const int thingSize = TILE_SIZE / 4;
	int tx0 = (pos.x - thingSize) / TILE_SIZE;
	int ty0 = (pos.y - thingSize) / TILE_SIZE;
	int tx1 = (pos.x + thingSize) / TILE_SIZE;
	int ty1 = (pos.y + thingSize) / TILE_SIZE;

	CLAMP(tx0,0,TILEMAP_WIDTH-1)
	CLAMP(tx1,0,TILEMAP_WIDTH-1)
	CLAMP(ty0,0,TILEMAP_HEIGHT-1)
	CLAMP(ty1,0,TILEMAP_HEIGHT-1)

	uint8* tmap = &getTilemap3D()[(layer + 1) * TILEMAP_LAYER_SIZE];

	return (tmap[ty0*TILEMAP_WIDTH+tx0] || tmap[ty0*TILEMAP_WIDTH+tx1] || tmap[ty1*TILEMAP_WIDTH+tx0] || tmap[ty1*TILEMAP_WIDTH+tx1]);
}

static void updateNarcs()
{
	for (int i = 0; i < MAX_NARCS; ++i) {
		GameThing *gt = &thing[NARC_THING_BASE + i];
		if (gt->alive) {
			int prevPosX = gt->pos.x;
			int prevPosY = gt->pos.y;

			gt->pos.x += gt->vel.x;
			if (checkThingMapCollision(gt)) {
				gt->pos.x = prevPosX;
				gt->vel.x = -gt->vel.x;
			}

			gt->pos.y += gt->vel.y;
			if (checkThingMapCollision(gt)) {
				gt->pos.y = prevPosY;
				gt->vel.y = -gt->vel.y;
			}

			gt->rot.x += (((i & 3) + 1) << 4);
			gt->rot.y += (((i & 7) + 1) << 3);
			gt->rot.z += ((i & 15) << 2);
		}
	}

	if (playerBulletTime > 0) playerBulletTime--;
}

static void updateBullets()
{
	for (int i = 0; i < MAX_BULLETS; ++i) {
		GameThing *gt = &thing[BULLET_THING_BASE + i];
		if (gt->alive) {
			gt->pos += gt->vel;
			if (checkThingMapCollision(gt)) {
				for (int n=0; n<16; ++n) {
					Vec3 vel0 = getVelocityFromAngle(getRand(0, SINTAB_SIZE-1), getRand(512,2048));
					spawnParticle(gt->pos, vel0, 96, 32);
				}
				gt->alive = false;
			}
		}
	}

	if (playerBulletTime > 0) playerBulletTime--;
}

static void updateParticles()
{
	for (int i=0; i<NUM_PARTICLES; ++i) {
		Particle *p = &particle[i];

		if (p->life != 0) {
			p->pos += p->vel;
			p->life--;
		}
	}
}

static void updateGameplay(int t, int dt)
{
	updateNarcs();
	updateBullets();
	updateParticles();
}

static void spawnBullet(Vec3 &pos, Vec3 &rot, Vec3 &vel)
{
	GameThing *gt = &thing[BULLET_THING_BASE + currentBullet];

	gt->pos = pos;
	gt->rot = rot;
	gt->vel = vel;
	gt->alive = true;

	currentBullet = (currentBullet + 1) % MAX_BULLETS;
}

static void setRandomThingVelocity(GameThing *gt)
{
	int angle = getRand(0, SINTAB_SIZE-1);

	gt->vel = getVelocityFromAngle(angle, 16 << PPOS_BITS);
}

static void setRandomThingPosition(GameThing *gt, uint8 layer)
{
	int tx, ty;
	if (layer < TILEMAP_LAYERS-1) {
		uint8* tmap = &getTilemap3D()[(layer + 1) * TILEMAP_LAYER_SIZE];

		do {
			tx = getRand(1,TILEMAP_WIDTH-2);
			ty = getRand(1,TILEMAP_HEIGHT-2);
		} while(tmap[ty*TILEMAP_WIDTH+tx]);
	} else {
		tx = getRand(1,TILEMAP_WIDTH-2);
		ty = getRand(1,TILEMAP_HEIGHT-2);
	}

	gt->pos.x = (tx * TILE_SIZE + TILE_SIZE / 2) << PPOS_BITS;
	gt->pos.y = (ty * TILE_SIZE + TILE_SIZE / 2) << PPOS_BITS;
	gt->pos.z = (layer * TILE_SIZE) << PPOS_BITS;
}

static void initPlayerThing()
{
	GameThing *gt = &thing[PLAYER_THING_BASE];

	gt->mesh = objectMesh[OBJ_SPACESHIP];
	gt->alive = true;

	gt->pos.x = ((TILEMAP_WIDTH / 2) * TILE_SIZE + TILE_SIZE / 2) << PPOS_BITS;
	gt->pos.y = ((TILEMAP_HEIGHT / 2) * TILE_SIZE + TILE_SIZE / 2) << PPOS_BITS;
	gt->pos.z = 0;

	gt->rot.x = SINTAB_SIZE >> 2;
	gt->rot.y = 0;
	gt->rot.z = 0;
}

static void initThings()
{
	memset(thing, 0, sizeof(GameThing) * NUM_THINGS);

	initPlayerThing();

	for (int i=0; i<MAX_BULLETS; ++i) {
		GameThing *gt = &thing[BULLET_THING_BASE + i];
		gt->mesh = objectMesh[OBJ_LASER];
		gt->alive = false;
	}

	for (int i=0; i<MAX_NARCS; ++i) {
		GameThing *gt = &thing[NARC_THING_BASE + i];
		gt->mesh = objectMesh[OBJ_CUBESTAR];
		gt->alive = true;
		setRandomThingPosition(gt, 0);
		setRandomThingVelocity(gt);
	}
}

#define THRUST_BITS 6
#define THRUST_MAX (1 << THRUST_BITS)

static void input3D(int dt)
{
	GameThing *gt = &thing[PLAYER_THING_BASE];
	Vec3 *pos = &gt->pos;
	Vec3 *rot = &gt->rot;
	
	static bool rPrevPressed = false;
	static bool rNextPressed = false;

	int prevPlayerPosX = pos->x;
	int prevPlayerPosY = pos->y;

	int tAng = (dt*playerAngleSpeed) << PPOS_BITS;
	int tZoom = (dt*viewZoomSpeed) << PPOS_BITS;

	int playerAngle = gt->rot.y;
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
	gt->rot.y = playerAngle;

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

	if (buttonsHeld.fire && playerBulletTime==0) {
		Vec3 bPos;
		Vec3 bVel;
		Vec3 bRot;

		bVel = getVelocityFromAngle(playerAngle, 80 << PPOS_BITS);

		bPos.x = pos->x + bVel.x;
		bPos.y = pos->y + bVel.y;
		bPos.z = pos->z;

		bRot.x = SINTAB_SIZE >> 2;
		bRot.y = playerAngle;
		bRot.z = 0;

		spawnBullet(bPos, bRot, bVel);

		playerBulletTime = BULLET_TIME_MAX;
	}

	if (buttonsHeld.escape) {
		setIsInGame(false);
	}

	if (playerThrustX != 0) {
		int tMov = (dt*playerMoveSpeed*playerThrustX) << (PPOS_BITS - THRUST_BITS);
		int tMovX = (tMov * sinTab[playerAngle & (SINTAB_SIZE - 1)]) >> AMPLITUDE_BITS;

		pos->x -= tMovX;
	}

	if (checkThingMapCollision(gt)) {
		pos->x = prevPlayerPosX;
		playerThrustX = -(playerThrustX * 12) >> 4;
	}

	if (playerThrustY != 0) {
		int tMov = (dt*playerMoveSpeed*playerThrustY) << (PPOS_BITS - THRUST_BITS);
		int tMovY = (tMov * sinTab[(playerAngle - (SINTAB_SIZE / 4)) & (SINTAB_SIZE - 1)]) >> AMPLITUDE_BITS;

		pos->y += tMovY;
	}

	if (playerThrustX != 0 || playerThrustY != 0) {
		Vec3 pos0 = Vec3(prevPlayerPosX, prevPlayerPosY, 0);
		Vec3 vel0 = getVelocityFromAngle(getRand(0, SINTAB_SIZE-1), 2048);
		spawnParticle(pos0, vel0, 32, 16);
	}

	if (checkThingMapCollision(gt)) {
		pos->y = prevPlayerPosY;
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
		pos->z -= (TILE_HEIGHT << PPOS_BITS);
		CLAMP(pos->z, 0, 3 * (TILE_HEIGHT << PPOS_BITS));
	}
	if (buttonsHeld.renderNext & !rNextPressed) {
		pos->z += (TILE_HEIGHT << PPOS_BITS);
		CLAMP(pos->z, 0, 3 * (TILE_HEIGHT << PPOS_BITS));
	}

	rPrevPressed = buttonsHeld.renderPrev;
	rNextPressed = buttonsHeld.renderNext;
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

	makeAndSetPal(128,143, 15,7,3, 63,47,15);
	makeAndSetPal(144,160, 3,7,15, 15,47,63);

	makeAndSetPal(240,255, 0,0,0, 63,63,63);
}

static void renderObject(int i, Screen *screen)
{
	GameThing *gt = &thing[i];
	Mesh *ms = gt->mesh;
	if (!ms) return;

	ms->pos.x = (gt->pos.x >> PPOS_BITS) - centeredViewPos.x;
	ms->pos.y = centeredViewPos.y - (gt->pos.y >> PPOS_BITS);
	ms->pos.z = centeredViewPos.z - (gt->pos.z >> PPOS_BITS);

	int edgeX = ((SCR_W/2 + TILE_SIZE / 2) * ms->pos.z) >> PROJ_BITS;
	int edgeY = ((SCR_H/2 + TILE_SIZE / 2) * ms->pos.z) >> PROJ_BITS;
	if (ms->pos.x < -edgeX || ms->pos.x > edgeX || 
		ms->pos.y < -edgeY || ms->pos.y > edgeY) return;

	ms->rot = gt->rot;

	renderMesh(ms, screen);
}

static void updateThingsLayerLists()
{
	memset(layerObjCount, 0, (TILEMAP_LAYERS+1) * sizeof(int));

	for (int i=0; i<NUM_THINGS; ++i) {
		GameThing *gt = &thing[i];

		if (!gt->alive) continue;

		int layerIndex = (gt->pos.z >> PPOS_BITS) / TILE_HEIGHT;
		CLAMP(layerIndex, 0, TILEMAP_LAYERS);
		objsInLayer[layerIndex][layerObjCount[layerIndex]++] = i;
	}
}

static void renderParticles(uint8 *vram)
{
	int offX = -thing[PLAYER_THING_BASE].pos.x;
	int offY = -thing[PLAYER_THING_BASE].pos.y;

	for (int i=0; i<NUM_PARTICLES; ++i) {
		Particle *p = &particle[i];

		if (p->life != 0) {
			Vec3 *pos = &p->pos;
			int sx = ((SCR_W/2) << PPOS_BITS) + (((offX + pos->x) << (SCR_BITS + PROJ_BITS - PPOS_BITS)) / centeredViewPos.z);
			int sy = ((SCR_H/2) << PPOS_BITS) + (((offY + pos->y) << (SCR_BITS + PROJ_BITS - PPOS_BITS)) / centeredViewPos.z);

			if (sx >= 0 && sx < ((SCR_W-1) << SCR_BITS) && sy >= 0 && sy < ((SCR_H-1) << SCR_BITS)) {
				renderAntialiasedDot(sx, sy, p->color, vram);
			}
		}
	}
}

static void updateScene3D(Screen *screen, int t)
{
	Mesh *ms;

	centeredViewPos.x = thing[PLAYER_THING_BASE].pos.x >> PPOS_BITS;
	centeredViewPos.y = thing[PLAYER_THING_BASE].pos.y >> PPOS_BITS;

	updateThingsLayerLists();

	for (int n=0; n<TILEMAP_LAYERS+1; ++n) {
		if (n < TILEMAP_LAYERS) {
			renderTilemap3dLayer(&centeredViewPos, n, screen);
		}

		if (n==0) renderParticles((uint8*)screen->data);

		const int layerCount = layerObjCount[n];
		int *layerSrc = objsInLayer[n];
		for (int i=0; i<layerCount; ++i) {
			renderObject(layerSrc[i], screen);
		}
	}
}

static void drawBar(uint8 bx, uint8 by, uint8 colbase, int value, uint8 *vram)
{
	uint32 *vram32 = (uint32*)(vram + (by * 8) * SCR_W + bx*8);

	value *= 3;
	for (int y=0; y<4; ++y) {
		uint8 c = colbase * 16 - 4*y - 1;
		uint32 c32 = (c << 24) | (c << 16) | (c << 8) | c;
		for (int x=0; x<value; x+=2) {
			*(vram32 + x) = c32;
			*(vram32 + x + 1) = c32;
		}
		vram32 += SCR_W / 4;
	}
}

static void updateUI(Screen *screen)
{
	uint8 *vram = (uint8*)screen->data;

	drawBar(1,1,9,energy,vram);
	drawBar(1,2,10,shield,vram);
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
			gridScaleMulX = 16;
			gridScaleMulY = 16;
			gridScaleMulZ = 32;
		}

		objectMesh[i] = initMeshFromCPCdata(objMeshData[i]);
		objectMesh[i]->gridScaleX = (objectMesh[i]->gridScaleX * gridScaleMulX) >> 8;
		objectMesh[i]->gridScaleY = (objectMesh[i]->gridScaleY * gridScaleMulY) >> 8;
		objectMesh[i]->gridScaleZ = (objectMesh[i]->gridScaleZ * gridScaleMulZ) >> 8;

		//reversePolygonOrder(objectMesh[i]); // Why did this work on EGA but here we shouldn't be doing it?
	}

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
		updateGameplay(t, t - t0);
		updateScene3D(screen, t);
		updateUI(screen);
	} else {
		menuRun(screen, t);
	}

	//soundRun();

#ifdef SHOW_PALETTE
	drawPalette((uint8*)screen->data);
#endif

	t0 = t;
}
