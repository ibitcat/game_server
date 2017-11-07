// 时间轮

#ifndef _LTIME_WHEEL_H_
#define _LTIME_WHEEL_H_

#include <lua.h>
#include <lauxlib.h>

typedef void (*timer_cb)(lua_State *L, uint32_t timerId);

void create_timer(lua_State *L, timer_cb cb);
void destroy_timer(void);
uint32_t add_timer(uint32_t tickNum);
void del_timer(uint32_t id);
int32_t timer_updatetime(void);

// lua call
int luaopen_timewheel(struct lua_State* L);

#endif