local frame = 0
local machine = manager.machine
local maincpu = machine.devices[":maincpu"]
local program = maincpu.spaces["program"]
local system = machine.ioport.ports[":SYSTEM"]
local player = machine.ioport.ports[":P1"]

local romset = os.getenv("CIRCUS_ROMSET") or "circusc4"
local jump_frame = tonumber(os.getenv("CIRCUS_JUMP_FRAME") or "1330")
local last_frame = tonumber(os.getenv("CIRCUS_TRACE_LAST") or "1900")
local output = string.format(
  "/tmp/%s-level1-hoop-activation-jump-%d", romset, jump_frame)
local writes = assert(io.open(output .. "-writes.csv", "w"))
local frames = assert(io.open(output .. "-frames.csv", "w"))
local table_reads = assert(io.open(output .. "-table-reads.csv", "w"))
local course_data = assert(io.open(output .. "-course-data.csv", "w"))

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
  set_button(jump, active(jump_frame, jump_frame + 4))
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
  "activation_acc_hi", "activation_acc_lo", "course_y", "selected_slot"
}, ","), "\n")

frames:write(table.concat({
  "frame", "pc", "scroll_hi", "scroll_lo", "scroll_delta_hi",
  "scroll_delta_lo", "course_index", "active_count", "activation_acc_hi",
  "activation_acc_lo", "course_y", "selected_slot",
  "obj_26d0_active", "obj_26d0_y", "obj_26d0_x_hi", "obj_26d0_x_lo",
  "obj_26d0_timer", "obj_26d0_code", "obj_26d0_attr",
  "obj_2700_active", "obj_2700_y", "obj_2700_x_hi", "obj_2700_x_lo",
  "obj_2700_timer", "obj_2700_code", "obj_2700_attr",
  "obj_2730_active", "obj_2730_y", "obj_2730_x_hi", "obj_2730_x_lo",
  "obj_2730_timer", "obj_2730_code", "obj_2730_attr",
  "obj_2760_active", "obj_2760_y", "obj_2760_x_hi", "obj_2760_x_lo",
  "obj_2760_timer", "obj_2760_code", "obj_2760_attr"
}, ","), "\n")
table_reads:write("frame,pc,address,value,a,b,x,y,u,course_index,activation_acc_hi,activation_acc_lo,course_byte\n")
course_data:write("index,address,value\n")
for index = 0, 31 do
  local address = 0xf7c4 + index
  course_data:write(string.format("%d,%04x,%02x\n", index, address,
    program:read_u8(address)))
end
course_data:close()

local write_tap = program:install_write_tap(
  0x26d0, 0x278f, "circusc_level1_hoop_activation_writes",
  function(address, data, _)
    if frame >= 1100 then
      writes:write(string.format(
        "%d,%04x,%04x,%02x,%02x,%02x,%04x,%04x,%04x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%04x\n",
        frame, register("PC"), address, data, register("A"), register("B"),
        register("X"), register("Y"), register("U"), u8(0x2203),
        u8(0x2204), u8(0x2208), u8(0x220a), u8(0x20c2), u8(0x20c3),
        u8(0x20bc), register("U")))
      writes:flush()
    end
    return data
  end)

local state_write_tap = program:install_write_tap(
  0x2000, 0x220f, "circusc_level1_hoop_activation_state_writes",
  function(address, data, _)
    if frame >= 1100 and (address == 0x20bc or address == 0x20c2 or
        address == 0x20c3 or address == 0x2208 or address == 0x220a) then
      writes:write(string.format(
        "%d,%04x,%04x,%02x,%02x,%02x,%04x,%04x,%04x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%04x\n",
        frame, register("PC"), address, data, register("A"), register("B"),
        register("X"), register("Y"), register("U"), u8(0x2203),
        u8(0x2204), u8(0x2208), u8(0x220a), u8(0x20c2), u8(0x20c3),
        u8(0x20bc), register("U")))
      writes:flush()
    end
    return data
  end)

local table_read_tap = program:install_read_tap(
  0xf7b4, 0xf81b, "circusc_level1_hoop_course_table_reads",
  function(address, data, _)
    if frame >= 1100 then
      table_reads:write(string.format(
        "%d,%04x,%04x,%02x,%02x,%02x,%04x,%04x,%04x,%02x,%02x,%02x,%02x\n",
        frame, register("PC"), address, data, register("A"), register("B"),
        register("X"), register("Y"), register("U"), u8(0x2208),
        u8(0x20c2), u8(0x20c3), u8(0x20bc)))
      table_reads:flush()
    end
    return data
  end)

local function append_object(row, base)
  table.insert(row, h8(base))
  table.insert(row, h8(base + 4))
  table.insert(row, h8(base + 6))
  table.insert(row, h8(base + 7))
  table.insert(row, h8(base + 8))
  table.insert(row, h8(base + 14))
  table.insert(row, h8(base + 15))
end

local function trace_frame()
  local row = {
    frame, string.format("%04x", register("PC")), h8(0x2203), h8(0x2204),
    h8(0x20b1), h8(0x20b2), h8(0x2208), h8(0x220a), h8(0x20c2),
    h8(0x20c3), h8(0x20bc), string.format("%04x", register("U"))
  }
  append_object(row, 0x26d0)
  append_object(row, 0x2700)
  append_object(row, 0x2730)
  append_object(row, 0x2760)
  frames:write(table.concat(row, ","), "\n")
end

local function finish()
  set_button(coin, false)
  set_button(start, false)
  set_button(left, false)
  set_button(right, false)
  set_button(jump, false)
  write_tap:remove()
  state_write_tap:remove()
  table_read_tap:remove()
  writes:close()
  frames:close()
  table_reads:close()
  print("Level 1 hoop activation trace complete: " .. output)
  machine:exit()
end

emu.register_frame_done(function()
  frame = frame + 1
  apply_inputs()
  if frame >= 1100 then trace_frame() end
  if frame >= last_frame then finish() end
end, "frame")
