-- Dedicated hardware capture for unresolved original Level 1 artwork.
--
-- This deliberately does not patch emulation state. It records both buffered
-- sprite banks and periodic lossless screenshots from an unmodified circusc4
-- run so reconstructed composites can be proven against arcade output.

local frame = 0
local machine = manager.machine
local maincpu = machine.devices[":maincpu"]
local program = maincpu.spaces["program"]
local screen = machine.screens[":screen"]
local system = machine.ioport.ports[":SYSTEM"]
local player = machine.ioport.ports[":P1"]

local output_dir = os.getenv("CIRCUS_UNRESOLVED_CAPTURE_DIR") or
  "/tmp/circusc4-level1-unresolved"
local end_frame = tonumber(os.getenv("CIRCUS_UNRESOLVED_END_FRAME") or "12000")
local first_frame = tonumber(os.getenv("CIRCUS_UNRESOLVED_FIRST_FRAME") or "900")
local snapshot_step = tonumber(os.getenv("CIRCUS_UNRESOLVED_SNAPSHOT_STEP") or "10")
local mode = os.getenv("CIRCUS_UNRESOLVED_MODE") or "attract"
local trace = assert(io.open(output_dir .. "-hardware.csv", "w"))

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

local function apply_inputs()
  if mode == "forward-probe" then
    set_button(coin, active(1040, 1048))
    set_button(start, active(1110, 1118))
    set_button(left, false)
    set_button(right, active(1320, end_frame - 2))
    set_button(jump,
      active(1346, 1350) or active(1518, 1522) or
      active(1690, 1694) or active(1862, 1866) or
      active(2034, 2038) or active(2206, 2210) or
      active(2378, 2382) or active(2550, 2554))
  else
    set_button(coin, false)
    set_button(start, false)
    set_button(left, false)
    set_button(right, false)
    set_button(jump, false)
  end
end

local function u8(address)
  return program:read_u8(address)
end

trace:write("frame,bank,slot,address,code,attr,x,y,decoded_code,color,flipx,flipy\n")

local function dump_hardware()
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
          "%d,%d,%d,%04x,%02x,%02x,%d,%d,%d,%d,%d,%d\n",
          frame, bank, offset / 4, address, code, attr, x, y,
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
  print("Level 1 unresolved-art capture complete: " .. output_dir)
  machine:exit()
end

emu.register_frame_done(function()
  frame = frame + 1
  apply_inputs()
  if frame >= first_frame then
    dump_hardware()
    if ((frame - first_frame) % snapshot_step) == 0 then
      screen:snapshot(string.format("%s-frame-%05d.png", output_dir, frame))
    end
  end
  if frame >= end_frame then finish() end
end, "frame")
