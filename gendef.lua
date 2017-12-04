local f = io.open("include/uv.h", "rb")
local s = f:read "*a"
f:close()

local names = {}
for name in s:gmatch("\nUV_EXTERN%s+[%w _*]+%s+([%w_]+)%s*%(.-%);") do
	names[#names + 1] = name
end
table.sort(names)

f = io.open("uvjni.def", "wb")
f:write("EXPORTS\r\n")
f:write("Java_jane_test_net_Libuv_libuv_1loop_1create\r\n")
f:write("Java_jane_test_net_Libuv_libuv_1loop_1destroy\r\n")
f:write("Java_jane_test_net_Libuv_libuv_1loop_1buffer\r\n")
f:write("Java_jane_test_net_Libuv_libuv_1loop_1run\r\n")
f:write("Java_jane_test_net_Libuv_libuv_1tcp_1bind\r\n")
f:write("Java_jane_test_net_Libuv_libuv_1tcp_1connect\r\n")
f:write("Java_jane_test_net_Libuv_libuv_1tcp_1send\r\n")
f:write("Java_jane_test_net_Libuv_libuv_1tcp_1close\r\n")
for _, name in ipairs(names) do
	f:write(name, "\r\n")
end
f:close()

print "done!"
