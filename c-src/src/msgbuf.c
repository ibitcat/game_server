#include <stdlib.h>
#include <assert.h>

#include "msgbuf.h"
#include "log.h"

// 优化TODO，使用可变长数组，调用一次malloc即可
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
	mbuf->used = 0;

	return mbuf;
}

void expandBuf(msgBuf *mbuf, int newcap){
	assert(newcap>0);
	if (mbuf->used > newcap){
		serverLog(0,"newcap is wrong value");
		return;
	}

	mbuf->buf = (unsigned char *)realloc(mbuf->buf, newcap);
	mbuf->cap = newcap;
}

unsigned char * getFreeBuf(msgBuf *mbuf, int *len){
	int free = mbuf->cap - mbuf->used;
	if (free<=0){
		int newcap = mbuf->cap * 2; // 扩大两倍
		expandBuf(mbuf, newcap);
	}

	*len = mbuf->cap - mbuf->used;
	return mbuf->buf + mbuf->used;
}

// 往buf追加inlen个字节
void appendBuf(msgBuf *mbuf, unsigned char * inbuf, int inlen){
	if (inlen<=0) return;

	int free = mbuf->cap - mbuf->used;
	if (inlen > free){
		int n = inlen/1024 + 1;
		expandBuf(mbuf, mbuf->cap + 1024*n);
		free = mbuf->cap - mbuf->used;
	}

	memcpy(mbuf->buf + mbuf->used, inbuf, inlen);
	mbuf->used += inlen;
}

// 整理buf，即把使用的部分往前移动len个字节
void trimBuf(msgBuf *mbuf, int len){
	int left = mbuf->used - len;
	if (left>0){
		memmove(mbuf->buf, mbuf->buf + len, left);
		mbuf->used -= len;
	}else{
		mbuf->used = 0;
	}
}