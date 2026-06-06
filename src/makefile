obj = main.obj game.obj menu.obj fonts.obj video.obj timer.obj input.obj sound.obj soundbpr.obj musplay.obj tinyfont.obj mathutil.obj gfxtools.obj engine.obj r_line.obj r_poly.obj poly.obj draw.obj mesh.obj meshdata.obj tile_3d.obj map1.obj
bin = demo.exe

AS = nasm
CPP = wpp386
ASFLAGS = -fobj
CXXFLAGS = -3 -fpc -otexan -oh -oi -ei -bt=DOS
LD = wlink
LDFLAGS = libpath .\muslib libr MUSLIB3R.LIB

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
