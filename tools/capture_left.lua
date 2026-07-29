local frame = 0
local machine = manager.machine
local system = machine.ioport.ports[":SYSTEM"]
local player = machine.ioport.ports[":P1"]
local screen = machine.screens[":screen"]

local coin = system:field(0x01)
local start = system:field(0x08)
local left = player:field(0x01)
local jump = player:field(0x10)

local function pulse(field, first, last)
  if frame == first then
    field:set_value(1)
  elseif frame == last then
    field:clear_value()
  end
end

local function capture(name)
  screen:snapshot("/tmp/" .. name .. ".png")
end

emu.register_frame_done(function()
  frame = frame + 1
  pulse(coin, 1040, 1048)
  pulse(start, 1110, 1118)
  pulse(jump, 1220, 1228)

  if frame == 1320 then left:set_value(1) end
  if frame == 1540 then left:clear_value() end

  if frame == 1400 or frame == 1460 or frame == 1520 or frame == 1560 then
    capture(string.format("circusc-left-%04d", frame))
  end

  if frame >= 1580 then
    machine:exit()
  end
end, "frame")
