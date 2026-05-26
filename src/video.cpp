#include "video.h"

#include <string.h>
#include <conio.h>
#include <dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <dos.h>

#include "mathutil.h"

#define NUM_SOFT_BUFFERS 2

static uint8 *VGAptr = (uint8*)0xA0000;
static uint8 *TXTptr = (uint8*)0xB8000;

static uint8 vramPage = 0;

static Video vmodes[VMODES_NUM];

void initVideoModeInfo()
{
	vmodes[VMODE_TEXT] = Video(0x0003, 80, 50, 4, false, false, TXTptr);
	vmodes[VMODE_320x200_8BPP] = Video(0x0013, 320, 200, 8, false, false, VGAptr);
	vmodes[VMODE_Y_320x200_8BPP] = Video(0x0013, 320, 200, 8, true, false, VGAptr);
	vmodes[VMODE_X_320x240_8BPP] = Video(0x0013, 320, 240, 8, true, false, VGAptr);

	vmodes[VMODE_640x400_8BPP] = Video(0x0100, 640, 400, 8, false, true, VGAptr);
	vmodes[VMODE_640x480_8BPP] = Video(0x0101, 640, 480, 8, false, true, VGAptr);
	vmodes[VMODE_800x600_8BPP] = Video(0x0103, 800, 600, 8, false, true, VGAptr);
	vmodes[VMODE_1024x768_8BPP] = Video(0x0105, 1024, 768, 8, false, true, VGAptr);
	vmodes[VMODE_1280x1024_8BPP] = Video(0x0107, 1280, 1024, 8, false, true, VGAptr);
	vmodes[VMODE_320x200_16BPP] = Video(0x010e, 320, 200, 16, false, true, VGAptr);
}

void setMode(uint16 mode)
{
	_asm
	{
		mov ax,mode
		int 10h
	}
}

void setVesaMode(uint16 mode)
{
	_asm
	{
		mov ax,4F02h
		mov bx,mode
		int 10h
	}
}

void setTextMode()
{
	setMode(vmodes[VMODE_TEXT].mode);
}

static void setUnchainedMode(int height)
{
	uint8 a,b,c,d,e,f,g,h;

	switch(height) {
		case 200:
			a = 0x63;
			b = 0xBF;
			c = 0x1F;
			d = 0x9C;
			e = 0x8E;
			f = 0x8F;
			g = 0x96;
			h = 0xB9;
		break;

		case 240:
			a = 0xE3;
			b = 0x0D;
			c = 0x3E;
			d = 0xEA;
			e = 0xAC;
			f = 0xDF;
			g = 0xE7;
			h = 0x06;
		break;

		default:
			printf("Unchained mode with screen height %d not found\n", height);
			return;
	}

	outp(0x3C4,0x04); outp(0x3C5,0x06);
	outp(0x3C4,0x00); outp(0x3C5,0x01); outp(0x3C2,a);
	outp(0x3C4,0x00); outp(0x3C5,0x03); outp(0x3D4,0x11); outp(0x3D5, inp(0x3D5) & 0x7F);

	outp(0x3D4,0x06); outp(0x3D5,b);
    outp(0x3D4,0x07); outp(0x3D5,c);
	outp(0x3D4,0x09); outp(0x3D5,0x41);
	outp(0x3D4,0x10); outp(0x3D5,d);
    outp(0x3D4,0x11); outp(0x3D5,e);
	outp(0x3D4,0x12); outp(0x3D5,f);
	outp(0x3D4,0x14); outp(0x3D5,0x00);
    outp(0x3D4,0x15); outp(0x3D5,g);
	outp(0x3D4,0x16); outp(0x3D5,h);
	outp(0x3D4,0x17); outp(0x3D5,0xE3);
}

static void setScaleY(int linesRepeat)
{
	outp(0x3D4,0x09);
	outp(0x3D5,0x40 | linesRepeat);
}


Video *setVideoMode(uint16 width, uint16 height, uint8 bpp, bool needsBuffer, bool unchained)
{
	Video *vm;
	for (uint8 i=0; i<VMODES_NUM; ++i)
	{
		vm = &vmodes[i];
		if (vm->width==width && vm->height==height && vm->bpp==bpp && vm->unchained==unchained) {
			if (needsBuffer) {
				if (vm->buffer!=0) free(vm->buffer);
				vm->buffer = (uint8*)malloc(((width * height * bpp) >> 3) * NUM_SOFT_BUFFERS);
			}
			if (vm->vesa) {
				setVesaMode(vm->mode);
			} else {
				setMode(vm->mode);
			}
			if (unchained) {
				setUnchainedMode(height);
				//setScaleY((400 / height) - 1);	// This might be if we wanted to do a scaled up 100 lines height or 50 after, but in regular ModeX or ModeY we already set things.
			}
			return vm;
		}
	}
	return 0;
}


void setPlaneMask(uint8 mask)
{
	outp(0x3C4,0x02);
	outp(0x3C5,mask);
}

void setSvgaWindow(uint16 window)
{
	_asm
	{
		mov ax,4F05h
		xor bx,bx
		mov dx,window
		int 10h
	}
}

void copyBufferToSvga(Video *vm)
{
	uint16 window = 0;
	int32 bytesLeft = (vm->width * vm->height * vm->bpp) >> 3;
	uint8 *src = (uint8*)vm->buffer;
	uint8 *dst = (uint8*)vm->vram;

	while (bytesLeft > 65535)
	{
		setSvgaWindow(window++);
		memcpy(dst, src, 65536);
		bytesLeft -= 65536;
		src += 65536;
	}
	setSvgaWindow(window);
	memcpy(dst, src, bytesLeft);
}

void clearFrame(Video *vm)
{
	uint8 *vram = getRenderBuffer(vm);
	const uint32 size = vm->width * vm->height;
	memset(vram, 0, size);
}

void updateFrame(Video *vm, bool vsync)
{
	if (vsync) waitForVsync();

	if (vm->unchained) {
		outp(0x3D4, 0x0C);
		outp(0x3D5, (vramPage * 16384) >> 8);

		vramPage = (vramPage + 1) & 3;	// quad buffer
		return;
	}

	if (vm->buffer==0) return;

	if (vm->vesa) {
		copyBufferToSvga(vm);
	} else {
		const uint32 size = (vm->width * vm->height * vm->bpp) >> 3;
		memcpy(vm->vram, &vm->buffer[vramPage * size], size);
		vramPage = (vramPage + 1) % NUM_SOFT_BUFFERS;
	}
}

uint8 *getRenderBuffer(Video *vm)
{
	if (vm->unchained) {
		return vm->vram + vramPage * 16384;
	}
	if (vm->buffer==0) {
		return vm->vram;
	} else {
		const uint32 size = (vm->width * vm->height * vm->bpp) >> 3;
		return &vm->buffer[vramPage * size];
	}
}

void waitForVsync()
{
	_asm
	{
		mov dx,3dah

		vsync_in:
			in al,dx
			and al,8
		jnz vsync_in

		vsync_out:
			in al,dx
			and al,8
		jz vsync_out
	}
}

void setPalFromTab(uint8 colstart, uint8 *paltab, uint16 colnum)
{
	_asm
	{
		mov dx,03c8h
		mov al,colstart
		out dx,al
		inc dx

		mov ax,colnum
		lea ecx,[eax + eax*2]

		mov ebx,paltab
		paltab_loop:
			mov al,[ebx]
			inc ebx
			out dx,al
		loopw paltab_loop
	}
}

void setSingleColorPal(uint8 color, uint8 r, uint8 g, uint8 b)
{
	_asm
	{
		mov dx,03c8h
		mov al,color
		out dx,al
		inc dx

		mov al,r
		out dx,al

		mov al,g
		out dx,al

		mov al,b
		out dx,al
	}
}

void setGradPal(int c0, int c1, int r0, int g0, int b0, int r1, int g1, int b1)
{
	const int dc = c1 - c0;
	float r = (float)r0;
	float g = (float)g0;
	float b = (float)b0;
	const float dr = (float)(r1 - r0) / (float)dc;
	const float dg = (float)(g1 - g0) / (float)dc;
	const float db = (float)(b1 - b0) / (float)dc;

	for (int c = c0; c <= c1; ++c) {
		CLAMP(r,0,63)
		CLAMP(g,0,63)
		CLAMP(b,0,63)

		setSingleColorPal(c, (uint8)r, (uint8)g, (uint8)b);

		r += dr;
		g += dg;
		b += db;
    }
}
