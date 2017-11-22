#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "msgbuf.h"
#include "log.h"

msgBuf * expandBuf(msgBuf *mbuf, int newcap){
	assert(newcap>0);
	if (mbuf->size > newcap){
		serverLog(0,"newcap is wrong value");
		return mbuf;
	}

	int total = sizeof(msgBuf)+newcap;
	msgBuf * newbuf = (msgBuf *)realloc(mbuf, total);
	newbuf->cap = newcap;
	return newbuf;
}

msgBuf * newBuf(int cap){
	assert(cap>0);
	int total = sizeof(msgBuf)+cap;
	msgBuf * mbuf = (msgBuf *)malloc(total);
	if (mbuf==NULL){
		serverLog(0,"newBuf fail");
		return NULL;
	}
	memset(mbuf, 0, total);
	mbuf->cap = cap;

	return mbuf;
}

void writeToBuf(msgBuf *mbuf, unsigned char * inbuf, int inlen){
	if (inlen<=0){
		return;
	}

	int total = mbuf->size + inlen;
	assert(total<=mbuf->cap);

	memcpy(mbuf->buf + mbuf->size, inbuf, inlen);
	mbuf->size += inlen;
}

unsigned char * getAvailableBuf(msgBuf *mbuf, int *len){
	int free = mbuf->cap - mbuf->size;
	if (free>0){
		unsigned char * buf = mbuf->buf + mbuf->size;
		*len = free;
		return buf;
	}else{
		*len = -1;
	}
	return NULL;
}

void readBuf(msgBuf *mbuf, int readed){
	int left = mbuf->size - readed;
	if (left>0){
		memmove(mbuf->buf, mbuf->buf + readed, left);
	}
	mbuf->size -= readed;
}