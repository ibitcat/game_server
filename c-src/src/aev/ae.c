// ae

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <poll.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <sys/types.h>

#include "ae.h"

/* ======================= epoll api ======================= */
typedef struct aeApiState {
	int epfd;
	struct epoll_event *events; // epoll_wait需要的64个events
} aeApiState;

static int aeApiCreate(aeEventLoop *eventLoop) {
	aeApiState *state = malloc(sizeof(aeApiState));

	if (!state) return -1;
	state->events = malloc(sizeof(struct epoll_event)*AE_EPMAX);
	if (!state->events) {
		free(state);
		return -1;
	}

	// Linux 2.6.8之后，size参数不使用，内核会动态调整大小，但是size必须>0
	state->epfd = epoll_create(1024); /* 1024 is just a hint for the kernel */
	if (state->epfd == -1) {
		free(state->events);
		free(state);
		return -1;
	}
	eventLoop->apidata = state;
	return 0;
}

static int aeApiResize(aeEventLoop *eventLoop, int setsize) {
	aeApiState *state = eventLoop->apidata;
	state->events = realloc(state->events, sizeof(struct epoll_event)*setsize);
	return 0;
}

static void aeApiFree(aeEventLoop *eventLoop) {
	aeApiState *state = eventLoop->apidata;

	close(state->epfd);
	free(state->events);
	free(state);
}

static int aeApiAddEvent(aeEventLoop *eventLoop, int fd, int mask, void* ud) {
	aeApiState *state = eventLoop->apidata;
	struct epoll_event ee = {0};
	ee.events = 0;
	ee.data.ptr = ud; // data是一个union

	struct aeEvent *ae = (struct aeEvent *)ud;
	int op = ae->mask == AE_NONE ? EPOLL_CTL_ADD : EPOLL_CTL_MOD;
	if (mask & AE_READABLE) ee.events |= EPOLLIN;
	if (mask & AE_WRITABLE) ee.events |= EPOLLOUT;
	if (epoll_ctl(state->epfd,op,fd,&ee) == -1) return -1;
	return 0;
}

static void aeApiDelEvent(aeEventLoop *eventLoop, int fd, int delmask, void* ud) {
	aeApiState *state = eventLoop->apidata;
	struct epoll_event ee = {0}; /* avoid valgrind warning */
	ee.events = 0;
	ee.data.ptr = ud;

	int mask = ae.mask & (~delmask);
	if (delmask==0){
		mask = AE_NONE;
	}
	if (mask & AE_READABLE) ee.events |= EPOLLIN;
	if (mask & AE_WRITABLE) ee.events |= EPOLLOUT;
	if (mask != AE_NONE) {
		epoll_ctl(state->epfd,EPOLL_CTL_MOD,fd,&ee);
	} else {
		epoll_ctl(state->epfd,EPOLL_CTL_DEL,fd,&ee);
	}
}

static int aeApiPoll(aeEventLoop *eventLoop, int ms) {
	aeApiState *state = eventLoop->apidata;
	int retval, numevents = 0;

	retval = epoll_wait(state->epfd, state->events, AE_EPMAX, ms);
	if (retval > 0) {
		numevents = retval;
		for (int i = 0; i < numevents; i++) {
			int mask = 0;
			struct epoll_event *e = state->events+i;

			if (e->events & EPOLLIN) mask |= AE_READABLE;
			if (e->events & EPOLLOUT) mask |= AE_WRITABLE;
			if (e->events & EPOLLERR) mask |= AE_ERROR;
			if (e->events & EPOLLHUP) mask |= AE_ERROR;

			// 插入到eventloop中
			eventLoop->epev[i].mask = mask;
			eventLoop->epev[i].ud = e.data.ptr;
		}
	}
	return numevents;
}

/* ======================= event loop ======================= */
int aeEventDel(struct aeEventList * list, struct aeEvent *ae){
	if (ae!=NULL && (ae->pre || ae->next)){
		// TODO
	}
	return 0;
}

aeEvent* aeEventPop(struct aeEventList * list){
	struct aeEvent *ae = list->head;
	if (ae!=NULL){
		list->head = ae->next;
		if (list->head==NULL){
			list->tail = NULL;
		}
		ae->next = NULL;
	}
	return ae;
}

int aeEventPush(struct aeEventList * list, struct aeEvent *ae){
	assert(!ae);
	if (list->head==NULL){
		list->head = ae;
		list->tail = ae;
	}else{
		list->tail->next = ae;
		list->tail = ae;
	}
	return 0;
}

aeEventLoop *aeCreateEventLoop(int setsize) {
	aeEventLoop *eventLoop;

	if ((eventLoop = malloc(sizeof(*eventLoop))) == NULL) goto err;
	int byteSize = sizeof(aeEvent)*setsize;
	eventLoop->events = malloc(byteSize);
	memset(eventLoop->events, 0, byteSize);

	if (aeCreateTimer(eventLoop) == -1) goto err;
	if (aeApiCreate(eventLoop) == -1) goto err;

	memset(eventLoop->epev, 0, sizeof(eventLoop->epev));
	eventLoop->setsize = setsize;
	eventLoop->lastTime = time(NULL);
	eventLoop->stop = 0;

	eventLoop->used.head = NULL;
	eventLoop->used.tail = NULL;
	eventLoop->free.head = NULL;
	eventLoop->free.tail = NULL;
	eventLoop->used.recycl = NULL;
	eventLoop->used.recycl = NULL;

	struct aeEventList *freeList = &eventLoop->free;
	for (int i = 0; i < setsize; i++){
		eventLoop->events[i].id = i+1;
		aeEventPush(freeList, &eventLoop->events[i]);
	}
	return eventLoop;

err:
	if (eventLoop) {
		free(eventLoop->events);
		aeDestroyTimer(eventLoop->timer);
		free(eventLoop->timer);
		free(eventLoop);
	}
	return NULL;
}

int aeGetSetSize(aeEventLoop *eventLoop) {
	return eventLoop->setsize;
}

int aeResizeSetSize(aeEventLoop *eventLoop, int setsize) {
	int oldSize = eventLoop->setsize;
	if (setsize == oldSize) return AE_OK;
	if (oldSize >  setsize) return AE_ERR;
	//if (aeApiResize(eventLoop,setsize) == -1) return AE_ERR;

	eventLoop->events = realloc(eventLoop->events, sizeof(aeEvent)*setsize);
	eventLoop->setsize = setsize;
	struct aeEventList *freeList = &eventLoop->free;
	for (int i = oldSize; i < setsize; i++){
		eventLoop->events[i].id = i+1;
		aeEventPush(freeList, &eventLoop->events[i]);
	}
	return AE_OK;
}

void aeDeleteEventLoop(aeEventLoop *eventLoop) {
	aeApiFree(eventLoop);
	aeDestroyTimer(eventLoop);
	free(eventLoop->timer);
	free(eventLoop->events);
	free(eventLoop);
}

void aeStop(aeEventLoop *eventLoop) {
	eventLoop->stop = 1;
}

int aeCreateEvent(aeEventLoop *eventLoop, int fd, int mask, void *clientData) {
	struct aeEvent *ae = aeEventPop(&eventLoop->free);
	if (!ae) {
		errno = ERANGE;
		return AE_ERR;
	}

	if (aeApiAddEvent(eventLoop, fd, mask, ae) == -1) return AE_ERR;
	ae->ud = clientData;
	ae->mask |= mask;
	return AE_OK;
}

void aeDeleteEvent(aeEventLoop *eventLoop, int id, int mask) {
	if (id>eventLoop->setsize || id<=0){
		return;
	}

	struct aeEvent *ae = eventLoop->events[id-1];
	if (ae!=NULL && ae->fd>0){
		aeApiDelEvent(eventLoop, ae->fd, mask);

		// 从used中移除
		// 丢进垃圾箱
		aeEventPush(&eventLoop->recycl, ae);
	}
}

static void aeGetTime(long *seconds, long *milliseconds) {
	struct timeval tv;

	gettimeofday(&tv, NULL);
	*seconds = tv.tv_sec;
	*milliseconds = tv.tv_usec/1000;
}

int aeProcessEvents(aeEventLoop *eventLoop) {
	int processed = 0, numevents; // 这里表示声明两边int变量

	// timer
	eventLoop->lastTime = time(NULL);
	int ms = aeTimerUpdatetime(eventLoop);
	struct timeval tv;
	struct timeval *tvp;
	tv.tv_sec = ms/1000;
	tv.tv_usec = (ms % 1000)*1000;
	tvp = &tv;

	// 回收aeEvent
	// TODO

	// 网络消息处理
	// TODO

	// 下面这种，不太适合游戏服务器，游戏服务器最好保证每一个逻辑帧都要平衡
	numevents = aeApiPoll(eventLoop, tvp);
	for (int j = 0; j < numevents; j++) {
		aeFileEvent *fe = &eventLoop->events[eventLoop->fired[j].fd];
		int mask = eventLoop->fired[j].mask;
		int fd = eventLoop->fired[j].fd;
		int rfired = 0;

		/* note the fe->mask & mask & ... code: maybe an already processed
		 * event removed an element that fired and we still didn't
		 * processed, so we check if the event is still valid. */
		if (fe->mask & mask & AE_READABLE) {
			rfired = 1;
			fe->rfileProc(eventLoop,fd,fe->clientData,mask);
		}
		if (fe->mask & mask & AE_WRITABLE) {
			if (!rfired || fe->wfileProc != fe->rfileProc)
				fe->wfileProc(eventLoop,fd,fe->clientData,mask);
		}
		processed++;
	}
	return processed; /* return the number of processed file/time events */
}

void aeMain(aeEventLoop *eventLoop) {
	eventLoop->stop = 0;
	while (!eventLoop->stop) {
		if (eventLoop->beforesleep != NULL)
			eventLoop->beforesleep(eventLoop);
		aeProcessEvents(eventLoop);
	}
}