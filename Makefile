CC := g++
CFL := -shared
TOOLCHAIN := arm-linux-gnueabihf-
OUT_NAME := librpccam.so
ARCH := -march=armv7-a
CLIBS := -lpthread -L. -l:libambaipc.so
SOURCE_FILES := camera_interface.cpp

all:
	$(TOOLCHAIN)$(CC) $(CFL) $(ARCH) $(SOURCE_FILES) $(CLIBS) -o $(OUT_NAME)

clear:
	rm -f *.o
	rm -f $(OUT_NAME)