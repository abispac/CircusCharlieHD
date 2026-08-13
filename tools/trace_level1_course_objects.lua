local frame = 0
local machine = manager.machine
local maincpu = machine.devices[":maincpu"]
local program = maincpu.spaces["program"]
local system = machine.ioport.ports[":SYSTEM"]
local player = machine.ioport.ports[":P1"]

local romset = os.getenv("CIRCUS_ROMSET") or "circusc4"
local last_frame = tonumber(os.getenv("CIRCUS_TRACE_LAST") or "1750")
local output = "/tmp/" .. romset .. "-level1-course-objects"
local writes = assert(io.open(output .. "-writes.csv", "w"))
local reads = assert(io.open(output .. "-reads.csv", "w"))
local frames = assert(io.open(output .. "-frames.csv", "w"))

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
  set_button(right, active(1320, last_frame))
  set_button(jump, active(1346, 1350))
end

local function register(name)
  local value = maincpu.state[name]
  return value and value.value or 0
end

local function u8(address)
  return program:read_u8(address)
end

local function h8(address)
  return string.format("%02x", u8(address))
end

writes:write(table.concat({
  "frame", "pc", "address", "value", "a", "b", "x", "y", "u",
  "scroll_hi", "scroll_lo", "course_index", "active_count",
  "activation_hi", "activation_lo", "course_state"
}, ","), "\n")
reads:write(table.concat({
  "frame", "pc", "address", "value", "a", "b", "x", "y", "u",
  "course_index", "activation_hi", "activation_lo", "course_state"
}, ","), "\n")
frames:write(table.concat({
  "frame", "pc", "p1", "player_state", "airborne", "scroll_hi",
  "scroll_lo", "course_index", "active_count", "activation_hi",
  "activation_lo", "course_state", "small_2400_x", "small_2430_x",
  "small_2460_x", "small_2490_x", "hoop_26d0_status", "hoop_26d0_x",
  "hoop_2700_status", "hoop_2700_x", "hoop_2730_status",
  "hoop_2730_x", "hoop_2760_status", "hoop_2760_x"
}, ","), "\n")

local function log_write(address, data)
  if frame < 1100 then return data end
  writes:write(string.format(
    "%d,%04x,%04x,%02x,%02x,%02x,%04x,%04x,%04x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",
    frame, register("PC"), address, data, register("A"), register("B"),
    register("X"), register("Y"), register("U"), u8(0x2203),
    u8(0x2204), u8(0x2208), u8(0x220a), u8(0x20c2), u8(0x20c3),
    u8(0x20bc)))
  writes:flush()
  return data
end

local object_write_tap = program:install_write_tap(
  0x2400, 0x278f, "circusc_level1_course_object_writes", log_write)
local state_write_tap = program:install_write_tap(
  0x2000, 0x220f, "circusc_level1_course_state_writes",
  function(address, data, _)
    if address == 0x20bc or address == 0x20c2 or address == 0x20c3 or
        address == 0x2203 or address == 0x2204 or address == 0x2208 or
        address == 0x220a then
      return log_write(address, data)
    end
    return data
  end)
local table_read_tap = program:install_read_tap(
  0xf780, 0xf87f, "circusc_level1_course_table_reads",
  function(address, data, _)
    if frame >= 1100 then
      reads:write(string.format(
        "%d,%04x,%04x,%02x,%02x,%02x,%04x,%04x,%04x,%02x,%02x,%02x,%02x\n",
        frame, register("PC"), address, data, register("A"), register("B"),
        register("X"), register("Y"), register("U"), u8(0x2208),
        u8(0x20c2), u8(0x20c3), u8(0x20bc)))
      reads:flush()
    end
    return data
  end)

local function x16(base)
  return h8(base + 6) .. h8(base + 7)
end

local function trace_frame()
  frames:write(table.concat({
    frame, string.format("%04x", register("PC")),
    string.format("%02x", player:read()), h8(0x2800), h8(0x20b0),
    h8(0x2203), h8(0x2204), h8(0x2208), h8(0x220a), h8(0x20c2),
    h8(0x20c3), h8(0x20bc), x16(0x2400), x16(0x2430), x16(0x2460),
    x16(0x2490), h8(0x26d0), x16(0x26d0), h8(0x2700), x16(0x2700),
    h8(0x2730), x16(0x2730), h8(0x2760), x16(0x2760)
  }, ","), "\n")
end

local function finish()
  set_button(coin, false)
  set_button(start, false)
  set_button(left, false)
  set_button(right, false)
  set_button(jump, false)
  object_write_tap:remove()
  state_write_tap:remove()
  table_read_tap:remove()
  writes:close()
  reads:close()
  frames:close()
  print("Level 1 course-object trace complete: " .. output)
  machine:exit()
end

emu.register_frame_done(function()
  frame = frame + 1
  apply_inputs()
  if frame >= 1100 then trace_frame() end
  if frame >= last_frame then finish() end
end, "frame")
