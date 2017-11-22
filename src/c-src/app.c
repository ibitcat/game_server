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
	}else {
		printf("lua traceback\n");
		lua_pushliteral(L, "(no error message)");
	}
	return 1;
}

int createApp(const char * sty, int sid){
	// app
	app.maxSize = 1024;
	app.sty = sty[1];
	app.sid = sid;
	app.session = (netSession**)malloc(app.maxSize * sizeof(netSession*));
	memset(&app.freelist, 0, sizeof(sessionList));

	for (int i = 1; i <= app.maxSize; ++i){
		NEW_SESSION(i);
	}

	// lua
	lua_State *L = luaL_newstate();
	luaL_openlibs(L);  // 加载Lua通用扩展库
	app.L = L;
	app.pEl = aeCreateEventLoop(app.maxSize);
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
	char tmp[64];
	sprintf(tmp,"../src/l-src/%c/main.lua",app.sty);
	int r = luaL_loadfile(L,tmp);
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