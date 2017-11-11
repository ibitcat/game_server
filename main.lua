
print("c call lua")

env.addTimer(1*100)

function onTimer()
	local tickNum = 1--math.random(1,5)
	env.addTimer(tickNum*100)
	--print("exp = ", tickNum, luuid.uuid())
	return 1
end

-- for i=1,10 do
-- 	print(luuid.uuid())
-- end

-- lsnowflake.init(7)
-- for i=1,4096 do
-- 	local id = lsnowflake.next_id()
-- 	if i==1 or i==4096 then
-- 		print(i, id)
-- 	end
-- end

-- print(lsnowflake.next_id())

-- for i=1,10 do
-- 	local timerId = ltimewheel.addTimer(i*100)
-- 	print("timerid = ", timerId)
-- end

-- 进程在启动完毕后，lua 层需要的操作有：
-- 发起连接（需要一个c调用）
-- 监听连接
-- 连接成功后，发起hello

-- for k,v in pairs(_ENV) do
-- 	print(k,v)
-- end

--lenv.net_listen("127.0.0.1",7777)