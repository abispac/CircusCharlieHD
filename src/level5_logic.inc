// Direct-page accessors by address.
#define L5V(addr) (l5.v[(addr) - 0xb0])

// $9FF9: the stage init.
void level5Init(Game& game) {
  Level5& l5 = game.level5;
  for (int i = 0; i < 0x13; ++i) l5.v[i] = 0;
  l5.phase = 0;
  // Charlie's four cells ($2680-$26B0): rows $9C / $AC, columns $30 / $40.
  l5.chRow = 0x9c;
  l5.chFrac = 0;   // $B9D9 clears the sprite table before $9FF9 runs
  L5V(0xb7) = 0xb0;
  // The pony's six cells ($2620-$2670): rows $B0 / $C0.
  l5.ponyRow = 0xb0;
  for (auto& r : l5.plRow) r = 0xe0;
  l5.chCol = 0x30;
  l5.ponyCol = 0x2c;
  // $A061: plaque columns $2616 = $2C, $2606 = $3C, $25F6 = $4C.
  l5.plCol = {0x48, 0x38, 0x28};
  l5.chCode = 0;
  l5.ponyCode = kLevel5PonyCells[0];
  const int page = (l5.scroll >> 8) & 0xff;
  for (int k = 0; k < 3; ++k) {
    l5.plCode[static_cast<std::size_t>(k)] =
        kLevel5PlaqueCells[static_cast<std::size_t>(3 * page + (2 - k))];
  }
  for (auto& s : l5.springs) s = Level5Spring{};
  l5.springs[0].state = 1;
  l5.springs[0].gap = 8;
  l5.chain &= 0xfe;   // LSR / ASL $220B clears bit 0
  // $BB25: the bonus.
  if (!l5.started) {
    game.bonus = kLevel5FirstBonus;
    l5.started = true;
  } else {
    const int b = page >> 1;
    game.bonus = b < 2 ? kLevel5RestartBonus[0]
                       : (b < 4 ? kLevel5RestartBonus[1] : kLevel5RestartBonus[2]);
  }
  l5.bonusHold = 0;
  l5.timeout = 0x40;
  // $A0E3: DEC $2203 -- the first scroll step brings the page back.
  l5.scroll = ((l5.scroll - 0x100) & 0xffff);
  l5.phase = 1;
  l5.died = false;
  l5.finished = false;
  l5.popupCode = {0, 0, 0, 0, 0, 0};
  l5.birdActive = false;
}

// The processed joystick byte the board keeps at $2032: bit 0 LEFT, bit 1
// RIGHT, bit 4 the button, active high.
int level5Speed(int input) {
  int b = 0xff;
  if ((input & 0x01) == 0) {
    --b;
    if ((input & 0x02) != 0) --b;
  }
  return b & 0xff;
}

void level5SetCharlieCells(Level5& l5, int code) { l5.chCode = code & 0xff; }
void level5SetPonyCells(Level5& l5) {
  l5.ponyCode = kLevel5PonyCells[static_cast<std::size_t>(L5V(0xb3) & 3)];
}

// $A592: the compressed platform tiles when Charlie presses on it -- $D,U is
// the given cell, $E,U comes from $EEF0[B + type - $12].
// 6809 ADD/ADC followed by DAA: a + b + carryIn in packed BCD; carry out in
// `carry`.
int bcdAdd(int a, int b, int carryIn, int& carry) {
  const int raw = a + b + carryIn;
  const int halfCarry = ((a & 0x0f) + (b & 0x0f) + carryIn) > 0x0f;
  int result = raw & 0xff;
  int c = raw > 0xff ? 1 : 0;
  int correction = 0;
  const int lsn = result & 0x0f;
  const int msn = (result >> 4) & 0x0f;
  if (halfCarry || lsn > 9) correction |= 0x06;
  if (c || msn > 9 || (msn > 8 && lsn > 9)) correction |= 0x60;
  const int fixed = result + correction;
  if (fixed > 0xff) c = 1;
  carry = c;
  return fixed & 0xff;
}

void level5PressPlatform(Level5Spring& s, int d) {
  // $EEF0: the pressed-platform tile, indexed by the low byte of D plus the
  // springboard type (0x12-0x17).
  static constexpr std::array<int, 9> kTable{0x52, 0x52, 0xcb, 0x56, 0xce,
                                             0xcd, 0xca, 0xc8, 0xc8};
  s.tileD = (d >> 8) & 0xff;
  const int b = (d & 0xff) + (s.type - 0x12);
  s.tileE = kTable[static_cast<std::size_t>(std::clamp(b, 0, 8))];
}

// $A5FE: Charlie is knocked down.
void level5Fall(Game& game) {
  Level5& l5 = game.level5;
  // The six OH NO cells: rows $52 / $5A, columns $30-$50 and $90-$B0.
  l5.popupRow = {0x52, 0x52, 0x52, 0x5a, 0x5a, 0x5a};
  l5.popupCol = {0x30, 0x40, 0x50, 0x90, 0xa0, 0xb0};
  l5.popupCode = {0x16, 0x17, 0x18, 0x16, 0x17, 0x18};
  L5V(0xb4) = 0;
  L5V(0xb5) = 0;
  l5.chFrac = 0;   // $A64B STA $26B5
  l5.phase = 2;
  L5V(0xb9) = L5V(0xb9) + 1;
  L5V(0xb2) = u8w(0x2c - L5V(0xb2) + (l5.scroll & 0xff));
  ++game.stage5FallAudioSerial;   // $A65C stops the music, $A65F plays $4A
}

// $9F24: the bookkeeping the restart runs: live springboards come off the
// spawn count and the page steps back.
void level5RestartBookkeeping(Game& game) {
  Level5& l5 = game.level5;
  int a = 0;
  for (const auto& s : l5.springs) {
    if (s.state >= 2) --a;
  }
  l5.spawnCount = u8w(l5.spawnCount + a);
  int page = (l5.scroll >> 8) & 0xff;
  if (page != 0) {
    page = u8w(page - 1);
    if (page >= 0x12) page = 0x11;
  }
  l5.scroll = (page << 8);   // $2204 is cleared by the wipe
  l5.restartPage = page;
  l5.died = true;
}

// ---- $A2E4: one springboard record --------------------------------------

void level5Spring1(Game& game, std::size_t index) {   // $A328 waiting
  Level5& l5 = game.level5;
  Level5Spring& s = l5.springs[index];
  if (s.gap >= 3) return;
  // $A32E: in a game ($2022 set once a credit is taken) the schedule starts
  // at the difficulty dips ($202F bits 5-6, halved) plus $10 per earlier
  // visit; the attract mode uses $10 flat.
  int a = (((l5.dips & 0x60) >> 1) + ((l5.visits - 1) << 4)) & 0xff;
  a = u8w(a + l5.spawnCount);
  if (a >= 0x68) a = (a & 7) + 0x60;
  const std::size_t entry = static_cast<std::size_t>(a);
  if (l5.chain == 1) {
    // $A35B: the second springboard copies the first one's platform.
    const Level5Spring& prev = l5.springs[(index + 1) & 7];
    s.type = prev.type;
    s.height = 7;
    s.cols = 7;
    s.halfC = 0x25;
    s.halfA = 0x1e;
    s.halfB = 0x21;
  } else {
    s.height = kLevel5Heights[entry];
    s.cols = 7;
    int b = u8w(((s.height - 1) << 2) + 9);
    s.halfB = b;
    s.halfA = u8w(b - 3);
    s.halfC = u8w(b + 4);   // SUBB #$03 then ADDB #$07
    s.type = kLevel5Types[entry];
  }
  const int low = l5.scroll & 0xff;
  s.col = low;
  // $A3A0: the tilemap column of the entry edge.
  int d = ((low & 0xf8) << 2) + s.type + 0x3400;
  s.vram = d & 0xffff;
  Level5Spring& next = l5.springs[(index + 7) & 7];   // U - $10
  int gap = kLevel5Gaps[entry];
  if (l5.chain == 0) {
    l5.chain = u8w(l5.chain + 1);
    gap = 0x34;
  }
  next.state = u8w(next.state + 1);
  next.gap = gap;
  s.state = u8w(s.state + 1);
  s.points = 0;
  l5.spawnCount = u8w(l5.spawnCount + 1);
}

void level5Spring2(Game& game, std::size_t index) {   // $A3CB drawing
  Level5& l5 = game.level5;
  Level5Spring& s = l5.springs[index];
  const int low = l5.scroll & 0xff;
  if (u8w(s.col - low) < 8) return;
  s.col = u8w(s.col - 8);
  // The tile writes themselves are not modelled; only the pointer walk.
  int d = (s.vram - 0x3420) & 0xffff;
  d = ((d & 0x03ff) | 0x3400);
  s.vram = d;
  s.cols = u8w(s.cols - 1);
  if (s.cols != 0) return;
  d = (d + 0xcce0) & 0xffff;
  d = ((d & 0x03ff) | 0x3400);
  s.vram = d;
  s.x = u8w(s.height << 2);
  s.x = u8w((s.col & 7) + 0xf3 - s.col + low - s.x);
  s.hdX = s.x;
  s.row = u8w((s.type + 1) << 3);
  s.cols = 7;
  s.col = u8w(s.col + 0x38);
  s.state = u8w(s.state + 1);
}

void level5Collide(Game& game, std::size_t index);

void level5Spring3(Game& game, std::size_t index) {   // $A2EC live
  Level5& l5 = game.level5;
  level5Collide(game, index);
  Level5Spring& s = l5.springs[index];
  const int low = l5.scroll & 0xff;
  if (u8w(low - s.col) > 8) return;
  s.col = u8w(s.col - 8);
  int d = (s.vram - 0x3420) & 0xffff;
  s.vram = ((d & 0x03ff) | 0x3400);
  s.cols = u8w(s.cols - 1);
  if (s.cols != 0) return;
  s.state = 0;
  l5.chain = 2;
}

// $A46C: Charlie against a live springboard.
void level5Collide(Game& game, std::size_t index) {
  Level5& l5 = game.level5;
  Level5Spring& s = l5.springs[index];
  const int b7 = L5V(0xb7);
  // |a - c| < w, written the way $A48D does it (ASL / CMP / LSR).
  const auto within = [](int value, int centre, int width) {
    const int a = u8w(value - centre + width);
    return a < u8w(width << 1);
  };
  if (L5V(0xb0) != 0) {
    // Airborne: reach for the platform edge.
    if (u8w(s.x - s.halfB - 0x48) < 3) level5SetCharlieCells(l5, 0x19);
    if (within(s.x, 0x40, s.halfA)) {
      if (u8w(s.row - b7 - 0x0d) < 4) {
        if (L5V(0xb4) & 0x80) {
          // $A4AF: still rising -- only the pole test at $A5E5 applies.
          const int a = u8w(0x40 + b7 - s.x - s.row + s.halfC);
          if (a < u8w(s.halfC << 1)) level5Fall(game);
          return;
        }
        // $A4B3: the landing.
        L5V(0xb6) = u8w(L5V(0xb6) + 1);
        level5PressPlatform(s, 0x5503);
        level5SetCharlieCells(l5, 0x29);
        // $A4CC: points -- height plus type, then doubled once per step of
        // ($B1 - $B8) counting up to zero (ADDA ,X adds the running total to
        // itself), all in BCD.
        int repeats = u8w(L5V(0xb1) - L5V(0xb8));
        const std::size_t heightIndex = static_cast<std::size_t>(u8w(s.height - 1) % 7);
        const std::size_t t = static_cast<std::size_t>(u8w(s.type - 0x12) % 6) * 2;
        int carry = 0;
        int lo = bcdAdd(kLevel5HeightPoints[heightIndex], kLevel5TypePoints[t], 0, carry);
        int hi = u8w(kLevel5TypePoints[t + 1] + carry);
        for (;;) {
          repeats = u8w(repeats + 1);
          if (repeats == 0) break;
          carry = 0;
          const int nlo = bcdAdd(lo, lo, 0, carry);
          const int nhi = bcdAdd(hi, hi, carry, carry);
          lo = nlo;
          hi = nhi;
        }
        const int total_lo = lo, total_hi = hi;
        const int points = ((total_hi >> 4) & 0xf) * 1000 + (total_hi & 0xf) * 100 +
                           ((total_lo >> 4) & 0xf) * 10 + (total_lo & 0xf);
        game.score += points;
        l5.score = points;
        s.points = points;   // the board writes them into the tiles over the platform
        L5V(0xb8) = u8w(L5V(0xb8) + 1);
        // $A556: the pace is re-read on the landing frame.
        L5V(0xb1) = level5Speed(l5.curInput);
        ++game.stage5SpringAudioSerial;   // $A56D -> $BA97 ($4B)
        // $B9F3: the background flash.
        l5.flashPhase = 1;
        l5.flashTimer = 1;
        l5.flashState = 0;
        return;
      }
    }
    // $A573: the underside; a miss here ends the test.
    if (!within(s.x, 0x40, s.halfB)) return;
    if (u8w(s.row - b7 - 0x11) < 4) {
      level5PressPlatform(s, 0x5100);
      return;
    }
  }
  // $A5C9: the pole and the platform against a rider.
  if (within(s.x, 0x40, s.halfA)) {
    if (u8w(s.row - b7 + 0x12) < 0x24) {
      const int a = u8w(0x40 + b7 - s.x - s.row + s.halfC);
      if (a < u8w(s.halfC << 1)) level5Fall(game);
    }
  }
}

// ---- $A664: the goal stand ($20BD state) --------------------------------

// Returns true when the routine leaves with PULS X,PC, which ends the frame.
void level5Flasher(Level5& l5);

bool level5GoalStand(Game& game) {
  Level5& l5 = game.level5;
  switch (L5V(0xbd)) {
    case 0: return false;
    case 1: {   // $A707: the stand enters at the top of page $12
      const int page = (l5.scroll >> 8) & 0xff;
      const int low = l5.scroll & 0xff;
      if (page != 0x12) return false;
      if (u8w(low + 0x20) >= 3) return false;
      L5V(0xbe) = 8;
      L5V(0xbf) = low;
      const int d = (((low & 0xf8) << 2) + 0x3415) & 0xffff;
      L5V(0xc0) = d >> 8;
      L5V(0xc1) = d & 0xff;
      L5V(0xbd) = 2;
      return false;
    }
    case 2: {   // $A72C: drawn column by column
      const int low = l5.scroll & 0xff;
      if (u8w(L5V(0xbf) - low) < 8) return false;
      L5V(0xbf) = u8w(L5V(0xbf) - 8);
      int d = ((L5V(0xc0) << 8) | L5V(0xc1));
      d = (d - 0x3420) & 0xffff;
      d = (d & 0x03ff) | 0x3400;
      L5V(0xc0) = d >> 8;
      L5V(0xc1) = d & 0xff;
      L5V(0xbe) = u8w(L5V(0xbe) - 1);
      if (L5V(0xbe) != 0) return false;
      d = (d + 0xcd00) & 0xffff;
      d = (d & 0x03ff) | 0x3400;
      L5V(0xc0) = d >> 8;
      L5V(0xc1) = d & 0xff;
      L5V(0xc2) = u8w((L5V(0xbf) & 7) + 0xd8 - L5V(0xbf) + low);
      L5V(0xbe) = 8;
      L5V(0xbf) = u8w(L5V(0xbf) + 0x40);
      L5V(0xbd) = 3;
      return false;
    }
    case 3: {   // $A795: landing on it, or falling past it
      const int b7 = L5V(0xb7);
      // $A79F CMPA #$A1 / LBCC: he lands once his feet are back down to
      // row $A1 (the stand's top) inside its columns.
      if (u8w(L5V(0xc2) - 0x24) < 0x38 && b7 >= 0xa1) {
        // $A81C: Charlie is on the stand.
        l5.chRow = 0x8d;
        level5SetCharlieCells(l5, 0xad);
        l5.ponyCol = 0;   // $A83D clears the six pony columns; the code stays
        l5.popupRow = {0x52, 0x52, 0x52, 0x5a, 0x5a, 0x5a};
        l5.popupCol = {0x30, 0x40, 0x50, 0x90, 0xa0, 0xb0};
        l5.popupCode = {0x10, 0x11, 0x12, 0x13, 0x14, 0x15};
        l5.flashState = 0x11;
        l5.flashTimer = 0x02;
        // $A8A3: with no double bounce the bird brings an extra Charlie.
        if (l5.perfect == 0) {
          l5.birdActive = true;
          l5.birdTarget = (std::min(game.lives, 4) << 4) + 8;
          l5.birdCol = 0;
        }
        L5V(0xbc) = 0;
        L5V(0xbd) = 4;
        game.scene = Scene::Goal;
        game.goalFrame = 0;
        return true;
      }
      if (b7 >= 0xc4 || (b7 >= 0xa1 && u8w(L5V(0xc2) - 0x16) < 0x54)) {
        // $A7F5: he missed it.
        l5.ponyCol = 0;
        level5Fall(game);
        return true;
      }
      if (L5V(0xbe) == 0) return false;
      const int low = l5.scroll & 0xff;
      if (u8w(low - L5V(0xbf)) > 8) return false;
      L5V(0xbf) = u8w(L5V(0xbf) - 8);
      int d = ((L5V(0xc0) << 8) | L5V(0xc1));
      d = (d - 0x3420) & 0xffff;
      d = (d & 0x03ff) | 0x3400;
      L5V(0xc0) = d >> 8;
      L5V(0xc1) = d & 0xff;
      L5V(0xbe) = u8w(L5V(0xbe) - 1);
      return false;
    }
    default: {   // $A67A: the celebration
      l5.flashPhase = 2;
      level5Flasher(l5);   // $A67F JSR $BA1F
      L5V(0xbc) = u8w(L5V(0xbc) + 1);
      if (L5V(0xbc) == 0) {
        l5.finished = true;
        return true;
      }
      const std::size_t k = static_cast<std::size_t>(L5V(0xbc) & 0x0e);
      l5.popupAnimA = kLevel5PopupRowA[k];
      l5.popupAnimB = kLevel5PopupRowA[k + 1];
      if (l5.perfect == 0 && l5.birdActive) {
        // $A6A6: the bird flies in from the right; at its mark the life is
        // awarded.
        const int bc = L5V(0xbc);
        const int a = u8w(-bc);
        l5.birdCol = a;
        const int lead = u8w(a - 0x10 + 8);
        if (lead == l5.birdTarget) {
          ++game.lives;
          ++game.extraCharlieAudioSerial;
          l5.birdActive = false;
        }
      }
      if ((L5V(0xbc) & 0x1f) == 0) {
        level5SetCharlieCells(l5, 0xad + ((L5V(0xbc) & 0x20) >> 3));
      }
      return true;
    }
  }
}

// ---- $A0EA: the playing frame -------------------------------------------

// $BA1F: the background flasher after a landing.
void level5Flasher(Level5& l5) {
  if (l5.flashPhase == 0) return;
  l5.flashTimer = u8w(l5.flashTimer - 1);
  if (l5.flashTimer != 0) return;
  l5.flashTimer = l5.flashPhase;
  l5.flashState = (l5.flashState + 1) & 3;
  if (l5.flashState != 0) return;
  l5.flashPhase = u8w(l5.flashPhase + 1);
  if (l5.flashPhase == 6) l5.flashPhase = 0;
}

void level5Playing(Game& game, int input) {
  Level5& l5 = game.level5;
  l5.curInput = input;
  if (level5GoalStand(game)) return;
  if (l5.phase != 1) return;
  // $BB73: the bonus, one tick every three frames on this stage.
  if (l5.bonusHold != 0) {
    --l5.bonusHold;
  } else {
    l5.bonusHold = kLevel5BonusHold;
    if (game.bonus > 0) --game.bonus;
  }
  level5Flasher(l5);

  L5V(0xb6) = 0;
  for (std::size_t i = 0; i < 8; ++i) {
    const int state = l5.springs[i].state;
    if (state == 0) continue;
    if (state == 1) level5Spring1(game, i);
    else if (state == 2) level5Spring2(game, i);
    else level5Spring3(game, i);
    if (l5.phase != 1) return;
  }
  if (L5V(0xb6) != 0) {
    // $A115: the rebound reverses the fall.
    int vel = ((L5V(0xb4) << 8) | L5V(0xb5));
    vel = (0xffe8 - vel) & 0xffff;
    L5V(0xb4) = vel >> 8;
    L5V(0xb5) = vel & 0xff;
  }
  if (L5V(0xb0) != 0) {
    // Airborne.
    if (L5V(0xbc) != 0) {
      const int bc = u8w(L5V(0xbc) + 1);
      L5V(0xbc) = bc;
      if ((bc & 7) == 0 && bc < 0x38) {
        level5SetCharlieCells(
            l5, kLevel5TumbleCells[static_cast<std::size_t>((bc >> 1) % 28)]);
      }
    }
    int vel = ((L5V(0xb4) << 8) | L5V(0xb5));
    vel = (vel + 0x18) & 0xffff;
    L5V(0xb4) = vel >> 8;
    L5V(0xb5) = vel & 0xff;
    // $A16F: ADDD $26B4 -- the row carries the fraction in $26B5.
    const int d = (vel + ((l5.chRow << 8) | l5.chFrac)) & 0xffff;
    l5.chRow = d >> 8;
    l5.chFrac = d & 0xff;
    L5V(0xb7) = u8w(l5.chRow + 0x14);
    if (L5V(0xbc) == 0 && L5V(0xb7) == 0xb0) {
      // $A18C: back on the pony.
      level5SetCharlieCells(l5, L5V(0xb3) << 2);
      L5V(0xb8) = u8w(L5V(0xb8) - 1);
      if (L5V(0xb8) != 0) {
        if (!game.level2Perfect) l5.perfect = u8w(l5.perfect + 1);   // --replay-perfect
        L5V(0xb8) = 0;
      }
      L5V(0xb0) = 0;
    }
  } else {
    // $A1AC: riding -- the pace and the jump.
    L5V(0xb1) = level5Speed(input);
    const bool edge = (input & 0x10) != 0 && (l5.prevInput & 0x10) == 0;
    if (edge) {
      L5V(0xb0) = u8w(L5V(0xb0) + 1);
      L5V(0xb4) = 0xfd;
      L5V(0xb5) = 0x00;
      int code = 0x29;
      if (L5V(0xbd) >= 2) {
        L5V(0xbc) = u8w(L5V(0xbc) + 1);
        code = kLevel5TumbleCells[0];
      }
      level5SetCharlieCells(l5, code);
      ++game.jumpAudioSerial;
    } else {
      level5SetCharlieCells(l5, L5V(0xb3) << 2);
    }
  }
  // $A205: the pony's gait.
  if (L5V(0xbd) >= 3 && L5V(0xc2) <= 0x40) {
    l5.ponyCol = 0;
  } else {
    const int low = l5.scroll & 0xff;
    if (u8w(L5V(0xb2) - low) >= 0x0a) {
      L5V(0xb2) = u8w(L5V(0xb2) - 0x0a);
      L5V(0xb3) = (L5V(0xb3) + 1) & 3;
      level5SetPonyCells(l5);
    }
  }
  // $A258: the scroll.
  {
    int low = l5.scroll & 0xff;
    int page = (l5.scroll >> 8) & 0xff;
    const int sum = L5V(0xb1) + low;
    low = sum & 0xff;
    if (sum > 0xff) {
      // Carry: a normal step.  $A26E re-enters the plaque cells as the low
      // byte passes $A8-$CA.
      int k = -1;
      const int b1 = (low + 0x58) & 0xff;
      if (b1 < 0x10) {
        k = 0;
      } else {
        const int b2 = b1 - 0x10;
        if (b2 < 0x10) {
          k = 1;
        } else if (b2 - 0x10 < 3) {
          k = 2;
        }
      }
      if (k >= 0) {
        const int idx = (3 * page + 5 - k) & 0xff;
        if (idx < static_cast<int>(kLevel5PlaqueCells.size())) {
          l5.plCode[static_cast<std::size_t>(k)] =
              kLevel5PlaqueCells[static_cast<std::size_t>(idx)];
        }
      }
    } else {
      page = u8w(page + 1);
      if (page == 0x11) {
        // $A2CF: the goal page -- pending springboards are cancelled.
        for (auto& s : l5.springs) {
          if (s.state == 1) s.state = 0;
        }
        L5V(0xbd) = u8w(L5V(0xbd) + 1);
      }
    }
    l5.scroll = (page << 8) | low;
  }
  // $A29F: everything on the field scrolls with the pace.
  const int pace = L5V(0xb1) >= 0x80 ? L5V(0xb1) - 0x100 : L5V(0xb1);
  for (auto& s : l5.springs) {
    s.gap = u8w(s.gap + L5V(0xb1));
    s.x = u8w(s.x + L5V(0xb1));
    s.hdX += pace;
  }
  for (auto& c : l5.plCol) c = u8w(c + L5V(0xb1));
  L5V(0xc2) = u8w(L5V(0xc2) + L5V(0xb1));
}

// ---- $9F19: Charlie is down ------------------------------------------------

void level5Down(Game& game) {
  Level5& l5 = game.level5;
  if (L5V(0xb9) == 1) {
    // $9F53: falling off the pony.
    int vel = ((L5V(0xb4) << 8) | L5V(0xb5));
    vel = (vel + 0x18) & 0xffff;
    L5V(0xb4) = vel >> 8;
    L5V(0xb5) = vel & 0xff;
    const int d = (vel + ((l5.chRow << 8) | l5.chFrac)) & 0xffff;
    if ((d >> 8) >= 0xb0) {
      level5SetCharlieCells(l5, 0x31);
      l5.chRow = 0xb0;
      L5V(0xba) = 0x38;
      L5V(0xb9) = u8w(L5V(0xb9) + 1);
      ++game.crashAudioSerial;
      return;
    }
    l5.chRow = d >> 8;
    l5.chFrac = d & 0xff;
    // The pony runs on to the right.
    if (L5V(0xbd) >= 3) {
      if (l5.ponyCol == 0) return;
      if (u8w(l5.ponyCol + 0x18) >= L5V(0xc2)) return;
    }
    l5.ponyCol = u8w(l5.ponyCol - L5V(0xb1));
    // $9F9E compares the third cell's column (+ $20) with the gait counter.
    if (u8w(l5.ponyCol + 0x20 - L5V(0xb2)) >= 0x0a) {
      L5V(0xb2) = u8w(L5V(0xb2) + 0x0a);
      L5V(0xb3) = (L5V(0xb3) + 1) & 3;
      level5SetPonyCells(l5);
    }
    return;
  }
  if (L5V(0xb9) == 2) {
    // $9F20: lying on the grass until the wipe.
    L5V(0xba) = u8w(L5V(0xba) - 1);
    if (L5V(0xba) != 0) return;
    level5RestartBookkeeping(game);
  }
}

void syncLevel5World(Game& game) {
  const Level5& l5 = game.level5;
  const int page = (l5.scroll >> 8) & 0xff;
  const int low = l5.scroll & 0xff;
  const int progress = page * 256 + ((256 - low) & 0xff);
  const float scale = kWorldWidth / 224.0F;
  game.cameraX = static_cast<float>(progress) * scale;
  game.player.position = {static_cast<float>(progress + l5.chCol) * scale,
                          level3RowToY(static_cast<float>(l5.chRow))};
  game.player.grounded = L5V(0xb0) == 0;
  game.player.alive = l5.phase == 1;
}

void updateLevel5(Game& game, const Uint8* keyboard, bool jumpPressed,
                  float controllerAxis) {
  Level5& l5 = game.level5;
  game.player.previous = game.player.position;
  game.previousCameraX = game.cameraX;
  const bool moveLeft = keyboard[SDL_SCANCODE_LEFT] || controllerAxis < -0.35F;
  const bool moveRight = keyboard[SDL_SCANCODE_RIGHT] || controllerAxis > 0.35F;
  const bool button = jumpPressed || keyboard[SDL_SCANCODE_SPACE] ||
                      keyboard[SDL_SCANCODE_Z];
  const int input = (moveLeft ? 1 : 0) | (moveRight ? 2 : 0) | (button ? 0x10 : 0);

  if (l5.phase == 1) {
    level5Playing(game, input);
  } else if (l5.phase == 2) {
    level5Down(game);
  }
  l5.prevInput = input;

  if (l5.finished) {
    l5.finished = false;
    game.scene = Scene::Tally;
    game.tallyFrame = 0;
  } else if (l5.died) {
    l5.died = false;
    game.scene = Scene::Crashed;
    game.crashFrame = 0;
    game.crashDurationFrames = kLevel5RestartFrames;
    if (game.lives > 0) --game.lives;
    // The wipe clears the sprite cells and the plaques and leaves $2800 at
    // zero until $9FF9 runs again; $2208 keeps the $9F24 result.
    l5.phase = 0;
    l5.chRow = l5.chCol = l5.chCode = 0;
    l5.ponyRow = l5.ponyCol = l5.ponyCode = 0;
    l5.plRow = {0, 0, 0};
    l5.plCol = {0, 0, 0};
    l5.plCode = {0, 0, 0};
    l5.flashState = l5.flashTimer = l5.flashPhase = 0;   // $287D-$287F
  }
  // HD animation bookkeeping for the leap frames.
  if (l5.chCode == 0x29) {
    ++l5.hdAir;
  } else {
    l5.hdAir = 0;
  }
  l5.hdRising = l5.chRow < l5.hdPrevRow;
  l5.hdPrevRow = l5.chRow;
  syncLevel5World(game);
}
#undef L5V
