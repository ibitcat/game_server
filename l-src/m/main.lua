
print("client……")

local fd = env.lc_netConnect("127.0.0.1",7777)
print("lua connect server, fd  = ", fd)
if fd>0 then
	env.lc_netSendMsg(fd,"m --> g")
end

function cl_handleMsg()
	print("fuck c2s……")
end

--c call lua
function cl_onHandlePkt(fd,cmd,fromType,fromId,toType,toId, msg)
	-- body
	print(fd,cmd,string.char(fromType),fromId,string.char(toType),toId, msg)

	--env.lc_netSendMsg(fd,"hello from m")
end