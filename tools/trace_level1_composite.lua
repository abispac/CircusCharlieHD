local frame = 0
local machine = manager.machine
local maincpu = machine.devices[":maincpu"]
local program = maincpu.spaces["program"]
local system = machine.ioport.ports[":SYSTEM"]
local player = machine.ioport.ports[":P1"]

local romset = os.getenv("CIRCUS_ROMSET") or "circusc4"
local output = "/tmp/" .. romset .. "-level1-composite-1375-1410"
local state_trace = assert(io.open(output .. "-state.csv", "w"))
local object_trace = assert(io.open(output .. "-objects.csv", "w"))
local sprite_trace = assert(io.open(output .. "-sprites.csv", "w"))
local writes = assert(io.open(output .. "-writes.csv", "w"))
local collision_trace = assert(io.open(output .. "-collision.csv", "w"))

local coin = system:field(0x01)
local start = system:field(0x08)
local left = player:field(0x01)
local right = player:field(0x02)
local jump = player:field(0x10)

local first_focus_frame = 1375
local last_focus_frame = 1410

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

local function pc()
  local state = maincpu.state["PC"]
  return state and state.value or 0
end

local function u8(address)
  return program:read_u8(address)
end

local function h8(address)
  return string.format("%02x", u8(address))
end

local function in_focus()
  return frame >= first_focus_frame and frame <= last_focus_frame
end

state_trace:write(table.concat({
  "frame", "pc", "p1", "scroll_port", "ram_2031", "ram_2032",
  "ram_2033", "ram_20b0", "ram_20b1", "ram_20b2", "ram_20b3",
  "ram_20b4", "ram_20b5", "ram_20b6", "ram_2203", "ram_2204",
  "ram_2205", "ram_220a", "ram_2243", "ram_2244", "ram_2245",
  "ram_2246", "ram_241a", "ram_241b", "ram_28d3", "ram_28f0"
}, ","), "\n")
object_trace:write("frame,slot,b00,b01,b02,b03,y_hi,y_lo,x_hi,x_lo,b08,b09,b0a,b0b,b0c,b0d,code,attr\n")
sprite_trace:write("frame,bank,slot,code,attr,x,y,decoded_code,color,flipx,flipy\n")
writes:write("frame,pc,address,value,region\n")
collision_trace:write("frame,pc,u,rider_y,object_y,object_x,object_code,object_attr\n")

local write_taps = {}
local function install_write_trace(first, last, name, region)
  local tap = program:install_write_tap(first, last, name,
    function(offset, data, _)
      if in_focus() then
        writes:write(string.format("%d,%04x,%04x,%02x,%s\n",
          frame, pc(), offset, data, region))
      end
      return data
    end)
  table.insert(write_taps, tap)
end

install_write_trace(0x2000, 0x2fff, "circusc_composite_state_writes", "state")
install_write_trace(0x3800, 0x39ff, "circusc_composite_sprite_writes", "sprite")

local failure_write_tap = program:install_write_tap(
  0x264e, 0x264e, "circusc_composite_failure_entry",
  function(offset, data, _)
    if in_focus() then
      local u_state = maincpu.state["U"]
      local u = u_state and u_state.value or 0
      collision_trace:write(string.format(
        "%d,%04x,%04x,%02x,%02x,%02x,%02x,%02x\n",
        frame, pc(), u, u8(0x2644), u8(u + 4), u8(u + 6),
        u8(u + 14), u8(u + 15)))
    end
    return data
  end)

local function trace_state()
  local addresses = {
    0x2031, 0x2032, 0x2033,
    0x20b0, 0x20b1, 0x20b2, 0x20b3, 0x20b4, 0x20b5, 0x20b6,
    0x2203, 0x2204, 0x2205, 0x220a,
    0x2243, 0x2244, 0x2245, 0x2246,
    0x241a, 0x241b, 0x28d3, 0x28f0
  }
  local row = {
    frame, string.format("%04x", pc()), string.format("%02x", player:read()),
    h8(0x1c00)
  }
  for _, address in ipairs(addresses) do table.insert(row, h8(address)) end
  state_trace:write(table.concat(row, ","), "\n")
end

local function trace_objects()
  local addresses = {
    0x25f0, 0x2600, 0x2610, 0x2620, 0x2630, 0x2640,
    0x26d0, 0x2700, 0x2730, 0x2760
  }
  for _, address in ipairs(addresses) do
    local row = {frame, string.format("%04x", address)}
    for offset = 0, 15 do table.insert(row, h8(address + offset)) end
    object_trace:write(table.concat(row, ","), "\n")
  end
end

local function trace_sprites()
  for bank = 0, 1 do
    local base = 0x3800 + bank * 0x100
    for offset = 0, 0xfc, 4 do
      local code = u8(base + offset)
      local attr = u8(base + offset + 1)
      local x = u8(base + offset + 2)
      local y = u8(base + offset + 3)
      if code ~= 0 or attr ~= 0 or x ~= 0 or y ~= 0 then
        sprite_trace:write(string.format(
          "%d,%d,%02d,%02x,%02x,%d,%d,%d,%d,%d,%d\n",
          frame, bank, offset / 4, code, attr, x, y,
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
  for _, tap in ipairs(write_taps) do tap:remove() end
  failure_write_tap:remove()
  state_trace:close()
  object_trace:close()
  sprite_trace:close()
  writes:close()
  collision_trace:close()
  print("Level 1 composite trace complete: " .. output)
  machine:exit()
end

emu.register_frame_done(function()
  frame = frame + 1
  apply_inputs()
  if in_focus() then
    trace_state()
    trace_objects()
    trace_sprites()
  end
  if frame > last_focus_frame then finish() end
end, "frame")
