#ifndef _APP_H_
#define _APP_H_

// lua
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

// event loop
#include "ae.h"
#include "anet.h"

typedef struct appClient{
	char * ip;		// 连接ip
	int port;		// 连接端口
	int fd;			// fd
} appClient;


typedef struct appServer{
	char *bindaddr;
	short port;
	int size;
	char err[1024];
	char neterr[1024];
	char family;			// 进程类型
	unsigned char index; 	// 进程编号 0-255

	lua_State *L;
	aeEventLoop *pEl;
} appServer;


int initApp(appServer *app);
int runApp(appServer *app);

appClient * createClient(fd);
int freeClient(appClient * client);
#endif