// ---- Event 6 (the trapeze): the $A8E9 handler transcribed against a RAM mirror
//
// Direct page is $20, so "$D2" below is $20D2.  Records live at $2900 with a
// stride of $30; the three rope sprite slots are $2400, $2500 and $2600 (16
// cells each, the bar in the last), the goal marker $2720/$2730, Charlie
// $2740-$2770, the points popup $2700/$2710, the plaque $27C0-$27E0.

namespace l6 {

constexpr int kBarTable = kLevel6BarTableAddress;

inline int rd(const Level6& l, int a) { return l.ram[static_cast<std::size_t>(a & 0x3fff)]; }
inline void wr(Level6& l, int a, int v) { l.ram[static_cast<std::size_t>(a & 0x3fff)] = static_cast<std::uint8_t>(v & 0xff); }
inline int rd16(const Level6& l, int a) { return (rd(l, a) << 8) | rd(l, a + 1); }
inline void wr16(Level6& l, int a, int v) { wr(l, a, v >> 8); wr(l, a + 1, v); }
inline int s8(int v) { return (v & 0x80) ? v - 0x100 : v; }
inline int s16(int v) { return (v & 0x8000) ? v - 0x10000 : v; }
inline int dp(int x) { return 0x2000 + x; }
inline int barCell(int entry) { return kBarTable + entry; }

// The ROM tables reached by address.
inline int romByte(int a) {
  if (a >= 0xf850 && a < 0xf8b8) return kLevel6Amplitude[static_cast<std::size_t>(a - 0xf850)];
  if (a >= 0xf8b8 && a < 0xf920) return kLevel6Length[static_cast<std::size_t>(a - 0xf8b8)];
  if (a >= 0xf112 && a < 0xf16d) return kLevel6Sine[static_cast<std::size_t>(a - 0xf112)];
  return 0;
}

// $BA46: a sound command.  The stage is always in play here, so every code
// goes to the queue.
inline void sound(Game& game, int code) {
  switch (code) {
    case 0x44: ++game.jumpAudioSerial; break;
    case 0x45: ++game.stage6BounceAudioSerial; break;
    case 0x4f: ++game.stage6FloorAudioSerial; break;   // the floor
    case 0x41: ++game.extraCharlieAudioSerial; break;
    default: break;
  }
}

// $6114 with D = $0200: add the BCD points at $20A9-$20AB to the score.
inline void addPoints(Game& game, Level6& l) {
  const int hi = rd(l, 0x20aa);
  const int lo = rd(l, 0x20ab);
  const int points = ((hi >> 4) & 0xf) * 1000 + (hi & 0xf) * 100 +
                     ((lo >> 4) & 0xf) * 10 + (lo & 0xf);
  game.score += points;
  l.score = points;
}

// $B4C0: the record whose index byte is A.
inline int findRecord(const Level6& l, int a) {
  for (int u = 0x2900; u < 0x2a00; u += 0x30) {
    if (rd(l, u) == (a & 0xff)) return u;
  }
  return 0x2900;
}

// $B365: a fresh trapeze record U for index A.
inline void spawnRecord(Level6& l, int u, int a) {
  wr(l, u, a);
  int index = a & 0xff;
  if (((index - rd(l, dp(0xd9))) & 0x80) != 0) index = 0;   // CMPA $D9 / BMI
  else if (index > rd(l, dp(0xdc))) index = 0;             // CMPA $DC / BLS
  if (index >= 0x68) index = (index & 7) + 0x60;
  const int amplitude = kLevel6Amplitude[static_cast<std::size_t>(index)];
  const int length = kLevel6Length[static_cast<std::size_t>(index)];
  wr(l, u + 8, amplitude);
  wr(l, u + 9, length);
  const int b = ((length - 0x20) & 0xff) >> 1;
  wr(l, u + 0xa, kLevel6AngularStep[static_cast<std::size_t>(b % 16)]);
  wr(l, u + 0xb, kLevel6AngularStep[static_cast<std::size_t>((b + 1) % 16)]);
  wr(l, u + 1, 0x01);
  wr(l, u + 2, 0x5a);
  int quarter = (0x3c - length) & 0xff;
  quarter = s8(quarter) >> 2;   // ASRA twice
  wr(l, u + 4, quarter);
}

void release(Game& game, Level6& l);

// $B3B5: one record's swing and its sixteen rope cells.
inline void swingRecord(Game& game, Level6& l, int u) {
  const int da = rd(l, dp(0xda));
  int d = (rd16(l, u + 2) + rd16(l, u + 0xa)) & 0xffff;
  if ((d >> 8) >= 0xb4) {
    d = ((d - 0xb400) & 0xffff);
    wr(l, u + 1, rd(l, u + 1) + 1);
    const int saved = d;
    if (rd(l, u) == da) {
      const int swings = rd(l, u + 1);
      if ((swings & 1) == 0) {
        wr(l, u + 4, rd(l, u + 4) + 1);
        if (rd(l, u + 4) >= 0x10) {
          wr(l, u + 4, rd(l, u + 4) - 1);
          if (rd(l, dp(0xb0)) == 0) release(game, l);
        }
      }
    }
    d = saved;
  }
  wr16(l, u + 2, d);
  int a = d >> 8;
  if (a > 0x5a) a = (0xb4 - a) & 0xff;
  a = kLevel6Sine[static_cast<std::size_t>(a % 0x5b)];
  const int extent = (a * rd(l, u + 8)) >> 8;   // MUL, high byte
  const int c = kLevel6Sine[static_cast<std::size_t>(((0x5a - extent) & 0xff) % 0x5b)];
  const int dd = kLevel6Sine[static_cast<std::size_t>(extent % 0x5b)];
  wr(l, u + 0xc, c);
  wr(l, u + 0xd, dd);
  // $B3FB: the rows, from the bar up.
  wr(l, dp(0xf9), 0);
  const int fa = (c << 2) & 0xffff;               // 4 * $C,U
  int drop = (rd(l, u + 9) * c) & 0xffff;         // length * $C,U
  if (rd(l, u) == da) {
    wr16(l, dp(0xc1), rd16(l, dp(0xbf)));
    wr16(l, dp(0xbf), rd16(l, dp(0xbd)));
    wr16(l, dp(0xbd), rd16(l, dp(0xbb)));
    wr16(l, dp(0xbb), rd16(l, dp(0xb9)));
    wr16(l, dp(0xb9), drop);
    int diff = (drop - rd16(l, dp(0xc1))) & 0xffff;
    diff = (s16(diff) >> 2) & 0xffff;   // ASRA/RORB twice
    wr16(l, dp(0xc3), diff);
  }
  {
    // A += $50 once; then the running 16-bit value drops by 4 * $C,U per
    // cell while its high byte stays at or above $50, and the cells left
    // over repeat the last row.
    int x = u + 0x20;
    int count = 0;
    int running = ((((drop >> 8) + 0x50) & 0xff) << 8) | (drop & 0xff);
    for (;;) {
      --x;
      wr(l, x, running >> 8);
      ++count;
      running = (running - fa) & 0xffff;
      if ((running >> 8) < 0x50) break;
      if (x == u + 0x10) break;   // the ROM has no guard; the tables never need one
    }
    wr(l, dp(0xf9), count);
    if (x != u + 0x10) {
      running = (running + fa) & 0xffff;
      const int a = running >> 8;
      while (x > u + 0x10) {
        --x;
        wr(l, x, a);
      }
    }
  }
  // $B451: the columns.
  const int remaining = (0x10 - rd(l, dp(0xf9))) & 0xff;
  wr(l, dp(0xfa), remaining);
  int fb = ((0 - dd) << 2) & 0xffff;              // -4 * $D,U
  int prod = (rd(l, u + 9) * dd) & 0xffff;        // length * $D,U
  const int odd = rd(l, u + 1) & 1;
  int y = prod;
  if (odd) {
    y = (0 - prod) & 0xffff;
    fb = (0 - fb) & 0xffff;
  }
  if (rd(l, u) == da) {
    wr16(l, dp(0xcd), rd16(l, dp(0xcb)));
    wr16(l, dp(0xcb), rd16(l, dp(0xc9)));
    wr16(l, dp(0xc9), rd16(l, dp(0xc7)));
    wr16(l, dp(0xc7), rd16(l, dp(0xc5)));
    wr16(l, dp(0xc5), y);
    int diff = (rd16(l, dp(0xcd)) - y) & 0xffff;
    diff = (s16(diff) >> 2) & 0xffff;
    wr16(l, dp(0xcf), diff);
  }
  int x = u + 0x30;
  int dcur = y;
  for (int n = rd(l, dp(0xf9)); n > 0; --n) {
    --x;
    wr(l, x, dcur >> 8);
    dcur = (dcur + fb) & 0xffff;
  }
  // $B4B7: one SUBD, then the same column for every cell left.
  if (remaining != 0) {
    dcur = (dcur - fb) & 0xffff;
    for (int n = remaining; n > 0; --n) {
      --x;
      wr(l, x, dcur >> 8);
    }
  }
}

// $B39C: swing every record in range.
inline void swingAll(Game& game, Level6& l) {
  for (int u = 0x2900; u < 0x29f0; u += 0x30) {
    const int a = rd(l, u);
    if (((a - rd(l, dp(0xd9))) & 0x80) != 0) continue;   // CMPA $D9 / BMI
    if (a > rd(l, dp(0xdc))) continue;
    swingRecord(game, l, u);
  }
}

// $B4CB: the rope's colour by the swings on it.
inline void ropeColours(Level6& l, int u, int x) {
  const int a = rd(l, u + 4);
  for (int f9 = 0x0f; f9 > 0; --f9) {
    int b = 1;
    if (a >= f9) b = 6;
    wr(l, x, b);
    x -= 0x10;
  }
}

// $B4E2: Charlie lets go.
inline void release(Game& game, Level6& l) {
  wr(l, dp(0xb0), rd(l, dp(0xb0)) + 1);
  wr(l, dp(0xde), rd(l, dp(0xde)) + 1);
  int a = kLevel6HangCells[0];
  wr(l, 0x2776, a); wr(l, 0x2756, a);
  a += 0x10;
  wr(l, 0x2766, a); wr(l, 0x2746, a);
  wr(l, 0x277e, 0xf8); wr(l, 0x276e, 0xf9); wr(l, 0x275e, 0xfa); wr(l, 0x274e, 0xfb);
  wr16(l, dp(0xb7), rd16(l, dp(0xc3)));
  wr16(l, dp(0xb5), rd16(l, dp(0xcf)));
  wr(l, 0x2775, 0);
  wr(l, 0x2205, 0);
  wr(l, dp(0xd3), 0);
  wr(l, dp(0xe1), 0);
  wr(l, dp(0xe6), 0);
  wr(l, dp(0xeb), 0);
  wr(l, dp(0xf0), 0);
  wr16(l, 0x271e, 0xfe00);
  wr16(l, 0x270e, 0xfe00);
  sound(game, 0x44);
}

// $B270: a plain rope slot's codes (D = slot * $100 + $F0).
inline void plainRopeCodes(Level6& l, int slot) {
  int y = 0x2400 + slot * 0x100 + 0xf0;
  wr(l, y + 0xe, 0x56);
  wr(l, y + 0xf, 0);
  for (int a = 7; a > 0; --a) {
    y -= 0x10;
    wr(l, y + 0xe, 0x55);
  }
}

// $B291: a decorated trapeze's bar cells.
inline void decoratedRopeCodes(Level6& l, int u, int slot) {
  const int y = 0x2400 + slot * 0x100 + 0xf0;
  int a = rd(l, u + 2);
  if (a > 0x5a) a = (0xb4 - a) & 0xff;
  a = kLevel6Sine[static_cast<std::size_t>(a % 0x5b)];
  a = (a * rd(l, u + 8)) >> 8;
  int b = 0;
  if (a < 0x52) { ++b; if (a < 0x43) { ++b; if (a < 0x3c) { ++b; if (a < 0x2d) { ++b;
    if (a < 0x25) { ++b; if (a < 0x1e) { ++b; if (a < 0x07) ++b; } } } } } }
  if ((rd(l, u + 1) & 1) == 0) b = (0x0f - b) & 0xff;
  const int col = rd(l, y + 6);
  if (col <= 2 || col >= 0xfe) {
    // $B2D9: the bar is at the edge -- blank three cells.
    wr(l, y - 0x12, rd(l, y + 0xe));
    int yy = y;
    int bb = 0xfe;
    for (int pass = 0; pass < 2; ++pass) {
      for (int n = 3; n > 0; --n) {
        if (bb == 0) wr(l, yy + 0xf, bb); else wr(l, yy + 0xe, bb);
        yy -= 0x10;
      }
      yy += 0x40;
      if (bb == 0) break;
      bb = 0;
    }
  } else {
    wr(l, y - 0x22, 0x56);
    wr(l, y - 0x21, 0);
    l.prizeFlag = b;
    const int entry = b * 6;
    wr(l, y + 0xe, romByte(0));   // placeholder, replaced below
    const auto cell = [&](int k) {
      // (attribute, code) pairs from $FB02.
      static constexpr std::array<int, 96> table{
          0x20, 0x56, 0x20, 0x57, 0x20, 0x5e, 0x20, 0x5f, 0x20, 0x60, 0x20, 0x5e,
          0x20, 0x5f, 0x20, 0x62, 0x20, 0x61, 0x20, 0x63, 0x20, 0x62, 0x20, 0x64,
          0x20, 0x63, 0x20, 0x62, 0x20, 0x65, 0x20, 0x63, 0x20, 0x66, 0x20, 0x65,
          0x20, 0x63, 0x20, 0x66, 0x20, 0x67, 0x20, 0x7a, 0x20, 0x73, 0x20, 0x67,
          0x20, 0x7a, 0x20, 0x73, 0x20, 0x79, 0x20, 0x7a, 0x20, 0x73, 0x20, 0x79,
          0x20, 0x6f, 0x20, 0x73, 0x20, 0x72, 0x20, 0x6f, 0x20, 0x6e, 0x20, 0x72,
          0x20, 0x6f, 0x20, 0x6e, 0x20, 0x71, 0x20, 0x6b, 0x20, 0x6e, 0x20, 0x6d,
          0x20, 0x6b, 0x20, 0x6c, 0x20, 0x68, 0x20, 0x6a, 0x20, 0x69, 0x20, 0x68};
      return table[static_cast<std::size_t>((entry + k) % 96)];
    };
    wr(l, y + 0xe, cell(1)); wr(l, y + 0xf, cell(0));
    wr(l, y - 0x2, cell(3)); wr(l, y - 0x1, cell(2));
    wr(l, y - 0x12, cell(5)); wr(l, y - 0x11, cell(4));
  }
  // $B328: shuffle the bottom cells up the rope.
  wr(l, y + 4, rd(l, y - 0xc)); wr(l, y + 6, rd(l, y - 0xa));
  wr(l, y - 0xc, rd(l, y - 0x2c)); wr(l, y - 0xa, rd(l, y - 0x2a));
  wr(l, y - 0x1c, rd(l, y - 0x5c)); wr(l, y - 0x1a, rd(l, y - 0x5a));
  wr(l, y - 0x2c, rd(l, y - 0x5c)); wr(l, y - 0x2a, rd(l, y - 0x5a));
  wr(l, y - 0x5c, 0); wr(l, y - 0x4c, 0); wr(l, y - 0x3c, 0);
  wr(l, y - 0x5a, 0); wr(l, y - 0x4a, 0); wr(l, y - 0x3a, 0);
}

// $B52C: the goal platform's colour rows.
inline void goalColours(Level6& l) {
  const int d = (rd16(l, dp(0xdf)) - 0x19) & 0xffff;
  if (d >= 0xef) return;
  int x = (rd16(l, dp(0xe2)) - 0x3fa) & 0xffff;
  for (int a = 4; a > 0; --a) {
    x += 0x20;
    if (x >= 0x3400) x -= 0x400;
    wr(l, x, 0x10); wr(l, x + 1, 0x10); wr(l, x + 2, 0x10);
  }
}

// The tilemap of a trampoline at vram U with six rows from $F0B8 ($A9D4).
inline void drawTrampolineTiles(Level6& l, int record) {
  int d = rd16(l, record);
  if ((d >> 8) != 0) return;
  int u = rd16(l, record + 3);
  for (int row = 0; row < 6; ++row) {
    wr(l, u, kLevel6TrampolineTiles[static_cast<std::size_t>(row * 3)]);
    wr(l, u + 1, kLevel6TrampolineTiles[static_cast<std::size_t>(row * 3 + 1)]);
    wr(l, u + 2, kLevel6TrampolineTiles[static_cast<std::size_t>(row * 3 + 2)]);
    d = (d - 8) & 0xffff;
    u += 0x20;
    if (u >= 0x3800) u -= 0x400;
  }
}

// $B0BF: the goal platform's tiles.
inline void drawGoalTiles(Level6& l) {
  int f9 = 0;
  int u = rd16(l, dp(0xe2));
  int d = rd16(l, dp(0xdf));
  if (d & 0x8000) return;
  if (d >= 0x130) return;
  if (d >= 0x38) {
    d = (d - 0xf8) & 0xffff;
    if (d & 0x8000) return;
    f9 = 0xff;
  }
  int b = (d & 0x38) >> 1;
  b = ((s8(b) >> 1) + b) & 0xff;   // ASRB then ADDB $FA
  int x = b;
  int fa = 0;
  for (;;) {
    const int selector = kLevel6GoalRows[static_cast<std::size_t>(x % 42)] ^ f9;
    ++x;
    if ((selector & 0xff) == 0) {
      // a tile string from $F0CA[fa]
      const int offset = kLevel6GoalStringOffsets[static_cast<std::size_t>(fa % 6)];
      int uu = u;
      for (int k = offset; k < 30; ++k) {
        const int t = kLevel6GoalStrings[static_cast<std::size_t>(k)];
        if (t == 0) break;
        wr(l, uu, t);
        ++uu;
      }
    } else if (((selector + 1) & 0xff) == 0) {
      wr(l, u, 0x10);
      wr16(l, u + 1, 0x1010); wr16(l, u + 3, 0x1010);
      wr16(l, u + 5, 0x8949); wr16(l, u + 7, 0x5959);
      wr(l, u + 0xb, 0x60);
      if ((u & 0x20) == 0) wr16(l, u + 9, 0x5b7b); else wr16(l, u + 9, 0x4b6b);
    }
    ++fa;
    if (fa == 6) break;
    u += 0x20;
    if (u >= 0x3800) u -= 0x400;
  }
}

// $B857: Charlie hits the floor.
inline void hitFloor(Game& game, Level6& l, int a);

// $AC27: the common tail of a playing frame.
inline void commonTail(Game& game, Level6& l);

// $B556: the landing on the goal platform.
inline void goalLanding(Game& game, Level6& l) {
  (void)game;
  for (int x = 0x26f6; x >= 0x2406; x -= 0x10) wr(l, x, 0);
  wr(l, 0x2774, 0x68); wr(l, 0x2764, 0x68);
  wr(l, 0x2754, 0x78); wr(l, 0x2744, 0x78);
  wr(l, 0x2776, 0x24); wr(l, 0x2756, 0x24);
  wr(l, 0x2766, 0x34); wr(l, 0x2746, 0x34);
  wr16(l, 0x277e, 0x7620); wr16(l, 0x276e, 0x7720);
  wr16(l, 0x275e, 0x7e20); wr16(l, 0x274e, 0x7f20);
  wr16(l, 0x2734, 0x3cb0);
  wr(l, 0x2724, 0x4c);
  int b = (0x2c - rd(l, 0x2736)) & 0xff;
  int d = (s8(b) << 3) & 0xffff;
  wr16(l, 0x2732, d);
  wr(l, 0x273e, 0x58); wr(l, 0x272e, 0x59);
  wr(l, 0x2738, 2);
  wr(l, 0x2454, 0xaa); wr(l, 0x2444, 0xaa); wr(l, 0x2434, 0xaa);
  wr(l, 0x2424, 0xb2); wr(l, 0x2414, 0xb2); wr(l, 0x2404, 0xb2);
  wr(l, 0x2456, 0x60); wr(l, 0x2446, 0x70); wr(l, 0x2436, 0x80);
  wr(l, 0x2426, 0xa8); wr(l, 0x2416, 0xb8); wr(l, 0x2406, 0xc8);
  wr16(l, 0x245e, 0x1000); wr16(l, 0x244e, 0x1100); wr16(l, 0x243e, 0x1200);
  wr16(l, 0x242e, 0x1300); wr16(l, 0x241e, 0x1400); wr16(l, 0x240e, 0x1500);
  // $BB0A: the music stops and the clear jingle plays; the HD build has
  // the crowd cheer instead.
  ++game.stage6GoalAudioSerial;
  wr(l, 0x2800, rd(l, 0x2800) + 1);
}

// $B9F3: the background flash.
inline void flash(Level6& l) {
  wr(l, 0x287f, 1);
  wr(l, 0x287e, 1);
  wr(l, 0x287d, 0);
}

// $BA1F: the flasher.
inline void flasher(Level6& l) {
  if (rd(l, 0x287f) == 0) return;
  wr(l, 0x287e, rd(l, 0x287e) - 1);
  if (rd(l, 0x287e) != 0) return;
  wr(l, 0x287e, rd(l, 0x287f));
  wr(l, 0x287d, (rd(l, 0x287d) + 1) & 3);
  if (rd(l, 0x287d) != 0) return;
  int a = rd(l, 0x287f) + 1;
  if (a == 6) a = 0;
  wr(l, 0x287f, a);
}

// $A8FB: the stage set-up (phase 0).
inline void init(Game& game, Level6& l) {
  for (int a = 0x20b0; a < 0x2100; ++a) wr(l, a, 0);
  int a = (((l.dips & 0x60) >> 1) + ((l.visits - 1) << 4)) & 0xff;
  a = (a + rd(l, 0x2208)) & 0xff;
  wr(l, dp(0xd9), a); wr(l, dp(0xdb), a); wr(l, dp(0xda), a);
  wr(l, dp(0xdc), 0xff);
  spawnRecord(l, 0x2900, (a - 2) & 0xff);
  spawnRecord(l, 0x2930, (a - 1) & 0xff);
  spawnRecord(l, 0x2960, a);
  spawnRecord(l, 0x2990, (a + 1) & 0xff);
  spawnRecord(l, 0x29c0, (a + 2) & 0xff);
  swingAll(game, l);
  wr(l, dp(0xd2), (0x34 - rd(l, 0x298f)) & 0xff);
  wr(l, 0x2904, 0x0f);
  wr(l, 0x2934, 0x0f);
  wr(l, dp(0xdf), 0xc0);
  // The three trampolines under the next three trapezes.
  {
    int u = 0x20e4;
    int x = 0x2969;
    int f9 = ((rd(l, dp(0xd2)) + rd(l, x) + 0x2c) & 0xff);   // A = 0
    for (;;) {
      wr16(l, dp(0xf9), f9);
      wr16(l, u, f9 & 0xfff8);
      u += 5;
      if (u > 0x20ee) break;
      x += 0x30;
      int b = ((rd(l, x) << 1) + 0x10) & 0xff;
      f9 = (rd16(l, dp(0xf9)) + b) & 0xffff;
    }
    u -= 5;
    if (rd(l, dp(0xda)) & 1) {
      wr(l, u, 0xc0);
      wr(l, u - 0xa, 0xc0);
    } else {
      wr(l, u - 5, 0xc0);
    }
    for (int rec = 0x20ee; rec >= 0x20e4; rec -= 5) {
      int d = ((((0 - rd(l, 0x2203)) & 0xff) << 8) | rd(l, 0x2204));
      d = (d - rd16(l, rec)) & 0xffff;
      if (d >= 0xfa00) {
        int b = (0 - rd(l, rec + 1)) & 0xff;
        int dd = ((b << 2) + 0x341c) & 0xffff;
        wr16(l, rec + 3, dd);
      } else {
        wr(l, rec, 0xc0);   // $A9C4: off the page
      }
    }
    for (int rec = 0x20e4; rec <= 0x20ee; rec += 5) drawTrampolineTiles(l, rec);
  }
  wr(l, 0x2734, 0x44); wr(l, 0x2724, 0x54);
  wr16(l, 0x273e, 0x7020); wr16(l, 0x272e, 0x7820);
  wr(l, 0x27e4, 0xa8); wr(l, 0x27d4, 0xa8); wr(l, 0x27c4, 0xa8);
  wr(l, 0x27e6, 0x68); wr(l, 0x27d6, 0x78); wr(l, 0x27c6, 0x88);
  {
    const int page = rd(l, 0x2203);
    const int base = (page * 3) % 27;
    wr(l, 0x27ee, kLevel6PlaqueCells[static_cast<std::size_t>(base)]);
    wr(l, 0x27de, kLevel6PlaqueCells[static_cast<std::size_t>((base + 1) % 27)]);
    wr(l, 0x27ce, kLevel6PlaqueCells[static_cast<std::size_t>((base + 2) % 27)]);
  }
  for (int x = 0x26fe; x >= 0x240e; x -= 0x10) wr(l, x, 0x55);
  wr(l, 0x24fe, 0x56); wr(l, 0x25fe, 0x56); wr(l, 0x26fe, 0x56);
  wr(l, 0x2716, 0x34); wr(l, 0x2706, 0x43);
  wr(l, 0x271e, 0xfe); wr(l, 0x270e, 0xfe);
  wr(l, dp(0xdd), 7);
  for (int x = 0x32e0; x <= 0x3340; x += 0x20) {
    wr(l, x, rd(l, x) | 0x10); wr(l, x + 1, rd(l, x + 1) | 0x10);
    wr(l, x + 4, rd(l, x + 4) | 0x10);
  }
  wr(l, 0x2800, rd(l, 0x2800) + 1);
}

// $AD6E: hanging -- the world follows the bar.
inline void hangingTail(Game& game, Level6& l, int u) {
  wr(l, dp(0xd1), 0);
  int a = (0x34 - rd(l, u + 0x2f)) & 0xff;
  if (a & 0x80) wr(l, dp(0xd1), 0xff);
  const int b = a;
  const int raw = a - rd(l, dp(0xd2));
  a = raw & 0xff;
  const bool borrow = raw < 0;
  wr(l, dp(0xf9), a);
  const int sum = a + rd(l, 0x2204);
  wr(l, 0x2204, sum);
  wr(l, dp(0xd2), b);
  if (sum > 0xff) {
    // BCS $AD94: PULS CC; BMI skip; DEC $2203
    if (!(a & 0x80)) wr(l, 0x2203, rd(l, 0x2203) - 1);
  } else {
    // PULS CC; BPL skip; INC $2203  -- the N flag of the subtraction
    if (a & 0x80) wr(l, 0x2203, rd(l, 0x2203) + 1);
  }
  (void)borrow;
  // The pump: the stick with the swing grows the amplitude.
  const int stick = rd(l, dp(0xd7)) & 3;
  if (stick != 0) {
    const int pump = rd(l, dp(0xd8)) + 0x18;
    wr(l, dp(0xd8), pump);
    if (pump > 0xff) {
      int fa = (rd(l, u + 2) < 0x5a) ? 1 : 0;   // CMPB #$5A -> C set when B < $5A
      fa ^= rd(l, u + 1) & 1;
      int aa = ((stick - 1) ^ fa) & 0xff;
      aa = (aa << 1) & 0xff;
      aa = (0 - aa) & 0xff;
      aa = (aa + 1) & 0xff;
      aa = (aa + rd(l, u + 8)) & 0xff;
      if (aa <= 0x5a) wr(l, u + 8, aa);
    }
  }
  int row = rd(l, u + 0x1f);
  wr(l, 0x2774, row); wr(l, 0x2764, row);
  row = (row + 0x10) & 0xff;
  wr(l, 0x2754, row); wr(l, 0x2744, row);
  row = (row + 0x0c) & 0xff;
  wr(l, 0x2714, row); wr(l, 0x2704, row);
  int angle = rd(l, u + 2);
  if (angle > 0x5a) angle = (0xb4 - angle) & 0xff;
  int extent = (kLevel6Sine[static_cast<std::size_t>(angle % 0x5b)] * rd(l, u + 8)) >> 8;
  int bb = 0;
  if (extent < 0x3c) { bb = 2; if (extent < 0x1e) bb = 4; }
  if ((rd(l, u + 1) & 1) == 0) bb = (0x0a - bb) & 0xff;
  int col = kLevel6HangCells[static_cast<std::size_t>(bb % 20)];
  int code = kLevel6HangCells[static_cast<std::size_t>((bb + 1) % 20)];
  wr(l, 0x2776, col); wr(l, 0x2756, col);
  col = (col + 0x10) & 0xff;
  wr(l, 0x2766, col); wr(l, 0x2746, col);
  wr(l, 0x277e, code); wr(l, 0x276e, code + 1); wr(l, 0x275e, code + 2); wr(l, 0x274e, code + 3);
  // $AE2B: the goal platform is placed on page 6.
  if (rd(l, 0x2203) == 6 && (rd(l, dp(0xdf)) & 0x80)) {
    wr(l, dp(0xfb), (rd(l, dp(0xd2)) + rd(l, u + 9)) & 0xff);
    const int last = (rd(l, dp(0xda)) + 2) & 0xff;
    wr(l, dp(0xdc), last);
    int uu = findRecord(l, last);
    int b = rd(l, uu + 9);   // ADDB $3B00: unused RAM, zero
    b = (b << 1) & 0xff;
    uu = findRecord(l, (rd(l, dp(0xda)) + 1) & 0xff);
    b = (b + rd(l, uu + 9)) & 0xff;
    b = (b + rd(l, uu + 9)) & 0xff;
    wr(l, dp(0xfa), 0);
    int d = (b + rd(l, dp(0xfb))) & 0xffff;
    d = (d + 0x68) & 0xffff;
    d &= 0xfff8;
    wr16(l, dp(0xdf), d);
    int bb2 = ((d & 0xff) - rd(l, 0x2204)) & 0xf8;
    bb2 = (0 - bb2) & 0xff;
    int vram = ((bb2 << 2) + 0x3411) & 0xffff;
    wr16(l, dp(0xe2), vram);
  }
  // $AE75: the scroll delta moves the goal and the trampolines.
  const int delta = s8(rd(l, dp(0xf9))) & 0xffff;
  wr16(l, dp(0xdf), (rd16(l, dp(0xdf)) + delta) & 0xffff);
  wr16(l, dp(0xe4), (rd16(l, dp(0xe4)) + delta) & 0xffff);
  wr16(l, dp(0xe9), (rd16(l, dp(0xe9)) + delta) & 0xffff);
  wr16(l, dp(0xee), (rd16(l, dp(0xee)) + delta) & 0xffff);
  if (rd(l, dp(0xdd)) != 0) wr(l, dp(0xdd), rd(l, dp(0xdd)) - 1);
  (void)game;
}

// $AC35: airborne -- the scroll follows Charlie's velocity.
inline void airborneTail(Game& game, Level6& l, int& u) {
  const int b5 = rd16(l, dp(0xb5));
  {
    const int sum = rd16(l, 0x2204) + b5;
    wr16(l, 0x2204, sum);
    const bool carry = sum > 0xffff;
    if (!(rd(l, dp(0xb5)) & 0x80)) {
      if (carry) wr(l, 0x2203, rd(l, 0x2203) - 1);
    } else {
      if (!carry) wr(l, 0x2203, rd(l, 0x2203) + 1);
    }
  }
  int d = (rd16(l, dp(0xd2)) + b5) & 0xffff;
  wr16(l, dp(0xd2), d);
  wr(l, dp(0xd1), (d & 0x8000) ? 0xff : 0);
  int a = (rd(l, u + 9) + 8) & 0xff;
  int b = (rd(l, dp(0xd2)) - 0x34) & 0xff;
  wr(l, dp(0xf9), a); wr(l, dp(0xfa), b);
  const int within = (b + a) & 0xff;
  const int span = (a << 1) & 0xff;
  if (within < span) {
    // still beside the bar
  } else if (!(b & 0x80)) {
    // $ACF9: past the bar to the right? -- the ROM branches on N clear
    wr(l, dp(0xde), 0);
    wr(l, dp(0xda), rd(l, dp(0xda)) - 1);
    findRecord(l, (rd(l, dp(0xda)) + 3) & 0xff);
    const int uu = findRecord(l, (rd(l, dp(0xda)) + 3) & 0xff);
    spawnRecord(l, uu, (rd(l, dp(0xda)) - 2) & 0xff);
    u = findRecord(l, rd(l, dp(0xda)));
    int bb = (rd(l, dp(0xd2)) - rd(l, dp(0xd4)) - 0x10 - rd(l, u + 9)) & 0xff;
    wr(l, dp(0xd1), (bb & 0x80) ? 0xff : 0);
    wr(l, dp(0xd2), bb);
  } else {
    // $AC75: the next bar.
    wr(l, dp(0xde), 0);
    wr(l, dp(0xda), rd(l, dp(0xda)) + 1);
    const int uu = findRecord(l, (rd(l, dp(0xda)) - 3) & 0xff);
    spawnRecord(l, uu, (rd(l, dp(0xda)) + 2) & 0xff);
    const int f9 = (rd(l, uu + 9) << 1) & 0xff;   // twice the new bar's length
    u = findRecord(l, rd(l, dp(0xda)));
    wr(l, dp(0xf9), f9);
    int bb = (rd(l, dp(0xd2)) + rd(l, dp(0xd4)) + 0x10 + rd(l, u + 9)) & 0xff;
    wr(l, dp(0xd2), bb);
    wr(l, dp(0xd1), 0);
    if ((rd(l, dp(0xda)) & 1) == 0) {
      // $ACA2: an even index brings a trampoline under the bar after next.
      const int saved = u;
      int fa = (bb + rd(l, u + 9) + 0x4c) & 0xff;
      wr(l, dp(0xfa), fa);
      const int un = findRecord(l, (rd(l, dp(0xda)) + 1) & 0xff);
      int b2 = ((rd(l, un + 9) << 1) + f9 + 4) & 0xff;
      int d2 = (b2 + fa) & 0xffff;   // CLRA; STA $F9; ADDD $F9 -> D = B + (0:$FA)
      d2 &= 0xfff8;
      wr16(l, dp(0xf9), d2);
      int scroll = ((((0 - rd(l, 0x2203)) & 0xff) << 8) | rd(l, 0x2204));
      scroll = (scroll - d2) & 0xffff;
      if (scroll >= 0xfa00) {
        // the trampoline record with the smallest position takes it
        int best = 0x20e4;
        for (int rec = 0x20e9; rec <= 0x20ee; rec += 5) {
          if (s16(rd16(l, best)) > s16(rd16(l, rec))) best = rec;
        }
        wr16(l, best, d2);
        int b3 = ((d2 & 0xff) - rd(l, 0x2204)) & 0xf8;
        b3 = (0 - b3) & 0xff;
        wr16(l, best + 3, ((b3 << 2) + 0x341c) & 0xffff);
      }
      u = saved;
    }
  }
  // $AD1B: the goal and the trampolines move with the scroll.
  const auto advance = [&](int hi, int lo) {
    const int sum = rd16(l, lo) + b5;
    wr16(l, lo, sum);
    const bool carry = sum > 0xffff;
    if (!(rd(l, dp(0xb5)) & 0x80)) {
      if (carry) wr(l, hi, rd(l, hi) + 1);
    } else {
      if (!carry) wr(l, hi, rd(l, hi) - 1);
    }
  };
  advance(dp(0xdf), dp(0xe0));
  advance(dp(0xe4), dp(0xe5));
  advance(dp(0xe9), dp(0xea));
  advance(dp(0xee), dp(0xef));
  (void)game;
}

// $AE97: the rope slots, the goal, the trampolines, the popup and the plaque.
inline void ropesAndTiles(Game& game, Level6& l, int u) {
  wr(l, dp(0xd4), rd(l, u + 9));
  const int da = rd(l, dp(0xda));
  const int d9 = rd(l, dp(0xd9));
  const int dc = rd(l, dp(0xdc));
  const auto inRange = [&](int index) {
    index &= 0xff;
    if ((index - d9) & 0x80) return false;   // BMI after CMPB $D9
    return index <= dc;
  };
  const auto fillSlot = [&](int slot, int rec, int f9) {
    int x = 0x2400 + slot * 0x100 + 0xf6;
    int uu = rec + 0x30;
    for (int n = 0x10; n > 0; --n) {
      --uu;
      int d = (s8(rd(l, uu)) + s16(f9)) & 0xffff;
      int b = d & 0xff;
      if ((d >> 8) != 0) b = 0;
      wr(l, x, b);
      wr(l, x - 2, rd(l, uu - 0x10));
      x -= 0x10;
    }
  };
  const auto codes = [&](int slot, int index, int rec) {
    if (((index & 3) - 2) == 0) decoratedRopeCodes(l, rec, slot);
    else plainRopeCodes(l, slot);
  };
  // Slot 0: the current bar.
  {
    int f9 = 0x8000 | da;
    if (inRange(da)) {
      ropeColours(l, u, 0x24ef);
      f9 = rd16(l, dp(0xd1));
    }
    fillSlot(0, u, f9);
    codes(0, da, u);
  }
  // Slot 1: the bar after it.
  int u1 = findRecord(l, (da + 1) & 0xff);
  wr(l, dp(0xd5), rd(l, u1 + 9));
  {
    int f9 = 0x8000 | ((da + 1) & 0xff);
    if (inRange(da + 1)) {
      ropeColours(l, u1, 0x25ef);
      int b = (rd(l, dp(0xd4)) + 0x10 + rd(l, u1 + 9)) & 0xff;
      f9 = (b + rd16(l, dp(0xd1))) & 0xffff;
    }
    fillSlot(1, u1, f9);
    codes(1, da + 1, u1);
  }
  // Slot 2: two bars on.
  int u2 = findRecord(l, (da + 2) & 0xff);
  {
    int f9 = 0x8000 | ((da + 2) & 0xff);
    if (inRange(da + 2)) {
      ropeColours(l, u2, 0x26ef);
      // The carry taken is the last ADDB's only.
      int b = (((rd(l, dp(0xd5)) << 1) & 0xff) + rd(l, dp(0xd4))) & 0xff;
      b = (b + 0x20) & 0xff;
      const int sum = b + rd(l, u2 + 9);
      const int a = (sum > 0xff) ? 1 : 0;
      b = sum & 0xff;
      f9 = (((a << 8) | b) + rd16(l, dp(0xd1))) & 0xffff;
    }
    fillSlot(2, u2, f9);
    codes(2, da + 2, u2);
  }
  // $AFBF: with slot 2 off the left edge, the bar before the current one
  // takes it, and then the one before that takes slot 1.
  const auto nearZero = [&](int col) { return ((col + 5) & 0xff) <= 0x0a; };
  if (nearZero(rd(l, 0x26f6)) && nearZero(rd(l, 0x2606))) {
    int up = findRecord(l, (da - 1) & 0xff);
    int f9 = 0x8000 | ((da - 1) & 0xff);
    if (inRange(da - 1)) {
      ropeColours(l, up, 0x26ef);
      wr(l, dp(0xd6), rd(l, up + 9));
      int b = (rd(l, dp(0xd4)) + 0x10 + rd(l, up + 9)) & 0xff;
      b = (0 - b) & 0xff;
      f9 = ((0xff00 | b) + rd16(l, dp(0xd1))) & 0xffff;
    }
    fillSlot(2, up, f9);
    codes(2, da - 1, up);
    if (nearZero(rd(l, 0x25f6)) && nearZero(rd(l, 0x2506))) {
      int upp = findRecord(l, (da - 2) & 0xff);
      int f9b = 0x8000 | ((da - 2) & 0xff);
      if (inRange(da - 2)) {
        ropeColours(l, upp, 0x25ef);
        // CLRA; ...ADDB; ADCA #0; COMA; NEGB: the negative of the 9-bit sum.
        int b = (((rd(l, dp(0xd6)) << 1) & 0xff) + rd(l, dp(0xd4))) & 0xff;
        b = (b + 0x20) & 0xff;
        const int sum = b + rd(l, upp + 9);
        f9b = (((0 - sum) & 0xffff) + rd16(l, dp(0xd1))) & 0xffff;
      }
      fillSlot(1, upp, f9b);
      codes(1, da - 2, upp);
    }
  }
  drawGoalTiles(l);
  // $B154: the goal marker sprites.
  {
    int d = (rd16(l, dp(0xdf)) - 0x18) & 0xffff;
    int b = d & 0xff;
    if ((d >> 8) != 0) b = 0;
    wr(l, 0x2736, b);
    wr(l, 0x2726, b);
  }
  // $B163: the trampolines' tiles.
  for (int fd = 0x0a; fd >= 0; fd -= 5) {
    const int rec = 0x20e4 + fd;
    int d = rd16(l, rec);
    int uu = rd16(l, rec + 3);
    int fb = 0;
    if (d & 0x8000) continue;
    if (d >= 0x130) continue;
    if (d >= 0x38) {
      d = (d - 0xf8) & 0xffff;
      if (d & 0x8000) continue;
      fb = 0xff;
    }
    int b = (d & 0x38) >> 1;
    b = ((s8(b) >> 1) + b) & 0xff;
    int x = b;
    int y = 0;
    for (int fc = 6; fc > 0; --fc) {
      const int selector = kLevel6GoalRows[static_cast<std::size_t>(x % 42)] ^ fb;
      ++x;
      if ((selector & 0xff) == 0) {
        wr(l, uu, kLevel6TrampolineTiles[static_cast<std::size_t>(y % 18)]);
        wr(l, uu + 1, kLevel6TrampolineTiles[static_cast<std::size_t>((y + 1) % 18)]);
        wr(l, uu + 2, kLevel6TrampolineTiles[static_cast<std::size_t>((y + 2) % 18)]);
        wr16(l, uu - 0x400, 0x1010);
      } else if (((selector + 1) & 0xff) == 0) {
        wr16(l, uu, 0x6070);
        wr(l, uu + 2, 0x61);
        wr16(l, uu - 0x400, 0x1010);
      }
      y += 3;
      uu += 0x20;
      if (uu >= 0x3800) uu -= 0x400;
    }
  }
  // $B1E6: a bounced trampoline flashes for six frames.
  if (rd(l, dp(0xf3)) != 0) {
    int uu = 0x20e7;
    int found = -1;
    for (int a = 2; a > 0; --a) {
      if (rd(l, uu - 3) == 0 && !(rd(l, uu - 2) & 0x80)) { found = uu; break; }
      uu += 5;
    }
    if (found < 0) found = uu;
    int vram = (rd16(l, found) - 0x400) & 0xffff;
    wr(l, dp(0xf3), rd(l, dp(0xf3)) - 1);
    int a = 0x10;
    if (rd(l, dp(0xf3)) != 0) a = 0x17;
    for (int f9 = 6; f9 > 0; --f9) {
      wr(l, vram, a); wr(l, vram + 1, a);
      vram += 0x20;
      if (vram >= 0x3400) vram -= 0x400;
    }
  }
  // $B223: the points popup times out.
  if (rd(l, dp(0xf4)) != 0) {
    wr(l, dp(0xf4), rd(l, dp(0xf4)) - 1);
    if (rd(l, dp(0xf4)) == 0) {
      wr16(l, 0x271e, 0xfe00);
      wr16(l, 0x270e, 0xfe00);
    }
  }
  // $B234: the plaque rides the scroll.
  {
    int f9 = 0;
    int x = 0x27e0;
    int a = (rd(l, 0x2204) + 0x68) & 0xff;
    wr(l, x + 6, a);
    a = (a + 0x10) & 0xff;
    wr(l, x - 0xa, a);
    a = (a + 0x10) & 0xff;
    wr(l, x - 0x1a, a);
    for (;;) {
      a = rd(l, x + 6);
      if (a >= 0xf0) {
        int b = (rd(l, 0x2203) * 3 + f9) & 0xff;
        if (a >= 0xf8) b = (b - 3) & 0xff;
        wr(l, x + 0xe, kLevel6PlaqueCells[static_cast<std::size_t>(b % 27)]);
        break;
      }
      ++f9;
      x -= 0x10;
      if (x < 0x27c0) break;
    }
  }
  (void)game;
}

// $AC27: the frame's common tail.
inline void commonTail(Game& game, Level6& l) {
  swingAll(game, l);
  int u = findRecord(l, rd(l, dp(0xda)));
  if (rd(l, dp(0xb0)) == 0) {
    hangingTail(game, l, u);
  } else {
    airborneTail(game, l, u);
  }
  ropesAndTiles(game, l, u);
}

// $B857: the floor.
inline void hitFloor(Game& game, Level6& l, int a) {
  a = (a + 0x14) & 0xff;
  if (a & 0x80) {
    wr(l, 0x277e, 0x31); wr(l, 0x276e, 0x32); wr(l, 0x275e, 0x33); wr(l, 0x274e, 0x34);
    wr(l, 0x2774, 0xd4); wr(l, 0x2764, 0xd4);
    wr(l, 0x2754, 0xe4); wr(l, 0x2744, 0xe4);
  } else {
    wr(l, 0x277e, 0x2d); wr(l, 0x276e, 0x2e);
    wr(l, 0x275e, 0xaf); wr(l, 0x274e, 0xb0);
    wr(l, 0x2774, 0); wr(l, 0x2764, 0);
    wr(l, 0x2754, 0x0e); wr(l, 0x2744, 0x0e);
  }
  wr(l, 0x27b4, 0xaa); wr(l, 0x27a4, 0xaa); wr(l, 0x2794, 0xaa);
  wr(l, 0x2784, 0xb2); wr(l, 0x2714, 0xb2); wr(l, 0x2704, 0xb2);
  wr(l, 0x27b6, 0x30); wr(l, 0x27a6, 0x40); wr(l, 0x2796, 0x50);
  wr(l, 0x2786, 0x90); wr(l, 0x2716, 0xa0); wr(l, 0x2706, 0xb0);
  wr(l, 0x27be, 0x16); wr(l, 0x278e, 0x16);
  wr(l, 0x27ae, 0x17); wr(l, 0x271e, 0x17);
  wr(l, 0x279e, 0x18); wr(l, 0x270e, 0x18);
  sound(game, 0x4f);
  wr(l, dp(0xb5), 0); wr(l, dp(0xb6), 0);
  commonTail(game, l);
  wr(l, dp(0xf8), 0x80);
  wr(l, 0x2800, rd(l, 0x2800) + 2);
}

// $AAA9: a playing frame.
inline void playing(Game& game, Level6& l) {
  flasher(l);
  goalColours(l);
  if (rd(l, dp(0xb0)) != 0) {
    // Airborne.
    if (rd(l, dp(0xdf)) == 0) {
      const int a = (rd(l, dp(0xe0)) - 0x3c) & 0xff;
      if (a <= 0x28) {
        const int row = (rd(l, 0x2774) - 0x68) & 0xff;
        if (row <= 2) {
          goalLanding(game, l);
          return;
        }
      }
    }
    bool caught = false;
    if (!(rd(l, dp(0xb7)) & 0x80)) {
      // Falling: a trampoline under him bounces him back up.
      int u = 0x20e4;
      bool onScreen = false;
      for (; u < 0x20f3; u += 5) {
        if (rd(l, u) == 0) { onScreen = true; break; }
      }
      if (onScreen) {
        const int a = (rd(l, u + 1) - 0x40) & 0xff;
        if (a <= 0x20 && rd(l, 0x2774) >= 0xcc) {
          wr(l, dp(0xde), 0);
          sound(game, 0x45);
          wr(l, dp(0xf3), 6);
          const int stick = l.curInput & 3;
          int b = 0;
          if (stick != 0) {
            b = 0x70;
            if (!(stick & 1)) b = (0 - b) & 0xff;
          }
          const int d = (s8(b)) & 0xffff;
          wr16(l, dp(0xb5), d);
          int index = 0;
          if (d == 0) {
            const int b0 = rd(l, dp(0xb0));
            wr(l, dp(0xb0), b0 + 1);
            index = (((b0 - 1) & 0xff) << 1) & 0xff;
          }
          wr(l, dp(0xb7), kLevel6BounceVelocity[static_cast<std::size_t>(index % 14)]);
          wr(l, dp(0xb8), kLevel6BounceVelocity[static_cast<std::size_t>((index + 1) % 14)]);
          caught = true;   // straight to $ABEF
        }
      }
    }
    if (!caught && rd(l, dp(0xde)) == 0) {
      // $AB3C: the catch.
      const int db = rd(l, dp(0xda)) & 3;
      int f9 = 0x06, fa = 0x0c;
      if (((db - 2) & 0xff) == 0) { f9 = 0x09; fa = 0x0c; }
      int a = (rd(l, 0x24f6) - 0x34 + f9) & 0xff;
      if (a <= ((f9 << 1) & 0xff)) {
        a = (rd(l, 0x24f4) - rd(l, 0x2774) - 1 + fa) & 0xff;
        if (a <= ((fa << 1) & 0xff)) {
          wr(l, dp(0xb0), 0);
          flash(l);
          wr(l, dp(0xdb), rd(l, dp(0xda)));
          int u = findRecord(l, rd(l, dp(0xda)));
          int angle = rd(l, u + 2);
          if (angle > 0x5a) angle = (0xb4 - angle) & 0xff;
          const int amp = (kLevel6Sine[static_cast<std::size_t>(angle % 0x5b)] * rd(l, u + 8)) >> 8;
          wr(l, u + 8, amp);
          wr16(l, u + 2, 0x5a00);
          const int up = findRecord(l, (rd(l, dp(0xda)) - 1) & 0xff);
          int aa = (rd(l, up + 4) << 1) & 0xff;
          int b = (0x3c - rd(l, up + 9)) & 0xff;
          b = (s8(b) >> 1) & 0xff;
          wr(l, dp(0xf9), b);
          aa = (aa - b) & 0xff;
          wr(l, dp(0xf9), aa);
          wr(l, 0x20aa, kLevel6Points[static_cast<std::size_t>(aa % 32)]);
          wr(l, 0x20ab, kLevel6Points[static_cast<std::size_t>((aa + 1) % 32)]);
          wr(l, 0x20a9, 0);
          addPoints(game, l);
          const int entry = (aa << 1) & 0xff;   // four bytes from $EFB8 + 2 * $F9
          wr16(l, 0x271e, (kLevel6PopupCells[static_cast<std::size_t>(entry % 64)] << 8) |
                              kLevel6PopupCells[static_cast<std::size_t>((entry + 1) % 64)]);
          wr16(l, 0x270e, (kLevel6PopupCells[static_cast<std::size_t>((entry + 2) % 64)] << 8) |
                              kLevel6PopupCells[static_cast<std::size_t>((entry + 3) % 64)]);
          wr(l, dp(0xf4), 0x30);
          wr(l, dp(0xdd), 7);
          commonTail(game, l);
          return;
        }
      }
    }
    // $ABEF: gravity.
    int d = (rd16(l, dp(0xb7)) + 0x15) & 0xffff;
    wr16(l, dp(0xb7), d);
    d = (d + rd16(l, 0x2774)) & 0xffff;
    wr16(l, 0x2774, d);
    int a = d >> 8;
    wr(l, 0x2764, a);
    a = (a + 0x10) & 0xff;
    wr(l, 0x2754, a); wr(l, 0x2744, a);
    a = (a - 0x10) & 0xff;
    if (a > 0xd8) {
      hitFloor(game, l, a);
      return;
    }
    commonTail(game, l);
    return;
  }
  // $AC10: hanging on the bar.
  wr(l, dp(0xd7), l.curInput);
  const int edge = ((~l.curInput) | l.prevInput) & 0x10;
  if ((edge | rd(l, dp(0xdd))) == 0) release(game, l);
  commonTail(game, l);
}

// $B8FB: on the floor, waiting for the restart (phase 3).
inline void down(Game& game, Level6& l) {
  commonTail(game, l);
  wr(l, dp(0xf8), rd(l, dp(0xf8)) - 1);
  if (rd(l, dp(0xf8)) != 0) return;
  int a = (rd(l, dp(0xdb)) - rd(l, dp(0xd9)) + rd(l, 0x2208)) & 0xff;
  wr(l, 0x2208, a);
  int page = rd(l, 0x2203);
  if (page != 0) {
    if (page >= 7) page = 6;
    page = (page - 1) & 0xff;
  }
  wr(l, 0x2203, page);
  l.restartPage = page;
  l.died = true;
}

// $B6FE: the celebration (phase 2).
inline void celebration(Game& game, Level6& l) {
  (void)game;
  wr(l, 0x287f, 2);
  flasher(l);
  goalColours(l);
  {
    const int k = rd(l, dp(0xf7)) & 0x0e;
    const int a = kLevel5PopupRowA[static_cast<std::size_t>(k)];
    const int b = kLevel5PopupRowA[static_cast<std::size_t>(k + 1)];
    wr(l, 0x245f, a); wr(l, 0x244f, a); wr(l, 0x243f, a);
    wr(l, 0x242f, b); wr(l, 0x241f, b); wr(l, 0x240f, b);
  }
  if (rd(l, 0x2738) != 0) {
    // $B608: the bag flies to Charlie.
    wr(l, dp(0xf7), rd(l, dp(0xf7)) + 1);
    const int remainingBag = (rd(l, 0x2738) - 1) & 0xff;
    if (remainingBag != 0) {
      const int x = 0x2730;
      int d = (rd16(l, x + 2) + rd16(l, x + 6)) & 0xffff;
      wr16(l, x + 6, d);
      wr(l, x - 0xa, d >> 8);
      d = (rd16(l, x) + 0x15) & 0xffff;
      wr16(l, x, d);
      d = (d + rd16(l, x + 4)) & 0xffff;
      wr16(l, x + 4, d);
      int row = ((d >> 8) + 0x10) & 0xff;
      wr(l, x - 0xc, row);
      if (row == 0x78) {
        wr(l, 0x273e, 0x55); wr(l, 0x272e, 0x5d);
        wr(l, 0x2738, rd(l, 0x2738) - 1);
      }
      return;
    }
    // $B635: Charlie waves with the bag.
    wr(l, 0x2739, rd(l, 0x2739) + 1);
    if (rd(l, 0x2739) != 0) {
      const int t = rd(l, 0x2739);
      if ((t & 0x1f) != 0) return;
      if ((t & 0x20) == 0) {
        wr(l, 0x273e, 0x55); wr(l, 0x272e, 0x5d);
        wr(l, 0x277e, 0x76); wr(l, 0x276e, 0x77);
        wr(l, 0x275e, 0x7e); wr(l, 0x274e, 0x7f);
      } else {
        wr(l, 0x273e, 0x5a); wr(l, 0x272e, 0x7b);
        wr(l, 0x277e, 0x74); wr(l, 0x276e, 0x75);
        wr(l, 0x275e, 0x7c); wr(l, 0x274e, 0x7d);
      }
      return;
    }
    // $B67D: the finale begins.
    wr(l, 0x2474, rd(l, 0x2734)); wr(l, 0x2476, rd(l, 0x2736));
    wr(l, 0x2464, rd(l, 0x2724)); wr(l, 0x2466, rd(l, 0x2726));
    wr16(l, 0x247e, 0x5520);
    wr16(l, 0x246e, 0x5d20);
    wr(l, 0x2738, 0);
    wr(l, 0x2470, 0);
    int x = 0x2484;
    wr(l, x + 1, 0);
    int a = 0x68;
    for (int f9 = 2; f9 > 0; --f9) {
      int b = 0x44;
      for (int fa = 0x0c; fa > 0; --fa) {
        wr(l, x, a);
        b = (b - 0x10) & 0xff;
        wr(l, x + 2, b);
        wr(l, x + 3, 0);
        wr(l, x + 0x10, a);
        b = (b + 0x10) & 0xff;
        wr(l, x + 0x12, b);
        wr(l, x + 0x13, 0);
        x += 0x40;
      }
      x -= 0x2e0;
      a = (a + 0x10) & 0xff;
    }
    // $B791: twelve copies of the cheering cells.
    {
      int xx = 0x248e;
      wr16(l, xx, 0xad00); wr16(l, xx + 0x10, 0xae00);
      wr16(l, xx + 0x20, 0xaf00); wr16(l, xx + 0x30, 0xb000);
      for (int f9 = 4; f9 > 0; --f9) {
        for (int fa = 0x0b; fa > 0; --fa) {
          const int d = rd16(l, xx);
          xx += 0x40;
          wr16(l, xx, d);
        }
        xx -= 0x2b0;
      }
    }
    wr16(l, dp(0xb7), 0xff20);
    int d = 0xff58;
    for (int xx = 0x2480, f9 = 0x0c; f9 > 0; --f9, xx += 0x40) {
      wr16(l, xx, d);
      d = (d + 0x4c) & 0xffff;
    }
    wr(l, dp(0xf7), 0xe0);
    wr(l, dp(0xf6), 0);
    // $BB13: the finale jingle.
    return;
  }
  // $B72B: the finale.
  if ((rd(l, dp(0xf7)) & 0x1f) == 0) {
    wr(l, 0x2470, rd(l, 0x2470) + 1);
    int d = 0x555d;
    if (!(rd(l, 0x2470) & 1)) d = 0x5a7b;
    wr(l, 0x247e, d >> 8);
    wr(l, 0x246e, d & 0xff);
  }
  const auto cheerCells = [&](int a) {
    int xx = 0x248e;
    wr16(l, xx, a << 8); wr16(l, xx + 0x10, (a + 1) << 8);
    wr16(l, xx + 0x20, (a + 2) << 8); wr16(l, xx + 0x30, (a + 3) << 8);
    for (int f9 = 4; f9 > 0; --f9) {
      for (int fa = 0x0b; fa > 0; --fa) {
        const int d = rd16(l, xx);
        xx += 0x40;
        wr16(l, xx, d);
      }
      xx -= 0x2b0;
    }
  };
  const auto moveAll = [&]() {
    // $B767: the copies rise with $B7 and their own velocities.
    int x = 0x2484;
    int d = (rd16(l, dp(0xb7)) + 0x15) & 0xffff;
    wr16(l, dp(0xb7), d);
    d = (d + rd16(l, x)) & 0xffff;
    wr16(l, x, d);
    int a = d >> 8;
    for (int f9 = 2; f9 > 0; --f9) {
      for (int b = 0x0c; b > 0; --b) {
        wr(l, x, a);
        wr(l, x + 0x10, a);
        x += 0x40;
      }
      x -= 0x2e0;
      a = (a + 0x10) & 0xff;
    }
  };
  if (rd(l, dp(0xf6)) == 1) {
    // $B74C: the second act.
    wr(l, dp(0xf7), rd(l, dp(0xf7)) + 1);
    if (rd(l, dp(0xf7)) == 0) {
      cheerCells(0xad);
      wr16(l, dp(0xb7), 0xfd60);
      int d = 0x0398;
      for (int xx = 0x2480, f9 = 0x0c; f9 > 0; --f9, xx += 0x40) {
        wr16(l, xx, d);
        d = (d - 0x4c) & 0xffff;
      }
      wr(l, dp(0xf7), 0xe0);
      wr(l, dp(0xf6), rd(l, dp(0xf6)) + 1);
      return;
    }
    const int a = rd(l, dp(0xf7)) & 0x7f;
    if (a == 0) { cheerCells(0xad); return; }
    if (a < 0x21) return;
    if (a == 0x21) {
      wr16(l, dp(0xb7), 0xfc10);
      cheerCells(0xb1);
    }
    moveAll();
    return;
  }
  if (rd(l, dp(0xf7)) & 0x80) {
    // $B83D
    wr(l, dp(0xf7), rd(l, dp(0xf7)) + 1);
    if (rd(l, dp(0xf7)) == 0) cheerCells(0x29);
    return;
  }
  moveAll();
  {
    int x = 0x2486;
    for (int f9 = 0x0c; f9 > 0; --f9) {
      int d = (rd16(l, x - 6) + rd16(l, x)) & 0xffff;
      wr16(l, x, d);
      int a = d >> 8;
      wr(l, x + 0x20, a);
      a = (a + 0x10) & 0xff;
      wr(l, x + 0x10, a);
      wr(l, x + 0x30, a);
      x += 0x40;
    }
  }
  wr(l, dp(0xf7), rd(l, dp(0xf7)) + 1);
  const int f7 = rd(l, dp(0xf7));
  if (f7 == 0x40) {
    if (rd(l, dp(0xf6)) != 0) {
      l.finished = true;
      return;
    }
    cheerCells(0xad);
    wr(l, dp(0xf7), 0);
    wr(l, dp(0xf6), rd(l, dp(0xf6)) + 1);
    return;
  }
  if (f7 & 7) return;
  {
    const int step = f7 >> 1;
    int b = 0;
    if (step == 0x18) b = 0xc0;
    int xx = 0x248e;
    for (int k = 0; k < 4; ++k) {
      const int code = kLevel6FinaleCells[static_cast<std::size_t>((step + k) % 32)];
      wr16(l, xx + k * 0x10, (code << 8) | b);
    }
    for (int f9 = 4; f9 > 0; --f9) {
      for (int fa = 0x0b; fa > 0; --fa) {
        const int d = rd16(l, xx);
        xx += 0x40;
        wr16(l, xx, d);
      }
      xx -= 0x2b0;
    }
  }
}

}  // namespace l6

void level6Init(Game& game) {
  Level6& l = game.level6;
  l6::wr(l, 0x2800, 0);
  // The wipe left $2400-$28FF clear; the records at $2900 keep their bytes.
  for (int a = 0x2400; a < 0x2900; ++a) l6::wr(l, a, 0);
  l6::wr(l, 0x2204, 0);
  l6::wr(l, 0x2205, 0);
  // $BB25 -> $BCC5: 6000 on the first go, then by the page.
  const int page = l6::rd(l, 0x2203);
  if (!l.started) {
    game.bonus = 6000;
    l.started = true;
  } else {
    game.bonus = page < 2 ? 4800 : (page < 4 ? 4000 : 3000);
  }
  game.level6BonusHold = 0;
  l6::wr(l, 0x2263, 0x40);   // $BB6B
  l6::init(game, l);
}

void syncLevel6World(Game& game) {
  Level6& l = game.level6;
  const int page = l6::rd(l, 0x2203);
  const int low = l6::rd(l, 0x2204);
  const int progress = page * 256 + ((256 - low) & 0xff);
  const float scale = kWorldWidth / 224.0F;
  game.cameraX = static_cast<float>(progress) * scale;
  game.player.position = {static_cast<float>(progress + l6::rd(l, 0x2776)) * scale,
                          level3RowToY(static_cast<float>(l6::rd(l, 0x2774)))};
  game.player.grounded = l6::rd(l, l6::dp(0xb0)) == 0;
  game.player.alive = l6::rd(l, 0x2800) == 1;
}

void updateLevel6(Game& game, const Uint8* keyboard, bool jumpPressed,
                  float controllerAxis) {
  Level6& l = game.level6;
  ++l.hdFrame;
  game.player.previous = game.player.position;
  game.previousCameraX = game.cameraX;
  const bool moveLeft = keyboard[SDL_SCANCODE_LEFT] || controllerAxis < -0.35F;
  const bool moveRight = keyboard[SDL_SCANCODE_RIGHT] || controllerAxis > 0.35F;
  const bool button = jumpPressed || keyboard[SDL_SCANCODE_SPACE] ||
                      keyboard[SDL_SCANCODE_Z];
  l.curInput = (moveLeft ? 1 : 0) | (moveRight ? 2 : 0) | (button ? 0x10 : 0);
  const int phase = l6::rd(l, 0x2800);
  if (phase == 1) {
    // $BB73: one bonus tick every four frames on this stage.  Once the bonus
    // is gone $2263 counts down on the same ticks ($BC6D): at 2 the miss
    // plays and $F5 = 1 freezes the handler, at 0 $F5 = 2 ends the life.
    if (game.level6BonusHold != 0) {
      --game.level6BonusHold;
    } else {
      game.level6BonusHold = 3;
      if (game.bonus > 0) --game.bonus;
      if (game.bonus == 0) {
        const int hold = l6::rd(l, 0x2263);
        if (hold == 0) {
          l6::wr(l, l6::dp(0xf5), 2);
        } else {
          l6::wr(l, 0x2263, hold - 1);
          if (hold == 2) {
            ++game.crashAudioSerial;   // $BADE
            l6::wr(l, l6::dp(0xf5), 1);
          }
        }
      }
    }
    // $B924: $F5 = 1 abandons the frame; $F5 = 2 runs $B902 and goes on.
    const int f5 = l6::rd(l, l6::dp(0xf5));
    if (f5 == 1) {
      l.prevInput = l.curInput;
      syncLevel6World(game);
      return;
    }
    if (f5 >= 2) {
      const int a = (l6::rd(l, l6::dp(0xdb)) - l6::rd(l, l6::dp(0xd9)) + l6::rd(l, 0x2208)) & 0xff;
      l6::wr(l, 0x2208, a);
      int page = l6::rd(l, 0x2203);
      if (page != 0) {
        if (page >= 7) page = 6;
        page = (page - 1) & 0xff;
      }
      l6::wr(l, 0x2203, page);
      l.restartPage = page;
      l.died = true;
    }
    l6::playing(game, l);
  } else if (phase == 2) {
    l6::celebration(game, l);
  } else if (phase == 3) {
    l6::down(game, l);
  }
  l.prevInput = l.curInput;
  if (l.finished) {
    l.finished = false;
    game.scene = Scene::Tally;
    game.tallyFrame = 0;
  } else if (l.died) {
    l.died = false;
    game.scene = Scene::Crashed;
    game.crashFrame = 0;
    game.crashDurationFrames = game.level6FloorRestartFrames > 0
                                   ? game.level6FloorRestartFrames
                                   : kLevel6RestartFrames;
    game.level6FloorRestartFrames = 0;
    if (game.lives > 0) --game.lives;
    l6::wr(l, 0x2800, 0);
    for (int a = 0x2400; a < 0x2900; a++) l6::wr(l, a, 0);
    l6::wr(l, 0x2204, 0);
    l6::wr(l, 0x2205, 0);
  }
  syncLevel6World(game);
}
