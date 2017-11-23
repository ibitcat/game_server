#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "msgbuf.h"
#include "log.h"

msgBuf * newBuf(int cap){
	assert(cap>0);
	msgBuf * mbuf = (msgBuf *)malloc(sizeof(msgBuf));
	if (mbuf==NULL){
		serverLog(0,"newBuf fail");
		return NULL;
	}
	mbuf->buf = (unsigned char *)malloc(cap);
	memset(mbuf->buf, 0, cap);
	mbuf->cap = cap;
	mbuf->size = 0;

	return mbuf;
}

void expandBuf(msgBuf *mbuf, int newcap){
	assert(newcap>0);
	if (mbuf->size > newcap){
		serverLog(0,"newcap is wrong value");
		return;
	}

	mbuf->buf = (unsigned char *)realloc(mbuf->buf, newcap);
	mbuf->cap = newcap;
}

void writeToBuf(msgBuf *mbuf, unsigned char * inbuf, int inlen){
	if (inlen<=0) return;

	int free = mbuf->cap - mbuf->size;
	if (inlen > free){
		int n = inlen/1024 + 1;
		expandBuf(mbuf, mbuf->cap + 1024*n);
		free = mbuf->cap - mbuf->size;
	}

	memcpy(mbuf->buf + mbuf->size, inbuf, inlen);
	mbuf->size += inlen;
}

unsigned char * getFreeBuf(msgBuf *mbuf, int *len){
	int free = mbuf->cap - mbuf->size;
	if (free<=0){
		int newcap = mbuf->cap * 2; // 扩大两倍
		expandBuf(mbuf, newcap);
	}

	free = mbuf->cap - mbuf->size;
	*len = free;
	return mbuf->buf + mbuf->size;
}

void readBuf(msgBuf *mbuf, int readed){
	int left = mbuf->size - readed;
	if (left>0){
		memmove(mbuf->buf, mbuf->buf + readed, left);
	}
	mbuf->size -= readed;
}