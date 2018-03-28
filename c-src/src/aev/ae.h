// ae,参考redis网络库

/*
事件触发后，并不立即处理，而是先做标记
然后，在每一个逻辑帧内，先处理timer，然后再处理网络消息

每个逻辑帧以10ms为单位，注意的是需要控制每一帧都是走10ms

假设：
t1 = timer时间 + 网络处理时间，
t2 = epoll阻塞时间
也即是 t = t1 + t2 = 10ms

那么将有几种情况：
1、t1<10ms,则t2=10-t1
2、t1=10ms,则t2=0,也即是说epoll立即范围，什么也不干，等到下一帧再处理
3、20ms>t1>10ms,则t2<0,也就表示这一帧花费太多时间，那么在下一帧的时候要在20-t1内处理完
4、t1>=20ms,那么下一帧就要连续处理2帧，来弥补这一帧耗费的过长的时间

这样做是为了，保证timer一定是以10ms为单位往前推进，确保定时器的精确

while{
	// timer
	// 网络
	// epoll 等待10ms
}

可以优化的地方：
1、events 不用fd作为key


*/


#ifndef __AE_H__
#define __AE_H__

#include <time.h>
#include <stdint.h>

#define AE_OK 0
#define AE_ERR -1

#define AE_NONE 0
#define AE_READABLE 1
#define AE_WRITABLE 2
#define AE_ERROR 4

#define AE_EPMAX 64

struct aeEventLoop;

// Types
typedef int aeTimeProc(uint32_t id, void *clientData);

// ev callback
typedef int aeA_func(void *ud);
typedef int aeR_func(void *ud);
typedef int aeW_func(void *ud);
typedef int aeE_func(void *ud);

// time wheel
struct timer_node {
	uint32_t expire;                // 到期时间
	uint32_t id;                    // 定时器id
	int32_t level;                  // -1表示在near内
	int32_t index;
	uint32_t isuse;                 // 1表示在使用中的节点
	aeTimeProc *cb;
	void *clientData;
	struct timer_node *pre;
	struct timer_node *next;
};

struct timer_list{
	struct timer_node *head;
	struct timer_node *tail;
};

typedef struct aeTimer{
	struct timer_list near[256];
	struct timer_list tv[4][64];
	uint32_t tick;                  // 经过的滴答数,2^32个滴答
	uint32_t starttime;             // 定时器启动时的时间戳（秒）
	uint64_t currentMs;             // 当前毫秒数
	uint32_t usedNum;               // 定时器工厂大小
	struct timer_node **usedTimer;  // 定时器工厂（所有的定时器节点都是从这里产生的）
	struct timer_list freeTimer;    // 回收的定时器
} aeTimer;

// epoll event
typedef struct epEvent{
	void *ud;       // 指向aeEvent的指针
	uint8_t mask; 	// 事件状态（bit位为1表示这个事件被触发）
}

// ae event
struct aeEventList;
typedef struct aeEvent{
	uint32_t id;
	int fd;
	void *ud;       //userdata,指向一个session
	uint8_t mask;	//这个ev上注册的事件类型
	uint8_t canr;
	uint8_t carw;
	uint8_t error;
	aeA_func *af;
	aeR_func *rf;
	aeW_func *wf;
	aeE_func *ef;
	struct aeEventList *list; // aeEvent当前所处的list
	struct aeEvent *pre;
	struct aeEvent *next;
};

// ae event list
typedef struct aeEventList{
	struct aeEvent *head;
	struct aeEvent *tail;
};

// ae event loop
typedef struct aeEventLoop {
	uint8_t stop;
	int setsize;
	time_t lastTime;
	aeEvent *events;			// setsize个event的数组
	aeTimer *timer;				// 时间轮(skynet)
	void *apidata;				// linux上用epoll
	struct aeEventList used;	// 使用中的aeEvent
	struct aeEventList free;	// 空闲中的aeEvent
	struct aeEventList recycl; 	// 需要回收的aeEvent
	epEvent epev[AE_EPMAX];		// epoll 事件
} aeEventLoop;

// EventLoop
aeEventLoop *aeCreateEventLoop(int setsize);
void aeDeleteEventLoop(aeEventLoop *eventLoop);
void aeStop(aeEventLoop *eventLoop);
int aeCreateEvent(aeEventLoop *eventLoop, int fd, int mask, aeFileProc *proc, void *clientData);
void aeDeleteEvent(aeEventLoop *eventLoop, int fd, int mask);
int aeProcessEvents(aeEventLoop *eventLoop);
void aeMain(aeEventLoop *eventLoop);
int aeGetSetSize(aeEventLoop *eventLoop);
int aeResizeSetSize(aeEventLoop *eventLoop, int setsize);

// Time Event
int aeCreateTimer(aeEventLoop * eventLoop);
void aeDestroyTimer(aeEventLoop * eventLoop);
uint32_t aeAddTimer(aeEventLoop * eventLoop, uint32_t tickNum, aeTimeProc *proc, void *clientData);
void aeDelTimer(aeEventLoop * eventLoop, uint32_t id);
int32_t aeTimerUpdatetime(aeEventLoop * eventLoop);

#endif
