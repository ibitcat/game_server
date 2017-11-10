#include <assert.h>
//#include <unistd.h> usleep 头文件

#include "app.h"

#include "luuid.h"
#include "lsnowflake.h"
#include "ltime_wheel.h"
#include "lenv.h"
#include "app.h"

// lua extra libs
static struct luaL_Reg libs[] = {
	{"luuid", luaopen_uuid},
	{"lsnowflake", luaopen_snowflake},
	{"ltimewheel", luaopen_timewheel},
	{"lenv", luaopen_env},
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

int initApp(appServer *app){
	lua_State *L = luaL_newstate();
	luaL_openlibs(L);  // 加载Lua通用扩展库
	app->L = L;
	app->pEl = aeCreateEventLoop(1024*10);

	// upvalue
	lua_pushlightuserdata(L, app);
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

int runApp(appServer *app){
	aeMain(app->pEl);	//loop
	aeDeleteEventLoop(app->pEl);
	lua_close(app->L);
}