
print("c call lua")

-- 进程在启动完毕后，lua 层需要的操作有：
-- 发起连接（需要一个c调用）
-- 监听连接
-- 连接成功后，发起hello
-- 连接发起方，发起心跳包

-- for k,v in pairs(_ENV) do
-- 	print(k,v)
-- end

local fd = env.lc_netListen("127.0.0.1",7777)
print("lua call listen, fd  = ", fd)

local cfd
function onTimer()
	env.lc_addTimer(1*100)
	env.lc_netSendMsg(cfd,"reply from g")
end

--c call lua
function cl_onNetConnected(fd)
	-- body
end

function cl_onNetAccpeted(fd)
	-- body
end

function cl_onNetClosed(fd)
	-- body
end

function cl_onHandlePkt(fd,cmd,fromType,fromId,toType,toId, msg)
	-- body
	print(fd,cmd,string.char(fromType),fromId,string.char(toType),toId, msg)
	cfd = fd

	-- 回写
	env.lc_addTimer(1*100)
end