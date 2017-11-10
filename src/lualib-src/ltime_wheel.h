// 时间轮

#ifndef _LTIME_WHEEL_H_
#define _LTIME_WHEEL_H_

#include <lua.h>
#include <lauxlib.h>
#include <stdint.h>

typedef void (*timer_cb)(lua_State *L, int timerId);

// lua call
int luaopen_timewheel(struct lua_State* L);

#endif