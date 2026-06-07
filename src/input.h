#ifndef INPUT_H
#define INPUT_H

#include "types.h"

typedef struct Buttons
{
	bool left;
	bool right;
	bool up;
	bool down;
	bool select;
	bool start;
	bool fire;
	bool map;
	bool escape;
}Buttons;

void initKeyboard();
void deinitKeyboard();

extern Buttons buttonsHeld;

#endif
