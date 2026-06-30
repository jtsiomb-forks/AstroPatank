obj = main.obj game.obj menu.obj fonts.obj video.obj timer.obj input.obj &
	sound.obj soundbpr.obj musplay.obj tinyfont.obj mathutil.obj gfxtools.obj &
	engine.obj r_line.obj r_poly.obj poly.obj draw.obj mesh.obj meshdata.obj &
	tile_3d.obj map1.obj g_input0.obj g_input1.obj
bin = astropat.exe

AS = nasm
CXX = wpp386
ASFLAGS = -fobj
CXXFLAGS = -3 -fpc -zq -otexan -oh -oi -ei -bt=DOS
LD = wlink
LDFLAGS = libpath muslib libr muslib3r.lib

$(bin): $(obj)
    $(LD) name $@ system stub32x option stack=16k file { *.obj } $(LDFLAGS)

.cpp.obj: .autodepend
    $(CXX) -fo=$@ $(CXXFLAGS) $<

.asm.obj:
	$(AS) $(ASFLAGS) -o $@ $[*.asm

!ifdef __UNIX__
clean: .symbolic
	rm -f $(obj) $(bin)
	rm -f *.err
!else
clean: .symbolic
    del *.obj
    del *.err
    del $(bin)
!endif
