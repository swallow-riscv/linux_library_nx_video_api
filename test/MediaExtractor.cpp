#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>		//	getopt & optarg
#include <sys/time.h>	//	gettimeofday

#include "MediaExtractor.h"

#include "CodecInfo.h"
#include "Util.h"

//==============================================================================
//Theora specific display information
typedef struct {
	int frameWidth;
	int frameHeight;
	int picWidth;
	int picHeight;
	int picOffsetX;
	int picOffsetY;
} ThoScaleInfo;

#ifndef MKTAG
#define MKTAG(a,b,c,d) (a | (b << 8) | (c << 16) | (d << 24))
#endif

#define PUT_LE32(_p, _var) \
	*_p++ = (unsigned char)((_var)>>0);  \
	*_p++ = (unsigned char)((_var)>>8);  \
	*_p++ = (unsigned char)((_var)>>16); \
	*_p++ = (unsigned char)((_var)>>24);

#define PUT_BE32(_p, _var) \
	*_p++ = (unsigned char)((_var)>>24);  \
	*_p++ = (unsigned char)((_var)>>16);  \
	*_p++ = (unsigned char)((_var)>>8); \
	*_p++ = (unsigned char)((_var)>>0);

#define PUT_LE16(_p, _var) \
	*_p++ = (unsigned char)((_var)>>0);  \
	*_p++ = (unsigned char)((_var)>>8);

#define PUT_BE16(_p, _var) \
	*_p++ = (unsigned char)((_var)>>8);  \
	*_p++ = (unsigned char)((_var)>>0);


#define	RCV_V2

#define	NX_MAX_NUM_SPS		3
#define	NX_MAX_SPS_SIZE		1024
#define	NX_MAX_NUM_PPS		3
#define	NX_MAX_PPS_SIZE		1024

#define	NX_MAX_NUM_CFG		6
#define	NX_MAX_CFG_SIZE		1024


typedef struct {
	int				version;
	int				profile_indication;
	int				compatible_profile;
	int				level_indication;
	int				nal_length_size;
	int				num_sps;
	int				sps_length[NX_MAX_NUM_SPS];
	unsigned char	sps_data  [NX_MAX_NUM_SPS][NX_MAX_SPS_SIZE];
	int				num_pps;
	int				pps_length[NX_MAX_NUM_PPS];
	unsigned char	pps_data  [NX_MAX_NUM_PPS][NX_MAX_PPS_SIZE];
} NX_AVCC_TYPE;

typedef struct {
	int				profile;
	int				level;
	int				nal_length_size;
	int				num_cfg;
	int				cfg_length[NX_MAX_NUM_CFG];
	unsigned char	cfg_data[NX_MAX_NUM_CFG][NX_MAX_CFG_SIZE];
} NX_HVC1_TYPE;

static int NX_ParseSpsPpsFromAVCC( unsigned char *extraData, int extraDataSize, NX_AVCC_TYPE *avcCInfo )
{
	int pos = 0;
	int i;
	int length;
	if( 1!=extraData[0] || 11>extraDataSize ){
		printf( "Error : Invalid \"avcC\" data\n" );
		return -1;
	}

	//	Parser "avcC" format data
	avcCInfo->version				= (int)extraData[pos];			pos++;
	avcCInfo->profile_indication	= (int)extraData[pos];			pos++;
	avcCInfo->compatible_profile	= (int)extraData[pos];			pos++;
	avcCInfo->level_indication		= (int)extraData[pos];			pos++;
	avcCInfo->nal_length_size		= (int)(extraData[pos]&0x03)+1;	pos++;
	//	parser SPSs
	avcCInfo->num_sps				= (int)(extraData[pos]&0x1f);	pos++;
	for( i=0 ; i<avcCInfo->num_sps ; i++){
		length = avcCInfo->sps_length[i] = (int)(extraData[pos]<<8)|extraData[pos+1];
		pos+=2;
		if( (pos+length) > extraDataSize ){
			printf("Error : extraData size too small(SPS)\n" );
			return -1;
		}
		memcpy( avcCInfo->sps_data[i], extraData+pos, length );
		pos += length;
	}

	//	parse PPSs
	avcCInfo->num_pps				= (int)extraData[pos];			pos++;
	for( i=0 ; i<avcCInfo->num_pps ; i++ ){
		length = avcCInfo->pps_length[i] = (int)(extraData[pos]<<8)|extraData[pos+1];
		pos+=2;
		if( (pos+length) > extraDataSize ){
			printf( "Error : extraData size too small(PPS)\n");
			return -1;
		}
		memcpy( avcCInfo->pps_data[i], extraData+pos, length );
		pos += length;
	}
	return 0;
}

static void NX_MakeH264StreamAVCCtoANNEXB( NX_AVCC_TYPE *avcc, unsigned char *pBuf, int *size )
{
	int i;
	int pos = 0;
	for( i=0 ; i<avcc->num_sps ; i++ )
	{
		pBuf[pos++] = 0x00;
		pBuf[pos++] = 0x00;
		pBuf[pos++] = 0x00;
		pBuf[pos++] = 0x01;
		memcpy( pBuf + pos, avcc->sps_data[i], avcc->sps_length[i] );
		pos += avcc->sps_length[i];
	}
	for( i=0 ; i<avcc->num_pps ; i++ )
	{
		pBuf[pos++] = 0x00;
		pBuf[pos++] = 0x00;
		pBuf[pos++] = 0x00;
		pBuf[pos++] = 0x01;
		memcpy( pBuf + pos, avcc->pps_data[i], avcc->pps_length[i] );
		pos += avcc->pps_length[i];
	}
	*size = pos;
}

static int NX_ParseHEVCCfgFromHVC1( unsigned char *extraData, int extraDataSize, NX_HVC1_TYPE *hevcInfo )
{
    uint8_t *ptr = (uint8_t *)extraData;

    // verify minimum size and configurationVersion == 1.
    if (extraDataSize < 7 || ptr[0] != 1) {
        return -1;
    }

	hevcInfo->profile = (ptr[1] & 31);
	hevcInfo->level = ptr[12];
	hevcInfo->nal_length_size = 1 + (ptr[14 + 7] & 3);

    ptr += 22;
    extraDataSize -= 22;

    int numofArrays = (char)ptr[0];

    ptr += 1;
    extraDataSize -= 1;
    int j = 0, i = 0, cnt = 0;

    for (i = 0; i < numofArrays; i++) {
        ptr += 1;
        extraDataSize -= 1;

        // Num of nals
        int numofNals = ptr[0] << 8 | ptr[1];

        ptr += 2;
        extraDataSize -= 2;

        for (j = 0;j < numofNals;j++) {
            if (extraDataSize < 2) {
                return -1;
            }

            int length = ptr[0] << 8 | ptr[1];

            ptr += 2;
            extraDataSize -= 2;

            if (extraDataSize < length) {
                return -1;
            }

			hevcInfo->cfg_length[cnt] = length;
			memcpy( hevcInfo->cfg_data[cnt], ptr, length );

            ptr += length;
            extraDataSize -= length;
			cnt += 1;
		}
    }

	hevcInfo->num_cfg = cnt;
    return 0;
}

static void NX_MakeHEVCStreamHVC1ANNEXB( NX_HVC1_TYPE *hvc1, unsigned char *pBuf, int *size )
{
	int i;
	int pos = 0;

	for( i=0 ; i<hvc1->num_cfg ; i++ )
	{
		pBuf[pos++] = 0x00;
		pBuf[pos++] = 0x00;
		pBuf[pos++] = 0x00;
		pBuf[pos++] = 0x01;
		memcpy( pBuf + pos, hvc1->cfg_data[i], hvc1->cfg_length[i] );
		pos += hvc1->cfg_length[i];
	}

	*size = pos;
}

static int PasreAVCStream( AVPacket *pkt, int nalLengthSize, unsigned char *buffer, int outBufSize )
{
	int nalLength;

	//	input
	unsigned char *inBuf = pkt->data;
	int inSize = pkt->size;
	int pos=0;

	//	'avcC' format
	do{
		nalLength = 0;

		if( nalLengthSize == 2 )
		{
			nalLength = inBuf[0]<< 8 | inBuf[1];
		}
		else if( nalLengthSize == 3 )
		{
			nalLength = inBuf[0]<<16 | inBuf[1]<<8  | inBuf[2];
		}
		else if( nalLengthSize == 4 )
		{
			nalLength = inBuf[0]<<24 | inBuf[1]<<16 | inBuf[2]<<8 | inBuf[3];
		}
		else if( nalLengthSize == 1 )
		{
			nalLength = inBuf[0];
		}

		inBuf  += nalLengthSize;
		inSize -= nalLengthSize;

		if( 0==nalLength || inSize<(int)nalLength )
		{
			printf("Error : avcC type nal length error (nalLength = %d, inSize=%d, nalLengthSize=%d)\n", nalLength, inSize, nalLengthSize);
			return -1;
		}

		//	put nal start code
		buffer[pos + 0] = 0x00;
		buffer[pos + 1] = 0x00;
		buffer[pos + 2] = 0x00;
		buffer[pos + 3] = 0x01;
		pos += 4;

		memcpy( buffer + pos, inBuf, nalLength );
		pos += nalLength;

		inSize -= nalLength;
		inBuf += nalLength;
	}while( 2<inSize );
	return pos;
}

static int MakeRvStream( AVPacket *pkt, AVStream *stream, unsigned char *buffer, int outBufSize )
{
	unsigned char *p = pkt->data;
    int cSlice, nSlice;
    int i, val, offset;
//	int has_st_code = 0;
	int size;

	cSlice = p[0] + 1;
	nSlice =  pkt->size - 1 - (cSlice * 8);
	size = 20 + (cSlice*8);

	PUT_BE32(buffer, nSlice);
	if (AV_NOPTS_VALUE == (long long)pkt->pts)
	{
		PUT_LE32(buffer, 0);
	}
	else
	{
		PUT_LE32(buffer, (int)((double)(pkt->pts/stream->time_base.den))); // milli_sec
	}
	PUT_BE16(buffer, stream->codec->frame_number);
	PUT_BE16(buffer, 0x02); //Flags
	PUT_BE32(buffer, 0x00); //LastPacket
	PUT_BE32(buffer, cSlice); //NumSegments
	offset = 1;
	for (i = 0; i < (int) cSlice; i++)
	{
		val = (p[offset+3] << 24) | (p[offset+2] << 16) | (p[offset+1] << 8) | p[offset];
		PUT_BE32(buffer, val); //isValid
		offset += 4;
		val = (p[offset+3] << 24) | (p[offset+2] << 16) | (p[offset+1] << 8) | p[offset];
		PUT_BE32(buffer, val); //Offset
		offset += 4;
	}

	memcpy(buffer, pkt->data+(1+(cSlice*8)), nSlice);
	size += nSlice;

	//printf("size = %6d, nSlice = %6d, cSlice = %4d, pkt->size=%6d, frameNumber=%d\n", size, nSlice, cSlice, pkt->size, stream->codec->frame_number );
	return size;
}

static int MakeVC1Stream( AVPacket *pkt, AVStream *stream, unsigned char *buffer, int outBufSize )
{
	int size=0;
	unsigned char *p = pkt->data;

	if( stream->codec->codec_id == CODEC_ID_VC1 )
	{
		if (p[0] != 0 || p[1] != 0 || p[2] != 1) // check start code as prefix (0x00, 0x00, 0x01)
		{
			*buffer++ = 0x00;
			*buffer++ = 0x00;
			*buffer++ = 0x01;
			*buffer++ = 0x0D;
			size = 4;
			memcpy(buffer, pkt->data, pkt->size);
			size += pkt->size;
		}
		else
		{
			memcpy(buffer, pkt->data, pkt->size);
			size = pkt->size; // no extra header size, there is start code in input stream.
		}
	}
	else
	{
		PUT_LE32(buffer, pkt->size | ((pkt->flags & AV_PKT_FLAG_KEY) ? 0x80000000 : 0));
		size += 4;
#ifdef RCV_V2	//	RCV_V2
		if (AV_NOPTS_VALUE == (long long)pkt->pts)
		{
			PUT_LE32(buffer, 0);
		}
		else
		{
			PUT_LE32(buffer, (int)((double)(pkt->pts/stream->time_base.den))); // milli_sec
		}
		size += 4;
#endif
		memcpy(buffer, pkt->data, pkt->size);
		size += pkt->size;
	}
	return size;
}


static int MakeDIVX3Stream( AVPacket *pkt, AVStream *stream, unsigned char *buffer, int outBufSize )
{
	int size = pkt->size;
	unsigned int tag = stream->codec->codec_tag;
	if( tag == MKTAG('D', 'I', 'V', '3') || tag == MKTAG('M', 'P', '4', '3') ||
		tag == MKTAG('M', 'P', 'G', '3') || tag == MKTAG('D', 'V', 'X', '3') || tag == MKTAG('A', 'P', '4', '1') )
	{
 		PUT_LE32(buffer,pkt->size);
 		PUT_LE32(buffer,0);
 		PUT_LE32(buffer,0);
 		size += 12;
	}
	memcpy( buffer, pkt->data, pkt->size );
	return size;
}

static int MakeVP8Stream( AVPacket *pkt, AVStream *stream, unsigned char *buffer, int outBufSize )
{
	PUT_LE32(buffer,pkt->size);		//	frame_chunk_len
	PUT_LE32(buffer,0);				//	time stamp
	PUT_LE32(buffer,0);
	memcpy( buffer, pkt->data, pkt->size );
	return ( pkt->size + 12 );
}

int isIdrFrame( unsigned char *buf, int size )
{
	int i;
	int isIdr=0;

	for( i=0 ; i<size-4; i++ )
	{
		//	Check Nal Start Code & Nal Type
		if( buf[i]==0 && buf[i+1]==0 && ((buf[i+2]&0x1F)==0x05) )
		{
			isIdr = 1;
			break;
		}
	}

	return isIdr;
}

CMediaReader::CMediaReader()
	: m_VideoStream( NULL )
	, m_pFormatCtx( NULL )
	, m_VideoStreamIndex( -1 )
	, m_pVideoCodec( NULL )
//	, m_pTheoraParser( NULL )
	, m_AudioStreamIndex( -1 )
	, m_AudioStream( NULL )
	, m_pAudioCodec( NULL )
{
	av_register_all();
}

CMediaReader::~CMediaReader()
{
	if( m_pFormatCtx )
	{
		avformat_close_input( &m_pFormatCtx );
	}
}


bool CMediaReader::OpenFile(const char *fileName)
{
	AVFormatContext *fmt_ctx = NULL;
	AVInputFormat *iformat = NULL;
	AVCodec *video_codec = NULL;
	AVCodec *audio_codec = NULL;
	AVStream *video_stream = NULL;
	AVStream *audio_stream = NULL;
	int video_stream_idx = -1;
	int audio_stream_idx = -1;
	int i;

	fmt_ctx = avformat_alloc_context();
	fmt_ctx->flags |= CODEC_FLAG_TRUNCATED;

	if ( avformat_open_input(&fmt_ctx, fileName, iformat, NULL) < 0 )
	{
		goto ErrorExit;
	}

	/* fill the streams in the format context */
	if ( avformat_find_stream_info(fmt_ctx, NULL) < 0)
	{
		goto ErrorExit;
	}

	av_dump_format(fmt_ctx, 0, fileName, 0);

	//	Video Codec Binding
	for( i=0; i<(int)fmt_ctx->nb_streams ; i++ )
	{
		AVStream *stream = fmt_ctx->streams[i];
		if( stream->codec->codec_type == AVMEDIA_TYPE_VIDEO )
		{
			if( !(video_codec=avcodec_find_decoder(stream->codec->codec_id)) )
			{
				printf( "Unsupported codec (id=%d) for input stream %d\n", stream->codec->codec_id, stream->index );
				goto ErrorExit;
			}

			if( avcodec_open2(stream->codec, video_codec, NULL)<0 )
			{
				printf( "Error while opening codec for input stream %d\n", stream->index );
				goto ErrorExit;
			}
			else
			{
				if( video_stream_idx == -1 )
				{
					video_stream_idx = i;
					video_stream = stream;;
				}
				else
				{
					avcodec_close( stream->codec );
				}
			}
		}
		else if( stream->codec->codec_type == AVMEDIA_TYPE_AUDIO )
		{
			if( !(audio_codec=avcodec_find_decoder(stream->codec->codec_id)) )
			{
				printf( "Unsupported codec (id=%d) for input stream %d\n", stream->codec->codec_id, stream->index );
				goto ErrorExit;
			}

			if( avcodec_open2(stream->codec, audio_codec, NULL)<0 )
			{
				printf( "Error while opening codec for input stream %d\n", stream->index );
				goto ErrorExit;
			}
			else
			{
				if( audio_stream_idx == -1 )
				{
					audio_stream_idx = i;
					audio_stream = stream;;
				}
				else
				{
					avcodec_close( stream->codec );
				}
			}
		}
	}

	m_pFormatCtx		= fmt_ctx;
	m_VideoStreamIndex	= video_stream_idx;
	m_VideoStream		= video_stream;
	m_pVideoCodec		= video_codec;
	m_AudioStreamIndex	= audio_stream_idx;
	m_AudioStream		= audio_stream;
	m_pAudioCodec		= audio_codec;

	// Intialize Theora Parser
	// if( m_VideoStream && (m_VideoStream->codec->codec_id == CODEC_ID_THEORA) )
	// {
	// 	theora_parser_init((void**)&m_pTheoraParser);
	// }

	return true;

ErrorExit:
	if( fmt_ctx )
	{
		avformat_close_input( &fmt_ctx );
	}
	return false;
}

void CMediaReader::CloseFile()
{
	if( m_pFormatCtx )
	{
		avformat_close_input( &m_pFormatCtx );
		m_pFormatCtx = NULL;
	}

	// Close Theora Parser
	// if( m_pTheoraParser )
	// {
	// 	theora_parser_end(m_pTheoraParser);
	// }
}

int32_t CMediaReader::ReadStream( int32_t type, uint8_t *buf, int32_t *size, int32_t *key, int64_t *timeStamp )
{
	int32_t ret;
	AVPacket pkt;
	AVStream *stream;

	if( type == MEDIA_TYPE_VIDEO )
		stream = m_VideoStream;
	else if( type == MEDIA_TYPE_AUDIO )
		stream = m_AudioStream;
	else
		return -1;

	enum AVCodecID codecId = stream->codec->codec_id;
	double timeStampRatio = (double)stream->time_base.num*1000./(double)stream->time_base.den;
	do{
		ret = av_read_frame( m_pFormatCtx, &pkt );
		if( ret < 0 )
			return -1;
		if( pkt.stream_index == stream->index )
		{
			//	check codec type
			if( codecId == CODEC_ID_H264 && stream->codec->extradata_size > 0 && stream->codec->extradata[0]==1 )
			{
				*size = PasreAVCStream( &pkt, m_NalLengthSize, buf, 0 );
				if (key)
					*key = (pkt.flags & AV_PKT_FLAG_KEY)?1:0;
				if( (long long)pkt.pts != AV_NOPTS_VALUE )
					*timeStamp = pkt.pts*timeStampRatio;
				else
					*timeStamp = -1;
				av_free_packet( &pkt );
				return 0;
			}
			else if(  (codecId == CODEC_ID_VC1) || (codecId == CODEC_ID_WMV1) || (codecId == CODEC_ID_WMV2) || (codecId == CODEC_ID_WMV3) )
			{
				*size = MakeVC1Stream( &pkt, stream, buf, 0 );
				if (key)
					*key = (pkt.flags & AV_PKT_FLAG_KEY)?1:0;
				if( (long long)pkt.pts != AV_NOPTS_VALUE )
					*timeStamp = pkt.pts*timeStampRatio;
				else
					*timeStamp = -1;
				av_free_packet( &pkt );
				return 0;
			}
			else if( codecId == CODEC_ID_RV30 || codecId == CODEC_ID_RV40 )
			{
				*size = MakeRvStream( &pkt, stream, buf, 0 );
				if (key)
					*key = (pkt.flags & AV_PKT_FLAG_KEY)?1:0;
				if( (long long)pkt.pts != AV_NOPTS_VALUE )
					*timeStamp = pkt.pts*timeStampRatio;
				else
					*timeStamp = -1;
				av_free_packet( &pkt );
				return 0;
			}
			else if( codecId == CODEC_ID_MSMPEG4V3 )
			{
				*size = MakeDIVX3Stream( &pkt, stream, buf, 0 );
				if (key)
					*key = (pkt.flags & AV_PKT_FLAG_KEY)?1:0;
				if( (long long)pkt.pts != AV_NOPTS_VALUE )
					*timeStamp = pkt.pts*timeStampRatio;
				else
					*timeStamp = -1;
				av_free_packet( &pkt );
				return 0;
			}
#if 0
			else if( codecId == CODEC_ID_THEORA )
			{
				if( m_pTheoraParser->read_frame( m_pTheoraParser->handle, pkt.data, pkt.size ) < 0 )
				{
					printf("Theora Read Frame Failed!!!\n");
					exit(1);
				}
				*size = theora_make_stream((void *)m_pTheoraParser->handle, buf, 3 /*Theora Picture Run*/);
				*key = (pkt.flags & AV_PKT_FLAG_KEY)?1:0;
				if( (long long)pkt.pts != AV_NOPTS_VALUE )
					*timeStamp = pkt.pts*timeStampRatio;
				else
					*timeStamp = -1;
				av_free_packet( &pkt );
				return 0;
			}
#endif
			else if( codecId == CODEC_ID_VP8 )
			{
				*size = MakeVP8Stream( &pkt, stream, buf, 0 );
				if (key)
					*key = (pkt.flags & AV_PKT_FLAG_KEY)?1:0;
				if( (long long)pkt.pts != AV_NOPTS_VALUE )
					*timeStamp = pkt.pts*timeStampRatio;
				else
					*timeStamp = -1;
				av_free_packet( &pkt );
				return 0;
			}
#if 0
			else if( codecId == AV_CODEC_ID_HEVC && stream->codec->extradata_size > 0 && (stream->codec->extradata[0]==1 || stream->codec->extradata[1]==1) )
			{
				*size = PasreAVCStream( &pkt, m_NalLengthSize, buf, 0 );
				*key = (pkt.flags & AV_PKT_FLAG_KEY)?1:0;
				if( (long long)pkt.pts != AV_NOPTS_VALUE )
					*timeStamp = pkt.pts*timeStampRatio;
				else
					*timeStamp = -1;
				av_free_packet( &pkt );
				return 0;
			}
#endif
			else
			{
				memcpy(buf, pkt.data, pkt.size );
				*size = pkt.size;
				if (key)
					*key = (pkt.flags & AV_PKT_FLAG_KEY)?1:0;
				if( (long long)pkt.pts != AV_NOPTS_VALUE )
					*timeStamp = pkt.pts*timeStampRatio;
				else
					*timeStamp = -1;
				av_free_packet( &pkt );
				return 0;
			}
		}
		av_free_packet( &pkt );
	}while(1);
	return -1;
}

#ifndef INT64_MIN
#define INT64_MIN		0x8000000000000000LL
#define INT64_MAX		0x7fffffffffffffffLL
#endif

int32_t CMediaReader::SeekStream( int64_t seekTime )
{
	int32_t seekFlag = AVSEEK_FLAG_FRAME;
	seekFlag &= ~AVSEEK_FLAG_BYTE;

	int32_t iRet = avformat_seek_file(m_pFormatCtx, -1, INT64_MIN, seekTime*1000, INT64_MAX, seekFlag);
	if( 0 > iRet ) {
		printf( "Fail, SeekStream().\n" );
	}

	return iRet;
}

int32_t CMediaReader::GetVideoSeqInfo( uint8_t *buf )
{
	return GetSequenceInformation( m_VideoStream, buf );
}

int32_t CMediaReader::GetAudioSeqInfo( uint8_t *buf )
{
	return GetSequenceInformation( m_AudioStream, buf );
}


//
//		Private Members
//
int32_t CMediaReader::GetSequenceInformation( AVStream *stream, unsigned char *buffer )
{
	uint8_t *pbHeader = buffer;
	enum AVCodecID codecId = stream->codec->codec_id;
	int32_t fourcc;
	int32_t frameRate = 0;
	int32_t nMetaData = stream->codec->extradata_size;
	uint8_t *pbMetaData = stream->codec->extradata;
	int32_t retSize = 0;
	uint32_t tag = stream->codec->codec_tag;

	if (stream->avg_frame_rate.den && stream->avg_frame_rate.num)
		frameRate = (int)((double)stream->avg_frame_rate.num/(double)stream->avg_frame_rate.den);
	if (!frameRate && stream->r_frame_rate.den && stream->r_frame_rate.num)
		frameRate = (int)((double)stream->r_frame_rate.num/(double)stream->r_frame_rate.den);

	if( (codecId == CODEC_ID_H264) && (stream->codec->extradata_size>0) )
	{
		if( stream->codec->extradata[0] == 0x1 )
		{
			NX_AVCC_TYPE avcHeader;
			NX_ParseSpsPpsFromAVCC( pbMetaData, nMetaData, &avcHeader );
			NX_MakeH264StreamAVCCtoANNEXB(&avcHeader, buffer, &retSize );
			m_NalLengthSize = avcHeader.nal_length_size;
			return retSize;
		}
	}
    else if ( (codecId == CODEC_ID_VC1) )
    {
		retSize = nMetaData;
        memcpy(pbHeader, pbMetaData, retSize);
		//if there is no seq startcode in pbMetatData. VPU will be failed at seq_init stage.
		return retSize;
	}
    else if ( (codecId == CODEC_ID_MSMPEG4V3) )
	{
		switch( tag )
		{
			case MKTAG('D','I','V','3'):
			case MKTAG('M','P','4','3'):
			case MKTAG('M','P','G','3'):
			case MKTAG('D','V','X','3'):
			case MKTAG('A','P','4','1'):
				if( !nMetaData )
				{
					PUT_LE32(pbHeader, MKTAG('C', 'N', 'M', 'V'));	//signature 'CNMV'
					PUT_LE16(pbHeader, 0x00);						//version
					PUT_LE16(pbHeader, 0x20);						//length of header in bytes
					PUT_LE32(pbHeader, MKTAG('D', 'I', 'V', '3'));	//codec FourCC
					PUT_LE16(pbHeader, stream->codec->width);		//width
					PUT_LE16(pbHeader, stream->codec->height);		//height
					PUT_LE32(pbHeader, stream->r_frame_rate.num);	//frame rate
					PUT_LE32(pbHeader, stream->r_frame_rate.den);	//time scale(?)
					PUT_LE32(pbHeader, stream->nb_index_entries);	//number of frames in file
					PUT_LE32(pbHeader, 0);							//unused
					retSize += 32;
				}
				else
				{
					PUT_BE32(pbHeader, nMetaData);
					retSize += 4;
					memcpy(pbHeader, pbMetaData, nMetaData);
					retSize += nMetaData;
				}
				return retSize;
			default:
				break;

		}
	}
	else if(  (codecId == CODEC_ID_WMV1) || (codecId == CODEC_ID_WMV2) || (codecId == CODEC_ID_WMV3) )
	{
#ifdef	RCV_V2	//	RCV_V2
        PUT_LE32(pbHeader, ((0xC5 << 24)|0));
        retSize += 4; //version
        PUT_LE32(pbHeader, nMetaData);
        retSize += 4;

        memcpy(pbHeader, pbMetaData, nMetaData);
		pbHeader += nMetaData;
        retSize += nMetaData;

        PUT_LE32(pbHeader, stream->codec->height);
        retSize += 4;
        PUT_LE32(pbHeader, stream->codec->width);
        retSize += 4;
        PUT_LE32(pbHeader, 12);
        retSize += 4;
        PUT_LE32(pbHeader, 2 << 29 | 1 << 28 | 0x80 << 24 | 1 << 0);
        retSize += 4; // STRUCT_B_FRIST (LEVEL:3|CBR:1:RESERVE:4:HRD_BUFFER|24)
        PUT_LE32(pbHeader, stream->codec->bit_rate);
        retSize += 4; // hrd_rate
		PUT_LE32(pbHeader, frameRate);
        retSize += 4; // frameRate
#else	//RCV_V1
        PUT_LE32(pbHeader, (0x85 << 24) | 0x00);
        retSize += 4; //frames count will be here
        PUT_LE32(pbHeader, nMetaData);
        retSize += 4;
        memcpy(pbHeader, pbMetaData, nMetaData);
		pbHeader += nMetaData;
        retSize += nMetaData;
        PUT_LE32(pbHeader, stream->codec->height);
        retSize += 4;
        PUT_LE32(pbHeader, stream->codec->width);
        retSize += 4;
#endif
		return retSize;
	}
	else if( (stream->codec->codec_id == CODEC_ID_RV40 || stream->codec->codec_id == CODEC_ID_RV30 )
		&& (stream->codec->extradata_size>0) )
	{
		if( CODEC_ID_RV40 == stream->codec->codec_id )
		{
			fourcc = MKTAG('R','V','4','0');
		}
		else
		{
			fourcc = MKTAG('R','V','3','0');
		}
        retSize = 26 + nMetaData;
        PUT_BE32(pbHeader, retSize); //Length
        PUT_LE32(pbHeader, MKTAG('V', 'I', 'D', 'O')); //MOFTag
		PUT_LE32(pbHeader, fourcc); //SubMOFTagl
        PUT_BE16(pbHeader, stream->codec->width);
        PUT_BE16(pbHeader, stream->codec->height);
        PUT_BE16(pbHeader, 0x0c); //BitCount;
        PUT_BE16(pbHeader, 0x00); //PadWidth;
        PUT_BE16(pbHeader, 0x00); //PadHeight;
        PUT_LE32(pbHeader, frameRate);
        memcpy(pbHeader, pbMetaData, nMetaData);
		return retSize;
	}
	else if( stream->codec->codec_id == CODEC_ID_VP8 )
	{
		PUT_LE32(pbHeader, MKTAG('D', 'K', 'I', 'F'));	//signature 'DKIF'
		PUT_LE16(pbHeader, 0x00);						//version
		PUT_LE16(pbHeader, 0x20);						//length of header in bytes
		PUT_LE32(pbHeader, MKTAG('V', 'P', '8', '0'));	//codec FourCC
		PUT_LE16(pbHeader, stream->codec->width);		//width
		PUT_LE16(pbHeader, stream->codec->height);		//height
		PUT_LE32(pbHeader, stream->r_frame_rate.num);	//frame rate
		PUT_LE32(pbHeader, stream->r_frame_rate.den);	//time scale(?)
		PUT_LE32(pbHeader, stream->nb_index_entries);	//number of frames in file
		PUT_LE32(pbHeader, 0);							//unused
		retSize += 32;
		return retSize;
	}
#if 0
	else if( stream->codec->codec_id == CODEC_ID_THEORA )
	{
		ThoScaleInfo thoScaleInfo;
		m_pTheoraParser->open(m_pTheoraParser->handle, stream->codec->extradata, stream->codec->extradata_size, (int *)&thoScaleInfo);
		retSize = theora_make_stream((void *)m_pTheoraParser->handle, buffer, 1);
		return retSize;
	}
#endif
	if( (codecId == AV_CODEC_ID_HEVC) && (stream->codec->extradata_size>0) )
	{
		if( stream->codec->extradata[0] == 0x1 )
		{
			NX_HVC1_TYPE hevcHeader;
			NX_ParseHEVCCfgFromHVC1( pbMetaData, nMetaData, &hevcHeader );
			NX_MakeHEVCStreamHVC1ANNEXB( &hevcHeader, buffer, &retSize );
			m_NalLengthSize = hevcHeader.nal_length_size;
			return retSize;
		}
		else if ( stream->codec->extradata[1] == 0x1 )
		{
			m_NalLengthSize = 1 + (pbMetaData[14 + 7] & 3);
			return 0;
		}
	}

	memcpy( buffer, stream->codec->extradata, stream->codec->extradata_size );

	return stream->codec->extradata_size;
}
