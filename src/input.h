#ifndef INPUT_H
#define INPUT_H

#include "types.h"

typedef struct Buttons
{
	bool left;
	bool right;
	bool up;
	bool down;
	bool zoomIn;
	bool zoomOut;
	bool fire;
	bool escape;
}Buttons;

void initKeyboard();
void deinitKeyboard();

extern Buttons buttonsHeld;

#endif
