
print("c call lua")

function onTimer()
	local tickNum = 1--math.random(1,5)
	ltimewheel.addTimer(tickNum*100)
	print("exp = ", tickNum, luuid.uuid())
	return 1
end

for i=1,10 do
	print(luuid.uuid())
end

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