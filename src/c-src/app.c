#include <assert.h>
#include <stdlib.h>
#include <string.h> //memmove
#include <errno.h>
//#include <unistd.h> usleep 头文件

#include "app.h"
#include "lsnowflake.h"
#include "lenv.h"

appServer app;

// lua extra libs
static struct luaL_Reg libs[] = {
	{"snowflake", luaopen_snowflake},
	{"env", luaopen_env},
	{NULL, NULL}
};

static void initConfig(){
	// TODO
}

// lua traceback
static int ltraceback(lua_State *L){
	const char *msg = lua_tostring(L, 1);
	if (msg){
		luaL_traceback(L, L, msg, 1);
	}else {
		printf("lua traceback\n");
		lua_pushliteral(L, "(no error message)");
	}
	return 1;
}

int createApp(){
	lua_State *L = luaL_newstate();
	luaL_openlibs(L);  // 加载Lua通用扩展库
	app.L = L;
	app.pEl = aeCreateEventLoop(1024*10);
	app.nextClientId = 0;

	// upvalue
	lua_pushlightuserdata(L, &app);
	lua_setfield(L, LUA_REGISTRYINDEX, "app_context");

	// open lua lib
	struct luaL_Reg* lib = libs;
	for (;lib->name!=NULL; ++lib){
		luaL_requiref(L, lib->name, lib->func,1);
		lua_pop(L,1);
	}

	// load main.lua
	lua_pushcfunction(L, ltraceback);
	assert(lua_gettop(L) == 1);
	int r = luaL_loadfile(L,"../main.lua");
	if (r != LUA_OK) {
		printf("load lua file fail, err = %s\n", lua_tostring(L,-1));
		return 1;
	}

	r = lua_pcall(L,0,0,1);
	if (r != LUA_OK) {
		printf("lua loader error : %s\n", lua_tostring(L, -1));
		return 1;
	}
	return 0;
};

int runApp(){
	aeMain(app.pEl);	//loop
	aeDeleteEventLoop(app.pEl);
	lua_close(app.L);
	return 0;
}

void readQueryFromClient(aeEventLoop *el, int fd, void *privdata, int mask) {
	appClient * client = (appClient *)privdata;
	if (client->usedlen==client->buflen){
		// 没有空闲buf了，增加2倍长度
		client->buflen *= 2;
		client->querybuf = realloc(client->querybuf, client->buflen);
	}
	int readlen = client->buflen - client->usedlen;
	int nread = read(fd, client->querybuf+client->usedlen, readlen);
	printf("read from client = %s\n", client->querybuf);
	if (nread == -1) {
		if (errno == EAGAIN) {
			return;
		} else {
			serverLog(LL_VERBOSE, "Reading from client: %s",strerror(errno));
			//freeClient(c);
			return;
		}
	} else if (nread == 0) {
		serverLog(LL_VERBOSE, "Client closed connection");
		//freeClient(c);
		return;
	}

	// 处理buf
	int headLen = sizeof(struct msgBuff);
	int pos = 0;
	int diff = client->usedlen - pos;
	while(diff>=headLen){
		struct msgBuff * buf = (struct msgBuff *)(client->querybuf+pos);
		// 检查len是否合法
		// TODO
		assert(buf->len>=headLen);
		assert(pos+buf->len<=client->usedlen);
		pos += buf->len;
		diff = client->usedlen - pos;

		// pcall
		// TODO
	}
	client->usedlen = diff;
	memmove(client->querybuf, client->querybuf+pos, diff);
}

appClient * createClient(int fd){
	appClient * client = malloc(sizeof(*client));
	if (client==NULL){
		printf("create client fail\n");
		return NULL;
	}

	if (fd != -1) {
		anetNonBlock(NULL,fd);	// 设置为非阻塞
		anetEnableTcpNoDelay(NULL,fd);

		if (app.tcpkeepalive){
			anetKeepAlive(NULL,fd,app.tcpkeepalive);
		}

		if (aeCreateFileEvent(app.pEl, fd, AE_READABLE, readQueryFromClient, client) == AE_ERR){
			close(fd);
			free(client);
			return NULL;
		}
	}

	client->id = ++app.nextClientId;
	client->buflen = 1024;	// 初始buff=1kb
	client->querybuf = (unsigned char *)malloc(1024);
	client->usedlen = 0;
	return client;
}