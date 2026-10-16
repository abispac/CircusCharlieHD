// ---- Event 5 (pony and springboards) drawing -------------------------------
// Everything is drawn from the board records of `game.level5`: the scroll is
// already folded into the springboard, plaque and goal-stand columns ($8,U,
// $2606 and $C2 are screen columns), so only the backdrop takes the camera.
// Charlie's cells never carry the mirror attribute on this stage ($20 stays
// clear in MAME), so no pose is ever flipped.

// The plaque's middle cell names the tens digit ($EF25): $C9 0, $BD 1 ... $C8 9.
std::string level5PlaqueText(const std::array<int, 3>& code) {
  // code[2] is the leftmost cell (column $28), code[0] the rightmost.
  const int leftCell = code[2];
  const int middle = code[1];
  if (leftCell == 0xb8) return "START";
  if (leftCell == 0xc2) return "GOAL";
  if (leftCell != 0xbb && leftCell != 0xca) return {};
  int digit = -1;
  switch (middle) {
    case 0xc9: digit = 0; break;
    case 0xbd: digit = 1; break;
    case 0xbe: digit = 2; break;
    case 0xbf: digit = 3; break;
    case 0xc0: digit = 4; break;
    case 0xc1: digit = 5; break;
    case 0xc5: digit = 6; break;
    case 0xc6: digit = 7; break;
    case 0xc7: digit = 8; break;
    case 0xc8: digit = 9; break;
    default: return {};
  }
  return (leftCell == 0xbb ? "1" : "") + std::to_string(digit) + "0M";
}

// The grass under the pony: its two cell rows end at $D0.
constexpr float kLevel5GroundRow = 0xd0;

// A springboard: the trampoline frame is two cells tall with its top on
// $9,U - 16 and half-width $B,U ($A46C lands Charlie with his feet on rows
// $9,U - 16 .. $9,U - 13); its pole leans away from the frame's right end
// ($A5E5 tests the diagonal band under the platform).
void drawStage5Springboard(SDL_Renderer* renderer, const Assets& assets,
                           const Level5Spring& spring) {
  (void)assets;
  // Only a live board (state 3) has its position: while the board's tiles
  // are still being drawn in from the edge (state 2) the record keeps the
  // previous board's x and row, which showed up as a phantom springboard.
  if (spring.state != 3 || spring.row == 0) return;
  const float scale = kSourceToWorldX;
  // The record's screen column is a byte: a board scrolling off the left
  // edge wraps to the right.  The unwrapped column keeps it off-screen.
  const float centerX = static_cast<float>(spring.hdX) * scale;
  if (centerX < -200.0F || centerX > kWorldWidth + 200.0F) return;
  // Measured on MAME: the frame's tiles span columns $8 - $B .. $8 + $B - 20
  // and rows $9 - 8 .. $9 + 7.  The striped bars are the top two and the
  // bottom two rows, blue-and-white checkered pillars stand at both ends
  // between them, and two grey poles with a hook run from the frame's
  // middle row down to the grass (row 199): the left one nine columns in
  // from the left end, the right one three columns past the right end.
  // A pressed board ($A4B3 writes tile $55) sinks its bottom bar.
  const float left = (static_cast<float>(spring.hdX - spring.halfB)) * scale;
  const float right = (static_cast<float>(spring.hdX + spring.halfB - 19)) * scale;
  const float halfWidth = (right - left) * 0.5F;
  const float topY = level3RowToY(static_cast<float>(spring.row) - 8.0F);
  const float bottomY = level3RowToY(static_cast<float>(spring.row) + 8.0F);
  const bool pressed = spring.tileD == 0x55;
  const float grassY = level3RowToY(200.0F);
  const SDL_Color poleLight = color(226, 226, 236);
  const SDL_Color poleDark = color(138, 138, 160);
  for (const float px : {left + 9.0F * scale, right + 3.0F * scale}) {
    const float poleTop = topY + 8.0F * kLevel3RowScale;
    fillRect(renderer, px - 2.0F, poleTop, 4.0F, grassY - poleTop, poleDark);
    fillRect(renderer, px - 2.0F, poleTop, 2.0F, grassY - poleTop, poleLight);
    // The hook: a short arm back toward the frame.
    fillRect(renderer, px - 7.0F, poleTop - 4.0F, 7.0F, 3.0F, poleLight);
    fillRect(renderer, px - 7.0F, poleTop - 1.0F, 7.0F, 2.0F, poleDark);
  }
  const float barHeight = 2.0F * kLevel3RowScale;
  const float frameBottom = pressed ? bottomY - 4.0F : bottomY;
  auto stripedBar = [&](float y) {
    fillRect(renderer, left, y, halfWidth * 2.0F, barHeight, color(255, 226, 40));
    for (float x = left; x < right; x += 8.0F * scale) {
      fillRect(renderer, x + 3.0F * scale, y, 2.0F * scale, barHeight,
               color(250, 60, 30));
    }
    fillRect(renderer, left, y + barHeight - 1.5F, halfWidth * 2.0F, 1.5F,
             color(150, 60, 20));
  };
  auto pillar = [&](float x) {
    const float w = 4.0F * scale;
    const float top = topY + barHeight;
    const float h = frameBottom - barHeight - top;
    fillRect(renderer, x, top, w, h, color(40, 110, 240));
    for (float y = top; y < top + h; y += 2.0F * kLevel3RowScale) {
      const bool odd = static_cast<int>((y - top) / (2.0F * kLevel3RowScale)) & 1;
      fillRect(renderer, x + (odd ? w * 0.5F : 0.0F), y, w * 0.5F,
               2.0F * kLevel3RowScale, color(235, 245, 255));
    }
  };
  pillar(left + 1.0F * scale);
  pillar(right - 5.0F * scale);
  stripedBar(topY);
  stripedBar(frameBottom - barHeight);
}

void drawStage5Pony(SDL_Renderer* renderer, const Assets& assets,
                    const Level5& l5) {
  if (l5.ponyCol == 0 || l5.ponyCode == 0) return;
  const float scale = kSourceToWorldX;
  const float centerX = (static_cast<float>(l5.ponyCol) + 8.0F) * scale;
  const float bottomY = level3RowToY(static_cast<float>(l5.ponyRow) + 30.0F);
  int gait = 0;
  for (std::size_t i = 0; i < kLevel5PonyCells.size(); ++i) {
    if (kLevel5PonyCells[i] == l5.ponyCode) gait = static_cast<int>(i);
  }
  // The board's pony composite is 48 columns wide and rides low on the
  // grass; the painting is drawn a little larger than that so the pair reads
  // at the size of the arcade's.
  // Measured on MAME: the pony's pixels span columns $6 - 14 .. $6 + 30 and
  // rows $4 + 1 .. $4 + 29, so it is 45 columns wide and 29 rows tall,
  // centred eight columns past its record column.
  const float width = 45.0F * scale;
  const float height = 29.0F * kLevel3RowScale;
  const float ponyCenter = (static_cast<float>(l5.ponyCol) + 8.0F) * scale;
  const float ponyBottom = level3RowToY(static_cast<float>(l5.ponyRow) + 30.0F);
  if (assets.stage5Pony) {
    drawSheetFrame(renderer, assets.stage5Pony, 4, 2, gait * 2, ponyCenter,
                   ponyBottom, width * 1.08F, height * 1.08F);
    return;
  }
  fillRect(renderer, centerX - width * 0.5F, bottomY - height * 0.6F, width,
           height * 0.6F, color(240, 240, 240));
}

// Charlie by his leading cell code:
//   $00/$04/$08/$0C  riding (one per pony gait)        -> seated crouch
//   $19              reaching for a platform edge      -> stride
//   $29              take-off and the landing          -> the jump
//   $31              sitting on the grass (OH NO)      -> sitting
//   $95-$AC          the tumble after a double bounce  -> the roll
//   $AD/$B1          cheering on the goal stand        -> cheer
void drawStage5Charlie(SDL_Renderer* renderer, const Assets& assets,
                       const Level5& l5) {
  if (l5.chCol == 0 && l5.chRow == 0) return;
  const float scale = kSourceToWorldX;
  // Measured on MAME: riding, Charlie's pixels span columns $6 - 9 .. $6 + 9
  // and rows $4 + 2 .. $4 + 28 (19 by 27), so he is centred on his record
  // column with his shoes 29 rows below its row; the atlases' standing
  // frames are about 220 px tall.
  const float centerX = static_cast<float>(l5.chCol) * scale;
  const float feetY = level3RowToY(static_cast<float>(l5.chRow) + 29.0F);
  const int code = l5.chCode;
  const float unit = 27.0F * kLevel3RowScale / 222.0F;
  if (code >= 0x95 && code <= 0xac) {
    // The tumble after a double bounce: six somersault frames.
    const int step = code - 0x95;
    drawStage2Pose(renderer, assets.stage6CharlieTumble, assets.stage6CharlieTumbleCells,
                   step % 6, centerX, feetY, unit);
    return;
  }
  if (code == 0xad || code == 0xb1) {
    // Cheering on the drum: $AD arms up, $B1 the other cell; the HD cheer
    // and a wave alternate.
    // The painted drum's top surface sits about 20 units under its top edge.
    drawStage2Pose(renderer, assets.stage6CharlieFloor, assets.stage6CharlieFloorCells,
                   2, centerX, feetY + 3.0F * kLevel3RowScale + 20.0F, unit);
    return;
  }
  if (code == 0x31) {
    // Sitting on the grass after the fall, dazed.
    drawStage2Pose(renderer, assets.stage5CharlieSit, assets.stage5CharlieSitCells,
                   0, centerX, feetY + 6.0F, unit);
    return;
  }
  // The jump is the ball stage's: the cabinet leaps the same way on both,
  // so the $E9B4 cells' painted frames serve here.  The rider crouches 27
  // rows tall, which is a 33-row figure standing, so the standing frame
  // sets that scale and the split-leg leap comes out the rider's size.
  const SheetCells& jumpCells = assets.stage4CharlieCells;
  const float jumpUnit =
      jumpCells.valid() && jumpCells.bounds[0].h > 0
          ? 33.0F * kLevel3RowScale / static_cast<float>(jumpCells.bounds[0].h)
          : unit;
  if (code == 0x19) {
    // Reaching for the platform edge.
    drawStage2Pose(renderer, assets.stage4Charlie, jumpCells, 5, centerX, feetY, jumpUnit);
    return;
  }
  if (code == 0x29) {
    // The leap: legs split while rising, the other split on the way down,
    // knees bent as the cells have only just landed.
    int frame = 6;
    if (l5.hdRising) {
      frame = l5.hdAir < 3 ? 8 : 5;
    } else if (l5.hdAir < 3) {
      frame = 8;
    }
    drawStage2Pose(renderer, assets.stage4Charlie, jumpCells, frame, centerX,
                   feetY + 2.0F, jumpUnit);
    return;
  }
  // Riding: one seated pose per pony gait, rocking slightly.
  const int gait = (code >> 2) & 3;
  const float bob = (gait == 1 || gait == 2) ? -3.0F : 0.0F;
  drawStage2Pose(renderer, assets.stage5CharlieRide, assets.stage5CharlieRideCells,
                 gait, centerX, feetY + bob, unit);
}

// The six popup cells: the OH NO callout ($A5FE) or the GREAT / points cells
// of the goal ($A84F), drawn once per word.
void drawStage5Popups(SDL_Renderer* renderer, const Level5& l5) {
  if (l5.popupRow[0] == 0) return;
  const float scale = kSourceToWorldX;
  const float y = level3RowToY(static_cast<float>(l5.popupRow[0]));
  const float leftX = (static_cast<float>(l5.popupCol[0]) + 24.0F) * scale;
  const float rightX = (static_cast<float>(l5.popupCol[3]) + 24.0F) * scale;
  if (l5.popupCode[0] == 0x16) {
    drawText(renderer, "OH", leftX, y, 2.0F, color(255, 255, 255), true);
    drawText(renderer, "NO!", rightX, y, 2.0F, color(255, 255, 255), true);
    return;
  }
  if (l5.popupCode[0] == 0x10) {
    drawText(renderer, "GREAT", leftX, y, 2.0F, color(255, 236, 120), true);
    drawText(renderer, "FAR OUT", rightX, y, 2.0F, color(255, 236, 120),
             true);
  }
}

// The goal stand ($A707-$A795): drawn column by column from the top of page
// $12; its column $C2 is where Charlie's feet must come down.
void drawStage5GoalStand(SDL_Renderer* renderer, const Assets& assets,
                         const Level5& l5) {
  const int state = l5.v[0xbd - 0xb0];
  if (state < 2) return;
  const float scale = kSourceToWorldX;
  // $A72C draws eight columns of eight; $C2 ends up as
  // ($BF & 7) + $D8 - $BF + low with $BF eight columns below its entry
  // value, so while the columns are still coming in the same formula runs
  // with the entry value recovered from the columns left ($BE).
  int column;
  if (state == 2) {
    const int bf = l5.v[0xbf - 0xb0];
    const int be = l5.v[0xbe - 0xb0];
    const int low = l5.scroll & 0xff;
    column = ((bf & 7) + 0xd8 + 0x40 - bf - 8 * (8 - be) + low) & 0xff;
  } else {
    column = l5.v[0xc2 - 0xb0];
  }
  const float centerX = static_cast<float>(column) * scale;
  if (centerX < -200.0F || centerX > kWorldWidth + 200.0F) return;
  // The cabinet's goal is the pink-and-white drum (60 columns wide, rows
  // 170-212, Charlie's shoes on its top at row 170); the Event 3 goal
  // painting is that drum.
  const float topY = level3RowToY(170.0F);
  const float bottomY = level3RowToY(212.0F);
  if (assets.stage3GoalTambourine) {
    const float width = 60.0F * scale;
    const SDL_FRect destination{centerX - width * 0.5F, topY, width, bottomY - topY};
    SDL_RenderCopyF(renderer, assets.stage3GoalTambourine, nullptr, &destination);
    return;
  }
  fillRect(renderer, centerX - 64.0F, topY, 128.0F, bottomY - topY,
           color(240, 150, 200));
}

void drawStage5Scene(SDL_Renderer* renderer, const Game& game,
                     const Assets& assets, double timeSeconds,
                     double interpolation) {
  const Level5& l5 = game.level5;
  const float camera = game.previousCameraX +
                       (game.cameraX - game.previousCameraX) *
                           static_cast<float>(interpolation);
  drawBackdrop(renderer, camera, false, assets, game, timeSeconds, false);
  const float scale = kSourceToWorldX;
  const bool restarting =
      game.scene == Scene::Crashed && game.crashFrame >= 24;

  // The distance plaque: three cells at $2606/$2616/$2626 (right to left).
  if (l5.plRow[0] != 0) {
    const std::string text = level5PlaqueText(l5.plCode);
    if (!text.empty()) {
      const float x = (static_cast<float>(l5.plCol[1]) + 8.0F) * scale;
      if (x > -80.0F && x < kWorldWidth + 80.0F) {
        drawFloorPlaque(renderer, x, text, text == "START");
      }
    }
  }
  drawHud(renderer, game, assets.charlieLife);
  if (restarting) return;

  drawStage5GoalStand(renderer, assets, l5);
  for (const auto& spring : l5.springs) {
    drawStage5Springboard(renderer, assets, spring);
  }
  drawStage5Pony(renderer, assets, l5);
  drawStage5Charlie(renderer, assets, l5);
  drawStage5Popups(renderer, l5);
  if (l5.birdActive && assets.bird) {
    // The extra-life bird ($A6A6) flies in from the right with the bag.
    const float birdX = static_cast<float>(l5.birdCol) * scale;
    const float birdY = level3RowToY(0x40) + 60.0F;
    int textureWidth = 0;
    int textureHeight = 0;
    SDL_QueryTexture(assets.bird, nullptr, nullptr, &textureWidth,
                     &textureHeight);
    const int cellWidth = textureWidth / 4;
    const int flap = (l5.birdCol / 8) & 1;
    const SDL_Rect source{flap * cellWidth, 0, cellWidth, textureHeight};
    const SDL_FRect destination{birdX - 45.0F, birdY - 60.0F, 90.0F, 120.0F};
    SDL_RenderCopyF(renderer, assets.bird, &source, &destination);
    if (assets.rewardBag) {
      const SDL_FRect bag{birdX - 24.0F, birdY + 30.0F, 48.0F, 59.0F};
      SDL_RenderCopyF(renderer, assets.rewardBag, nullptr, &bag);
    }
  }}
