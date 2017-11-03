// tick最大支持256*64*64*64*64=2^32个

#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>

#include <sys/time.h>


struct timer_node {
	uint32_t expire;	// 到期时间
	uint32_t id;		// 定时器id
	int32_t level;		// -1表示在near内
	uint32_t index;
	struct timer_node *pre;
	struct timer_node *next;
};

struct timer_list{
	struct timer_node *head;
	struct timer_node *tail;
};

struct timer{
	struct timer_list near[256];
	struct timer_list tv[4][64];
	uint32_t tick;					// 经过的滴答数,2^32个滴答
	uint32_t starttime;				// 定时器启动时的时间戳（秒）
	uint64_t currentms;				// 定时器启动时的毫秒数
	uint64_t current_point;			// 定时器启动时的时间戳（毫秒）
	uint32_t usedNum;				// 定时器工厂大小
	struct timer_node **usedTimer;	// 定时器工厂（所有的定时器节点都是从这里产生的）
	struct timer_list freeTimer;	// 回收的定时器
};

static struct timer *TI = NULL;

static struct timer_node * list_clear(struct timer_list *list){
	struct timer_node *nodeHead = list->head;
	list->head = NULL;
	list->tail = NULL;
	return nodeHead;
}

static void list_free(struct timer_list *list){
	while(list->head){
		struct timer_node *nextNode = list->head->next;
		free(list->head);
		list->head = nextNode;
	}
	list->tail = NULL;
}

static void link_node(struct timer_list *list, struct timer_node *node){
	assert(list);
	assert(node);
	node->next = NULL;
	if (list->head==NULL){
		list->head = node;
		list->tail = node;
	}else{
		list->tail->next = node;
		list->tail = node;
	}
}

static struct timer_node * node_alloc(){
	// 先尝试从freeTimer里面找一个
	struct timer_node * pNode = NULL;
	if (TI->freeTimer.tail!=NULL){
		pNode = TI->freeTimer->tail;
		if (TI->freeTimer->tail == TI->freeTimer->head){
			// 最后一个
			TI->freeTimer->tail = TI->freeTimer->head = NULL;
		}else{
			TI->freeTimer->tail = pNode->pre;
		}

		// 节点重置
		pNode->pre = NULL;
		pNode0->next = NULL;
	}
	return pNode;
}

static void add_node(struct timer_node *node){
	uint32_t expTick = node->expire; 	// 到期tick
	uint32_t curTick = TI->tick;		// 当前tick

	if ((expTick|255)==(curTick|255)) { // 检查expTick是否在curTick所在的桶内
		uint32_t idx = expTick%256; 	// 等同于 expTick&255
		link_node(&TI->near[idx],node);
	} else {
		int i;
		uint32_t mask = 256 << 6;
		for (i=0;i<3;i++) {
			if ((expTick|(mask-1))==(curTick|(mask-1))) {
				// 这里检查expTick是否与curTick在同一个范围内
				break;
			}
			mask <<= 6;
		}

		uint32_t idx = (expTick>>(8+i*6))%64;
		link_node(&TI->tv[i][idx],node);
	}
}

// 俗称 cascades
static void move_list(uint32_t level, uint32_t index){
	assert(level>=0 && level<4)
	assert(index>=0 && index<64)
	struct timer_node *headNode = list_clear(&(TI->tv[level][index]));
	while(headNode!=NULL){
		struct timer_node *temp = headNode;
		add_node(temp);
	}
}

uint32_t add_timer(uint32_t tm){
	assert(TI);
	uint32_t ticks = tm; // 暂时把tm作为tick数量

	struct timer_node *node = (struct timer_node *)malloc(sizeof(*node));
	assert(node~=NULL);
	node->next = NULL;
	node->expire = TI.tick + tm; //tm个tick后到期
	add_node(node);
}

void del_timer(){
}

void timer_shift(){
	uint32_t curTick = ++TI->tick;
	if (curTick==0){
		//溢出，即整个时间轮已经转弯一轮，开始进入下一轮
		//因为 add_timer 添加的已经溢出的timer都放在了tv[3][0]中，
		//所以，当一整轮转完后，在进入下一个轮时，要把溢出的timer再重新添加进去
		move_list(3,0);
	}else{
		uint32_t mask = 256;
		uint32_t temp = curTick>>8;
		int i;

		// 每 TIME_NEAR 次循环，执行一次重建事件层级的逻辑
		// 每 TIME_LEVEL 次，执行一次下一级事件重建逻辑
		while((curTick%mask)==0){ //是不是跑完了一轮(256小轮 64大轮)
			int idx = temp%64; // 如果idx==0，则表示一个大轮也已经跑完，需要调到更上一层的大轮，把上一层大轮内的timer散开
			if (idx!=0){
				move_list(i,idx);
				break;
			}
			mask <<= 6;
			temp >>= 6;
			++i;
		}
	}
}

void create_timer(){
	struct timer *ti = (struct timer *)malloc(sizeof(struct timer));
	assert(ti!=NULL);
	memset(ti,0,sizeof(*ti));

	int i;
	for (i = 0; i < 256; ++i){
		list_clear(&(ti->near[i]));
	}

	for (i = 0; i < 4; ++i){
		for (int j = 0; j < 64; ++j){
			list_clear(&(ti->tv[i][j]));
		}
	}

	struct timeval tv;
    gettimeofday(&tv, 0);
    ti.starttime = (uint32_t)tv.tv_sec;
    ti.currentms = (uint64_t)tv.tv_usec/1000;
    uint64_t ms = tv.tv_sec * 1000 + tv.tv_usec / 1000;
    ti.current_point = ms;
    printf("sec = %d\n", ms);

    TI = ti;
}

void destroy_timer(){
	int i;
	for (i = 0; i < 256; ++i){
		list_free(&(ti->near[i]));
	}

	for (i = 0; i < 4; ++i){
		for (int j = 0; j < 64; ++j){
			list_free(&(ti->tv[i][j]));
		}
	}
	free(TI);
}