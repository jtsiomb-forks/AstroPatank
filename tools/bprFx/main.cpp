#include <dos.h>
#include <stdio.h>
#include <conio.h>
#include <string.h>
#include <stdlib.h>

#include "types.h"

#include "main.h"
#include "video.h"
#include "input.h"
#include "fonts.h"
#include "gfxtools.h"
#include "soundbpr.h"

uint8 *VGAptr = (uint8*)0xA0000;
uint8 *TXTptr = (uint8*)0xB8000;

static Video *video;

static int soundIndex = SOUND_FIRE;


static void setupPalette()
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

static void setupSounds()
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

static void input()
{
	static bool upPressed = false;
	static bool downPressed = false;
	static bool leftPressed = false;
	static bool rightPressed = false;
	static bool firePressed = false;
	static bool selectPressed = false;
	static bool startPressed = false;
	static bool mapPressed = false;

	if (buttonsHeld.fire && !firePressed) {
		playSound(soundIndex);
	}

	if (buttonsHeld.select && !selectPressed) {
	}

	if (buttonsHeld.start && !startPressed) {
	}

	if (buttonsHeld.map && !mapPressed) {
		soundIndex = (soundIndex + 1) % SOUNDS_NUM;
	}

	if (buttonsHeld.up && !upPressed) {
	}

	if (buttonsHeld.down && !downPressed) {
	}

	if (buttonsHeld.left && !leftPressed) {
	}

	if (buttonsHeld.right && !rightPressed) {
	}

	upPressed = buttonsHeld.up;
	downPressed = buttonsHeld.down;
	leftPressed = buttonsHeld.left;
	rightPressed = buttonsHeld.right;
	firePressed = buttonsHeld.fire;
	selectPressed = buttonsHeld.select;
	startPressed = buttonsHeld.start;
	mapPressed = buttonsHeld.map;
}


static void drawNumber(int posX, int posY, uint32 num, uint8 colOffset)
{
	static char text[256];

	uint8 *vram = getRenderBuffer(video);

	sprintf(text, "%d", num);
	drawText(posX, posY, text, colOffset, 0, vram);
}

static void drawUI()
{
	Sound *sound = getSound(soundIndex);

	drawNumber(32, SCR_H - 32, soundIndex, 16);

	drawNumber(64, SCR_H - 32, sound->duration, 32);
	drawNumber(112, SCR_H - 32, sound->mul, 64);
	drawNumber(176, SCR_H - 32, sound->mod, 64);
}

int main(int argc, char **argv)
{
	initKeyboard();
	initSoundBpr();

	initVideoModeInfo();
	video = setVideoMode(SCR_W, SCR_H, SCR_BPP, true);
	clearFrame(video);
	setupPalette();

	fontsInit();

	setupSounds();

	while(!buttonsHeld.escape) {
		clearFrame(video);

		drawUI();

		input();

		updateFrame(video, true);
		updateSound();
	}

	deinitKeyboard();

	setTextMode();

	return 0;
}
