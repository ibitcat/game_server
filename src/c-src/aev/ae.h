#ifndef __AE_H__
#define __AE_H__

#include <time.h>
#include <stdint.h>

#define AE_OK 0
#define AE_ERR -1

#define AE_NONE 0
#define AE_READABLE 1
#define AE_WRITABLE 2

/* Macros */
#define AE_NOTUSED(V) ((void) V)

struct aeEventLoop;

/* Types and data structures */
typedef void aeFileProc(struct aeEventLoop *eventLoop, int fd, void *clientData, int mask);
typedef void aeBeforeSleepProc(struct aeEventLoop *eventLoop);
typedef int aeTimeProc(uint32_t id, void *clientData);

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

/* File event structure */
typedef struct aeFileEvent {
    int mask; /* one of AE_(READABLE|WRITABLE) */
    aeFileProc *rfileProc;
    aeFileProc *wfileProc;
    void *clientData;
} aeFileEvent;


/* A fired event */
typedef struct aeFiredEvent {
    int fd;
    int mask;
} aeFiredEvent;


/* State of an event based program */
typedef struct aeEventLoop {
    int maxfd;   /* highest file descriptor currently registered */
    int setsize; /* max number of file descriptors tracked */
    time_t lastTime;     /* Used to detect system clock skew */
    aeFileEvent *events; /* Registered events */
    aeFiredEvent *fired; /* Fired events */
    aeTimer *timer; /* Time wheel */
    int stop;
    void *apidata; /* This is used for polling API specific data */
    aeBeforeSleepProc *beforesleep;
} aeEventLoop;


/* Prototypes */
aeEventLoop *aeCreateEventLoop(int setsize);
void aeDeleteEventLoop(aeEventLoop *eventLoop);
void aeStop(aeEventLoop *eventLoop);

/* File Event */
int aeCreateFileEvent(aeEventLoop *eventLoop, int fd, int mask, aeFileProc *proc, void *clientData);
void aeDeleteFileEvent(aeEventLoop *eventLoop, int fd, int mask);
int aeGetFileEvents(aeEventLoop *eventLoop, int fd);

/* Process */
int aeProcessEvents(aeEventLoop *eventLoop);
void aeMain(aeEventLoop *eventLoop);
char *aeGetApiName(void);
void aeSetBeforeSleepProc(aeEventLoop *eventLoop, aeBeforeSleepProc *beforesleep);
int aeGetSetSize(aeEventLoop *eventLoop);
int aeResizeSetSize(aeEventLoop *eventLoop, int setsize);

/* Time Event */
int aeCreateTimer(aeEventLoop * eventLoop);
void aeDestroyTimer(aeEventLoop * eventLoop);
uint32_t aeAddTimer(aeEventLoop * eventLoop, uint32_t tickNum, aeTimeProc *proc, void *clientData);
void aeDelTimer(aeEventLoop * eventLoop, uint32_t id);
int32_t aeTimerUpdatetime(aeEventLoop * eventLoop);

#endif
