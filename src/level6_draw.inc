// ---- Event 6 (the trapeze) drawing ------------------------------------------
// The board's sprite table ($2400-$27FF) and the goal / trampoline positions
// are read straight from the RAM mirror: rope cells are drawn where the
// board puts them, Charlie's four cells name his pose, and the tilemap
// objects (the tubs, the goal platform) come from their records.
//
// Everything is placed as measured on MAME snapshots of the board (rows of
// the 256-row screen, columns of the 224-column screen):
//   HUD to row 80, the overhead truss on rows 82-87, a black sky down to row
//   185, the blue rail at 186, the crowd 188-207, the valance 208-222 and
//   the grass from 224.
//   Charlie's hands are at ($6 + 6, $4 + 1) of his record; hanging, his body
//   continues the rope's line past the bar, about 28 rows long.
//   The catcher on a decorated bar hangs 20 rows along the rope from the bar
//   to her hands.
//   A tub is 42 columns wide and 18 rows tall, its base on row 242.
//   The goal plank is 40 columns wide on row 136; its lattice tower is 30
//   columns wide and reaches the grass.
//   The partner stands on the truss (cells $70/$78) and, in the finale, on
//   the plank (cells $55/$5D and $5A/$7B in slot $2470); her shoes are on the
//   row of her lower cell.

namespace l6draw {

inline int rd(const Level6& l, int a) { return l.ram[static_cast<std::size_t>(a & 0x3fff)]; }

constexpr float kHandsColumnOffset = 6.0F;
constexpr float kHandsRowOffset = 1.0F;
// The v3 hang set (Gemini, art/source/incoming/charlie-hang-sheet-gemini-01.jpeg)
// is 594 px from fists to toes; at 31 rows his body keeps the size of the
// other Event 6 poses with the arms added on top.
constexpr float kCharlieHangUnit = 31.0F * kLevel3RowScale / 594.0F;
constexpr float kCharlieStandUnit = 27.0F * kLevel3RowScale / 222.0F;
// The catcher, knees on the bar to fingertips; the board hangs her 20 rows,
// the painted one is a head taller so she reads at Charlie's scale.
constexpr float kCatcherLengthRows = 30.0F;
// v3 (art/source/incoming/stage6-catcher-twohands-gemini-01.jpeg): cell 0
// reaches down with both arms, fists together, at 0.86 of her height.
constexpr float kCatcherHandsFraction = 0.86F;
// Where Charlie's hands go on her (units from the bar, measured on the v3
// cells): the middle of her two fists less the four units from his own fist
// tops to their middle, so both her hands close around his.
constexpr float kCatcherFistsAcross = 5.9F;
constexpr float kCatcherFistsDown = 64.2F;
// Her standing pose spans 45 board rows: in the kiss painting she is 1307 of
// its rows to Charlie's 924 (32 board rows), so she keeps that size when she
// steps out of it to cheer, and on the truss and the plank before it.
constexpr float kPartnerRows = 45.0F;
constexpr float kPartnerUnit = kPartnerRows * kLevel3RowScale / 372.0F;

struct Grip {
  bool valid = false;
  Vec2 point{0.0F, 0.0F};
  float angle = 0.0F;
};

// A Charlie pose by the code of his leading cell.
//   $25/$39/$3D/$41/$F0/$F4  hanging, by swing phase
//   $F8                      airborne
//   $29                      the finale's jump
//   $2D                      flat on the floor
//   $31                      sitting on the floor
//   $74/$76/$7C/$7E          on the goal platform, with the bag
//   $95-$AC                  the finale's tumble
//   $AD/$B1                  cheering
inline void drawCharlie(SDL_Renderer* renderer, const Assets& assets, int code,
                        int row, int col, const Grip* grip = nullptr) {
  const float scale = kSourceToWorldX;
  float handsX = (static_cast<float>(col) + kHandsColumnOffset) * scale;
  float handsY = level3RowToY(static_cast<float>(row) + kHandsRowOffset);
  float swingAngle = 0.0F;
  // Standing, the four cells are a 32x32 composite whose shoes are on the
  // row of the lower cells + 16, centred two columns past the record.
  const float centerX = (static_cast<float>(col) + 2.0F) * scale;
  const float feetY = level3RowToY(static_cast<float>(row) + 32.0F);
  const float unit = kCharlieStandUnit;
  if (code >= 0x95 && code <= 0xac) {
    // The finale's tumble: six somersault frames, 24 rows across.
    drawStage2Pose(renderer, assets.stage6CharlieTumble, assets.stage6CharlieTumbleCells,
                   (code - 0x95) % 6, centerX + 2.0F * scale, feetY - 4.0F, unit);
    return;
  }
  switch (code) {
    case 0x25: case 0x39: case 0x3d: case 0x41: case 0xf0: case 0xf4: {
      // Hanging: the hands on the bar, the body along the swing.  Cells:
      // 0 straight, 1-2 legs forward (bent, tucked), 3 straight, 4-5 legs
      // back (bent, tucked).
      int frame = 0;
      switch (code) {
        case 0x39: frame = 1; break;
        case 0x3d: frame = 2; break;
        case 0x41: frame = 3; break;
        case 0xf0: frame = 4; break;
        case 0xf4: frame = 5; break;
        default: break;
      }
      if (grip != nullptr && grip->valid) {
        handsX = grip->point.x;
        handsY = grip->point.y + 2.0F;
        swingAngle = grip->angle;
      }
      drawPoseFromTop(renderer, assets.stage6CharlieHang, assets.stage6CharlieHangCells,
                      frame, handsX, handsY, kCharlieHangUnit,
                      static_cast<double>(swingAngle));
      return;
    }
    case 0xf8:
      // Airborne after a release: tucked, 16 rows tall around the record.
      drawStage2Pose(renderer, assets.stage5CharlieLeap, assets.stage5CharlieLeapCells,
                     3, centerX, level3RowToY(static_cast<float>(row) + 20.0F),
                     unit * 0.8F);
      return;
    case 0x29:
      drawStage2Pose(renderer, assets.stage5CharlieLeap, assets.stage5CharlieLeapCells,
                     2, centerX, feetY, unit);
      return;
    case 0x2d:   // flat on the floor
      drawStage2Pose(renderer, assets.stage6CharlieFloor, assets.stage6CharlieFloorCells,
                     1, centerX, feetY + 2.0F, unit);
      return;
    case 0x31:   // sitting on the floor
      drawStage2Pose(renderer, assets.stage6CharlieFloor, assets.stage6CharlieFloorCells,
                     0, centerX, feetY + 2.0F, unit);
      return;
    case 0x76: case 0x7e:   // on the goal platform (the cabinet's bag pose)
      // His partner is in his arms here, not the bag: wave with the other
      // cheer pose so the two alternate.
      drawStage2Pose(renderer, assets.stage5CharlieFall, assets.stage5CharlieFallCells,
                     0, centerX, feetY, unit);
      return;
    case 0x74: case 0x7c: case 0xad: case 0xb1:   // cheering
      drawStage2Pose(renderer, assets.stage6CharlieFloor, assets.stage6CharlieFloorCells,
                     2, centerX, feetY, unit);
      return;
    case 0x2e:
      drawStage2Pose(renderer, assets.stage5CharlieFall, assets.stage5CharlieFallCells,
                     0, centerX, feetY, unit);
      return;
    default:
      drawStage2Pose(renderer, assets.stage2CharlieWalk, assets.stage2CharlieWalkCells,
                     0, centerX, feetY, unit);
      return;
  }
}

// The partner (two cells: codes $70/$78 standing on the truss, her shoes
// nine rows under the lower cell; $55/$5D and $5A/$7B on the plank, sixteen
// rows under it), centred two columns past the record like Charlie.
inline void drawPartner(SDL_Renderer* renderer, const Assets& assets, int frame,
                        int lowerRow, int col, float feetOffsetRows,
                        float feetOverride = -1.0F) {
  const float scale = kSourceToWorldX;
  const float x = (static_cast<float>(col) + 2.0F) * scale;
  const float feetY = feetOverride >= 0.0F
                          ? feetOverride
                          : level3RowToY(static_cast<float>(lowerRow) + feetOffsetRows);
  if (assets.stage6Girl && assets.stage6GirlCells.valid()) {
    // Her standing pose (the first cell) spans kPartnerRows.
    const float standing = static_cast<float>(assets.stage6GirlCells.bounds[0].h);
    const float unit = standing > 0.0F ? kPartnerRows * kLevel3RowScale / standing
                                       : kPartnerUnit;
    drawStage2Pose(renderer, assets.stage6Girl, assets.stage6GirlCells, frame, x,
                   feetY, unit);
  } else {
    fillRect(renderer, x - 8.0F, feetY - 60.0F, 16.0F, 60.0F, color(255, 120, 200));
  }
}

// The rope cells of one slot: the beads from the pivot down, the bar
// ($56) and, on a decorated trapeze, the catcher's cells ($57-$7A) past it.
struct RopeShape {
  bool valid = false;
  Vec2 top{0.0F, 0.0F};
  Vec2 bar{0.0F, 0.0F};
  bool hasBar = false;
  bool catcher = false;
  // One bead cell per rope cell, pivot first; `spent` marks the cells the
  // board colours with attribute 6, one per full swing hung on the bar.
  struct Bead { Vec2 at; bool spent; };
  std::vector<Bead> beads;
  float angle = 0.0F;   // SDL degrees of the pivot-to-bar line from "down"
};

inline RopeShape ropeShape(const Level6& l, int slot) {
  RopeShape shape;
  const int base = 0x2400 + slot * 0x100;
  const float scale = kSourceToWorldX;
  for (int cell = 15; cell >= 0; --cell) {
    const int a = base + 0xf0 - cell * 0x10;
    const int col = rd(l, a + 6);
    const int row = rd(l, a + 4);
    const int code = rd(l, a + 0xe);
    const int attr = rd(l, a + 0xf);
    if (col == 0 || code == 0 || code == 0xfe) continue;
    const Vec2 p{(static_cast<float>(col) + 8.0F) * scale,
                 level3RowToY(static_cast<float>(row) + 8.0F)};
    if (code == 0x56 && !shape.hasBar) {
      // The first $56 after the rope is the bar.  The catcher's own cells
      // can use $56 too (one of her poses swinging back flat), so a later
      // one is part of her, not a second bar.
      shape.hasBar = true;
      shape.bar = p;
    } else if (code == 0x56) {
      shape.catcher = true;
    } else if (code == 0x55) {
      shape.beads.push_back({p, attr == 6});
    } else if (code >= 0x57 && code <= 0x7a) {
      shape.catcher = true;
    }
  }
  if (shape.beads.empty() && !shape.hasBar) return shape;
  shape.valid = true;
  shape.top = shape.beads.empty() ? shape.bar : shape.beads.front().at;
  for (const RopeShape::Bead& b : shape.beads) {
    if (b.at.y < shape.top.y) shape.top = b.at;
  }
  const Vec2 end = shape.hasBar ? shape.bar : shape.beads.back().at;
  const float dx = end.x - shape.top.x;
  const float dy = end.y - shape.top.y;
  if (dx * dx + dy * dy > 1.0F) {
    shape.angle = std::atan2(dx, dy) * -180.0F / kPi;
  }
  return shape;
}

// Hanging, Charlie holds the current bar, which the board always shows in
// slot 0 ($AE97 fills it from the record `$DA` names); his hands are drawn
// at its bar -- or at the catcher's hands on a decorated trapeze -- and
// his body follows the rope's line.  The cells put the hands at different
// columns of the composite in each pose, so the record only says which bar.
// Should slot 0 be empty (the bar scrolled off), the nearest bar to the
// centre of his composite stands in.
inline Grip charlieGrip(const Level6& l, int row, int col) {
  const float scale = kSourceToWorldX;
  const float centerX = (static_cast<float>(col) + 16.0F) * scale;
  const float centerY = level3RowToY(static_cast<float>(row) + 16.0F);
  const auto gripOf = [&](const RopeShape& shape) {
    Vec2 grip = shape.bar;
    if (shape.catcher) {
      // His fists go into hers: in the v3 catcher her two fists sit
      // kCatcherFistsAcross to the side of the line through her knees and
      // kCatcherFistsDown along it, his fist tops four units above their
      // middle, turned with the swing.
      const float radians = shape.angle * kPi / 180.0F;
      const float across = kCatcherFistsAcross;
      const float down = kCatcherFistsDown;
      grip = Vec2{shape.bar.x + across * std::cos(radians) - down * std::sin(radians),
                  shape.bar.y + across * std::sin(radians) + down * std::cos(radians)};
    }
    Grip g;
    g.valid = true;
    g.point = grip;
    g.angle = shape.angle;
    return g;
  };
  {
    const RopeShape current = ropeShape(l, 0);
    if (current.valid && current.hasBar) return gripOf(current);
  }
  Grip best;
  float bestDistance = 1e9F;
  for (int slot = 1; slot < 3; ++slot) {
    const RopeShape shape = ropeShape(l, slot);
    if (!shape.valid || !shape.hasBar) continue;
    const Grip candidate = gripOf(shape);
    const float dx = candidate.point.x - centerX;
    const float dy = candidate.point.y - centerY;
    const float distance = dx * dx + dy * dy;
    if (distance < bestDistance) {
      bestDistance = distance;
      best = candidate;
    }
  }
  if (bestDistance > 48.0F * scale * 48.0F * scale) best.valid = false;
  return best;
}

inline void drawRope(SDL_Renderer* renderer, const Assets& assets,
                     const Level6& l, int slot) {
  const RopeShape shape = ropeShape(l, slot);
  if (!shape.valid) return;
  const Vec2 top = shape.top;
  // One bead per rope cell, strung from a bracket under the truss.  Every
  // full swing Charlie hangs on a bar spends one cell from the pivot down
  // (the board's attribute 6): those beads go dark, so the bright beads
  // left are the swings he still has before the bar lets go of him.
  const SDL_Color fresh = color(232, 40, 36);
  const SDL_Color freshDark = color(140, 12, 16);
  const SDL_Color freshShine = color(255, 160, 140);
  const SDL_Color spent = color(96, 40, 44);
  const SDL_Color spentDark = color(48, 18, 22);
  const SDL_Color spentShine = color(150, 90, 90);
  fillRect(renderer, top.x - 7.0F, top.y - 9.0F, 14.0F, 8.0F, color(120, 120, 140));
  for (const RopeShape::Bead& bead : shape.beads) {
    const SDL_Color body = bead.spent ? spent : fresh;
    const SDL_Color dark = bead.spent ? spentDark : freshDark;
    const SDL_Color shine = bead.spent ? spentShine : freshShine;
    const float x = bead.at.x;
    const float y = bead.at.y;
    filledCircle(renderer, x, y, 6.4F, dark);
    filledCircle(renderer, x - 0.7F, y - 0.7F, 5.3F, body);
    filledCircle(renderer, x - 2.0F, y - 2.0F, 1.7F, shine);
  }
  if (shape.hasBar) {
    // The bar seen end-on: a small white grip with a knob.
    const Vec2 bar = shape.bar;
    fillRect(renderer, bar.x - 8.0F, bar.y - 3.0F, 16.0F, 6.0F, color(70, 50, 30));
    fillRect(renderer, bar.x - 7.0F, bar.y - 2.0F, 14.0F, 4.0F, color(236, 236, 244));
    filledCircle(renderer, bar.x, bar.y, 5.0F, color(120, 80, 10));
    filledCircle(renderer, bar.x, bar.y, 4.0F, color(250, 205, 40));
    filledCircle(renderer, bar.x - 1.5F, bar.y - 1.5F, 1.4F, color(255, 245, 190));
    if (shape.catcher) {
      // The catcher hangs by her knees from the bar, along the swing, her
      // hands twenty rows down the rope's line.
      if (assets.stage6Catcher && assets.stage6CatcherCells.valid()) {
        const SDL_Rect& source = assets.stage6CatcherCells.bounds[0];
        // Her fists sit at kCatcherHandsFraction of the painted bounds; the
        // ponytail hangs past them.
        const float unit = kCatcherLengthRows * kLevel3RowScale /
                           (static_cast<float>(source.h) * kCatcherHandsFraction);
        drawPoseFromTop(renderer, assets.stage6Catcher, assets.stage6CatcherCells,
                        0, bar.x, bar.y - 2.0F, unit,
                        static_cast<double>(shape.angle));
      } else {
        ellipse(renderer, bar.x, bar.y + 25.0F, 8.0F, 25.0F, color(220, 200, 60), 4);
      }
    }
  }
}

// A tub at its record's column (low byte; the high byte off the page): 42
// columns wide, 18 rows tall, its base on row 242.
inline void drawTub(SDL_Renderer* renderer, const Assets& assets,
                    const Level6& l, int record) {
  if (rd(l, record) != 0) return;
  const int low = rd(l, record + 1);
  const float scale = kSourceToWorldX;
  const float centerX = (static_cast<float>(low) - 44.0F) * scale;
  const float bottomY = level3RowToY(242.0F);
  const float width = 42.0F * scale;
  const float height = 18.0F * kLevel3RowScale;
  if (assets.stage6Tub) {
    // The HD leather tub (the Event 3 drums' style), on the board's own
    // 42 x 18 box.
    const SDL_FRect destination{centerX - width * 0.5F, bottomY - height, width, height};
    SDL_RenderCopyF(renderer, assets.stage6Tub, nullptr, &destination);
    return;
  }
  fillRect(renderer, centerX - width * 0.5F, bottomY - height, width, height,
           color(220, 60, 60));
}

inline void drawGoalPlatform(SDL_Renderer* renderer, const Assets& assets,
                             const Level6& l) {
  if (rd(l, 0x20df) != 0) return;
  const int low = rd(l, 0x20e0);
  const float scale = kSourceToWorldX;
  // Measured on MAME at the goal: the plank spans columns $E0 - 59 .. $E0 - 13
  // (47 wide) on rows 137-142, and the lattice tower 29 columns under its
  // centre down to the valance (row 230).
  const float centerX = (static_cast<float>(low) - 36.0F) * scale;
  const float plankTop = level3RowToY(137.0F);
  const float plankBottom = level3RowToY(143.0F);
  const float groundY = level3RowToY(230.0F);
  const float towerWidth = 29.0F * scale;
  if (assets.stage6Lattice) {
    const float bay = 16.0F * kLevel3RowScale;
    for (float y = plankBottom; y < groundY; y += bay) {
      const float h = std::min(bay, groundY - y);
      const SDL_Rect source{0, 0, 300, static_cast<int>(160.0F * h / bay)};
      const SDL_FRect destination{centerX - towerWidth * 0.5F, y, towerWidth, h};
      SDL_RenderCopyF(renderer, assets.stage6Lattice, &source, &destination);
    }
  } else {
    fillRect(renderer, centerX - 10.0F, plankBottom, 20.0F, groundY - plankBottom,
             color(120, 120, 140));
  }
  const float plankWidth = 47.0F * scale;
  if (assets.stage6Plank) {
    const SDL_FRect destination{centerX - plankWidth * 0.5F, plankTop, plankWidth,
                                plankBottom - plankTop};
    SDL_RenderCopyF(renderer, assets.stage6Plank, nullptr, &destination);
  } else {
    fillRect(renderer, centerX - plankWidth * 0.5F, plankTop, plankWidth,
             plankBottom - plankTop, color(160, 170, 190));
  }
}

inline std::string plaqueText(const Level6& l) {
  switch (rd(l, 0x27de)) {
    case 0xb9: return "START";
    case 0xc1: return "50M";
    case 0xc0: return "40M";
    case 0xbf: return "30M";
    case 0xbe: return "20M";
    case 0xbd: return "10M";
    default: return {};
  }
}

}  // namespace l6draw

void drawStage6Scene(SDL_Renderer* renderer, const Game& game,
                     const Assets& assets, double timeSeconds,
                     double interpolation) {
  using l6draw::rd;
  const Level6& l = game.level6;
  const float camera = game.previousCameraX +
                       (game.cameraX - game.previousCameraX) *
                           static_cast<float>(interpolation);
  const float scale = kSourceToWorldX;

  // The night: black from the HUD down to the stands, under the tent's
  // crown and the score panel every event shares.
  fillRect(renderer, 0.0F, 0.0F, kWorldWidth, kWorldHeight, color(0, 0, 0));
  drawMarqueeBand(renderer, false, assets, game, timeSeconds);
  // The stands band and the grass, as on the tightrope: the blue rail at
  // row 186, the crowd, the valance and the grass from row 224.
  const float railY = level3RowToY(186.0F);
  if (assets.arena) {
    int textureWidth = 0;
    int textureHeight = 0;
    SDL_QueryTexture(assets.arena, nullptr, nullptr, &textureWidth, &textureHeight);
    const int sourceTop = static_cast<int>(
        static_cast<float>(textureHeight) * kArenaContentSourceTop);
    drawArenaBand(renderer, assets.arena, assets.elephantDoor, camera, railY,
                  (kWorldHeight - railY) * 1.14F /
                      static_cast<float>(textureHeight - sourceTop));
  } else {
    fillRect(renderer, 0.0F, railY, kWorldWidth, kWorldHeight - railY, color(36, 142, 49));
  }
  // The overhead truss on rows 82-87.
  {
    // Rows 82-87 on the board; the HD score panel reaches row 84, so the
    // band sits just under it, the same six rows deep.
    const float trussTop = std::max(level3RowToY(82.0F), kArenaTop);
    const float trussBottom = trussTop + 6.0F * kLevel3RowScale;
    if (assets.stage6Truss) {
      const float bay = 16.0F * scale;
      float offset = std::fmod(camera, bay);
      if (offset < 0.0F) offset += bay;
      for (float x = -offset; x < kWorldWidth; x += bay) {
        const SDL_FRect destination{x, trussTop, bay, trussBottom - trussTop};
        SDL_RenderCopyF(renderer, assets.stage6Truss, nullptr, &destination);
      }
    } else {
      fillRect(renderer, 0.0F, trussTop, kWorldWidth, trussBottom - trussTop,
               color(150, 150, 170));
    }
  }
  const bool restarting =
      game.scene == Scene::Crashed && game.crashFrame >= 24;
  // The plaque rides the scroll on the fence.
  {
    const std::string text = l6draw::plaqueText(l);
    const int col = rd(l, 0x27d6);
    if (!text.empty() && rd(l, 0x27e4) != 0) {
      const float x = (static_cast<float>(col) + 8.0F) * scale;
      drawLevel3Sign(renderer, x, text);
    }
  }
  drawHud(renderer, game, assets.charlieLife);
  if (restarting) return;

  for (int record : {0x20e4, 0x20e9, 0x20ee}) {
    l6draw::drawTub(renderer, assets, l, record);
  }
  l6draw::drawGoalPlatform(renderer, assets, l);

  // The partner: on the truss while the stage plays ($2720/$2730 cells
  // $70/$78); the same slot carries the bag ($58) at the goal.
  {
    const int code = rd(l, 0x273e);
    const int col = rd(l, 0x2736);
    if (col != 0 && (code == 0x70 || code == 0x78)) {
      // Her boots on the truss's top edge: the HD truss sits just under the
      // score panel, a little lower than the board's row 82.
      const float trussTop = std::max(level3RowToY(82.0F), kArenaTop);
      l6draw::drawPartner(renderer, assets, (l.hdFrame / 48) % 2 == 0 ? 0 : 1,
                          rd(l, 0x2724), col, 9.0F, trussTop + 2.0F);
    }
  }

  // The ropes are cleared at the goal ($B556) and their slots then hold the
  // finale's cells.
  if (rd(l, 0x2800) != 2) {
    for (int slot = 2; slot >= 0; --slot) l6draw::drawRope(renderer, assets, l, slot);
  }

  int kissRow = -1;
  int kissCol = 0;
  if (rd(l, 0x2800) == 2) {
    // The partner drops from the truss to Charlie ($2730, code $58, the
    // cabinet's bag-carrying tumble); once she is down, the slot and later
    // $2470 show her on the plank ($55/$5A).
    for (int base : {0x2730, 0x2470}) {
      const int code = rd(l, base + 0xe);
      const int col = rd(l, base + 6);
      if (col == 0) continue;
      if (code == 0x58) {
        l6draw::drawPartner(renderer, assets, 2, rd(l, base + 4), col, 16.0F);
      } else if (code == 0x55 || code == 0x5a) {
        // On the board Charlie's cells cover her while he waves ($74-$7F);
        // here he has caught her: she stands at his side, kissing him, and
        // he is drawn over her shoulder.
        const int charlieCode = rd(l, 0x277e);
        const bool wave = charlieCode >= 0x74 && charlieCode <= 0x7f;
        if (wave) {
          // Drawn over Charlie below, at his side.
          kissRow = rd(l, base - 0x10 + 4);
          kissCol = col;
        } else {
          l6draw::drawPartner(renderer, assets, code == 0x55 ? 2 : 1,
                              rd(l, base - 0x10 + 4), col, 16.0F);
        }
      }
    }
  }
  // Charlie ($2740-$2770), in front of the partner he hides on the plank.
  // In the finale his record is the twelfth copy ($2480 + $40 * 11) and is
  // drawn with the others below.
  const bool finale = rd(l, 0x2800) == 2 && rd(l, 0x2738) == 0;
  if (!finale && (rd(l, 0x2776) != 0 || rd(l, 0x2774) != 0)) {
    const int code = rd(l, 0x277e);
    const bool hanging = code == 0x25 || code == 0x39 || code == 0x3d ||
                         code == 0x41 || code == 0xf0 || code == 0xf4;
    const l6draw::Grip grip = hanging ? l6draw::charlieGrip(l, rd(l, 0x2774),
                                                            rd(l, 0x2776))
                                      : l6draw::Grip{};
    if (kissRow >= 0 && assets.stage6Kiss && assets.stage6KissCells.valid()) {
      // The goal, as the cabinet plays it: she lands behind him, his cells
      // cover her for 34 frames, then he turns to the crowd blushing with
      // her kiss on his cheek and waves.  The painted pair plays that out
      // in six cells (tools/build_stage6_kiss.py): side by side, she leans
      // in, the peck, the blush (two cells), both waving; every cell has
      // Charlie's middle on its centre and both their feet on its baseline,
      // and she stands kPartnerRows tall.
      static int kissStart = -1;
      static int kissLastFrame = -1;
      if (kissStart < 0 || l.hdFrame < kissLastFrame || l.hdFrame - kissLastFrame > 10) {
        kissStart = l.hdFrame;
      }
      kissLastFrame = l.hdFrame;
      const int age = l.hdFrame - kissStart;
      // Side by side, she leans in, the peck: the 34 frames the board hides
      // her; then the blush, swapping with his $74/$76 cells as the board
      // does, and both waving for the last of the celebration.
      const int charlieCode = rd(l, 0x277e);
      int cell = 0;
      if (age < 10) cell = 0;
      else if (age < 20) cell = 1;
      else if (age < 46) cell = 2;
      else if (age < 200) cell = charlieCode == 0x74 || charlieCode == 0x7c ? 4 : 3;
      else cell = 5;
      const SheetCells& cells = assets.stage6KissCells;
      const float scale = kSourceToWorldX;
      const float standing = static_cast<float>(cells.bounds[0].h);   // her full height
      const float unit = l6draw::kPartnerRows * kLevel3RowScale / standing;
      const float centerX = (static_cast<float>(rd(l, 0x2776)) + 2.0F) * scale;
      const float feetY = level3RowToY(static_cast<float>(rd(l, 0x2774)) + 32.0F);
      const SDL_Rect source{cell * cells.cellWidth, 0, cells.cellWidth, cells.cellHeight};
      const float w = static_cast<float>(cells.cellWidth) * unit;
      const float h = static_cast<float>(cells.cellHeight) * unit;
      const SDL_FRect destination{centerX - w * 0.5F, feetY + 8.0F * unit - h, w, h};
      SDL_RenderCopyF(renderer, assets.stage6Kiss, &source, &destination);
    } else {
      l6draw::drawCharlie(renderer, assets, code, rd(l, 0x2774), rd(l, 0x2776),
                          &grip);
      if (kissRow >= 0) {
        l6draw::drawPartner(renderer, assets, 3, kissRow, kissCol - 15, 16.0F);
      }
    }
  }
  // The finale's copies ($2480 + $40 k), two rows of cells each.
  if (rd(l, 0x2800) == 2 && rd(l, 0x2738) == 0) {
    for (int k = 0; k < 12; ++k) {
      const int base = 0x2480 + k * 0x40;
      const int col = rd(l, base + 6);
      const int row = rd(l, base + 4);
      const int code = rd(l, base + 0xe);
      if (col == 0 && row == 0) continue;
      if (code == 0) continue;
      l6draw::drawCharlie(renderer, assets, code, row, col);
    }
  }
  // The points popup ($2700/$2710) shows the last catch's points.
  if (rd(l, 0x271e) != 0xfe && rd(l, 0x271e) != 0 && l.score > 0 &&
      rd(l, 0x2800) == 1) {
    const float x = (static_cast<float>(rd(l, 0x2716)) + 16.0F) * scale;
    const float y = level3RowToY(static_cast<float>(rd(l, 0x2714)));
    drawText(renderer, std::to_string(l.score), x, y, 1.6F,
             color(255, 236, 120), true);
  }
  // OH NO ($2780-$27B0) and GREAT / FAR OUT ($2400-$2450).
  // The balloon callouts over their cells: two OH NO!!s on the fall, the
  // right one eight rows lower; GREAT and FAR OUT at the goal, FAR OUT
  // eight rows lower, their palette cycling every two frames.
  if (rd(l, 0x27be) == 0x16) {
    const int row = rd(l, 0x27b4);
    drawCallout(renderer, assets.calloutOhNo, rd(l, 0x27b6), row, -1);
    drawCallout(renderer, assets.calloutOhNo, rd(l, 0x2786), row + 8, -1);
  }
  if (rd(l, 0x245e) == 0x10 && rd(l, 0x2800) == 2) {
    const int row = rd(l, 0x2454);
    const int phase = (l.hdFrame / 2) & 7;
    drawCallout(renderer, assets.calloutGreat, rd(l, 0x2456), row, phase);
    drawCallout(renderer, assets.calloutFarOut, rd(l, 0x2426), row + 8, phase);
  }
}
