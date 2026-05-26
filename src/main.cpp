#include <i86.h>
#include <stdio.h>
#include <conio.h>
#include <string.h>
#include <stdlib.h>

#include "types.h"

#include "game.h"
#include "video.h"
#include "timer.h"
#include "input.h"
#include "sound.h"
#include "musplay.h"
#include "tinyfont.h"


static Video *video;
static Screen screen;
static bool vsync = true;


static void interpretArgument(char *arg)
{
	if (!strcmp(arg, "-b")) {
		vsync = false;
	}
}

static void initSystem()
{
	#ifdef SOUND_ON
		//initSound();
		startMusPlayTest();
		delay(500);
	#endif

	initTimer();
	initKeyboard();

	initVideoModeInfo();

	#ifndef SCR_UNCHAINED
		video = setVideoMode(SCR_W, SCR_H, SCR_BPP, true);
	#else
		video = setVideoMode(SCR_W, SCR_H, SCR_BPP, false, true);
	#endif

	if (video==0) {
		printf("Video Mode not found\n");
		return;
	}

	screen.width = video->width;
	screen.height = video->height;
	screen.bpp = video->bpp;
	screen.data = getRenderBuffer(video);

	clearFrame(video);

	initTinyFonts();
}

int main(int argc, char **argv)
{
	for (int i=1; i<argc; ++i) {
		interpretArgument(argv[i]);
	}

	initSystem();

	gameInit();

	while(!buttonsHeld.escape) {
		screen.data = getRenderBuffer(video);
		gameRun(&screen, getTime());
		drawFps(video);
		updateFrame(video, vsync);
	}

	deinitTimer();
	deinitKeyboard();

	#ifdef SOUND_ON
		stopMusPlayTest();
		//deinitSound();
	#endif

	setTextMode();

	return 0;
}
