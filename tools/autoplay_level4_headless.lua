-- Headless Stage 4 (rolling balls) reference capture for circusc4:
--   mame circusc4 -video none -sound none -nothrottle -seconds_to_run 400 \
--     -autoboot_script tools/autoplay_level4_headless.lua
-- The script inserts a coin at frame 300, presses START at 400 and forces
-- $2201 = 3 while the game initialises so the first stage is the balls.  It
-- then drives the joystick and button and writes one row per emulated frame
-- with Charlie, his ball, the four rolling balls, the bonus popups, scroll,
-- score and every counter tools/compare_level4_replay.py checks against the
-- native --replay-event 4 output.
--
-- Environment:
--   CIRCUS_L4_MODE     auto    hold RIGHT and jump onto every ball that
--                              comes close enough (default)
--                      right   hold RIGHT and never jump
--                      script  read CIRCUS_L4_SCRIPT lines "from,to,mask"
--                              (stage-relative frames, mask 1 LEFT 2 RIGHT
--                              4 JUMP); the auto pilot is off
--                      autoscript  the auto pilot plus the script's LEFT and
--                              RIGHT bits (mask 8 = force no jump)
--   CIRCUS_L4_JUMPAT   n       auto mode: jump when the nearest ball reaches
--                              this column (default 173: a centred landing)
--   CIRCUS_L4_OUT      path    output CSV (default /tmp/circusc4-level4.csv)
--   CIRCUS_L4_LAST     n       stop after n emulated frames (default 4000)
--   CIRCUS_L4_VISITS   n       $2211 (Stage 4 visit count) poked after START
--   CIRCUS_L4_BONUS    dddd    poke the bonus digits $227C-$227F on the first
--                              stage frame (e.g. 0050 to reach the time-out)
--   CIRCUS_L4_SNAP     a,b,c   emulated frames at which to save a snapshot
--                              (needs -video soft or similar; ignored headless)
local machine = manager.machine
local maincpu = assert(machine.devices[":maincpu"])
local program = assert(maincpu.spaces["program"])
local system_port = assert(machine.ioport.ports[":SYSTEM"])
local player_port = assert(machine.ioport.ports[":P1"])
local mode = os.getenv("CIRCUS_L4_MODE") or "auto"
local jump_at = tonumber(os.getenv("CIRCUS_L4_JUMPAT") or "173")
local out_path = os.getenv("CIRCUS_L4_OUT") or "/tmp/circusc4-level4.csv"
local last_frame = tonumber(os.getenv("CIRCUS_L4_LAST") or "4000")
local visits = tonumber(os.getenv("CIRCUS_L4_VISITS") or "1")
local bonus_poke = os.getenv("CIRCUS_L4_BONUS")
local snaps = {}
do
  local list = os.getenv("CIRCUS_L4_SNAP")
  if list then for n in list:gmatch("%d+") do snaps[tonumber(n)] = true end end
end
local script = {}
do
  local path = os.getenv("CIRCUS_L4_SCRIPT")
  if path then
    for line in io.lines(path) do
      local a, b, m = line:match("^%s*(%d+)%s*,%s*(%d+)%s*,%s*(%d+)")
      if a then script[#script + 1] = {tonumber(a), tonumber(b), tonumber(m)} end
    end
  end
end

local function u8(a) return program:read_u8(a) end
local function u16(a) return u8(a) * 256 + u8(a + 1) end

local coin = system_port:field(0x01)
local start = system_port:field(0x08)
local left = player_port:field(0x01)
local right = player_port:field(0x02)
local button = player_port:field(0x10)
local frame = 0
local stage_start = nil
local poked = false
local function set(field, pressed) if pressed then field:set_value(1) else field:clear_value() end end
local function between(a, b) return frame >= a and frame < b end

local out = assert(io.open(out_path, "w"))
local header = {"frame", "rel", "input", "state", "y", "x", "phase", "landvel", "launch", "dir",
  "vel", "jump", "stick", "idle_hi", "idle_lo", "countdown",
  "ball_active", "ball_flag", "ball_state", "ball_y", "ball_x", "ball_half", "ball_align",
  "scroll", "score", "bonus", "lives",
  "buffered", "doubles", "pending", "falls", "landings", "held", "standing", "flash", "timeout", "visits"}
for i = 0, 3 do
  for _, n in ipairs({"active", "flag", "state", "y", "x", "half"}) do header[#header + 1] = "roll" .. i .. "_" .. n end
end
for i = 0, 11 do
  for _, n in ipairs({"state", "flag", "y", "x", "timer"}) do header[#header + 1] = "fx" .. i .. "_" .. n end
end
for _, n in ipairs({"plaque0_x", "plaque1_x", "plaque2_x", "phase05", "phase06", "frame_byte", "dips"}) do header[#header + 1] = n end
out:write(table.concat(header, ",") .. "\n")

local function row(rel)
  local v = {}
  local function push(x) v[#v + 1] = tostring(x) end
  push(frame); push(rel); push(string.format("%02x", player_port:read()))
  push(u8(0x2402)); push(u8(0x2404)); push(u8(0x2406)); push(u8(0x2407)); push(u8(0x2408)); push(u8(0x2409)); push(u8(0x240a))
  push(u16(0x2417)); push(u8(0x241a)); push(u8(0x241b)); push(u8(0x2427)); push(u8(0x2428)); push(u8(0x242d))
  push(u8(0x2440)); push(u8(0x2441)); push(u8(0x2442)); push(u8(0x2444)); push(u8(0x2446)); push(u8(0x2445)); push(u8(0x2469))
  push(u16(0x2203))
  push(tonumber(string.format("%02x%02x%02x", u8(0x20a0), u8(0x20a1), u8(0x20a2))))
  push(u8(0x227c) * 1000 + u8(0x227d) * 100 + u8(0x227e) * 10 + u8(0x227f))
  push(u8(0x2200))
  push(u8(0x28d3)); push(u8(0x28d4)); push(u8(0x28d5)); push(u8(0x28df)); push(u8(0x28ef)); push(u8(0x28f0)); push(u8(0x28fc)); push(u8(0x2888)); push(u8(0x2263)); push(u8(0x2211))
  for _, a in ipairs({0x2480, 0x24c0, 0x2500, 0x2540}) do
    push(u8(a)); push(u8(a + 1)); push(u8(a + 2)); push(u8(a + 4)); push(u8(a + 6)); push(u8(a + 5))
  end
  for a = 0x2600, 0x26b0, 0x10 do
    if u8(a) == 0 then push(-1); push(0); push(0); push(0); push(0)
    else push(u8(a + 2)); push(u8(a + 1)); push(u8(a + 4)); push(u8(a + 6)); push(u8(a + 0xa)) end
  end
  push(u8(0x26c6)); push(u8(0x26d6)); push(u8(0x26e6))
  push(u8(0x2005)); push(u8(0x2006)); push(u8(0x2014)); push(u8(0x202f))
  out:write(table.concat(v, ",") .. "\n")
end

-- Auto pilot.  A jump lasts 57 frames; with RIGHT held the world scrolls one
-- column per frame and a rolling ball closes at 1.5 columns per frame, so a
-- ball at column x now sits near x - 85 when Charlie lands.  $9A42 accepts a
-- landing when |ball + 16 - (Charlie + 24)| < 18, i.e. the ball between 71
-- and 105 with Charlie at $50, and $9B10 pays most when the ball is within a
-- column of Charlie, so the pilot aims at 173 (jump_at).
--
-- Balls that arrive in a tight train (gaps of 64 columns or less) have to be
-- taken alternately: land, jump over the next one onto the one behind it, and
-- so on, and the train must end on a landing.  The pilot therefore reads the
-- $F1B5 schedule the way $9D73 does, so it also knows about balls that have
-- not rolled in yet, and skips the first ball of a train with an even number
-- of balls.  A lone ball that is already too close to be landed on is handled
-- the way the arcade footage shows a human doing it: walk LEFT so it drifts
-- back out to a usable distance, then turn round and jump.  A ball inside 28
-- columns of the ridden ball ends the life ($97F1), so anything closer than
-- 114 forces a jump right away.  On the last page ($2203 == $F8) the scroll
-- has stopped and Charlie walks, so distances are taken from his own column;
-- once nothing is left ahead he jumps from column 120 onto the goal stand,
-- which $9BC0 accepts between $A0 and $CA.
local retreating = false
local function rom16(a) return u8(a) * 256 + u8(a + 1) end
local function difficulty_index()
  local a = 2
  if u8(0x2022) ~= 0 then a = (u8(0x202f) & 0x60) >> 4 end
  local b = u8(0x2211)
  if b ~= 0 then b = (b - 1) * 2 end
  a = a + b
  local landings = u8(0x28ef)
  for _, th in ipairs({0x14, 0x18, 0x1c, 0x20, 0x24, 0x28, 0x2c, 0x30}) do
    if landings >= th then a = a + 2 else break end
  end
  if a > 10 then a = 10 end
  return a
end
-- Virtual screen columns of the balls the schedule will still spawn while
-- RIGHT is held: 241 plus 1.5 columns per frame until the spawn.
local function scheduled_columns()
  local cols = {}
  local table_base = rom16(0xf1b5 + difficulty_index())
  local scroll = u16(0x2203)
  local page = (-(scroll >> 8)) & 0xff
  local low = scroll & 0xff
  for p = page, 8 do
    local list = rom16(table_base + 2 * p)
    local floor = 0x100
    for i = 0, 4 do
      local v = u8(list + i)
      if v == 0 then break end
      if v < floor then
        floor = v
        local s = (((-p) & 0xff) << 8) | v
        if p > page or v <= low then
          local frames = ((scroll - s) & 0xffff) + 1
          cols[#cols + 1] = 241 + 1.5 * frames
        end
      end
    end
  end
  return cols
end
local function auto_pilot()
  local l, r, j = false, true, false
  if u8(0x2402) ~= 1 or u8(0x2407) ~= 0 then return l, r, j end
  local charlie = u8(0x2406)
  local offset = 80 - charlie
  local balls = {}
  for _, a in ipairs({0x2480, 0x24c0, 0x2500, 0x2540}) do
    if u8(a) ~= 0 and u8(a + 2) <= 1 then
      local x = u8(a + 6) + offset
      if x > 0x60 then balls[#balls + 1] = x end
    end
  end
  for _, x in ipairs(scheduled_columns()) do balls[#balls + 1] = x + offset end
  table.sort(balls)
  if #balls == 0 then
    retreating = false
    if u8(0x2203) == 0xf8 and charlie >= 118 and charlie <= 122 then return false, true, true end
    return l, r, j
  end
  local nearest = balls[1]
  if nearest <= 113 then return false, true, true end
  local train = 1
  while train < #balls and balls[train + 1] - balls[train] <= 64 do train = train + 1 end
  local target = nearest
  if train % 2 == 0 then target = balls[2] end
  if retreating then
    if target >= jump_at + 6 then retreating = false else return true, false, false end
  end
  if target >= jump_at - 3 and target <= jump_at + 3 then return false, true, true end
  if target < jump_at - 3 and target >= 160 then return false, true, true end
  if target < 160 then retreating = true; return true, false, false end
  return l, r, j
end

emu.register_frame_done(function()
  frame = frame + 1
  set(coin, between(300, 306))
  set(start, between(400, 406))
  if frame >= 402 and frame <= 470 then program:write_u8(0x2201, 3) end
  if frame == 471 and not poked then
    -- As if Levels 1 to 3 had been played once: the visit counts that $6BD5
    -- could not increment before the poke.
    program:write_u8(0x220e, 1); program:write_u8(0x220f, 1); program:write_u8(0x2210, 1)
    program:write_u8(0x2211, visits)
    poked = true
  end
  if stage_start == nil and frame > 470 and u8(0x2400) ~= 0 and u8(0x2402) == 1 and u8(0x2201) == 3 then
    stage_start = frame
    if bonus_poke and #bonus_poke == 4 then
      for digit = 1, 4 do
        program:write_u8(0x227b + digit, tonumber(bonus_poke:sub(digit, digit)))
      end
    end
  end
  local rel = stage_start and (frame - stage_start) or -1
  local l, r, j = false, false, false
  if stage_start then
    if mode == "right" then
      r = true
    elseif mode == "auto" then
      l, r, j = auto_pilot()
    elseif mode == "script" or mode == "autoscript" then
      local nojump = false
      for _, s in ipairs(script) do
        if rel >= s[1] and rel < s[2] then
          l = l or (s[3] & 1) ~= 0; r = r or (s[3] & 2) ~= 0; j = j or (s[3] & 4) ~= 0
          nojump = nojump or (s[3] & 8) ~= 0
        end
      end
      if mode == "autoscript" and not nojump then
        local al, ar, aj = auto_pilot()
        if not l and not r then l, r = al, ar end
        j = j or aj
      end
    end
  end
  set(left, l); set(right, r); set(button, j)
  if snaps[frame] then machine.video:snapshot() end
  row(rel)
  if frame >= last_frame then
    out:close()
    machine:exit()
  end
end, "frame")
