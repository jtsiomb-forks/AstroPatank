#include <i86.h>
#include <stdio.h>
#include <conio.h>
#include <string.h>
#include <stdlib.h>

#include "types.h"

#include "demo.h"
#include "video.h"
#include "timer.h"
#include "input.h"
#include "sound.h"
#include "musplay.h"
#include "tinyfont.h"


static Video *video;
static Screen screen;
static bool vsync = true;

static int selectedPart = FX_3D;


static void interpretArgument(char *arg)
{
	if (!strcmp(arg, "-b")) {
		vsync = false;
	} else {
		char c = arg[0];
		if (c >= '0' && c <= '9') {
			selectedPart = c - '0';
		}
	}
}

static void initSystem()
{
	startMusPlayTest();
	delay(500);
	initTimer();
	initKeyboard();
	//initSound();

	initVideoModeInfo();

	video = setVideoMode(SCR_W, SCR_H, SCR_BPP, true);
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

	demoInit(selectedPart);

	while(!buttonsHeld.escape) {
		demoRun(&screen, getTime());
		updateFrame(video, vsync);

		drawFps(video);
	}

	deinitTimer();
	deinitKeyboard();

	stopMusPlayTest();

	//deinitSound();

	setTextMode();

	return 0;
}
