-- Dump the DU 4110 screen as text whenever it changes, sampled every second,
-- and report where the three processors are.
local every = tonumber(os.getenv("EVERY") or "1")
local snapat = tonumber(os.getenv("SNAPAT") or "0")
local m = manager.machine
local du = m.devices[":ducpu"]
local dmem = du and du.spaces["program"]
local cpu68 = m.devices[":maincpu"]
local tcc = m.devices[":tcccpu"]

local function vram_offset()
	local best, score = 0, -1
	for off = 0, 79 do
		local n = 0
		for r = 0, 24 do
			local c = dmem:read_u8(0x7800 + ((off + r * 80 + 79) % 0x800)) & 0x7f
			if c == 0x20 or c == 0 then n = n + 1 end
		end
		if n > score then best, score = off, n end
	end
	return best
end

local function screen_text()
	local off = vram_offset()
	local lines = {}
	for r = 0, 24 do
		local line = ""
		for i = 0, 79 do
			local c = dmem:read_u8(0x7800 + ((off + r * 80 + i) % 0x800)) & 0x7f
			line = line .. ((c >= 0x20 and c < 0x7f) and string.char(c) or " ")
		end
		lines[#lines + 1] = (line:gsub("%s+$", ""))
	end
	return table.concat(lines, "\n")
end

local last, next_t, snapped = "", every, false
SCREENSUB = emu.add_machine_frame_notifier(function()
	local t = m.time:as_double()
	if t < next_t then return end
	next_t = t + every
	local s = dmem and screen_text() or ""
	print(string.format("[t=%6.1f] 68k PC=%06X  TCC PC=%04X%s", t,
		cpu68.state["PC"].value, tcc.state["PC"].value,
		du and string.format("  DU PC=%04X", du.state["PC"].value) or ""))
	if s ~= last and s:gsub("%s", "") ~= "" then
		print(".--- screen")
		for l in s:gmatch("[^\n]*") do if l ~= "" then print("|" .. l) end end
		print("'---")
		last = s
	end
	if snapat > 0 and t >= snapat and not snapped then
		snapped = true
		m.video:snapshot()
	end
	io.stdout:flush()
end)
