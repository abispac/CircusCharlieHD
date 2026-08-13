local frame = 0
local machine = manager.machine
local maincpu = machine.devices[":maincpu"]
local program = maincpu.spaces["program"]
local system = machine.ioport.ports[":SYSTEM"]
local player = machine.ioport.ports[":P1"]

local romset = os.getenv("CIRCUS_ROMSET") or "circusc4"
local first_frame = tonumber(os.getenv("CIRCUS_TRACE_FIRST") or "900")
local last_frame = tonumber(os.getenv("CIRCUS_TRACE_LAST") or "3300")
local input_mode = os.getenv("CIRCUS_TRACE_INPUT_MODE") or "attract"
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
  if input_mode == "attract" then return end
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
  "scroll_hi", "scroll_lo", "course_index", "extra_charlie_state",
  "activation_hi", "activation_lo", "course_state"
}, ","), "\n")
reads:write(table.concat({
  "frame", "pc", "address", "value", "a", "b", "x", "y", "u",
  "course_index", "activation_hi", "activation_lo", "course_state"
}, ","), "\n")
frames:write(table.concat({
  "frame", "pc", "p1", "player_state", "airborne", "scroll_hi",
  "scroll_lo", "movement_hi", "movement_lo", "course_index", "extra_charlie_state", "activation_hi",
  "activation_lo", "course_state", "course_offset", "small_2400_x", "small_2430_x",
  "small_2460_x", "small_2490_x", "hoop_26d0_status", "hoop_26d0_x",
  "hoop_2700_status", "hoop_2700_x", "hoop_2730_status",
  "hoop_2730_x", "hoop_2760_status", "hoop_2760_x", "board_frame"
  , "pot_24b0_status", "pot_24b0_x", "pot_24b0_y", "pot_24b0_code",
  "pot_24f0_status", "pot_24f0_x", "pot_24f0_y", "pot_24f0_code",
  "pot_2530_status", "pot_2530_x", "pot_2530_y", "pot_2530_code",
  "coin_2570_status", "coin_2570_x", "coin_2570_y", "coin_2570_code",
  "hoop_26d0_codes", "hoop_26d0_attrs", "hoop_2700_codes",
  "hoop_2700_attrs", "hoop_2730_codes", "hoop_2730_attrs",
  "hoop_2760_codes", "hoop_2760_attrs", "prize_state", "tracked_object"
}, ","), "\n")

local function log_write(address, data)
  if frame < first_frame then return data end
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
    if frame >= first_frame then
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

local function word(base)
  return h8(base) .. h8(base + 1)
end

local function codes(base)
  return h8(base + 0x0e) .. ":" .. h8(base + 0x1e) .. ":" ..
         h8(base + 0x2e)
end

local function attrs(base)
  return h8(base + 0x0f) .. ":" .. h8(base + 0x1f) .. ":" ..
         h8(base + 0x2f)
end

local function trace_frame()
  frames:write(table.concat({
    frame, string.format("%04x", register("PC")),
    string.format("%02x", player:read()), h8(0x2800), h8(0x20b0),
    h8(0x2203), h8(0x2204), h8(0x20b1), h8(0x20b2), h8(0x2208), h8(0x220a), h8(0x20c2),
    h8(0x20c3), h8(0x20bc), h8(0x20bb), x16(0x2400), x16(0x2430), x16(0x2460),
    x16(0x2490), h8(0x26d0), x16(0x26d0), h8(0x2700), x16(0x2700),
    h8(0x2730), x16(0x2730), h8(0x2760), x16(0x2760), h8(0x2014),
    h8(0x24b0), x16(0x24b0), h8(0x24b4), h8(0x24be),
    h8(0x24f0), x16(0x24f0), h8(0x24f4), h8(0x24fe),
    h8(0x2530), x16(0x2530), h8(0x2534), h8(0x253e),
    h8(0x2570), x16(0x2570), h8(0x2574), h8(0x257e),
    codes(0x26d0), attrs(0x26d0), codes(0x2700), attrs(0x2700),
    codes(0x2730), attrs(0x2730), codes(0x2760), attrs(0x2760),
    h8(0x25e1), word(0x20bf)
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
  if frame >= first_frame then trace_frame() end
  if frame >= last_frame then finish() end
end, "frame")
