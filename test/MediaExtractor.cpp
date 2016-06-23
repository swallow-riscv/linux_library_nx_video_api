#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>		// getopt & optarg
#include <sys/time.h>	// gettimeofday

#include "MediaExtractor.h"

#include "CodecInfo.h"
#include "Util.h"

//==============================================================================
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

	for (i=0 ; i<avcc->num_sps ; i++)
	{
		pBuf[pos++] = 0x00;
		pBuf[pos++] = 0x00;
		pBuf[pos++] = 0x00;
		pBuf[pos++] = 0x01;
		memcpy( pBuf + pos, avcc->sps_data[i], avcc->sps_length[i] );
		pos += avcc->sps_length[i];
	}
	
	for (i=0 ; i<avcc->num_pps ; i++)
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

static int NX_ParseHEVCCfgFromHVC1(unsigned char *extraData, int extraDataSize, NX_HVC1_TYPE *hevcInfo)
{
	uint8_t *ptr = (uint8_t *)extraData;

	// verify minimum size and configurationVersion == 1.
	if (extraDataSize < 7 || ptr[0] != 1)
		return -1;

	hevcInfo->profile = (ptr[1] & 31);
	hevcInfo->level = ptr[12];
	hevcInfo->nal_length_size = 1 + (ptr[14 + 7] & 3);

	ptr += 22;
	extraDataSize -= 22;

	int numofArrays = (char)ptr[0];

	ptr += 1;
	extraDataSize -= 1;
	int j = 0, i = 0, cnt = 0;

	for (i = 0; i < numofArrays; i++)
	{
		ptr += 1;
		extraDataSize -= 1;

		// Num of nals
		int numofNals = ptr[0] << 8 | ptr[1];

		ptr += 2;
		extraDataSize -= 2;

		for (j = 0;j < numofNals;j++)
		{
			if (extraDataSize < 2)
				return -1;

			int length = ptr[0] << 8 | ptr[1];

			ptr += 2;
			extraDataSize -= 2;

			if (extraDataSize < length)
				return -1;

			hevcInfo->cfg_length[cnt] = length;
			memcpy(hevcInfo->cfg_data[cnt], ptr, length);

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

CMediaReader::CMediaReader()
	: m_VideoStream( NULL )
	, m_pFormatCtx( NULL )
	, m_VideoStreamIndex( -1 )
	, m_pVideoCodec( NULL )
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
			else
			{
				memcpy(buf, pkt.data, pkt.size );
				*size = pkt.size;
				if (key)
					*key = (pkt.flags & AV_PKT_FLAG_KEY) ? 1 : 0;
				if ((long long)pkt.pts != AV_NOPTS_VALUE)
					*timeStamp = pkt.pts * timeStampRatio;
				else
					*timeStamp = -1;
				av_free_packet(&pkt);
				return 0;
			}
		}
		av_free_packet( &pkt );
	} while (1);

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
	enum AVCodecID codecId = stream->codec->codec_id;
	int32_t nMetaData = stream->codec->extradata_size;
	uint8_t *pbMetaData = stream->codec->extradata;
	int32_t retSize = 0;

	if ((codecId == CODEC_ID_H264) && (stream->codec->extradata_size > 0))
	{
		if (stream->codec->extradata[0] == 0x1)
		{
			NX_AVCC_TYPE avcHeader;
			NX_ParseSpsPpsFromAVCC(pbMetaData, nMetaData, &avcHeader);
			NX_MakeH264StreamAVCCtoANNEXB(&avcHeader, buffer, &retSize);
			m_NalLengthSize = avcHeader.nal_length_size;
			return retSize;
		}
	}

	if ((codecId == AV_CODEC_ID_HEVC) && (stream->codec->extradata_size > 0))
	{
		if (stream->codec->extradata[0] == 0x1)
		{
			NX_HVC1_TYPE hevcHeader;
			NX_ParseHEVCCfgFromHVC1(pbMetaData, nMetaData, &hevcHeader);
			NX_MakeHEVCStreamHVC1ANNEXB(&hevcHeader, buffer, &retSize);
			m_NalLengthSize = hevcHeader.nal_length_size;
			return retSize;
		}
		else if (stream->codec->extradata[1] == 0x1)
		{
			m_NalLengthSize = 1 + (pbMetaData[14 + 7] & 3);
			return 0;
		}
	}

	memcpy(buffer, stream->codec->extradata, stream->codec->extradata_size);

	return stream->codec->extradata_size;
}
