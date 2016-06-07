################################################################################
#
#	video_api_test
#
include ../buildcfg.mk

# Directory Path
INSTALL_PATH	:= ../../sysroot/bin
INC_PATH		:= ../../sysroot/include
LIB_PATH		:= ../../sysroot/lib

# Target Information
TARGET	:= video_api_test
FFMPEG_VERSION := ffmpeg-2.8.6-arm64

# Sources
COBJS  	:=
CPPOBJS	:= \
		MediaExtractor.o	\
		CodecInfo.o			\
		NX_Queue.o			\
		NX_Semaphore.o		\
		Util.o				\
		VideoDecTest.o		\
		VideoEncTest.o		\
		main.o
CPPOBJS += DrmRender.o
OBJS	:= $(COBJS) $(CPPOBJS)

# Add Include Path
INCLUDE += \
		-I./				\
		-I../include		\
		-I../src			\
		-I../src/nx_video_alloc \
		-I./ffmpeg/$(FFMPEG_VERSION)/include

INCLUDE += -I$(INC_PATH) -I$(INC_PATH)/libdrm

# Add Dependent Libraries
LIBRARY += -lstdc++ -lm

LIBRARY += \
		-L$(LIB_PATH)	\
		-L../src		\
		-lnx_video_api

# Add FFMPEG libraries
LIBRARY += \
		-L./ffmpeg/$(FFMPEG_VERSION)/lib	\
		-lavcodec -lavformat -lavutil -lswresample

LIBRARY += -L$(LIB_PATH) -ldrm -lkms

# Compile Options
CFLAGS	+= -g -pthread

# Build
all: $(TARGET)

$(TARGET):	depend $(OBJS)
	$(quiet)$(CC) $(CPPFLAGS) $(CFLAGS) $(OBJS) -o $@ $(LIBRARY)

install:
	@echo "$(ColorMagenta)[[[ Install $(TARGET) ]]]$(ColorEnd)"
	install -m 755 -d $(INSTALL_PATH)
	install -m 755 $(TARGET) $(INSTALL_PATH)

clean:
	@echo "$(ColorMagenta)[[[ Clean $(TARGET) ]]]$(ColorEnd)"
	rm -f $(COBJS) $(CPPOBJS) $(TARGET) .depend

distclean: clean
	rm -f $(INSTALL_PATH)/$(TARGET)

# Dependency
ifeq (.depend,$(wildcard .depend))
include .depend
endif

SRCS := $(COBJS:.o=.c) $(CPPOBJS:.o=.cpp)
INCS := $(INCLUDE)
depend dep:
	@echo "$(ColorMagenta)[[[ Build $(TARGET) ]]]$(ColorEnd)"
	$(quiet)$(CC) -M $(CFLAGS) $(INCS) $(SRCS) > .depend
