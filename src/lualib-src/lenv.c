#include "lenv.h"
#include "app.h"
#include <errno.h>

#define LL_DEBUG 0
#define LL_VERBOSE 1
#define LL_NOTICE 2
#define LL_WARNING 3

void serverLog(int level, const char *fmt, ...) {
	va_list ap;
	char msg[1024];

	va_start(ap, fmt);
	vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);
}

static int lc_net_connect(lua_State *L){
	// 如果是同一个物理机，可以使用unix域套接字
	// 否则，使用tcp socket 连接
	appServer *app = lua_touserdata(L, lua_upvalueindex(1));
	if (app==NULL){
		return luaL_error(L, "lc_net_connect error");
	}

	char *addr = luaL_checkstring(L, -1);
	int port = (int)luaL_checkinteger(L, -2);
	int fd = anetTcpConnect(app->neterr, addr, port);
	if (fd==ANET_ERR){
		printf("net connect fail\n");
	}
	lua_pushinteger(L, fd);
	return 1;
}

void acceptTcpHandler(aeEventLoop *el, int fd, void *privdata, int mask) {
	int cport;
	char cip[64];

	appServer *app = (appServer *)privdata;
	int cfd = anetTcpAccept(app->neterr, fd, cip, sizeof(cip), &cport);
	if (cfd==ANET_ERR){
		if (errno != EWOULDBLOCK){
			serverLog(LL_WARNING, "Accepting client connection: %s", app->neterr);
		}
		return;
	}

	printf("Accepted %s:%d \n", cip, cport);
	char *str = "hello, accepted……";
	write(cfd,str,strlen(str));
	//serverLog(LL_VERBOSE,"Accepted %s:%d", cip, cport);
	//acceptCommonHandler(cfd,0,cip);
}

static int lc_net_listen(lua_State *L){
	appServer *app = lua_touserdata(L, lua_upvalueindex(1));
	if (app==NULL){
		return luaL_error(L, "lc_net_connect error");
	}

	char *addr = luaL_checkstring(L, 1);
	int port = (int)luaL_checkinteger(L, 2);
	int fd = anetTcpServer(app->neterr, port, addr, 511);
	if (fd == ANET_ERR) {
		serverLog(LL_WARNING, "Creating Server TCP listening socket %s:%d: %s", addr ? addr : "*", port, app->neterr);
	}else{
		anetNonBlock(NULL, fd);

		// listen
		if (aeCreateFileEvent(app->pEl, fd, AE_READABLE, acceptTcpHandler, app) == -1){
			return luaL_error(L, "Unrecoverable error creating server.ipfd file event.");
		}
	}

	lua_pushinteger(L, fd);
	return 1;
}

// 需要的一些杂项函数
int luaopen_env(struct lua_State* L){
	luaL_checkversion(L);

	luaL_Reg l[] = {
		{ "net_connect", lc_net_connect },
		{ "net_listen", lc_net_listen },
		{ NULL,  NULL }
	};

	luaL_newlibtable(L, l);
	lua_getfield(L, LUA_REGISTRYINDEX, "app_context");
	appServer *app = lua_touserdata(L,-1);
	if (app == NULL) {
		return luaL_error(L, "Init app context first ");
	}
	luaL_setfuncs(L,l,1);//注册，并共享一个upvalue
	return 1;
}