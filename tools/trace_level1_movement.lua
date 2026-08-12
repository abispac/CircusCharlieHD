local frame = 0
local machine = manager.machine
local maincpu = machine.devices[":maincpu"]
local program = maincpu.spaces["program"]
local system = machine.ioport.ports[":SYSTEM"]
local player = machine.ioport.ports[":P1"]

local mode = os.getenv("CIRCUS_LEVEL1_TRACE") or "hold-right"
local romset = os.getenv("CIRCUS_ROMSET") or "circusc4"
local output = "/tmp/" .. romset .. "-level1-" .. mode
local trace = assert(io.open(output .. ".csv", "w"))
local writes = assert(io.open(output .. "-writes.csv", "w"))

local coin = system:field(0x01)
local start = system:field(0x08)
local left = player:field(0x01)
local right = player:field(0x02)
local jump = player:field(0x10)

local function active(first, last)
  return frame >= first and frame < last
end

local jump_start = 1330

local function set_button(field, pressed)
  if pressed then field:set_value(1) else field:clear_value() end
end

local function apply_inputs()
  set_button(coin, active(1040, 1048))
  set_button(start, active(1110, 1118))
  set_button(left, false)
  set_button(right, false)
  set_button(jump, false)

  if mode == "hold-right" then
    set_button(right, active(1320, 1470))
  elseif mode == "right-release" then
    set_button(right, active(1320, 1380))
  elseif mode == "right-left" then
    set_button(right, active(1320, 1380))
    set_button(left, active(1380, 1440))
  elseif mode == "forward-jump" then
    set_button(right, active(1320, 1470))
    set_button(jump, active(jump_start, jump_start + 4))
  else
    error("Unknown CIRCUS_LEVEL1_TRACE mode: " .. mode)
  end
end

local function hex8(address)
  return string.format("%02x", program:read_u8(address))
end

local function pc()
  local state = maincpu.state["PC"]
  return state and state.value or 0
end

trace:write(table.concat({
  "frame", "pc", "p1", "input_left", "input_right", "input_jump",
  "ram_2032", "ram_20b1", "ram_20b2", "ram_2203", "ram_2204",
  "ram_220a", "ram_241a",
  "ram_241b", "ram_28d3", "ram_28f0", "rider_25f0_y",
  "rider_25f0_x", "rider_2600_y", "rider_2600_x"
}, ","), "\n")
writes:write("frame,pc,address,value\n")

local scroll_write_tap = program:install_write_tap(
  0x2203, 0x2204, "circusc_level1_scroll_writes",
  function(offset, data, _)
    writes:write(string.format("%d,%04x,%04x,%02x\n",
      frame, pc(), offset, data))
    return data
  end)

local delta_write_tap = program:install_write_tap(
  0x20b1, 0x20b2, "circusc_level1_delta_writes",
  function(offset, data, _)
    writes:write(string.format("%d,%04x,%04x,%02x\n",
      frame, pc(), offset, data))
    return data
  end)

local function finish()
  set_button(coin, false)
  set_button(start, false)
  set_button(left, false)
  set_button(right, false)
  set_button(jump, false)
  scroll_write_tap:remove()
  delta_write_tap:remove()
  trace:close()
  writes:close()
  print("Level 1 movement trace complete: " .. output .. ".csv")
  machine:exit()
end

emu.register_frame_done(function()
  frame = frame + 1
  apply_inputs()

  trace:write(table.concat({
    frame,
    string.format("%04x", pc()),
    string.format("%02x", player:read()),
    active(1380, 1440) and mode == "right-left" and 1 or 0,
    ((active(1320, 1470) and (mode == "hold-right" or
      mode == "forward-jump")) or
      (active(1320, 1380) and (mode == "right-release" or
      mode == "right-left"))) and 1 or 0,
    active(jump_start, jump_start + 4) and mode == "forward-jump" and 1 or 0,
    hex8(0x2032), hex8(0x20b1), hex8(0x20b2), hex8(0x2203),
    hex8(0x2204), hex8(0x220a),
    hex8(0x241a), hex8(0x241b), hex8(0x28d3), hex8(0x28f0),
    hex8(0x25f4), hex8(0x25f6), hex8(0x2604), hex8(0x2606)
  }, ","), "\n")

  if frame >= 1470 then finish() end
end, "frame")
