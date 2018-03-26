// 消息缓存

#ifndef _MSGBUF_H_
#define _MSGBUF_H_

#include <string.h>

typedef struct msgBuf{
	unsigned int cap;		// buf容量
	unsigned int used;		// buf已使用的大小
	unsigned char *buf;
} msgBuf;

static inline void cleanBuf(msgBuf *mbuf){
	memset(mbuf->buf, 0, mbuf->cap);
	mbuf->used = 0;
}

msgBuf * newBuf(int cap);
void appendBuf(msgBuf *msgbuf, unsigned char * inbuf, int inlen);
void trimBuf(msgBuf *msgbuf, int readed);

#endif