AM_CFLAGS = \
	$(WARN_CFLAGS)	\
	-I./include		\
	-I${includedir}

libnx_video_api_la_LTLIBRARIES = libnx_video_api.la
libnx_video_api_ladir = ${libdir}
libnx_video_api_la_LDFLAGS = -L${libdir} -ldrm

libnx_video_api_la_SOURCES = \
	nx_video_alloc.c	\
	nx_video_enc.c		\
	nx_video_dec.c

libnx_video_apiincludedir = ${includedir}
libnx_video_apiinclude_HEADERS = \
	include/linux/videodev2_nxp_media.h	\
	nx_video_alloc.h	\
	nx_video_api.h	\
	mm_types.h
