#include <math.h>
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "menu.h"
#include "fonts.h"

#include "input.h"
#include "sound.h"
#include "musplay.h"
#include "mathutil.h"
#include "vector.h"
#include "engine.h"
#include "render.h"
#include "mesh.h"

#include "meshdata.h"

static char* menuOptions[] = { "Start", "Quit" };

static int menuSelect = 0;


#define NUM_STARS 256
#define STAR_RANGE_X 512
#define STAR_RANGE_Y 512
#define STAR_NEAR 32
#define STAR_FAR 2048

enum {
	OBJ_UFO, OBJ_UFO2,
	OBJ_CUBESTAR, OBJ_ROMBUS_RING, 
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

static int8 *objMeshData[NUM_MESHES] =	{ 	objUfoData, objUfo2Data, objCubeStarData, objRombusRingData, 
											objLetterAData, objLetterSData, objLetterTData, objLetterRData, objLetterOData, objLetterPData, objLetterNData, objLetterKData
										};

static Mesh *objMesh[NUM_MESHES];

static bool cameFromGame = true;	// what a hack I am bored


typedef struct Star
{
	Vec3 pos, vel;
} Star;

static Star stars[NUM_STARS];


static void inputMenu()
{
	static bool upPressed = false;
	static bool downPressed = false;
	static bool leftPressed = false;
	static bool rightPressed = false;
	static bool escapePressed = false;

	if (buttonsHeld.up & !upPressed) {
		if (menuSelect > 0) menuSelect = 0;
	}
	if (buttonsHeld.down & !downPressed) {
		if (menuSelect < 1) menuSelect = 1;
	}	
	if (buttonsHeld.left & !leftPressed) {
	}
	if (buttonsHeld.right & !rightPressed) {
	}

	if (buttonsHeld.fire) {
		if (menuSelect==0) {
			setIsInGame(true);
		} else {
			setGameQuit(true);
		}
	}

	upPressed = buttonsHeld.up;
	downPressed = buttonsHeld.down;
	leftPressed = buttonsHeld.left;
	rightPressed = buttonsHeld.right;
	escapePressed = buttonsHeld.escape;
}

static void updateStars(Screen *screen, int t)
{
	for (int i=0; i<NUM_STARS; ++i) {
		Vec3 *pos = &stars[i].pos;
		Vec3 *vel = &stars[i].vel;

		*pos += *vel;

		if (pos->z < STAR_NEAR) pos->z = STAR_FAR;
		if (pos->z > STAR_FAR) pos->z = STAR_NEAR;

		int sx = (pos->x << (PROJ_BITS + SCR_BITS)) / pos->z + (SCR_W << SCR_BITS) / 2;
		int sy = (SCR_H << SCR_BITS) / 2 - (pos->y << (PROJ_BITS + SCR_BITS)) / pos->z;

		if (sx >= 0 && sx < ((SCR_W-1) << SCR_BITS) && sy >= 0 && sy < ((SCR_H-1) << SCR_BITS)) {
			int alphaShade = (STAR_FAR - pos->z) >> 3;
			if (alphaShade > SHADE_ALPHA_MAX) alphaShade = SHADE_ALPHA_MAX;
			renderAntialiasedDot(sx,sy,((i & 1)<<6),alphaShade,(uint8*)screen->data);
		}
	}
}

static void update3D(Screen *screen, int t)
{
	Mesh *ms = objMesh[OBJ_ROMBUS_RING];

	t >>= 1;
	ms->rot.x = t;
	ms->rot.y = 2*t;
	ms->rot.z = 3*t;

	ms->pos.x = 0;
	ms->pos.y = 512;
	ms->pos.z = 4096;

	renderMesh(ms, screen);
}

static void renderMenu()
{
	for (int i=0; i<2; ++i) {
		uint8 colOffset = 0;
		if (menuSelect!=i) colOffset = 16;
		drawText(120 + i * 8, 144 + i * 24, menuOptions[i], true, colOffset);
	}
}

static void updateMenu(Screen *screen, int t)
{
	updateStars(screen, t);

	update3D(screen, t);

	renderMenu();
}

static void initStars()
{
	for (int i=0; i<NUM_STARS; ++i) {
		stars[i].pos = Vec3(getRand(-STAR_RANGE_X, STAR_RANGE_X), getRand(-STAR_RANGE_Y, STAR_RANGE_Y), getRand(STAR_NEAR, STAR_FAR));
		stars[i].vel = Vec3(0,0,-12 + getRand(0, 8));
	}
}

void menuInit()
{
	for (int i=0; i<NUM_MESHES; ++i) {
		objMesh[i] = initMeshFromCPCdata(objMeshData[i]);
	}

	initStars();
}

void menuRun(Screen *screen, int t)
{
	inputMenu();
	updateMenu(screen, t);
}
