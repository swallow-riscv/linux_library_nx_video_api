#include <stdint.h>
#include <stdlib.h>	//	malloc, free
#include <unistd.h>	//	close
#include <stdio.h>
#include <errno.h>	//	errno
#include <string.h>	//	strerror

#include <sys/types.h>	//	open
#include <sys/stat.h>
#include <fcntl.h>

#include <drm_fourcc.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <nx_video_alloc.h>

#include "DrmRender.h"


#define	MAX_DRM_BUFFERS		64

struct DRM_DSP_INFO
{
	int			drmFd;
	uint32_t	planeID;
	uint32_t	crtcID;
	uint32_t	format;
	DRM_RECT	srcRect;		//	Source RECT
	DRM_RECT	dstRect;		//	Destination RECT

	int32_t		numBuffers;
	uint32_t	bufferIDs[MAX_DRM_BUFFERS];

	NX_VID_MEMORY_INFO vidMem[MAX_DRM_BUFFERS];
	NX_VID_MEMORY_INFO prevMem;
};

static int32_t FindDrmBufferIndex( DRM_DSP_HANDLE hDsp, NX_VID_MEMORY_INFO *pMem )
{
	int32_t i=0;

	for( i = 0 ; i < hDsp->numBuffers ; i ++ )
	{
		if( hDsp->vidMem[i].dmaFd[0] == pMem->dmaFd[0] )
		{
			//	already added memory
			return i;
		}
	}
	return -1;
}


static int32_t AddDrmBuffer( DRM_DSP_HANDLE hDsp, NX_VID_MEMORY_INFO *pMem )
{
	int32_t err;
	int32_t newIndex = hDsp->numBuffers;
	//	Add
	uint32_t handles[4] = {0,};
	uint32_t pitches[4] = {0,};
	uint32_t offsets[4] = {0,};

	//	Set Pitches
	pitches[0] = pMem->stride[0];
	pitches[1] = pMem->stride[1];
	pitches[2] = pMem->stride[2];
	pitches[3] = 0;
	offsets[0] = offsets[1] = offsets[2] = offsets[3] = 0;
	hDsp->bufferIDs[newIndex] = 0;

	//	Get gem Handel
	NX_GetGEMHandles( hDsp->drmFd, pMem, handles );

	// printf("hDsp->format = %c%c%c%c\n", 
	// 	hDsp->format&0xff,
	// 	(hDsp->format>> 8)&0xff,
	// 	(hDsp->format>>16)&0xff,
	// 	(hDsp->format>>24)&0xff );
	printf("Fd(%d), Width(%d), Height(%d), Stride(%d,%d,%d) \n", 
		hDsp->drmFd, pMem->width, pMem->height, pMem->stride[0], pMem->stride[1], pMem->stride[2]);

	err = drmModeAddFB2( hDsp->drmFd, pMem->width, pMem->height,
		DRM_FORMAT_YUV420, handles, pitches, offsets, &hDsp->bufferIDs[newIndex], 0);
	if( err < 0 )
	{
		printf("drmModeAddFB2() failed !(%d)\n", err);
		return -1;
	}

	hDsp->vidMem[newIndex] = *pMem;
	hDsp->numBuffers ++;

	return newIndex;
}
//
//	Create DRM Display
//
DRM_DSP_HANDLE CreateDrmDisplay( int fd )
{
	DRM_DSP_HANDLE hDsp;

	if( 0 > drmSetClientCap( fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1) )
	{
		printf("drmSetClientCap() failed !!!!\n");
		close( fd );
		return  NULL;
	}

	hDsp = (DRM_DSP_HANDLE)calloc(sizeof(struct DRM_DSP_INFO), 1);
	hDsp->drmFd = fd;

	return hDsp;
}


//
//	Initialize DRM Display Device
//
int32_t InitDrmDisplay( DRM_DSP_HANDLE hDsp, uint32_t planeId, uint32_t crtcId, uint32_t format,
						DRM_RECT srcRect, DRM_RECT dstRect )
{
	hDsp->planeID = planeId;
	hDsp->crtcID = crtcId;
	hDsp->format = format;
	hDsp->srcRect = srcRect;
	hDsp->dstRect = dstRect;
	return 0;
}


//
//	Update Buffer
//
int32_t UpdateBuffer( DRM_DSP_HANDLE hDsp, NX_VID_MEMORY_INFO *pMem, NX_VID_MEMORY_INFO **pOldMem )
{
	int32_t err;
	int32_t index = FindDrmBufferIndex( hDsp, pMem );
	// NX_VID_MEMORY_INFO *pAddedMem;

	if( 0 > index ){
		index = AddDrmBuffer( hDsp, pMem );
	}

	// pAddedMem = &hDsp->vidMem[index];

	// printf("Index = %d, drmFd(%d), planeId(%d), crtcId(%d), bufferIDs(%d), srcRect(%d,%d,%d,%d), dstRect(%d,%d,%d,%d)\n",
	// 	index, pAddedMem->drmFd, hDsp->planeID, hDsp->crtcID, hDsp->bufferIDs[index],
	// 	hDsp->srcRect.x, hDsp->srcRect.y, hDsp->srcRect.width, hDsp->srcRect.height,
	// 	hDsp->dstRect.x, hDsp->dstRect.y, hDsp->dstRect.width, hDsp->dstRect.height );

	err = drmModeSetPlane( hDsp->drmFd, hDsp->planeID, hDsp->crtcID, hDsp->bufferIDs[index], 0,
			hDsp->srcRect.x, hDsp->srcRect.y, hDsp->srcRect.width, hDsp->srcRect.height,
			hDsp->dstRect.x<<16, hDsp->dstRect.y<<16, hDsp->dstRect.width<<16, hDsp->dstRect.height<<16);

	if( 0 > err )
	{
		printf("drmModeSetPlane() Failed!!(%s(%d))\n", strerror(err), err );
	}

	// if( pOldMem )
	// {
	// 	if( hDsp->pPrevMem )
	// 		*pOldMem = hDsp->pPrevMem;
	// 	else
	// 		*pOldMem = NULL;
	// }
	// hDsp->pPrevMem = pMem;

	return err;
}


void DestroyDrmDisplay( DRM_DSP_HANDLE hDsp )
{
	int32_t i;
	for( i = 0; i < hDsp->numBuffers; i++ )
	{
		if( hDsp->bufferIDs[i] )
		{
			drmModeRmFB( hDsp->drmFd, hDsp->bufferIDs[i] );
		}
	}

	free( hDsp );
}
