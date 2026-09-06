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
  if (spring.state < 2 || spring.row == 0) return;
  const float scale = kSourceToWorldX;
  const float centerX = static_cast<float>(spring.x) * scale;
  if (centerX < -200.0F || centerX > kWorldWidth + 200.0F) return;
  const float halfWidth = static_cast<float>(spring.halfB) * scale;
  const float bottomY = level3RowToY(static_cast<float>(spring.row));
  const float groundY = level3RowToY(kLevel5GroundRow);
  const float frameHeight = 16.0F * 2.5F;
  const bool pressed = spring.tileE != 0;

  // The pole: from the frame's right end down to the grass, leaning right.
  const float poleTopX = centerX + halfWidth - 6.0F;
  const float poleBottomX = poleTopX + 10.0F;
  const SDL_Color poleLight = color(214, 214, 226);
  const SDL_Color poleDark = color(132, 132, 150);
  for (int i = -2; i <= 2; ++i) {
    line(renderer, poleTopX + static_cast<float>(i), bottomY - 4.0F,
         poleBottomX + static_cast<float>(i), groundY,
         i == 2 ? poleDark : poleLight);
  }

  const float drawHeight = pressed ? frameHeight * 0.62F : frameHeight;
  if (assets.stage5Springboard) {
    const SDL_FRect destination{centerX - halfWidth, bottomY - drawHeight,
                                halfWidth * 2.0F, drawHeight};
    SDL_RenderCopyF(renderer, assets.stage5Springboard, nullptr, &destination);
    return;
  }
  // Vector fallback: two striped bars on checkered posts.
  const SDL_Color yellow = color(255, 226, 74);
  const SDL_Color blue = color(46, 120, 247);
  fillRect(renderer, centerX - halfWidth, bottomY - drawHeight,
           halfWidth * 2.0F, 6.0F, yellow);
  fillRect(renderer, centerX - halfWidth, bottomY - 6.0F, halfWidth * 2.0F,
           6.0F, yellow);
  fillRect(renderer, centerX - halfWidth, bottomY - drawHeight, 8.0F,
           drawHeight, blue);
  fillRect(renderer, centerX + halfWidth - 8.0F, bottomY - drawHeight, 8.0F,
           drawHeight, blue);
}

// The pony: three cells wide (48 source columns) and two tall, its leading
// cell at $2676 / $2674.  The sheet's eight frames pair up with the four
// board gaits ($EECA).
void drawStage5Pony(SDL_Renderer* renderer, const Assets& assets,
                    const Level5& l5) {
  if (l5.ponyCol == 0 || l5.ponyCode == 0) return;
  const float scale = kSourceToWorldX;
  const float centerX = (static_cast<float>(l5.ponyCol) + 24.0F) * scale;
  const float bottomY = level3RowToY(static_cast<float>(l5.ponyRow) + 32.0F);
  int gait = 0;
  for (std::size_t i = 0; i < kLevel5PonyCells.size(); ++i) {
    if (kLevel5PonyCells[i] == l5.ponyCode) gait = static_cast<int>(i);
  }
  const float width = 56.0F * scale;
  const float height = width * 244.0F / 370.0F;
  if (assets.stage5Pony) {
    drawSheetFrame(renderer, assets.stage5Pony, 4, 2, gait * 2, centerX,
                   bottomY + 2.0F, width, height);
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
  const float centerX = (static_cast<float>(l5.chCol) + 16.0F) * scale;
  const float feetY = level3RowToY(static_cast<float>(l5.chRow) + 32.0F);
  const int code = l5.chCode;
  const float unit = kStage2CharlieUnitsPerPixel;
  if (code >= 0x95 && code <= 0xac) {
    // The tumble: the stage-3 sheet's somersault row.
    const int step = code - 0x95;
    drawSheetFrame(renderer, assets.stage3Charlie, 4, 3, 4 + (step & 3),
                   centerX, feetY - 6.0F, 110.0F, 110.0F);
    return;
  }
  if (code == 0xad || code == 0xb1) {
    drawSheetFrame(renderer, assets.stage3Charlie, 4, 3,
                   code == 0xad ? 11 : 10, centerX, feetY + 4.0F, 104.0F,
                   104.0F);
    return;
  }
  if (code == 0x31) {
    // Sitting on the grass after the fall.
    drawSheetFrame(renderer, assets.stage4Charlie, 4, 3, 9, centerX,
                   feetY + 6.0F, 100.0F, 100.0F);
    return;
  }
  if (code == 0x19) {
    drawSheetFrame(renderer, assets.stage4Charlie, 4, 3, 6, centerX, feetY,
                   104.0F, 104.0F);
    return;
  }
  if (code == 0x29) {
    drawSheetFrame(renderer, assets.stage4Charlie, 4, 3, 5, centerX,
                   feetY + 2.0F, 104.0F, 104.0F);
    return;
  }
  // Riding: the crouch sits into the saddle; the gait rocks him slightly.
  const int gait = (code >> 2) & 3;
  const float bob = (gait == 1 || gait == 2) ? -3.0F : 0.0F;
  (void)unit;
  drawSheetFrame(renderer, assets.stage3Charlie, 4, 3, gait & 1 ? 2 : 1,
                 centerX, feetY + 16.0F + bob, 100.0F, 100.0F);
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
  const float topY = level3RowToY(0xa1);
  if (assets.goalPlatform) {
    const SDL_FRect destination{centerX - 86.0F, topY, 172.0F, 48.0F};
    SDL_RenderCopyF(renderer, assets.goalPlatform, nullptr, &destination);
    return;
  }
  fillRect(renderer, centerX - 80.0F, topY, 160.0F, 40.0F,
           color(200, 60, 60));
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
