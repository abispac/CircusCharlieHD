local frame = 0
local machine = manager.machine
local debugger = machine.debugger
local cpu = machine.devices[":maincpu"]
local system = machine.ioport.ports[":SYSTEM"]
local player = machine.ioport.ports[":P1"]

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
  set_button(coin, active(1040, 1048))
  set_button(start, active(1110, 1118))
  set_button(left, false)
  set_button(right, active(1320, 1470))
  set_button(jump, active(1330, 1334))
end

debugger.visible_cpu = cpu
cpu.debug:go()

emu.register_frame_done(function()
  frame = frame + 1
  apply_inputs()
  if frame == 1381 then
    debugger:command([[
trace /private/tmp/circusc4-level1-large-hoop-collision.tr,,noloop,{tracelog "F=%d PC=%04X A=%02X B=%02X X=%04X Y=%04X U=%04X S=%04X CC=%02X RY=%02X O0=%02X OY=%02X OX=%02X OL=%02X OC=%02X OA=%02X TRACK=%04X JS=%02X JH=%02X JL=%02X SDH=%02X SDL=%02X\n",frame,pc,a,b,x,y,u,s,cc,b@2644,b@u,b@(u+4),b@(u+6),b@(u+7),b@(u+14),b@(u+15),w@20bf,b@20b4,b@20b5,b@20b6,b@20b1,b@20b2}]]
    )
  elseif frame == 1391 then
    debugger:command("trace off")
    debugger:command("traceflush")
    set_button(coin, false)
    set_button(start, false)
    set_button(right, false)
    set_button(jump, false)
    machine:exit()
  end
end, "frame")
