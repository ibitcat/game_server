#define LUA_LIB

#include <time.h>
#include <sys/time.h>
#include "spinlock.h"
#include "lsnowflake.h"

#include <stdio.h>

typedef struct _t_ctx {
    int64_t last_timestamp;
    int32_t work_id;
    int16_t index;
} ctx_t;

static volatile int g_inited = 0;
static ctx_t g_ctx = { 0, 0, 0 };
static struct spinlock sync_policy;

static int64_t get_timestamp() {
    struct timeval tv;
    gettimeofday(&tv, 0);
    printf("sec = %lld\n", tv.tv_sec);
    printf("msec = %lld\n", tv.tv_usec / 1000);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static void wait_next_msec() {
    int64_t current_timestamp = 0;
    do {
        current_timestamp = get_timestamp();
    } while (g_ctx.last_timestamp >= current_timestamp);
    g_ctx.last_timestamp = current_timestamp;
    g_ctx.index = 0;
}

int snowflakeInit(uint16_t work_id) {
    spinlock_lock(&sync_policy);
    if (g_inited) {
        spinlock_unlock(&sync_policy);
        return 0;
    }
    g_ctx.work_id = work_id;
    g_ctx.index = 0;
    g_inited = 1;
    spinlock_unlock(&sync_policy);
    return 0;
}

uint64_t snowflakeNextId() {
    spinlock_lock(&sync_policy);
    if (!g_inited) {
        spinlock_unlock(&sync_policy);
        return -1;
    }
    int64_t current_timestamp = get_timestamp();
    if (current_timestamp == g_ctx.last_timestamp) {
        if (g_ctx.index < MAX_INDEX_VAL) {
            ++g_ctx.index;
        } else {
            printf("%d ,超过了\n", MAX_INDEX_VAL);
            wait_next_msec();
        }
    } else {
        g_ctx.last_timestamp = current_timestamp;
        g_ctx.index = 0;
    }
    int64_t nextid = (int64_t)(
        ((g_ctx.last_timestamp & MAX_TIMESTAMP_VAL) << 22) | 
        ((g_ctx.work_id & MAX_WORKID_VAL) << 12) | 
        (g_ctx.index & MAX_INDEX_VAL)
    );
    spinlock_unlock(&sync_policy);
    return nextid;
}