#ifndef SOUND_H
#define SOUND_H

#include "types.h"

#include "soundbpr.h"	// will use the beeper sounds instead of doing anything with this OPL not event start sound effects code. Using OPL only in the musplay instead.

void initSound();
void deinitSound();

#ifdef USE_OPL_FOR_SOUND_FX
	uint8 getOplDetected();

	void soundTest();
	void stopSoundTest();
#endif

#endif
