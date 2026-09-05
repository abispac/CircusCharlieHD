-- Headless Stage 2 (tightrope and monkeys) reference capture for circusc4:
--   mame circusc4 -video none -sound none -nothrottle -seconds_to_run 400 \
--     -autoboot_script tools/autoplay_level2_headless.lua
-- The script inserts a coin at frame 300, presses START at 400 and forces
-- $2201 = 1 while the game initialises so the first stage is the rope.  It
-- then drives the joystick and button and writes one row per emulated frame
-- with Charlie, the seven monkey records, the goal stand, the score popups,
-- the plaques, the bird and coins of the perfect clear, the scroll, the
-- score and every counter tools/compare_level2_replay.py checks against the
-- native --replay-event 2 output.
--
-- Environment:
--   CIRCUS_L2_MODE     auto    hold RIGHT and jump over every monkey that
--                              comes close enough, then take the goal stand
--                              (default)
--                      right   hold RIGHT and never jump
--                      idle    never touch the stick (Charlie falls asleep)
--                      script  read CIRCUS_L2_SCRIPT lines "from,to,mask"
--                              (stage-relative frames, mask 1 LEFT 2 RIGHT
--                              4 JUMP); the auto pilot is off
--                      autoscript  the auto pilot plus the script's bits
--                              (mask 8 = force no jump)
--                      inputs  replay the "input" column of a CSV given in
--                              CIRCUS_L2_INPUTS (a previous capture): drives
--                              the port from its logged bytes, so a search
--                              capture can be reproduced and snapshotted
--                      search  the auto pilot with save states: every death
--                              rewinds to the landing before the fatal jump
--                              and retries it with another timing, so the
--                              run that is written out reaches the goal
--   CIRCUS_L2_STAND    1       on the last page walk all the way to column
--                              $C0 before jumping, so the goal stand ($8015)
--                              appears and its 5000 points are collected
--   CIRCUS_L2_STILL    1       release the stick on the frame of every jump
--                              so Charlie jumps on the spot (direction 0):
--                              $8697 then never raises $220A and the perfect
--                              clear bird appears at the goal
--   CIRCUS_L2_PERFECT  1       neutralise the $869B write with a memory tap
--                              so $220A stays clear and the goal shows the
--                              perfect-clear bird and coins (the native
--                              replay does the same with --replay-perfect)
--   CIRCUS_L2_JUMPAT   n       auto mode: jump when the nearest monkey is
--                              this many frames from Charlie (default 8)
--   CIRCUS_L2_OUT      path    output CSV (default /tmp/circusc4-level2.csv)
--   CIRCUS_L2_LAST     n       stop after n emulated frames (default 4000)
--   CIRCUS_L2_VISITS   n       $220F (Stage 2 visit count) poked after START
--   CIRCUS_L2_BONUS    dddd    poke the bonus digits $227C-$227F on the first
--                              stage frame (e.g. 0050 to reach the time-out)
--   CIRCUS_L2_SNAP     a,b,c   stage-relative frames at which to save a
--                              snapshot (needs -video soft; ignored headless)
local machine = manager.machine
local maincpu = assert(machine.devices[":maincpu"])
local program = assert(maincpu.spaces["program"])
local system_port = assert(machine.ioport.ports[":SYSTEM"])
local player_port = assert(machine.ioport.ports[":P1"])
local mode = os.getenv("CIRCUS_L2_MODE") or "auto"
local jump_at = tonumber(os.getenv("CIRCUS_L2_JUMPAT") or "6")
local out_path = os.getenv("CIRCUS_L2_OUT") or "/tmp/circusc4-level2.csv"
local last_frame = tonumber(os.getenv("CIRCUS_L2_LAST") or "4000")
local visits = tonumber(os.getenv("CIRCUS_L2_VISITS") or "1")
local want_stand = os.getenv("CIRCUS_L2_STAND") == "1"
local want_still = os.getenv("CIRCUS_L2_STILL") == "1"
local want_perfect = os.getenv("CIRCUS_L2_PERFECT") == "1"
local input_rows = {}
if mode == "inputs" then
  local path = os.getenv("CIRCUS_L2_INPUTS")
  local header, icol, rcol
  for line in io.lines(path) do
    local cells = {}
    for c in (line .. ","):gmatch("([^,]*),") do cells[#cells + 1] = c end
    if not header then
      header = true
      for i, name in ipairs(cells) do
        if name == "input" then icol = i end
        if name == "rel" then rcol = i end
      end
    else
      local rel = tonumber(cells[rcol])
      if rel and rel >= 0 then input_rows[rel] = tonumber(cells[icol], 16) end
    end
  end
end
if want_perfect then
  perfect_tap = program:install_write_tap(0x220a, 0x220a, "perfect", function(offset, data, mask)
    local pc = maincpu.state["PC"].value
    if pc >= 0x8697 and pc <= 0x869e then return 0 end
    return data
  end)
end
local bonus_poke = os.getenv("CIRCUS_L2_BONUS")
local snaps = {}
do
  local list = os.getenv("CIRCUS_L2_SNAP")
  if list then for n in list:gmatch("%d+") do snaps[tonumber(n)] = true end end
end
local script = {}
do
  local path = os.getenv("CIRCUS_L2_SCRIPT")
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

local monkeys = {0x2440, 0x2480, 0x24c0, 0x2500, 0x2540, 0x2580, 0x25c0}

local out = assert(io.open(out_path, "w"))
local header = {"frame", "rel", "input", "state", "y", "x", "half", "phase", "landvel", "launch", "dir",
  "vel", "jump", "stick", "idle_hi", "idle_lo", "countdown", "code",
  "scroll", "halfstep", "score", "bonus", "lives",
  "sched", "schedptr", "buffered", "stand", "goalhit", "jumps", "held", "skip", "browns", "purples",
  "flash", "timeout", "missed", "leftbonus", "perfect", "visits"}
for i = 0, 6 do
  for _, n in ipairs({"active", "flag", "state", "y", "x", "half", "phase", "landvel", "vel", "timer", "kind", "owner", "code"}) do
    header[#header + 1] = "mk" .. i .. "_" .. n
  end
end
for _, n in ipairs({"stand_active", "stand_state", "stand_y", "stand_x", "stand_x2", "stand_x3"}) do header[#header + 1] = n end
for i = 0, 11 do
  for _, n in ipairs({"state", "flag", "y", "x", "timer"}) do header[#header + 1] = "fx" .. i .. "_" .. n end
end
for _, n in ipairs({"plaque0_x", "plaque1_x", "plaque2_x"}) do header[#header + 1] = n end
header[#header + 1] = "bird_state"; header[#header + 1] = "bird_x"; header[#header + 1] = "bird_bagx"
for i = 0, 12 do header[#header + 1] = "coin" .. i .. "_y"; header[#header + 1] = "coin" .. i .. "_x" end
for _, n in ipairs({"coinidx", "coincount", "phase05", "phase06", "frame_byte", "dips"}) do header[#header + 1] = n end
out:write(table.concat(header, ",") .. "\n")

local function owner_index(a)
  local p = u16(a + 0x31)
  for i, m in ipairs(monkeys) do if m == p then return i - 1 end end
  return -1
end

local function row(rel)
  local v = {}
  local function push(x) v[#v + 1] = tostring(x) end
  push(frame); push(rel); push(string.format("%02x", player_port:read()))
  push(u8(0x2402)); push(u8(0x2404)); push(u8(0x2406)); push(u8(0x2405)); push(u8(0x2407)); push(u8(0x2408)); push(u8(0x2409)); push(u8(0x240a))
  push(u16(0x2417)); push(u8(0x241a)); push(u8(0x241b)); push(u8(0x2427)); push(u8(0x2428)); push(u8(0x242d)); push(u8(0x240e))
  push(u16(0x2203)); push(u8(0x2205))
  push(tonumber(string.format("%02x%02x%02x", u8(0x20a0), u8(0x20a1), u8(0x20a2))))
  push(u8(0x227c) * 1000 + u8(0x227d) * 100 + u8(0x227e) * 10 + u8(0x227f))
  push(u8(0x2200))
  push(u8(0x288b)); push(string.format("%04x", u16(0x288d))); push(u8(0x28d3)); push(u8(0x28df)); push(u8(0x28ee)); push(u8(0x28ef)); push(u8(0x28f0)); push(u8(0x28f8)); push(u8(0x28f9)); push(u8(0x28fa))
  push(u8(0x2888)); push(u8(0x2263)); push(u8(0x220a)); push(u8(0x220b)); push(u8(0x220c)); push(u8(0x220f))
  for _, a in ipairs(monkeys) do
    if u8(a) == 0 and u8(a + 1) == 0 then
      for _ = 1, 13 do push(0) end
    else
      push(u8(a)); push(u8(a + 1)); push(u8(a + 2)); push(u8(a + 4)); push(u8(a + 6)); push(u8(a + 5)); push(u8(a + 7)); push(u8(a + 8))
      push(u16(a + 0x17)); push(u8(a + 0x1d)); push(u8(a + 0x30)); push(owner_index(a)); push(u8(a + 0xe))
    end
  end
  push(u8(0x2760)); push(u8(0x2762)); push(u8(0x2764)); push(u8(0x2766)); push(u8(0x2776)); push(u8(0x2786))
  for a = 0x2600, 0x26b0, 0x10 do
    if u8(a) == 0 then push(-1); push(0); push(0); push(0); push(0)
    else push(u8(a + 2)); push(u8(a + 1)); push(u8(a + 4)); push(u8(a + 6)); push(u8(a + 0xa)) end
  end
  push(u8(0x26c6)); push(u8(0x26d6)); push(u8(0x26e6))
  if u8(0x2700) == 0 then push(-1); push(0); push(0) else push(u8(0x2702)); push(u8(0x2706)); push(u8(0x2726)) end
  for a = 0x2730, 0x27f0, 0x10 do
    if u8(a) == 0 then push(-1); push(0) else push(u8(a + 4)); push(u8(a + 6)) end
  end
  push(u8(0x28dc)); push(u8(0x28dd))
  push(u8(0x2005)); push(u8(0x2006)); push(u8(0x2014)); push(u8(0x202f))
  return table.concat(v, ",") .. "\n"
end

-- Auto pilot.  Charlie walks at column $40 and a jump lasts 57 frames; the
-- $80C2 hit test needs a monkey within 13 columns of Charlie + 6 and within
-- 16 rows, so he is safe from the fifth frame of the jump to the fourth
-- before the landing.  A brown monkey closes at 1.25 columns per frame while
-- RIGHT is held and a purple one at 2.5, so the pilot converts every monkey
-- ahead into the number of frames before it reaches column 83 and jumps when
-- the nearest is jump_at frames away.  On the last page the scroll stops and
-- Charlie walks to column $C0 by himself; once nothing is left ahead he
-- stands there a moment and jumps onto the goal stand.
--
-- That simple rule dies now and then: a purple monkey leaps over a brown one
-- right under Charlie, or a second monkey arrives while he is coming down.
-- The search mode keeps a save state from every landing and, when Charlie
-- is hit, rewinds to the landing before the fatal jump and tries that jump
-- with the next entry of `candidates`: the frames-to-go at which to press
-- the button (larger is earlier) and, for the spots where no timing works
-- because a purple monkey leaps right over Charlie's column, a number of
-- frames spent walking LEFT first, which moves the leap ahead of him.  When
-- every entry of a jump fails, the jump before it is retimed.  The rows of
-- the abandoned attempts are dropped, so the CSV holds one continuous run.
local candidates = {{8, 0}, {5, 0}, {12, 0}, {2, 0}, {16, 0}, {20, 0}, {24, 0},
  {8, 40}, {5, 40}, {12, 40}, {28, 0}, {34, 0}, {40, 0}, {8, 80}, {12, 80},
  {0, 0}, {48, 0}, {2, 40}, {20, 40}, {2, 80}, {20, 80}, {8, 120}, {20, 120}}
local stand_wait = 0
local function nearest_ahead(standing)
  local charlie = u8(0x2406)
  local last_page = u8(0x2203) == 0xf8
  local nearest = nil
  for _, a in ipairs(monkeys) do
    if u8(a) ~= 0 then
      local st = u8(a + 2)
      local x = u8(a + 6)
      if x > charlie and (st == 0 or st == 8 or st == 9) then
        -- $8943: a brown monkey closes at 1.25 columns a frame, a purple one
        -- at 2.5; on the last page ($8965/$8931) they walk 1 and 1.5 and
        -- Charlie adds his own column a frame.  Standing still on the last
        -- page they close at 0.75 and 1.
        local purple = u8(a + 0x30) ~= 0
        local speed = purple and 2.5 or 1.25
        if standing then speed = purple and 1.5 or 0.75 end
        if last_page then
          if standing then speed = purple and 1 or 0.75 else speed = purple and 2.5 or 2 end
        end
        local t = (x - (charlie + 19)) / speed
        if nearest == nil or t < nearest then nearest = t end
      end
    end
  end
  return nearest, last_page, charlie
end
local function auto_pilot(threshold, retreat)
  local l, r, j = false, true, false
  if u8(0x2402) ~= 1 or u8(0x2407) ~= 0 then return l, r, j end
  if retreat and retreat > 0 then return true, false, false end
  local nearest, last_page, charlie = nearest_ahead(want_still)
  -- $8015 only fires on a frame spent walking against column $C0, so the
  -- first frames there are never a jump.
  if last_page and charlie >= 0xc0 then
    stand_wait = stand_wait + 1
    if stand_wait < 3 then return false, true, false end
  end
  if nearest ~= nil then
    if not (last_page and charlie >= 0xc0) then stand_wait = 0 end
    -- To reach the stand Charlie has to walk from column 100 to $C0 with
    -- nothing coming, and a jump from 100 or beyond already lands in the
    -- goal range, so before column 100 he stands still and lets the monkey
    -- come to him (a standing jump lands where it started).
    if want_stand and last_page and charlie < 100 then
      nearest = nearest_ahead(true)
      r = false
    end
    if nearest <= threshold then j = true end
    if want_stand and last_page and charlie >= 100 and charlie < 0xc0 then j = false end
    if want_still and j then r = false end
    return l, r, j
  end
  if last_page and charlie >= 0xc0 and stand_wait >= 12 then return false, not want_still, true end
  return l, r, j
end

-- Search bookkeeping: `choice[i]` indexes `candidates` for the i-th jump,
-- `checkpoint[i]` is the save state taken at the landing before it (or the
-- first stage frame), `rows[frame]` the CSV rows written so far.
local rows = {}
local choice = {}
local checkpoint = {}
local jumps = 0
local pending_load = nil
local was_airborne = false
local search = mode == "search"
local function save_checkpoint(index)
  local path = string.format("/tmp/circusc4-l2-cp%d", index)
  checkpoint[index] = {path = path, frame = frame, stage_start = stage_start, stand_wait = stand_wait,
                       input = {left = false, right = true, button = false}}
  machine:save(path)
end
local function finish()
  for f = 1, frame do if rows[f] then out:write(rows[f]) end end
  out:close()
  machine:exit()
end
local rewinds = 0
local max_rewinds = tonumber(os.getenv("CIRCUS_L2_REWINDS") or "100000")
local function rewind_after_death()
  rewinds = rewinds + 1
  if rewinds > max_rewinds then
    print("search: rewind limit reached; writing the failed attempt")
    rows[frame] = row(frame - (stage_start or frame))
    finish()
    return
  end
  -- The jump to retime is the last one made, or the one the pilot is still
  -- waiting to make if Charlie was hit on the rope well after the landing.
  local i = jumps + 1
  local cp = checkpoint[i]
  if was_airborne or (cp and frame - cp.frame < 20) then i = jumps end
  -- Hit while refusing to jump on the way to the stand: retime the jump that
  -- put Charlie there instead.
  if not was_airborne and want_stand and u8(0x2203) == 0xf8 and u8(0x2406) >= 100 then i = jumps end
  if i < 1 then i = 1 end
  while true do
    choice[i] = (choice[i] or 1) + 1
    if choice[i] <= #candidates then break end
    choice[i] = 1
    i = i - 1
    if i < 1 then
      print("search: every timing failed; giving up")
      i = 1
      break
    end
  end
  cp = checkpoint[i]
  if cp == nil then
    print("search: no checkpoint for jump " .. i)
    finish()
    return
  end
  print(string.format("search: jump %d failed at frame %d, retrying with %d frames to go (after %d frames LEFT) from frame %d",
    i, frame, candidates[choice[i]][1], candidates[choice[i]][2], cp.frame))
  pending_load = {index = i, cp = cp}
  machine:load(cp.path)
  for f = cp.frame + 1, frame do rows[f] = nil end
  frame = cp.frame
  stage_start = cp.stage_start
  stand_wait = cp.stand_wait
  jumps = i - 1
  was_airborne = false
  set(left, cp.input.left); set(right, cp.input.right); set(button, cp.input.button)
end

emu.register_frame_done(function()
  if pending_load then
    -- The state loaded during this frame: the inputs re-applied above have
    -- produced the frame after the checkpoint.
    pending_load = nil
  end
  frame = frame + 1
  set(coin, between(300, 306))
  set(start, between(400, 406))
  if frame >= 402 and frame <= 470 then program:write_u8(0x2201, 1) end
  if frame == 471 and not poked then
    -- As if Stage 1 had been played once, plus the Stage 2 visit count that
    -- $6BD5 could not increment before the poke.
    program:write_u8(0x220e, 1); program:write_u8(0x220f, visits)
    poked = true
  end
  if stage_start == nil and frame > 470 and u8(0x2400) ~= 0 and u8(0x2402) == 1 and u8(0x2201) == 1 then
    stage_start = frame
    if bonus_poke and #bonus_poke == 4 then
      for digit = 1, 4 do
        program:write_u8(0x227b + digit, tonumber(bonus_poke:sub(digit, digit)))
      end
    end
    if search then save_checkpoint(1) end
  end
  local rel = stage_start and (frame - stage_start) or -1
  if search and stage_start then
    local state = u8(0x2402)
    -- With CIRCUS_L2_STAND a goal landing that skipped the stand is retimed
    -- like a death, so the run that survives walks all the way to $C0.
    if state == 7 or state == 2 or (want_stand and state == 4 and u8(0x2762) == 0) then
      rewind_after_death()
      return
    end
    local airborne = state == 1 and u8(0x2407) ~= 0
    if airborne and not was_airborne then jumps = jumps + 1 end
    if not airborne and was_airborne and state == 1 then save_checkpoint(jumps + 1) end
    was_airborne = airborne
  end
  local l, r, j = false, false, false
  if stage_start then
    if mode == "inputs" then
      local byte = input_rows[rel + 1]
      if byte then
        l = (byte & 0x01) == 0; r = (byte & 0x02) == 0; j = (byte & 0x10) == 0
      end
    elseif mode == "auto" or mode == "autoscript" then
      l, r, j = auto_pilot(jump_at, 0)
    elseif search then
      local pick = candidates[choice[jumps + 1] or 1]
      local cp = checkpoint[jumps + 1]
      local retreat = 0
      if cp then retreat = pick[2] - (frame - cp.frame) end
      l, r, j = auto_pilot(pick[1], retreat)
    elseif mode == "right" then
      r = true
    end
    if mode == "script" or mode == "autoscript" then
      for _, s in ipairs(script) do
        if rel >= s[1] and rel < s[2] then
          l = l or (s[3] & 1) ~= 0; r = r or (s[3] & 2) ~= 0
          if (s[3] & 4) ~= 0 then j = true end
          if (s[3] & 8) ~= 0 then j = false end
        end
      end
    end
  end
  set(left, l); set(right, r); set(button, j)
  rows[frame] = row(rel)
  if snaps[rel] and not search then
    machine.video:snapshot()
  end
  if frame >= last_frame then finish() end
end, "frame")
