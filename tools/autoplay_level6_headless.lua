-- Headless Stage 6 (the trapeze) reference capture for circusc4:
--   mame circusc4 -video none -sound none -nothrottle -seconds_to_run 400 \
--     -autoboot_script tools/autoplay_level6_headless.lua
-- Inserts a coin at frame 300, presses START at 400 and forces $2201 = 5 while
-- the game initialises so the first stage is the trapeze.  Logs one row per
-- emulated frame with the $20B0-$20FF stage variables, the five trapeze
-- records at $2900 (their first 16 bytes), Charlie's cells, the trampoline
-- and goal cells, the three rope sprite columns and the scroll, score and
-- bonus.
--
-- Environment:
--   CIRCUS_L6_MODE     idle    never touch anything (default)
--                      jump    press the button once at CIRCUS_L6_JUMPAT
--                      hold    hold the button from CIRCUS_L6_JUMPAT on
--                      script  read CIRCUS_L6_SCRIPT lines "from,to,mask"
--                              (stage-relative frames, mask 1 LEFT 2 RIGHT 4 JUMP)
--                      search  save states at every catch: a fall rewinds to
--                              the catch before it and retries the release
--                              with the next candidate (frames to hang on,
--                              and the stick held meanwhile), so the run
--                              written out reaches the goal platform
--   CIRCUS_L6_REWINDS  n       search mode: give up after n rewinds
--   CIRCUS_L6_OUT      path    output CSV (default /tmp/circusc4-level6.csv)
--   CIRCUS_L6_LAST     n       stop after n emulated frames (default 3000)
--   CIRCUS_L6_VISITS   n       $2213 (Stage 6 visit count) poked after START
--   CIRCUS_L6_BONUS    dddd    poke the bonus digits on the first stage frame
--   CIRCUS_L6_JUMPAT   n       stage-relative frame of the jump/hold modes
--   CIRCUS_L6_SNAP     a,b,c   stage-relative frames at which to snapshot
--   CIRCUS_L6_TRACE    path    log every $0800 sound latch write with its frame
local machine = manager.machine
local maincpu = assert(machine.devices[":maincpu"])
local program = assert(maincpu.spaces["program"])
local system_port = assert(machine.ioport.ports[":SYSTEM"])
local player_port = assert(machine.ioport.ports[":P1"])
local mode = os.getenv("CIRCUS_L6_MODE") or "idle"
local out_path = os.getenv("CIRCUS_L6_OUT") or "/tmp/circusc4-level6.csv"
local last_frame = tonumber(os.getenv("CIRCUS_L6_LAST") or "3000")
local visits = tonumber(os.getenv("CIRCUS_L6_VISITS") or "1")
local jump_at = tonumber(os.getenv("CIRCUS_L6_JUMPAT") or "120")
local bonus_poke = os.getenv("CIRCUS_L6_BONUS")
local snaps = {}
do local list = os.getenv("CIRCUS_L6_SNAP"); if list then for n in list:gmatch("%d+") do snaps[tonumber(n)] = true end end end
local script = {}
do
  local path = os.getenv("CIRCUS_L6_SCRIPT")
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

local trace_path = os.getenv("CIRCUS_L6_TRACE")
if trace_path then
  local trace = assert(io.open(trace_path, "w"))
  sound_tap = program:install_write_tap(0x0800, 0x0800, "l6snd", function(offset, data, mask)
    trace:write(string.format("%d,%d,%02x\n", frame, stage_start and (frame - stage_start) or -1, data))
    trace:flush()
  end)
end

local out = assert(io.open(out_path, "w"))
local header = {"frame", "rel", "input", "phase", "stage", "scroll", "page", "low", "spawncount", "perfect", "v220b", "lives", "score", "bonus"}
for i = 0xb0, 0xff do header[#header + 1] = string.format("v%02x", i) end
for _, n in ipairs({"ch_row", "ch_col", "ch_code", "ch2_row", "ch2_col", "ch2_code",
  "tr0_row", "tr0_col", "tr0_code", "tr1_row", "tr1_col", "tr1_code",
  "goal_row", "goal_col", "goal_code", "pl_row", "pl_col", "pl_code"}) do header[#header + 1] = n end
for i = 0, 4 do for b = 0, 15 do header[#header + 1] = string.format("t%d_%x", i, b) end end
for r = 0, 2 do for c = 0, 15 do header[#header + 1] = string.format("r%d_%x", r, c) end end
for _, n in ipairs({"v287d", "v287e", "v287f", "v2213", "phase05", "phase06", "frame_byte", "dips"}) do header[#header + 1] = n end
out:write(table.concat(header, ",") .. "\n")
local function row(rel)
  local v = {}
  local function push(x) v[#v + 1] = tostring(x) end
  push(frame); push(rel); push(string.format("%02x", player_port:read()))
  push(u8(0x2800)); push(u8(0x2201)); push(u16(0x2203)); push(u8(0x2203)); push(u8(0x2204)); push(u8(0x2208)); push(u8(0x220a)); push(u8(0x220b)); push(u8(0x2200))
  push(tonumber(string.format("%02x%02x%02x", u8(0x20a0), u8(0x20a1), u8(0x20a2))))
  push(u8(0x227c) * 1000 + u8(0x227d) * 100 + u8(0x227e) * 10 + u8(0x227f))
  for i = 0xb0, 0xff do push(u8(0x2000 + i)) end
  push(u8(0x2774)); push(u8(0x2776)); push(u8(0x277e)); push(u8(0x2764)); push(u8(0x2766)); push(u8(0x276e))
  push(u8(0x2734)); push(u8(0x2736)); push(u8(0x273e)); push(u8(0x2724)); push(u8(0x2726)); push(u8(0x272e))
  push(u8(0x27e4)); push(u8(0x27e6)); push(u8(0x27ee)); push(u8(0x2714)); push(u8(0x2716)); push(u8(0x271e))
  for i = 0, 4 do for b = 0, 15 do push(u8(0x2900 + i * 0x30 + b)) end end
  for r = 0, 2 do for c = 0, 15 do push(u8(0x24f6 + r * 0x100 - c * 0x10)) end end
  push(u8(0x287d)); push(u8(0x287e)); push(u8(0x287f)); push(u8(0x2213)); push(u8(0x2005)); push(u8(0x2006)); push(u8(0x2014)); push(u8(0x202f))
  return table.concat(v, ",") .. "\n"
end

-- Search mode.  A checkpoint is saved whenever Charlie takes hold of a bar
-- ($B0 goes back to zero) and at the stage start; a fall (phase 3) rewinds
-- to the last checkpoint and tries the next candidate: how many frames to
-- hang on before pressing the button, with LEFT, nothing or RIGHT held.
local search = mode == "search"
local rows = {}
local candidates = {}
for w = 8, 1200, 4 do candidates[#candidates + 1] = {wait = w, stick = 3} end
local pump_group_end = #candidates
for w = 8, 240, 4 do candidates[#candidates + 1] = {wait = w, stick = 0} end
-- stick 3: pump -- hold the stick that grows the amplitude ($ADA9: LEFT
-- while the bar swings one way, RIGHT the other); 0: hang still.
local function pump_stick()
  local u = 0x2900
  while u < 0x2a00 and u8(u) ~= u8(0x20da) do u = u + 0x30 end
  local fa = (u8(u + 2) < 0x5a) and 1 or 0
  fa = fa ~ (u8(u + 1) & 1)
  return fa + 1
end
local choice = {}
local checkpoint = {}
local grabs = 0
local was_hanging = true
local pressed = false
local best_da = -1   -- a catch must be on a bar past every bar caught before
local rewinds = 0
local saves = 0
local max_rewinds = tonumber(os.getenv("CIRCUS_L6_REWINDS") or "100000")
local function save_checkpoint(index)
  saves = saves + 1
  local path = string.format("/tmp/circusc4-l6-cp%d-%d", index, saves)
  checkpoint[index] = {path = path, frame = frame, stage_start = stage_start, best = best_da}
  machine:save(path)
  -- The run so far, for a look at where the search is.
  local partial = io.open(out_path .. ".partial", "w")
  if partial then
    partial:write(table.concat(header, ",") .. "\n")
    for f = 1, frame do if rows[f] then partial:write(rows[f]) end end
    partial:close()
  end
end
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
  -- The release to retime is the one made from the bar he is hanging on
  -- (or was, before the fall): checkpoint grabs + 1.
  local i = grabs + 1
  -- A fall that came without a press (the bar let him go after sixteen
  -- swings, or a bounce carried him past every bar) cannot be retimed here:
  -- exhaust this release and retime the one before.
  if not pressed then
    local c = choice[i] or 1
    choice[i] = c <= pump_group_end and pump_group_end or #candidates
  end
  while true do
    choice[i] = (choice[i] or 1) + 1
    if choice[i] <= #candidates then break end
    choice[i] = 1
    i = i - 1
    if i < 1 then
      print("search: every release failed; giving up")
      finish()
      return
    end
  end
  local cp = checkpoint[i]
  if cp == nil then
    print("search: no checkpoint for grab " .. i)
    finish()
    return
  end
  local pick = candidates[choice[i]]
  print(string.format("search: release %d failed at frame %d, retrying after %d frames (stick %d) from frame %d",
    i, frame, pick.wait, pick.stick, cp.frame))
  machine:load(cp.path)
  for f = cp.frame + 1, frame do rows[f] = nil end
  frame = cp.frame
  stage_start = cp.stage_start
  grabs = i - 1
  was_hanging = true
  pressed = false
  best_da = cp.best
  local stick = pick.stick
  if stick == 3 then stick = pump_stick() end
  set(left, stick == 1); set(right, stick == 2); set(button, false)
end

emu.register_frame_done(function()
  frame = frame + 1
  set(coin, between(300, 306))
  set(start, between(400, 406))
  if frame >= 402 and frame <= 470 then program:write_u8(0x2201, 5) end
  if frame == 471 and not poked then
    program:write_u8(0x220e, 1); program:write_u8(0x220f, 1); program:write_u8(0x2210, 1); program:write_u8(0x2211, 1); program:write_u8(0x2212, 1)
    program:write_u8(0x2213, visits)
    poked = true
  end
  if stage_start == nil and frame > 470 and u8(0x2800) == 1 and u8(0x2201) == 5 then
    stage_start = frame
    if bonus_poke and #bonus_poke == 4 then
      for digit = 1, 4 do program:write_u8(0x227b + digit, tonumber(bonus_poke:sub(digit, digit))) end
    end
  end
  local rel = stage_start and (frame - stage_start) or -1
  if search and stage_start then
    if rel == 1 then best_da = u8(0x20da); save_checkpoint(1) end
    local cp = checkpoint[grabs + 1]
    local pick = candidates[choice[grabs + 1] or 1]
    -- A press that did not let go ($DD still counting) wastes the candidate.
    local ignored = cp and u8(0x2800) == 1 and u8(0x20b0) == 0 and
        (frame - cp.frame) == pick.wait + 3
    if u8(0x2800) == 3 or ignored then
      rewind_after_fall()
      return
    end
    local hanging = u8(0x2800) == 1 and u8(0x20b0) == 0
    if hanging and not was_hanging then
      if u8(0x20da) <= best_da then
        -- Back on a bar already caught: no progress, so this release fails.
        rewind_after_fall()
        return
      end
      best_da = u8(0x20da)
      grabs = grabs + 1
      save_checkpoint(grabs + 1)
      pressed = false
    end
    was_hanging = hanging
  end
  local l, r, j = false, false, false
  if stage_start then
    if search then
      local cp = checkpoint[grabs + 1]
      local pick = candidates[choice[grabs + 1] or 1]
      if u8(0x2800) == 1 and u8(0x20b0) == 0 and cp then
        local stick = pick.stick
        if stick == 3 then stick = pump_stick() end
        l = stick == 1; r = stick == 2
        j = (frame - cp.frame) == pick.wait
        if j then pressed = true end
      end
    elseif mode == "pump" then
      local stick = pump_stick()
      l = stick == 1; r = stick == 2
      j = rel == jump_at
    elseif mode == "jump" then j = rel == jump_at
    elseif mode == "hold" then j = rel >= jump_at
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
