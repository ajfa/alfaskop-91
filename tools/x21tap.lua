-- Log main CPU accesses to the expansion boards at FF8C00-FF8FFF.
local m = manager.machine
local cpu = m.devices[":maincpu"]
local sp = cpu.spaces["program"]
local n, max = 0, tonumber(os.getenv("MAXLOG") or "3000")
local last, rep = nil, 0
local function log(kind, off, d, mask)
	local s = string.format("%s %06X %04X/%04X pc=%06X", kind, off, d & 0xffff, mask & 0xffff, cpu.state["PC"].value)
	if s == last then rep = rep + 1; return end
	if rep > 0 then print(string.format("   (x%d)", rep)); rep = 0 end
	last = s
	n = n + 1
	if n <= max then print(string.format("%.6f %s", m.time:as_double(), s)) end
end
XR = sp:install_read_tap(0xff8c00, 0xff8fff, "x21r", function(o, d, mask) log("R", o, d, mask) end)
XW = sp:install_write_tap(0xff8c00, 0xff8fff, "x21w", function(o, d, mask) log("W", o, d, mask) end)
