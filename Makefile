TARGET = emuz1500-solaris

CXX ?= /usr/local/bin/g++
SDL_CFLAGS ?= -I/usr/local/include/SDL2 -D_REENTRANT
SDL_LIBS ?= -L/usr/local/lib -lSDL2
GTK2_CFLAGS ?= $(shell pkg-config --cflags gtk+-2.0 2>/dev/null)
GTK2_LIBS ?= $(shell pkg-config --libs gtk+-2.0 2>/dev/null)

CXXFLAGS += -O3 -std=gnu++11 -mvis -mcpu=ultrasparc -mtune=ultrasparc
CXXFLAGS += -D_MZ1500 -D__BIG_ENDIAN__ -D__SOLARIS__
CXXFLAGS += -I./src -I./src/vm -I./src/vm/mz700 -I./src/solaris
CXXFLAGS += -include ./src/solaris/osd_compat.h
CXXFLAGS += $(SDL_CFLAGS) $(GTK2_CFLAGS) -MMD -MP

LDFLAGS += -L/usr/local/lib -Wl,-R,/usr/local/lib -L/usr/openwin/lib -Wl,-R,/usr/openwin/lib
LDLIBS += $(SDL_LIBS) $(GTK2_LIBS) -lX11 -lsocket -lnsl -lm -lrt -lpthread

COMMON_SRCS = \
	src/common.cpp \
	src/config.cpp \
	src/fileio.cpp \
	src/fifo.cpp

VM_SRCS = \
	src/vm/and.cpp \
	src/vm/cmu800.cpp \
	src/vm/datarec.cpp \
	src/vm/disk.cpp \
	src/vm/event.cpp \
	src/vm/i8253.cpp \
	src/vm/i8255.cpp \
	src/vm/io.cpp \
	src/vm/mb8877.cpp \
	src/vm/midi.cpp \
	src/vm/mz1p17.cpp \
	src/vm/mz700/cmos.cpp \
	src/vm/mz700/emm.cpp \
	src/vm/mz700/floppy.cpp \
	src/vm/mz700/joystick.cpp \
	src/vm/mz700/kanji.cpp \
	src/vm/mz700/keyboard.cpp \
	src/vm/mz700/memory.cpp \
	src/vm/mz700/mz700.cpp \
	src/vm/mz700/psg.cpp \
	src/vm/mz700/quickdisk.cpp \
	src/vm/mz700/ramfile.cpp \
	src/vm/noise.cpp \
	src/vm/not.cpp \
	src/vm/pcm1bit.cpp \
	src/vm/prnfile.cpp \
	src/vm/sn76489an.cpp \
	src/vm/z80.cpp \
	src/vm/z80pio.cpp \
	src/vm/z80sio.cpp

SOLARIS_SRCS = \
	src/solaris/osd.cpp \
	src/solaris/osd_console.cpp \
	src/solaris/osd_video.cpp \
	src/solaris/solaris_main.cpp

SRCS = $(COMMON_SRCS) $(VM_SRCS) $(SOLARIS_SRCS)
OBJS = $(SRCS:.cpp=.o)
DEPS = $(OBJS:.o=.d)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJS) $(LDFLAGS) $(LDLIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS) $(DEPS)

-include $(DEPS)
