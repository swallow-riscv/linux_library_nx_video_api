/*
 * Copyright (C) 2016  Nexell Co., Ltd.
 * Author: SeongO, Park <ray@nexell.co.kr>
 */

#ifndef __NX_VIDEO_ALLOC_H__
#define __NX_VIDEO_ALLOC_H__


#ifdef	__cplusplus
extern "C" {
#endif

#include <stdint.h>

#define	NX_MAX_PLANES	4
#define USE_ION_ALLOCATOR

//
//	Nexell Private Memory Type
//
typedef struct
{
#ifndef USE_ION_ALLOCATOR
	int			drmFd;		//	DRM Device Handle
	int			dmaFd;		//	DMA Memory Handle
	int			gemFd;		//	GEM Handle
	uint32_t	flink;		//	flink name
#else
	int			sharedFd;	//	ION Shared fd
#endif

	int32_t		size;		//	Allocate Size
	int32_t		align;		//	Start Address Align
	void		*pBuffer;	//	Virtual Address Pointer
	void		*pReserved;	//	For debugging or future user
} NX_MEMORY_INFO, *NX_MEMORY_HANDLE;


//
//	Nexell Private Video Memory Type
//
typedef struct
{
	int32_t		width;			//	Video Image's Width
	int32_t		height;			//	Video Image's Height
	int32_t		align;			//	Start address align
	int32_t		planes;			//	Number of valid planes
	uint32_t	format;			//	Pixel Format(N/A)

#ifndef USE_ION_ALLOCATOR
	int			drmFd;						//	Drm Device Handle
	int			dmaFd[NX_MAX_PLANES];		//	DMA memory Handle
	int			gemFd[NX_MAX_PLANES];		//	GEM Handle
	uint32_t	flink[NX_MAX_PLANES];		//	flink name
#else
	int			sharedFd[NX_MAX_PLANES];	//	Each plane's ion shared fd.
#endif

	int32_t		size[NX_MAX_PLANES];		//	Each plane's size.
	int32_t		stride[NX_MAX_PLANES];		//	Each plane's stride.
	void		*pBuffer[NX_MAX_PLANES];	//	Virtual Address Pointer
	void		*pReserved[NX_MAX_PLANES];	//	for debugging or future user
} NX_VID_MEMORY_INFO, *NX_VID_MEMORY_HANDLE;

//	Nexell Private Memory Allocator
NX_MEMORY_INFO *NX_AllocateMemory( int size, int align );
void NX_FreeMemory( NX_MEMORY_INFO *pMem );

//	Video Specific Allocator Wrapper
NX_VID_MEMORY_INFO * NX_AllocateVideoMemory( int width, int height, int32_t planes, uint32_t format, int align );
void NX_FreeVideoMemory( NX_VID_MEMORY_INFO *pMem );

int NX_MapMemory( NX_MEMORY_INFO *pMem );
int NX_UnmapMemory( NX_MEMORY_INFO *pMem );

int NX_MapVideoMemory( NX_VID_MEMORY_INFO *pMem );
int NX_UnmapVideoMemory( NX_VID_MEMORY_INFO *pMem );

#ifndef USE_ION_ALLOCATOR
int NX_GetGEMHandles( int drmFd, NX_VID_MEMORY_INFO *pMem, uint32_t handles[NX_MAX_PLANES] );
int NX_GetGemHandle( int drmFd, NX_VID_MEMORY_INFO *pMem, int32_t plane );
#endif

#ifdef ANDROID
#include <gralloc_priv.h>
#include <hardware/gralloc.h>

int NX_PrivateHandleToVideoMemory( struct private_handle_t const *pHandle, NX_VID_MEMORY_INFO *pMemInfo );
#endif

#ifdef	__cplusplus
};
#endif

#endif	//	__NX_VIDEO_ALLOC_H__
