#ifndef SOUND_BPR_H
#define SOUND_BPR_H

#include "types.h"

enum {
	SOUND_FIRE, 
	SOUND_PLAYER_HIT, 
	SOUND_PLAYER_BOUNCE, 
	SOUND_PLAYER_DEAD, 
	SOUND_ENEMY_DEAD, 
	SOUND_GEM_PICKUP, 
	SOUND_HEALTH_PICKUP,
	SOUND_POWER_PICKUP, 
	SOUND_LASER_PUFF, 
	SOUNDS_NUM
};

typedef struct Sound
{
	int t;
	int duration;
	uint16 *freq;
} Sound;


void initSoundBpr();
void stopSoundBpr();

void setupSound(uint8 duration, uint16 mul, uint16 mod, int index);
void playSound(int index);
void updateSound();

#endif
