-- Manual, read-only Circus Charlie Level 1 reference capture.
--
-- This script never writes emulated memory and never supplies gameplay input.
-- It records both sprite-RAM banks, the Level 1 object pool, relevant state,
-- and exact MAME screenshots.  Letter hotkeys add labelled capture bursts:
--   E extra Charlie, Up hidden coin, G goal/finish, B score/bonus, M generic.
-- Press X after the final requested presentation to finalize the files.

local machine = manager.machine
local maincpu = assert(machine.devices[":maincpu"])
local program = assert(maincpu.spaces["program"])
local screen = assert(machine.screens[":screen"])
local system_port = assert(machine.ioport.ports[":SYSTEM"])
local player_port = assert(machine.ioport.ports[":P1"])

local prefix = os.getenv("CIRCUS_MANUAL_CAPTURE_PREFIX") or
  "/tmp/circusc4-level1-manual/capture"
-- Lossless MAME snapshots can stall real-time audio on some Macs.  Manual
-- markers therefore capture one exact frame by default.  The full hardware
-- state remains continuous, and the player may press a marker repeatedly for
-- multiple animation poses.  Automatic screenshots are opt-in for the same
-- reason.
local burst_frames = tonumber(os.getenv("CIRCUS_MANUAL_BURST_FRAMES") or "1")
local auto_limit = tonumber(os.getenv("CIRCUS_MANUAL_AUTO_LIMIT") or "0")
local test_frames = tonumber(os.getenv("CIRCUS_MANUAL_TEST_FRAMES") or "0")

local state_file = assert(io.open(prefix .. "-state.csv", "w"))
local sprite_file = assert(io.open(prefix .. "-sprites.csv", "w"))
local object_file = assert(io.open(prefix .. "-objects.csv", "w"))
local marker_file = assert(io.open(prefix .. "-markers.csv", "w"))
local memory_file = assert(io.open(prefix .. "-marker-memory.csv", "w"))
local auto_file = assert(io.open(prefix .. "-automatic.csv", "w"))

local frame = 0
local marker_id = 0
local auto_id = 0
local active = true
local pending = {}
local seen_signatures = {}
local key_was_down = {}

local hotkeys = {
  extra_charlie = machine.input:seq_from_tokens("KEYCODE_E"),
  hidden_coin = machine.input:seq_from_tokens("KEYCODE_UP"),
  goal_finish = machine.input:seq_from_tokens("KEYCODE_G"),
  score_bonus = machine.input:seq_from_tokens("KEYCODE_B"),
  generic = machine.input:seq_from_tokens("KEYCODE_M"),
  finish = machine.input:seq_from_tokens("KEYCODE_X"),
}

local function u8(address)
  return program:read_u8(address)
end

local function u16(hi_address)
  return u8(hi_address) * 256 + u8(hi_address + 1)
end

local function hex8(value)
  return string.format("%02x", value)
end

local function flush_all()
  state_file:flush()
  sprite_file:flush()
  object_file:flush()
  marker_file:flush()
  memory_file:flush()
  auto_file:flush()
end

state_file:write(table.concat({
  "frame", "pc", "system_input", "player_input",
  "player_state_2800", "airborne_20b0",
  "rider_x_2646", "rider_y_2644", "jump_anim_20b4",
  "jump_acc_20b5", "scroll_command_20b1", "scroll_acc_2203",
  "activation_acc_20c2", "course_index_2208", "course_state_20bc",
  "state_20bd", "state_20be", "state_20bf", "score_bcd_20a0",
  "object_26d0_status", "object_26d0_x88", "object_26d0_y",
  "object_26d0_timer", "object_26d0_code", "object_26d0_attr"
}, ",") .. "\n")

sprite_file:write(
  "frame,bank,slot,address,code,attr,x,y,decoded_code,color,flipx,flipy\n")

local object_header = {"frame", "record"}
for index = 0, 15 do
  object_header[#object_header + 1] = string.format("b%02d", index)
end
object_file:write(table.concat(object_header, ",") .. "\n")

marker_file:write(
  "marker_id,label,trigger_frame,capture_frame,burst_index,screenshot\n")
memory_file:write(
  "marker_id,label,capture_frame,burst_index,region,address,value\n")
auto_file:write("automatic_id,frame,reason,screenshot\n")

local function dump_state()
  state_file:write(string.format(
    "%d,%04x,%02x,%02x,%02x,%02x,%d,%d,%02x,%04x,%04x,%04x,%04x,%02x,%02x,%02x,%02x,%02x,%06x,%02x,%04x,%d,%02x,%02x,%02x\n",
    frame, maincpu.state["PC"].value,
    system_port:read(), player_port:read(),
    u8(0x2800), u8(0x20b0), u8(0x2646), u8(0x2644), u8(0x20b4),
    u16(0x20b5), u16(0x20b1), u16(0x2203), u16(0x20c2),
    u8(0x2208), u8(0x20bc), u8(0x20bd), u8(0x20be), u8(0x20bf),
    u8(0x20a0) * 65536 + u8(0x20a1) * 256 + u8(0x20a2),
    u8(0x26d0), u16(0x26d6), u8(0x26d4), u8(0x26d8),
    u8(0x26de), u8(0x26df)))
end

local function dump_sprites()
  for bank = 0, 1 do
    local base = 0x3800 + bank * 0x100
    for offset = 0, 0xfc, 4 do
      local address = base + offset
      local code = u8(address)
      local attr = u8(address + 1)
      local x = u8(address + 2)
      local y = u8(address + 3)
      sprite_file:write(string.format(
        "%d,%d,%d,%04x,%02x,%02x,%d,%d,%d,%d,%d,%d\n",
        frame, bank, offset / 4, address, code, attr, x, y,
        code + 8 * (attr & 0x20), attr & 0x0f,
        (attr & 0x40) ~= 0 and 1 or 0,
        (attr & 0x80) ~= 0 and 1 or 0))
    end
  end
end

local function dump_objects()
  for address = 0x2400, 0x27f0, 0x10 do
    local fields = {tostring(frame), string.format("%04x", address)}
    for offset = 0, 15 do
      fields[#fields + 1] = hex8(u8(address + offset))
    end
    object_file:write(table.concat(fields, ",") .. "\n")
  end
end

local function dump_marker_memory(id, label, capture_frame, burst_index)
  local regions = {
    {"work", 0x2000, 0x2fff},
    {"color", 0x3000, 0x33ff},
    {"video", 0x3400, 0x37ff},
    {"sprites", 0x3800, 0x39ff},
  }
  for _, region in ipairs(regions) do
    for address = region[2], region[3] do
      memory_file:write(string.format(
        "%d,%s,%d,%d,%s,%04x,%02x\n",
        id, label, capture_frame, burst_index, region[1], address, u8(address)))
    end
  end
end

local function capture_marker(item)
  local shot = string.format(
    "%s-marker-%03d-%s-frame-%06d-burst-%02d.png",
    prefix, item.id, item.label, frame, item.index)
  screen:snapshot(shot)
  marker_file:write(string.format(
    "%d,%s,%d,%d,%d,%s\n",
    item.id, item.label, item.trigger_frame, frame, item.index, shot))
  dump_marker_memory(item.id, item.label, frame, item.index)
  item.index = item.index + 1
  item.remaining = item.remaining - 1
end

local function start_marker(label)
  marker_id = marker_id + 1
  pending[#pending + 1] = {
    id = marker_id,
    label = label,
    trigger_frame = frame,
    index = 0,
    remaining = burst_frames,
  }
  machine:popmessage(string.format(
    "Captured %s marker %d", label, marker_id))
end

local function sprite_signature()
  local fields = {}
  for bank = 0, 1 do
    local base = 0x3800 + bank * 0x100
    for offset = 0, 0xfc, 4 do
      fields[#fields + 1] = string.char(u8(base + offset), u8(base + offset + 1))
    end
  end
  return table.concat(fields)
end

local function capture_new_signature()
  if auto_id >= auto_limit then return end
  local signature = sprite_signature()
  if seen_signatures[signature] then return end
  seen_signatures[signature] = true
  auto_id = auto_id + 1
  local shot = string.format("%s-auto-%04d-frame-%06d.png", prefix, auto_id, frame)
  screen:snapshot(shot)
  auto_file:write(string.format("%d,%d,new-code-attribute-layout,%s\n", auto_id, frame, shot))
end

local function pressed_once(name)
  local down = machine.input:seq_pressed(hotkeys[name])
  local rising = down and not key_was_down[name]
  key_was_down[name] = down
  return rising
end

local function write_complete(reason)
  if not active then return end
  active = false
  flush_all()
  state_file:close()
  sprite_file:close()
  object_file:close()
  marker_file:close()
  memory_file:close()
  auto_file:close()
  local complete = assert(io.open(prefix .. "-complete.txt", "w"))
  complete:write(string.format(
    "status=complete\nreason=%s\nlast_frame=%d\nmarkers=%d\nautomatic_screenshots=%d\nburst_frames=%d\n",
    reason, frame, marker_id, auto_id, burst_frames))
  complete:close()
  print("Manual Level 1 reference capture complete: " .. prefix)
  machine:popmessage("REFERENCE CAPTURE COMPLETE - you may quit MAME")
end

emu.register_frame_done(function()
  if not active then return end
  frame = frame + 1

  dump_state()
  dump_sprites()
  dump_objects()
  capture_new_signature()

  if pressed_once("extra_charlie") then start_marker("extra-charlie") end
  if pressed_once("hidden_coin") then start_marker("hidden-coin") end
  if pressed_once("goal_finish") then start_marker("goal-finish") end
  if pressed_once("score_bonus") then start_marker("score-bonus") end
  if pressed_once("generic") then start_marker("generic") end

  for index = #pending, 1, -1 do
    capture_marker(pending[index])
    if pending[index].remaining <= 0 then table.remove(pending, index) end
  end

  if (frame % 60) == 0 then flush_all() end

  if pressed_once("finish") then
    start_marker("session-finish")
    -- Capture the finish frame immediately, then close. Normal markers already
    -- preserve their complete bursts; X exists to make file finalization explicit.
    capture_marker(pending[#pending])
    write_complete("manual-hotkey")
  elseif test_frames > 0 and frame >= test_frames then
    write_complete("test-frame-limit")
    machine:exit()
  end
end, "manual_level1_reference_capture")

machine:popmessage(
  "LEVEL 1 CAPTURE ACTIVE | E extra | UP coin | G goal | B bonus | M generic | X finish")
