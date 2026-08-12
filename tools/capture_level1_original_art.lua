local frame = 0
local machine = manager.machine
local maincpu = machine.devices[":maincpu"]
local program = maincpu.spaces["program"]
local screen = machine.screens[":screen"]
local system = machine.ioport.ports[":SYSTEM"]
local player = machine.ioport.ports[":P1"]

local output_dir = os.getenv("CIRCUS_ART_CAPTURE_DIR") or
  "/tmp/circusc4-level1-original-art"
local trace = assert(io.open(output_dir .. "-hardware.csv", "w"))

local coin = system:field(0x01)
local start = system:field(0x08)
local left = player:field(0x01)
local right = player:field(0x02)
local jump = player:field(0x10)

local capture_frames = {
  [1326] = "run-a-00",
  [1328] = "run-b-00",
  [1336] = "run-c-00",
  [1347] = "airborne-00",
  [1350] = "airborne-03",
  [1360] = "airborne-13",
  [1375] = "airborne-28",
  [1395] = "hoop-crossing",
  [1410] = "landed-run-a",
  [1418] = "landed-run-b",
  [1425] = "landed-run-c"
}

local function active(first, last)
  return frame >= first and frame < last
end

local function set_button(field, pressed)
  if pressed then field:set_value(1) else field:clear_value() end
end

local function apply_inputs()
  set_button(coin, active(1040, 1048))
  set_button(start, active(1110, 1118))
  set_button(left, false)
  set_button(right, active(1320, 1432))
  set_button(jump, active(1346, 1350))
end

local function u8(address)
  return program:read_u8(address)
end

trace:write("frame,label,bank,slot,address,code,attr,x,y,decoded_code,color,flipx,flipy\n")

local function dump_hardware(label)
  for bank = 0, 1 do
    local base = 0x3800 + bank * 0x100
    for offset = 0, 0xfc, 4 do
      local address = base + offset
      local code = u8(address)
      local attr = u8(address + 1)
      local x = u8(address + 2)
      local y = u8(address + 3)
      if code ~= 0 or attr ~= 0 or x ~= 0 or y ~= 0 then
        trace:write(string.format(
          "%d,%s,%d,%d,%04x,%02x,%02x,%d,%d,%d,%d,%d,%d\n",
          frame, label, bank, offset / 4, address, code, attr, x, y,
          code + 8 * (attr & 0x20), attr & 0x0f,
          (attr & 0x40) ~= 0 and 1 or 0,
          (attr & 0x80) ~= 0 and 1 or 0))
      end
    end
  end
end

local function finish()
  set_button(coin, false)
  set_button(start, false)
  set_button(left, false)
  set_button(right, false)
  set_button(jump, false)
  trace:close()
  print("Level 1 original-art capture complete: " .. output_dir)
  machine:exit()
end

emu.register_frame_done(function()
  frame = frame + 1
  apply_inputs()
  local label = capture_frames[frame]
  if label ~= nil then
    dump_hardware(label)
    screen:snapshot(string.format("%s-frame-%04d-%s.png",
      output_dir, frame, label))
  end
  if frame >= 1430 then finish() end
end, "frame")
