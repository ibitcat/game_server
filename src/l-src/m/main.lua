
print("client……")

local fd = env.lc_netConnect("127.0.0.1",7777)
print("lua connect server, fd  = ", fd)
if fd>0 then
	env.lc_netSendMsg(fd,"client send msg")
end

--c call lua
function cl_handleMsg(fd,cmd,fromType,fromId,toType,toId, msg)
	-- body
	print(fd,cmd,string.char(fromType),fromId,string.char(toType),toId, msg)

	--env.lc_netSendMsg(fd,"hello from m")
end