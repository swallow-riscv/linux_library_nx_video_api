LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

#
#	Compile Flags
#
USE_ION_ALLOCATOR := true

#
#	Compile Options
#
ifeq ($(USE_ION_ALLOCATOR),true)
LOCAL_CFLAGS += -DPLATFORM_SDK_VERSION=$(PLATFORM_SDK_VERSION) -DUSE_ION_ALLOCATOR
else
LOCAL_CFLAGS += -DARM64=1
endif

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/include		\
	system/core/include			\
	system/media/core/include 	\
	system/core/libsync/include \
	frameworks/native/include 	\
	frameworks/av/include 		\
	hardware/nexell/s5pxx18/gralloc

LOCAL_SHARED_LIBRARIES :=	\
	liblog 		\
	libcutils	\
	libhardware

ifeq ($(USE_ION_ALLOCATOR),true)
LOCAL_SRC_FILES := \
	nx_video_alloc_ion.cpp \
	nx_video_enc.c \
	nx_video_dec.c
else
LOCAL_SRC_FILES := \
	nx_video_alloc_drm.c \
	nx_video_enc.c \
	nx_video_dec.c
endif

# We need this because the current asm generates the following link error:
# requires unsupported dynamic reloc R_ARM_REL32; recompile with -fPIC
# Bug: 16853291
LOCAL_LDFLAGS := -Wl,-Bsymbolic
LOCAL_LDFLAGS += \
	-L$(LINUX_LIBS)

LOCAL_32_BIT_ONLY := true

LOCAL_MODULE := libnx_video_api
#LOCAL_MODULE_PATH := $(LOCAL_PATH)

LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

