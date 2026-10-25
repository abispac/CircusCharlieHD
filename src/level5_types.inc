// ============================================================================
// Event 5 (pony and springboards): a clean-room port of the circusc4 stage
// handler at $9F0D-$A8E8.  docs/LEVEL5_ROM_MODEL.md explains the model and
// lists the captures in docs/diagnostics/level5-replay/ that verify it.
//
// Unlike Events 2-4 this handler keeps its state in the direct page
// ($20B0-$20C2) and in a few fixed sprite cells, and it draws the springboards
// into the background tilemap column by column as the field scrolls; the
// records at $2900-$2970 hold each springboard's tile pointer, screen x,
// platform row and collision widths.  Every field is kept here under the
// name of the address it comes from so a capture can be compared byte for
// byte.
// ============================================================================

// $EE78: the somersault cells (indexed by $BC / 2) and, at $EE78, the jump.
constexpr std::array<int, 28> kLevel5TumbleCells{
    0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, 0xa0,
    0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa4, 0xa3, 0xa2, 0xa1,
    0xa9, 0xaa, 0xab, 0xac};
// $EECA: the pony's leading cell for each of the four gait frames.
constexpr std::array<int, 4> kLevel5PonyCells{0x65, 0x6b, 0x71, 0x77};
// $EF25: three plaque cells per page (START, 180M ... GOAL) and the run-in.
constexpr std::array<int, 0x60> kLevel5PlaqueCells{
    0xb8, 0xb9, 0xba, 0xbb, 0xc7, 0xbc, 0xbb, 0xc6, 0xbc, 0xbb, 0xc5, 0xbc,
    0xbb, 0xc1, 0xbc, 0xbb, 0xc0, 0xbc, 0xbb, 0xbf, 0xbc, 0xbb, 0xbe, 0xbc,
    0xbb, 0xbd, 0xbc, 0xbb, 0xc9, 0xbc, 0xca, 0xc8, 0xbc, 0xca, 0xc7, 0xbc,
    0xca, 0xc6, 0xbc, 0xca, 0xc5, 0xbc, 0xca, 0xc1, 0xbc, 0xca, 0xc0, 0xbc,
    0xca, 0xbf, 0xbc, 0xca, 0xbe, 0xbc, 0xca, 0xbd, 0xbc, 0xc2, 0xc3, 0xc4,
    0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xb8, 0xb9, 0xba, 0xca, 0xc1, 0xbc,
    0xca, 0xc0, 0xbc, 0xca, 0xbf, 0xbc, 0xca, 0xbe, 0xbc, 0xca, 0xbd, 0xbc,
    0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0x12, 0x21, 0x12};
// $EF8C: landing points by pole height (BCD tens) and $EF92 by platform type.
constexpr std::array<int, 7> kLevel5HeightPoints{0x60, 0x50, 0x40, 0x30,
                                                 0x20, 0x10, 0x00};
// $EF92: (low, high) BCD points per springboard type 0x12-0x17.
constexpr std::array<int, 12> kLevel5TypePoints{0x00, 0x02, 0x00, 0x01, 0x80, 0x00,
                                                0x50, 0x00, 0x20, 0x00, 0x10, 0x00};
// $F67C / $F6E4 / $F74C: the springboard schedule -- pole height, platform
// type ($12 high, $13 middle, $14 low) and the gap to the next one.
constexpr std::array<int, 0x68> kLevel5Heights{
    7, 7, 7, 7, 7, 7, 7, 7, 7, 6, 7, 6, 7, 6, 7, 6, 7, 6, 5, 6, 7, 7, 6, 5, 7, 6,
    7, 5, 6, 6, 4, 6, 6, 5, 6, 4, 7, 5, 6, 4, 6, 4, 3, 7, 4, 5, 7, 4, 6, 3, 5, 6,
    4, 4, 5, 4, 5, 3, 5, 4, 4, 4, 6, 3, 5, 4, 3, 4, 3, 4, 2, 4, 4, 3, 4, 3, 3, 4,
    2, 3, 3, 4, 3, 2, 5, 2, 3, 2, 2, 2, 3, 3, 2, 3, 2, 3, 2, 2, 2, 2, 2, 2, 2, 2};
constexpr std::array<int, 0x68> kLevel5Types{
    0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x12, 0x14, 0x14,
    0x14, 0x12, 0x14, 0x14, 0x14, 0x13, 0x14, 0x13, 0x14, 0x12, 0x13, 0x13,
    0x14, 0x14, 0x13, 0x14, 0x12, 0x13, 0x14, 0x13, 0x13, 0x12, 0x13, 0x12,
    0x14, 0x14, 0x12, 0x13, 0x13, 0x13, 0x12, 0x14, 0x12, 0x13, 0x14, 0x14,
    0x13, 0x12, 0x13, 0x13, 0x12, 0x13, 0x14, 0x13, 0x13, 0x13, 0x12, 0x12,
    0x13, 0x12, 0x13, 0x13, 0x14, 0x12, 0x13, 0x13, 0x12, 0x13, 0x12, 0x13,
    0x13, 0x14, 0x12, 0x13, 0x12, 0x14, 0x13, 0x13, 0x13, 0x12, 0x13, 0x13,
    0x12, 0x13, 0x12, 0x14, 0x12, 0x14, 0x13, 0x14, 0x14, 0x12, 0x12, 0x13,
    0x14, 0x12, 0x12, 0x14, 0x12, 0x13, 0x12, 0x14};
constexpr std::array<int, 0x68> kLevel5Gaps{
    0x50, 0xf8, 0x50, 0xf8, 0x50, 0xf8, 0x50, 0xf8, 0x50, 0xf8, 0x50, 0xf8,
    0x50, 0xf8, 0x50, 0xf8, 0x50, 0xe0, 0x50, 0xe8, 0x50, 0xd8, 0x50, 0xf0,
    0x50, 0xd0, 0x50, 0xe8, 0x50, 0xd8, 0x50, 0xe0, 0x50, 0xc8, 0x50, 0xb8,
    0x50, 0xe0, 0x50, 0xb0, 0x50, 0xd8, 0x50, 0xe0, 0x50, 0xb0, 0x50, 0xc8,
    0x50, 0xb0, 0x50, 0x98, 0x50, 0xe8, 0x50, 0xc8, 0x50, 0xe0, 0x50, 0x78,
    0x50, 0x80, 0x50, 0xb0, 0x50, 0x98, 0x50, 0x78, 0x50, 0xb8, 0x50, 0x68,
    0x50, 0x60, 0x50, 0xc8, 0x50, 0xd0, 0x50, 0x98, 0x50, 0x80, 0x50, 0x50,
    0x50, 0xb8, 0x50, 0x98, 0x50, 0xb0, 0x50, 0x48, 0x50, 0x80, 0x50, 0x68,
    0x50, 0x6c, 0x50, 0x84, 0x50, 0x54, 0x50, 0x84};
// $F102: the two cell rows of the OH NO / GREAT animation, by $BC & $0E.
constexpr std::array<int, 16> kLevel5PopupRowA{0x00, 0x00, 0x01, 0x01, 0x04, 0x03,
                                               0x06, 0x06, 0x07, 0x09, 0x0a, 0x0a,
                                               0x0c, 0x0c, 0x0e, 0x0e};
// $FB80-$FB8A: the bonus on a fresh start and by page after a miss.
constexpr int kLevel5FirstBonus = 5360;
constexpr std::array<int, 3> kLevel5RestartBonus{4800, 4000, 3000};
constexpr int kLevel5BonusHold = 2;   // $FB9C: two held frames per tick

inline int u8w(int v) { return v & 0xff; }

// Spring records at $2900 + 16 * i.
struct Level5Spring {
  int state = 0;    // ,U    0 free, 1 waiting its gap, 2 being drawn, 3 live
  int gap = 0;      // $1,U  scroll pixels until it enters (state 1)
  int height = 0;   // $2,U  pole height in tiles
  int cols = 0;     // $3,U  tile columns left to draw / erase
  int type = 0;     // $4,U  $12 high, $13 middle, $14 low platform
  int col = 0;      // $5,U  the $2204 value of the next column step
  int vram = 0;     // $6,U:$7,U tilemap pointer
  int x = 0;        // $8,U  screen x reference
  int row = 0;      // $9,U  platform row, ($4 + 1) * 8
  int halfA = 0;    // $A,U  landing half width
  int halfB = 0;    // $B,U  under-side half width
  int halfC = 0;    // $C,U  pole strike half width
  int tileD = 0;    // $D,U / $E,U  the compressed platform tiles
  int tileE = 0;
  int hdX = 0;      // HD only: the screen column unwrapped, so a board that
                    // has left by the left edge never reappears on the right
  int points = 0;   // HD only: the points a landing scored, shown over the board
};

struct Level5 {
  int phase = 0;              // $2800: 0 init, 1 playing, 2 Charlie down
  int scroll = 0;             // $2203:$2204
  int spawnCount = 0;         // $2208
  int perfect = 0;            // $220A: set by a double bounce
  int chain = 0;              // $220B
  std::array<int, 0x13> v{};  // $20B0-$20C2
  int chRow = 0, chCol = 0, chCode = 0;        // $26B4 $26B6 $26BE
  int chFrac = 0;             // $26B5: the fall's 8.8 fraction rides along
  int curInput = 0;           // this frame's $2032 byte
  int birdTarget = 0, birdCol = 0;   // $2560 / $2566: the extra-Charlie bird
  bool birdActive = false;
  int ponyRow = 0, ponyCol = 0, ponyCode = 0;  // $2674 $2676 $267E
  std::array<int, 3> plRow{}, plCol{}, plCode{};   // $25F0, $2600, $2610
  std::array<Level5Spring, 8> springs{};
  std::array<int, 6> popupRow{}, popupCol{}, popupCode{};   // $2590-$25E0
  int popupAnimA = 0, popupAnimB = 0;  // $25EF / $25BF
  int bonusHold = 0;          // $2276
  int timeout = 0x40;         // $2263
  int dips = 0xb4;            // $202F
  int visits = 1;             // $2212
  int prevInput = 0;          // $2035: last frame's port image
  int flashState = 0, flashTimer = 0, flashPhase = 0;   // $287D $287E $287F
  int lyingTimer = 0;
  int hdAir = 0;              // HD animation: frames since the $29 cells came up
  bool hdRising = false;      // HD animation: the row went up this frame
  int hdPrevRow = 0;
  bool started = false;
  bool died = false;
  bool finished = false;
  int restartPage = 0;
  int score = 0;
};

