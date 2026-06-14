#ifndef GAME_H
#define GAME_H

#include "types.h"

#ifndef __DJGPP__
	#define SOUND_ON
#endif

#define SCR_W 320
#define SCR_H 200
#define SCR_BPP 8

//#define SCR_UNCHAINED

#ifndef SCR_UNCHAINED
	#define UNCHAINED_BITS 0
	#define SCR_LINE_BYTES ((SCR_W * SCR_BPP) / 8)
#else
	#define UNCHAINED_BITS 2
	#define SCR_LINE_BYTES (((SCR_W >> UNCHAINED_BITS) * SCR_BPP) / 8)
#endif

#define VRAM_PIXEL_OFFSET(x,y) ((y) * SCR_LINE_BYTES + (x))


typedef struct Screen
{
	int width, height, bpp;
	void *data;
} Screen;

void gameInit();
void gameRun(Screen *screen, int t);

void setIsInGame(bool inGame);
void setGameQuit(bool quit);
bool isGameQuit();

void startGameMusic(int musIndex);

#endif
