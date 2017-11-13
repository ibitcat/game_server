#ifndef _APP_H_
#define _APP_H_

#include <assert.h>

// lua
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

// event loop
#include "ae.h"
#include "anet.h"

#define LL_DEBUG 0
#define LL_VERBOSE 1
#define LL_NOTICE 2
#define LL_WARNING 3

static inline void serverLog(int level, const char *fmt, ...) {
	va_list ap;
	char msg[1024];

	va_start(ap, fmt);
	vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);
	printf("[log][%d]:%s\n",level, msg);
}

// 整个包长度|flag预留|from type|from id|to type|to id|协议号|消息内容
struct msgBuff{
	int len;
	int flag;
	char fromType;
	unsigned char fromId;
	char toType;
	unsigned char toId;
	unsigned int cmd;
	char buf[];
};

// 网络会话
typedef struct netSession{
	unsigned int id;		// 连接id
	int port;				// 连接端口
	int fd;					// fd
	unsigned int usedlen; 	// 已经用掉的长度
	unsigned int buflen;	// buf总长度
	char * ip;				// 连接ip
	char * querybuf;		// buf
	struct netSession * next;
} netSession;

typedef struct sessionList{
	netSession *head;
	netSession *tail;
	int count;
} sessionList;

typedef struct appServer{
	unsigned int nextClientId;
	char *bindaddr;
	short port;
	int size;
	char err[1024];
	char neterr[1024];
	char family;			// 进程类型
	unsigned char index; 	// 进程编号 0-255
	int tcpkeepalive;

	lua_State *L;
	aeEventLoop *pEl;
} appServer;


int createApp();
int runApp();

netSession * createSession(int fd);
int freeSession(netSession * session);

// net api
int netListen(int port, char * addr);
int netConnect(char * addr, int port);
#endif