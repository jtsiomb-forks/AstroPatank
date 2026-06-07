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

#define GROUND_Z (2048 + 512)
#define MID_Z (3144 + 768)
#define FAR_Z (4096 + 1024)
#define MAP_OUT_Z (16384 + 2048)
#define MAP_INDEX_SIZE 4

#define PPOS_BITS 8

#define NUM_THINGS 64

#define PLAYER_THING_BASE 0
#define NUM_PLAYERS 1

#define LASER_TIME_MAX 32
#define LASER_THING_BASE (PLAYER_THING_BASE + NUM_PLAYERS)
#define MAX_LASERS 7

#define NARC_THING_BASE (LASER_THING_BASE + MAX_LASERS)
#define MAX_NARCS 12

#define ENERGY_BONUS_THING_BASE (NARC_THING_BASE + MAX_NARCS)
#define MAX_ENERGY_BONUS 3

#define SHIELD_BONUS_THING_BASE (ENERGY_BONUS_THING_BASE + MAX_ENERGY_BONUS)
#define MAX_SHIELD_BONUS 2

#define WEAPON_BONUS_THING_BASE (SHIELD_BONUS_THING_BASE + MAX_SHIELD_BONUS)
#define MAX_WEAPON_BONUS 1

#define RING_BONUS_THING_BASE (WEAPON_BONUS_THING_BASE + MAX_WEAPON_BONUS)
#define MAX_RING_BONUS 6


#define ANTI_SPAWN_PLAYER 128
#define ANTI_SPAWN_NARC 256
#define ANTI_SPAWN_ENERGY 512
#define ANTI_SPAWN_RING 128
#define ANTI_SPAWN_WEAPON 1024
#define SPAWN_FULL 64

#define NUM_PARTICLES 256

#define THRUST_BITS 6
#define THRUST_MAX (1 << THRUST_BITS)

#define MAX_SHIELD 8
#define MAX_ENERGY 8
#define MAX_HIT_BLINK 64

#define ENERGY_SCALER 2

#define MAX_RINGS_TO_FINISH 50

#define ENEMY_KILL_SCORE 50
#define RING_PICKUP_SCORE 100

typedef struct PlayerHit
{
	bool justHit;
	uint8 damage;
	uint8 warmUp;
} PlayerHit;

static bool mustUpdateScore = true;
static bool mustUpdateRings = true;
static bool mustUpdateLives = true;
static bool mustUpdatePower = true;
static int score = 0;
static int rings = 0;
static int energy = MAX_ENERGY * ENERGY_SCALER;
static int shield = MAX_SHIELD * ENERGY_SCALER;
static int power = 0;
static int lives = 3;

static bool gameOver = false;
static bool gateOpened = false;
static bool youWarp = false;
static bool youWin = false;

PlayerHit playerHit = { false, 0, 0 };

static int mapZ[] = { GROUND_Z, MID_Z, FAR_Z, MAP_OUT_Z };
static uint8 mapIndex = 1;



typedef struct GameThing
{
	Vec3 pos, rot, vel;
	int size;
	Mesh *mesh;
	int spawn;
	Vec3 spawnMeshScale;
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
	OBJ_TRIPOD, 
	OBJ_UFO, OBJ_UFO2,
	OBJ_GLENZ, OBJ_DRUM, OBJ_CROSS,
	OBJ_SPACESHIP,
	OBJ_TORUS, OBJ_CUBESTAR,
	OBJ_ROMBUS_RING, OBJ_EIGHT_CUBES,
	OBJ_LASER,
	NUM_MESHES
};

static int8 *objMeshData[NUM_MESHES] =	{ 	objTripodData, objUfoData, objUfo2Data, objGlenzData, objDrumData, objSquareCrossData, 
											objSpaceship1Data, objTorusData, objCubeStarData, objRombusRingData, objEightCubesData, objLaserData
										};

static Mesh *objectMesh[NUM_MESHES];

static int objsInLayer[TILEMAP_LAYERS+1][NUM_THINGS];
static int layerObjCount[TILEMAP_LAYERS+1];

static int playerThrustX = 0;
static int playerThrustY = 0;
static int playerMoveSpeed = 4;
static int playerAngleSpeed = 2;

static int currentLaser = 0;
static int playerLaserTime = 0;

static Vec3 centeredViewPos;

static bool isInGame = true;
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

	playerLaserTime = LASER_TIME_MAX;
}

static int getRandomAntiSpawn(int antiSpawnBase)
{
	int antiSpawnRange = antiSpawnBase >> 2;
	return -antiSpawnBase + getRand(-antiSpawnRange, antiSpawnRange);
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

static void damagePlayer(uint8 damage, uint8 warmUp)
{
	if (playerHit.warmUp!=0) return;

	playerHit.justHit = true;
	playerHit.warmUp = warmUp;
	playerHit.damage = damage;
}

static bool checkThingThingCollision(GameThing *gt1, GameThing *gt2)
{
	int range1 = gt1->size << PPOS_BITS;
	int range2 = gt2->size << PPOS_BITS;

	int posX1 = gt1->pos.x;
	int posX2 = gt2->pos.x;
	if (posX1 + range1 >= posX2 - range2 && posX2 + range2 >= posX1 - range1) {
		int posY1 = gt1->pos.y;
		int posY2 = gt2->pos.y;
		if (posY1 + range1 >= posY2 - range2 && posY2 + range2 >= posY1 - range1) {
			return true;
		}
	}
	return false;
}

static bool checkThingMapCollision(GameThing *gt)
{
	int posX = gt->pos.x >> PPOS_BITS;
	int posY = gt->pos.y >> PPOS_BITS;
	int posZ = gt->pos.z >> PPOS_BITS;

	int layer = posZ / TILE_HEIGHT;

	if (posX < TILE_SIZE || posX >= (TILEMAP_WIDTH - 1) * TILE_SIZE || posY < TILE_SIZE || posY >= (TILEMAP_HEIGHT - 1) * TILE_SIZE) return true;
	if (layer < 0  || layer >= TILEMAP_LAYERS-1) return false;

	const int thingSize = gt->size;
	int tx0 = (posX - thingSize) / TILE_SIZE;
	int ty0 = (posY - thingSize) / TILE_SIZE;
	int tx1 = (posX + thingSize) / TILE_SIZE;
	int ty1 = (posY + thingSize) / TILE_SIZE;

	CLAMP(tx0,0,TILEMAP_WIDTH-1)
	CLAMP(tx1,0,TILEMAP_WIDTH-1)
	CLAMP(ty0,0,TILEMAP_HEIGHT-1)
	CLAMP(ty1,0,TILEMAP_HEIGHT-1)

	uint8* tmap = &getTilemap3D()[(layer + 1) * TILEMAP_LAYER_SIZE];

	return (tmap[ty0*TILEMAP_WIDTH+tx0] || tmap[ty0*TILEMAP_WIDTH+tx1] || tmap[ty1*TILEMAP_WIDTH+tx0] || tmap[ty1*TILEMAP_WIDTH+tx1]);
}

static bool checkPlayerGateCollision()
{
	if (!gateOpened) return false;

	GameThing *gtPlayer = &thing[PLAYER_THING_BASE];

	int posX = (gtPlayer->pos.x >> PPOS_BITS) / TILE_SIZE;
	int posY = (gtPlayer->pos.y >> PPOS_BITS) / TILE_SIZE;

	return (posX == TILEMAP_WIDTH / 2) && (posY == TILEMAP_HEIGHT / 2);
}

static void updateNarcs()
{
	GameThing *gtPlayer = &thing[PLAYER_THING_BASE];

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

			if (gtPlayer->alive && checkThingThingCollision(gt, gtPlayer)) {
				damagePlayer(ENERGY_SCALER, MAX_HIT_BLINK);
			}
		}
	}
}

static void incScore(int value)
{
	score += value;
	mustUpdateScore = true;
}

static void incRings()
{
	rings++;
	if (rings >= MAX_RINGS_TO_FINISH) {
		gateOpened = true;
	}
	mustUpdateRings = true;
}

static void spawnParticleMiniExplosion(Vec3 &pos, int numParticles, uint8 color, uint8 life, int velMul = 1)
{
	for (int n=0; n<numParticles; ++n) {
		Vec3 vel = getVelocityFromAngle(getRand(0, SINTAB_SIZE-1), getRand(512,2048) * velMul);
		spawnParticle(pos, vel, color, life);
	}
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

static void laserAgainstEnemies(GameThing *gtLaser)
{
	for (int i = 0; i < MAX_NARCS; ++i) {
		GameThing *gt = &thing[NARC_THING_BASE + i];
		if (gt->alive && checkThingThingCollision(gt, gtLaser)) {
			gt->alive = gtLaser->alive = false;
			gt->spawn = getRandomAntiSpawn(ANTI_SPAWN_NARC);
			spawnParticleMiniExplosion(gt->pos, 32, 128, 48);
			incScore(ENEMY_KILL_SCORE);
			playSound(SOUND_ENEMY_DEAD);
		}
	}
}

static void updateLasers()
{
	for (int i = 0; i < MAX_LASERS; ++i) {
		GameThing *gt = &thing[LASER_THING_BASE + i];
		if (gt->alive) {
			gt->pos += gt->vel;
			if (checkThingMapCollision(gt)) {
				spawnParticleMiniExplosion(gt->pos, 16, 96, 32);
				gt->alive = false;
				playSound(SOUND_LASER_PUFF);
			}
			laserAgainstEnemies(gt);	// not optimized, it's O(N^2)= O(LASERS * ENEMIES)
		}
	}

	if (playerLaserTime > 0) playerLaserTime--;
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

static void updatePlayerHit()
{
	static int winZoom = 0;

	GameThing *gtPlayer = &thing[PLAYER_THING_BASE];

	if (checkPlayerGateCollision()) youWarp = true;

	if (youWarp) {
		winZoom += 32;
		if (mapIndex==MAP_INDEX_SIZE-1) winZoom += 96;
		gtPlayer->pos.x = ((TILEMAP_WIDTH / 2) * TILE_SIZE + TILE_SIZE / 2) << PPOS_BITS;
		gtPlayer->pos.y = ((TILEMAP_HEIGHT / 2) * TILE_SIZE + TILE_SIZE / 2) << PPOS_BITS;
		gtPlayer->pos.z = winZoom << PPOS_BITS;
		gtPlayer->rot.y += 64;
		if (winZoom >= mapZ[mapIndex]) {
			youWin = true;
			gtPlayer->alive = false; // just to dissapear and pause
		}
		return;
	}

	if (energy==0) {
		if (gtPlayer->alive) {
			spawnParticleMiniExplosion(gtPlayer->pos, NUM_PARTICLES, 96, 80, 3);
			playSound(SOUND_PLAYER_DEAD);
			gtPlayer->alive = false;
			if (lives > 1) {
				gtPlayer->spawn = -ANTI_SPAWN_PLAYER;
			} else {
				lives = 0;
				gameOver = true;
				mustUpdateLives = true;
			}
		}
		return;
	}

	uint8 *spaceshipCols = gtPlayer->mesh->polyColor;
	if (playerHit.justHit) {
		if (shield == 0) {
			energy -= playerHit.damage;
			if (energy < 0) energy = 0;
		} else {
			shield -= playerHit.damage;
			if (shield < 0) shield = 0;
		}
		playSound(SOUND_PLAYER_HIT);
		playerHit.justHit = false;
	}

	if (playerHit.warmUp!=0) {
		// Ugly lazy hack for now
		if ((playerHit.warmUp & 15) > 7) {
			spaceshipCols[0] = 8;
			spaceshipCols[1] = 8;
			spaceshipCols[2] = 8;
		} else {
			spaceshipCols[0] = 2;
			spaceshipCols[1] = 4;
			spaceshipCols[2] = 2;
		}
		--playerHit.warmUp;
	}
}

static void updateItems()
{
	static Vec3 rotVel = Vec3(10, 12, 8);

	GameThing *gtPlayer = &thing[PLAYER_THING_BASE];

	for (int i=0; i<MAX_ENERGY_BONUS; ++i) {
		GameThing *gt = &thing[ENERGY_BONUS_THING_BASE + i];
		if (gt->alive) {
			gt->rot += rotVel;
			if (gtPlayer->alive && checkThingThingCollision(gt, gtPlayer)) {
				if (energy < MAX_ENERGY * ENERGY_SCALER) {
					energy += 2 * ENERGY_SCALER;
					if (energy > MAX_ENERGY * ENERGY_SCALER) energy = MAX_ENERGY * ENERGY_SCALER;
					gt->alive = false;
					gt->spawn = getRandomAntiSpawn(ANTI_SPAWN_ENERGY);
					playSound(SOUND_HEALTH_PICKUP);
				}
			};
		}
	}

	for (int i=0; i<MAX_SHIELD_BONUS; ++i) {
		GameThing *gt = &thing[SHIELD_BONUS_THING_BASE + i];
		if (gt->alive) {
			gt->rot += rotVel;
			if (gtPlayer->alive && checkThingThingCollision(gt, gtPlayer)) {
				if (shield < MAX_SHIELD * ENERGY_SCALER) {
					shield += 4 * ENERGY_SCALER;
					if (shield > MAX_SHIELD * ENERGY_SCALER) shield = MAX_SHIELD * ENERGY_SCALER;
					gt->alive = false;
					gt->spawn = getRandomAntiSpawn(ANTI_SPAWN_ENERGY);
					playSound(SOUND_HEALTH_PICKUP);
				}
			};
		}
	}

	for (int i=0; i<MAX_WEAPON_BONUS; ++i) {
		GameThing *gt = &thing[WEAPON_BONUS_THING_BASE + i];
		if (gt->alive) {
			gt->rot += rotVel;
			if (power < 3 && gtPlayer->alive && checkThingThingCollision(gt, gtPlayer)) {
				power++;
				mustUpdatePower = true;

				gt->alive = false;
				gt->spawn = getRandomAntiSpawn(ANTI_SPAWN_WEAPON);
				playSound(SOUND_POWER_PICKUP);
			};
		}
	}

	for (int i=0; i<MAX_RING_BONUS; ++i) {
		GameThing *gt = &thing[RING_BONUS_THING_BASE + i];
		if (gt->alive) {
			gt->rot += rotVel;
			if (gtPlayer->alive && checkThingThingCollision(gt, gtPlayer)) {
				gt->alive = false;
				gt->spawn = getRandomAntiSpawn(ANTI_SPAWN_RING);
				incScore(RING_PICKUP_SCORE);
				incRings();
				playSound(SOUND_RING_PICKUP);
			};
		}
	}
}

static void updateSpawning()
{
	for (int i=0; i<NUM_THINGS; ++i) {
		GameThing *gt = &thing[i];

		int spawnVal = gt->spawn;
		if (spawnVal < SPAWN_FULL) {
			gt->spawn = ++spawnVal;
			if (spawnVal >= 0 && spawnVal < SPAWN_FULL) {
				if (spawnVal == 0) {
					setRandomThingPosition(gt, 0);
				}
				int gridScaleX = (gt->mesh->gridScaleX * spawnVal) / SPAWN_FULL;
				int gridScaleY = (gt->mesh->gridScaleY * spawnVal) / SPAWN_FULL;
				int gridScaleZ = (gt->mesh->gridScaleZ * spawnVal) / SPAWN_FULL;
				if (gridScaleX<=0) gridScaleX = 1;
				if (gridScaleY<=0) gridScaleY = 1;
				if (gridScaleZ<=0) gridScaleZ = 1;
				gt->spawnMeshScale.x = gridScaleX;
				gt->spawnMeshScale.y = gridScaleY;
				gt->spawnMeshScale.z = gridScaleZ;
			}
			if (spawnVal == SPAWN_FULL) {
				gt->alive = true;
				if (i==PLAYER_THING_BASE) {
					if (--lives>0) {
						energy = MAX_ENERGY * ENERGY_SCALER;
						shield = MAX_SHIELD * ENERGY_SCALER;

						playerHit.justHit = false;
						playerHit.damage = 0;
						playerHit.warmUp = 0;
						playerThrustX = 0;
						playerThrustY = 0;
					} else {
						lives = 0;
						gt->alive = false;
					}
					mustUpdateLives = true;
				}
				gt->spawnMeshScale.x = gt->mesh->gridScaleX;
				gt->spawnMeshScale.y = gt->mesh->gridScaleY;
				gt->spawnMeshScale.z = gt->mesh->gridScaleZ;
			}
		}
	}
}


static void updateGameplay(int t, int dt)
{
	updateSpawning();
	updateNarcs();
	updateItems();
	updateLasers();
	updateParticles();
	updatePlayerHit();
}

static void spawnLaser(Vec3 &pos, Vec3 &rot, Vec3 &vel)
{
	GameThing *gt = &thing[LASER_THING_BASE + currentLaser];

	gt->pos = pos;
	gt->rot = rot;
	gt->vel = vel;
	gt->alive = true;

	currentLaser = (currentLaser + 1) % MAX_LASERS;

	playSound(SOUND_FIRE);
}

static void initPlayerThing()
{
	GameThing *gt = &thing[PLAYER_THING_BASE];

	gt->mesh = objectMesh[OBJ_SPACESHIP];
	gt->size = TILE_SIZE / 4;
	gt->alive = true;

	gt->pos.x = ((TILEMAP_WIDTH / 2) * TILE_SIZE + TILE_SIZE / 2) << PPOS_BITS;
	gt->pos.y = ((TILEMAP_HEIGHT / 2) * TILE_SIZE + TILE_SIZE / 2) << PPOS_BITS;
	gt->pos.z = 0;

	gt->rot.x = SINTAB_SIZE >> 2;
	gt->rot.y = 0;
	gt->rot.z = 0;

	gt->spawn = SPAWN_FULL;
}

static void setRandomThingInMap(GameThing *gt, Mesh *mesh, uint8 layer, int spawn, bool moving)
{
	gt->mesh = mesh;
	gt->size = TILE_SIZE / 4;
	gt->spawn = spawn;
	gt->alive = false;
	setRandomThingPosition(gt, layer);
	if (moving) setRandomThingVelocity(gt);
}


static void initThings()
{
	memset(thing, 0, sizeof(GameThing) * NUM_THINGS);

	initPlayerThing();

	for (int i=0; i<MAX_LASERS; ++i) {
		GameThing *gt = &thing[LASER_THING_BASE + i];
		gt->mesh = objectMesh[OBJ_LASER];
		gt->size = TILE_SIZE / 4;
		gt->alive = false;
		gt->spawn = SPAWN_FULL;
	}

	for (int i=0; i<MAX_NARCS; ++i) {
		setRandomThingInMap(&thing[NARC_THING_BASE + i], objectMesh[OBJ_CUBESTAR], 0, getRandomAntiSpawn(ANTI_SPAWN_NARC), true);
	}

	for (int i=0; i<MAX_ENERGY_BONUS; ++i) {
		setRandomThingInMap(&thing[ENERGY_BONUS_THING_BASE + i], objectMesh[OBJ_CROSS], 0, getRandomAntiSpawn(ANTI_SPAWN_ENERGY), true);
	}

	for (int i=0; i<MAX_SHIELD_BONUS; ++i) {
		setRandomThingInMap(&thing[SHIELD_BONUS_THING_BASE + i], objectMesh[OBJ_DRUM], 0, getRandomAntiSpawn(ANTI_SPAWN_ENERGY), true);
	}

	for (int i=0; i<MAX_WEAPON_BONUS; ++i) {
		setRandomThingInMap(&thing[WEAPON_BONUS_THING_BASE + i], objectMesh[OBJ_GLENZ], 0, getRandomAntiSpawn(ANTI_SPAWN_WEAPON), true);
	}

	for (int i=0; i<MAX_RING_BONUS; ++i) {
		setRandomThingInMap(&thing[RING_BONUS_THING_BASE + i], objectMesh[OBJ_ROMBUS_RING], 0, getRandomAntiSpawn(ANTI_SPAWN_RING), true);
	}

	for (int i=0; i<NUM_THINGS; i++) {
		GameThing *gt = &thing[i];
		if (gt->spawn == SPAWN_FULL) {
			gt->spawnMeshScale.x = gt->mesh->gridScaleX;
			gt->spawnMeshScale.y = gt->mesh->gridScaleY;
			gt->spawnMeshScale.z = gt->mesh->gridScaleZ;
		} else {
			gt->spawnMeshScale.x = 1;
			gt->spawnMeshScale.y = 1;
			gt->spawnMeshScale.z = 1;
		}
	}
}

static void input3D(int dt)
{
	static bool rMapPressed = false;

	if (buttonsHeld.escape) {
		setIsInGame(false);
	}

	if (!youWarp && (buttonsHeld.map & !rMapPressed)) {
		mapIndex = (mapIndex + 1) % MAP_INDEX_SIZE;

		uint8 tileRenderType = TILE_RENDER_MESH;
		if (mapIndex==MAP_INDEX_SIZE-1) tileRenderType = TILE_RENDER_QUADS;
		setTileRenderType(tileRenderType);

		centeredViewPos.z = mapZ[mapIndex];
	}

	rMapPressed = buttonsHeld.map;


	GameThing *gt = &thing[PLAYER_THING_BASE];
	if (!gt->alive || youWarp) return;

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

	if (buttonsHeld.fire && playerLaserTime==0) {
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

		spawnLaser(bPos, bRot, bVel);

		playerLaserTime = LASER_TIME_MAX - 4 * power;
	}

	if (playerThrustX != 0) {
		int tMov = (dt*playerMoveSpeed*playerThrustX) << (PPOS_BITS - THRUST_BITS);
		int tMovX = (tMov * sinTab[playerAngle & (SINTAB_SIZE - 1)]) >> AMPLITUDE_BITS;

		pos->x -= tMovX;
	}

	if (checkThingMapCollision(gt)) {
		pos->x = prevPlayerPosX;
		playerThrustX = -(playerThrustX * 12) >> 4;
		if (shield==0) damagePlayer(ENERGY_SCALER/2, MAX_HIT_BLINK/2);
		playSound(SOUND_PLAYER_BOUNCE);
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
		if (shield==0) damagePlayer(ENERGY_SCALER/2, MAX_HIT_BLINK/2);
		playSound(SOUND_PLAYER_BOUNCE);
	}
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

	int backX = ms->gridScaleX;
	int backY = ms->gridScaleY;
	int backZ = ms->gridScaleZ;

	ms->gridScaleX = gt->spawnMeshScale.x;
	ms->gridScaleY = gt->spawnMeshScale.y;
	ms->gridScaleZ = gt->spawnMeshScale.z;

	renderMesh(ms, screen);

	ms->gridScaleX = backX;
	ms->gridScaleY = backY;
	ms->gridScaleZ = backZ;
}

static void updateThingsLayerLists()
{
	memset(layerObjCount, 0, (TILEMAP_LAYERS+1) * sizeof(int));

	for (int i=0; i<NUM_THINGS; ++i) {
		GameThing *gt = &thing[i];

		if (gt->spawn < 0 || (gt->spawn == SPAWN_FULL && !gt->alive)) continue;

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
				int alphaShade = 8 * p->life;
				if (alphaShade > SHADE_ALPHA_MAX) alphaShade = SHADE_ALPHA_MAX;
				renderAntialiasedDot(sx, sy, p->color, alphaShade, vram);
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
			renderTilemap3dLayer(&centeredViewPos, n, screen, gateOpened);
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
	uint16 *vram16 = (uint16*)(vram + (by * 8) * SCR_W + bx*8);

	value *= 3;
	for (int y=0; y<4; ++y) {
		uint8 c = colbase * 16 - 4*y - 1;
		uint16 c16 = (c << 8) | c;
		for (int x=0; x<value; x++) {
			*(vram16 + x) = c16;
		}
		vram16 += SCR_W / 2;
	}
}

static void updateUI(Screen *screen, int t)
{
	static char txtLives[10];
	static char txtScore[16];
	static char txtRings[10];
	static char txtPower[10];

	uint8 *vram = (uint8*)screen->data;

	drawBar(1,1,9,energy, vram);
	drawBar(1,2,10,shield, vram);

	if (mustUpdateLives) {
		sprintf(txtLives, "Lives: %d\n", lives);
		mustUpdateLives = false;
	}
	drawText(SCR_W - 76, 8, txtLives, 64, 0, vram);

	if (mustUpdatePower) {
		sprintf(txtPower, "Power: %d\n", power);
		mustUpdatePower = false;
	}
	drawText(SCR_W - 76, 24, txtPower, 112, 0, vram);

	if (mustUpdateScore) {
		sprintf(txtScore, "Score: %d\n", score);
		mustUpdateScore = false;
	}
	drawText(8, SCR_H - 12, txtScore, 32, 0, vram);

	if (mustUpdateRings) {
		sprintf(txtRings, "Rings: %d\n", rings);
		mustUpdateRings = false;
	}
	drawText(SCR_W - 76, SCR_H - 12, txtRings, 48, 0, vram);

	if (gameOver) {
		drawText(16, 88, "GAME OVER", 32 + (((sinTab[t & (SINTAB_SIZE - 1)] * 32) >> AMPLITUDE_BITS)), 2, vram);
	} else if (youWin) {
		drawText(32, 88, "YOU WIN!", 64 + (((sinTab[t & (SINTAB_SIZE - 1)] * 32) >> AMPLITUDE_BITS)), 2, vram);
	}

	if (!youWarp && gateOpened) {
		drawText(92, 172, "GATE OPEN", 94 + (((sinTab[(6*t) & (SINTAB_SIZE - 1)] * 3) >> AMPLITUDE_BITS)), 1, vram);
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

static void initSoundsBpr()
{
	setupSound(16, 128, 4096, SOUND_FIRE);

	setupSound(8, 2048, 32768, SOUND_PLAYER_HIT);

	setupSound(64, 5500, 31500, SOUND_PLAYER_DEAD);
	setupSound(16, 6144, 32768, SOUND_ENEMY_DEAD);
	
	setupSound(12, 6144, 23000, SOUND_PLAYER_BOUNCE);

	setupSound(8, 768, 3144, SOUND_RING_PICKUP);
	setupSound(12, 1512, 6144, SOUND_HEALTH_PICKUP);
	setupSound(40, 1280, 16384, SOUND_POWER_PICKUP);
	setupSound(6, 6144, 28000, SOUND_LASER_PUFF);
}

void gameInit()
{
	initEngine();

	for (int i=0; i<NUM_MESHES; ++i) {
		objectMesh[i] = initMeshFromCPCdata(objMeshData[i]);

		int gridScaleMulX = 40;
		int gridScaleMulY = 40;
		int gridScaleMulZ = 40;
		if (i==OBJ_LASER) {
			gridScaleMulX = 16;
			gridScaleMulY = 16;
			gridScaleMulZ = 32;
		}
		if (i==OBJ_DRUM) {
			gridScaleMulX = 28;
			gridScaleMulY = 24;
			gridScaleMulZ = 28;
			uint8 *cols = objectMesh[i]->polyColor;			
			for (int n=0; n<objectMesh[i]->numPolys; ++n) {
				cols[n] += 4;
				if (cols[n] > 6) cols[n] = 10;
			}
		}

		if (i==OBJ_CROSS) {
			gridScaleMulX = 32;
			gridScaleMulY = 32;
			gridScaleMulZ = 32;
			uint8 *cols = objectMesh[i]->polyColor;			
			for (int n=0; n<objectMesh[i]->numPolys; ++n) {
				cols[n] += 4;
				if (cols[n] > 6) cols[n] = 9;
			}
		}

		if (i==OBJ_ROMBUS_RING) {
			uint8 *cols = objectMesh[i]->polyColor;			
			for (int n=0; n<objectMesh[i]->numPolys; ++n) {
				cols[n] += 6;
			}
		}

		objectMesh[i]->gridScaleX = (objectMesh[i]->gridScaleX * gridScaleMulX) >> 8;
		objectMesh[i]->gridScaleY = (objectMesh[i]->gridScaleY * gridScaleMulY) >> 8;
		objectMesh[i]->gridScaleZ = (objectMesh[i]->gridScaleZ * gridScaleMulZ) >> 8;
	}

	centeredViewPos.z = mapZ[mapIndex];

	tilemap3dInit();

	initThings();

	setupPalette3D();

	fontsInit();

	menuInit();

	initSoundsBpr();

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
		updateUI(screen, t);
	} else {
		menuRun(screen, t);
	}

#ifdef SHOW_PALETTE
	drawPalette((uint8*)screen->data);
#endif

	t0 = t;
}
