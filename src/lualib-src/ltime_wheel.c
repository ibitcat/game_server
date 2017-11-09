// tick最大支持256*64*64*64*64=2^32个

#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>

#include <sys/time.h>
#include <ltime_wheel.h>


struct timer_node {
	uint32_t expire;				// 到期时间
	uint32_t id;					// 定时器id
	int32_t level;					// -1表示在near内
	int32_t index;
	uint32_t isuse;					// 1表示在使用中的节点
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
	uint64_t currentMs;				// 当前毫秒数
	uint32_t usedNum;				// 定时器工厂大小
	struct timer_node **usedTimer;	// 定时器工厂（所有的定时器节点都是从这里产生的）
	struct timer_list freeTimer;	// 回收的定时器
	timer_cb timerCb;				// 定时器回调函数
	lua_State* L;
};

static struct timer *TI = NULL;
static uint64_t startMsec = 0;		// 定时器创建时的时间戳（ms）

static uint64_t gettime(){
	struct timeval tv;
    gettimeofday(&tv, 0);
    uint64_t sec = (uint64_t)tv.tv_sec;
    uint32_t msec = (uint32_t)tv.tv_usec/1000;
    uint64_t ms = sec * 1000 + msec;
   	return ms;
}

static struct timer_node * list_clear(struct timer_list *list){
	struct timer_node *nodeHead = list->head;
	list->head = NULL;
	list->tail = NULL;
	return nodeHead;
}

static void link_node(struct timer_list *list, struct timer_node *node){
	assert(list);
	assert(node);
	node->next = NULL;
	node->pre = NULL;
	if (list->head==NULL){
		list->head = node;
		list->tail = node;
	}else{
		node->pre = list->tail;
		list->tail->next = node;
		list->tail = node;
	}
}

static struct timer_node * node_alloc(){
	if (TI->freeTimer.tail==NULL){
		uint32_t newNum = TI->usedNum + 64;	//每次增加64个
		struct timer_node **us = (struct timer_node **)realloc(TI->usedTimer, sizeof(struct timer_node *)*newNum);
		assert(us);
		for (int i = TI->usedNum; i < newNum; ++i){
			struct timer_node *node = (struct timer_node *)malloc(sizeof(struct timer_node));
			memset(node, 0, sizeof(*node));
			node->id = i + 1;
			us[i]= node;

			// 插入到freeTimer
			link_node(&TI->freeTimer, node);
		}
		TI->usedTimer = us;
		TI->usedNum = newNum;
	}

	// 尝试从freeTimer里面弹出一个
	struct timer_node * pNode = NULL;
	pNode = TI->freeTimer.tail;
	if (TI->freeTimer.tail == TI->freeTimer.head){
		// 最后一个
		TI->freeTimer.tail = TI->freeTimer.head = NULL;
	}else{
		TI->freeTimer.tail = pNode->pre;
	}

	// 节点重置
	assert(pNode);
	pNode->pre = NULL;
	pNode->next = NULL;
	return pNode;
}

static void add_node(struct timer_node *node){
	uint32_t expTick = node->expire; 	// 到期tick
	uint32_t curTick = TI->tick;		// 当前tick

	if ((expTick|255)==(curTick|255)) { // 检查expTick是否在curTick所在的桶内
		uint32_t idx = expTick%256; 	// 等同于 expTick&255
		node->level = -1;
		node->index = idx;
		node->isuse = 1;
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
		node->level = i;
		node->index = idx;
		node->isuse = 1;
		link_node(&TI->tv[i][idx],node);
	}
}

// 俗称 cascades
static void move_list(uint32_t level, uint32_t index){
	assert(level>=0 && level<4);
	assert(index>=0 && index<64);
	struct timer_node *headNode = list_clear(&(TI->tv[level][index]));
	while(headNode!=NULL){
		struct timer_node *temp = headNode;
		headNode = headNode->next;
		add_node(temp);
	}
}

static void timer_shift(){
	uint32_t curTick = ++TI->tick;
	if (curTick==0){
		//溢出，即整个时间轮已经转弯一轮，开始进入下一轮
		//因为 add_timer 添加的已经溢出的timer都放在了tv[3][0]中，
		//所以，当一整轮转完后，在进入下一个轮时，要把溢出的timer再重新添加进去
		move_list(3,0);
	}else{
		uint32_t mask = 256;
		uint32_t temp = curTick>>8;
		int i = 0;

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

static void timer_exec(){
	int idx = TI->tick & 255;
	struct timer_node *pHead = list_clear(&TI->near[idx]);
	while(pHead){
		// 派发事件
		printf("timer id = %d, starttime = %lld, curTime = %llu, diff= %d \n", pHead->id, startMsec, TI->currentMs, TI->currentMs-startMsec);
		TI->timerCb(TI->L, pHead->id);
		struct timer_node *pTemp = pHead;
		pHead = pHead->next;

		pTemp->next = NULL;
		pTemp->pre = NULL;
		pTemp->isuse = 0;
		link_node(&TI->freeTimer, pTemp);
	}
}


/*----------------------------------- API -----------------------------------*/
uint32_t add_timer(uint32_t tickNum){
	assert(TI);
	assert(tickNum>0);

	struct timer_node *node = node_alloc();
	assert(node);
	node->next = NULL;
	node->expire = TI->tick + tickNum; //tickNum个滴答后到期
	add_node(node);
	return node->id;
}

void del_timer(uint32_t id){
	assert(id>0);
	assert(id<=TI->usedNum);
	struct timer_node *pNode = TI->usedTimer[id-1];
	if (pNode->isuse>0){
		int32_t level = pNode->level;
		int32_t index = pNode->index;
		struct timer_list *pTmList;
		if (level<0){
			pTmList = &TI->near[index];
		}else{
			pTmList = &TI->tv[level][index];
		}

		if (pTmList->head == pNode){
			if (pNode->next!=NULL){
				pTmList->head = pNode->next;
			}else{
				pTmList->head = pTmList->tail = NULL;
			}
		}else if(pTmList->tail == pNode){
			pTmList->tail = pNode->pre;
		}else{
			pNode->pre->next = pNode->next;
		}

		memset(pNode,0,sizeof(*pNode));
		pNode->id = id;

		//插入到freeTimer
		link_node(&TI->freeTimer, pNode);
	}
}

int32_t timer_updatetime(void) {
	uint64_t cp = gettime();
	int64_t diff = (int64_t)(cp - TI->currentMs);
	if(diff<0) {
		//skynet_error(NULL, "time diff error: change from %lld to %lld", cp, TI->current_point);
		printf("fuck cp = %llu\n",cp);
		TI->currentMs = cp;
		return -1;
	} else if (diff>=10) {
		int i;
		for (i=0;i<(int)diff/10;i++) {
			TI->currentMs += 10;
			timer_shift();
			timer_exec();
		}

		uint64_t nextMs = TI->currentMs + 10;
		int32_t sleepMs = nextMs - cp;
		sleepMs = sleepMs>0 ? sleepMs : 0;
		return sleepMs;
	}
	return 10;
}

void create_timer(lua_State *L, timer_cb cb){
	struct timer *ti = (struct timer *)malloc(sizeof(struct timer));
	assert(ti!=NULL);
	memset(ti,0,sizeof(*ti));
	ti->L = L;
	ti->timerCb = cb;

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
    ti->starttime = (uint32_t)tv.tv_sec;
    uint64_t ms = tv.tv_sec * 1000 + tv.tv_usec / 1000;
    ti->currentMs = ms;
    startMsec = ms;

    TI = ti;
}

void destroy_timer(){
	int i;
	for (i = 0; i < 256; ++i){
		list_clear(&TI->near[i]);
	}

	for (i = 0; i < 4; ++i){
		for (int j = 0; j < 64; ++j){
			list_clear(&TI->tv[i][j]);
		}
	}
	list_clear(&TI->freeTimer);

	// free
	for (int i = 0; i < TI->usedNum; ++i){
		free(TI->usedTimer[i]);
	}
	free(TI);
}

/*----------------------------------- lua API -----------------------------------*/
static int lc_add_timer(lua_State *L){
	lua_Integer tickNum = lua_tointeger(L, 1);
	uint32_t timerId = add_timer((uint32_t)tickNum);
	lua_pushinteger(L, timerId);
	return 1;
}

static int lc_del_timer(lua_State *L){
	lua_Integer timerId = lua_tointeger(L, 1);
	del_timer((uint32_t)timerId);
	return 0;
}

int luaopen_timewheel(struct lua_State* L){
	luaL_checkversion(L);

	luaL_Reg l[] = {
		{ "addTimer", lc_add_timer },
		{ "delTimer", lc_del_timer },
		{ NULL,  NULL }
	};
	luaL_newlib(L,l);
	return 1;
}