
print("c call lua")

env.lc_addTimer(1*100)

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

local fd = env.lc_netListen("127.0.0.1",7777)
print("lua call listen, fd  = ", fd)

--c call lua
function cl_handleMsg(fd,cmd,fromType,fromId,toType,toId, msg)
	-- body
	print(fd,cmd,string.char(fromType),fromId,string.char(toType),toId, msg)

	-- 回写
	env.lc_netSendMsg(fd,"reply from g")
end