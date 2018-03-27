// tick最大支持256*64*64*64*64=2^32个

#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/time.h>

#include "ae.h"

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

static uint64_t startMsec = 0;
static struct timer_node * node_alloc(aeTimer *ti){
	if (ti->freeTimer.tail==NULL){
		uint32_t newNum = ti->usedNum + 64;	//每次增加64个
		struct timer_node **us = (struct timer_node **)realloc(ti->usedTimer, sizeof(struct timer_node *)*newNum);
		assert(us);
		for (int i = ti->usedNum; i < newNum; ++i){
			struct timer_node *node = (struct timer_node *)malloc(sizeof(struct timer_node));
			memset(node, 0, sizeof(*node));
			node->id = i + 1;
			us[i]= node;

			// 插入到freeTimer
			link_node(&ti->freeTimer, node);
		}
		ti->usedTimer = us;
		ti->usedNum = newNum;
	}

	// 尝试从freeTimer里面弹出一个
	struct timer_node * pNode = NULL;
	pNode = ti->freeTimer.head;
	if (ti->freeTimer.tail == ti->freeTimer.head){
		// 最后一个
		ti->freeTimer.tail = ti->freeTimer.head = NULL;
	}else{
		ti->freeTimer.head = pNode->next;
	}

	// 节点重置
	assert(pNode);
	pNode->pre = NULL;
	pNode->next = NULL;
	return pNode;
}

static void add_node(aeTimer *ti, struct timer_node *node){
	uint32_t expTick = node->expire; 	// 到期tick
	uint32_t curTick = ti->tick;		// 当前tick

	if ((expTick|255)==(curTick|255)) { // 检查expTick是否在curTick所在的桶内
		uint32_t idx = expTick%256; 	// 等同于 expTick&255
		node->level = -1;
		node->index = idx;
		node->isuse = 1;
		link_node(&ti->near[idx],node);
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
		link_node(&ti->tv[i][idx],node);
	}
}

// 俗称 cascades
static void move_list(aeTimer *ti, uint32_t level, uint32_t index){
	assert(level>=0 && level<4);
	assert(index>=0 && index<64);
	struct timer_node *headNode = list_clear(&(ti->tv[level][index]));
	while(headNode!=NULL){
		struct timer_node *temp = headNode;
		headNode = headNode->next;
		add_node(ti, temp);
	}
}

static void timer_shift(aeTimer *ti){
	uint32_t curTick = ++ti->tick;
	if (curTick==0){
		//溢出，即整个时间轮已经转完一轮，开始进入下一轮
		//因为 add_timer 添加的已经溢出的timer都放在了tv[3][0]中，
		//所以，当一整轮转完后，在进入下一个轮时，要把溢出的timer再重新添加进去
		move_list(ti, 3, 0);
	}else{
		uint32_t mask = 256;
		uint32_t temp = curTick>>8;
		int i = 0;

		// 每 TIME_NEAR 次循环，执行一次重建事件层级的逻辑
		// 每 TIME_LEVEL 次，执行一次下一级事件重建逻辑
		while((curTick%mask)==0){ //是不是跑完了一轮(256小轮 64大轮)
			int idx = temp%64; // 如果idx==0，则表示一个大轮也已经跑完，需要调到更上一层的大轮，把上一层大轮内的timer散开
			if (idx!=0){
				move_list(ti, i, idx);
				break;
			}
			mask <<= 6;
			temp >>= 6;
			++i;
		}
	}
}

static void timer_exec(aeTimer *ti){
	int idx = ti->tick & 255;
	struct timer_node *pHead = list_clear(&ti->near[idx]);
	while(pHead){
		// 派发事件
		//printf("timer id = %d, starttime = %lld, curTime = %llu, diff= %d \n", pHead->id, startMsec, ti->currentMs, ti->currentMs-startMsec);
		if (pHead->cb){
			pHead->cb(pHead->id, pHead->clientData);
		}
		struct timer_node *pTemp = pHead;
		pHead = pHead->next;

		pTemp->next = NULL;
		pTemp->pre = NULL;
		pTemp->isuse = 0;
		link_node(&ti->freeTimer, pTemp);
	}
}


/*----------------------------------- API -----------------------------------*/
int aeCreateTimer(aeEventLoop * eventLoop){
	struct aeTimer *ti = (struct timer *)malloc(sizeof(struct aeTimer));
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
    ti->starttime = (uint32_t)tv.tv_sec;
    uint64_t ms = tv.tv_sec * 1000 + tv.tv_usec / 1000;
    ti->currentMs = ms;
    startMsec = ms;

    eventLoop->timer = ti;
    return 0;
}

void aeDestroyTimer(aeEventLoop * eventLoop){
	aeTimer * ti = eventLoop->timer;
	int i;
	for (i = 0; i < 256; ++i){
		list_clear(&ti->near[i]);
	}

	for (i = 0; i < 4; ++i){
		for (int j = 0; j < 64; ++j){
			list_clear(&ti->tv[i][j]);
		}
	}
	list_clear(&ti->freeTimer);

	// free
	for (int i = 0; i < ti->usedNum; ++i){
		free(ti->usedTimer[i]);
	}
	free(ti);
	eventLoop->timer = NULL;
}

uint32_t aeAddTimer(aeEventLoop * eventLoop, uint32_t tickNum, aeTimeProc *proc, void *clientData){
	aeTimer *ti = eventLoop->timer;
	assert(ti);
	assert(tickNum>0);

	struct timer_node *node = node_alloc(ti);
	node->clientData = clientData;
	node->cb = proc;
	assert(node);
	node->next = NULL;
	node->expire = ti->tick + tickNum; //tickNum个滴答后到期
	add_node(ti, node);
	return node->id;
}

void aeDelTimer(aeEventLoop * eventLoop, uint32_t id){
	aeTimer *ti = eventLoop->timer;
	assert(id>0);
	assert(id<=ti->usedNum);
	struct timer_node *pNode = ti->usedTimer[id-1];
	if (pNode->isuse>0){
		int32_t level = pNode->level;
		int32_t index = pNode->index;
		struct timer_list *pTmList;
		if (level<0){
			pTmList = &ti->near[index];
		}else{
			pTmList = &ti->tv[level][index];
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
		link_node(&ti->freeTimer, pNode);
	}
}

int32_t aeTimerUpdatetime(aeEventLoop * eventLoop) {
	aeTimer *ti = eventLoop->timer;
	uint64_t cp = gettime();
	int64_t diff = (int64_t)(cp - ti->currentMs);
	if(diff<0) {
		printf("fuck cp = %llu\n",cp);
		ti->currentMs = cp - 10;
		return 0;
	} else if (diff>=10) {
		int i;
		for (i=0;i<(int)diff/10;i++) {
			ti->currentMs += 10;
			timer_shift(ti);
			timer_exec(ti);
		}

		uint64_t nextMs = ti->currentMs + 10;
		int32_t sleepMs = nextMs - gettime();
		sleepMs = sleepMs>0 ? sleepMs : 0;
		return sleepMs;
	}
	return 10;
}

