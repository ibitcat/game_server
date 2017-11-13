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

static void writeToSession(){

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

	session->id = ++app.nextClientId;
	session->buflen = 1024;	// 初始buff=1kb
	session->querybuf = (unsigned char *)malloc(1024);
	session->usedlen = 0;
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