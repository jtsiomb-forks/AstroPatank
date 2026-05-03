	section .text
	CPU 386
	bits 32

	%define SCR_UNCHAINED

	SCR_W equ 320
	SCR_H equ 200
	SCR_BPP equ 8

	%ifdef SCR_UNCHAINED
		SCR_BYTE_LENGTH equ (((SCR_W / 4) * SCR_BPP) / 8)
	%else
		SCR_BYTE_LENGTH equ ((SCR_W * SCR_BPP) / 8)
	%endif

	SCANLINE_STRUCT_SIZE equ 8	; sizeof(Scanline), if this changes and not be 1,2,4,8 you are f****ed


	extern _scanline
	extern _yScanlineMin
	extern _yScanlineMax

	global fillScanlinesAsm_

segment _TEXT use32 class=CODE
fillScanlinesAsm_:
	pusha

		mov edi,edx ; vram

		movzx ecx,al
		mov ch,cl
		mov eax,ecx
		shl eax,16
		or eax,ecx		; uint32 color32 = (color << 24) | (color << 16) | (color << 8) | color;

		mov ebx,[_yScanlineMin]
		mov ebp,[_yScanlineMax]
		sub ebp,ebx			; store yScanlineMax - yScanlineMin in EBP

		test ebp,ebp
		js aman
		jz aman

		lea esi,[_scanline + SCANLINE_STRUCT_SIZE * ebx]

		mov ecx,ebx
		%ifdef SCR_UNCHAINED
			; y * 80 = y * 64 + y * 16
			shl ebx,6
			shl ecx,4
		%else
			; y * 320 = y * 256 + y * 64
			shl ebx,8
			shl ecx,6
		%endif
		add ebx,ecx
		add edi,ebx		; uint8 *dstY = vram + VRAM_PIXEL_OFFSET(0,yScanlineMin);

		; In this point EBP=countY, EAX=color32, ESI=&scanline[yScanlineMin], EDI = &vram[yScanlineMin*320]
		; EBX,ECX,EDX are free now. ECX will be altered/used for REP STOSD

		; For later when we need to add ebx to edi, we move bx to dx, and high 16bit of edx is zero
		; Alternative as I learned later on would be to do movzx ebx,bx inside, but might be one cycle more on the inner loop, so no worth it
		xor edx,edx

		scanlineLoopY:
			push edi

			mov ebx,[esi]	; BX=x0, (BX>>16)=x1
			add esi,SCANLINE_STRUCT_SIZE

			mov ecx,ebx
			shr ecx,16		; BX=x0, CX=x1

			; if (x0 < 0) x0 = 0;
			test bx,bx
			jns noNegX0
				xor bx,bx
			noNegX0:

			; if (x1 > SCR_W - 1) x1 = SCR_W - 1;
			cmp cx,SCR_W
			jc noClampX1
				mov cx,SCR_W-1
			noClampX1:

			%ifdef SCR_UNCHAINED
				shr bx,2
				shr cx,2
			%endif

			mov dx,bx
			add edi,edx

			;movzx ebx,bx
			;add edi,ebx

			sub cx,bx	; CX = length

			jle afterRenderScanline
			cmp cx,5
			jc tinyWriteBeforeNextLine

			and bl,3	; Let's see left side if we need to write bytes
			jz noLeftPixels
				mov bh,4
				sub bh,bl
				mov bl,bh
				pixL:
					stosb
					dec bh
				jnz pixL
				sub cx,bx
			noLeftPixels:

			test cx,cx
			js afterRenderScanline
			mov dx,cx
			cmp cx,3
			jc noThickPixels
				shr cx,2
				rep stosd
			noThickPixels:

			and dl,3
			jz afterRenderScanline
				mov cl,dl
			tinyWriteBeforeNextLine:
				rep stosb

			afterRenderScanline:
			pop edi
			add edi,SCR_BYTE_LENGTH
			dec ebp
		jnz scanlineLoopY
aman:

	popa

	ret
