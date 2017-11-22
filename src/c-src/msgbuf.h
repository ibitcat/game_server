// 消息缓存

#ifndef _MSGBUF_H_
#define _MSGBUF_H_

typedef struct msgBuf{
	unsigned int cap;		// buf容量
	unsigned int size;		// buf已使用的大小
	unsigned char buf[];
} msgBuf;

static inline void cleanBuf(msgBuf *mbuf){
	memset(mbuf->buf, 0, mbuf->cap);
	mbuf->size = 0;
}

msgBuf * newBuf(int cap);
void readFromBuf(msgBuf *msgbuf);
void writeToBuf(msgBuf *msgbuf, unsigned char * inbuf, int inlen);

#endif