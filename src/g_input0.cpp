#include <math.h>

#include "types.h"
#include "mathutil.h"

#include "game.h"
#include "engine.h"
#include "input.h"
#include "vector.h"
#include "mathutil.h"

#define MOVE_MAX 64
#define THRUST_BITS 6
#define THRUST_MAX (1 << THRUST_BITS)

static int playerThrustX = 0;
static int playerThrustY = 0;
static int playerMoveSpeed = 4;
static int playerAngleSpeed = 2;

bool updateInputType0(GameThing *gt, int dt)
{
	bool collided = false;

	Vec3 *pos = &gt->pos;

	int prevPlayerPosX = pos->x;
	int prevPlayerPosY = pos->y;

	int tAng = (dt*playerAngleSpeed) << PPOS_BITS;

	int playerAngle = gt->rot.y;
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
			playerThrustY--;;
		}
	}

	if (buttonsHeld.fire) {
		playerFire(*pos, playerAngle);
	}

	if (playerThrustX != 0) {
		int tMov = (dt*playerMoveSpeed*playerThrustX) << (PPOS_BITS - THRUST_BITS);
		int tMovX = (tMov * sinTab[playerAngle & (SINTAB_SIZE - 1)]) >> AMPLITUDE_BITS;
		pos->x -= tMovX;
	}
	if (checkThingMapCollision(gt)) {
		pos->x = prevPlayerPosX;
		playerThrustX = -playerThrustX >> 1;
		collided = true;
	}

	if (playerThrustY != 0) {
		int tMov = (dt*playerMoveSpeed*playerThrustY) << (PPOS_BITS - THRUST_BITS);
		int tMovY = (tMov * sinTab[(playerAngle - (SINTAB_SIZE / 4)) & (SINTAB_SIZE - 1)]) >> AMPLITUDE_BITS;
		pos->y += tMovY;
	}
	if (checkThingMapCollision(gt)) {
		pos->y = prevPlayerPosY;
		playerThrustY = -playerThrustY >> 1;
		collided = true;
	}

	if (playerThrustX != 0 || playerThrustY != 0) {
		Vec3 pos0 = Vec3(prevPlayerPosX, prevPlayerPosY, 0);
		Vec3 vel0 = getVelocityFromAngle(getRand(0, SINTAB_SIZE-1), 2048);
		spawnParticle(pos0, vel0, 32, 16);
	}

	return collided;
}

void resetInput0()
{
	playerThrustX = 0;
	playerThrustY = 0;
}
