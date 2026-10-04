-- tso-logon.lua - log the DU 4110 on to TSO as HERC01 and run LISTCAT, the way a person would:
-- LOGON at the VTAM screen; RESET and PF3 when the keyboard locks on TSO's Read Partition Query;
-- user and password; ENTER at the welcome banner; PA1 at the fortune cookie, so that the logon
-- CLIST does not start TK5's colour ISPF menu. Screens go to tso.txt, snapshots to the snapshot directory.
local m = manager.machine
local dmem = m.devices[":ducpu"].spaces["program"]
local out = io.open("tso.txt", "w")
local function log(s) out:write(s, "\n"); out:flush() end
local function key(n)
	local port = m.ioport.ports[":du_kbd:P1" .. (n // 16)]
	for _, f in pairs(port.fields) do
		if f.mask == (1 << (n % 16)) then return f end
	end
	error("no key " .. n)
end
local reset, pf3, pa1 = key(120), key(62), key(6)
local function screen()
	local lines = {}
	for r = 0, 24 do
		local l = ""
		for i = 0, 79 do
			local c = dmem:read_u8(0x7800 + r * 80 + i) & 0x7f
			l = l .. ((c >= 0x20 and c < 0x7f) and string.char(c) or " ")
		end
		lines[#lines + 1] = (l:gsub("%s+$", ""))
	end
	return table.concat(lines, "\n"), lines[25] or ""
end
local presses = {}
local function press(f, at) presses[#presses + 1] = { f, at, at + 0.15 } end
local state, t_state, last, nextcheck = "wait", 0, "", 60
local shot = {}
local function snapshot(name)
	if not shot[name] then shot[name] = true; m.video:snapshot(); log("== snapshot " .. name) end
end
T = emu.add_machine_frame_notifier(function()
	local t = m.time:as_double()
	for _, p in ipairs(presses) do
		if not p.down and t >= p[2] then p[1]:set_value(1); p.down = true end
		if p.down and not p.up and t >= p[3] then p[1]:clear_value(); p.up = true end
	end
	if t < nextcheck then return end
	nextcheck = t + 0.5
	local s, status = screen()
	if s ~= last then log(string.format("== %.1f screen\n%s", t, s)); last = s end
	local function go(new) log(string.format("== %.1f %s -> %s", t, state, new)); state = new; t_state = t end
	local locked = status:find("X", 30, true) ~= nil
	if state == "wait" and t >= 90 and s:find("SYS OP", 1, true) then
		m.natkeyboard:post("LOGON APPLID(TSO) LOGMODE(MHP3278E)\r"); go("logon")
	elseif state == "logon" and s:find("MY JOB", 1, true) then
		go("bound")
	elseif state == "bound" and t >= t_state + 8 then
		if locked then press(reset, t) end
		go("pf3")
	elseif state == "pf3" and t >= t_state + 4 then
		if s:find("USERID", 1, true) then m.natkeyboard:post("HERC01\r"); go("userid")
		elseif locked then press(reset, t); t_state = t
		else press(pf3, t); t_state = t + 8 end
	elseif state == "userid" and t >= t_state + 3 and s:find("PASSWORD", 1, true) then
		m.natkeyboard:post("CUL8TR\r"); go("password")
	elseif state == "password" and t >= t_state + 3 then
		if s:find("Welcome", 1, true) then snapshot("welcome") end
		if s:find("READY", 1, true) then
			go("ready")
		elseif s:find("***", 1, true) and not locked and t >= t_state + 4 then
			if s:find("fortune", 1, true) or s:find("IKT00405I", 1, true) then press(pa1, t); go("attn")
			else m.natkeyboard:post("\r"); t_state = t end
		elseif locked and t >= t_state + 10 then
			press(reset, t); t_state = t
		end
	elseif state == "attn" and t >= t_state + 4 then
		if locked then press(reset, t); t_state = t
		elseif s:find("READY", 1, true) then go("ready") end
	elseif state == "ready" and t >= t_state + 2 then
		m.natkeyboard:post("LISTCAT\r"); go("listcat")
	elseif state == "listcat" and t >= t_state + 10 then
		snapshot("listcat"); go("done")
		log("TSO OK")
	end
end)
