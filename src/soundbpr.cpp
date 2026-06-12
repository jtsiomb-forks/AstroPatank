#include "soundbpr.h"

#ifdef __DJGPP__
	#include <pc.h>
#else
	#include <conio.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Sound soundFx[SOUNDS_NUM];

static void startSound(uint8 freq1, uint8 freq2)
{
	outp(0x43,0x0b6);
	outp(0x42,freq1);
	outp(0x42,freq2);

	outp(0x61, inp(0x61) | 3);
}

static void endSound()
{
	outp(0x61,inp(0x61) & 0x0fc);
}

void playSound(int index)
{
	if (index >= SOUNDS_NUM) return;

	soundFx[index].t = soundFx[index].duration;
}

void initSoundBpr()
{
	memset(soundFx, 0, SOUNDS_NUM * sizeof(Sound));
}

void setupSound(uint8 duration, uint16 mul, uint16 mod, int index)
{
	Sound *snd = &soundFx[index];

	snd->t = 0;
	snd->duration = duration;

	snd->freq = (uint16*)malloc(duration * sizeof(uint16));

	for (uint8 i=0; i<duration; ++i) {
		snd->freq[i] = ((uint16)i * mul) % mod;
	}
}

void updateSound()
{
	bool soundsStillPlaying = false;
	for (int i=0; i<SOUNDS_NUM; ++i) {
		Sound *snd = &soundFx[i];
		if (snd->t > 0) {
			uint16 freq = snd->freq[snd->duration - snd->t - 1];
			startSound(freq & 255, freq >> 8);
			snd->t--;
			soundsStillPlaying = true;
		}
	}
	if (!soundsStillPlaying) endSound();
}

void stopSoundBpr()
{
	endSound();
}
