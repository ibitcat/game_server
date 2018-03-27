// 网络连接相关

#include <string.h>
#include <errno.h>
#include <unistd.h> // for close

#include "app.h"
#include "msgbuf.h"

extern appServer app;

/* =========================== c call lua =========================== */
void cl_handlePkt(int fd, msgPack * pkt){
	// lua_handleMsg(fd,cmd,fromTy,fromId,toTy,toId,pkt);
	lua_State *L = app.L;
	int top = lua_gettop(L);
	lua_pushvalue(L, app.luaErrPos);
	lua_getglobal(L, "cl_onHandlePkt");
	lua_pushinteger(L, fd);
	lua_pushinteger(L, pkt->cmd);
	lua_pushinteger(L, pkt->fromType);
	lua_pushinteger(L, pkt->fromId);
	lua_pushinteger(L, pkt->toType);
	lua_pushinteger(L, pkt->toId);
	lua_pushlstring(L, pkt->buf, pkt->len - sizeof(msgPack));
	lua_pcall(L,7,0,1);
	lua_settop(L, top);
}

void cl_netAccpeted(int fd){
	lua_State *L = app.L;
	int top = lua_gettop(L);
	lua_pushvalue(L, app.luaErrPos);
	lua_getglobal(L, "cl_onNetAccpeted");
	lua_pushinteger(L, fd);
	lua_pcall(L,1,0,1);
	lua_settop(L, top);
}

void cl_netConnected(int fd){
	lua_State *L = app.L;
	int top = lua_gettop(L);
	lua_pushvalue(L, app.luaErrPos);
	lua_getglobal(L, "cl_onNetConnected");
	lua_pushinteger(L, fd);
	lua_pcall(L,1,0,1);
	lua_settop(L, top);
}

void cl_netClosed(int fd){
	lua_State *L = app.L;
	int top = lua_gettop(L);
	lua_pushvalue(L, app.luaErrPos);
	lua_getglobal(L, "cl_onNetClosed");
	lua_pushinteger(L, fd);
	lua_pcall(L,1,0,1);
	lua_settop(L, top);
}

/* =========================== static func =========================== */
static int readMsgbuff(netSession * session){
	int readlen = 0;
	msgBuf * input = session->input;
	unsigned char * buf = getFreeBuf(input, &readlen);
	assert(buf);

	int nread = read(session->fd, buf, readlen);
	if (nread < 0) {
		if (errno == EAGAIN) {
			return 0;
		} else {
			serverLog(LL_VERBOSE, "Reading from session: %s",strerror(errno));
			return -1;
		}
	} else if (nread == 0) {
		serverLog(LL_VERBOSE, "Connection closed, fd = %d", session->fd);
		return -1;
	}
	input->used += nread;
	return nread;
}

// 返回值>=0表示写入ok，一次性把buf内的字节全部写完
// TODO：可优化
static int writeMsgbuff(netSession * session){
	int fd = session->fd;
	if (fd<0) return -1;

	int wlen = 0;
	msgBuf * output = session->output;
	unsigned char * head = output->buf;
	int left = output->used;
	while(left>0){
		int nwritten = write(fd, head+wlen, left);
		if (nwritten>0){
			left -= nwritten;
			wlen += nwritten;
		}else if (nwritten<0){
			if(errno==EAGAIN){
				break;
			}else if(errno==EINTR){
				continue;
			}else{
				// socket有问题了
				return -1;
			}
		}else{
			return -2;
		}
	}
	if (wlen>0){
		trimBuf(output, wlen);
	}
	return wlen;
}

/* =========================== file event callback =========================== */
static void readFromSession(aeEventLoop *el, int fd, void *privdata, int mask) {
	netSession * session = getSession(fd);
	if (session == NULL) {
		serverLog(LL_VERBOSE, "no session");
		return;
	}

	// 处理buf
	int nread = readMsgbuff(session);
	if (nread>0){
		int headLen = sizeof(msgPack);
		msgBuf * input = session->input;
		unsigned char * head = input->buf;
		int left = input->used; // 剩余可读长度
		while(left >= headLen){
			msgPack * pkt = (msgPack *)head;
			assert(pkt->len >= headLen); // 检查len是否合法
			if (pkt->len > input->used){
				/* 不够一个包 */
				break;
			}else{
				head += pkt->len;
				left -= pkt->len;
				cl_handlePkt(fd, pkt);
			}
		}
		trimBuf(input, input->used - left);
	}else if (nread<0){
		closeSession(session);
	}
}

static void writeToSession(aeEventLoop *el, int fd, void *privdata, int mask) {
	netSession * session = getSession(fd);
	if (session){
		int ok = flushSession(session);
		if (ok!=0){
			closeSession(session);
		}
	}
}

static void acceptTcpHandler(aeEventLoop *el, int fd, void *privdata, int mask) {
	int cport;
	char cip[64];

	int cfd = anetTcpAccept(app.err, fd, cip, sizeof(cip), &cport);
	if (cfd==ANET_ERR){
		if (errno != EWOULDBLOCK){
			serverLog(LL_WARNING, "Accepting session connection: %s", app.err);
		}
		return;
	}

	// 处理接收到fd
	netSession * session = createSession(cfd, cip, cport);
	if (session==NULL){
		serverLog(LL_WARNING, "create session fail: %d", cfd);
		return;
	}

	// 告诉lua层
	cl_netAccpeted(fd);
}

/* =========================== net api =========================== */
static int freeSession(netSession * session){
	// 清空
	int fd = session->fd;
	if (fd>=0){
		flushSession(session);
		aeDeleteFileEvent(app.pEl, fd, AE_READABLE|AE_WRITABLE);
		close(fd);
	}
	serverLog(0,"close session");

	session->port = 0;
	session->ip = NULL;
	session->fd = -1;

	cleanBuf(session->input);
	cleanBuf(session->output);
	return fd;
}

netSession * createSession(int fd, const char *ip, short port){
	if (fd>=app.maxSize){
		serverLog(0, "create session fail, fd = %d, maxSize = %s", fd, app.maxSize);
		return NULL;
	}

	netSession * session = app.session[fd];
	if (session==NULL || session->fd>=0){
		serverLog(0, "create session fail, session = %p", session);
		return NULL;
	}

	if (fd != -1) {
		anetNonBlock(NULL,fd);	// 设置为非阻塞
		anetEnableTcpNoDelay(NULL,fd);

		if (app.tcpkeepalive){
			anetKeepAlive(NULL,fd,app.tcpkeepalive);
		}

		if (aeCreateFileEvent(app.pEl, fd, AE_READABLE, readFromSession, NULL) == AE_ERR){
			freeSession(session);
			return NULL;
		}
	}

	session->fd = fd;
	session->ip = ip;
	session->port = port;
	return session;
}

netSession * getSession(int fd){
	if (fd>=app.maxSize) return NULL;

	netSession * session = app.session[fd];
	if (session->fd != fd) return NULL;
	return session;
}

int flushSession(netSession * session){
	int wlen = writeMsgbuff(session); // 已经写入的长度
	if (wlen<0) return -1;

	int mask = aeGetFileEvents(app.pEl, session->fd);
	if (wlen==0){
		if (mask & AE_WRITABLE){
			aeDeleteFileEvent(app.pEl, session->fd, AE_WRITABLE);
		}
	}else if (wlen>0){
		if (!(mask & AE_WRITABLE)){
			// 如果发送未完，则注册write事件，继续发
			// aeCreateFileEvent(app.pEl, session->fd, AE_WRITABLE, writeToSession, NULL);
		}
	}
	return 0;
}


void closeSession(netSession * session){
	int fd = freeSession(session);
	cl_netClosed(fd);
}

int netListen(int port, char * addr){
	int fd = anetTcpServer(app.err, port, addr, 511); // 监听的socket
	if (fd == ANET_ERR) {
		serverLog(LL_WARNING, "Creating Server TCP listening socket %s:%d: %s", addr ? addr : "*", port, app.err);
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
	int fd = anetTcpConnect(app.err, addr, port);
	if (fd==ANET_ERR){
		serverLog(0, "net connect fail");
		return -1;
	}

	// event
	netSession * session = createSession(fd, addr, port);
	if (session==NULL){
		serverLog(LL_WARNING, "session create fail...");
		return -1;
	}
	return fd;
}

int netWrite(int fd, msgPack * pkt){
	netSession * session = getSession(fd);
	if(session==NULL){
		serverLog(0,"session not found,fd = %d", fd);
		return -1;
	}

	appendBuf(session->output, (unsigned char *)pkt, pkt->len);
	int ok = flushSession(session);
	if (ok!=0){
		closeSession(session);
	}
	return ok;
}