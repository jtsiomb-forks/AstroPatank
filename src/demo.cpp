#include <math.h>
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"

#include "demo.h"
#include "input.h"
#include "sound.h"
#include "musplay.h"
#include "mathutil.h"

//#define SHOW_PALETTE

static int fxIndex = FX_3D;
static int soundDuration = -1;

void (*fxInit[FX_NUM])(bool) = { fx3dInit };
void (*fxRun[FX_NUM])(Screen*, int) = { fx3dRun };

static void drawPalette(uint8 *vram)
{
	for (int y=0; y<16; ++y) {
		for (int x=0; x<256; ++x) {
			*(vram + x) = x;
		}
		vram += SCR_LINE_BYTES;
	}
}

static void demoInput()
{
	static bool firePressed = false;

	if (buttonsHeld.fire & !firePressed) {
		fxIndex = (fxIndex + 1) % FX_NUM;
		fxInit[fxIndex](true);
		soundDuration = 32;
	}

	firePressed = buttonsHeld.fire;
}

void demoInit(int partSelect)
{
	if (partSelect < FX_NUM) {
		fxIndex = partSelect;
	}

	for (int i=0; i<FX_NUM; ++i) {
		fxInit[i](false);
	}
	fxInit[fxIndex](true);

	runMusPlayTest();
}

void soundRun()
{
	if (soundDuration-- > 0) {
		soundTest();
	} else {
		stopSoundTest();
	}
}

void demoRun(Screen *screen, int t)
{
	demoInput();

	fxRun[fxIndex](screen, t);

	//soundRun();

#ifdef SHOW_PALETTE
	drawPalette((uint8*)screen->data);
#endif
}
