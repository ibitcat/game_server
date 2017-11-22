
print("client……")

local fd = env.lc_netConnect("127.0.0.1",7777)
print("lua connect server, fd  = ", fd)
if fd>0 then
	env.lc_netSendMsg(fd,"client send msg")
end

--c call lua
function cl_handleMsg(fd,cmd,fromType,fromId,toType,toId)
	-- body
	print(fd,cmd,fromType,fromId,toType,toId)
end