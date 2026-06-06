#include "soundbpr.h"

#include <stdlib.h>
#include <string.h>

static Sound sound[SOUNDS_NUM];

static void startSound(uint8 freq1, uint8 freq2)
{
	_asm
	{
		mov al,0b6h
		out 43h,al
		mov al,freq1
		out 42h,al
		mov al,freq2
		out 42h,al

		in al,61h
		or al,3
		out 61h,al
	}
}

static void endSound()
{
	_asm
	{
		in al,61h
		and al,0fch
		out 61h,al
	}
}

void playSound(int index)
{
	if (index >= SOUNDS_NUM) return;

	sound[index].t = sound[index].duration;
}

void initSoundBpr()
{
	memset(sound, 0, SOUNDS_NUM * sizeof(Sound));
}

void setupSound(uint8 duration, uint16 mul, uint16 mod, int index)
{
	Sound *snd = &sound[index];

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
		Sound *snd = &sound[i];
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
