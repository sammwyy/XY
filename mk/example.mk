XY_ROOT ?= ../..

EE_OBJS := main.o xy_image.o xy_image_png.o xy_image_jpg.o xy_image_p2t.o xy_sound.o xy_sound_wav.o xy_sound_snd.o xy_alloc.o xy_graphics.o xy_font.o xy_font_p2f.o xy_font_fnt.o xy_input.o xy_audio.o xy_async.o xy_game.o
EE_CC = $(EE_CXX)
EE_INCS += -I$(XY_ROOT)/src -I$(XY_ROOT)/src/image -I$(XY_ROOT)/src/sound -I$(XY_ROOT)/src/graphics -I$(XY_ROOT)/src/font -I$(XY_ROOT)/src/async -I$(GSKIT)/include -I$(PS2SDK)/ports/include
EE_CXXFLAGS += -std=gnu++17 -fno-exceptions -fno-rtti -Wall -Wextra
EE_LDFLAGS += -L$(GSKIT)/lib -L$(PS2SDK)/ports/lib
EE_LIBS += -laudsrv -lpad -lgskit_toolkit -lgskit -ldmakit -lpng -ljpeg -lz -lm -ldebug
VPATH += $(XY_ROOT)/src $(XY_ROOT)/src/image $(XY_ROOT)/src/sound $(XY_ROOT)/src/graphics $(XY_ROOT)/src/font $(XY_ROOT)/src/async

all: $(EE_BIN) audsrv.irx

audsrv.irx:
	cp $(PS2SDK)/iop/irx/audsrv.irx .

clean:
	rm -f $(EE_BIN) $(EE_OBJS) audsrv.irx

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
