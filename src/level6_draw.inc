// ---- Event 6 (the trapeze) drawing ------------------------------------------
// The board's sprite table ($2400-$27FF) and the goal / trampoline positions
// are read straight from the RAM mirror: rope cells are drawn where the
// board puts them, Charlie's four cells name his pose, and the tilemap
// objects (trampolines, the goal platform) come from their records.

namespace l6draw {

inline int rd(const Level6& l, int a) { return l.ram[static_cast<std::size_t>(a & 0x3fff)]; }

// A Charlie pose by the code of his leading (bottom-left) cell.
//   $25/$39/$3D/$41/$F0/$F4  hanging, by swing phase
//   $F8                      airborne
//   $29                      the finale's jump
//   $2D                      flat on the floor
//   $31                      sitting on the floor
//   $74/$76/$7C/$7E          on the goal platform, with the bag
//   $95-$AC                  the finale's tumble
//   $AD/$B1                  cheering
inline void drawCharlie(SDL_Renderer* renderer, const Assets& assets, int code,
                        int row, int col) {
  const float scale = kSourceToWorldX;
  const float centerX = (static_cast<float>(col) + 16.0F) * scale;
  const float feetY = level3RowToY(static_cast<float>(row) + 32.0F);
  if (code >= 0x95 && code <= 0xac) {
    drawSheetFrame(renderer, assets.stage3Charlie, 4, 3, 4 + ((code - 0x95) & 3),
                   centerX, feetY, 110.0F, 110.0F);
    return;
  }
  switch (code) {
    case 0x25: case 0x39: case 0x3d: case 0x41: case 0xf0: case 0xf4: {
      // Hanging: the upright arms-out cells, hands at the bar.
      static constexpr std::array<int, 6> kFrames{0, 1, 2, 3, 2, 1};
      int index = 0;
      switch (code) {
        case 0x39: index = 1; break;
        case 0x3d: index = 2; break;
        case 0x41: index = 3; break;
        case 0xf0: index = 4; break;
        case 0xf4: index = 5; break;
        default: break;
      }
      drawSheetFrame(renderer, assets.stage4Charlie, 4, 3,
                     kFrames[static_cast<std::size_t>(index)], centerX,
                     feetY + 6.0F, 100.0F, 100.0F);
      return;
    }
    case 0xf8:
    case 0x29:
      drawSheetFrame(renderer, assets.stage4Charlie, 4, 3, 5, centerX, feetY,
                     104.0F, 104.0F);
      return;
    case 0x2d:
      drawSheetFrame(renderer, assets.stage3Charlie, 4, 3, 11, centerX,
                     feetY + 4.0F, 110.0F, 110.0F);
      return;
    case 0x31:
      drawSheetFrame(renderer, assets.stage4Charlie, 4, 3, 9, centerX,
                     feetY + 4.0F, 100.0F, 100.0F);
      return;
    case 0x76: case 0x7e:
      drawStage2Pose(renderer, assets.stage2Charlie, assets.stage2CharlieCells,
                     0, centerX, feetY + 2.0F, kStage2CharlieUnitsPerPixel);
      return;
    case 0x74: case 0x7c: case 0xad: case 0xb1:
      drawStage2Pose(renderer, assets.stage2Charlie, assets.stage2CharlieCells,
                     5, centerX, feetY + 2.0F, kStage2CharlieUnitsPerPixel);
      return;
    case 0x2e:
      drawSheetFrame(renderer, assets.stage3Charlie, 4, 3, 7, centerX, feetY,
                     104.0F, 104.0F);
      return;
    default:
      drawStage2Pose(renderer, assets.stage2Charlie, assets.stage2CharlieCells,
                     0, centerX, feetY + 2.0F, kStage2CharlieUnitsPerPixel);
      return;
  }
}

// One rope slot: sixteen cells, the bar in the first ($24FE); a cell with
// column zero is off the screen.  Beads take their colour from the attribute
// ($B4CB: 1 fresh, 6 after the swings pile up).
inline void drawRope(SDL_Renderer* renderer, const Assets& assets,
                     const Level6& l, int slot) {
  const int base = 0x2400 + slot * 0x100;
  const float scale = kSourceToWorldX;
  // Top to bottom so the bar overlaps the beads.
  for (int cell = 15; cell >= 0; --cell) {
    const int a = base + 0xf0 - cell * 0x10;
    const int col = rd(l, a + 6);
    const int row = rd(l, a + 4);
    const int code = rd(l, a + 0xe);
    const int attr = rd(l, a + 0xf);
    if (col == 0 || code == 0 || code == 0xfe) continue;
    const float x = (static_cast<float>(col) + 8.0F) * scale;
    const float y = level3RowToY(static_cast<float>(row) + 8.0F);
    if (code == 0x55 || (code >= 0x57 && code <= 0x7a)) {
      const SDL_Color bead = attr == 6 ? color(255, 170, 60) : color(232, 40, 44);
      const SDL_Color shine = attr == 6 ? color(255, 230, 150) : color(255, 120, 120);
      filledCircle(renderer, x, y, 9.0F, bead);
      filledCircle(renderer, x - 3.0F, y - 3.0F, 3.0F, shine);
      if (code != 0x55) {
        // The decorated bar's extra cells: a second bead row.
        filledCircle(renderer, x, y + 12.0F, 7.0F, bead);
      }
      continue;
    }
    if (code == 0x56) {
      // The bar seen end-on: the knob and a stub of the white bar.
      fillRect(renderer, x - 14.0F, y - 4.0F, 28.0F, 8.0F, color(236, 236, 240));
      filledCircle(renderer, x, y, 10.0F, color(250, 205, 40));
      filledCircle(renderer, x - 3.0F, y - 3.0F, 3.5F, color(255, 245, 190));
      continue;
    }
  }
  (void)assets;
}

// A trampoline at its record's column (low byte; the high byte off the
// page).  The board draws it as six tile rows on the floor.
inline void drawTrampoline(SDL_Renderer* renderer, const Assets& assets,
                           const Level6& l, int record) {
  if (rd(l, record) != 0) return;
  const int low = rd(l, record + 1);
  const float scale = kSourceToWorldX;
  const float centerX = (static_cast<float>(low) - 44.0F) * scale;
  const float bottomY = level3RowToY(0xf0);
  const float width = 60.0F * scale;
  const float height = width * 299.0F / 474.0F;
  if (assets.stage6Trampoline) {
    const SDL_FRect destination{centerX - width * 0.5F, bottomY - height,
                                width, height};
    SDL_RenderCopyF(renderer, assets.stage6Trampoline, nullptr, &destination);
    return;
  }
  fillRect(renderer, centerX - width * 0.5F, bottomY - 30.0F, width, 30.0F,
           color(40, 90, 220));
}

inline void drawGoalPlatform(SDL_Renderer* renderer, const Assets& assets,
                             const Level6& l) {
  if (rd(l, 0x20df) != 0) return;
  const int low = rd(l, 0x20e0);
  const float scale = kSourceToWorldX;
  const float centerX = (static_cast<float>(low) - 0x18 + 16.0F) * scale;
  const float topY = level3RowToY(0x88);
  if (assets.goalPlatform) {
    const SDL_FRect destination{centerX - 86.0F, topY, 172.0F, 48.0F};
    SDL_RenderCopyF(renderer, assets.goalPlatform, nullptr, &destination);
  } else {
    fillRect(renderer, centerX - 80.0F, topY, 160.0F, 40.0F, color(200, 60, 60));
  }
  // The post under it, down to the floor.
  fillRect(renderer, centerX - 10.0F, topY + 40.0F, 20.0F,
           level3RowToY(0xf0) - topY - 40.0F, color(120, 120, 140));
  // The marker above ($2720/$2730: codes $70/$78).
  const int markerCol = rd(l, 0x2736);
  if (markerCol != 0) {
    const float mx = (static_cast<float>(markerCol) + 8.0F) * scale;
    const float my = level3RowToY(static_cast<float>(rd(l, 0x2734)));
    fillRect(renderer, mx - 3.0F, my, 6.0F, 80.0F, color(230, 230, 240));
    const std::array<SDL_Vertex, 3> flag{{
        {{mx + 3.0F, my}, color(240, 60, 60), {0, 0}},
        {{mx + 40.0F, my + 14.0F}, color(240, 60, 60), {0, 0}},
        {{mx + 3.0F, my + 28.0F}, color(240, 60, 60), {0, 0}},
    }};
    SDL_RenderGeometry(renderer, nullptr, flag.data(), 3, nullptr, 0);
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
  drawBackdrop(renderer, camera, false, assets, game, timeSeconds, false);
  const float scale = kSourceToWorldX;
  // The night: the arcade's sky above the crowd is black, so the stands are
  // dimmed behind the trapezes.
  const float trussY = level3RowToY(0x50);
  {
    const float bottom = level3RowToY(0x8c);
    const SDL_Color night = color(4, 6, 20, 170);
    const std::array<SDL_Vertex, 4> quad{{
        {{0.0F, trussY}, night, {0, 0}},
        {{kWorldWidth, trussY}, night, {0, 0}},
        {{kWorldWidth, bottom}, night, {0, 0}},
        {{0.0F, bottom}, night, {0, 0}},
    }};
    const int indices[]{0, 1, 2, 0, 2, 3};
    SDL_RenderGeometry(renderer, nullptr, quad.data(), 4, indices, 6);
  }
  fillRect(renderer, 0.0F, trussY - 14.0F, kWorldWidth, 14.0F, color(70, 70, 82));
  for (float x = 0.0F; x < kWorldWidth; x += 24.0F) {
    line(renderer, x, trussY - 14.0F, x + 12.0F, trussY, color(120, 120, 136));
    line(renderer, x + 12.0F, trussY - 14.0F, x + 24.0F, trussY, color(120, 120, 136));
  }
  const bool restarting =
      game.scene == Scene::Crashed && game.crashFrame >= 24;

  // The plaque rides the scroll on the fence.
  {
    const std::string text = l6draw::plaqueText(l);
    const int col = rd(l, 0x27d6);
    if (!text.empty() && rd(l, 0x27e4) != 0) {
      const float x = (static_cast<float>(col) + 8.0F) * scale;
      drawFloorPlaque(renderer, x, text, text == "START");
    }
  }
  drawHud(renderer, game, assets.charlieLife);
  if (restarting) return;

  for (int record : {0x20e4, 0x20e9, 0x20ee}) {
    l6draw::drawTrampoline(renderer, assets, l, record);
  }
  l6draw::drawGoalPlatform(renderer, assets, l);
  // The ropes are cleared at the goal ($B556) and their slots then hold the
  // finale's cells.
  if (rd(l, 0x2800) != 2) {
    for (int slot = 2; slot >= 0; --slot) l6draw::drawRope(renderer, assets, l, slot);
  }

  // Charlie ($2740-$2770).
  if (rd(l, 0x2776) != 0 || rd(l, 0x2774) != 0) {
    l6draw::drawCharlie(renderer, assets, rd(l, 0x277e), rd(l, 0x2774),
                        rd(l, 0x2776));
  }
  // The finale's copies ($2480 + $40 k), two rows of cells each.
  if (rd(l, 0x2800) == 2 && rd(l, 0x2738) == 0) {
    // $B67D onwards: the twelve copies.
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
  if (rd(l, 0x2800) == 2) {
    // The bag ($2730 while it flies, then $2470).
    for (int base : {0x2730, 0x2470}) {
      const int code = rd(l, base + 0xe);
      const int col = rd(l, base + 6);
      if (col == 0 || (code != 0x58 && code != 0x55 && code != 0x5a)) continue;
      if (assets.rewardBag) {
        const float x = (static_cast<float>(col) + 8.0F) * scale;
        const float y = level3RowToY(static_cast<float>(rd(l, base + 4)));
        const SDL_FRect bag{x - 24.0F, y, 48.0F, 59.0F};
        SDL_RenderCopyF(renderer, assets.rewardBag, nullptr, &bag);
      }
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
  if (rd(l, 0x27be) == 0x16) {
    const float y = level3RowToY(static_cast<float>(rd(l, 0x27b4)));
    drawText(renderer, "OH", (static_cast<float>(rd(l, 0x27b6)) + 24.0F) * scale,
             y, 2.0F, color(255, 255, 255), true);
    drawText(renderer, "NO!", (static_cast<float>(rd(l, 0x2786)) + 24.0F) * scale,
             y, 2.0F, color(255, 255, 255), true);
  }
  if (rd(l, 0x245e) == 0x10 && rd(l, 0x2800) == 2) {
    const float y = level3RowToY(static_cast<float>(rd(l, 0x2454)));
    drawText(renderer, "GREAT", (static_cast<float>(rd(l, 0x2456)) + 24.0F) * scale,
             y, 2.0F, color(255, 236, 120), true);
    drawText(renderer, "FAR OUT", (static_cast<float>(rd(l, 0x2426)) + 24.0F) * scale,
             y, 2.0F, color(255, 236, 120), true);
  }
}
