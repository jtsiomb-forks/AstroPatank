#include "sound.h"

#include <string.h>
#include <conio.h>
#include <dos.h>
#include <stdlib.h>
#include <i86.h>

#ifdef USE_OPL_FOR_SOUND_FX
	static uint8 oplDetected = 0;

	static void oplWrite(uint16 i, uint8 v)
	{
		if (i < 0x100) {
			outp(0x388,i);
			outp(0x389,v);
		} else {
			outp(0x38a,i);
			outp(0x38b,v);
		}
	}

	static void oplDetect()
	{
		oplWrite(0x04,0x60);
		oplWrite(0x04,0x80);

		uint8 s1 = inp(0x388) & 0xE0;
		oplWrite(0x02,0xFF);
		oplWrite(0x04,0x21);

		delay(80);

		uint8 s2 = inp(0x388) & 0xE0;
		oplWrite(0x04,0x60);
		oplWrite(0x04,0x80);

		if (s1==0 && s2==0xC0) {
			oplDetected = 2;
		}

		if (oplDetected > 0) {
			if ((inp(0x388) & 0x06)==0) {
				oplDetected = 3;
				oplWrite(0x105,0x01);	// init OPL3
			}
		}
	}

	void soundTest()
	{
		if (oplDetected==0) return;

		//set op1 to 75
		//that's Attack of 7 and a Decay of 5
		oplWrite(0x68,0x75);

		//same again with op2
		oplWrite(0x6B,0x75);

		if (oplDetected==3) {
			//The Left playback bit is bit 4
			oplWrite(0xC3,0x10);
		}

		//set the lower 8 bits of the F-Number.
		//F-Number for an F in octave 4 is 0x1CA
		oplWrite(0xA3,0xCA);

		//Set the top 2 bits of the F-Number,
		//the octave to 4,
		//and turn the note on by oring 0x20!
		oplWrite(0xB3,0x20|(4<<2)|0x01);
	}

	void stopSoundTest()
	{
		if (oplDetected==0) return;

		if (oplDetected==3) {
			oplWrite(0xC3,0);
		}

		oplWrite(0xA3,0);
		oplWrite(0xB3,0);
	}

	uint8 getOplDetected()
	{
		return oplDetected;
	}
#endif

void deinitSound()
{
	#ifdef USE_OPL_FOR_SOUND_FX
		for (int c = 0x01; c <= 0xF5; c++) {
			oplWrite(c,0);
			oplWrite(0x100|c,0);
		}
	#else
		stopSoundBpr(); // Same as below. Sound beeper simple engine taken from my previous CGA gamejam.
	#endif
}

void initSound()
{
	#ifdef USE_OPL_FOR_SOUND_FX
		oplDetect();
	#else
		initSoundBpr(); //commenting out opl because no time to figure out sound fx and could interfere with player. Using beeper sounds instead.
	#endif
}
