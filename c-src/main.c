// main

#include "game-src/app.h"

/*
uuid 完成
snowflake 完成
时间轮 完成
eventloop 完成
aoi
a星寻路
双数组字典树屏蔽字
postgre sql 驱动
地图
*/

int main(int argc, char** argv)
{
	printf("argc = %d\n", argc);
	for (int i = 0; i < argc; ++i){
		printf("argv = %s\n", argv[i]);
	}
	printf("--------------> server start <--------------\n");
	if (createApp(argv[1], 1)==0){
		runApp();
	}
	return 0;
}
