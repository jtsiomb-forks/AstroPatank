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

static const char* menuOptions[] = { "Start", "Quit" };

static int menuSelect = 0;


#define NUM_STARS 256
#define STAR_RANGE_X 512
#define STAR_RANGE_Y 512
#define STAR_NEAR 32
#define STAR_FAR 2048

enum {
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

static int8 *objMeshData[NUM_MESHES] =	{ objLetterAData, objLetterSData, objLetterTData, objLetterRData, objLetterOData, objLetterPData, objLetterNData, objLetterKData };

static Mesh *objMesh[NUM_MESHES];

#define ASTRO_LETTERS_NUM 5
#define PATANK_LETTERS_NUM 6

#define TITLE_INTERP_BITS 12

static int titleAstroMeshIndex[ASTRO_LETTERS_NUM] = { OBJ_LETTER_A, OBJ_LETTER_S, OBJ_LETTER_T, OBJ_LETTER_R, OBJ_LETTER_O };
static int titlePatankMeshIndex[PATANK_LETTERS_NUM] = { OBJ_LETTER_P, OBJ_LETTER_A, OBJ_LETTER_T, OBJ_LETTER_A, OBJ_LETTER_N, OBJ_LETTER_K };

static Vec3 titleAstroPos[ASTRO_LETTERS_NUM];
static Vec3 titlePatankPos[PATANK_LETTERS_NUM];

static bool cameFromGame = true;	// what a hack I am bored
static bool menuFinallyOn = false;

static int accelerateIntro = 256;

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
	static bool firePressed = false;
	static bool startPressed = false;

	if (menuFinallyOn) {
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

		if ((buttonsHeld.fire & !firePressed) || (buttonsHeld.start & !startPressed)) {
			if (menuSelect==0) {
				setIsInGame(true);
			} else {
				setGameQuit(true);
			}
		}
	} else {
		if ((buttonsHeld.fire & !firePressed) || (buttonsHeld.start & !startPressed) || buttonsHeld.escape) {
			if (accelerateIntro==256) accelerateIntro = 257;
		}
	}

	upPressed = buttonsHeld.up;
	downPressed = buttonsHeld.down;
	leftPressed = buttonsHeld.left;
	rightPressed = buttonsHeld.right;
	escapePressed = buttonsHeld.escape;
	firePressed = buttonsHeld.fire;
	startPressed = buttonsHeld.start;
}

static void updateStars(Screen *screen, int t)
{
	for (int i=0; i<NUM_STARS; ++i) {
		Vec3 *pos = &stars[i].pos;
		Vec3 *vel = &stars[i].vel;

		*pos += *vel;

		if (pos->z < STAR_NEAR) pos->z = STAR_FAR;
		if (pos->z > STAR_FAR) continue; //pos->z = STAR_NEAR;

		int sx = (pos->x << (PROJ_BITS + SCR_BITS)) / pos->z + (SCR_W << SCR_BITS) / 2;
		int sy = (SCR_H << SCR_BITS) / 2 - (pos->y << (PROJ_BITS + SCR_BITS)) / pos->z;

		if (sx >= 0 && sx < ((SCR_W-1) << SCR_BITS) && sy >= 0 && sy < ((SCR_H-1) << SCR_BITS)) {
			int alphaShade = (STAR_FAR - pos->z) >> 3;
			if (alphaShade > SHADE_ALPHA_MAX) alphaShade = SHADE_ALPHA_MAX;
			renderAntialiasedDot(sx,sy,((i & 1)<<6),alphaShade,(uint8*)screen->data);
		}
	}
}

static Vec3 interpolateVec(Vec3 &src, Vec3 &dst, int dt)
{
	Vec3 v;

	if (dt > (1 << TITLE_INTERP_BITS)) dt = 1 << TITLE_INTERP_BITS;
	int ddt = (1 << TITLE_INTERP_BITS) - dt;

	v.x = (src.x * ddt + dst.x * dt) >> TITLE_INTERP_BITS;
	v.y = (src.y * ddt + dst.y * dt) >> TITLE_INTERP_BITS;
	v.z = (src.z * ddt + dst.z * dt) >> TITLE_INTERP_BITS;

	return v;
}

static void update3D(Screen *screen, int t, int timeFromStart)
{
	/*Mesh *ms = objMesh[OBJ_ROMBUS_RING];

	t >>= 1;
	ms->rot.x = t;
	ms->rot.y = 2*t;
	ms->rot.z = 3*t;

	ms->pos.x = 0;
	ms->pos.y = 512;
	ms->pos.z = 4096;

	renderMesh(ms, screen);*/

	Vec3 farPos = Vec3(0,0,32768);
	Vec3 farRot = Vec3(4096, 8192, 6144);
	Vec3 dstRot;

	for (int i=0; i<ASTRO_LETTERS_NUM; ++i) {
		Mesh *ms = objMesh[titleAstroMeshIndex[i]];

		int dt = timeFromStart - i * 384 - 2048 - 512;
		if (dt > 0) {
			dstRot.z = (192 * sinTab[(t + 64*i) & (SINTAB_SIZE - 1)]) >> AMPLITUDE_BITS;

			ms->pos = interpolateVec(farPos, titleAstroPos[i], dt);
			ms->rot = interpolateVec(farRot, dstRot, dt);

			renderMesh(ms, screen);
		}
	}

	for (int i=0; i<PATANK_LETTERS_NUM; ++i) {
		Mesh *ms = objMesh[titlePatankMeshIndex[i]];

		int dt = timeFromStart - i * 384 - (1 << TITLE_INTERP_BITS) - 2048 - 768;
		if (dt > 0) {
			dstRot.z = (224 * sinTab[(t + 96*i) & (SINTAB_SIZE - 1)]) >> AMPLITUDE_BITS;

			ms->pos = interpolateVec(farPos, titlePatankPos[i], dt);
			ms->rot = interpolateVec(farRot, dstRot, dt);

			renderMesh(ms, screen);
		}

		if (!menuFinallyOn && (i==PATANK_LETTERS_NUM-1 && dt >= (1 << TITLE_INTERP_BITS))) {
			menuFinallyOn = true;
		}
	}

}

static void renderMenu(Screen *screen)
{
	uint8 *vram = (uint8*)screen->data;

	for (int i=0; i<2; ++i) {
		uint8 colOffset = 0;
		if (menuSelect!=i) colOffset = 16;
		drawText(120 + i * 8, 144 + i * 24, menuOptions[i], colOffset, 1, vram);
	}
}

static void initTitleLetters()
{
	for (int i=0; i<ASTRO_LETTERS_NUM; ++i) {
		Vec3 *pos = &titleAstroPos[i];

		int oof = 0;
		if (i == ASTRO_LETTERS_NUM-1) oof = 64;

		pos->x = (i - ASTRO_LETTERS_NUM / 2) * (1024 + oof) + 128 - 192;
		pos->y = 1512;
		pos->z = 8192 - 1024;
	}

	for (int i=0; i<PATANK_LETTERS_NUM; ++i) {
		Vec3 *pos = &titlePatankPos[i];

		int oof = 0;
		if (i >= PATANK_LETTERS_NUM-2) oof = 192;

		pos->x = (i - PATANK_LETTERS_NUM / 2) * (1024 + oof) + 384 - 192;
		pos->y = 0;
		pos->z = 8192 - 1024;
	}
}

static void initStars()
{
	for (int i=0; i<NUM_STARS; ++i) {
		stars[i].pos = Vec3(getRand(-STAR_RANGE_X, STAR_RANGE_X), getRand(-STAR_RANGE_Y, STAR_RANGE_Y), getRand(2 * STAR_FAR + STAR_NEAR, 2 * STAR_FAR + STAR_FAR));
		stars[i].vel = Vec3(0,0,-12 + getRand(0, 8));
	}
}

void menuInit()
{
	for (int i=0; i<NUM_MESHES; ++i) {
		objMesh[i] = initMeshFromCPCdata(objMeshData[i]);
	}

	initStars();

	initTitleLetters();
}

void menuRun(Screen *screen, int t)
{
	static int tStart = t;

	updateStars(screen, t);

	update3D(screen, t, (accelerateIntro * (t - tStart)) >> 8);
	if (!menuFinallyOn && accelerateIntro > 256) accelerateIntro+=16;

	inputMenu();

	if (menuFinallyOn) {
		renderMenu(screen);
	}
}
