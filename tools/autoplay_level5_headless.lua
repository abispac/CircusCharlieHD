-- Headless Stage 5 (pony and springboards) reference capture for circusc4:
--   mame circusc4 -video none -sound none -nothrottle -seconds_to_run 400 \
--     -autoboot_script tools/autoplay_level5_headless.lua
-- Inserts a coin at frame 300, presses START at 400 and forces $2201 = 4 while
-- the game initialises so the first stage is the pony.  Logs one row per
-- emulated frame with the $20B0-$20C2 stage variables, Charlie's and the
-- pony's cells, the plaques, the eight springboard records at $2900 and the
-- scroll, score and bonus.
--
-- Environment:
--   CIRCUS_L5_MODE     right   hold RIGHT (fast) and never jump (default)
--                      idle    never touch anything (pony trots at the middle pace)
--                      auto    hold RIGHT and jump onto every springboard
--                      script  read CIRCUS_L5_SCRIPT lines "from,to,mask"
--                              (stage-relative frames, mask 1 LEFT 2 RIGHT 4 JUMP)
--   CIRCUS_L5_OUT      path    output CSV (default /tmp/circusc4-level5.csv)
--   CIRCUS_L5_LAST     n       stop after n emulated frames (default 4000)
--   CIRCUS_L5_VISITS   n       $2212 (Stage 5 visit count) poked after START
--   CIRCUS_L5_BONUS    dddd    poke the bonus digits on the first stage frame
--   CIRCUS_L5_SNAP     a,b,c   stage-relative frames at which to snapshot
--   CIRCUS_L5_JUMPAT   n       auto mode: jump when a springboard's screen x is this
--                      search  the auto pilot with save states: every fall
--                              rewinds to the landing before the fatal jump
--                              and retries it with the next entry of
--                              `candidates` (the springboard x at which to
--                              press the button), so the run written out
--                              reaches the goal
--   CIRCUS_L5_REWINDS  n       search mode: give up after n rewinds
--   CIRCUS_L5_PERFECT  1       neutralise the $A1A3 INC $220A with a memory
--                              tap so the goal shows the extra-life bird (the
--                              native replay does the same with
--                              --replay-perfect)
local machine = manager.machine
local maincpu = assert(machine.devices[":maincpu"])
local program = assert(maincpu.spaces["program"])
local system_port = assert(machine.ioport.ports[":SYSTEM"])
local player_port = assert(machine.ioport.ports[":P1"])
local mode = os.getenv("CIRCUS_L5_MODE") or "right"
local out_path = os.getenv("CIRCUS_L5_OUT") or "/tmp/circusc4-level5.csv"
local last_frame = tonumber(os.getenv("CIRCUS_L5_LAST") or "4000")
local visits = tonumber(os.getenv("CIRCUS_L5_VISITS") or "1")
local jump_at = tonumber(os.getenv("CIRCUS_L5_JUMPAT") or "120")
local bonus_poke = os.getenv("CIRCUS_L5_BONUS")
if os.getenv("CIRCUS_L5_PERFECT") == "1" then
  perfect_tap = program:install_write_tap(0x220a, 0x220a, "perfect", function(offset, data, mask)
    local pc = maincpu.state["PC"].value
    if pc >= 0xa1a3 and pc <= 0xa1a6 then return 0 end
    return data
  end)
end
local snaps = {}
do local list = os.getenv("CIRCUS_L5_SNAP"); if list then for n in list:gmatch("%d+") do snaps[tonumber(n)] = true end end end
local script = {}
do
  local path = os.getenv("CIRCUS_L5_SCRIPT")
  if path then for line in io.lines(path) do
    local a, b, m = line:match("^%s*(%d+)%s*,%s*(%d+)%s*,%s*(%d+)")
    if a then script[#script + 1] = {tonumber(a), tonumber(b), tonumber(m)} end
  end end
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
local header = {"frame", "rel", "input", "phase", "stage", "scroll", "page", "low", "spawncount", "perfect", "v220b", "lives", "score", "bonus"}
for i = 0xb0, 0xc2 do header[#header + 1] = string.format("v%02x", i) end
for _, n in ipairs({"ch_row", "ch_col", "ch_code", "pony_row", "pony_col", "pony_code",
  "pl0_row", "pl0_col", "pl0_code", "pl1_row", "pl1_col", "pl1_code", "pl2_row", "pl2_col", "pl2_code"}) do header[#header + 1] = n end
for i = 0, 7 do for b = 0, 14 do header[#header + 1] = string.format("h%d_%x", i, b) end end
for _, n in ipairs({"v287d", "v287e", "v287f", "v2212", "phase05", "phase06", "frame_byte", "dips"}) do header[#header + 1] = n end
out:write(table.concat(header, ",") .. "\n")
local function row(rel)
  local v = {}
  local function push(x) v[#v + 1] = tostring(x) end
  push(frame); push(rel); push(string.format("%02x", player_port:read()))
  push(u8(0x2800)); push(u8(0x2201)); push(u16(0x2203)); push(u8(0x2203)); push(u8(0x2204)); push(u8(0x2208)); push(u8(0x220a)); push(u8(0x220b)); push(u8(0x2200))
  push(tonumber(string.format("%02x%02x%02x", u8(0x20a0), u8(0x20a1), u8(0x20a2))))
  push(u8(0x227c) * 1000 + u8(0x227d) * 100 + u8(0x227e) * 10 + u8(0x227f))
  for i = 0xb0, 0xc2 do push(u8(0x2000 + i)) end
  push(u8(0x26b4)); push(u8(0x26b6)); push(u8(0x26be)); push(u8(0x2674)); push(u8(0x2676)); push(u8(0x267e))
  push(u8(0x25f4)); push(u8(0x25f6)); push(u8(0x25fe)); push(u8(0x2604)); push(u8(0x2606)); push(u8(0x260e)); push(u8(0x2614)); push(u8(0x2616)); push(u8(0x261e))
  for i = 0, 7 do for b = 0, 14 do push(u8(0x2900 + i * 16 + b)) end end
  push(u8(0x287d)); push(u8(0x287e)); push(u8(0x287f)); push(u8(0x2212)); push(u8(0x2005)); push(u8(0x2006)); push(u8(0x2014)); push(u8(0x202f))
  return table.concat(v, ",") .. "\n"
end

local function auto_pilot(at, hold_left, hold_right)
  -- Ride with the given stick (the pace: RIGHT 3, neutral 2, LEFT 1 pixel a
  -- frame) and jump when Charlie is on the pony ($B0 == 0) and a live
  -- springboard's screen x ($8,U) is at `at`.  Airborne, RIGHT is held; the
  -- pace is frozen at its take-off value anyway ($B1 is only re-read while
  -- riding and on a landing).
  if hold_left == nil then hold_left, hold_right = false, true end
  if u8(0x20b0) ~= 0 then return false, true, false end
  if u8(0x20bd) == 3 then
    -- The goal stand ($A795): Charlie lands when he is back down to row $A1
    -- with its column $C2 inside $24-$5B, and riding into it (or coming
    -- down) with $C2 at $5C-$69 or $16-$23 is a miss.  A jump lasts some 58
    -- frames, so ride at the slow pace and take off with the stand at 140.
    local x = u8(0x20c2)
    return true, false, x >= 138 and x <= 142
  end
  for i = 0, 7 do
    local a = 0x2900 + i * 16
    if u8(a) == 3 then
      local x = u8(a + 8)
      if x >= at - 2 and x <= at + 2 then return hold_left, hold_right, true end
    end
  end
  return hold_left, hold_right, false
end


-- Search mode: a save state from the stage start and from every landing back
-- on the pony; a fall rewinds to the landing before the fatal jump and
-- retries it with the next candidate: the pace to ride at until the jump
-- and the springboard x at which to press the button.  A landing needs
-- Charlie descending through rows $90-$93 while the platform is within
-- $A,U of his column and the rise through those rows must happen before it
-- gets that close, so the jump is made some 54 frames early -- at x around
-- 240 at the fast pace, 160 at the neutral one and 134 at the slow one --
-- and the pace is what lets a springboard that is already close be made.
-- When every candidate of a jump fails the jump before it is retimed.  Rows
-- of abandoned attempts are dropped so the CSV holds one continuous run.
local search = mode == "search"
local rows = {}
local candidates = {}
for _, x in ipairs({220, 214, 226, 208, 232, 202, 238, 196, 244, 190, 250, 184}) do candidates[#candidates + 1] = {x = x, l = false, r = true} end
for _, x in ipairs({160, 154, 166, 148, 172, 142, 178, 136, 184, 130, 190, 124}) do candidates[#candidates + 1] = {x = x, l = false, r = false} end
for _, x in ipairs({134, 130, 138, 126, 142, 122, 146, 118, 150, 114, 154, 110, 106, 102, 98, 94, 90}) do candidates[#candidates + 1] = {x = x, l = true, r = false} end
local choice = {}
local checkpoint = {}
local jumps = 0
local was_airborne = false
local rewinds = 0
local max_rewinds = tonumber(os.getenv("CIRCUS_L5_REWINDS") or "100000")
local saves = 0
local function save_checkpoint(index)
  saves = saves + 1
  local path = string.format("/tmp/circusc4-l5-cp%d-%d", index, saves)
  checkpoint[index] = {path = path, frame = frame, stage_start = stage_start,
                       scroll = u16(0x2203), score = u8(0x20a0) * 65536 + u8(0x20a1) * 256 + u8(0x20a2), fb = u8(0x2014)}
  machine:save(path)
end
local verify = nil
local function finish()
  for f = 1, frame do if rows[f] then out:write(rows[f]) end end
  out:close()
  machine:exit()
end
local function rewind_after_fall()
  rewinds = rewinds + 1
  if rewinds > max_rewinds then
    print("search: rewind limit reached; writing the failed attempt")
    finish()
    return
  end
  -- The jump to retime is the last one made when Charlie fell from the air
  -- (or right after landing), otherwise the one the pilot still owes: he
  -- rode into the pole.
  local i = jumps + 1
  local cp = checkpoint[i]
  if was_airborne or (cp and frame - cp.frame < 12) then i = jumps end
  if i < 1 then i = 1 end
  while true do
    choice[i] = (choice[i] or 1) + 1
    if choice[i] <= #candidates then break end
    choice[i] = 1
    i = i - 1
    if i < 1 then
      print("search: every timing failed; giving up")
      finish()
      return
    end
  end
  cp = checkpoint[i]
  if cp == nil then
    print("search: no checkpoint for jump " .. i)
    finish()
    return
  end
  local pick = candidates[choice[i]]
  print(string.format("search: jump %d failed at frame %d, retrying at x %d (%s) from frame %d",
    i, frame, pick.x, pick.l and "LEFT" or (pick.r and "RIGHT" or "neutral"), cp.frame))
  machine:load(cp.path)
  verify = cp
  for f = cp.frame + 1, frame do rows[f] = nil end
  frame = cp.frame
  stage_start = cp.stage_start
  jumps = i - 1
  was_airborne = false
  -- The frame after the checkpoint is emulated with the inputs set now.
  set(left, pick.l); set(right, pick.r); set(button, false)
end

emu.register_frame_done(function()
  frame = frame + 1
  set(coin, between(300, 306))
  set(start, between(400, 406))
  if frame >= 402 and frame <= 470 then program:write_u8(0x2201, 4) end
  if frame == 471 and not poked then
    program:write_u8(0x220e, 1); program:write_u8(0x220f, 1); program:write_u8(0x2210, 1); program:write_u8(0x2211, 1)
    program:write_u8(0x2212, visits)
    poked = true
  end
  if stage_start == nil and frame > 470 and u8(0x2800) == 1 and u8(0x2201) == 4 then
    stage_start = frame
    if bonus_poke and #bonus_poke == 4 then
      for digit = 1, 4 do program:write_u8(0x227b + digit, tonumber(bonus_poke:sub(digit, digit))) end
    end
  end
  local rel = stage_start and (frame - stage_start) or -1
  if verify then
    -- The first frame after a load: the state must continue the checkpoint.
    local score = u8(0x20a0) * 65536 + u8(0x20a1) * 256 + u8(0x20a2)
    local fb = u8(0x2014)
    if score ~= verify.score or (fb - verify.fb) % 256 ~= 1 then
      print(string.format("search: WARNING load of %s did not restore the checkpoint (score %06x vs %06x, frame byte %02x vs %02x)",
        verify.path, score, verify.score, fb, verify.fb))
    end
    verify = nil
  end
  if search and stage_start then
    if rel == 1 then save_checkpoint(1) end
    if u8(0x2800) == 2 then
      rewind_after_fall()
      return
    end
    local airborne = u8(0x2800) == 1 and u8(0x20b0) ~= 0
    if airborne and not was_airborne then jumps = jumps + 1 end
    if not airborne and was_airborne and u8(0x2800) == 1 then save_checkpoint(jumps + 1) end
    was_airborne = airborne
  end
  local l, r, j = false, false, false
  if stage_start then
    if mode == "right" then r = true
    elseif mode == "auto" then l, r, j = auto_pilot(jump_at)
    elseif search then
      local pick = candidates[choice[jumps + 1] or 1]
      l, r, j = auto_pilot(pick.x, pick.l, pick.r)
    elseif mode == "script" then
      for _, s in ipairs(script) do
        if rel >= s[1] and rel < s[2] then
          l = l or (s[3] & 1) ~= 0; r = r or (s[3] & 2) ~= 0; j = j or (s[3] & 4) ~= 0
        end
      end
    end
  end
  set(left, l); set(right, r); set(button, j)
  if stage_start and snaps[rel] and not search then machine.video:snapshot() end
  rows[frame] = row(rel)
  if frame >= last_frame then finish() end
end, "frame")
