AM_CFLAGS =

AM_CPPFLAGS = \
	$(WARN_CFLAGS)	\
	-I.				\
	-I${includedir}/libdrm \
	-I${includedir}/ffmpeg \
	-I${includedir}	\
	-I../include

bin_PROGRAMS = video_api_test

video_api_test_LDADD = \
	-L${libdir}			\
	-lavcodec			\
	-lavformat			\
	-lavutil			\
	-lswresample		\
	-ldrm				\
	-lnx_video_api

video_api_test_SOURCES = \
	CodecInfo.cpp		\
	DrmRender.cpp		\
	MediaExtractor.cpp	\
	NX_Queue.cpp		\
	NX_Semaphore.cpp	\
	Util.cpp			\
	VideoDecTest.cpp	\
	VideoEncTest.cpp	\
	main.cpp
