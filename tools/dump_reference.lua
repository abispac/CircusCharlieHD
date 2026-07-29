local machine = manager.machine
local maincpu = machine.devices[":maincpu"]
local completed = false

emu.register_frame_done(function()
  if completed then
    return
  end
  completed = true

  print("Main CPU: " .. tostring(maincpu.name))
  for name, _ in pairs(maincpu.spaces) do
    print("Address space: " .. tostring(name))
  end

  local debugger = machine.debugger
  if debugger then
    debugger.visible_cpu = maincpu
    debugger:command("dasm /tmp/circusc-maincpu.asm,6000,a000,1")
    debugger:command("memdump /tmp/circusc-memory-map.txt")
  else
    print("MAME debugger manager is unavailable")
  end

  machine:exit()
end, "frame")
