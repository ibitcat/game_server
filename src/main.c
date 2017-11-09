#include <stdio.h>
#include <unistd.h>
#include <assert.h>

//lua
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "luuid.h"
#include "lsnowflake.h"
#include "ltime_wheel.h"
#include "lenv.h"
#include "app.h"


/*
uuid 完成
snowflake 完成
aoi
a星寻路
双数组字典树屏蔽字
postgre sql 驱动
地图
定时器（时间轮） 完成
*/

appServer app;
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

// 定时器回调
void timer_callback(lua_State *L, uint32_t timerId){
	int top = lua_gettop(L);
	int rType = lua_getglobal(L,"onTimer");
	int status = lua_pcall(L,0,1,1);
	if (status != LUA_OK) {
		printf("timer cb err = %s\n", lua_tostring(L,-1));
		return;
	}else{
		if (lua_isinteger(L,-1)){
			//printf("onTimer result = %d\n",lua_tointeger(L,-1));
		}
	}
	lua_settop(L,top);
}

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

static void initApp(){
	lua_State *L = luaL_newstate();
	luaL_openlibs(L);  // 加载Lua通用扩展库
	app.L = L;
	app.pEl = aeCreateEventLoop(1024*10);

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
	aeMain(app.pEl);
};

int main(int argc, char** argv)
{
	extern appServer app;
	printf("\n\n");
	/*
	create_timer(app.L, timer_callback);
	add_timer(100);
	for (;;) {
		int32_t sleepMs = timer_updatetime();
		//printf("loop ……, sleepMs=%d\n",sleepMs);
		if(sleepMs<0){
			break;
		}else{
			usleep(sleepMs*1000);
		}
	}
	destroy_timer();
	*/
	initApp();
	lua_close(app.L);
	return 0;
}
