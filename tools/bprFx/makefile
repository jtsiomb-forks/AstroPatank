obj = main.obj video.obj input.obj fonts.obj soundbpr.obj gfxtools.obj
bin = demo.exe

AS = nasm
CPP = wpp386
ASFLAGS = -fobj
CXXFLAGS = -3 -fpc -otexan -oh -oi -ei -bt=DOS
LD = wlink
LDFLAGS = 

$(bin): $(obj)
    $(LD) name $@ system stub32x option stack=16k file { *.obj } $(LDFLAGS)

.cpp.obj: .autodepend
    $(CPP) $(CXXFLAGS) $<

.asm.obj:
    $(AS) $(ASFLAGS) -o $@ $[*.asm
clean: .symbolic
    del *.obj
    del *.err
    del $(bin)
