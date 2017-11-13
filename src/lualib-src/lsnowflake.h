#ifndef _LSNOWFLAKE_H_
#define _LSNOWFLAKE_H_

#include <stdint.h>

#define MAX_INDEX_VAL       0x0fff
#define MAX_WORKID_VAL      0x03ff
#define MAX_TIMESTAMP_VAL   0x01ffffffffff

int snowflakeInit(uint16_t work_id);
uint64_t snowflakeNextId();

#endif