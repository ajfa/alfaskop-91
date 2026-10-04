-- gate.lua - type a phrase into the host session at AT and check the host's echo in DU video memory
local m = manager.machine
local at = 90
local phrase = "PACK GATE OK"
local dmem = m.devices[":ducpu"].spaces["program"]
local posted = false
local function screen_text()
	local s = ""
	for i = 0, 0x7ff do
		local c = dmem:read_u8(0x7800 + i) & 0x7f
		s = s .. ((c >= 0x20 and c < 0x7f) and string.char(c) or " ")
	end
	return s
end
GATE = emu.add_machine_frame_notifier(function()
	local t = m.time:as_double()
	if not posted and t >= at then
		posted = true
		m.natkeyboard:post(phrase .. "\r")
	elseif posted and t >= at + 10 then
		local s = screen_text()
		local ok = s:find("YOU TYPED: " .. phrase, 1, true) ~= nil
		m.video:snapshot()
		print(ok and "GATE PASS" or "GATE FAIL")
		local f = io.open("gate-result.txt", "w")
		if f then f:write(ok and "PASS\n" or "FAIL\n"); f:write(s:gsub("%s+", " "), "\n"); f:close() end
		m:exit()
		GATE = nil
	end
end)
