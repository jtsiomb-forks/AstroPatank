obj = main.obj demo.obj video.obj timer.obj input.obj sound.obj musplay.obj tinyfont.obj mathutil.obj gfxtools.obj engine.obj r_line.obj r_poly.obj poly.obj draw.obj mesh.obj meshdata.obj fx_3d.obj tile_3d.obj
bin = demo.exe

AS = nasm
CPP = wpp386
ASFLAGS = -fobj
CXXFLAGS = -3 -fpc -otexan -oh -oi -ei -bt=DOS
LD = wlink
LDFLAGS = libpath .\muslib libr MUSLIB3R.LIB

$(bin): $(obj)
    $(LD) name $@ file { *.obj } $(LDFLAGS)

.cpp.obj: .autodepend
    $(CPP) $(CXXFLAGS) $<

.asm.obj:
    $(AS) $(ASFLAGS) -o $@ $[*.asm
clean: .symbolic
    del *.obj
    del *.err
    del $(bin)
