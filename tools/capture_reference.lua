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

  if frame == 1320 then right:set_value(1) end
  if frame == 2100 then right:clear_value() end

  pulse(jump, 1620, 1628)
  pulse(jump, 1800, 1808)

  if frame == 1180 then capture("circusc-stage-select") end
  if frame == 1300 then capture("circusc-stage1-start") end
  if frame >= 1580 and frame <= 1760 and frame % 5 == 0 then
    capture(string.format("circusc-stage1-frame-%04d", frame))
  end

  if frame >= 2120 then
    machine:exit()
  end
end, "frame")
