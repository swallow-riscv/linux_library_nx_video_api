/*
 *	Copyright (C) 2016 Nexell Co. All Rights Reserved
 *	Nexell Co. Proprietary & Confidential
 *
 *	NEXELL INFORMS THAT THIS CODE AND INFORMATION IS PROVIDED "AS IS" BASE
 *  AND	WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING
 *  BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS
 *  FOR A PARTICULAR PURPOSE.
 *
 *	File		: nx_video_api.c
 *	Brief		: V4L2 Video En/Decoder
 *	Author		: SungWon Jo (doriya@nexell.co.kr)
 *	History		: 2016.04.25 : Create
 */

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/ioctl.h>

#include <linux/videodev2.h>
#include <linux/videodev2_nxp_media.h>

#include <nx_video_alloc.h>
#include <nx_video_api.h>

/*----------------------------------------------------------------------------*/
#define NX_V4L2_DEC_NAME		"nx-vpu-dec"
#define VIDEODEV_MINOR_MAX		63
#define STREAM_BUFFER_NUM		1


struct NX_V4L2DEC_INFO {
	int fd;
	uint32_t codecType;
	int32_t width;
	int32_t height;

	int32_t 	useExternalFrameBuffer;
	int32_t 	numFrameBuffers;
	NX_VID_MEMORY_HANDLE hImage[MAX_FRAME_BUFFER_NUM];

	NX_MEMORY_HANDLE hStream[STREAM_BUFFER_NUM];

	IMG_DISP_INFO dispInfo;

	int32_t planesNum;

	int32_t frameCnt;
};


/*
 *		Find Device Node
 */

/*----------------------------------------------------------------------------*/
static int32_t V4l2DecOpen(void)
{
	int fd = -1;

	bool found = false;
	struct stat s;
	FILE *stream_fd;
	char filename[64], name[64];
	int32_t i = 0;

	do
	{
		if (i > VIDEODEV_MINOR_MAX)
			break;

		/* video device node */
		sprintf(filename, "/dev/video%d", i);

		/* if the node is video device */
		if ((lstat(filename, &s) == 0) && S_ISCHR(s.st_mode) && ((int)((unsigned short)(s.st_rdev) >> 8) == 81))
		{
			/* open sysfs entry */
			sprintf(filename, "/sys/class/video4linux/video%d/name", i);
			stream_fd = fopen(filename, "r");
			if (stream_fd == NULL) {
				printf("failed to open sysfs entry for videodev \n");
				i++;
				continue;
			}

			/* read sysfs entry for device name */
			if (fgets(name, sizeof(name), stream_fd) == 0)
			{
				printf("failed to read sysfs entry for videodev\n");
			}
			else
			{
				if (strncmp(name, NX_V4L2_DEC_NAME, strlen(NX_V4L2_DEC_NAME)) == 0)
				{
					printf("node found for device %s: /dev/video%d \n", NX_V4L2_DEC_NAME, i);
					found = true;
				}
			}

			fclose(stream_fd);
		}

		i++;
	} while (found == false);

	if (found)
	{
		sprintf(filename, "/dev/video%d", i - 1);
		fd = open(filename,  O_RDWR);
	}

	return fd;
}


/*
 *		V4L2 Decoder
 */

/*----------------------------------------------------------------------------*/
NX_V4L2DEC_HANDLE NX_V4l2DecOpen(uint32_t codecType)
{
	NX_V4L2DEC_HANDLE hDec = (NX_V4L2DEC_HANDLE)malloc(sizeof(struct NX_V4L2DEC_INFO));

	memset(hDec, 0, sizeof(struct NX_V4L2DEC_INFO));

	hDec->fd = V4l2DecOpen();
	if (hDec->fd <= 0)
	{
		printf("Failed to open video decoder device\n");
		goto ERROR_EXIT;
	}

	/* Query capabilities of Device */
	{
		struct v4l2_capability cap;

		memset(&cap, 0, sizeof(cap));

		if (ioctl(hDec->fd, VIDIOC_QUERYCAP, &cap) != 0)
		{
			printf("failed to ioctl: VIDIOC_QUERYCAP\n");
			goto ERROR_EXIT;
		}
	}

	hDec->codecType = codecType;

	return hDec;

ERROR_EXIT:
	if(hDec)
		free(hDec);

	return NULL;
}

/*----------------------------------------------------------------------------*/
int32_t NX_V4l2DecClose(NX_V4L2DEC_HANDLE hDec)
{
	enum v4l2_buf_type type;
	int32_t ret = 0, i;

	if (NULL == hDec)
	{
		printf("Fail, Invalid Handle.\n");
		return -1;
	}

	type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	if (ioctl(hDec->fd, VIDIOC_STREAMOFF, &type) != 0)
	{
		printf("failed to ioctl: VIDIOC_STREAMOFF(Stream)\n");
		return -1;
	}

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	if (ioctl(hDec->fd, VIDIOC_STREAMOFF, &type) != 0)
	{
		printf("failed to ioctl: VIDIOC_STREAMOFF(Image)\n");
		return -1;
	}

	if (hDec->useExternalFrameBuffer == 0)
	{
		for (i = 0; i < hDec->numFrameBuffers; i++ )
		{
			if (hDec->hImage[i])
			{
				NX_FreeVideoMemory(hDec->hImage[i]);
				hDec->hImage[i] = NULL;
			}
		}
	}

	for (i=0 ; i<STREAM_BUFFER_NUM ; i++)
		NX_FreeMemory(hDec->hStream[i]);

	close(hDec->fd);

	free(hDec);

	return ret;
}

/*----------------------------------------------------------------------------*/
int32_t NX_V4l2DecParseVideoCfg(NX_V4L2DEC_HANDLE hDec, NX_V4L2DEC_SEQ_IN *pSeqIn, NX_V4L2DEC_SEQ_OUT *pSeqOut)
{
	int imgWidth = pSeqIn->width;
	int imgHeight = pSeqIn->height;
	int i;

	memset(pSeqOut, 0, sizeof(NX_V4L2DEC_SEQ_OUT));

	if (NULL == hDec)
	{
		printf("Fail, Invalid Handle.\n");
		return -1;
	}

	// Set Stream Formet
	{
		struct v4l2_format fmt;

		memset(&fmt, 0, sizeof(fmt));

		fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
		fmt.fmt.pix_mp.pixelformat = hDec->codecType;

		if ((imgWidth == 0) || (imgHeight == 0))
			fmt.fmt.pix_mp.plane_fmt[0].sizeimage = MAX_IMAGE_WIDTH * MAX_IMAGE_HEIGHT * 3 / 4;
		else
			fmt.fmt.pix_mp.plane_fmt[0].sizeimage = imgWidth * imgHeight * 3 / 4;

		fmt.fmt.pix_mp.width = imgWidth;
		fmt.fmt.pix_mp.height = imgHeight;
		fmt.fmt.pix_mp.num_planes = 1;

		if (ioctl(hDec->fd, VIDIOC_S_FMT, &fmt) != 0)
		{
			printf("Failed to ioctx : VIDIOC_S_FMT(Input Stream)\n");
			return -1;
		}
	}

	// Malloc Stream Buffer
	{
		struct v4l2_requestbuffers req;
		int32_t buffCnt = STREAM_BUFFER_NUM;

		// IOCTL : VIDIOC_REQBUFS For Input Stream
		memset(&req, 0, sizeof(req));
		req.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
		req.count = buffCnt;
		req.memory = V4L2_MEMORY_DMABUF;//V4L2_MEMORY_USERPTR;

		if (ioctl(hDec->fd, VIDIOC_REQBUFS, &req) != 0)
		{
			printf("failed to ioctl: VIDIOC_REQBUFS(Input Stream)\n");
			return -1;
		}

		for (i=0 ; i<buffCnt ; i++)
		{
			hDec->hStream[i] = NX_AllocateMemory(MAX_IMAGE_WIDTH * MAX_IMAGE_HEIGHT * 3 / 4, 4096);
			if (hDec->hStream[i] == NULL)
			{
				printf("Failed to allocate stream buffer(%d, %d)\n", i, MAX_IMAGE_WIDTH * MAX_IMAGE_HEIGHT * 3 / 4);
				return -1;
			}

			if (NX_MapMemory(hDec->hStream[i]) != 0)
			{
				printf("Stream memory Mapping Failed\n");
				return -1;
			}
		}
	}

	// Set Parameter
	{
		if (hDec->codecType == V4L2_PIX_FMT_MJPEG)
		{
			struct v4l2_control ctrl;

			ctrl.id = V4L2_CID_MPEG_VIDEO_THUMBNAIL_MODE;
			ctrl.value = pSeqIn->thumbnailMode;

			if (ioctl(hDec->fd, VIDIOC_S_CTRL, &ctrl) != 0)
			{
				printf("failed to ioctl: Set Thumbnail Mode\n");
				return -1;
			}
		}
	}

	// Parser Sequence Header
	{
		struct v4l2_plane planes[1];
		struct v4l2_buffer buf;
		enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;

		memcpy((void *)hDec->hStream[0]->pBuffer, pSeqIn->seqBuf, pSeqIn->seqSize);

		memset(&buf, 0, sizeof(buf));
		buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
		buf.m.planes = planes;
		buf.length = 1;
		buf.memory = V4L2_MEMORY_DMABUF;//V4L2_MEMORY_USERPTR;
		buf.index = 0;

		buf.m.planes[0].m.userptr = (unsigned long)hDec->hStream[0]->pBuffer;
		buf.m.planes[0].m.fd = hDec->hStream[0]->dmaFd;
		buf.m.planes[0].length = hDec->hStream[0]->size;
		buf.m.planes[0].bytesused = pSeqIn->seqSize;
		buf.m.planes[0].data_offset = 0;

		buf.timestamp.tv_sec = pSeqIn->timeStamp/1000;
		buf.timestamp.tv_usec = (pSeqIn->timeStamp % 1000) * 1000;

		if (ioctl(hDec->fd, VIDIOC_QBUF, &buf) != 0)
		{
			printf("failed to ioctl: VIDIOC_QBUF(Header Stream)\n");
			return -1;
		}

		if (ioctl(hDec->fd, VIDIOC_STREAMON, &type) != 0) {
			printf("Fail, ioctl(): VIDIOC_STREAMON. (Input)\n");
			return -1;
		}

		memset(&buf, 0, sizeof(buf));
		buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
		buf.m.planes = planes;
		buf.length = 1;
		buf.memory = V4L2_MEMORY_DMABUF;//V4L2_MEMORY_USERPTR;

		if (ioctl(hDec->fd, VIDIOC_DQBUF, &buf) != 0)
		{
			printf("failed to ioctl: VIDIOC_DQBUF(Header Stream)\n");
			return -1;
		}

		pSeqOut->usedByte = buf.bytesused;

		if (buf.field == V4L2_FIELD_NONE)
			pSeqOut->interlace = NONE_FIELD;
		else if (V4L2_FIELD_INTERLACED)
			pSeqOut->interlace = FIELD_INTERLACED;
	}

	/* Get Image Information */
	{
		struct v4l2_format fmt;
		struct v4l2_crop crop;

		memset(&fmt, 0, sizeof(fmt));
		fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

		if (ioctl(hDec->fd, VIDIOC_G_FMT, &fmt) != 0)
		{
			printf("Fail, ioctl(): VIDIOC_G_FMT.\n");
			return -1;
		}

		pSeqOut->imgFourCC = fmt.fmt.pix_mp.pixelformat;
		pSeqOut->width = fmt.fmt.pix_mp.width;
		pSeqOut->height = fmt.fmt.pix_mp.height;
		pSeqOut->minBuffers = fmt.fmt.pix_mp.reserved[1];
		hDec->numFrameBuffers = pSeqOut->minBuffers;

		memset(&crop, 0, sizeof(crop));
		crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

		if (ioctl(hDec->fd, VIDIOC_G_CROP, &crop) != 0)
		{
			printf("Fail, ioctl(): VIDIOC_G_CROP\n");
			return -1;
		}

		pSeqOut->dispInfo.dispLeft = crop.c.left;
		pSeqOut->dispInfo.dispTop = crop.c.top;
		pSeqOut->dispInfo.dispRight = crop.c.left + crop.c.width;
		pSeqOut->dispInfo.dispBottom = crop.c.top + crop.c.height;
	}

	return 0;
}

/*----------------------------------------------------------------------------*/
int32_t NX_V4l2DecInit(NX_V4L2DEC_HANDLE hDec, NX_V4L2DEC_SEQ_IN *pSeqIn)
{
	/* Set Output Image */
	{
		struct v4l2_format fmt;

		memset(&fmt, 0, sizeof(fmt));
		fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		fmt.fmt.pix_mp.pixelformat = pSeqIn->imgFormat;
		fmt.fmt.pix_mp.width = pSeqIn->width;
		fmt.fmt.pix_mp.height = pSeqIn->height;
		fmt.fmt.pix_mp.num_planes = pSeqIn->imgPlaneNum;

		if (ioctl(hDec->fd, VIDIOC_S_FMT, &fmt) != 0)
		{
			printf("failed to ioctl: VIDIOC_S_FMT(Output Yuv)\n");
			return -1;
		}

		hDec->planesNum = pSeqIn->imgPlaneNum;
	}

	/* Malloc Output Image */
	{
		struct v4l2_requestbuffers req;
		struct v4l2_plane planes[3];
		struct v4l2_buffer buf;
		enum v4l2_buf_type type;
		int32_t imgBuffCnt, i, j;

		/* Calculate Buffer Number */
		if (pSeqIn->pMemHandle == NULL)
		{
			hDec->useExternalFrameBuffer = false;
 			imgBuffCnt = hDec->numFrameBuffers + pSeqIn->numBuffers;
		}
		else
		{
			hDec->useExternalFrameBuffer = true;
			if (2 > pSeqIn->numBuffers - hDec->numFrameBuffers)
				printf("External Buffer too small.(min=%d, buffers=%d)\n", hDec->numFrameBuffers, pSeqIn->numBuffers );

			imgBuffCnt = pSeqIn->numBuffers;
		}
		hDec->numFrameBuffers = imgBuffCnt;

		/* Request Output Buffer */
		memset(&req, 0, sizeof(req));
		req.type	= V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		req.count = imgBuffCnt;
		req.memory = V4L2_MEMORY_DMABUF;

		if (ioctl(hDec->fd, VIDIOC_REQBUFS, &req) != 0)
		{
			printf("failed to ioctl: VIDIOC_REQBUFS(Output YUV)\n");
			return -1;
		}

		memset(&buf, 0, sizeof(buf));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		buf.m.planes = planes;
		buf.length = pSeqIn->imgPlaneNum;
		buf.memory = V4L2_MEMORY_DMABUF;

		/* Allocate Buffer(Internal or External) */
		for (i=0 ; i<imgBuffCnt ; i++)
		{
			if (true == hDec->useExternalFrameBuffer)
			{
				hDec->hImage[i] = pSeqIn->pMemHandle[i];
			}
			else
			{
				hDec->hImage[i] = NX_AllocateVideoMemory(pSeqIn->width, pSeqIn->height, pSeqIn->imgPlaneNum, pSeqIn->imgFormat, 4096);
				if (hDec->hImage[i] == NULL)
				{
					printf("Failed to allocate image buffer(%d, %d, %d)\n", i, pSeqIn->width, pSeqIn->height);
					return -1;
				}

				if (NX_MapVideoMemory(hDec->hImage[i]) != 0)  {
					printf("Video Memory Mapping Failed\n");
					return -1;
				}
			}

			buf.index = i;

			for (j=0 ; j<(int32_t)pSeqIn->imgPlaneNum; j++)
			{
				buf.m.planes[j].m.fd = hDec->hImage[i]->dmaFd[j];
				buf.m.planes[j].length = hDec->hImage[i]->size[j];
			}

			if (ioctl(hDec->fd, VIDIOC_QBUF, &buf) != 0)
			{
				printf("failed to ioctl: VIDIOC_QBUF(Output YUV - %d)\n", i);
				return -1;
			}
		}

		type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		if (ioctl(hDec->fd, VIDIOC_STREAMON, &type) != 0)
		{
			printf("failed to ioctl: VIDIOC_STREAMON\n");
			return -1;
		}
	}

	return 0;
}

/*----------------------------------------------------------------------------*/
int32_t NX_V4l2DecDecodeFrame(NX_V4L2DEC_HANDLE hDec, NX_V4L2DEC_IN *pDecIn, NX_V4L2DEC_OUT *pDecOut)
{
	struct v4l2_buffer buf;
	struct v4l2_plane planes[3];
	int idx = hDec->frameCnt % STREAM_BUFFER_NUM;

	if (NULL == hDec)
	{
		printf("Fail, Invalid Handle.\n");
		return -1;
	}

	/* Ready Input Stream */
	memcpy((void *)hDec->hStream[idx]->pBuffer, pDecIn->strmBuf, pDecIn->strmSize);

	/* Queue Input Buffer */
	memset(&buf, 0, sizeof(buf));
	buf.type	 = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	buf.m.planes = planes;
	buf.length = 1;
	buf.memory = V4L2_MEMORY_DMABUF;
	buf.index = idx;
	buf.timestamp.tv_sec	= pDecIn->timeStamp/1000;
	buf.timestamp.tv_usec	= (pDecIn->timeStamp % 1000) * 1000;
	buf.flags = pDecIn->eos ? 1 : 0;

	// buf.m.planes[0].m.userptr = (unsigned long)hStream->pBuffer;
	buf.m.planes[0].m.fd = hDec->hStream[idx]->dmaFd;
	buf.m.planes[0].length = hDec->hStream[idx]->size;
	buf.m.planes[0].bytesused = pDecIn->strmSize;
	buf.m.planes[0].data_offset = 0;

	if (ioctl(hDec->fd, VIDIOC_QBUF, &buf) != 0)
	{
		printf("Fail, ioctl(): VIDIOC_QBUF.(Input)\n");
		return -1;
	}

	if (pDecIn->strmSize > 0)
	{
		/* Dequeue Input ES Buffer -> Get Decoded Order Result */
		memset(&buf, 0, sizeof(buf));
		buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
		buf.m.planes = planes;
		buf.length = 1;
		buf.memory = V4L2_MEMORY_DMABUF;

		if (ioctl(hDec->fd, VIDIOC_DQBUF, &buf) != 0)
		{
			printf("Fail, ioctl(): VIDIOC_DQBUF.(Input)\n");
			return -1;
		}

		pDecOut->decIdx = buf.index;
		pDecOut->usedByte = buf.bytesused;
		pDecOut->outFrmReliable_0_100[DECODED_FRAME] = buf.reserved;
		pDecOut->timeStamp[DECODED_FRAME] = ((uint64_t)buf.timestamp.tv_sec)*1000 + buf.timestamp.tv_usec/1000;

		if (buf.reserved2 == V4L2_BUF_FLAG_KEYFRAME)
			pDecOut->picType[DECODED_FRAME] = PIC_TYPE_I;
		else if	(buf.reserved2 == V4L2_BUF_FLAG_PFRAME)
			pDecOut->picType[DECODED_FRAME] = PIC_TYPE_P;
		else if (buf.reserved2 == V4L2_BUF_FLAG_BFRAME)
			pDecOut->picType[DECODED_FRAME] = PIC_TYPE_B;
		else
			pDecOut->picType[DECODED_FRAME] = PIC_TYPE_UNKNOWN;

		if (buf.field == V4L2_FIELD_NONE)
			pDecOut->interlace[DECODED_FRAME] = NONE_FIELD;
		else if (buf.field == V4L2_FIELD_SEQ_TB)
			pDecOut->interlace[DECODED_FRAME] = TOP_FIELD_FIRST;
		else if (buf.field == V4L2_FIELD_SEQ_BT)
			pDecOut->interlace[DECODED_FRAME] = BOTTOM_FIELD_FIRST;
	}

	/* Dequeue Output YUV Buffer -> Get Display Order Result */
	memset(&buf, 0, sizeof(buf));
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	buf.m.planes = planes;
	buf.length = hDec->planesNum;
	buf.memory = V4L2_MEMORY_DMABUF;

	if (ioctl(hDec->fd, VIDIOC_DQBUF, &buf) != 0)
	{
		printf("Fail, ioctl(): VIDIOC_DQBUF(Output)\n");
		return -100;
	}

	pDecOut->dispIdx = buf.index;
	// pDecOut->dispInfo = &hDec->dispInfo;		// TBD.

	if (pDecOut->dispIdx >= 0)
	{
		pDecOut->hImg = *hDec->hImage[buf.index];
		pDecOut->timeStamp[DISPLAY_FRAME] = ((uint64_t)buf.timestamp.tv_sec)*1000 + buf.timestamp.tv_usec/1000;
		pDecOut->outFrmReliable_0_100[DISPLAY_FRAME] = buf.reserved;

		if (buf.reserved2 == V4L2_BUF_FLAG_KEYFRAME)
			pDecOut->picType[DISPLAY_FRAME] = PIC_TYPE_I;
		else if	(buf.reserved2 == V4L2_BUF_FLAG_PFRAME)
			pDecOut->picType[DISPLAY_FRAME] = PIC_TYPE_P;
		else if (buf.reserved2 == V4L2_BUF_FLAG_BFRAME)
			pDecOut->picType[DISPLAY_FRAME] = PIC_TYPE_B;
		else
			pDecOut->picType[DISPLAY_FRAME] = PIC_TYPE_UNKNOWN;

		if (buf.field == V4L2_FIELD_NONE)
			pDecOut->interlace[DISPLAY_FRAME] = NONE_FIELD;
		else if (buf.field == V4L2_FIELD_SEQ_TB)
			pDecOut->interlace[DISPLAY_FRAME] = TOP_FIELD_FIRST;
		else if (buf.field == V4L2_FIELD_SEQ_BT)
			pDecOut->interlace[DISPLAY_FRAME] = BOTTOM_FIELD_FIRST;
	}

	hDec->frameCnt++;

	if (pDecOut->dispIdx == -1)
		return -1;

	return 0;
}

/*----------------------------------------------------------------------------*/
int32_t NX_V4l2DecClrDspFlag(NX_V4L2DEC_HANDLE hDec, NX_VID_MEMORY_HANDLE hFrameBuf, int32_t iFrameIdx)
{
	struct v4l2_buffer buf;
	struct v4l2_plane planes[3];
	int32_t index = -1;
	int32_t i;

	if (NULL == hDec)
	{
		printf("Fail, Invalid Handle.\n");
		return -1;
	}

	if (iFrameIdx >= 0)
	{
		index = iFrameIdx;
	}
	else
	{
		/* Search Buffer Index */
		if (hFrameBuf != NULL)
		{
			for (i = 0; i < hDec->numFrameBuffers ; i++)
			{
				if (hFrameBuf == hDec->hImage[i])
				{
					index = i;
					break;
				}
			}
		}
	}

	if (index < 0)
	{
		printf("Fail, Invalid FrameBuffer or FrameIndex.\n");
		return -1;
	}

	memset(&buf, 0, sizeof(buf));
	buf.type	= V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	buf.index	= index;
	buf.m.planes= planes;
	buf.length	= hDec->planesNum;
	buf.memory	= V4L2_MEMORY_DMABUF;

	for (i = 0; i < hDec->planesNum; i++)
	{
		buf.m.planes[i].m.fd = hDec->hImage[index]->dmaFd[i];
		buf.m.planes[i].length = hDec->hImage[index]->size[i];
	}

	/* Queue Output Buffer */
	if (ioctl(hDec->fd, VIDIOC_QBUF, &buf) != 0)
	{
		printf("Fail, ioctl(): VIDIOC_QBUF.(Clear Display Index, index = %d)\n", index);
		return -1;
	}

	return 0;
}

/*----------------------------------------------------------------------------*/
int32_t NX_V4l2DecFlush(NX_V4L2DEC_HANDLE hDec)
{
	enum v4l2_buf_type type;

	if (NULL == hDec)
	{
		printf("Fail, Invalid Handle.\n");
		return -1;
	}

	type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	if (ioctl(hDec->fd, VIDIOC_STREAMOFF, &type) != 0)
	{
		printf("failed to ioctl: VIDIOC_STREAMOFF(Stream)\n");
		return -1;
	}

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	if (ioctl(hDec->fd, VIDIOC_STREAMOFF, &type) != 0)
	{
		printf("failed to ioctl: VIDIOC_STREAMOFF(Image)\n");
		return -1;
	}

	type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	if (ioctl(hDec->fd, VIDIOC_STREAMON, &type) != 0)
	{
		printf("Fail, ioctl(): VIDIOC_STREAMON. (Input)\n");
		return -1;
	}

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	if (ioctl(hDec->fd, VIDIOC_STREAMON, &type) != 0)
	{
		printf("failed to ioctl: VIDIOC_STREAMON\n");
		return -1;
	}

	{
		struct v4l2_plane planes[3];
		struct v4l2_buffer buf;
		int32_t i, j;

		memset(&buf, 0, sizeof(buf));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		buf.m.planes = planes;
		buf.length = hDec->planesNum;
		buf.memory = V4L2_MEMORY_DMABUF;

		for (i = 0 ; i < hDec->numFrameBuffers ; i++)
		{
			buf.index = i;

			for (j = 0 ; j < (int32_t)hDec->planesNum; j++)
			{
				buf.m.planes[j].m.fd = hDec->hImage[i]->dmaFd[j];
				buf.m.planes[j].length = hDec->hImage[i]->size[j];
			}

			if (ioctl(hDec->fd, VIDIOC_QBUF, &buf) != 0)
			{
				printf("failed to ioctl: VIDIOC_QBUF(Output YUV - %d)\n", i);
				return -1;
			}
		}
	}

	return 0;
}
