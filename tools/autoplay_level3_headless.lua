-- Headless Level 3 (trampolines) reference capture for circusc4:
--   mame circusc4 -video none -sound none -nothrottle -seconds_to_run 400 \
--     -autoboot_script tools/autoplay_level3_headless.lua
-- The script inserts a coin at frame 300, presses START at 400 and forces
-- $2201 = 2 while the game initialises so the first stage is Level 3.  It
-- then drives the joystick and writes one row per emulated frame with the
-- Charlie, scroll, performer, projectile, bag, presentation and score
-- state that tools/compare_level3_replay.py checks against the native
-- --replay-event 3 output.
--
-- Environment:
--   CIRCUS_L3_MODE     right   hold RIGHT from the first stage frame (default)
--                      bags    hold RIGHT but stop under every hanging bag
--                      script  read CIRCUS_L3_SCRIPT lines "from,to,mask"
--                              (stage-relative frames, mask 1 LEFT 2 RIGHT)
--   CIRCUS_L3_NODEATH  1       neutralise the $8B93 hit writes with a memory
--                              tap (projectiles keep flying, Charlie survives)
--   CIRCUS_L3_CLEAR    1       wipe the projectile records every frame
--   CIRCUS_L3_OUT      path    output CSV (default /tmp/circusc4-level3.csv)
--   CIRCUS_L3_LAST     n       stop after n emulated frames (default 3400)
--   CIRCUS_L3_L2       n       $220F (Level 2 visit count) poked after the
--                              start so the coin shower scores 100 (default 1)
--   CIRCUS_L3_BONUS    dddd    poke the bonus digits $227C-$227F on the first
--                              stage frame (e.g. 0050 to reach the time-out)
local machine = manager.machine
local maincpu = assert(machine.devices[":maincpu"])
local program = assert(maincpu.spaces["program"])
local system_port = assert(machine.ioport.ports[":SYSTEM"])
local player_port = assert(machine.ioport.ports[":P1"])
local mode = os.getenv("CIRCUS_L3_MODE") or "right"
local nodeath = os.getenv("CIRCUS_L3_NODEATH") == "1"
local clear = os.getenv("CIRCUS_L3_CLEAR") == "1"
local out_path = os.getenv("CIRCUS_L3_OUT") or "/tmp/circusc4-level3.csv"
local last_frame = tonumber(os.getenv("CIRCUS_L3_LAST") or "3400")
local level2_visits = tonumber(os.getenv("CIRCUS_L3_L2") or "1")
local bonus_poke = os.getenv("CIRCUS_L3_BONUS")
local script = {}
do
  local path = os.getenv("CIRCUS_L3_SCRIPT")
  if path then
    for line in io.lines(path) do
      local a, b, m = line:match("^%s*(%d+)%s*,%s*(%d+)%s*,%s*(%d+)")
      if a then script[#script + 1] = {tonumber(a), tonumber(b), tonumber(m)} end
    end
  end
end

local function u8(a) return program:read_u8(a) end
local function u16(a) return u8(a) * 256 + u8(a + 1) end

-- $8B93-$8BA8 clears Charlie, sets state 7 and freezes the projectile.  With
-- the tap every one of those writes keeps its old value.
if nodeath then
  nodeath_tap = program:install_write_tap(0x2400, 0x25ff, "nodeath", function(offset, data, mask)
    local pc = maincpu.state["PC"].value
    if pc >= 0x8b93 and pc <= 0x8ba8 then
      return program:read_u8(offset)
    end
    return data
  end)
end

local coin = system_port:field(0x01)
local start = system_port:field(0x08)
local left = player_port:field(0x01)
local right = player_port:field(0x02)
local frame = 0
local stage_start = nil
local poked = false
local function set(field, pressed) if pressed then field:set_value(1) else field:clear_value() end end
local function between(a, b) return frame >= a and frame < b end

local out = assert(io.open(out_path, "w"))
local header = {"frame", "rel", "input", "state", "y", "x", "phase", "vel", "target", "bnc",
  "dir", "stick", "scroll", "score", "missed", "bagidx", "bagtot", "tile"}
for i = 0, 3 do
  for _, n in ipairs({"active", "x", "timer", "rem", "type"}) do header[#header + 1] = "perf" .. i .. "_" .. n end
end
for i = 0, 2 do
  for _, n in ipairs({"state", "y", "x", "vel", "apex", "hold"}) do header[#header + 1] = "flame" .. i .. "_" .. n end
end
for i = 0, 3 do
  for _, n in ipairs({"state", "y", "x", "vel", "apex", "sway", "hold"}) do header[#header + 1] = "knife" .. i .. "_" .. n end
end
for i = 0, 2 do
  for _, n in ipairs({"state", "x", "key", "timer"}) do header[#header + 1] = "bag" .. i .. "_" .. n end
end
header[#header + 1] = "bird_state"; header[#header + 1] = "bird_x"; header[#header + 1] = "bird_bagx"
for i = 0, 12 do header[#header + 1] = "coin" .. i .. "_y"; header[#header + 1] = "coin" .. i .. "_x" end
for _, n in ipairs({"coinidx", "coincount", "bonus", "lives", "phase05", "phase06", "frame_byte"}) do header[#header + 1] = n end
out:write(table.concat(header, ",") .. "\n")

local function row(rel)
  local v = {}
  local function push(x) v[#v + 1] = tostring(x) end
  push(frame); push(rel); push(string.format("%02x", player_port:read()))
  push(u8(0x2402)); push(u8(0x2404)); push(u8(0x2406)); push(u8(0x2407))
  push(u16(0x2417)); push(u8(0x2408) * 256 + u8(0x243a)); push(u8(0x2437))
  push(u8(0x240a)); push(u8(0x241b)); push(u16(0x2203))
  push(tonumber(string.format("%02x%02x%02x", u8(0x20a0), u8(0x20a1), u8(0x20a2))))
  push(u8(0x220a)); push(u8(0x28f1)); push(u8(0x28f2)); push(u8(0x28f4))
  for _, a in ipairs({0x2440, 0x2480, 0x24c0, 0x2500}) do
    if u8(a) == 0 then push(0); push(0); push(0); push(0); push(0)
    else push(1); push(u8(a + 6)); push(u8(a + 0x39)); push(u8(a + 0x38)); push(u8(a + 0x3c)) end
  end
  for _, a in ipairs({0x2580, 0x25a0, 0x25c0}) do
    if u8(a) == 0 then push(-1); push(0); push(0); push(0); push(0); push(0)
    else push(u8(a + 2)); push(u8(a + 4)); push(u8(a + 6)); push(u16(a + 7)); push(u8(a + 9)); push(u8(a + 3)) end
  end
  for _, a in ipairs({0x2540, 0x2550, 0x2560, 0x2570}) do
    if u8(a) == 0 then push(-1); push(0); push(0); push(0); push(0); push(0); push(0)
    else push(u8(a + 2)); push(u8(a + 4)); push(u8(a + 6)); push(u16(a + 7)); push(u8(a + 9)); push(u8(a + 0xa)); push(u8(a + 3)) end
  end
  for _, a in ipairs({0x2690, 0x26a0, 0x26b0}) do
    if u8(a) == 0 then push(-1); push(0); push(0); push(0)
    else push(u8(a + 2)); push(u8(a + 6)); push(u8(a + 9)); push(u8(a + 0xa)) end
  end
  if u8(0x2700) == 0 then push(-1); push(0); push(0) else push(u8(0x2702)); push(u8(0x2706)); push(u8(0x2726)) end
  for a = 0x2730, 0x27f0, 0x10 do
    if u8(a) == 0 then push(-1); push(0) else push(u8(a + 4)); push(u8(a + 6)) end
  end
  push(u8(0x28dc)); push(u8(0x28dd))
  push(u8(0x227c) * 1000 + u8(0x227d) * 100 + u8(0x227e) * 10 + u8(0x227f))
  push(u8(0x2200)); push(u8(0x2005)); push(u8(0x2006)); push(u8(0x2014))
  out:write(table.concat(v, ",") .. "\n")
end

emu.register_frame_done(function()
  frame = frame + 1
  set(coin, between(300, 306))
  set(start, between(400, 406))
  if frame >= 402 and frame <= 470 then program:write_u8(0x2201, 2) end
  if frame == 471 and not poked then
    -- As if Levels 1 and 2 had been played once: $220E/$220F visit counts
    -- and the Level 3 count that $6BD5 could not increment before the poke.
    program:write_u8(0x220e, 1); program:write_u8(0x220f, level2_visits); program:write_u8(0x2210, 1)
    poked = true
  end
  if stage_start == nil and frame > 470 and u8(0x2400) ~= 0 and u8(0x2402) == 1 and u8(0x2201) == 2 then
    stage_start = frame
    if bonus_poke and #bonus_poke == 4 then
      for digit = 1, 4 do
        program:write_u8(0x227b + digit, tonumber(bonus_poke:sub(digit, digit)))
      end
    end
  end
  local rel = stage_start and (frame - stage_start) or -1
  local l, r = false, false
  if mode == "right" then
    r = stage_start ~= nil
  elseif mode == "bags" and stage_start then
    r = true
    for a = 0x2690, 0x26b0, 0x10 do
      if u8(a) ~= 0 and u8(a + 2) == 3 then
        local bx = u8(a + 6)
        if bx >= 88 and bx <= 166 then r = false end
      end
    end
  elseif mode == "script" and stage_start then
    for _, s in ipairs(script) do
      if rel >= s[1] and rel < s[2] then
        l = l or (s[3] & 1) ~= 0; r = r or (s[3] & 2) ~= 0
      end
    end
  end
  set(left, l); set(right, r)
  if clear then
    for a = 0x2540, 0x25ff do program:write_u8(a, 0) end
  end
  row(rel)
  if frame >= last_frame then
    out:close()
    machine:exit()
  end
end, "frame")
