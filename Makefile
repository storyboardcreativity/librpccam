CC := g++
CFL := -shared -I.
TOOLCHAIN := arm-linux-gnueabihf-
OUT_NAME := librpccam.so
ARCH := -march=armv7-a
CLIBS := -lpthread -L. -l:libambaipc.so
SOURCE_FILES := camera_interface.cpp

SOURCE_FILES += proc/2/*.cpp
SOURCE_FILES += proc/3/*.cpp
SOURCE_FILES += proc/4/*.cpp
SOURCE_FILES += proc/5/*.cpp
SOURCE_FILES += proc/6/*.cpp
SOURCE_FILES += proc/7/*.cpp

all:
	$(TOOLCHAIN)$(CC) $(CFL) $(ARCH) $(SOURCE_FILES) $(CLIBS) -o $(OUT_NAME)

clear:
	rm -f *.o
	rm -f $(OUT_NAME)