
print("c call lua")

for i=1,10 do
	print(luuid.uuid())
end

lsnowflake.init(7)
for i=1,4096 do
	local id = lsnowflake.next_id()
	if i==1 or i==4096 then
		print(i, id)
	end
end

print(lsnowflake.next_id())
