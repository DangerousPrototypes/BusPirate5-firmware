#Executable
OUTPUTBIN = decode

# common sources
COMMON_SRC := ./convert.c \
              ./timing.c \
              ./wave.c \
              ./ff_shim.c \
              ../interface/music_file.c \
              ../src/bitstream.c \
              ../src/buffers.c \
              ../src/dct32.c \
              ../src/dequant.c \
              ../src/dqchan.c \
              ../src/huffman.c \
              ../src/hufftabs.c \
              ../src/imdct.c \
              ../src/mp3dec.c \
              ../src/mp3tabs.c \
              ../src/polyphase.c \
              ../src/scalfact.c \
              ../src/stproc.c \
              ../src/subband.c \
              ../src/trigtabs.c \


ObjDir = ./build
BinDir = $(ObjDir)

CC = gcc
CFLAGS += -Og -ggdb -g3 -I../src -I../interface -I./ -DARM=1 -DOFF_TARGET
#CFLAGS += -O0 -ggdb -g3 -I../src -I../interface -I./ -DARM=1 -DOFF_TARGET
LFLAGS +=

# To decode whole file once, rather than run decoder for timed interval
#CFLAGS += -DNO_LOOP

# Create list of objects from source
COMMON_OBJS := $(patsubst %.c, $(ObjDir)/%.o, $(notdir $(COMMON_SRC)))

# Make sure the compiler looks in the correct directories
VPATH := $(sort  $(dir $(COMMON_SRC)))

# Build the executable in the build directory
$(BinDir)/$(OUTPUTBIN): $(COMMON_OBJS)
	$(CC) -o $(BinDir)/$(OUTPUTBIN) $(COMMON_OBJS) $(LFLAGS)

# Build the objects in the build directory
$(ObjDir)/%.o : %.c 
	@ mkdir -p $(ObjDir)
	$(CC) -c $< -o $@ $(CFLAGS) $(EXTRA_CFLAGS) 

PHONY: clean

clean:
	rm -f $(BinDir)/$(OUTPUTBIN)
	rm -f $(ObjDir)/*.o

