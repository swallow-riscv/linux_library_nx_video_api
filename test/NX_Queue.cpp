//------------------------------------------------------------------------------
//
//	Copyright (C) 2010 Nexell co., Ltd All Rights Reserved
//
//	Module     : Queue Module
//	File       : 
//	Description:
//	Author     : RayPark
//	History    :
//------------------------------------------------------------------------------
#include <assert.h>
#include <string.h>		//	string
#include <pthread.h>

#include "NX_Queue.h"

#define DbgMsg(fmt,...)		printf(fmt)

//
//	Description : Initialize queue structure
//	Return      : 0 = no error, -1 = error
//
int NX_InitQueue( NX_QUEUE *pQueue, unsigned int maxNumElement )
{
	//	Initialize Queue
	memset( pQueue, 0, sizeof(NX_QUEUE) );
	if( maxNumElement > NX_MAX_QUEUE_ELEMENT ){
		return -1;
	}
	if( 0 != pthread_mutex_init( &pQueue->hMutex, NULL ) ){
		return -1;
	}
	pQueue->maxElement = maxNumElement;
	pQueue->bEnabled = 1;
	return 0;
}

int NX_PushQueue( NX_QUEUE *pQueue, void *pElement )
{
	assert( NULL != pQueue );
	pthread_mutex_lock( &pQueue->hMutex );
	//	Check Buffer Full
	if( pQueue->curElements >= pQueue->maxElement || !pQueue->bEnabled ){
		pthread_mutex_unlock( &pQueue->hMutex );
		return -1;
	}else{
		pQueue->pElements[pQueue->tail] = pElement;
		pQueue->tail = (pQueue->tail+1)%pQueue->maxElement;
		pQueue->curElements ++;
	}
	pthread_mutex_unlock( &pQueue->hMutex );
	return 0;
}

int NX_PopQueue( NX_QUEUE *pQueue, void **pElement )
{
	assert( NULL != pQueue );
	pthread_mutex_lock( &pQueue->hMutex );
	//	Check Buffer Full
	if( pQueue->curElements == 0 || !pQueue->bEnabled ){
		pthread_mutex_unlock( &pQueue->hMutex );
		return -1;
	}else{
		*pElement = pQueue->pElements[pQueue->head];
		pQueue->head = (pQueue->head + 1)%pQueue->maxElement;
		pQueue->curElements --;
	}
	pthread_mutex_unlock( &pQueue->hMutex );
	return 0;
}

int NX_GetNextQueuInfo( NX_QUEUE *pQueue, void **pElement )
{
	assert( NULL != pQueue );
	pthread_mutex_lock( &pQueue->hMutex );
	//	Check Buffer Full
	if( pQueue->curElements == 0 || !pQueue->bEnabled ){
		pthread_mutex_unlock( &pQueue->hMutex );
		return -1;
	}else{
		*pElement = pQueue->pElements[pQueue->head];
	}
	pthread_mutex_unlock( &pQueue->hMutex );
	return 0;
}

unsigned int NX_GetQueueCnt( NX_QUEUE *pQueue )
{
	assert( NULL != pQueue );
	return pQueue->curElements;
}

void NX_DeinitQueue( NX_QUEUE *pQueue )
{
	assert( NULL != pQueue );
	pthread_mutex_lock( &pQueue->hMutex );
	pQueue->bEnabled = 0;
	pthread_mutex_unlock( &pQueue->hMutex );
	pthread_mutex_destroy( &pQueue->hMutex );
	memset( pQueue, 0, sizeof(NX_QUEUE) );
}
