local frame = 0
local machine = manager.machine
local maincpu = machine.devices[":maincpu"]
local program = maincpu.spaces["program"]
local system = machine.ioport.ports[":SYSTEM"]
local player = machine.ioport.ports[":P1"]

local mode = os.getenv("CIRCUS_LEVEL1_AIR_MODE") or "right-hold"
local romset = os.getenv("CIRCUS_ROMSET") or "circusc4"
local output = "/tmp/" .. romset .. "-level1-air-" .. mode
local trace = assert(io.open(output .. ".csv", "w"))
local writes = assert(io.open(output .. "-writes.csv", "w"))

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

local function inputs_for_frame()
  local input_left = false
  local input_right = false
  local input_jump = false

  if mode == "right-hold" then
    input_right = active(1320, 1435)
    input_jump = active(1346, 1350)
  elseif mode == "right-left-air" then
    input_right = active(1320, 1349)
    input_left = active(1349, 1435)
    input_jump = active(1346, 1350)
  elseif mode == "right-release-air" then
    input_right = active(1320, 1349)
    input_jump = active(1346, 1350)
  elseif mode == "left-right-air" then
    input_left = active(1320, 1333)
    input_right = active(1333, 1420)
    input_jump = active(1330, 1334)
  elseif mode == "left-release-air" then
    input_left = active(1320, 1333)
    input_jump = active(1330, 1334)
  elseif mode == "neutral-jump" then
    input_jump = active(1330, 1334)
  else
    error("Unknown CIRCUS_LEVEL1_AIR_MODE: " .. mode)
  end
  return input_left, input_right, input_jump
end

local function apply_inputs()
  set_button(coin, active(1040, 1048))
  set_button(start, active(1110, 1118))
  local input_left, input_right, input_jump = inputs_for_frame()
  set_button(left, input_left)
  set_button(right, input_right)
  set_button(jump, input_jump)
end

local function hex8(address)
  return string.format("%02x", program:read_u8(address))
end

local function pc()
  local state = maincpu.state["PC"]
  return state and state.value or 0
end

trace:write(table.concat({
  "frame", "pc", "p1", "input_left", "input_right", "input_jump",
  "ram_20b0", "ram_20b1", "ram_20b2", "ram_20b3", "ram_20b4",
  "ram_20b5", "ram_20b6", "ram_20b7", "ram_2203", "ram_2204",
  "ram_2243", "ram_2244", "ram_2246", "ram_241a", "ram_241b",
  "ram_28d3", "ram_28f0", "rider_y", "rider_x"
}, ","), "\n")
writes:write("frame,pc,address,value\n")

local taps = {}
local function add_tap(first, last, name)
  table.insert(taps, program:install_write_tap(first, last, name,
    function(offset, data, _)
      writes:write(string.format("%d,%04x,%04x,%02x\n",
        frame, pc(), offset, data))
      writes:flush()
      return data
    end))
end
add_tap(0x20b0, 0x20b7, "circusc_air_direct_page")
add_tap(0x2203, 0x2204, "circusc_air_scroll")
add_tap(0x2243, 0x2246, "circusc_air_latch")

local function finish()
  set_button(coin, false)
  set_button(start, false)
  set_button(left, false)
  set_button(right, false)
  set_button(jump, false)
  for _, tap in ipairs(taps) do tap:remove() end
  trace:close()
  writes:close()
  print("Level 1 airborne-direction trace complete: " .. output .. ".csv")
  machine:exit()
end

emu.register_frame_done(function()
  frame = frame + 1
  apply_inputs()
  local input_left, input_right, input_jump = inputs_for_frame()
  trace:write(table.concat({
    frame, string.format("%04x", pc()), string.format("%02x", player:read()),
    input_left and 1 or 0, input_right and 1 or 0, input_jump and 1 or 0,
    hex8(0x20b0), hex8(0x20b1), hex8(0x20b2), hex8(0x20b3),
    hex8(0x20b4), hex8(0x20b5), hex8(0x20b6), hex8(0x20b7),
    hex8(0x2203), hex8(0x2204), hex8(0x2243), hex8(0x2244),
    hex8(0x2246), hex8(0x241a), hex8(0x241b), hex8(0x28d3),
    hex8(0x28f0), hex8(0x25f4), hex8(0x25f6)
  }, ","), "\n")
  trace:flush()
  if frame >= 1440 then finish() end
end, "frame")
