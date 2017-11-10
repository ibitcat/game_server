#ifndef _APP_H_
#define _APP_H_

// lua
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

// event loop
#include "ae.h"
#include "anet.h"

typedef struct appServer{
	char *bindaddr;
	short port;
	char err[1024];
	char neterr[1024];
	char family;			// 进程类型
	unsigned char index; 	// 进程编号 0-255

	lua_State *L;
	aeEventLoop *pEl;
} appServer;


int initApp(appServer *app);
int runApp(appServer *app);

#endif