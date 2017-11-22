// 网络连接相关

#include <string.h>
#include <errno.h>
#include "app.h"

extern appServer app;

static void checkInputBuf(netSession * session){
	msgBuf * mbuf = session->input;
	if (mbuf->size == mbuf->cap){
		int newcap = mbuf->cap + 1024;
		session->input = expandBuf(mbuf, newcap);
	}
}

static void checkOutputBuf(netSession * session){
	msgBuf * mbuf = session->output;
	if (mbuf->size == mbuf->cap){
		int newcap = mbuf->cap + 1024;
		session->output = expandBuf(mbuf, newcap);
	}
}

static void readFromSession(aeEventLoop *el, int fd, void *privdata, int mask) {
	netSession * session = (netSession *)privdata;
	checkInputBuf(session);

	int readlen = 0;
	msgBuf * inupt = session->input;
	unsigned char * buf = getAvailableBuf(inupt, &readlen);
	if (buf==NULL){
		serverLog(LL_VERBOSE, "input buff not enought");
		return;
	}
	int nread = read(fd, buf, readlen);
	if (nread == -1) {
		if (errno == EAGAIN) {
			return;
		} else {
			serverLog(LL_VERBOSE, "Reading from session: %s",strerror(errno));
			//freeSession(c);
			return;
		}
	} else if (nread == 0) {
		serverLog(LL_VERBOSE, "Client closed connection");
		//freeSession(c);
		aeDeleteFileEvent(app.pEl, fd, AE_READABLE);
		return;
	}

	// 处理buf
	int headLen = sizeof(msgPack);
	unsigned char * head = inupt->buf;
	int left = inupt->size;
	lua_State *L = app.L;
	while(left >= headLen){
		msgPack * pkt = (msgPack *)head;
		assert(pkt->len >= headLen); // 检查len是否合法
		if (pkt->len > input->size){
			/* 不够一个包 */
			break;
		}else{
			head += pkt->len;
			left -= pkt->len;
		}

		// pcall
		// lua_handleMsg(fd,cmd,fromTy,fromId,toTy,toId,pkt);
		printf("%d\n", pkt->cmd);
		lua_getglobal(L, "cl_handleMsg");
		lua_pushinteger(L,fd);
		lua_pushinteger(L,pkt->cmd);
		lua_pushinteger(L,pkt->fromType);
		lua_pushinteger(L,pkt->fromId);
		lua_pushinteger(L,pkt->toType);
		lua_pushinteger(L,pkt->toId);
		lua_pcall(L,6,0,0);
	}
	readBuf(inupt, inupt->used - left);
}

void writeToSession(aeEventLoop *el, int fd, void *privdata, int mask) {
	netSession * session = (netSession *)privdata;

	int total = session->remainlen;
	int sendedLen = 0;
	while((total-sendedLen)>0){
		int nwritten = write(fd, session->outbuf+sendedLen, total-sendedLen);
		if (nwritten>0){
			assert(nwritten<=total);
			sendedLen += nwritten;
		}else if (nwritten<0){
			// 写入失败
			if(errno==EAGAIN){
				// 注册事件
				break;
			}else if(errno==EINTR){
				// 继续循环
			}else{
				// socket有问题了
			}
		}
	}

	// 没有发完
	int difflen = total-sendedLen;
	difflen = difflen>0?difflen:0;
	session->remainlen = difflen;
	if (difflen==0){
		// del file event
		aeDeleteFileEvent(app.pEl, fd, AE_WRITABLE);
	}
}

static void acceptTcpHandler(aeEventLoop *el, int fd, void *privdata, int mask) {
	int cport;
	char cip[64];

	int cfd = anetTcpAccept(app.neterr, fd, cip, sizeof(cip), &cport);
	if (cfd==ANET_ERR){
		if (errno != EWOULDBLOCK){
			serverLog(LL_WARNING, "Accepting session connection: %s", pApp->neterr);
		}
		return;
	}

	printf("Accepted %s:%d \n", cip, cport);
	char *str = "hello, accepted……";
	write(cfd,str,strlen(str));
	//serverLog(LL_VERBOSE,"Accepted %s:%d", cip, cport);
	//acceptCommonHandler(cfd,0,cip);

	// 处理接收到fd
	netSession * session = createSession(cfd);
	if (session==NULL){
		serverLog(LL_WARNING, "create session fail: %d", cfd);
		return;
	}

	// 告诉lua层
	// lua_pcall(L,1,0,0);
}

static netSession * popSession(){
	if (app.freelist.head==NULL){
		// 扩充
		int newSize = app.maxSize + 8;
		for (int i = app.maxSize + 1; i <= newSize; ++i){
			NEW_SESSION(i);
		}
		app.maxSize = newSize;
	}

	netSession * session = app.freelist.head;
	if (session){
		app.freelist.head = session->next;
		--app.freelist.count;

		if (app.freelist.tail==session){
			app.freelist.tail = app.freelist.head = NULL;
			app.freelist.count = 0;
		}
	}
	return session;
}

/* =========================== net api =========================== */
netSession * createSession(int fd, const char *ip, short port){
	netSession * session = popSession();
	if (session==NULL){
		serverLog(0, "create session fail");
		return NULL;
	}

	if (fd != -1) {
		anetNonBlock(NULL,fd);	// 设置为非阻塞
		anetEnableTcpNoDelay(NULL,fd);

		if (app.tcpkeepalive){
			anetKeepAlive(NULL,fd,app.tcpkeepalive);
		}

		if (aeCreateFileEvent(app.pEl, fd, AE_READABLE, readFromSession, session) == AE_ERR){
			close(fd);
			free(session);
			return NULL;
		}
	}

	session->ip = ip;
	session->port = port;
	session->fd = fd;
	session->next = NULL;

	// 插入到list中
	netSession * head = app.sessionHead;
	if (head==NULL) {
		app.sessionHead = session;
	}else{
		head->next = session;
	}
	return session;
}

void freeSession(netSession * session){
	// 清空
	session->port = 0;
	session->ip = NULL;
	session->fd = -1;
	session->next = NULL;
	cleanBuf(session->input);
	cleanBuf(session->output);

	pushFree(session);
}

void pushFree(netSession * session){
	assert(session);
	if (app.freelist.head==NULL){
		app.freelist.head = session;
		app.freelist.tail = session;
		++app.freelist.count;
	}else{
		session->next = NULL;
		app.freelist.tail->next = session;
		app.freelist.tail = session;
		++app.freelist.count;
	}
}

int netListen(int port, char * addr){
	int fd = anetTcpServer(app.neterr, port, addr, 511); // 监听的socket
	if (fd == ANET_ERR) {
		serverLog(LL_WARNING, "Creating Server TCP listening socket %s:%d: %s", addr ? addr : "*", port, app.neterr);
		return -1;
	}else{
		anetNonBlock(NULL, fd);

		// listen
		if (aeCreateFileEvent(app.pEl, fd, AE_READABLE, acceptTcpHandler, NULL) == -1){
			serverLog(LL_WARNING, "Create Listen File Event Fail...");
			return -1;
		}
		return fd;
	}
}

int netConnect(char * addr, int port){
	int fd = anetTcpConnect(app.neterr, addr, port);
	if (fd==ANET_ERR){
		serverLog(0, "net connect fail");
		return -1;
	}

	// event
	netSession * session = createSession(fd);
	if (session==NULL){
		serverLog(LL_WARNING, "session create fail...");
		return -1;
	}
	return fd;
}

void netWrite(int fd, char * buf, int len){
	printf("fd = %d, msg = %s, len  =%d\n",fd,buf,len );
	netSession * session = findSessionByFd(fd);
	if(session==NULL){
		serverLog(0,"session not found,fd = %d", fd);
		return;
	}

	int headLen = sizeof(struct msgBuff);
	int totalLen = headLen +len;
	printf("totalLen = %d\n", totalLen);
	struct msgBuff * msg = (struct msgBuff *)malloc(totalLen);
	msg->len = totalLen;
	msg->fromType = 'a';
	msg->fromId = 1;
	msg->toType = 'b';
	msg->toId = 1;
	msg->cmd = 1001;
	if (len>0){
		assert(buf);
		memcpy(msg+headLen, buf, len);
	}

	// write
	// 如果buf没有剩余数据，直接发
	if (session->remainlen==0){
		int sendedLen = 0;
		while((totalLen-sendedLen)>0){
			int nwritten = write(fd,msg+sendedLen,totalLen-sendedLen);
			printf("write len = %d\n", nwritten);
			if (nwritten>0){
				assert(nwritten<=totalLen);
				sendedLen += nwritten;
			}else if (nwritten<0){
				// 写入失败
				if(errno==EAGAIN){
					// 注册事件
					break;
				}else if(errno==EINTR){
					// 继续循环
				}else{
					// socket有问题了
				}
			}
		}

		// 没有发完的放入到out buf
		int diff = totalLen-sendedLen;
		if (diff>0){
			session->remainlen += diff;
			memcpy(session->outbuf, msg+sendedLen, diff);
			//aeCreateFileEvent();
		}
	}else{
		int diff = session->outlen - session->remainlen;
		if (diff<totalLen){
			// outbuf扩大
			int newLen = session->remainlen + totalLen;
			session->outbuf = realloc(session->outbuf, newLen);
			session->outlen = newLen;
		}
		session->remainlen += totalLen;
		memmove(session->outbuf + session->remainlen, msg, totalLen);

		// 发送
		int total = session->remainlen;
		int sendedLen = 0;
		while((total-sendedLen)>0){
			int nwritten = write(fd, session->outbuf+sendedLen, total-sendedLen);
			if (nwritten>0){
				assert(nwritten<=total);
				sendedLen += nwritten;
			}else if (nwritten<0){
				// 写入失败
				if(errno==EAGAIN){
					// 注册事件
					break;
				}else if(errno==EINTR){
					// 继续循环
				}else{
					// socket有问题了
				}
			}
		}

		// 没有发完
		int difflen = total-sendedLen;
		if (difflen>0){
			session->remainlen = difflen;
			memmove(session->outbuf, session->outbuf+sendedLen, difflen);
			aeCreateFileEvent(app.pEl, fd, AE_WRITABLE, writeToSession, session);
		}
	}
}