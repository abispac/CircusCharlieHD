local frame = 0
local machine = manager.machine
local maincpu = machine.devices[":maincpu"]
local program = maincpu.spaces["program"]
local system = machine.ioport.ports[":SYSTEM"]
local player = machine.ioport.ports[":P1"]

local romset = os.getenv("CIRCUS_ROMSET") or "circusc4"
local jump_frame = tonumber(os.getenv("CIRCUS_JUMP_FRAME") or "1346")
local first_frame = tonumber(os.getenv("CIRCUS_TRACE_FIRST") or "1326")
local last_frame = tonumber(os.getenv("CIRCUS_TRACE_LAST") or "1430")
local output = string.format(
  "/tmp/%s-level1-successful-large-hoop", romset)
local trace = assert(io.open(output .. ".csv", "w"))

local coin = system:field(0x01)
local start = system:field(0x08)
local left = player:field(0x01)
local right = player:field(0x02)
local jump = player:field(0x10)

local rider_slots = {0x25f0, 0x2600, 0x2610, 0x2620, 0x2630, 0x2640}
local hoop_slots = {0x26d0, 0x2700, 0x2730, 0x2760}

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
  set_button(right, active(1320, last_frame + 2))
  set_button(jump, active(jump_frame, jump_frame + 4))
end

local function u8(address)
  return program:read_u8(address)
end

local function u16(address)
  return (u8(address) << 8) | u8(address + 1)
end

local function h8(address)
  return string.format("%02x", u8(address))
end

local function h16(address)
  return string.format("%04x", u16(address))
end

local columns = {
  "frame", "input_right", "input_jump", "player_state", "airborne_state",
  "rider_source_x", "rider_source_y", "jump_anim_state",
  "jump_accumulator", "scroll_command", "scroll_accumulator",
  "activation_accumulator", "course_index", "course_state",
  "active_hoop_slot", "hoop_status", "hoop_x_8_8", "hoop_y",
  "hoop_timer", "hoop_code", "hoop_attr", "collision_result",
  "score_bcd", "score_event_bcd", "landing_transition"
}
for index, slot in ipairs(rider_slots) do
  table.insert(columns, string.format("rider%d_status", index - 1))
  table.insert(columns, string.format("rider%d_y", index - 1))
  table.insert(columns, string.format("rider%d_x", index - 1))
  table.insert(columns, string.format("rider%d_code", index - 1))
  table.insert(columns, string.format("rider%d_attr", index - 1))
end
trace:write(table.concat(columns, ","), "\n")

local previous_score = u16(0x20a1) | (u8(0x20a0) << 16)
local previous_airborne = u8(0x20b0)

local function active_hoop()
  local chosen = nil
  local chosen_distance = 0x100
  for _, slot in ipairs(hoop_slots) do
    if u8(slot) ~= 0 then
      local distance = math.abs(u8(slot + 6) - 0x40)
      if distance < chosen_distance then
        chosen = slot
        chosen_distance = distance
      end
    end
  end
  return chosen
end

local function collision_result(slot)
  if slot == nil then return "inactive" end
  local distance = math.abs(u8(slot + 6) - 0x40)
  if distance >= 0x0e then return "safe_x" end
  local tracked = u16(0x20bf)
  if slot == tracked then return "tracked_branch" end
  local rider_y = u8(0x2644)
  if slot == 0x2760 then rider_y = (rider_y + 0x10) & 0xff end
  if rider_y < 0xb6 then return "safe_above" end
  if rider_y - 0xb6 + distance > 0x1c then return "safe_boundary" end
  return "failure"
end

local function append_rider(row, slot)
  table.insert(row, h8(slot))
  table.insert(row, h8(slot + 4))
  table.insert(row, h8(slot + 6))
  table.insert(row, h8(slot + 14))
  table.insert(row, h8(slot + 15))
end

local function trace_frame()
  local hoop = active_hoop()
  local score = u16(0x20a1) | (u8(0x20a0) << 16)
  local airborne = u8(0x20b0)
  local landing = previous_airborne ~= 0 and airborne == 0
  local row = {
    frame,
    active(1320, last_frame + 2) and 1 or 0,
    active(jump_frame, jump_frame + 4) and 1 or 0,
    h8(0x2800), h8(0x20b0), h8(0x2646), h8(0x2644), h8(0x20b4),
    h16(0x20b5), h16(0x20b1), h16(0x2203), h16(0x20c2),
    h8(0x2208), h8(0x20bc),
    hoop and string.format("%04x", hoop) or "none",
    hoop and h8(hoop) or "00",
    hoop and h16(hoop + 6) or "0000",
    hoop and h8(hoop + 4) or "00",
    hoop and h8(hoop + 8) or "00",
    hoop and h8(hoop + 14) or "00",
    hoop and h8(hoop + 15) or "00",
    collision_result(hoop),
    string.format("%06x", score),
    string.format("%06x", (score - previous_score) & 0xffffff),
    landing and 1 or 0
  }
  for _, slot in ipairs(rider_slots) do append_rider(row, slot) end
  trace:write(table.concat(row, ","), "\n")
  previous_score = score
  previous_airborne = airborne
end

local function finish()
  set_button(coin, false)
  set_button(start, false)
  set_button(left, false)
  set_button(right, false)
  set_button(jump, false)
  trace:close()
  print("Successful Level 1 large-hoop trace complete: " .. output .. ".csv")
  machine:exit()
end

emu.register_frame_done(function()
  frame = frame + 1
  apply_inputs()
  if frame >= first_frame then trace_frame() end
  if frame >= last_frame then finish() end
end, "frame")
