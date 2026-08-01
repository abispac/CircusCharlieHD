local machine = manager.machine
local maincpu = machine.devices[":maincpu"]
local program = maincpu.spaces["program"]
local frame = 0
local output_path = os.getenv("CIRCUS_SOUND_TRACE") or
    "/tmp/circusc4-manual-sound-trace.csv"
local output = assert(io.open(output_path, "w"))
local closed = false

output:write("frame,pc,sound_id_hex,sound_id_decimal\n")

local sound_tap = program:install_write_tap(
  0x0800, 0x0800, "circusc_manual_sound_trace",
  function(_, data, _)
    local pc = maincpu.state["PC"]
    local pc_value = pc and pc.value or 0
    output:write(string.format("%d,%04x,0x%02x,%d\n",
      frame, pc_value, data, data))
    output:flush()
    print(string.format("Sound 0x%02x at frame %d", data, frame))
    return data
  end)

local function close_trace()
  if closed then
    return
  end
  closed = true
  output:flush()
  output:close()
  sound_tap:remove()
  print("Manual sound trace saved to " .. output_path)
end

emu.register_frame_done(function()
  frame = frame + 1
end, "frame")

local stop_notifier = emu.add_machine_stop_notifier(close_trace)
