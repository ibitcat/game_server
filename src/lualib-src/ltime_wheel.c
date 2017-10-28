
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>

struct timer_node {
	uint32_t expire;	// 到期时间
	struct timer_node *next;
};

struct timer{
	struct timer_node *near[256];
	struct timer_node *tv[4][64];
	uint32_t time;
	uint32_t starttime;
	uint64_t current;
	uint64_t current_point;
};

static struct timer *TI = NULL;

void create_timer(){
	struct timer *ti = (struct timer *)malloc(sizeof(struct timer));
	assert(ti!=NULL);
	memset(ti,0,sizeof(*ti));

	TI = ti;
}