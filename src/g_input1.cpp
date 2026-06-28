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

static int playerThrust = 0;
static int playerAngleSpeed = 4;
static int playerVelocityX = 0;
static int playerVelocityY = 0;


static void diminishForce(int &force)
{
	int threshold = MOVE_MAX;

	if (force < -threshold) {
		force += threshold;
	} else if (force > threshold) {
		force -= threshold;
	} else {
		force = 0;
	}
}

bool updateInputType1(GameThing *gt, int dt)
{
	bool collided = false;

	Vec3 *pos = &gt->pos;
	Vec3 *rot = &gt->rot;

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
		playerThrust = 1 << (THRUST_BITS - 3);
	} else if (buttonsHeld.down) {
		playerThrust = -1 << (THRUST_BITS - 3);
	} else {
		playerThrust = 0;
	}

	if (buttonsHeld.fire) {
		playerFire(*pos, playerAngle);
	}

	int tMov = (dt*playerThrust) << (PPOS_BITS - THRUST_BITS);
	playerVelocityX += (tMov * sinTab[playerAngle & (SINTAB_SIZE - 1)]) >> AMPLITUDE_BITS;
	playerVelocityY += (tMov * sinTab[(playerAngle - (SINTAB_SIZE / 4)) & (SINTAB_SIZE - 1)]) >> AMPLITUDE_BITS;

	CLAMP(playerVelocityX, (-MOVE_MAX << PPOS_BITS), (MOVE_MAX << PPOS_BITS));
	CLAMP(playerVelocityY, (-MOVE_MAX << PPOS_BITS), (MOVE_MAX << PPOS_BITS));

	pos->x -= playerVelocityX;
	if (checkThingMapCollision(gt)) {
		pos->x = prevPlayerPosX;
		playerVelocityX = -(playerVelocityX * 12) >> 4;
		collided = true;
	}

	pos->y += playerVelocityY;
	if (checkThingMapCollision(gt)) {
		pos->y = prevPlayerPosY;
		playerVelocityY = -(playerVelocityY * 12) >> 4;
		collided = true;
	}

	if (playerThrust != 0) {
		Vec3 pos0 = Vec3(prevPlayerPosX, prevPlayerPosY, 0);
		Vec3 vel0 = getVelocityFromAngle(getRand(0, SINTAB_SIZE-1), 2048);
		spawnParticle(pos0, vel0, 32, 16);
	}

	diminishForce(playerVelocityX);
	diminishForce(playerVelocityY);

	return collided;
}

void resetInput1()
{
	playerThrust = 0;
	playerVelocityX = 0;
	playerVelocityY = 0;
}
