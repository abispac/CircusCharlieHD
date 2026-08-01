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
local snapshot_first = tonumber(os.getenv("CIRCUS_SNAPSHOT_FIRST") or "1")
local snapshot_last = tonumber(os.getenv("CIRCUS_SNAPSHOT_LAST") or tostring(maximum_frames))
local snapshot_every = tonumber(os.getenv("CIRCUS_SNAPSHOT_EVERY") or "120")
local output_prefix = "/tmp/" .. romset .. "-" .. mode
local trace = assert(io.open(output_prefix .. "-trace.csv", "w"))
local sound_trace = assert(io.open(output_prefix .. "-sound.csv", "w"))
sound_trace:write("frame,pc,sound_id_hex,sound_id_decimal\n")

-- Preserve every real game-to-audio-CPU command. This maps each effect back
-- to its gameplay frame and avoids assigning sounds by ear alone.
local sound_tap = program:install_write_tap(
  0x0800, 0x0800, "circusc_sound_commands",
  function(_, data, _)
    sound_trace:write(string.format("%d,%04x,0x%02x,%d\n",
      frame, maincpu.state["PC"].value, data, data))
    sound_trace:flush()
    return data
  end)

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

  if string.sub(mode, 1, 7) == "stage1_" then
    set_button(left, active(1140, 1210))
    set_button(right, active(1320, 1560))
    set_button(jump, active(1560, 1564))
    if mode == "stage1_crash" then
      set_button(right, active(1320, 2050))
      set_button(jump, false)
    elseif mode == "stage1_probe" then
      set_button(right, active(1320, 2700))
      set_button(jump,
        active(1560, 1564) or
        active(1740, 1752) or
        active(1940, 1944) or
        active(2140, 2152) or
        active(2380, 2384))
    elseif mode == "stage1_jump_tap" then
      set_button(right, active(1320, 1560) or active(1840, 2050))
      set_button(jump, active(1560, 1564) or active(1860, 1862))
    elseif mode == "stage1_jump_hold" then
      set_button(right, active(1320, 1560) or active(1840, 2050))
      set_button(jump, active(1560, 1564) or active(1860, 1884))
    end
  elseif string.sub(mode, 1, 7) == "stage2_" then
    -- Move once from Event 1 to Event 2 and confirm. The crash variant then
    -- walks into the first monkey; the idle variant provides a matched music
    -- recording so the contact effect can be isolated by subtraction.
    set_button(right, active(1140, 1148))
    if mode == "stage2_crash" then
      set_button(right, active(1140, 1148) or active(1750, 2800))
    end
    set_button(jump, active(1560, 1564))
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
  sound_trace:flush()
  sound_trace:close()
  sound_tap:remove()
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

  if frame >= snapshot_first and frame <= snapshot_last and
      ((frame - snapshot_first) % snapshot_every == 0) then
    capture()
  end

  if frame >= maximum_frames then
    finish()
  end
end, "frame")
