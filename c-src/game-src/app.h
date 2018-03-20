#ifndef _APP_H_
#define _APP_H_

#include <assert.h>

// lua
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

// event loop
#include "aev/ae.h"
#include "aev/anet.h"

#include "log.h"
#include "msgbuf.h"

#define NEW_SESSION(i) do { \
	netSession * ses = (netSession*)malloc(sizeof(netSession)); \
	ses->fd = -1;\
	ses->port = 0;\
	ses->ip = NULL;\
	ses->input = newBuf(1024);\
	ses->output = newBuf(1024);\
	app.session[(i)] = ses;\
} while(0)

// 整个包长度|flag预留|from type|from id|to type|to id|协议号|消息内容
// 紧凑模式（即不对齐内存）
typedef struct __attribute__ ((__packed__)) msgPack{
	int len;
	int flag;
	unsigned int cmd;
	char fromType;
	char toType;
	unsigned char fromId;
	unsigned char toId;
	char buf[];
} msgPack;

// 网络会话
typedef struct netSession{
	int fd;						// fd
	char *ip;					// 连接ip
	int port;					// 连接端口
	msgBuf *input;
	msgBuf *output;
} netSession;

typedef struct appServer{
	int maxSize;
	char err[1024];
	char sty;					// 进程类型
	unsigned char sid; 			// 进程编号 0-255
	int tcpkeepalive;
	netSession **session;		// 会话列表(maxSize个)
	int luaErrPos;

	lua_State *L;
	aeEventLoop *pEl;
} appServer;


int createApp(const char * sty, int sid);
int runApp();

netSession * createSession(int fd, const char *ip, short port);
netSession * getSession(int fd);
void closeSession(netSession * session);
int flushSession(netSession * session);

// net api
int netListen(int port, char * addr);
int netConnect(char * addr, int port);
int netWrite(int fd, msgPack *pkt);
#endif