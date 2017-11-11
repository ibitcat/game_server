#include <stdio.h>
#include <errno.h>

// uuid
#include "uuid.h"
#include "lbase64.h"

#include "lenv.h"
#include "app.h"

static int ltimeCallback(uint32_t id, void *clientData){
	printf("ltimeCallback : timer id = %d\n", id);
	appServer *app = (appServer *)clientData;
	lua_State *L = app->L;

	int top = lua_gettop(L);
	int rType = lua_getglobal(L,"onTimer");
	int status = lua_pcall(L,0,1,0);
	if (status != LUA_OK) {
		printf("timer cb err = %s\n", lua_tostring(L,-1));
		return;
	}else{
		if (lua_isinteger(L,-1)){
			//printf("onTimer result = %d\n",lua_tointeger(L,-1));
		}
	}
	lua_settop(L,top);
	return 0;
}

static void acceptTcpHandler(aeEventLoop *el, int fd, void *privdata, int mask) {
	int cport;
	char cip[64];

	appServer *app = (appServer *)privdata;
	int cfd = anetTcpAccept(app->neterr, fd, cip, sizeof(cip), &cport);
	printf("%d\n", cfd);
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

	// 处理接收到fd
	appClient * client = createClient(cfd);
	if (client==NULL){
		serverLog(LL_WARNING, "create client fail: %d", cfd);
		return;
	}
}


/* =========================== lua api =========================== */
static int lc_uuid(lua_State *L){
	// uuid
	uuid_t uu;
	uuid_generate(uu);

	size_t outSize;
	unsigned char *uuidstr = base64_encode(uu, sizeof(uu), &outSize);
	uuidstr[22] = '\0';
	for (int i = 0; i < 22; ++i){
		if (*(uuidstr+i)=='/'){
			*(uuidstr+i) = '-'; // 替换"/"
		}
	}
	lua_pushlstring(L,uuidstr,22);
	return 1;
}

// net
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

static int lc_net_sendmsg(){
	return 0;
}

// timer
static int lc_timer_add(lua_State *L){
	appServer *app = lua_touserdata(L, lua_upvalueindex(1));
	if (app==NULL){
		return luaL_error(L, "lc_add_timer error");
	}

	lua_Integer tickNum = lua_tointeger(L, 1);
	uint32_t timerId = aeAddTimer(app->pEl, tickNum, ltimeCallback, app);
	lua_pushinteger(L, timerId);
	return 1;
}

static int lc_timer_del(lua_State *L){
	appServer *app = lua_touserdata(L, lua_upvalueindex(1));
	if (app==NULL){
		return luaL_error(L, "lc_del_timer error");
	}

	lua_Integer timerId = lua_tointeger(L, 1);
	aeDelTimer(app->pEl, (uint32_t)timerId);
	return 0;
}


// c lua lib
int luaopen_env(struct lua_State* L){
	luaL_checkversion(L);

	luaL_Reg l[] = {
		{ "uuid", lc_uuid },
		{ "net_connect", lc_net_connect },
		{ "net_listen", lc_net_listen },
		{ "addTimer", lc_timer_add },
		{ "delTimer", lc_timer_del },
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