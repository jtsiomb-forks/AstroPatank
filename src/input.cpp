#include <dos.h>
//#include <i86.h>
#include <stdio.h>
#include <conio.h>
#include <string.h>

#include "input.h"

#define INTERRUPT __interrupt __far
#define KEYBOARD_INTERRUPT	0x9

#define PIC1_CMD_PORT	0x20
#define OCW2_EOI		(1 << 5)

static void (INTERRUPT *prevKeyboardHandler)();
static void INTERRUPT myKeyboardInterrupt();

Buttons buttonsHeld;


static void clearKeyboardBuffer()
{
	_asm
	{
		// Safer?
		mov ax,0c00h
		int 21h

		// Hack
		/*push ds
		mov ax,0040h
		mov ds,ax
		mov ax,[ds:001ah]
		mov [ds:001ch],ax
		pop ds*/
	}
}

static uint8 getKey()
{
	uint8 keypress = 0;

	_asm
	{
		in al,60h
		mov [keypress],al

		// fix for XT keyboards?
		in al,61h
		mov ah,al
		or al,80h
		out 61h,al
		mov al,ah
		out 61h,al
	}

	return keypress;
}

static void keyCommands()
{
	uint8 key = getKey();
	bool released = key >> 7;
	key &= 127;
	switch (key) {
		case 75:	// Left arrow
		case 30:	// A
			buttonsHeld.left = !released;
		break;

		case 77:	// Right arrow
		case 32:	// D
			buttonsHeld.right = !released;
		break;

		case 72:	// Up arrow
		case 17:	// W
			buttonsHeld.up = !released;
		break;

		case 80:	// Down arrow
		case 31:	// S
		case 45:	// X
			buttonsHeld.down = !released;
		break;

		case 57:	// Space
			buttonsHeld.fire = !released;
		break;

		case 28:	// Return
			buttonsHeld.start = !released;
		break;

		case 15:	// TAB
			buttonsHeld.map = !released;
		break;

		case 1:	// esc
			buttonsHeld.escape = !released;
		break;

		default:
			//printf("%d\n", key);
		break;
	}

	clearKeyboardBuffer();
}

static void INTERRUPT myKeyboardInterrupt()
{
	keyCommands();

	outp(PIC1_CMD_PORT, OCW2_EOI);	// send end-of-interrupt
}

void initKeyboard()
{
	if(!prevKeyboardHandler) {
		// set our interrupt handler
		_disable();
		prevKeyboardHandler = _dos_getvect(KEYBOARD_INTERRUPT);
		_dos_setvect(KEYBOARD_INTERRUPT, myKeyboardInterrupt);
		_enable();
	}

	memset(&buttonsHeld, 0, sizeof(Buttons));
}

void deinitKeyboard()
{
	if(prevKeyboardHandler) {
		// restore the original interrupt handler
		_disable();
		_dos_setvect(KEYBOARD_INTERRUPT, prevKeyboardHandler);
		_enable();
	}
}
