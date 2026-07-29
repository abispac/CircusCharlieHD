local frame = 0
local machine = manager.machine
local maincpu = machine.devices[":maincpu"]
local program = maincpu.spaces["program"]
local system = machine.ioport.ports[":SYSTEM"]
local player = machine.ioport.ports[":P1"]
local screen = machine.screens[":screen"]

local mode = os.getenv("CIRCUS_AUTOPLAY_MODE") or "attract"
local romset = os.getenv("CIRCUS_ROMSET") or "circusc"
local maximum_frames = tonumber(os.getenv("CIRCUS_AUTOPLAY_FRAMES") or "6000")
local output_prefix = "/tmp/" .. romset .. "-" .. mode
local trace = assert(io.open(output_prefix .. "-trace.csv", "w"))

local coin = system:field(0x01)
local start = system:field(0x08)
local left = player:field(0x01)
local right = player:field(0x02)
local jump = player:field(0x10)

local object_slots = {}
for address = 0x2400, 0x27f0, 0x10 do
  table.insert(object_slots, address)
end

local object_offsets = {}
for offset = 0x00, 0x0f do
  table.insert(object_offsets, offset)
end

local function set_button(field, pressed)
  if pressed then
    field:set_value(1)
  else
    field:clear_value()
  end
end

local function active(first, last)
  return frame >= first and frame < last
end

local function scripted_inputs()
  set_button(coin, active(1040, 1048))
  set_button(start, active(1110, 1118))
  set_button(left, false)
  set_button(right, false)
  set_button(jump, false)

  if mode == "stage1_probe" then
    set_button(left, active(1140, 1210))
    set_button(jump, active(1220, 1228))
    set_button(right, active(1320, 2700))
    set_button(jump,
      active(1560, 1564) or
      active(1740, 1752) or
      active(1940, 1944) or
      active(2140, 2152) or
      active(2380, 2384))
  end
end

local function write_header()
  local columns = {
    "frame", "pc", "system", "p1",
    "ram_2003", "ram_2031", "ram_2032", "ram_2033",
    "ram_2201", "ram_2203", "ram_2204", "ram_220a",
    "ram_241a", "ram_241b", "ram_28d3", "ram_28f0"
  }

  for _, base in ipairs(object_slots) do
    for _, offset in ipairs(object_offsets) do
      table.insert(columns, string.format("obj_%04x_%02x", base, offset))
    end
  end

  trace:write(table.concat(columns, ","), "\n")
end

local function read_pc()
  local state = maincpu.state["PC"]
  if state then
    return state.value
  end
  return 0
end

local function write_trace()
  local values = {
    frame,
    string.format("%04x", read_pc()),
    string.format("%02x", system:read()),
    string.format("%02x", player:read()),
    string.format("%02x", program:read_u8(0x2003)),
    string.format("%02x", program:read_u8(0x2031)),
    string.format("%02x", program:read_u8(0x2032)),
    string.format("%02x", program:read_u8(0x2033)),
    string.format("%02x", program:read_u8(0x2201)),
    string.format("%02x", program:read_u8(0x2203)),
    string.format("%02x", program:read_u8(0x2204)),
    string.format("%02x", program:read_u8(0x220a)),
    string.format("%02x", program:read_u8(0x241a)),
    string.format("%02x", program:read_u8(0x241b)),
    string.format("%02x", program:read_u8(0x28d3)),
    string.format("%02x", program:read_u8(0x28f0))
  }

  for _, base in ipairs(object_slots) do
    for _, offset in ipairs(object_offsets) do
      table.insert(values, string.format("%02x", program:read_u8(base + offset)))
    end
  end

  trace:write(table.concat(values, ","), "\n")
end

local function capture()
  local path = string.format("%s-frame-%05d.png", output_prefix, frame)
  local failure = screen:snapshot(path)
  if failure then
    print("Snapshot failed: " .. tostring(failure))
  end
end

local function finish()
  set_button(coin, false)
  set_button(start, false)
  set_button(left, false)
  set_button(right, false)
  set_button(jump, false)
  trace:flush()
  trace:close()
  print(string.format(
    "Autoplay capture complete: mode=%s frames=%d trace=%s-trace.csv",
    mode, frame, output_prefix))
  machine:exit()
end

write_header()

emu.register_frame_done(function()
  frame = frame + 1

  if mode ~= "attract" then
    scripted_inputs()
  end

  write_trace()

  if frame == 1 or frame % 120 == 0 then
    capture()
  end

  if frame >= maximum_frames then
    finish()
  end
end, "frame")
