#ifndef FONTS_H
#define FONTS_H

#include "types.h"

void fontsInit();
void drawText(int xp, int yp, char *text, bool zoom, uint8 colOffset, uint8 *vram);

#endif
