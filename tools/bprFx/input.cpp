#include <dos.h>
#include <stdio.h>
#include <conio.h>
#include <string.h>

#ifdef __DJGPP__
	#include <go32.h>
	#include <dpmi.h>
#endif

#include "input.h"

#define INTERRUPT __interrupt __far
#define KEYBOARD_INTERRUPT	0x9

#define PIC1_CMD_PORT	0x20
#define OCW2_EOI		(1 << 5)


#ifdef __DJGPP__
	static _go32_dpmi_seginfo oldKeyboardHandler;
	static _go32_dpmi_seginfo newKeyboardHandler;
#else
	static void (INTERRUPT *oldKeyboardInterrupt)();
	static void INTERRUPT newKeyboardInterrupt();
#endif

Buttons buttonsHeld;


static void clearKeyboardBuffer()
{
	#ifdef __DJGPP__
		union REGS regs;

		regs.h.ah = 0x0c;
		regs.h.al = 0x00;
		int86(0x21, &regs, &regs);
	#else
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
	#endif
}

static uint8 getKey()
{
	uint8 keypress = 0;

	#ifdef __DJGPP__
		__asm__ __volatile__ (
			"inb $0x60, %%al\n\t"
			"movb %%al, %0\n\t"

			// fix for XT keyboards?
			"inb $0x61, %%al\n\t"
			"movb %%al, %%ah\n\t"
			"orb $0x80, %%al\n\t"
			"outb %%al, $0x61\n\t"
			"movb %%ah, %%al\n\t"
			"outb %%al, $0x61"
			: "=q" (keypress)
			:
			: "eax", "memory"
		);
	#else
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
	#endif

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

		case 54:	// Right Shift
			buttonsHeld.select = !released;
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

#ifdef __DJGPP__
static void newKeyboardInterrupt()
#else
static void INTERRUPT newKeyboardInterrupt()
#endif
{
	keyCommands();

	#ifndef __DJGPP__
		outp(PIC1_CMD_PORT, OCW2_EOI);	// send end-of-interrupt
	#endif
}

void initKeyboard()
{
	_disable();

	#ifdef __DJGPP__
		_go32_dpmi_get_protected_mode_interrupt_vector(KEYBOARD_INTERRUPT, &oldKeyboardHandler);

		newKeyboardHandler.pm_offset   = (unsigned long)newKeyboardInterrupt;
		newKeyboardHandler.pm_selector = _go32_my_cs();

		_go32_dpmi_chain_protected_mode_interrupt_vector(KEYBOARD_INTERRUPT, &newKeyboardHandler);
	#else
		oldKeyboardInterrupt = _dos_getvect(KEYBOARD_INTERRUPT);
		_dos_setvect(KEYBOARD_INTERRUPT, newKeyboardInterrupt);
	#endif

	memset(&buttonsHeld, 0, sizeof(Buttons));

	_enable();
}

void deinitKeyboard()
{
	_disable();

	#ifdef __DJGPP__
		_go32_dpmi_set_protected_mode_interrupt_vector(KEYBOARD_INTERRUPT, &oldKeyboardHandler);
	#else
		_dos_setvect(KEYBOARD_INTERRUPT, oldKeyboardInterrupt);
	#endif

	_enable();
}
