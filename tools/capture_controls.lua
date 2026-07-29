local frame = 0
local machine = manager.machine
local system = machine.ioport.ports[":SYSTEM"]
local player = machine.ioport.ports[":P1"]
local screen = machine.screens[":screen"]

local coin = system:field(0x01)
local start = system:field(0x08)
local left = player:field(0x01)
local right = player:field(0x02)
local jump = player:field(0x10)

local function pulse(field, first, last)
  if frame == first then
    field:set_value(1)
  elseif frame == last then
    field:clear_value()
  end
end

local function capture(name)
  local failure = screen:snapshot("/tmp/" .. name .. ".png")
  if failure then
    print("Snapshot failed: " .. tostring(failure))
  end
end

emu.register_frame_done(function()
  frame = frame + 1

  pulse(coin, 1040, 1048)
  pulse(start, 1110, 1118)
  pulse(jump, 1220, 1228)

  pulse(jump, 1540, 1542)
  pulse(jump, 1720, 1745)

  if frame == 1900 then right:set_value(1) end
  if frame == 1960 then right:clear_value() end
  if frame == 1970 then left:set_value(1) end
  if frame == 2030 then left:clear_value() end

  if frame == 1460 or frame == 1520 or frame == 1900 or
      frame == 1960 or frame == 1970 or frame == 2030 then
    capture(string.format("circusc-control-%04d", frame))
  end

  if (frame >= 1530 and frame <= 1640 and frame % 5 == 0) or
      (frame >= 1710 and frame <= 1840 and frame % 5 == 0) then
    capture(string.format("circusc-control-%04d", frame))
  end

  if frame >= 2060 then
    machine:exit()
  end
end, "frame")
