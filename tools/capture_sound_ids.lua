local machine = manager.machine
local maincpu = machine.devices[":maincpu"]
local program = maincpu.spaces["program"]

local first_id = tonumber(os.getenv("CIRCUS_SOUND_FIRST") or "0")
local last_id = tonumber(os.getenv("CIRCUS_SOUND_LAST") or "127")
local boot_frames = tonumber(os.getenv("CIRCUS_SOUND_BOOT_FRAMES") or "300")
local hold_frames = tonumber(os.getenv("CIRCUS_SOUND_HOLD_FRAMES") or "180")
local output_path = os.getenv("CIRCUS_SOUND_LOG") or
    "/tmp/circusc-sound-ids.csv"
local output = assert(io.open(output_path, "w"))
local frame = 0
local sound_id = first_id
local complete = false

output:write("sound_id_hex,sound_id_decimal,trigger_frame\n")

local function trigger_sound(command)
  -- The official board map writes sound data at 0x0800 and pulses the Z80
  -- interrupt at 0x0c00. Using the mapped main-CPU handlers preserves the
  -- original latch, IRQ, Z80 program, tone chips, DAC, and discrete mixer.
  program:write_u8(0x0800, command)
  program:write_u8(0x0c00, 0)
  output:write(string.format("0x%02x,%d,%d\n", command, command, frame))
  output:flush()
  print(string.format("Circus Charlie sound command 0x%02x at frame %d",
      command, frame))
end

emu.register_frame_done(function()
  if complete then
    return
  end

  frame = frame + 1
  if frame >= boot_frames and
      ((frame - boot_frames) % hold_frames == 0) then
    if sound_id <= last_id then
      trigger_sound(sound_id)
      sound_id = sound_id + 1
    else
      complete = true
      output:close()
      machine:exit()
    end
  end
end, "frame")
