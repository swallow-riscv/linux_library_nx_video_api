#ifndef __UTIL_h__
#define __UTIL_h__

#include <stdint.h>

uint64_t NX_GetTickCount( void );
void dumpdata( void *data, int32_t len, const char *msg );


//	Encoder Application Data
typedef struct CODEC_APP_DATA {
	//	Input Options
	char *inFileName;			//	Input File Name
	int32_t	width;				//	Input YUV Image Width
	int32_t	height;				//	Input YUV Image Height
	int32_t fpsNum;				//	Input Image Fps Number
	int32_t fpsDen;				// 	Input Image Fps Density

	//	Output Options
	char *outFileName;			//	Output File Name
	char *outLogFileName;		//	Output Log File Name
	char *outImgName;			//	Output Reconstructed Image File Name

	int32_t kbitrate;			//	Kilo Bitrate
	int32_t gop;				//	GoP
	uint32_t codec;				//	0:H.264, 1:Mp4v, 2:H.263, 3:JPEG (def:H.264)
	int32_t qp;					//	Fixed Qp
	int32_t vbv;
	int32_t maxQp;
	int32_t RCAlgorithm;

	int32_t angle;

	//	Preview Options
	int32_t dspX;				//	Display X Axis Offset
	int32_t dspY;				//	Display Y Axis Offset
	int32_t	dspWidth;			//	Display Width
	int32_t dspHeight;			//	Dispplay Height

	int32_t deinterlaceMode;
} CODEC_APP_DATA;

#endif // __UTIL_h__