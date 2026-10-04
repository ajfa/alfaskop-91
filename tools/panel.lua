-- panel.lua - every value the A91 puts on PI/T port B (front panel) through a full boot to the 3270 session
local m = manager.machine
local cpu = m.devices[":maincpu"]
local sp = cpu.spaces["program"]
local out = io.open("panel.txt", "w")
local last
PB = sp:install_write_tap(0xff8812, 0xff8813, "pbdr", function(o, d, mask)
	local v = d & 0xff
	if v ~= last then
		last = v
		out:write(string.format("%9.4f PB=%02X pc=%06X\n", m.time:as_double(), v, cpu.state["PC"].value))
	end
end)
PBD = sp:install_write_tap(0xff8806, 0xff8807, "pbddr", function(o, d, mask)
	out:write(string.format("%9.4f PBDDR=%02X pc=%06X\n", m.time:as_double(), d & 0xff, cpu.state["PC"].value))
end)
PBC = sp:install_write_tap(0xff880e, 0xff880f, "pbcr", function(o, d, mask)
	out:write(string.format("%9.4f PBCR=%02X pc=%06X\n", m.time:as_double(), d & 0xff, cpu.state["PC"].value))
end)
local posted, done = false, false
P = emu.add_machine_frame_notifier(function()
	local t = m.time:as_double()
	if not posted and t >= 90 then posted = true; m.natkeyboard:post("PANEL TEST\r") end
	if not done and t >= 105 then
		done = true
		local tab = ""
		for a = 0x442, 0x45f do tab = tab .. string.format("%02X", sp:read_u8(a)) end
		out:write(string.format("end: 0442..045F=%s\n", tab))
		out:close()
		m:exit()
	end
end)
