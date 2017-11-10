#include "ltime_wheel.h"
#include "app.h"


int ltimeCallback(uint32_t id, void *clientData){
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

static int lc_add_timer(lua_State *L){
	appServer *app = lua_touserdata(L, lua_upvalueindex(1));
	if (app==NULL){
		return luaL_error(L, "lc_add_timer error");
	}

	lua_Integer tickNum = lua_tointeger(L, 1);
	uint32_t timerId = aeAddTimer(app->pEl, tickNum, ltimeCallback, app);
	lua_pushinteger(L, timerId);
	return 1;
}

static int lc_del_timer(lua_State *L){
	appServer *app = lua_touserdata(L, lua_upvalueindex(1));
	if (app==NULL){
		return luaL_error(L, "lc_del_timer error");
	}

	lua_Integer timerId = lua_tointeger(L, 1);
	aeDelTimer(app->pEl, (uint32_t)timerId);
	return 0;
}

int luaopen_timewheel(struct lua_State* L){
	luaL_checkversion(L);

	luaL_Reg l[] = {
		{ "addTimer", lc_add_timer },
		{ "delTimer", lc_del_timer },
		{ NULL,  NULL }
	};

	luaL_newlibtable(L, l);
	lua_getfield(L, LUA_REGISTRYINDEX, "app_context");
	appServer *app = lua_touserdata(L,-1);
	if (app == NULL) {
		return luaL_error(L, "Init app timer first ");
	}
	luaL_setfuncs(L,l,1);//注册，并共享一个upvalue
	//luaL_newlib(L,l);
	return 1;
}