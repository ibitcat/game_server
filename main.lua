
print("c call lua")

env.addTimer(1*100)

function onTimer()
	local tickNum = 1--math.random(1,5)
	--env.addTimer(tickNum*100)
	--print("exp = ", tickNum, luuid.uuid())
	return 1
end

-- 进程在启动完毕后，lua 层需要的操作有：
-- 发起连接（需要一个c调用）
-- 监听连接
-- 连接成功后，发起hello

-- for k,v in pairs(_ENV) do
-- 	print(k,v)
-- end

local fd = env.net_listen("127.0.0.1",7777)
print("lua call listen, fd  = ", fd)

function handleMsg(fd,cmd,fromType,fromId,toType,toId)
	-- body
	local test
end