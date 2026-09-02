-- Headless Level 1 autoplay: coin, start, hold RIGHT, scripted jumps.
-- Records the same state columns as capture_level1_manual_reference.lua plus
-- the object records needed by the native replay comparator.
local machine = manager.machine
local maincpu = assert(machine.devices[":maincpu"])
local program = assert(maincpu.spaces["program"])
local system_port = assert(machine.ioport.ports[":SYSTEM"])
local player_port = assert(machine.ioport.ports[":P1"])
local prefix = os.getenv("CIRCUS_AUTO_PREFIX") or "/home/claude/mame/out/auto"
local last_frame = tonumber(os.getenv("CIRCUS_AUTO_LAST") or "2400")
local mode = os.getenv("CIRCUS_AUTO_MODE") or "death"

local state_file = assert(io.open(prefix .. "-state.csv", "w"))
local object_file = assert(io.open(prefix .. "-objects.csv", "w"))
local frame = 0
local coin = system_port:field(0x01)
local start = system_port:field(0x08)
local left = player_port:field(0x01)
local right = player_port:field(0x02)
local jump = player_port:field(0x10)

local function u8(a) return program:read_u8(a) end
local function u16(a) return u8(a) * 256 + u8(a + 1) end

state_file:write(table.concat({
  "frame", "pc", "system_input", "player_input",
  "player_state_2800", "airborne_20b0",
  "rider_x_2646", "rider_y_2644", "jump_anim_20b4",
  "jump_acc_20b5", "scroll_command_20b1", "scroll_acc_2203",
  "activation_acc_20c2", "course_index_2208", "course_state_20bc",
  "state_20bd", "state_20be", "state_20bf", "score_bcd_20a0",
  "object_26d0_status", "object_26d0_x88", "object_26d0_y",
  "object_26d0_timer", "object_26d0_code", "object_26d0_attr",
  "frame_byte_14", "lives_2200", "extra_220a", "coin_220b", "missed_220c",
  "bb_20bb", "c1_20c1", "c4_20c4", "phase_05", "phase_06", "cb_20cb"
}, ",") .. "\n")
local recs = {0x24b0, 0x24f0, 0x2530, 0x2570, 0x2580, 0x25e0, 0x26d0, 0x2700, 0x2730, 0x2760}
local header = {"frame"}
for _, r in ipairs(recs) do
  local n = string.format("%04x", r)
  header[#header+1] = n .. "_st"; header[#header+1] = n .. "_x"; header[#header+1] = n .. "_b1b2"; header[#header+1] = n .. "_b8"
end
object_file:write(table.concat(header, ",") .. "\n")

csv_inputs = {}
csv_offset = tonumber(os.getenv("CIRCUS_AUTO_CSV_OFFSET") or "9")
csv_cutoff = tonumber(os.getenv("CIRCUS_AUTO_CSV_CUTOFF") or "999999")
do
  local path = os.getenv("CIRCUS_AUTO_CSV")
  if path then
    local f = assert(io.open(path, "r"))
    local header = f:read("l")
    local cols = {}
    local i = 0
    for name in header:gmatch("[^,]+") do i = i + 1; cols[name] = i end
    for line in f:lines() do
      local fields = {}
      for v in line:gmatch("[^,]*") do fields[#fields+1] = v end
      local fr = tonumber(fields[cols["frame"]])
      local inp = tonumber(fields[cols["player_input"]], 16)
      if fr and inp then csv_inputs[fr] = inp end
    end
    f:close()
  end
end

local function set(field, pressed)
  if pressed then field:set_value(1) else field:clear_value() end
end

local function between(a, b) return frame >= a and frame < b end

local inputs = function()
  set(coin, between(300, 306))
  set(start, between(400, 406))
  if mode == "death" then
    -- Hold RIGHT from frame 200; never jump: the first hoop kills Charlie.
    set(right, frame >= 450)
    set(left, false)
    set(jump, false)
  elseif mode == "csv" then
    -- Replay the P1 port column of a manual capture, aligned by the level
    -- start frame, until a cutoff frame after which only RIGHT is held.
    local row = frame + csv_offset
    local v = csv_inputs[row]
    if frame >= csv_cutoff or v == nil then
      set(right, frame >= 450); set(left, false); set(jump, false)
    else
      set(left, (v & 0x01) == 0)
      set(right, (v & 0x02) == 0)
      set(jump, (v & 0x10) == 0)
    end
  elseif mode == "death-jump" then
    -- Hold RIGHT; jump once well before the first hoop so the miss happens
    -- while airborne, then keep running into the second hoop.
    set(right, frame >= 450)
    set(left, false)
    set(jump, between(480, 483))
  end
end

emu.register_frame_done(function()
  frame = frame + 1
  inputs()
  state_file:write(string.format(
    "%d,%04x,%02x,%02x,%02x,%02x,%d,%d,%02x,%04x,%04x,%04x,%04x,%02x,%02x,%02x,%02x,%02x,%06x,%02x,%04x,%d,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%04x,%02x,%02x,%02x\n",
    frame, maincpu.state["PC"].value,
    system_port:read(), player_port:read(),
    u8(0x2800), u8(0x20b0), u8(0x2646), u8(0x2644), u8(0x20b4),
    u16(0x20b5), u16(0x20b1), u16(0x2203), u16(0x20c2),
    u8(0x2208), u8(0x20bc), u8(0x20bd), u8(0x20be), u8(0x20bf),
    u8(0x20a0) * 65536 + u8(0x20a1) * 256 + u8(0x20a2),
    u8(0x26d0), u16(0x26d6), u8(0x26d4), u8(0x26d8), u8(0x26de), u8(0x26df),
    u8(0x2014), u8(0x2200), u8(0x220a), u8(0x220b), u8(0x220c),
    u8(0x20bb), u8(0x20c1), u16(0x20c4), u8(0x2005), u8(0x2006), u8(0x20cb)))
  local row = {tostring(frame)}
  for _, r in ipairs(recs) do
    row[#row+1] = string.format("%02x", u8(r))
    row[#row+1] = string.format("%04x", u16(r + 6))
    row[#row+1] = string.format("%04x", u16(r + 1))
    row[#row+1] = string.format("%02x", u8(r + 8))
  end
  object_file:write(table.concat(row, ",") .. "\n")
  if frame >= last_frame then
    state_file:close(); object_file:close()
    machine:exit()
  end
end, "frame")
