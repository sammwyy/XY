XY_ROOT ?= ../..
OBJ_DIR := obj

# Core 2D modules
XY_OBJS_2D := xy_image.o xy_image_png.o xy_image_jpg.o xy_image_p2t.o \
              xy_sound.o xy_sound_wav.o xy_sound_snd.o \
              xy_alloc.o xy_graphics.o \
              xy_font.o xy_font_p2f.o xy_font_fnt.o \
              xy_input.o xy_audio.o xy_async.o xy_game.o

# 3D modules (mesh, camera, lighting, renderer)
XY_OBJS_3D := xy_mesh.o xy_camera.o xy_light.o xy_renderer3d.o

EE_OBJS := main.o $(XY_OBJS_2D) $(XY_OBJS_3D)
EE_OBJS := $(addprefix $(OBJ_DIR)/, $(EE_OBJS))

EE_CC = $(EE_CXX)

EE_INCS += \
    -I$(XY_ROOT)/src \
    -I$(XY_ROOT)/src/image \
    -I$(XY_ROOT)/src/sound \
    -I$(XY_ROOT)/src/graphics \
    -I$(XY_ROOT)/src/mesh \
    -I$(XY_ROOT)/src/font \
    -I$(XY_ROOT)/src/async \
    -I$(GSKIT)/include \
    -I$(PS2SDK)/ports/include

EE_CXXFLAGS += -std=gnu++17 -fno-exceptions -fno-rtti -Wall -Wextra -DXY_MEM_DEBUG
EE_LDFLAGS  += -L$(GSKIT)/lib -L$(PS2SDK)/ports/lib
EE_LIBS     += -laudsrv -lpad -lgskit_toolkit -lgskit -ldmakit -lpng -ljpeg -lz -lm -ldebug

VPATH += \
    $(XY_ROOT)/src \
    $(XY_ROOT)/src/image \
    $(XY_ROOT)/src/sound \
    $(XY_ROOT)/src/graphics \
    $(XY_ROOT)/src/mesh \
    $(XY_ROOT)/src/font \
    $(XY_ROOT)/src/async

.PHONY: all clean strip $(OBJ_DIR)

all: $(OBJ_DIR) $(EE_BIN) audsrv.irx

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# Custom rule to compile objects into the obj directory
$(OBJ_DIR)/%.o: %.cpp
	$(EE_CXX) $(EE_CXXFLAGS) $(EE_INCS) -c $< -o $@

strip: $(EE_BIN)
	$(EE_STRIP) --strip-all $(EE_BIN) -o $(basename $(EE_BIN))_stripped.elf

audsrv.irx:
	cp $(PS2SDK)/iop/irx/audsrv.irx .

clean:
	rm -f $(EE_BIN) $(basename $(EE_BIN))_stripped.elf audsrv.irx
	rm -rf $(OBJ_DIR)

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
