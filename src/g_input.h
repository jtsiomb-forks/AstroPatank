#ifndef G_INPUT_H
#define G_INPUT_H

#include "types.h"
#include "game.h"

enum {
	INPUT_MOVE_CAR,
	INPUT_MOVE_HOVER,
	INPUT_MOVE_TANK,
	INPUT_MOVE_DUDE,
	INPUT_MOVE_NUM
};

bool updateInputType0(GameThing *gt, int dt);
void resetInput0();

bool updateInputType1(GameThing *gt, int dt);
void resetInput1();

bool (*updateInputType[INPUT_MOVE_NUM])(GameThing*, int) = { updateInputType0, updateInputType1, updateInputType1, updateInputType1 };
void (*resetInput[INPUT_MOVE_NUM])() = { resetInput0, resetInput1, resetInput1, resetInput1 };

#endif
