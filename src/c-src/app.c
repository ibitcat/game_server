#include <stdlib.h>
#include <string.h> //memmove
//#include <unistd.h> usleep 头文件

#include "app.h"
#include "lenv.h"

appServer app;

// lua extra libs
static struct luaL_Reg libs[] = {
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
		serverLog(0, "[LUA ERROR]%s\n", lua_tostring(L, -1));
	}else {
		lua_pushliteral(L, "(no error message)");
	}
	return 1;
}

int createApp(const char * sty, int sid){
	// app
	app.maxSize = 1024;
	app.sty = sty[0];
	app.sid = sid;
	memset(app.err, 0, sizeof(app.err));
	app.session = (netSession**)malloc(app.maxSize * sizeof(netSession*));
	for (int i = 0; i < app.maxSize; ++i){
		NEW_SESSION(i);
	}
	app.pEl = aeCreateEventLoop(app.maxSize);

	// lua
	lua_State *L = luaL_newstate();
	luaL_openlibs(L);  // 加载Lua通用扩展库
	app.L = L;

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
	char tmp[64];
	lua_pushcfunction(L, ltraceback);
	app.luaErrPos = lua_gettop(L);
	sprintf(tmp,"../src/l-src/%c/main.lua",app.sty);
	int r = luaL_loadfile(L,tmp);
	if (r != LUA_OK) {
		serverLog(0, "load lua file fail, err = %s", lua_tostring(L,-1));
		return -1;
	}
	r = lua_pcall(L, 0, 0, app.luaErrPos);
	if (r != LUA_OK) {
		return -1;
	}

	// 通知lua，初始化ok
	return 0;
};

int runApp(){
	aeMain(app.pEl);	//loop
	aeDeleteEventLoop(app.pEl);
	lua_close(app.L);
	return 0;
}