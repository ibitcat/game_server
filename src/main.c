#include "app.h"

/*
uuid 完成
snowflake 完成
aoi
a星寻路
双数组字典树屏蔽字
postgre sql 驱动
地图
定时器（时间轮） 完成
*/

appServer app;
int main(int argc, char** argv)
{
	extern appServer app;
	initApp(&app);
	runApp(&app);
	return 0;
}
