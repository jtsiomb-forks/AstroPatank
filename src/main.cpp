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

#define MEM_DEBUG


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
		initSound();
		loadMusDriver();
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

struct dpmi_free_mem {
	uint32 largest_block;
	uint32 max_unlocked_page;
	uint32 max_locked_page;
	uint32 total_unlocked;
	uint32 total_free;
	uint32 total_pages;
	uint32 free_linear;
	uint32 swap_file_size;
	uint32 dummy[3];
};

#ifdef MEM_DEBUG
uint32 getFreeMem() {
    union REGS regs = {0};
    struct SREGS sregs = {0};
    struct dpmi_free_mem meminfo = {0};

	memset(&meminfo, 0xFF, sizeof(meminfo));

    sregs.es = FP_SEG(&meminfo);
    regs.x.edi = FP_OFF(&meminfo);
    regs.x.eax = 0x0500;

    int386x(0x31, &regs, &regs, &sregs);

    if (regs.x.cflag == 0) {
		return meminfo.largest_block;
    }
	return 0;
}
#endif

int main(int argc, char **argv)
{
	#ifdef MEM_DEBUG
	uint32 mem0 = getFreeMem();
	#endif

	for (int i=1; i<argc; ++i) {
		interpretArgument(argv[i]);
	}

	initSystem();

	gameInit();

	#ifdef MEM_DEBUG
	uint32 mem1 = getFreeMem();
	#endif
	
	while(!isGameQuit()) {
		screen.data = getRenderBuffer(video);
		gameRun(&screen, getTime());
		drawFps(video);
		updateFrame(video, vsync);
	}

	deinitTimer();
	deinitKeyboard();

	#ifdef SOUND_ON
		shutdownMusPlay();
		deinitSound();
	#endif

	setTextMode();

	#ifdef MEM_DEBUG
	uint32 mem2 = getFreeMem();
	printf("%d\n%d\n%d\n", mem0, mem1, mem2);
	#endif

	return 0;
}
