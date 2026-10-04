-- Log main CPU accesses to [LO,HI] from time FROM, at most MAXLOG lines.
local m = manager.machine
local cpu = m.devices[":maincpu"]
local sp = cpu.spaces["program"]
local from, max = tonumber(os.getenv("FROM") or "0"), tonumber(os.getenv("MAXLOG") or "3000")
local n, last, rep = 0, nil, 0
local function log(kind, off, d, mask)
	if m.time:as_double() < from or n >= max then return end
	local s = string.format("%s %06X %04X/%04X pc=%06X", kind, off, d & 0xffff, mask & 0xffff, cpu.state["PC"].value)
	if s == last then rep = rep + 1; return end
	if rep > 0 then print(string.format("   (x%d)", rep)); rep = 0 end
	last = s; n = n + 1
	print(string.format("%.6f %s", m.time:as_double(), s))
end
IR = sp:install_read_tap(tonumber(os.getenv("LO")), tonumber(os.getenv("HI")), "ior", function(o, d, mask) log("R", o, d, mask) end)
IW = sp:install_write_tap(tonumber(os.getenv("LO")), tonumber(os.getenv("HI")), "iow", function(o, d, mask) log("W", o, d, mask) end)
if os.getenv("LO2") then
	IR2 = sp:install_read_tap(tonumber(os.getenv("LO2")), tonumber(os.getenv("HI2")), "ior2", function(o, d, mask) log("R", o, d, mask) end)
	IW2 = sp:install_write_tap(tonumber(os.getenv("LO2")), tonumber(os.getenv("HI2")), "iow2", function(o, d, mask) log("W", o, d, mask) end)
end
