local frame = 0
local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local program = cpu.spaces["program"]
local system = machine.ioport.ports[":SYSTEM"]
local player = machine.ioport.ports[":P1"]

local mode = os.getenv("CIRCUS_START_MODE") or "neutral"
local last_frame = tonumber(os.getenv("CIRCUS_TRACE_LAST") or "1490")
local input_first = tonumber(os.getenv("CIRCUS_START_INPUT_FIRST") or "1310")
local output = "/tmp/circusc4-level1-start-" .. mode .. ".csv"
local trace = assert(io.open(output, "w"))

local coin = system:field(0x01)
local start = system:field(0x08)
local left = player:field(0x01)
local right = player:field(0x02)
local jump = player:field(0x10)

local function active(first, last)
  return frame >= first and frame < last
end

local function set_button(field, pressed)
  if pressed then field:set_value(1) else field:clear_value() end
end

local function controls()
  local move_left = false
  local move_right = false
  if mode == "left" then
    move_left = frame >= input_first
  elseif mode == "right" then
    move_right = frame >= input_first
  elseif mode == "right-left" then
    move_right = active(input_first, input_first + 60)
    move_left = frame >= 1370
  elseif mode == "right-return-left" then
    move_right = active(input_first, input_first + 25)
    move_left = frame >= 1335
  end
  return move_left, move_right
end

local function apply_inputs()
  set_button(coin, active(1040, 1048))
  set_button(start, active(1110, 1118))
  local move_left, move_right = controls()
  set_button(left, move_left)
  set_button(right, move_right)
  set_button(jump, false)
end

local function u8(address)
  return program:read_u8(address)
end

local function x16(base)
  return u8(base + 6) * 0x100 + u8(base + 7)
end

trace:write(table.concat({
  "frame", "mode", "p1", "scene", "airborne", "movement_hi",
  "movement_lo", "stored_direction_hi", "stored_direction_lo",
  "course_scroll_hi", "course_scroll_lo", "activation_hi",
  "activation_lo", "course_index", "course_state", "active_count",
  "small_2400_x", "hoop_26d0_status", "hoop_26d0_x"
}, ","), "\n")

local function hex8(value)
  return string.format("%02x", value)
end

local function hex16(value)
  return string.format("%04x", value)
end

local function finish()
  set_button(coin, false)
  set_button(start, false)
  set_button(left, false)
  set_button(right, false)
  set_button(jump, false)
  trace:close()
  print("Level 1 start-boundary trace complete: " .. output)
  machine:exit()
end

emu.register_frame_done(function()
  frame = frame + 1
  apply_inputs()
  if frame >= 1280 then
    trace:write(table.concat({
      frame, mode, hex8(player:read()), hex8(u8(0x2800)),
      hex8(u8(0x20b0)), hex8(u8(0x20b1)), hex8(u8(0x20b2)),
      hex8(u8(0x2243)), hex8(u8(0x2244)), hex8(u8(0x2203)),
      hex8(u8(0x2204)), hex8(u8(0x20c2)), hex8(u8(0x20c3)),
      hex8(u8(0x2208)), hex8(u8(0x20bc)), hex8(u8(0x220a)),
      hex16(x16(0x2400)), hex8(u8(0x26d0)), hex16(x16(0x26d0))
    }, ","), "\n")
  end
  if frame >= last_frame then finish() end
end, "frame")
