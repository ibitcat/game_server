// 网络连接相关

#include <errno.h>
#include "app.h"

extern appServer app;

static void readFromSession(aeEventLoop *el, int fd, void *privdata, int mask) {
	netSession * session = (netSession *)privdata;
	if (session->usedlen==session->buflen){
		// 没有空闲buf了，增加2倍长度
		session->buflen *= 2;
		session->querybuf = realloc(session->querybuf, session->buflen);
	}
	int readlen = session->buflen - session->usedlen;
	int nread = read(fd, session->querybuf+session->usedlen, readlen);
	printf("read from session = %s\n", session->querybuf);
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
		return;
	}

	// 处理buf
	int headLen = sizeof(struct msgBuff);
	int pos = 0;
	int diff = session->usedlen - pos;
	lua_State *L = app.L;
	while(diff>=headLen){
		struct msgBuff * buf = (struct msgBuff *)(session->querybuf+pos);
		// 检查len是否合法
		// TODO
		assert(buf->len>=headLen);
		assert(pos+buf->len<=session->usedlen);
		pos += buf->len;
		diff = session->usedlen - pos;

		// pcall
		lua_getglobal(L, "handleMsg");
		lua_pushinteger(L,fd);
		lua_pushinteger(L,buf->cmd);
		lua_pushinteger(L,buf->fromType);
		lua_pushinteger(L,buf->fromId);
		lua_pushinteger(L,buf->toType);
		lua_pushinteger(L,buf->toId);
		lua_pcall(L,6,0,0);
		// lua_handleMsg(fd,cmd,fromTy,fromId,toTy,toId,pkt);
	}
	session->usedlen = diff;
	memmove(session->querybuf, session->querybuf+pos, diff);
}

static netSession *findSessionByFd(int fd){
	netSession * head = app.sessionHead;
	while(head){
		if (head->fd == fd){
			return head;
		}
		head = head->next;
	}
	return NULL;
}

void writeHandler(aeEventLoop *el, int fd, void *privdata, int mask) {
	netSession * session = findSessionByFd(fd);
	assert(session);

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


static void writeToSession(int fd, char * buf, int len){
	netSession * session = findSessionByFd(fd);
	if(session==NULL){
		serverLog(0,"session not found,fd = %d", fd);
		return;
	}

	int headLen = sizeof(struct msgBuff);
	int totalLen = headLen +len;
	struct msgBuff * msg = (struct msgBuff *)malloc(totalLen);
	msg->len = totalLen;
	msg->fromType = 'a';
	msg->fromId = 1;
	msg->toType = 'b';
	msg->toId = 1;
	msg->cmd = 1001;
	if (len>0){
		memcpy(msg+headLen, buf, len);
	}

	// write
	// 如果buf没有剩余数据，直接发
	if (session->remainlen==0){
		int sendedLen = 0;
		while((totalLen-sendedLen)>0){
			int nwritten = write(fd,msg+sendedLen,totalLen-sendedLen);
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
			aeCreateFileEvent(app.pEl, fd, AE_WRITABLE, writeHandler, session);
		}
	}
}

static void acceptTcpHandler(aeEventLoop *el, int fd, void *privdata, int mask) {
	int cport;
	char cip[64];

	appServer *pApp = (appServer *)privdata;
	int cfd = anetTcpAccept(pApp->neterr, fd, cip, sizeof(cip), &cport);
	printf("%d\n", cfd);
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
	// TODO
}

/* =========================== net api =========================== */
netSession * createSession(int fd){
	netSession * session = malloc(sizeof(*session));
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

	session->next = NULL;
	session->id = ++app.nextClientId;
	session->buflen = 1024;	// 初始buff=1kb
	session->querybuf = (unsigned char *)malloc(1024);
	session->usedlen = 0;

	// 插入到list中
	return session;
}

int freeSession(netSession * session){
	return 0;
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
		return -1;
	}

	// TODO: 保存这个session
	return fd;
}