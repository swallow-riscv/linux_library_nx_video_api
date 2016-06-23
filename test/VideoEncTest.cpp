//------------------------------------------------------------------------------
//
//	Copyright (C) 2016 Nexell Co. All Rights Reserved
//	Nexell Co. Proprietary & Confidential
//
//	NEXELL INFORMS THAT THIS CODE AND INFORMATION IS PROVIDED "AS IS" BASE
//  AND	WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING
//  BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS
//  FOR A PARTICULAR PURPOSE.
//
//	Module		:
//	File		:
//	Description	:
//	Author		: 
//	Export		:
//	History		:
//
//------------------------------------------------------------------------------

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>	// open
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

#include <linux/videodev2.h>
#include <videodev2_nxp_media.h>

#include <nx_video_alloc.h>
#include <nx_video_api.h>

#include "Util.h"

#define ENABLE_DRM_DISPLAY
#define ENABLE_ASPECT_RATIO
#define SCREEN_WIDTH	(1080)
#define SCREEN_HEIGHT	(1920)

#ifdef ENABLE_DRM_DISPLAY
#include <drm_fourcc.h>
#include "DrmRender.h"
#endif

#define IMAGE_BUFFER_NUM	8
#define IMG_FORMAT			V4L2_PIX_FMT_YUV420M  //V4L2_PIX_FMT_NV12M
#define IMG_PLANE_NUM		1


static bool bExitLoop = false;


//----------------------------------------------------------------------------------------------------
//
//	Signal Handler
//
static void signal_handler( int32_t sig )
{
	printf("Aborted by signal %s (%d)..\n", (char*)strsignal(sig), sig);

	switch( sig )
	{
		case SIGINT :
			printf("SIGINT..\n"); 	break;
		case SIGTERM :
			printf("SIGTERM..\n");	break;
		case SIGABRT :
			printf("SIGABRT..\n");	break;
		default :
			break;
	}

	if( !bExitLoop )
		bExitLoop = true;
	else{
		usleep(1000000);	//	wait 1 seconds for double Ctrl+C operation
		exit(EXIT_FAILURE);
	}
}

static void register_signal( void )
{
	signal( SIGINT,  signal_handler );
	signal( SIGTERM, signal_handler );
	signal( SIGABRT, signal_handler );
}

//------------------------------------------------------------------------------
static int32_t LoadImage(uint8_t *pSrc, int32_t w, int32_t h, NX_VID_MEMORY_INFO *pImg)
{
	int32_t i, j;
	int32_t cWidth = 0, cHeight = 0;
	uint8_t *pDst, *pCb, *pCr, *pCbCr, *pCrCb;
	int32_t planeNum = pImg->planes;
	int32_t cStride = pImg->stride[1];
	int32_t cSize = 0;
	int32_t luSize = ((w + 31) & (~31)) * ((h + 15) & (~15));

	// Copy Lu
	pDst = (uint8_t *)pImg->pBuffer[0];
	for (i=0 ; i<h ; i++)
	{
		memcpy((void *)pDst, (void *)pSrc, w);
		pDst += pImg->stride[0];
		pSrc += w;
	}

	switch (pImg->format)
	{
	case V4L2_PIX_FMT_YUV420M:
	case V4L2_PIX_FMT_NV12M:
	case V4L2_PIX_FMT_NV21M:
	case DRM_FORMAT_YUV420:
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV21:
		cWidth = w / 2;
		cHeight = h / 2;
		if (planeNum == 1)
		{
			cStride = pImg->stride[0] / 2;
			cSize = cStride * ((h/2 + 15) & (~15));
		}
		break;

	case V4L2_PIX_FMT_YUV422M:
	case V4L2_PIX_FMT_NV16M:
	case V4L2_PIX_FMT_NV61M:
	case DRM_FORMAT_YUV422:
	case DRM_FORMAT_NV16:
	case DRM_FORMAT_NV61:
		cWidth = w / 2;
		cHeight = h;
		if (planeNum == 1)
		{
			cStride = pImg->stride[0] / 2;
			cSize = cStride * ((h + 15) & (~15));
		}
		break;

	case V4L2_PIX_FMT_YUV444M:
	case V4L2_PIX_FMT_NV24M:
	case V4L2_PIX_FMT_NV42M:
	case DRM_FORMAT_YUV444:
		cWidth = w;
		cHeight = h;
		if (planeNum == 1)
		{
			cStride = pImg->stride[0];
			cSize = luSize;
		}
	}

	pCb = pSrc;
	pCr = pSrc + (cWidth * cHeight);

	switch (pImg->format)
	{
	case V4L2_PIX_FMT_YUV420M:
	case V4L2_PIX_FMT_YUV422M:
	case V4L2_PIX_FMT_YUV444M:
	case DRM_FORMAT_YUV420:
	case DRM_FORMAT_YUV422:
	case DRM_FORMAT_YUV444:
		for (i=1 ; i<3 ; i++)
		{
			if (planeNum > 1)
				pDst = (uint8_t *)pImg->pBuffer[i];
			else if (i == 1)
				pDst = (uint8_t *)pImg->pBuffer[0] + luSize;
			else if (i == 2)
				pDst = (uint8_t *)pImg->pBuffer[0] + luSize + cSize;

			for (j=0 ; j<cHeight ; j++)
			{
				memcpy(pDst, pSrc, cWidth);
				pDst += cStride;
				pSrc += cWidth;
			}
		}
		break;

	case V4L2_PIX_FMT_NV12M:
	case V4L2_PIX_FMT_NV16M:
	case V4L2_PIX_FMT_NV24M:
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV16:
		if (planeNum > 1)
			pDst = (uint8_t *)pImg->pBuffer[1];	
		else 
			pDst = (uint8_t *)pImg->pBuffer[0] + luSize;

		for (i=0 ; i<cHeight ; i++)
		{
			pCbCr = pDst + cStride * i;

			for (j=0 ; j<cWidth ; j++)
			{
				*pCbCr++ = *pCb++;
				*pCbCr++ = *pCr++;
			}
		}
		break;

	case V4L2_PIX_FMT_NV21M:
	case V4L2_PIX_FMT_NV61M:
	case V4L2_PIX_FMT_NV42M:
	case DRM_FORMAT_NV21:
	case DRM_FORMAT_NV61:
		if (planeNum > 1)
			pDst = (uint8_t *)pImg->pBuffer[1];
		else 
			pDst = (uint8_t *)pImg->pBuffer[0] + luSize;

		for (i=0 ; i<cHeight ; i++)
		{
			pCrCb = pDst + cStride * i;
			
			for (j=0 ; j<cWidth ; j++)
			{
				*pCrCb++ = *pCr++;
				*pCrCb++ = *pCb++;
			}
		}
	}

	return 0;
}

static int32_t GetImgInfo(uint32_t format, int32_t lSize, int *Size)
{
	switch (format)
	{
	case V4L2_PIX_FMT_YUV420M:
	case V4L2_PIX_FMT_NV12M:
	case V4L2_PIX_FMT_NV21M:
	case DRM_FORMAT_YUV420:
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV21:
		*Size = lSize * 3 / 2;
		break;

	case V4L2_PIX_FMT_YUV422M:
	case V4L2_PIX_FMT_NV16M:
	case V4L2_PIX_FMT_NV61M:
	case DRM_FORMAT_YUV422:
	case DRM_FORMAT_NV16:
	case DRM_FORMAT_NV61:
		*Size = lSize * 2;
		break;

	case V4L2_PIX_FMT_YUV444M:
	case V4L2_PIX_FMT_NV24M:
	case V4L2_PIX_FMT_NV42M:
	case DRM_FORMAT_YUV444:
		*Size = lSize * 3;
		break;

	case V4L2_PIX_FMT_GREY:
		*Size = lSize;
		break;

	default :
		printf("The color format is not supported!!!");
		return -1;
	}

	return 0;
}

//------------------------------------------------------------------------------
static int32_t VpuEncPerfMain(CODEC_APP_DATA *pAppData)
{
	NX_V4L2ENC_HANDLE hEnc = NULL;
#ifdef ENABLE_DRM_DISPLAY
	DRM_DSP_HANDLE hDsp = NULL;
#endif
	NX_VID_MEMORY_HANDLE hImage[IMAGE_BUFFER_NUM];

	int32_t inWidth = pAppData->width;
	int32_t inHeight = pAppData->height;
	int32_t ret = 0, i;
	int32_t planes = IMG_PLANE_NUM;
	
	FILE *fpIn = fopen(pAppData->inFileName, "rb");
	FILE *fpOut = fopen(pAppData->outFileName, "wb");

	if (fpIn == NULL || fpOut == NULL)
	{
		printf("input file or output file open error!!\n");
		goto ENC_TERMINATE;
	}

	register_signal();

	//==============================================================================
	// DISPLAY INITIALIZATION
	//==============================================================================
	{
		int drmFd = open("/dev/dri/card0", O_RDWR);

#ifdef ENABLE_DRM_DISPLAY
		hDsp = CreateDrmDisplay(drmFd);
		DRM_RECT srcRect, dstRect;
#endif	//	ENABLE_DRM_DISPLAY

#ifdef ENABLE_DRM_DISPLAY
		srcRect.x = 0;
		srcRect.y = 0;
		srcRect.width = inWidth;
		srcRect.height = inHeight;
		dstRect.x = 0;
		dstRect.y = 0;
		dstRect.width = inWidth;
		dstRect.height = inHeight;

#ifdef ENABLE_ASPECT_RATIO
		double xRatio = (double)SCREEN_WIDTH/(double)inWidth;
		double yRatio = (double)SCREEN_HEIGHT/(double)inHeight;

		if (xRatio > yRatio)
		{
			dstRect.width = inWidth * yRatio;
			dstRect.height = SCREEN_HEIGHT;
			dstRect.x = abs(SCREEN_WIDTH - dstRect.width)/2;
		}
		else
		{
			dstRect.width = SCREEN_WIDTH;
			dstRect.height = inHeight * xRatio;
			dstRect.y = abs(SCREEN_HEIGHT - dstRect.height) / 2;
		}
#endif

		InitDrmDisplay(hDsp, 17, 22, DRM_FORMAT_YUV420, dstRect, srcRect );
#endif	//	ENABLE_DRM_DISPLAY
	}

	//==============================================================================
	// ENCODER INITIALIZATION
	//==============================================================================
	{
		NX_V4L2ENC_PARA encPara;
		uint8_t *pSeqData;
		int32_t seqSize;

		// Initialize Encoder
		if (pAppData->codec == 0)
		{
			pAppData->codec = V4L2_PIX_FMT_H264;
		} 
		else if (pAppData->codec == 1)
		{
			pAppData->codec = V4L2_PIX_FMT_MPEG4;
		}
		else if (pAppData->codec == 2)
		{
			pAppData->codec = V4L2_PIX_FMT_H263;
			encPara.profile = V4L2_MPEG_VIDEO_H263_PROFILE_P3;
		}
		else if (pAppData->codec == 3) 
		{
			pAppData->codec = V4L2_PIX_FMT_MJPEG;
		}
		else
		{
			printf("Fail, not supported codec!!!\n");
			goto ENC_TERMINATE;
		}

		hEnc = NX_V4l2EncOpen(pAppData->codec);
		if (hEnc == NULL)
		{
			printf("video encoder open is failed!!!");
			ret = -1;
			goto ENC_TERMINATE;
		}

		memset(&encPara, 0, sizeof(encPara));
		encPara.width = inWidth;
		encPara.height = inHeight;
		encPara.fpsNum = (pAppData->fpsNum) ? (pAppData->fpsNum) : (30);
		encPara.fpsDen = (pAppData->fpsDen) ? (pAppData->fpsDen) : (1);
		encPara.keyFrmInterval = (pAppData->gop) ? (pAppData->gop) : (encPara.fpsNum / encPara.fpsDen);
		encPara.bitrate = pAppData->kbitrate * 1024;
		encPara.maximumQp = (pAppData->maxQp) ? (pAppData->maxQp) : (0);
		encPara.rcVbvSize = (pAppData->vbv) ? (pAppData->vbv) : (0);
		encPara.disableSkip = 0;
		encPara.RCDelay = 0;
		encPara.gammaFactor = 0;
		encPara.initialQp = pAppData->qp;
		encPara.numIntraRefreshMbs = 0;
		encPara.searchRange = 0;
		encPara.enableAUDelimiter = 0;
		encPara.imgFormat = IMG_FORMAT;
		encPara.imgBufferNum = IMAGE_BUFFER_NUM;
		encPara.imgPlaneNum = planes;

		if (pAppData->codec == V4L2_PIX_FMT_MJPEG)
			encPara.jpgQuality = (pAppData->qp == 0) ? (90) : (pAppData->qp);

		ret = NX_V4l2EncInit(hEnc, &encPara);
		if (ret < 0) 
		{
			printf("video encoder initialization is failed!!!");
			goto ENC_TERMINATE;
		}

		ret = NX_V4l2EncGetSeqInfo(hEnc, &pSeqData, &seqSize);
		if (ret < 0) 
		{
			printf("Getting Sequence header is failed!!!");
			goto ENC_TERMINATE;
		}

		// Write Sequence Data
		if (seqSize > 0)
			fwrite((void *)pSeqData, 1, seqSize, fpOut);
	}

	//==============================================================================
	// PROCESS UNIT
	//==============================================================================
	{
		int32_t frmCnt = 0, readSize;
		uint64_t startTime, endTime;
		int32_t imgSize;
		uint8_t *pSrcBuf;

		GetImgInfo(IMG_FORMAT, inWidth*inHeight, &imgSize);

		// Allocate Output Buffer
		for (i = 0; i < IMAGE_BUFFER_NUM; i++)
		{
			hImage[i] = NX_AllocateVideoMemory(inWidth, inHeight, planes, IMG_FORMAT, 4096);

			if (hImage[i] == NULL)
			{
				printf("Failed to allocate %d image buffer\n", i);
				goto ENC_TERMINATE;
			}

			if (NX_MapVideoMemory(hImage[i]) != 0)
			{
				printf("Video Memory Mapping Failed\n");
				goto ENC_TERMINATE;
			}
		}

		pSrcBuf = (uint8_t *)malloc(imgSize);

		while (!bExitLoop)
		{
			NX_V4L2ENC_IN encIn;
			NX_V4L2ENC_OUT encOut;
			int32_t index = frmCnt % IMAGE_BUFFER_NUM;

			if (fpIn)
			{
				readSize = fread(pSrcBuf, 1, imgSize, fpIn);
				if (readSize != imgSize)
				{
					printf("End of Image\n");
					break;
				}

				LoadImage(pSrcBuf, inWidth, inHeight, hImage[index]);
			}

#ifdef ENABLE_DRM_DISPLAY
			UpdateBuffer(hDsp, hImage[index], NULL);
#endif

			memset(&encIn, 0, sizeof(NX_V4L2ENC_IN));
			memset(&encOut, 0, sizeof(NX_V4L2ENC_OUT));

			encIn.pImage = hImage[index];
			encIn.imgIndex = index;
			encIn.forcedIFrame = 0;
			encIn.forcedSkipFrame = 0;
			encIn.quantParam = pAppData->qp;

			startTime = NX_GetTickCount();
			ret = NX_V4l2EncEncodeFrame(hEnc, &encIn, &encOut);
			endTime = NX_GetTickCount();

			if (ret < 0)
			{
				printf("[%d] Frame]NX_V4l2EncEncodeFrame() is failed!!\n", frmCnt);
				break;
			}

			printf("[%04d Frm]Size = %5d, Type = %1d, TIme = %6lu\n", frmCnt, encOut.strmSize, encOut.frameType, (endTime - startTime));

			if (fpOut && encOut.strmSize > 0)
				fwrite((void *)encOut.strmBuf, 1, encOut.strmSize, fpOut);

			frmCnt++;
		}

		if (pSrcBuf)
			free(pSrcBuf);
	}

	//==============================================================================
	// TERMINATION
	//==============================================================================	
ENC_TERMINATE:
	if (hEnc)
		NX_V4l2EncClose(hEnc);

	for (i = 0; i < IMAGE_BUFFER_NUM; i++)
			NX_FreeVideoMemory(hImage[i]);

	if (fpIn)
		fclose(fpIn);

	if (fpOut)
		fclose(fpOut);

	printf("Encode End!!\n" );

	return ret;
}

int32_t VpuEncMain(CODEC_APP_DATA *pAppData)
{
	if (pAppData->inFileName)
		return VpuEncPerfMain(pAppData);
	else
		return -1;
		//return VpuCamEncMain(pAppData);		// TBD	
}
