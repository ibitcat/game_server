// 时间轮

#ifndef _LTIME_WHEEL_H_
#define _LTIME_WHEEL_H_

void create_timer(void);
void destroy_timer(void);
uint32_t add_timer(uint32_t tm);
void del_timer(uint32_t id);
int32_t timer_updatetime(void);

#endif