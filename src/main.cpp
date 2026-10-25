#include <SDL.h>
#include <SDL_image.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <functional>
#include <iostream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr int kWorldWidth = 480;
constexpr int kWorldHeight = 640;
constexpr double kBoardRefresh =
    6144000.0 / (384.0 * 264.0);  // 60.606060...
constexpr double kFixedDt = 1.0 / kBoardRefresh;
// The original 224x256 cabinet frame puts the rider's contact point near
// source y=230.  On the 480x640 logical canvas that places Charlie close to
// the bottom instead of halfway up the playfield.
constexpr float kGroundY = 590.0F;
// The HD distance sign (assets/floor-sign-v1.png), shared by every event's
// distance markers once it is loaded.
SDL_Texture* gFloorSign = nullptr;
// The rope is on source rows 152-153: row * 2.5 - 8.
constexpr float kStage2RopeY = 372.0F;
constexpr float kStage2MonkeySpeed = 52.0F;
constexpr float kStage2PurpleSpeed = 61.0F;
constexpr float kStage2GoalTopY = kStage2RopeY - 76.0F;
constexpr float kStage2GoalX = 5700.0F;
// ---- circusc4 Level 3 (trampolines) board geometry -----------------------
// Everything in Level 3 is kept in the board's own units: sprite columns
// ($6 bytes, 224-wide screen) and rows ($4 bytes, 256-tall screen).  The
// logical canvas maps a board row r to 2.5 * r - 8 so that a performer's feet
// (row $E0 + 32) land on the grass line and Charlie's feet (row $B4 + 32) on
// the drum tops.  Columns use the same 480/224 scale as every other event.
constexpr float kStage3GroundY = 592.0F;
constexpr float kStage3TambourineTopY = 482.0F;
constexpr float kLevel3RowScale = 2.5F;
constexpr float kLevel3RowOffset = -8.0F;
// $8D9D scrolls $2203:$2204 two columns per frame; the scroll stops once the
// page byte reaches $F8 (1794 columns) and Charlie then walks from column
// $50 to $C0 himself.  Drums are the tile rows 6-11, 17-22 and 27-0 of the
// 256-column tilemap page: centres 80, 168 and 256 (+256 per page).
constexpr int kLevel3PlayerColumn = 0x50;
constexpr int kLevel3StandRow = 0xb4;
constexpr int kLevel3PerformerRow = 0xe0;
constexpr int kLevel3BagRow = 0x50;
constexpr int kLevel3ScrollEnd = 1794;
constexpr std::array<int, 3> kLevel3DrumCentres{80, 168, 256};
// $FA65: launch velocities of the stationary rebounds (8.8 rows per frame).
constexpr std::array<std::uint16_t, 5> kLevel3StationaryLaunch{
    0x0450, 0x0510, 0x0630, 0x0810, 0x0810};
// $FA43: bag spawn points as ($2203, $2204) pairs.  $8AC7 stops at the first
// entry whose page byte matches, so the second $F9 entry can never trigger.
constexpr std::array<std::pair<std::uint8_t, std::uint8_t>, 8> kLevel3BagSpawns{{
    {0xff, 0x40}, {0xfe, 0x98}, {0xfd, 0x98}, {0xfc, 0x40},
    {0xfb, 0x98}, {0xfa, 0x40}, {0xf9, 0x98}, {0xf9, 0x08}}};
// $FAA1/$FAAB: performer attack period, indexed by dip difficulty / 2 plus
// the number of bag thresholds already passed in this game.
constexpr std::array<int, 9> kLevel3BagThresholds{8, 10, 12, 14, 16, 19, 22, 25, 28};
constexpr std::array<std::uint8_t, 10> kLevel3AttackPeriods{
    0x3a, 0x39, 0x38, 0x34, 0x30, 0x2c, 0x28, 0x24, 0x22, 0x20};
// $F517: six difficulty tables of nine pages, three performer slots per page
// (0 none, 1 fire breather, 2 juggler with one knife, 3 juggler with two).
// Slot 0 spawns at $2204 == $C4, slot 1 at $74 and slot 2 at $18.
constexpr std::array<std::array<std::array<std::uint8_t, 3>, 9>, 6> kLevel3PerformerTables{{
    {{{0,0,1},{0,0,1},{0,0,2},{0,1,1},{2,2,1},{0,0,2},{0,1,1},{1,2,0},{1,2,0}}},
    {{{0,1,0},{0,1,0},{2,0,0},{2,0,1},{0,1,1},{0,1,1},{0,1,1},{1,3,0},{1,3,0}}},
    {{{0,1,0},{0,1,0},{3,3,0},{3,2,1},{1,3,2},{3,2,1},{0,1,1},{2,3,0},{2,3,0}}},
    {{{0,1,1},{0,1,1},{2,2,0},{1,2,0},{3,3,0},{1,3,2},{0,1,1},{3,3,0},{3,3,0}}},
    {{{0,1,1},{0,1,1},{1,2,0},{1,3,2},{0,1,2},{1,3,2},{1,2,0},{3,3,0},{3,3,0}}},
    {{{0,1,1},{0,1,1},{2,2,1},{2,2,1},{1,3,1},{1,3,2},{3,2,1},{3,3,0},{3,3,0}}}}};
// $F5C5: the water round's six tables of nine pages.  Slot bytes 1 and 2 are
// dolphins that start with a leap (1) or a dive (2), 3 is a pair (a diver and
// a leaper in one spot) and bit 7 ($82) is a diver that is moved under water
// to the left edge of the screen when it spawns.  In the water round $958B
// also moves the table index up two for every bag threshold passed.
constexpr std::array<std::array<std::array<std::uint8_t, 3>, 9>, 6> kLevel3WaterTables{{
    {{{0x00,0x03,0x03},{0x00,0x03,0x03},{0x82,0x82,0x82},{0x02,0x02,0x02},{0x01,0x01,0x02},{0x82,0x82,0x82},{0x02,0x02,0x02},{0x02,0x01,0x00},{0x02,0x01,0x00}}},
    {{{0x02,0x02,0x02},{0x02,0x02,0x02},{0x02,0x02,0x02},{0x01,0x01,0x82},{0x01,0x02,0x01},{0x01,0x02,0x01},{0x01,0x02,0x01},{0x02,0x82,0x00},{0x02,0x82,0x00}}},
    {{{0x01,0x02,0x01},{0x01,0x02,0x01},{0x02,0x01,0x02},{0x82,0x82,0x82},{0x01,0x01,0x03},{0x82,0x82,0x82},{0x01,0x01,0x03},{0x01,0x82,0x00},{0x01,0x82,0x00}}},
    {{{0x02,0x03,0x02},{0x02,0x03,0x02},{0x02,0x02,0x03},{0x82,0x82,0x82},{0x01,0x01,0x01},{0x01,0x01,0x01},{0x01,0x01,0x01},{0x82,0x82,0x00},{0x82,0x82,0x00}}},
    {{{0x01,0x02,0x01},{0x01,0x02,0x01},{0x82,0x82,0x82},{0x01,0x01,0x02},{0x01,0x01,0x02},{0x01,0x01,0x02},{0x01,0x02,0x01},{0x03,0x03,0x00},{0x03,0x03,0x00}}},
    {{{0x82,0x82,0x03},{0x82,0x82,0x03},{0x82,0x03,0x82},{0x82,0x03,0x82},{0x01,0x03,0x01},{0x02,0x01,0x02},{0x01,0x02,0x01},{0x00,0x03,0x03},{0x82,0x82,0x00}}}}};
// The dolphin pose scripts ($91DE, nine bytes per pose): five poses each
// whose hold byte keeps the pose for hold + 1 frames.  $ED9E/$EDCE leap
// (nose up), $EDFE/$EE2E dive (nose down); the second of each pair is used
// for the low leap.  $EE66 is the under-water pose of a moved diver.
enum Level3DolphinScript { kDolphinLeap = 0, kDolphinLeapLow = 1,
                           kDolphinDive = 2, kDolphinDiveLow = 3,
                           kDolphinUnderWater = 4 };
constexpr std::array<std::array<std::uint8_t, 5>, 5> kLevel3DolphinHolds{{
    {0x0a, 0x0b, 0x0b, 0x0a, 0x08},   // $ED9E
    {0x08, 0x0a, 0x0a, 0x08, 0x08},   // $EDCE
    {0x0a, 0x0b, 0x0b, 0x0a, 0x08},   // $EDFE
    {0x08, 0x0a, 0x0a, 0x08, 0x08},   // $EE2E
    {0x0a, 0x0b, 0x0b, 0x0a, 0x08}}}; // $EE66 (shares $EDFE's timing)
// $F9B0: coin shower columns offsets and 8.8 drift speeds (bit 7 = leftward).
constexpr std::array<std::pair<std::uint8_t, std::uint8_t>, 13> kLevel3CoinLanes{{
    {0x00, 0x00}, {0x01, 0x20}, {0xff, 0xe0}, {0x00, 0x00}, {0x01, 0x14},
    {0xff, 0xe4}, {0x00, 0x00}, {0x01, 0x1c}, {0xff, 0xec}, {0x00, 0x00},
    {0x01, 0x18}, {0xff, 0xe8}, {0x00, 0x00}}};
// $8509: forty frames after the fallen pose the board enters the 96-frame
// restart phase (phase 5 to the first movement tick of the re-initialised
// stage), exactly like Level 1.
constexpr int kLevel3FallenFrames = 0x28;
constexpr int kLevel3RestartFrames = 96;
// Events 2, 3 and 4: the miss plays as Charlie lands on the floor and the
// fail piece follows it this many board frames later.
constexpr int kFloorMissFrames = 18;
// Distance plaques are the three $26C0-$26E0 prop cells re-entering at
// $2204 == $80/$70/$60 with the tile words of the next page ($EB5C+).
struct Level3Sign {
  int worldColumn;  // centre of the 48-column plaque
  const char* text;
};
constexpr std::array<Level3Sign, 8> kLevel3Signs{{
    {120, "START"}, {376, "60M"}, {632, "50M"}, {888, "40M"},
    {1144, "30M"}, {1400, "20M"}, {1656, "10M"}, {1912, "GOAL"}}};
constexpr float kStage4BallCenterY = 526.0F;
constexpr float kStage4BallRadius = 40.0F;
constexpr float kStage4CharlieBaselineY = kStage4BallCenterY - kStage4BallRadius;
constexpr float kStage4PlayerScreenX = 118.0F;
constexpr float kStage4GoalScreenX = 396.0F;
constexpr float kStage4CourseLength = 6320.0F;
constexpr float kStage4GoalTopY = kStage4CharlieBaselineY + 18.0F;
// The generated rider atlas has more transparent padding below the shoes
// than the arcade sprite. Lift only the painted rider; physics and landing
// measurements continue to use the ball's true top surface.
constexpr float kStage4CharlieVisualLift = 6.0F;
// The power-on footage (assets/boot/boot-NNNN.png, 30 fps) and the title
// footage between the demos (assets/between/between-NNNN.jpg, 30 fps): the
// counts are read from the files when the game starts.
constexpr float kBootFramesPerSecond = 30.0F;
int gBootFrameCount = 1;
int gBetweenFrameCount = 0;
int bootDurationFrames() {
  return static_cast<int>((static_cast<float>(gBootFrameCount) / kBootFramesPerSecond) *
                              static_cast<float>(kBoardRefresh) +
                          0.5F);
}

float level3RowToY(float row) {
  return row * kLevel3RowScale + kLevel3RowOffset;
}
// The original board places the ring tube directly below the crowd fascia
// (about source y=140), not at the top of the crowd.
constexpr float kTrackY = 350.0F;
// Event 1 movement is a direct 8.8 fixed-point scroll command in the ROM,
// not an acceleration curve.  $7363 writes FE80 (-1.5 source px/frame) for
// RIGHT and 0130 (+1.1875 source px/frame) for LEFT at 60.606 Hz.  Convert
// those cabinet pixels to this renderer's 480/224 horizontal coordinate.
constexpr float kSourceToWorldX = 480.0F / 224.0F;
constexpr float kForwardSpeed =
    1.5F * static_cast<float>(kBoardRefresh) * kSourceToWorldX;
constexpr float kBackSpeed =
    -1.1875F * static_cast<float>(kBoardRefresh) * kSourceToWorldX;
constexpr float kRingRailSpeed = 65.0F;
// Level 1 course stream.  $7633-$7658 forms selector B = $10 + $2208 (normal
// difficulty, wave 1), wraps B >= $68 into the eight-entry loop at $60-$67,
// and reads the reload byte from table base $F7B4.  These are the bytes at
// $F7C4-$F81B (selector $10-$67); the manual full-course capture reads
// entries 0-16 before the goal, the attract trace entries 0-12.
constexpr std::array<std::uint8_t, 88> kLevel1HoopActivationReload{
    0xdc, 0xf4, 0xd4, 0xec, 0xdc, 0xe4, 0xde, 0x1b, 0xee, 0xf6, 0x1b,
    0xde, 0xee, 0x1b, 0xd0, 0xc0, 0xe8, 0xb8, 0xe0, 0xe8, 0xb8, 0xd0,
    0xe8, 0x1e, 0xde, 0x1e, 0xc6, 0x1e, 0xe0, 0x1e, 0xc6, 0x1e, 0xbc,
    0xa4, 0xf4, 0xd4, 0xec, 0x84, 0x8c, 0xbc, 0xbc, 0x21, 0xc0, 0x21,
    0xc4, 0x21, 0xee, 0x21, 0xc0, 0x21, 0xa8, 0x88, 0xc8, 0x78, 0x70,
    0xd8, 0xe0, 0xa8, 0xe2, 0x24, 0xda, 0x7a, 0x24, 0xca, 0x8a, 0x24,
    0x94, 0x64, 0xcc, 0xac, 0xc4, 0x5c, 0x94, 0x7c, 0x7e, 0xc6, 0xae,
    0x27, 0xce, 0x66, 0x84, 0x9c, 0x6c, 0x9c, 0x84, 0x6c, 0x9c, 0x6c};
constexpr std::uint8_t level1CourseByte(std::size_t courseIndex) {
  std::size_t selector = 0x10U + courseIndex;
  if (selector >= 0x68U) selector = (selector & 0x07U) + 0x60U;
  return kLevel1HoopActivationReload[selector - 0x10U];
}
// Fire-pot spacing table read by $7841 (base $F81C).  Wave 1 on normal
// difficulty indexes entry 8 + pot counter; $7839 wraps indices >= $34 into
// $30-$33.  Each value is the 8.8 countdown (in source pixels) until the next
// pot appears at the right edge.
constexpr std::array<std::uint8_t, 52> kLevel1PotSpacing{
    0xf6, 0xfc, 0xf2, 0xfc, 0xee, 0xfc, 0xea, 0xfc, 0xe6, 0xfc, 0xe2,
    0xfc, 0xde, 0xfc, 0xda, 0xfc, 0xd6, 0xfc, 0xd2, 0xfc, 0xce, 0xfc,
    0xca, 0xfc, 0xc6, 0xfc, 0xc2, 0xfc, 0xbe, 0xfc, 0xba, 0xfc, 0xb6,
    0xfc, 0xb2, 0xfc, 0xae, 0xfc, 0xaa, 0xfc, 0xa6, 0xfc, 0xa2, 0xfc,
    0x9e, 0xfc, 0x9a, 0xfc, 0x96, 0xfc, 0x92, 0xfc};
constexpr float kLevel1RiderCollisionScreenX = 98.0F;
// Course progress is kept as the board keeps it: $2203 page, $2204:$2205
// descending page offset, moved by the signed 8.8 command each frame.  Native
// stores the same quantity as one signed value in 1/256 source pixel.
constexpr std::int32_t kLevel1RightCommand = -0x0180;  // $FE80
constexpr std::int32_t kLevel1LeftCommand = 0x0130;    // $0130
constexpr float kLevel1PlayerStartX = 78.0F;
// $7202-$7213: the goal triggers while airborne once $2203 == 7 with
// $2204 < $28 (course progress above 1752 source pixels) and the new rider row
// reaches $C5.  The plaque tiles for page 7 sit at 256 * 7 - 24 = 1768.
constexpr std::int32_t kLevel1GoalProgressFixed = 7 * 0x10000 - 0x2800;
constexpr float kLevel1GoalPlaqueSourceX = 1768.0F;
constexpr float kCourseLength =
    kLevel1RiderCollisionScreenX + kLevel1GoalPlaqueSourceX * kSourceToWorldX;
constexpr float kGoalScreenX = 150.0F;
constexpr float kGoalPlatformTop = kGroundY - 25.0F;
constexpr float kGoalLandingY = kGoalPlatformTop + 12.0F;
constexpr float kPi = 3.14159265358979323846F;
constexpr float kMarqueeHeight = 110.0F;
constexpr float kHudTop = kMarqueeHeight;
constexpr float kHudHeight = 92.0F;
constexpr float kArenaTop = kHudTop + kHudHeight;
constexpr float kCrowdTop = 235.0F;
constexpr float kGrassTop = 350.0F;
// The elephant's door: the arcade draws it at tile column 80 of every
// 256-column page; the HD wall shows it every third page.
constexpr float kElephantDoorColumn = 80.0F;
constexpr float kElephantDoorSpacingColumns = 768.0F;
// Crop the reusable arena painting to crowd + grandstand fascia + grass.
// The source's upper tents duplicate the fixed marquee and its lower front
// curtain does not exist in the Stage 1 cabinet playfield.
constexpr float kArenaContentSourceTop = 0.485F;
constexpr float kArenaGrassSourceTop = 0.738F;
// Stop inside the grass. The source's gold curtain finials begin immediately
// below this point and used to leak into the bottom edge of the playfield as
// unexplained yellow semicircles.
constexpr float kArenaContentSourceBottom = 0.850F;
constexpr float kBigHoopOpeningTop = kGroundY - 223.0F;
constexpr float kBigHoopOpeningBottom = kGroundY - 86.0F;
constexpr float kSourceToLogicalY =
    static_cast<float>(kWorldHeight) / 256.0F;
// Clean-room measurement of the original Event 1 rider sprite's vertical
// displacement. One entry is consumed per 60.606 Hz board update. The arcade
// jump is fixed rather than button-duration dependent.
constexpr std::array<std::uint8_t, 64> kJumpSourceDisplacement{
    0,  4,  7,  10, 13, 16, 19, 22, 25, 27, 29, 32, 34, 36, 38, 40,
    42, 43, 45, 46, 48, 49, 50, 51, 52, 52, 53, 54, 54, 54, 55, 55,
    55, 55, 54, 54, 54, 53, 52, 52, 51, 50, 49, 48, 46, 45, 43, 42,
    40, 38, 36, 34, 32, 29, 27, 25, 22, 19, 16, 13, 10, 7,  4,  0,
};
// Event 2 uses its own shorter fixed arc. These 58 board samples come from
// the decoded player composite at object slots $2400-$2430 during the
// original circusc attract run (frames 3403-3460): 52 source pixels high.
constexpr std::array<std::uint8_t, 58> kStage2JumpSourceDisplacement{
    0,  4,  7,  10, 13, 16, 19, 22, 25, 28, 30, 32, 34, 36, 38,
    40, 42, 44, 45, 46, 47, 48, 49, 50, 51, 52, 52, 52, 52, 52,
    52, 52, 52, 51, 50, 49, 48, 47, 46, 45, 44, 42, 40, 38, 36,
    34, 32, 30, 28, 25, 22, 19, 16, 13, 10, 7,  4,  0,
};
constexpr float kLionCollisionCenterOffset = 20.0F;
// The HD hoop source has about 50% transparent horizontal padding. A
// 108-unit destination produces the measured 46-unit visible MAME width.
constexpr float kBigRingVisualHalfWidth = 54.0F;
// circusc4 $7130-$7192 does not test the large hoop's rendered extents. It
// compares one fixed rider X against the object's source X high byte, then
// combines that distance with a rider-composite Y offset. Keep these values
// in original 224x256 board coordinates; converting the HD artwork bounds
// back into collision geometry caused the former three-frame-early failure.
constexpr int kLevel1RiderCollisionSourceX = 0x40;
constexpr int kLevel1RiderGroundSourceY = 0xd0;
constexpr int kLevel1RiderCollisionBaseY = 0xb6;
constexpr int kLevel1HoopHorizontalLimit = 0x0e;
constexpr int kLevel1HoopCombinedLimit = 0x1c;
constexpr int kLevel1FourthHoopYOffset = 0x10;
// Measured from hoop-extra.avi frame 800 after correcting the original
// 224x256 board image to the game's 480x640 logical display: the prize hoop
// has a visible flame oval about 50x110, centred 170 units above the grass
// contact line.
// The source cell has generous transparent padding on both axes. A 112x198
// target produces the measured 53x118 visible flame oval; using the visible
// measurements as the destination size makes the rendered hoop much smaller.
constexpr float kBonusRingVisualHalfWidth = 56.0F;
constexpr float kBonusRingVisualHalfHeight = 99.0F;
constexpr float kBonusRingCenterHeight = 170.0F;
// <$CB: the burning composite is shown for 64 board frames ($7CA9/$7CC1);
// the board hands control back 160 frames after the collision.
constexpr int kCrashBurnFrames = 64;
constexpr int kLevel1FailureFrames = 160;
constexpr int kGoalArrivalFrames = 90;
constexpr int kBirdArrivalFrames = 170;
constexpr int kBagDropFrames = 45;
constexpr int kCoinShowerFrames = 220;
constexpr int kRewardCoinCount = 18;
constexpr int kDefaultHighScore = 19830;
constexpr int kFirstScoreLife = 20000;
constexpr int kRecurringScoreLife = 70000;
constexpr int kEventCount = 6;
constexpr int kEventPlayLimit = 5;    // plays per event once the screen is open
constexpr int kTrapezeEvent = 5;
constexpr int kEventColumns = 3;

struct Options {
  int width = 480;
  int height = 640;
  // The portrait resolution the frame is rendered at before it is scaled to
  // the display: by default the game's own 480x640, so no monitor shows more
  // than the logical playfield; 240x320 reproduces a low-resolution cabinet
  // (low-detail drawing and nearest-neighbour scaling switch on at 320
  // lines); 0x0 renders at the display's full resolution.
  int renderWidth = 480;
  int renderHeight = 640;
  int rotation = 0;
  bool fullscreen = false;
  bool debug = false;
  bool lionTest = false;
  bool showHelp = false;
  std::string capturePath;
  // --record DIR: write the playfield of every Nth board frame as a JPEG
  // frame of an attract clip, from the first playing frame, for
  // recordSeconds, then quit.
  std::string recordDir;
  int recordSeconds = 60;   // the cap; the clip normally ends with the fall
  int recordFps = 20;
  std::string captureScene = "gameplay";
  std::string tracePath;
  std::string traceMode = "hold-right";
  std::string riderDiagnosticDir;
  std::vector<int> riderDiagnosticFrames;
  // Deterministic replay of a MAME capture-state.csv input column.
  std::string replayPath;
  int startEvent = 0;   // 1-6: skip the boot video and go straight in
  std::string replayOutput;
  std::string replayCaptureDir;
  std::vector<int> replayCaptureFrames;
  int replayStart = -1;
  int replayOffset = 0;
  int replayCourseOffset = -1;
  int replayCoinSelector = -1;
  int replayFrameByte = -1;
  // Level 3 replays read tools/autoplay_level3_headless.lua captures.
  int replayEvent = 0;
  bool replayInvulnerable = false;
  bool replayPerfect = false;
  bool replayWater = false;     // Event 3: the capture was made with $2207 set
  bool startWater = false;      // --event 3 --water   // Event 2: neutralise the $869B store
  int replayVisits = 1;         // Event 5: the $2212 visit count of the capture
  bool replayClearProjectiles = false;
};

struct Vec2 {
  float x = 0.0F;
  float y = 0.0F;
};

// circusc4 uses three ordinary reusable hoop records ($26d0-$2730) and a
// reserved fourth record ($2760) for the course-selected small/prize ring.
// $7684 may instead convert a newly allocated ordinary record into the
// one-per-stage hanging Charlie.  These are logical object kinds; the sprite
// staging cells written by $75eb-$7606 are merely components of a large hoop.
enum class Level1HoopKind : std::uint8_t {
  Large,
  PrizeRing,
  ExtraCharlie,
};

struct Hoop {
  float worldX = 0.0F;
  float openingBottom = kGroundY - 20.0F;
  float openingTop = kGroundY - 154.0F;
  bool cleared = false;
  float previousWorldX = 0.0F;
  // circusc4 stores each active hoop's horizontal position as an 8.8 value
  // at object-slot offsets +$06/+$07. It is independent of course world X.
  std::uint16_t sourceXFixed = 0;
  bool active = false;
  Level1HoopKind kind = Level1HoopKind::Large;
  // Record byte +$09: $7394 marks every object with X in [$40,$BF] at
  // takeoff; $72D9 counts those that moved below $40 by the landing.
  bool takeoffCandidate = false;
};

// circusc4 fire-pot records $24B0/$24F0/$2530 ($7750-$78F6).
struct FirePot {
  // +$00: 0 free, 1 pending (counting down), 2 visible.
  std::uint8_t status = 0;
  // +$01:+$02: 8.8 course countdown while pending.
  std::uint16_t countdown = 0;
  // +$06:+$07: 8.8 screen X while visible (spawns at $FE80 from 0).
  std::uint16_t sourceXFixed = 0;
  // +$08 flame timer and +$0F attribute (3..5) chosen from the frame byte.
  std::uint8_t animationTimer = 0;
  std::uint8_t flameVariant = 3;
  float worldX = 0.0F;
  bool visible() const { return status == 2; }
};

struct BonusRing {
  float worldX = 0.0F;
  float height = 0.0F;
  bool collected = false;
  bool containsPrize = false;
  // Visual state for the reserved $2760 small/prize-ring object.  Ordinary
  // large hoops do not own or activate one of these records.
  std::uint16_t sourceXFixed = 0;
  bool active = false;
};

struct MeterMarker {
  float worldX = 0.0F;
  int meters = 0;
};

enum class Stage2MonkeyKind {
  Brown,
  Purple,
};

struct Stage2Monkey {
  float worldX = 0.0F;
  Stage2MonkeyKind kind = Stage2MonkeyKind::Brown;
  bool cleared = false;
  bool leaping = false;
  int leapFrame = 0;
};

// ---- circusc4 Level 3 records ---------------------------------------------
// Performer records $2440/$2480/$24C0/$2500 ($93F8 spawn, $95B7 state 0).
struct Level3Performer {
  bool active = false;
  std::uint8_t x = 0;         // $6: column of the 32-column composite
  int type = 0;               // $3C: 0 fire breather, 1/2 juggler knives
  int remaining = 0;          // $38: knives still to be thrown
  std::uint8_t timer = 0;     // $39
  std::uint8_t timerReload = 0;  // $37
  int pose = 0;               // 0 idle, 1 breathing, 2 throwing, 3 catching
  int poseFrame = 0;
  // The water round ($2207) fills the same records with dolphins ($969C):
  int state = 0;              // $2: 0 performer, 3 dolphin
  std::uint8_t y = 0;         // $4: row of the lower cells
  int phase = 0;              // $7: 1 leaping, 2 falling, 3 diving, 4 surfacing
  std::uint16_t velocity = 0; // $17:$18 (8.8)
  std::uint16_t launch = 0;   // $8:$3A: the velocity a dive or a leap starts with
  int counter = 0;            // $23: counts the dives down to the low leap
  bool offset = false;        // $25: spawned under water, survives one wrap
  std::uint8_t key = 0;       // $3B: $2204 at the spawn
  int script = 0;             // which of the five-pose scripts animates it
  int scriptIndex = 0;        // pose within the script
  std::uint8_t scriptHold = 0;  // $B
};

// Flame records $2580/$25A0/$25C0 ($9273): two cells, bottom cell at y.
// The fourth record ($25E0) is only ever used by the dolphins' splashes
// (state 3, $9799), which the fire breathers' spawn loop never reaches.
struct Level3Flame {
  bool active = false;
  int state = 0;              // 0 at the mouth, 1 rising, 3 splash, 4 frozen by a hit
  std::uint8_t x = 0;
  std::uint8_t y = 0;
  std::uint16_t velocity = 0; // $7:$8
  bool apex = false;          // $9
  std::uint8_t hold = 0;      // $3
  std::uint8_t lifetime = 0;  // $11: the splash's 32 frames
  bool diving = false;        // $EE62 (a dolphin going in) or $EE5E (coming out)
  int age = 0;
};

// Knife records $2540-$2570 ($9343): one cell, thrown up and caught again.
struct Level3Knife {
  bool active = false;
  int state = 0;              // 0 rising, 1 falling, 2 held, 4 frozen
  std::uint8_t x = 0;
  std::uint8_t y = 0;
  std::uint16_t velocity = 0;
  bool apex = false;
  std::uint8_t sway = 0;      // $A: drifts one column right every 8 frames
  std::uint8_t hold = 0;      // $3
  int owner = -1;             // $1: performer record
  int age = 0;
};

// Bag records $2690/$26A0/$26B0 ($7DAE state 3, $7DC9 state 4 popup, and the
// state-6 coin piles of the perfect-clear presentation).
struct Level3Bag {
  bool active = false;
  int state = 0;
  std::uint8_t x = 0;
  std::uint8_t y = 0;
  std::uint8_t key = 0;       // $9: $2204 at the spawn
  std::uint8_t timer = 0;     // $A
  int value = 0;
  int age = 0;
};

// Coin records $2730-$27FF ($7E7B).  Slot 8 ($27B0) receives the copy of the
// bird's bag record and stays occupied for the rest of the shower.
struct Level3Coin {
  bool active = false;
  bool bagCopy = false;
  std::uint8_t x = 0;
  std::uint8_t y = 0;
  std::uint8_t xFraction = 0;
  std::uint8_t yFraction = 0;
  std::uint8_t xSpeed = 0;
  bool xNegative = false;
  int yVelocity = 0;
  int age = 0;
};

enum class Level3Pose : std::uint8_t {
  Stationary,   // $EC4E
  MovingRight,  // $EC93
  MovingLeft,   // $EC6C
  Cheer,        // $EAAA
  Roof,         // $ECBA
  Fallen,       // $EA9E
};

// ---- circusc4 Event 4 records ---------------------------------------------

// One board record.  The field names follow the offsets the ROM uses so the
// routines below read like the disassembly.
struct Level4Record {
  int active = 0;         // ,X
  int flag = 0;           // $1,X
  int state = 0;          // $2,X
  int row = 0;            // $4,X
  int half = 0;           // $5,X   half-column accumulator
  int col = 0;            // $6,X
  int phase = 0;          // $7,X   0 riding, 1 rising, 2 falling
  int landVelocity = 0;   // $8,X
  int launch = 0;         // $9,X
  int direction = 0;      // $A,X   0 still, 1 left, 2 right
  int animation = 0;      // $B,X
  int velocity = 0;       // $17,X:$18,X, 8.8
  int jump = 0;           // $1A,X
  int stick = 0;          // $1B,X
  int idleHigh = 0;       // $27,X
  int idleLow = 0;        // $28,X
  int script = 0;         // $C,X  which animation script
  int pose = -1;          // its current step
  int align = 0;          // $29,X  (the ridden ball only)
  int countdown = 0;      // $2D,X
  // Event 2 monkeys reuse this record: $30 marks the purple monkeys, $31:$32
  // is the brown record a leaping purple has frozen, and $1D is the short
  // timer that plays the turned pose after a leap lands.
  int kind = 0;           // $30,X  0 brown, 1 purple
  int owner = -1;         // $31:$32,X  the record a purple is leaping over
  int leapTimer = 0;      // $1D,X
};

// $2600-$26B0.  State 0 is a score popup that tracks a ball; state 2 is one
// cell of the FAR OUT / GREAT / OH NO callouts.
struct Level4Popup {
  int active = 0;
  int flag = 0;
  int state = 0;
  int row = 0;
  int col = 0;
  int timer = 0;
  int owner = -1;   // -1 the ridden ball, 0-3 a rolling ball
  int value = 0;    // points, for the drawing code
  int cell = 0;
  int kind = 0;     // 0 score popup, 1 goal callout, 2 failure callout
};

struct Level4Plaque {
  int col = 0;
  int row = 0;
  int trigger = 0;
  int cell = 0;
};

// ---- Event 4 board constants ----------------------------------------------

// The board's 224x256 screen maps onto the native canvas exactly as it does
// in Event 3.
constexpr float kLevel4BallRadius = 40.0F;
constexpr int kLevel4ScrollEnd = 1793;   // progress once page $F8 stops it

// $9912/$9883: the ball starts at row $C0 column $50 and Charlie sits on it.
constexpr int kLevel4BallRow = 0xc0;
constexpr int kLevel4StartColumn = 0x50;
constexpr int kLevel4CharlieRow = 0xa4;
constexpr int kLevel4RestartFrames = 96;
constexpr int kLevel5RestartFrames = 96;
constexpr int kLevel6RestartFrames = 96;
// $FB80 on a fresh start, then $BD1C by page.
constexpr int kLevel4FirstBonus = 6410;
constexpr int kLevel4RestartBonus[3] = {4000, 3500, 3000};

// The animation scripts $87CE walks: one HD cell and how many frames it is
// held.  Both directions share them -- the board never mirrors Charlie on
// this stage, it reverses the ball's spin instead -- and a jump holds a
// single pose for its whole 57 frames ($E9CF's delay is 240).
struct Level4Pose {
  int frame;
  int delay;
};
constexpr std::array<Level4Pose, 4> kLevel4Standing{   // $E9FF
    {{0, 16}, {4, 16}, {1, 16}, {0, 16}}};
constexpr std::array<Level4Pose, 4> kLevel4Walking{    // $E9B4
    {{0, 6}, {1, 6}, {2, 6}, {1, 6}}};
constexpr std::array<Level4Pose, 1> kLevel4Jumping{{{5, 240}}};          // $E9CF
constexpr std::array<Level4Pose, 6> kLevel4GoalJump{   // $E9D8
    {{5, 8}, {6, 8}, {5, 8}, {6, 8}, {6, 8}, {4, 80}}};
constexpr std::array<Level4Pose, 3> kLevel4Struck{     // $EA1A
    {{0, 3}, {4, 3}, {1, 3}}};
constexpr std::array<Level4Pose, 1> kLevel4Fallen{{{10, 8}}};            // $EA9E
constexpr std::array<Level4Pose, 2> kLevel4Cheer{{{4, 16}, {0, 16}}};    // $EAAA

enum Level4Script {
  kLevel4ScriptStanding = 0,
  kLevel4ScriptWalking,
  kLevel4ScriptJumping,
  kLevel4ScriptGoalJump,
  kLevel4ScriptStruck,
  kLevel4ScriptFallen,
  kLevel4ScriptCheer,
};

inline const Level4Pose* level4Script(int script, int& length) {
  switch (script) {
    case kLevel4ScriptWalking: length = 4; return kLevel4Walking.data();
    case kLevel4ScriptJumping: length = 1; return kLevel4Jumping.data();
    case kLevel4ScriptGoalJump: length = 6; return kLevel4GoalJump.data();
    case kLevel4ScriptStruck: length = 3; return kLevel4Struck.data();
    case kLevel4ScriptFallen: length = 1; return kLevel4Fallen.data();
    case kLevel4ScriptCheer: length = 2; return kLevel4Cheer.data();
    default: length = 4; return kLevel4Standing.data();
  }
}

// $EAFE and $EB23: the cells of the two callouts, as (row, column) pairs.
constexpr std::array<std::pair<int, int>, 9> kLevel4GoalCells{{
    {88, 32}, {88, 48}, {88, 64}, {104, 108}, {104, 124},
    {104, 140}, {88, 176}, {88, 192}, {88, 208}}};
constexpr std::array<std::pair<int, int>, 6> kLevel4FailCells{{
    {88, 64}, {88, 80}, {88, 96}, {112, 160}, {112, 176}, {112, 192}}};

// $EB48: the distance plaques, three per page.  Only the trigger column and
// the sign they show change; the row is always $E0.
constexpr std::array<int, 3> kLevel4PlaqueTriggers{0x80, 0x70, 0x60};
constexpr std::array<std::array<int, 3>, 10> kLevel4PlaqueCells{{
    {0xb8, 0xb9, 0xba}, {0xb8, 0xb9, 0xba}, {0xca, 0xc5, 0xbc},
    {0xca, 0xc1, 0xbc}, {0xca, 0xc0, 0xbc}, {0xca, 0xbf, 0xbc},
    {0xca, 0xbe, 0xbc}, {0xca, 0xbd, 0xbc}, {0xc2, 0xc3, 0xc4},
    {0xc2, 0xc3, 0xc4}}};

// The $F1B5 rolling-ball schedule: six difficulty tables of nine pages,
// each naming up to five $2204 columns at which a ball rolls in.
constexpr std::array<std::array<int, 5>, 22> kLevel4SpawnLists{{
    {224, 184, 64, 32, 0},   // $F1E9
    {248, 216, 152, 120, 80},   // $F1EE
    {228, 128, 8, 0, 0},   // $F1F4
    {192, 120, 32, 0, 0},   // $F1F8
    {212, 0, 0, 0, 0},   // $F1D5
    {224, 184, 64, 32, 1},   // $F214
    {192, 160, 128, 96, 64},   // $F1FC
    {0, 242, 20, 242, 20},   // $F201
    {224, 184, 64, 32, 1},   // $F22B
    {128, 89, 8, 192, 0},   // $F230
    {128, 89, 8, 224, 160},   // $F247
    {228, 128, 8, 64, 192},   // $F24C
    {192, 120, 32, 72, 0},   // $F251
    {192, 160, 128, 96, 32},   // $F256
    {224, 184, 144, 64, 16},   // $F26D
    {208, 176, 128, 89, 24},   // $F272
    {228, 170, 128, 72, 8},   // $F277
    {192, 120, 80, 32, 0},   // $F27C
    {224, 176, 128, 80, 32},   // $F281
    {208, 160, 120, 72, 16},   // $F286
    {204, 160, 124, 88, 48},   // $F28B
    {200, 164, 112, 64, 0},   // $F290
}};
constexpr std::array<std::array<int, 9>, 6> kLevel4SchedulePages{{
    {0, 0, 1, 2, 3, 3, 3, 3, 4},   // table 0 ($F1C3)
    {0, 0, 1, 2, 3, 5, 3, 6, 7},   // table 1 ($F1D7)
    {5, 5, 1, 2, 2, 1, 3, 6, 7},   // table 2 ($F202)
    {8, 8, 9, 8, 2, 1, 6, 6, 7},   // table 3 ($F219)
    {8, 10, 10, 11, 11, 12, 13, 6, 7},   // table 4 ($F235)
    {14, 14, 15, 16, 17, 18, 19, 20, 21},   // table 5 ($F25B)
}};

struct Stage4Ball {
  float worldX = 0.0F;
  float velocity = 0.0F;
  float rotation = 0.0F;
  bool active = true;
  int collisionCooldown = 0;
};

struct Player {
  Vec2 position{78.0F, kGroundY};
  Vec2 previous = position;
  float verticalVelocity = 0.0F;
  float runSpeed = 0.0F;
  int jumpFrame = -1;
  // circusc4 $7344/$736c copies the takeoff direction command through
  // $2243:$2244 and reuses it until the landing transition.  -1/0/+1 are
  // left, neutral, and right respectively.
  int level1AirborneDirection = 0;
  bool level1JumpPending = false;
  // $7235-$7254: during the last four source pixels of the descent the board
  // re-samples the joystick into $2243 and accepts a fresh jump press into
  // $2246, so a buffered jump starts on the landing tick.
  bool level1JumpBuffered = false;
  int level1BufferedDirection = 0;
  bool grounded = true;
  bool alive = true;
  bool facingRight = true;
};

enum class Scene {
  Boot,
  Title,
  Attract,   // the demo clips the cabinet shows while no coin is in
  GameOver,  // the card after the last life, before the title
  EventSelect,
  Playing,
  Crashed,
  Goal,
  Tally,
  Complete,
};

enum class Level1RiderState : std::uint8_t {
  RunA = 0,
  RunB = 1,
  RunC = 2,
  // $73F4 selects the second six-byte table at $EEA6 while the rider walks
  // backward: pose D shares A's cells, E and F keep C's and B's upper rows
  // over distinct walking leg cells ($91/$92/$93 and $8F/$94/$D4).
  BackD = 3,
  BackE = 4,
  BackF = 5,
};

// All accepted production poses use the same 1024x768 canvas and the same
// authoritative gameplay anchor. Airborne selects Run C without a distinct
// image or anchor; only the ROM-derived jump trajectory moves it.
constexpr std::array<float, 2> kLevel1RiderProductionAnchor{512.0F, 640.0F};

constexpr const char* level1RiderStateName(Level1RiderState state) {
  switch (state) {
    case Level1RiderState::RunA:
      return "A";
    case Level1RiderState::RunB:
      return "B";
    case Level1RiderState::RunC:
      return "C";
    case Level1RiderState::BackD:
      return "D";
    case Level1RiderState::BackE:
      return "E";
    case Level1RiderState::BackF:
      return "F";
  }
  return "?";
}

constexpr int level1RiderProductionAsset(Level1RiderState state) {
  switch (state) {
    case Level1RiderState::RunA:
    case Level1RiderState::BackD:
      return 1;
    case Level1RiderState::RunB:
      return 2;
    case Level1RiderState::RunC:
      return 3;
    case Level1RiderState::BackE:
      return 4;
    case Level1RiderState::BackF:
      return 5;
  }
  return 1;
}

constexpr std::array<int, 6> level1RiderCodes(Level1RiderState state) {
  switch (state) {
    case Level1RiderState::RunA:
    case Level1RiderState::BackD:
      return {0x62, 0x61, 0x60, 0x5f, 0x5e, 0x5d};
    case Level1RiderState::RunB:
      return {0xcd, 0xcc, 0xb6, 0xb5, 0x64, 0x63};
    case Level1RiderState::RunC:
      return {0x5c, 0x5b, 0x5a, 0x59, 0x58, 0x57};
    case Level1RiderState::BackE:
      return {0x93, 0x92, 0x91, 0x59, 0x58, 0x57};
    case Level1RiderState::BackF:
      return {0xd4, 0x94, 0x8f, 0xb5, 0x64, 0x63};
  }
  return {};
}

#include "level5_types.inc"
#include "level6_types.inc"

struct Game {
  Scene scene = Scene::Boot;
  Player player;
  std::vector<Hoop> hoops;
  std::vector<FirePot> firePots;
  std::vector<BonusRing> bonusRings;
  std::vector<MeterMarker> meterMarkers;
  std::vector<Stage2Monkey> stage2Monkeys;
  std::vector<Stage4Ball> stage4Balls;
  float cameraX = 0.0F;
  float previousCameraX = 0.0F;
  int score = 0;
  int highScore = kDefaultHighScore;
  int credits = 0;
  int lives = 3;
  int selectedEvent = 0;
  int bootFrame = 0;
  int eventSelectFrame = 0;
  // Attract mode: frames idle on the title with no credit, then the clip
  // playing, its frame and the tick within it.
  int titleIdleFrames = 0;
  // The cabinet's run: a coin starts Event 1 and a clear moves on to the
  // next; clearing the trapeze unlocks the event screen.  Each event may
  // then be picked five times (its visit count sets the difficulty, as the
  // board's $220F-$2213 do) before it locks; the trapeze never locks.
  bool arcadeRun = true;
  bool selectUnlocked = false;
  std::array<int, 6> playCount{};
  int completeFrame = 0;
  bool selectContinuing = false;
  int gameOverFrame = 0;
  int attractClip = 0;        // the stage clip the next demo plays
  int attractTick = 0;
  int attractFrameCount = 0;
  bool attractBetween = true; // playing the title footage, not a clip
  std::uint32_t betweenAudioSerial = 0;
  int eventSelectDurationFrames =
      static_cast<int>(17.0 * kBoardRefresh);
  int bonus = 6000;
  int nextScoreLife = kFirstScoreLife;
  int goalFrame = 0;
  int tallyFrame = 0;
  int crashFrame = 0;
  int crashDurationFrames = kCrashBurnFrames;
  int clearBonus = 0;
  int rewardCoinsAwarded = 0;
  int prizeBagsAvailable = 0;
  int prizeBagsCollected = 0;
  // $25E1 is the persistent Level 1 small-ring prize selector.  $7157-$718A
  // awards (state + 1) * 1000 and saturates at state four (5000 points).
  int level1PrizeState = 0;
  bool deathOccurred = false;
  bool perfectClear = false;
  bool hiddenCoinTriggered = false;
  bool timeScoreApplied = false;
  int openingBackwardJumps = 0;
  // $220A: 0 available, 1 pending, 2 converted record exists, 3 collected.
  // extraCharlieHoopIndex mirrors <$BF (the tracked record, -1 when none).
  int level1ExtraCharlieState = 0;
  int extraCharlieHoopIndex = -1;
  // Derived for rendering: the doll is visible while state 2 tracks a record.
  bool extraCharlieActive = false;
  // ---- circusc4 Level 1 board state ----
  // $2203:$2204:$2205 as one signed value in 1/256 source pixel.  RIGHT adds
  // $0180 per frame, LEFT subtracts $0130 unless the page byte is zero.
  std::int32_t level1ProgressFixed = 0;
  // Fire-pot records $24B0/$24F0/$2530 and the pointers kept at <$C6/<$C8
  // for the two fixed pots of pages six and seven ($7799-$77FD).
  std::array<FirePot, 3> level1Pots{};
  int level1FixedPotC6 = 2;
  int level1FixedPotC8 = 1;
  // $2209 counts chain pots; <$C1 (frame byte & 3, three becomes zero)
  // selects which of them hides the coin ($780D-$7816).
  std::uint8_t level1PotCounter = 0;
  int level1CoinPotSelector = 0;
  // $2580 pointer, $220B state (0 idle, 1 launched, 2 caught), $2582 arm
  // flag, $2584:$2585 8.8 row, $2587:$2588 8.8 velocity, $2589 spin.
  int level1CoinPot = -1;
  int level1CoinState = 0;
  std::uint8_t level1CoinArmed = 0;
  std::uint8_t level1CoinX = 0;
  std::int32_t level1CoinYFixed = 0xd200;
  std::int32_t level1CoinVelocityFixed = 0;
  int level1CoinSpin = 0;
  int level1CoinPopupTimer = 0;
  // <$C4:$C5 distance since the last left-edge retirement and <$BD reserved
  // retirement flag, used by $76FE to bring an object back from the left.
  std::uint16_t level1RetireDistance = 0;
  bool level1ReservedRetired = false;
  // $25E0: $FF while the small ring carries its bag, then a popup countdown.
  std::uint8_t level1BagState = 0;
  // $2781: pot whose X is watched between takeoff and landing ($73AA/$72F7).
  int level1PotMarker = -1;
  int level1PotPopupTimer = 0;
  float level1PotPopupWorldX = 0.0F;
  // $220C: bags missed at ring retirement plus deaths.  Zero at the goal
  // earns the bird and coin shower.
  int level1MissedRewards = 0;
  // <$BE counts fire-pot-only landings past page seven ($731D).
  int level1LatePotLandings = 0;
  // Goal presentation: $2500 phase count, <$CA counter, bird/bag/coins.
  int level1GoalCounter = 0;
  int level1GoalPhases = 0;
  int level1BirdPhase = 0;
  std::uint8_t level1BirdX = 0;
  // Bumped when the bag tears open at column $31, which is when the arcade
  // starts the coin-shower piece; the tally then waits for the piece to end.
  std::uint32_t level1ShowerAudioSerial = 0;
  int level1ShowerStartFrame = 0;
  int level1ShowerHoldFrames = 0;
  struct RewardCoin {
    int timer = 0;
    std::int32_t yFixed = 0;
    std::int32_t velocityFixed = 0;
    std::int32_t xFixed = 0;
    std::int32_t driftFixed = 0;
    int spin = 0;
    bool active = false;
  };
  std::array<RewardCoin, 11> level1RewardCoins{};
  int level1RewardCoinLaunches = 0;
  // Rider pose table select (0 forward $EE94, 1 backward $EEA6).
  bool level1RiderBackward = false;
  int level1LandingDirection = 0;
  // Replay overrides for deterministic comparison against MAME captures.
  int replayCourseOffsetOverride = -1;
  int replayCoinSelectorOverride = -1;
  int replayInitialInput = 0xff;
  int stage2JumpClears = 0;
  bool stage2JumpBrown = false;
  bool stage2JumpPurple = false;
  int stage2ScorePopup = 0;
  int stage2ScorePopupFrame = 0;
  float stage2ScorePopupWorldX = 0.0F;
  int stage1ScorePopup = 0;
  int stage1ScorePopupFrame = 0;
  float stage1ScorePopupWorldX = 0.0F;
  float stage1ScorePopupY = 0.0F;
  int level1PendingHoopScore = 0;
  int level1HoopScoreAwarded = 0;
  float level1PendingHoopScoreWorldX = 0.0F;
  float level1PendingHoopScoreY = 0.0F;
  // ---- circusc4 Level 3 board state ($8A6C) ----
  std::uint16_t level3Scroll = 0;         // $2203:$2204 (progress = -scroll)
  int level3State = 1;                    // $2402: 1 play, 4 goal, 7 hit, 8 fallen
  std::uint8_t level3Y = kLevel3StandRow; // $2404: row of the lower cells
  std::uint8_t level3X = kLevel3PlayerColumn;  // $2406
  int level3Phase = 1;                    // $2407: 1 rising, 2 falling
  std::uint16_t level3Velocity = 0x0420;  // $2417:$2418 (8.8)
  std::uint16_t level3Target = 0x0420;    // $2408:$243A landing velocity
  int level3Bounce = 0;                   // $2437 stationary rebound count
  int level3Direction = 0;                // $240A: 0 none, 1 left, 2 right
  int level3Stick = 0;                    // $241B
  int level3BothStick = 0;   // which way a two-key overlap resolves
  int level3Countdown = 0;                // $242D
  Level3Pose level3Pose = Level3Pose::Stationary;
  int level3PoseFrame = 0;
  std::array<Level3Performer, 4> level3Performers{};
  std::array<Level3Flame, 4> level3Flames{};   // the fourth only for splashes
  std::array<Level3Knife, 4> level3Knives{};
  std::array<Level3Bag, 3> level3Bags{};
  int level3BagValueIndex = 0;            // $28F1 (300 + 100 * index, max 900)
  int level3BagsTotal = 0;                // $28F2
  int level3Missed = 0;                   // $220A: bags scrolled off (+ shower end)
  int level3TileTimer = 0;                // $28F4 pressed-drum frames
  int level3PressedDrum = -1;
  int level3DeathKind = 0;                // 1 flame, 2 knife, 3 roof, 4 time, 5 dolphin
  int level3RestartPage = 0;              // $2203 after $8517
  Level4Record level4Charlie;             // $2400
  Level4Record level4Ball;                // $2440 the ball he rides
  std::array<Level4Record, 4> level4Rolls;   // $2480-$2540
  std::array<Level4Popup, 12> level4Popups;  // $2600-$26B0
  std::array<Level4Plaque, 3> level4Plaques; // $26C0-$26E0
  int level4Scroll = 0;                   // $2203:$2204
  int level4Timeout = 0;                  // $2263
  int level4BonusHold = 0;                // $2276
  int level4Buffered = 0;                 // $28D3
  int level4Doubles = 0;                  // $28D4
  int level4Pending = 0;                  // $28D5
  int level4PendingLead = -1;             // $28D6
  int level4PendingTrail = -1;            // $28D8
  int level4Falls = 0;                    // $28DF
  int level4Landings = 0;                 // $28EF
  int level4Held = 0;                     // $28F0
  int level4Standing = 0;                 // $28FC
  int level4Flash = 0;                    // $2888
  int level4Dips = 0xb4;                  // $202F
  int level4Visits = 1;                   // $2211
  bool level4Started = false;             // $2258
  int level4RestartPage = 0;
  int level4LatchedButton = 0;
  int level4BothStick = 0;   // which way a two-key overlap resolves
  Level5 level5;             // ---- Event 5 (pony and springboards) board ----
  Level6 level6;             // ---- Event 6 (the trapeze) board ----
  int level6BonusHold = 0;   // $2276 on the trapeze
  bool level4Died = false;
  bool level4Finished = false;
  // ---- Event 2 (tightrope and monkeys) board ----
  Level4Record level2Charlie;                // $2400
  std::array<Level4Record, 7> level2Monkeys; // $2440-$2500 brown, $2540-$25C0 purple
  Level4Record level2Stand;                  // $2760 the goal stand
  std::array<Level4Popup, 12> level2Popups;  // $2600-$26B0
  std::array<Level4Plaque, 3> level2Plaques; // $26C0-$26E0
  int level2Scroll = 0;                      // $2203:$2204
  int level2HalfStep = 0;                    // $2205
  int level2Difficulty = 0;                  // $81A6 table, fixed at stage start
  int level2SchedIndex = 0;                  // walks the $F2C0 schedule
  int level2SchedCounter = 0;                // $288B
  int level2Skip = 0;                        // $28F8 spawn spacing
  int level2Buffered = 0;                    // $28D3
  int level2Held = 0;                        // $28F0
  int level2Timeout = 0;                     // $2263
  int level2Flash = 0;                       // $2888
  int level2Landings = 0;                    // $28EF
  int level2StandHit = 0;                    // $28DF walked onto the stand
  int level2StandGoal = 0;                   // $28EE landed in the goal zone
  int level2Clear = 0;                       // $220A (0 keeps the perfect clear)
  int level2Near = 0;                        // $28F9 browns jumped
  int level2Far = 0;                         // $28FA purples jumped
  int level2NearFirst = -1;
  int level2FarFirst = -1;
  int level2Dips = 0xb4;                     // $202F
  int level2Visits = 1;                      // $220F
  int level2WalkAge = 0;      // HD animation: frames spent walking
  int level2JumpAge = 0;      // HD animation: frames spent in a jump
  int level4JumpAge = 0;      // the same for the balls
  float charlieScale = 1.0F;  // [ and ] tune the painted Charlie's size; kept in charlie-scale.txt
  int charlieScaleNotice = 0; // frames left showing the size on screen
  int level4WalkAge = 0;
  int level2MonkeyAge = 0;    // HD animation: frames the monkeys have walked
  bool level2Started = false;
  int level2RestartPage = 0;
  int level2BothStick = 0;
  bool level2Died = false;
  bool level2Finished = false;
  bool level2CrashCued = false;   // the miss already played at the hit
  // $2700/$2720: the perfect-clear bird and the bag it carries.  On this
  // stage the capture shows it simply crossing from column $E8 to 0 at one
  // column a frame, its cell changing at $A0 and $80, the bag leaving at $38;
  // the celebration holds at one frame left until it has gone.
  bool level2BirdActive = false;
  int level2BirdState = 0;
  int level2BirdX = 0;
  int level2BirdBagX = 0;
  bool level2Perfect = false;     // --replay-perfect: $869B is neutralised
  bool level3Died = false;                // $2258
  int level3BonusTimeout = 0;             // $2263
  bool level3BirdActive = false;          // $2700-$2720 presentation records
  int level3BirdState = 0;
  std::uint8_t level3BirdX = 0;           // $2706 (leading cell)
  std::uint8_t level3BirdBagX = 0;        // $2726
  std::array<Level3Coin, 13> level3Coins{};
  int level3CoinIndex = 0;                // $28DC
  int level3CoinCount = 0;                // $28DD
  bool level3CoinStarted = false;         // $28DE
  int level3PileFrame = 0;
  int level3Difficulty = 2;               // $28F3 (default dip setting)
  int level3Visits = 1;                   // $2210
  bool level3Water = false;               // $2207: the water round with the dolphins
  int level3Level2Visits = 1;             // $220F selects the coin value
  bool level3Invulnerable = false;        // replay aid: mirrors the MAME taps
  bool level3ClearProjectiles = false;    // replay aid: projectiles wiped
  int stage4CurrentBall = 0;
  bool stage4Airborne = false;
  bool stage4PinnedCrash = false;
  int stage4FallFrame = 0;
  int stage4RespawnGraceFrames = 0;
  int stage4NextBallToActivate = 0;
  int stage4JumpDirection = 0;
  int stage4IdleFrame = 0;
  std::uint32_t stage4BallCollisionAudioSerial = 0;
  std::uint32_t jumpAudioSerial = 0;
  std::uint32_t crashAudioSerial = 0;
  std::uint32_t extraCharlieAudioSerial = 0;
  std::uint32_t prizeBagAudioSerial = 0;
  std::uint32_t hiddenCoinAudioSerial = 0;
  std::uint32_t coinAudioSerial = 0;
  std::uint32_t eventSelectMoveAudioSerial = 0;
  std::uint32_t eventSelectConfirmAudioSerial = 0;
  std::uint32_t stage3BounceAudioSerial = 0;
  std::uint32_t stage5SpringAudioSerial = 0;   // $BA97 -> $4B
  std::uint32_t stage6BounceAudioSerial = 0;   // $BA7F -> $45
  std::uint32_t stage6GoalAudioSerial = 0;     // $BB0A: the landing on the plank
  std::uint32_t stage6FloorAudioSerial = 0;    // $B857: the floor
  int level6FloorRestartFrames = 0;            // crash frames after a floor fall
  // Event 2: the miss at a monkey hit ($8121), and the floor ($84F2/$85D2),
  // where the fail piece plays out before the restart, as on the trapeze.
  std::uint32_t stage2HitAudioSerial = 0;
  // Events 2, 3 and 4 share the floor: the miss as Charlie lands, the fail
  // piece after it, and the restart as the piece ends.
  std::uint32_t floorFallAudioSerial = 0;
  bool floorFallMiss = false;                // the floor also plays the miss (no hit before)
  int failPieceFrames = 0;                   // frames of fail.wav; 0: the board's own timing
  std::uint32_t stage5FallAudioSerial = 0;     // $BA93 -> $4A
  std::uint32_t stage3OverjumpAudioSerial = 0;   // the miss ($4F)
  bool level3MissCued = false;                    // played at the hit already
  std::uint32_t level3ShowerAudioSerial = 0;
  std::uint32_t randomState = 0x6d2b79f5U;
  bool highScoreDirty = false;
  bool debug = false;
  bool lionOnlyTest = false;
  // Event 1 object scheduler mirrors <$C2:$C3 / $20c2:$20c3 and the course
  // byte index at $2208.  $70AC seeds the accumulator with $10xx.
  std::uint16_t level1HoopActivationAccumulator = 0x1000;
  std::size_t level1HoopCourseIndex = 0;
  std::size_t level1HoopActivations = 0;
  // circusc4 <$BC is the previous course byte/state used by $765c-$7666 to
  // select the reserved fourth object, and <$BB is the selector adjustment
  // advanced by that same branch.  They are not extra-Charlie state.
  std::uint8_t level1HoopCourseState = 0x10;
  // Attract/Level 1 initializes <$BB to $5E. $765A adds this byte to the
  // course selector before BITB #$03, which makes stream indices 2, 6 and
  // 10 the reserved-$2760 decision points in the captured course.
  std::uint8_t level1HoopCourseOffset = 0x5e;
  // <$14 is the board's continuously advancing frame byte.  The reserved
  // fourth-object admission at $76e1-$76e3 uses (<$14 | $80) as its next
  // activation reload on normal difficulty.
  std::uint8_t level1BoardFrameByte = 0;
  // $2203:$2204 is still zero at the start line. $7539 and $7607 suppress
  // the LEFT scroll command while its high byte is zero, so live objects keep
  // only their intrinsic -$0080 drift until the first forward movement.
  bool level1ForwardProgressed = false;
  // circusc4 <$B4 is the grounded A/B/C selector. <$B3 stores the previous
  // course-position sample used by $73DC-$7405, while this fixed value
  // reproduces the low-byte course stream read by that routine. Rendering
  // reads these fields only; Level 1 movement and collision remain separate.
  Level1RiderState level1RiderState = Level1RiderState::RunA;
  std::uint8_t level1RiderPositionSample = 0;
  std::uint16_t level1RiderCourseFixed = 0xfe80;
};

struct RenderSurface {
  SDL_Texture* texture = nullptr;
  int width = 0;
  int height = 0;
  SDL_Rect destination{0, 0, 0, 0};
};

// The opaque bounds of every cell of a sprite sheet, measured when the sheet
// is loaded, so a pose can be anchored by its own feet and centre instead of
// by the cell edge (the Event 2 sheets carry uneven transparent padding).
struct SheetCells {
  std::vector<SDL_Rect> bounds;
  int columns = 0;
  int rows = 0;
  int cellWidth = 0;
  int cellHeight = 0;
  bool valid() const { return !bounds.empty(); }
};

struct Assets {
  mutable SDL_Texture* bootFrameTexture = nullptr;
  mutable int bootFrameTextureIndex = -1;
  mutable SDL_Texture* attractFrame = nullptr;
  mutable int attractFrameClip = -1;
  mutable int attractFrameIndex = -1;
  SDL_Texture* arena = nullptr;
  SDL_Texture* marquee = nullptr;
  SDL_Texture* ferrisWheel = nullptr;
  SDL_Texture* ferrisGondola = nullptr;
  SDL_Texture* riderRunA = nullptr;
  SDL_Texture* riderRunB = nullptr;
  SDL_Texture* riderRunC = nullptr;
  SDL_Texture* riderBackE = nullptr;
  SDL_Texture* riderBackF = nullptr;
  SDL_Texture* burnRider = nullptr;
  SDL_Texture* hoop = nullptr;
  SDL_Texture* hoopFlare = nullptr;
  SDL_Texture* props = nullptr;
  SDL_Texture* propsFlare = nullptr;
  SDL_Texture* bird = nullptr;
  SDL_Texture* rewardBag = nullptr;
  SDL_Texture* charlieLife = nullptr;
  SDL_Texture* extraCharlie = nullptr;
  SDL_Texture* goalPlatform = nullptr;
  SDL_Texture* finishRider = nullptr;
  SDL_Texture* eventSelectProps = nullptr;
  SDL_Texture* eventSelectChosen = nullptr;
  SDL_Texture* stage2Charlie = nullptr;
  SDL_Texture* stage2BrownWalk = nullptr;
  SDL_Texture* stage2PurpleWalk = nullptr;
  SDL_Texture* stage2PurpleJump = nullptr;
  SDL_Texture* stage2GoalRig = nullptr;
  SDL_Texture* stage2Backdrop = nullptr;    // the big top behind the rope
  SheetCells stage2CharlieCells;
  SDL_Texture* stage2CharlieWalk = nullptr;   // the nine-frame walk cycle
  SDL_Texture* stage2CharlieJump = nullptr;   // the ten-frame jump
  SDL_Texture* stage2CharliePoses = nullptr;  // stand, cheer, fallen, tuck
  SheetCells stage2CharlieWalkCells;
  SheetCells stage2CharlieJumpCells;
  SheetCells stage2CharliePosesCells;
  SheetCells stage2BrownCells;
  SheetCells stage2PurpleCells;
  SheetCells stage2PurpleJumpCells;
  SDL_Texture* stage3Charlie = nullptr;
  SheetCells stage3CharlieCells;              // v4: one row of 12, feet on the baseline
  SDL_Texture* stage3CharlieVertical = nullptr;
  SheetCells stage3CharlieVerticalCells;
  SDL_Texture* stage3CharlieRoofHead = nullptr;
  SDL_Texture* stage3Tambourine = nullptr;
  SDL_Texture* stage3GoalTambourine = nullptr;
  SDL_Texture* stage3KnifeThrower = nullptr;
  SDL_Texture* stage3FlameThrower = nullptr;
  SDL_Texture* stage3Projectiles = nullptr;
  SDL_Texture* stage3FlameProjectile = nullptr;
  SDL_Texture* stage3Dolphin = nullptr;
  SDL_Texture* stage3Splash = nullptr;
  SDL_Texture* stage3WaterTile = nullptr;
  SDL_Texture* stage3WaterSurface = nullptr;
  SDL_Texture* stage3PoolLadder = nullptr;
  SDL_Texture* elephantDoor = nullptr;   // the performers' entrance in the arena wall
  SDL_Texture* stage3DrumRing = nullptr;
  SDL_Texture* stage4Charlie = nullptr;
  SDL_Texture* stage4Ball = nullptr;
  SDL_Texture* stage5Pony = nullptr;
  SDL_Texture* stage5Springboard = nullptr;
  SDL_Texture* stage6Trapeze = nullptr;
  SDL_Texture* stage6Trampoline = nullptr;
  // Charlie atlases from tools/import_sprites.py (Events 5 and 6).
  SDL_Texture* stage5CharlieRide = nullptr;
  SDL_Texture* stage5CharlieLeap = nullptr;
  SDL_Texture* stage5CharlieFall = nullptr;
  SheetCells stage4CharlieCells;              // the ball stage's cells, reused by Event 5
  SDL_Texture* stage5CharlieSit = nullptr;    // Event 5: dazed on the grass
  SheetCells stage5CharlieSitCells;
  SDL_Texture* stage6CharlieHang = nullptr;
  SDL_Texture* stage6CharlieTumble = nullptr;
  SDL_Texture* stage6CharlieFloor = nullptr;
  SDL_Texture* stage6Tub = nullptr;         // the "trampoline": a circus tub
  SDL_Texture* floorSign = nullptr;         // the HD distance marker (Event 6)
  // The board's balloon callouts as three tintable layers each (cloud,
  // letters, outline; tools/build_callouts.py): GREAT, FAR OUT, OH NO!!.
  std::array<SDL_Texture*, 3> calloutGreat{};
  std::array<SDL_Texture*, 3> calloutFarOut{};
  std::array<SDL_Texture*, 3> calloutOhNo{};
  SDL_Texture* stage6Plank = nullptr;       // the goal platform's plank
  SDL_Texture* stage6Lattice = nullptr;     // one bay of the goal tower
  SDL_Texture* stage6Truss = nullptr;       // one bay of the overhead truss
  SDL_Texture* stage6Catcher = nullptr;     // the partner on the decorated bars
  SDL_Texture* stage6Girl = nullptr;        // the partner on the goal platform
  SDL_Texture* stage6Kiss = nullptr;        // Charlie and his partner at the goal
  SheetCells stage6KissCells;
  SheetCells stage6CatcherCells;
  SheetCells stage6GirlCells;
  SheetCells stage5CharlieRideCells;
  SheetCells stage5CharlieLeapCells;
  SheetCells stage5CharlieFallCells;
  SheetCells stage6CharlieHangCells;
  SheetCells stage6CharlieTumbleCells;
  SheetCells stage6CharlieFloorCells;
};

struct AudioClip {
  SDL_AudioSpec spec{};
  Uint8* data = nullptr;
  Uint32 length = 0;
};

struct AudioVoice {
  const AudioClip* clip = nullptr;
  Uint32 position = 0;
  int volume = SDL_MIX_MAXVOLUME;
  bool loop = false;
  bool active = false;
  Uint32 playbackStep = 1;
};

struct AudioEngine {
  SDL_AudioDeviceID device = 0;
  SDL_AudioSpec deviceSpec{};
  AudioClip stageMusic;
  AudioClip stage2Music;
  AudioClip stage3Music;
  AudioClip stage4Music;
  AudioClip stage6Music;       // $16: the aerial-bar piece
  AudioClip stage6Bounce;      // $45: the trampoline
  AudioClip boot;
  AudioClip between;           // the title footage's sound
  AudioClip stage3Bounce;
  AudioClip stage5Springboard;
  AudioClip stage5Fall;
  AudioClip stage4BallCollision;
  AudioClip jump;
  AudioClip fail;
  AudioClip miss;
  AudioClip missTwo;
  AudioClip crowdCheer;
  AudioClip birdCoinDrop;
  AudioClip bonusCount;
  AudioClip extraCharlie;
  AudioClip prizeBag;
  AudioClip hiddenCoin;
  AudioClip creditInsert;
  AudioClip eventSelectMusic;
  AudioClip eventSelectMove;
  AudioClip eventSelectConfirm;
  std::array<AudioVoice, 12> voices{};
  bool available = false;
  // --record: the mixed output is kept while a clip is being recorded and
  // written next to its frames as audio.wav, so the clip has its own sound.
  bool capturing = false;
  std::vector<Uint8> capture;
};

std::string highScoreMemoryPath() {
  char* preferencePath = SDL_GetPrefPath("abispac", "CircusCharlieHD");
  if (!preferencePath) return {};
  const std::filesystem::path path =
      std::filesystem::path(preferencePath) / "high-score.txt";
  SDL_free(preferencePath);
  // A high score saved under the project's former name ("Big Top Run
  // Native", stored as <prefs>/BigTopRun/BigTopRunNative/) is carried over
  // the first time the renamed build runs.
  std::error_code error;
  if (!std::filesystem::exists(path, error)) {
    const std::filesystem::path legacy =
        path.parent_path().parent_path().parent_path() / "BigTopRun" /
        "BigTopRunNative" / "high-score.txt";
    if (std::filesystem::exists(legacy, error)) {
      std::filesystem::copy_file(legacy, path, error);
    }
  }
  return path.string();
}

int loadHighScore(const std::string& path) {
  if (path.empty()) return kDefaultHighScore;
  std::ifstream input(path);
  int savedScore = 0;
  if (!(input >> savedScore) || savedScore < 0 || savedScore > 99999999) {
    return kDefaultHighScore;
  }
  return std::max(kDefaultHighScore, savedScore);
}

// The cabinet's battery RAM: the event screen's unlock and each event's
// plays, in <prefs>/progress.txt as "unlocked p1 p2 p3 p4 p5 p6".
std::string progressMemoryPath(const std::string& highScorePath) {
  if (highScorePath.empty()) return {};
  return (std::filesystem::path(highScorePath).parent_path() / "progress.txt").string();
}

// The painted Charlie's size, tuned live with [ and ], kept next to the
// high score so it survives a restart.
std::string charlieScalePath(const std::string& highScorePath) {
  if (highScorePath.empty()) return {};
  return (std::filesystem::path(highScorePath).parent_path() / "charlie-scale.txt").string();
}

void loadCharlieScale(const std::string& path, Game& game) {
  if (path.empty()) return;
  std::ifstream input(path);
  float value = 0.0F;
  if (input >> value && value >= 0.5F && value <= 2.0F) game.charlieScale = value;
}

void saveCharlieScale(const std::string& path, const Game& game) {
  if (path.empty()) return;
  std::ofstream output(path);
  output << game.charlieScale << '\n';
}

void loadProgress(const std::string& path, Game& game) {
  if (path.empty()) return;
  std::ifstream input(path);
  int unlocked = 0;
  if (!(input >> unlocked)) return;
  std::array<int, 6> plays{};
  for (int& value : plays) {
    if (!(input >> value) || value < 0 || value > 9999) return;
  }
  game.selectUnlocked = unlocked != 0;
  game.playCount = plays;
}

bool saveProgress(const std::string& path, const Game& game) {
  if (path.empty()) return false;
  std::ofstream output(path, std::ios::trunc);
  if (!output) return false;
  output << (game.selectUnlocked ? 1 : 0);
  for (const int value : game.playCount) output << ' ' << value;
  output << '\n';
  return output.good();
}

bool saveHighScore(const std::string& path, int highScore) {
  if (path.empty()) return false;
  std::ofstream output(path, std::ios::trunc);
  if (!output) return false;
  output << std::max(kDefaultHighScore, highScore) << '\n';
  return output.good();
}

bool parseMode(std::string_view value, int& width, int& height) {
  const auto split = value.find('x');
  if (split == std::string_view::npos) return false;
  try {
    const int parsedWidth = std::stoi(std::string(value.substr(0, split)));
    const int parsedHeight = std::stoi(std::string(value.substr(split + 1)));
    if (parsedWidth < 240 || parsedHeight < 240) return false;
    width = parsedWidth;
    height = parsedHeight;
    return true;
  } catch (...) {
    return false;
  }
}

void printUsage() {
  std::cout
      << "Circus Charlie HD\n"
      << "  --mode WIDTHxHEIGHT\n"
      << "  --render WIDTHxHEIGHT|native   (portrait render resolution, default 480x640)\n"
      << "  --rotate 0|90|180|270\n"
      << "  --fullscreen\n"
      << "  --event 1|2|3|4|5|6\n"
      << "  --record DIR [--record-seconds N] [--record-fps F]\n"
      << "        write an attract clip (JPEG frames) of the event from its first\n"
      << "        playing frame until Charlie's fall has played out (or N s), then quit\n"
      << "  --debug\n"
      << "  --lion-test\n"
      << "  --capture FILE.png\n"
      << "  --trace FILE.csv\n"
      << "  --rider-diagnostic-dir DIRECTORY\n"
      << "  --trace-mode hold-right|right-release|right-left|forward-jump|"
         "successful-hoop|air-right-hold|air-right-left|"
         "air-right-release|air-left-right|air-left-release|air-neutral|"
         "start-neutral|start-left|start-right|start-right-left\n"
      << "  --capture-scene start|select|layout|large|prize|gameplay|stage2|stage2-goal|stage3|stage3-transfer|stage3-approach|stage3-goal|stage3-roof|stage3-water|stage3-water-2|stage4|stage4-jump|stage4-fall|stage4-goal|ring|extra|crash|goal|tally\n";
}

std::optional<Options> parseOptions(int argc, char** argv) {
  Options options;
  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];
    if (argument == "--fullscreen") {
      options.fullscreen = true;
    } else if (argument == "--lion-test") {
      options.lionTest = true;
    } else if (argument == "--debug") {
      options.debug = true;
    } else if (argument == "--capture" && index + 1 < argc) {
      options.capturePath = argv[++index];
    } else if (argument == "--record" && index + 1 < argc) {
      options.recordDir = argv[++index];
    } else if (argument == "--record-seconds" && index + 1 < argc) {
      options.recordSeconds = std::max(1, std::atoi(argv[++index]));
    } else if (argument == "--record-fps" && index + 1 < argc) {
      options.recordFps = std::clamp(std::atoi(argv[++index]), 1, 60);
    } else if (argument == "--trace" && index + 1 < argc) {
      options.tracePath = argv[++index];
    } else if (argument == "--trace-mode" && index + 1 < argc) {
      options.traceMode = argv[++index];
      if (options.traceMode != "hold-right" &&
          options.traceMode != "right-release" &&
          options.traceMode != "right-left" &&
          options.traceMode != "forward-jump" &&
          options.traceMode != "successful-hoop" &&
          options.traceMode != "air-right-hold" &&
          options.traceMode != "air-right-left" &&
          options.traceMode != "air-right-release" &&
          options.traceMode != "air-left-right" &&
          options.traceMode != "air-left-release" &&
          options.traceMode != "air-neutral" &&
          options.traceMode != "start-neutral" &&
          options.traceMode != "start-left" &&
          options.traceMode != "start-right" &&
          options.traceMode != "start-right-left") {
        std::cerr << "Unknown Level 1 trace mode.\n";
        return std::nullopt;
      }
    } else if (argument == "--rider-diagnostic-dir" && index + 1 < argc) {
      options.riderDiagnosticDir = argv[++index];
    } else if (argument == "--rider-diagnostic-frames" && index + 1 < argc) {
      std::string list = argv[++index];
      std::size_t start = 0;
      while (start <= list.size()) {
        const std::size_t comma = list.find(',', start);
        const std::string item =
            list.substr(start, comma == std::string::npos ? std::string::npos
                                                          : comma - start);
        if (!item.empty()) options.riderDiagnosticFrames.push_back(std::stoi(item));
        if (comma == std::string::npos) break;
        start = comma + 1;
      }
    } else if (argument == "--replay" && index + 1 < argc) {
      options.replayPath = argv[++index];
    } else if (argument == "--replay-output" && index + 1 < argc) {
      options.replayOutput = argv[++index];
    } else if (argument == "--replay-capture-dir" && index + 1 < argc) {
      options.replayCaptureDir = argv[++index];
    } else if (argument == "--replay-capture-frames" && index + 1 < argc) {
      std::string list = argv[++index];
      std::size_t start = 0;
      while (start <= list.size()) {
        const std::size_t comma = list.find(',', start);
        const std::string item =
            list.substr(start, comma == std::string::npos ? std::string::npos
                                                          : comma - start);
        if (!item.empty()) options.replayCaptureFrames.push_back(std::stoi(item));
        if (comma == std::string::npos) break;
        start = comma + 1;
      }
    } else if (argument == "--replay-start" && index + 1 < argc) {
      options.replayStart = std::stoi(argv[++index]);
    } else if (argument == "--replay-offset" && index + 1 < argc) {
      options.replayOffset = std::stoi(argv[++index]);
    } else if (argument == "--replay-course-offset" && index + 1 < argc) {
      options.replayCourseOffset = std::stoi(argv[++index], nullptr, 0);
    } else if (argument == "--replay-coin-selector" && index + 1 < argc) {
      options.replayCoinSelector = std::stoi(argv[++index], nullptr, 0);
    } else if (argument == "--replay-frame-byte" && index + 1 < argc) {
      options.replayFrameByte = std::stoi(argv[++index], nullptr, 0);
    } else if (argument == "--replay-event" && index + 1 < argc) {
      options.replayEvent = std::stoi(argv[++index]) - 1;
    } else if (argument == "--replay-invulnerable") {
      options.replayInvulnerable = true;
    } else if (argument == "--replay-water") {
      options.replayWater = true;
    } else if (argument == "--water") {
      options.startWater = true;
    } else if (argument == "--replay-perfect") {
      options.replayPerfect = true;
    } else if (argument == "--replay-visits" && index + 1 < argc) {
      options.replayVisits = std::stoi(argv[++index]);
    } else if (argument == "--replay-clear-projectiles") {
      options.replayClearProjectiles = true;
    } else if (argument == "--capture-scene" && index + 1 < argc) {
      options.captureScene = argv[++index];
      if (options.captureScene != "start" &&
          options.captureScene != "select" &&
          options.captureScene != "layout" &&
          options.captureScene != "large" &&
          options.captureScene != "prize" &&
          options.captureScene != "gameplay" &&
          options.captureScene != "stage2" &&
          options.captureScene != "stage2-goal" &&
          options.captureScene != "stage3" &&
          options.captureScene != "stage3-transfer" &&
          options.captureScene != "stage3-approach" &&
          options.captureScene != "stage3-goal" &&
          options.captureScene != "stage3-roof" &&
          options.captureScene != "stage3-water" &&
          options.captureScene != "stage3-water-2" &&
          options.captureScene != "stage4" &&
          options.captureScene != "stage4-jump" &&
          options.captureScene != "stage4-fall" &&
          options.captureScene != "stage4-goal" &&
          options.captureScene != "ring" &&
          options.captureScene != "extra" &&
          options.captureScene != "crash" &&
          options.captureScene != "goal" &&
          options.captureScene != "tally") {
        std::cerr
            << "Capture scene must be start, select, layout, large, prize, gameplay, stage2, stage2-goal, stage3, stage3-transfer, stage3-approach, stage3-goal, stage3-roof, ring, extra, crash, goal, or tally.\n";
        return std::nullopt;
      }
    } else if (argument == "--mode" && index + 1 < argc) {
      if (!parseMode(argv[++index], options.width, options.height)) {
        std::cerr << "Invalid display mode.\n";
        return std::nullopt;
      }
    } else if (argument == "--render" && index + 1 < argc) {
      const std::string value = argv[++index];
      if (value == "native" || value == "full") {
        options.renderWidth = 0;
        options.renderHeight = 0;
      } else if (!parseMode(value, options.renderWidth, options.renderHeight)) {
        std::cerr << "Invalid render resolution.\n";
        return std::nullopt;
      }
    } else if (argument == "--event" && index + 1 < argc) {
      options.startEvent = std::atoi(argv[++index]);
      if (options.startEvent < 1 || options.startEvent > 6) {
        std::cerr << "Event must be 1 to 6.\n";
        return std::nullopt;
      }
    } else if (argument == "--rotate" && index + 1 < argc) {
      options.rotation = std::atoi(argv[++index]);
      if (options.rotation != 0 && options.rotation != 90 &&
          options.rotation != 180 && options.rotation != 270) {
        std::cerr << "Rotation must be 0, 90, 180 or 270.\n";
        return std::nullopt;
      }
    } else if (argument == "--help" || argument == "-h") {
      printUsage();
      options.showHelp = true;
    } else {
      std::cerr << "Unknown option: " << argument << '\n';
      return std::nullopt;
    }
  }
  if (!options.riderDiagnosticDir.empty() &&
      (options.tracePath.empty() || options.traceMode != "successful-hoop")) {
    std::cerr << "Rider diagnostics require --trace and "
                 "--trace-mode successful-hoop.\n";
    return std::nullopt;
  }
  return options;
}

SDL_Color color(Uint8 red, Uint8 green, Uint8 blue, Uint8 alpha = 255) {
  return SDL_Color{red, green, blue, alpha};
}

void setColor(SDL_Renderer* renderer, SDL_Color value) {
  SDL_SetRenderDrawColor(renderer, value.r, value.g, value.b, value.a);
}

// Scan a sheet's alpha for the opaque bounds of each cell.
SheetCells measureSheetCells(const char* filename, int columns, int rows) {
  SheetCells cells;
  SDL_Surface* loaded = nullptr;
  const std::string relativePath = std::string("assets/") + filename;
  loaded = IMG_Load(relativePath.c_str());
  if (!loaded) {
    if (char* basePath = SDL_GetBasePath()) {
      const std::string executablePath =
          std::string(basePath) + "assets/" + filename;
      SDL_free(basePath);
      loaded = IMG_Load(executablePath.c_str());
    }
  }
  if (!loaded) return cells;
  SDL_Surface* surface =
      SDL_ConvertSurfaceFormat(loaded, SDL_PIXELFORMAT_RGBA32, 0);
  SDL_FreeSurface(loaded);
  if (!surface) return cells;
  cells.columns = columns;
  cells.rows = rows;
  cells.cellWidth = surface->w / columns;
  cells.cellHeight = surface->h / rows;
  SDL_LockSurface(surface);
  const auto* pixels = static_cast<const Uint8*>(surface->pixels);
  for (int row = 0; row < rows; ++row) {
    for (int column = 0; column < columns; ++column) {
      int minX = cells.cellWidth, minY = cells.cellHeight, maxX = -1, maxY = -1;
      for (int y = 0; y < cells.cellHeight; ++y) {
        const Uint8* line = pixels +
            static_cast<std::size_t>(row * cells.cellHeight + y) *
                static_cast<std::size_t>(surface->pitch) +
            static_cast<std::size_t>(column * cells.cellWidth) * 4;
        for (int x = 0; x < cells.cellWidth; ++x) {
          if (line[x * 4 + 3] <= 40) continue;
          minX = std::min(minX, x);
          maxX = std::max(maxX, x);
          minY = std::min(minY, y);
          maxY = std::max(maxY, y);
        }
      }
      SDL_Rect bounds{column * cells.cellWidth, row * cells.cellHeight,
                      cells.cellWidth, cells.cellHeight};
      if (maxX >= 0) {
        bounds = {column * cells.cellWidth + minX, row * cells.cellHeight + minY,
                  maxX - minX + 1, maxY - minY + 1};
      }
      cells.bounds.push_back(bounds);
    }
  }
  SDL_UnlockSurface(surface);
  SDL_FreeSurface(surface);
  return cells;
}

SDL_Texture* loadAsset(SDL_Renderer* renderer, const char* filename) {
  const std::string relativePath = std::string("assets/") + filename;
  if (SDL_Texture* texture = IMG_LoadTexture(renderer, relativePath.c_str())) {
    SDL_SetTextureScaleMode(texture, SDL_ScaleModeLinear);
    return texture;
  }

  char* basePath = SDL_GetBasePath();
  if (!basePath) return nullptr;
  const std::string executablePath =
      std::string(basePath) + "assets/" + filename;
  SDL_free(basePath);
  SDL_Texture* texture = IMG_LoadTexture(renderer, executablePath.c_str());
  if (texture) SDL_SetTextureScaleMode(texture, SDL_ScaleModeLinear);
  return texture;
}

SDL_Texture* loadBootFrame(SDL_Renderer* renderer, int frameIndex) {
  std::array<char, 64> filename{};
  std::snprintf(filename.data(), filename.size(),
                "boot/boot-%04d.png", frameIndex);
  SDL_Texture* texture = loadAsset(renderer, filename.data());
  if (texture) {
    // Preserve the recorded arcade pixels while the logical canvas scales
    // the 224x256 source to the cabinet render size.
    SDL_SetTextureScaleMode(texture, SDL_ScaleModeNearest);
  }
  return texture;
}

Assets loadAssets(SDL_Renderer* renderer) {
  Assets assets;
  assets.arena = loadAsset(renderer, "stage1-arena-green.png");
  assets.marquee = loadAsset(renderer, "stage1-marquee-v2.png");
  assets.ferrisWheel = loadAsset(renderer, "stage1-ferris-wheel.png");
  assets.ferrisGondola = loadAsset(renderer, "stage1-ferris-gondola.png");
  assets.riderRunA = loadAsset(renderer, "stage1-rider-run-a-hd.png");
  assets.riderRunB = loadAsset(renderer, "stage1-rider-run-b-hd.png");
  assets.riderRunC = loadAsset(renderer, "stage1-rider-run-c-hd.png");
  // Backward walk poses E/F ($EEA6 table): the walking lion from the
  // lion-walk atlas carrying the production Charlie/saddle cut from Run A.
  assets.riderBackE = loadAsset(renderer, "stage1-rider-back-e-hd.png");
  assets.riderBackF = loadAsset(renderer, "stage1-rider-back-f-hd.png");
  assets.burnRider = loadAsset(renderer, "stage1-burn-rider-v1.png");
  assets.hoop = loadAsset(renderer, "stage1-hoop.png");
  assets.hoopFlare = loadAsset(renderer, "stage1-hoop-flare.png");
  assets.props = loadAsset(renderer, "stage1-props.png");
  assets.propsFlare = loadAsset(renderer, "stage1-props-flare.png");
  assets.bird = loadAsset(renderer, "stage1-bird-sheet-v2.png");
  assets.rewardBag = loadAsset(renderer, "stage1-reward-bag.png");
  assets.charlieLife = loadAsset(renderer, "stage1-charlie-life-v2.png");
  assets.extraCharlie =
      loadAsset(renderer, "stage1-extra-charlie-hang-v1.png");
  assets.goalPlatform = loadAsset(renderer, "stage1-goal-platform-v4.png");
  assets.finishRider =
      loadAsset(renderer, "stage1-finish-rider-v2.png");
  assets.eventSelectProps =
      loadAsset(renderer, "event-select-props-v3.png");
  assets.eventSelectChosen =
      loadAsset(renderer, "event-select-selected-v7.png");
  assets.stage2Charlie =
      loadAsset(renderer, "stage2-charlie-sheet-v1.png");
  assets.stage2CharlieWalk =
      loadAsset(renderer, "stage2-charlie-walk-9-v4.png");
  assets.stage2CharlieJump =
      loadAsset(renderer, "stage2-charlie-jump-10-v4.png");
  assets.stage2CharliePoses =
      loadAsset(renderer, "stage2-charlie-poses-4-v4.png");
  assets.stage2BrownWalk =
      loadAsset(renderer, "stage2-brown-walk-4-v3.png");
  assets.stage2PurpleWalk =
      loadAsset(renderer, "stage2-purple-walk-4-v3.png");
  assets.stage2PurpleJump =
      loadAsset(renderer, "stage2-purple-leap-3-v3.png");
  assets.stage2GoalRig =
      loadAsset(renderer, "stage2-goal-rig-v1.png");
  assets.stage2Backdrop = loadAsset(renderer, "stage2-backdrop-v1.png");
  assets.stage2CharlieCells =
      measureSheetCells("stage2-charlie-sheet-v1.png", 3, 2);
  assets.stage2CharlieWalkCells =
      measureSheetCells("stage2-charlie-walk-9-v4.png", 9, 1);
  assets.stage2CharlieJumpCells =
      measureSheetCells("stage2-charlie-jump-10-v4.png", 10, 1);
  assets.stage2CharliePosesCells =
      measureSheetCells("stage2-charlie-poses-4-v4.png", 4, 1);
  assets.stage2BrownCells = measureSheetCells("stage2-brown-walk-4-v3.png", 4, 1);
  assets.stage2PurpleCells =
      measureSheetCells("stage2-purple-walk-4-v3.png", 4, 1);
  assets.stage2PurpleJumpCells =
      measureSheetCells("stage2-purple-leap-3-v3.png", 3, 1);
  assets.stage3Charlie =
      loadAsset(renderer, "stage3-charlie-bounce-12-v4.png");
  assets.stage3CharlieCells =
      measureSheetCells("stage3-charlie-bounce-12-v4.png", 12, 1);
  assets.stage3CharlieVertical =
      loadAsset(renderer, "stage3-charlie-vertical-12-v4.png");
  assets.stage3CharlieVerticalCells =
      measureSheetCells("stage3-charlie-vertical-12-v4.png", 12, 1);
  assets.stage3CharlieRoofHead =
      loadAsset(renderer, "stage3-charlie-roof-head-4-v2.png");
  assets.stage3Tambourine =
      loadAsset(renderer, "stage3-tambourine-v1.png");
  assets.stage3GoalTambourine =
      loadAsset(renderer, "stage3-goal-tambourine-v1.png");
  assets.stage3KnifeThrower =
      loadAsset(renderer, "stage3-knife-thrower-12-v4.png");
  assets.stage3FlameThrower =
      loadAsset(renderer, "stage3-fire-breather-12-v5.png");
  assets.stage3Projectiles =
      loadAsset(renderer, "stage3-projectiles-8-v1.png");
  assets.stage3FlameProjectile =
      loadAsset(renderer, "stage3-flame-projectile-4-v3.png");
  assets.stage3Dolphin = loadAsset(renderer, "stage3-dolphin-6-v2.png");
  assets.stage3Splash = loadAsset(renderer, "stage3-splash-4-v2.png");
  assets.stage3WaterTile = loadAsset(renderer, "stage3-water-tile-v1.png");
  assets.stage3WaterSurface = loadAsset(renderer, "stage3-water-surface-v1.png");
  assets.stage3PoolLadder = loadAsset(renderer, "stage3-pool-ladder-v1.png");
  assets.elephantDoor = loadAsset(renderer, "arena-elephant-door-v1.png");
  assets.stage3DrumRing = loadAsset(renderer, "stage3-drum-ring-v1.png");
  assets.stage4Charlie = loadAsset(renderer, "stage4-charlie-12-v2.png");
  assets.stage4CharlieCells = measureSheetCells("stage4-charlie-12-v2.png", 4, 3);
  assets.stage4Ball = loadAsset(renderer, "stage4-ball-centered-v2.png");
  assets.stage5Pony = loadAsset(renderer, "stage5-pony-8-v1.png");
  assets.stage5Springboard = loadAsset(renderer, "stage5-springboard-v1.png");
  assets.stage6Trapeze = loadAsset(renderer, "stage6-trapeze-v1.png");
  assets.stage6Trampoline = loadAsset(renderer, "stage6-trampoline-v1.png");
  assets.stage5CharlieRide = loadAsset(renderer, "stage5-charlie-ride-4-v3.png");
  assets.stage5CharlieLeap = loadAsset(renderer, "stage5-charlie-leap-6-v1.png");
  assets.stage5CharlieFall = loadAsset(renderer, "stage5-charlie-fall-4-v1.png");
  assets.stage5CharlieSit = loadAsset(renderer, "stage5-charlie-sit-1-v2.png");
  assets.stage5CharlieSitCells = measureSheetCells("stage5-charlie-sit-1-v2.png", 1, 1);
  assets.stage6CharlieHang = loadAsset(renderer, "stage6-charlie-hang-6-v3.png");
  assets.stage6CharlieTumble = loadAsset(renderer, "stage6-charlie-tumble-6-v1.png");
  assets.stage6CharlieFloor = loadAsset(renderer, "stage6-charlie-floor-4-v1.png");
  assets.stage5CharlieRideCells = measureSheetCells("stage5-charlie-ride-4-v3.png", 4, 1);
  assets.stage5CharlieLeapCells = measureSheetCells("stage5-charlie-leap-6-v1.png", 6, 1);
  assets.stage5CharlieFallCells = measureSheetCells("stage5-charlie-fall-4-v1.png", 4, 1);
  assets.stage6CharlieHangCells = measureSheetCells("stage6-charlie-hang-6-v3.png", 6, 1);
  assets.stage6CharlieTumbleCells = measureSheetCells("stage6-charlie-tumble-6-v1.png", 6, 1);
  assets.stage6CharlieFloorCells = measureSheetCells("stage6-charlie-floor-4-v1.png", 4, 1);
  assets.stage6Tub = loadAsset(renderer, "stage6-tub-v2.png");
  assets.floorSign = loadAsset(renderer, "floor-sign-v1.png");
  {
    const char* layers[3] = {"cloud", "letters", "outline"};
    for (int i = 0; i < 3; ++i) {
      assets.calloutGreat[i] = loadAsset(renderer, (std::string("callout-great-") + layers[i] + ".png").c_str());
      assets.calloutFarOut[i] = loadAsset(renderer, (std::string("callout-farout-") + layers[i] + ".png").c_str());
      assets.calloutOhNo[i] = loadAsset(renderer, (std::string("callout-ohno-") + layers[i] + ".png").c_str());
    }
  }
  gFloorSign = assets.floorSign;
  assets.stage6Plank = loadAsset(renderer, "stage6-plank-v1.png");
  assets.stage6Lattice = loadAsset(renderer, "stage6-lattice-v1.png");
  assets.stage6Truss = loadAsset(renderer, "stage6-truss-v2.png");
  assets.stage6Catcher = loadAsset(renderer, "stage6-catcher-3-v3.png");
  assets.stage6Girl = loadAsset(renderer, "stage6-girl-4-v2.png");
  assets.stage6CatcherCells = measureSheetCells("stage6-catcher-3-v3.png", 3, 1);
  assets.stage6GirlCells = measureSheetCells("stage6-girl-4-v2.png", 4, 1);
  assets.stage6Kiss = loadAsset(renderer, "stage6-kiss-6-v3.png");
  assets.stage6KissCells = measureSheetCells("stage6-kiss-6-v3.png", 6, 1);
  if (!assets.arena || !assets.marquee || !assets.ferrisWheel ||
      !assets.ferrisGondola || !assets.riderRunA || !assets.riderRunB ||
      !assets.riderRunC ||
      !assets.burnRider || !assets.hoop ||
      !assets.hoopFlare || !assets.props || !assets.propsFlare ||
      !assets.bird || !assets.rewardBag || !assets.charlieLife ||
      !assets.extraCharlie ||
      !assets.goalPlatform || !assets.finishRider ||
      !assets.eventSelectProps || !assets.eventSelectChosen ||
      !assets.stage2Charlie || !assets.stage2BrownWalk ||
      !assets.stage2PurpleWalk || !assets.stage2PurpleJump ||
      !assets.stage2GoalRig || !assets.stage3Charlie ||
      !assets.stage3CharlieVertical ||
      !assets.stage3CharlieRoofHead ||
      !assets.stage3Tambourine || !assets.stage3GoalTambourine ||
      !assets.stage3KnifeThrower ||
      !assets.stage3FlameThrower || !assets.stage3Projectiles ||
      !assets.stage3FlameProjectile || !assets.stage4Charlie ||
      !assets.stage4Ball) {
    std::cerr << "Some HD assets could not be loaded; vector fallbacks remain "
                 "available. SDL_image: "
              << IMG_GetError() << '\n';
  }
  return assets;
}

void destroyAssets(Assets& assets) {
  if (assets.bootFrameTexture) SDL_DestroyTexture(assets.bootFrameTexture);
  if (assets.attractFrame) SDL_DestroyTexture(assets.attractFrame);
  if (assets.arena) SDL_DestroyTexture(assets.arena);
  if (assets.marquee) SDL_DestroyTexture(assets.marquee);
  if (assets.ferrisWheel) SDL_DestroyTexture(assets.ferrisWheel);
  if (assets.ferrisGondola) SDL_DestroyTexture(assets.ferrisGondola);
  if (assets.riderRunA) SDL_DestroyTexture(assets.riderRunA);
  if (assets.riderRunB) SDL_DestroyTexture(assets.riderRunB);
  if (assets.riderRunC) SDL_DestroyTexture(assets.riderRunC);
  if (assets.riderBackE) SDL_DestroyTexture(assets.riderBackE);
  if (assets.riderBackF) SDL_DestroyTexture(assets.riderBackF);
  if (assets.burnRider) SDL_DestroyTexture(assets.burnRider);
  if (assets.hoop) SDL_DestroyTexture(assets.hoop);
  if (assets.hoopFlare) SDL_DestroyTexture(assets.hoopFlare);
  if (assets.props) SDL_DestroyTexture(assets.props);
  if (assets.propsFlare) SDL_DestroyTexture(assets.propsFlare);
  if (assets.bird) SDL_DestroyTexture(assets.bird);
  if (assets.rewardBag) SDL_DestroyTexture(assets.rewardBag);
  if (assets.charlieLife) SDL_DestroyTexture(assets.charlieLife);
  if (assets.extraCharlie) SDL_DestroyTexture(assets.extraCharlie);
  if (assets.goalPlatform) SDL_DestroyTexture(assets.goalPlatform);
  if (assets.finishRider) SDL_DestroyTexture(assets.finishRider);
  if (assets.eventSelectProps) SDL_DestroyTexture(assets.eventSelectProps);
  if (assets.eventSelectChosen) SDL_DestroyTexture(assets.eventSelectChosen);
  if (assets.stage2Charlie) SDL_DestroyTexture(assets.stage2Charlie);
  if (assets.stage2CharlieWalk) SDL_DestroyTexture(assets.stage2CharlieWalk);
  if (assets.stage2CharlieJump) SDL_DestroyTexture(assets.stage2CharlieJump);
  if (assets.stage2CharliePoses) SDL_DestroyTexture(assets.stage2CharliePoses);
  if (assets.stage2BrownWalk) SDL_DestroyTexture(assets.stage2BrownWalk);
  if (assets.stage2PurpleWalk) SDL_DestroyTexture(assets.stage2PurpleWalk);
  if (assets.stage2PurpleJump) SDL_DestroyTexture(assets.stage2PurpleJump);
  if (assets.stage2GoalRig) SDL_DestroyTexture(assets.stage2GoalRig);
  if (assets.stage3Charlie) SDL_DestroyTexture(assets.stage3Charlie);
  if (assets.stage3CharlieVertical)
    SDL_DestroyTexture(assets.stage3CharlieVertical);
  if (assets.stage3CharlieRoofHead)
    SDL_DestroyTexture(assets.stage3CharlieRoofHead);
  if (assets.stage3Tambourine) SDL_DestroyTexture(assets.stage3Tambourine);
  if (assets.stage3GoalTambourine)
    SDL_DestroyTexture(assets.stage3GoalTambourine);
  if (assets.stage3KnifeThrower) SDL_DestroyTexture(assets.stage3KnifeThrower);
  if (assets.stage3FlameThrower) SDL_DestroyTexture(assets.stage3FlameThrower);
  if (assets.stage3Projectiles) SDL_DestroyTexture(assets.stage3Projectiles);
  if (assets.stage3FlameProjectile)
    SDL_DestroyTexture(assets.stage3FlameProjectile);
  if (assets.stage4Charlie) SDL_DestroyTexture(assets.stage4Charlie);
  if (assets.stage4Ball) SDL_DestroyTexture(assets.stage4Ball);
  if (assets.stage5Pony) SDL_DestroyTexture(assets.stage5Pony);
  if (assets.stage5Springboard) SDL_DestroyTexture(assets.stage5Springboard);
  if (assets.stage6Trapeze) SDL_DestroyTexture(assets.stage6Trapeze);
  if (assets.stage6Trampoline) SDL_DestroyTexture(assets.stage6Trampoline);
  for (SDL_Texture* texture : {assets.stage5CharlieRide, assets.stage5CharlieLeap,
                               assets.stage5CharlieFall, assets.stage5CharlieSit,
                               assets.stage6CharlieHang,
                               assets.stage6CharlieTumble, assets.stage6CharlieFloor,
                               assets.stage6Catcher, assets.stage6Girl,
                               assets.stage6Kiss, assets.stage6Tub, assets.floorSign, assets.stage6Plank,
                               assets.calloutGreat[0], assets.calloutGreat[1], assets.calloutGreat[2],
                               assets.calloutFarOut[0], assets.calloutFarOut[1], assets.calloutFarOut[2],
                               assets.calloutOhNo[0], assets.calloutOhNo[1], assets.calloutOhNo[2],
                               assets.stage6Lattice, assets.stage6Truss}) {
    if (texture) SDL_DestroyTexture(texture);
  }
  assets = {};
}

bool loadAudioAsset(const char* filename, AudioClip& clip) {
  const std::string relativePath = std::string("assets/audio/") + filename;
  if (SDL_LoadWAV(relativePath.c_str(), &clip.spec, &clip.data,
                  &clip.length)) {
    return true;
  }

  char* basePath = SDL_GetBasePath();
  if (!basePath) return false;
  const std::string executablePath =
      std::string(basePath) + "assets/audio/" + filename;
  SDL_free(basePath);
  return SDL_LoadWAV(executablePath.c_str(), &clip.spec, &clip.data,
                     &clip.length) != nullptr;
}

void audioCallback(void* userdata, Uint8* stream, int byteCount) {
  auto& audio = *static_cast<AudioEngine*>(userdata);
  SDL_memset(stream, 0, static_cast<size_t>(byteCount));
  const Uint32 frameBytes =
      static_cast<Uint32>(SDL_AUDIO_BITSIZE(audio.deviceSpec.format) / 8) *
      audio.deviceSpec.channels;
  std::array<Uint8, 8192> steppedBuffer{};

  for (auto& voice : audio.voices) {
    int outputOffset = 0;
    while (voice.active && outputOffset < byteCount && voice.clip &&
           voice.clip->data && voice.clip->length > 0) {
      if (voice.position >= voice.clip->length) {
        if (voice.loop) {
          voice.position = 0;
        } else {
          voice.active = false;
          break;
        }
      }
      if (voice.playbackStep > 1 && frameBytes > 0) {
        const Uint32 requested =
            static_cast<Uint32>(byteCount - outputOffset);
        const Uint32 frameCapacity = std::min(
            requested / frameBytes,
            static_cast<Uint32>(steppedBuffer.size()) / frameBytes);
        Uint32 copiedFrames = 0;
        while (copiedFrames < frameCapacity && voice.active) {
          if (voice.position + frameBytes > voice.clip->length) {
            if (voice.loop) {
              voice.position = 0;
            } else {
              voice.active = false;
              break;
            }
          }
          SDL_memcpy(steppedBuffer.data() + copiedFrames * frameBytes,
                     voice.clip->data + voice.position, frameBytes);
          voice.position += frameBytes * voice.playbackStep;
          ++copiedFrames;
        }
        const Uint32 mixedBytes = copiedFrames * frameBytes;
        if (mixedBytes == 0) break;
        SDL_MixAudioFormat(stream + outputOffset, steppedBuffer.data(),
                           audio.deviceSpec.format, mixedBytes,
                           voice.volume);
        outputOffset += static_cast<int>(mixedBytes);
        continue;
      }

      const Uint32 remaining = voice.clip->length - voice.position;
      const Uint32 requested =
          static_cast<Uint32>(byteCount - outputOffset);
      const Uint32 mixedBytes = std::min(remaining, requested);
      SDL_MixAudioFormat(
          stream + outputOffset, voice.clip->data + voice.position,
          audio.deviceSpec.format, mixedBytes, voice.volume);
      voice.position += mixedBytes;
      outputOffset += static_cast<int>(mixedBytes);
    }
  }
  if (audio.capturing) {
    audio.capture.insert(audio.capture.end(), stream, stream + byteCount);
  }
}

void setAudioCapture(AudioEngine& audio, bool on) {
  if (!audio.available) return;
  SDL_LockAudioDevice(audio.device);
  audio.capturing = on;
  if (on) audio.capture.clear();
  SDL_UnlockAudioDevice(audio.device);
}

// The captured mix as a WAV in the device's own PCM format.
bool writeAudioCapture(const AudioEngine& audio, const std::string& path) {
  if (!audio.available) return false;
  const SDL_AudioSpec& spec = audio.deviceSpec;
  const Uint16 bits = SDL_AUDIO_BITSIZE(spec.format);
  const Uint16 channels = spec.channels;
  const Uint16 blockAlign = static_cast<Uint16>(channels * bits / 8);
  const Uint32 byteRate = static_cast<Uint32>(spec.freq) * blockAlign;
  const Uint32 dataBytes = static_cast<Uint32>(audio.capture.size());
  std::ofstream out(path, std::ios::binary);
  if (!out) return false;
  auto put16 = [&](Uint16 v) { out.put(static_cast<char>(v & 0xFF)); out.put(static_cast<char>(v >> 8)); };
  auto put32 = [&](Uint32 v) { put16(static_cast<Uint16>(v & 0xFFFF)); put16(static_cast<Uint16>(v >> 16)); };
  out.write("RIFF", 4); put32(36 + dataBytes); out.write("WAVE", 4);
  out.write("fmt ", 4); put32(16);
  put16(SDL_AUDIO_ISFLOAT(spec.format) ? 3 : 1);
  put16(channels); put32(static_cast<Uint32>(spec.freq)); put32(byteRate);
  put16(blockAlign); put16(bits);
  out.write("data", 4); put32(dataBytes);
  out.write(reinterpret_cast<const char*>(audio.capture.data()), dataBytes);
  return static_cast<bool>(out);
}

bool loadAudio(AudioEngine& audio) {
  const bool loaded =
      loadAudioAsset("boot.wav", audio.boot) &&
      loadAudioAsset("event1-stage.wav", audio.stageMusic) &&
      loadAudioAsset("event2-stage.wav", audio.stage2Music) &&
      loadAudioAsset("event3-stage.wav", audio.stage3Music) &&
      loadAudioAsset("event4-stage.wav", audio.stage4Music) &&
      loadAudioAsset("stage3-bounce.wav", audio.stage3Bounce) &&
      loadAudioAsset("stage4-ball-collision.wav",
                     audio.stage4BallCollision) &&
      loadAudioAsset("jump.wav", audio.jump) &&
      loadAudioAsset("fail.wav", audio.fail) &&
      loadAudioAsset("miss.wav", audio.miss) &&
      loadAudioAsset("miss-2.wav", audio.missTwo) &&
      loadAudioAsset("crowd-cheer.wav", audio.crowdCheer) &&
      loadAudioAsset("bird-coin-drop.wav", audio.birdCoinDrop) &&
      loadAudioAsset("bonus-count.wav", audio.bonusCount) &&
      loadAudioAsset("event-select.wav", audio.eventSelectMusic) &&
      loadAudioAsset("event-select-move.wav", audio.eventSelectMove) &&
      loadAudioAsset("event-select-confirm.wav", audio.eventSelectConfirm);
  if (!loaded) {
    std::cerr << "One or more audio assets could not be loaded: "
              << SDL_GetError() << '\n';
    return false;
  }

  const SDL_AudioSpec reference = audio.stageMusic.spec;
  const auto matchesReference = [&reference](const AudioClip& clip) {
    return clip.spec.freq == reference.freq &&
           clip.spec.format == reference.format &&
           clip.spec.channels == reference.channels;
  };
  // Reward effects remain optional until their exact arcade command IDs have
  // been verified. RMS 1007 from isolated command 0x42 is the confirmed
  // extra-Charlie pickup effect. The credit sound remains reserved for an
  // actual credit insertion and is never substituted for a gameplay reward.
  loadAudioAsset("extra-charlie.wav", audio.extraCharlie);
  loadAudioAsset("prize-bag.wav", audio.prizeBag);
  loadAudioAsset("hidden-coin.wav", audio.hiddenCoin);
  loadAudioAsset("coin.wav", audio.creditInsert);
  loadAudioAsset("stage5-springboard.wav", audio.stage5Springboard);
  loadAudioAsset("stage5-fall.wav", audio.stage5Fall);
  loadAudioAsset("event6-stage.wav", audio.stage6Music);
  loadAudioAsset("between.wav", audio.between);
  loadAudioAsset("stage6-bounce.wav", audio.stage6Bounce);

  if (!matchesReference(audio.jump) || !matchesReference(audio.fail) ||
      !matchesReference(audio.miss) ||
      !matchesReference(audio.missTwo) ||
      !matchesReference(audio.crowdCheer) ||
      !matchesReference(audio.birdCoinDrop) ||
      !matchesReference(audio.bonusCount) ||
      !matchesReference(audio.eventSelectMusic) ||
      !matchesReference(audio.eventSelectMove) ||
      !matchesReference(audio.eventSelectConfirm) ||
      !matchesReference(audio.stage2Music) ||
      !matchesReference(audio.stage3Music) ||
      !matchesReference(audio.stage4Music) ||
      !matchesReference(audio.boot) ||
      !matchesReference(audio.stage3Bounce) ||
      !matchesReference(audio.stage4BallCollision) ||
      (audio.extraCharlie.data && !matchesReference(audio.extraCharlie)) ||
      (audio.prizeBag.data && !matchesReference(audio.prizeBag)) ||
      (audio.hiddenCoin.data && !matchesReference(audio.hiddenCoin)) ||
      (audio.creditInsert.data && !matchesReference(audio.creditInsert))) {
    std::cerr << "Audio assets do not share one PCM format.\n";
    return false;
  }

  SDL_AudioSpec desired = reference;
  desired.samples = 1024;
  desired.callback = audioCallback;
  desired.userdata = &audio;
  audio.device = SDL_OpenAudioDevice(nullptr, 0, &desired,
                                     &audio.deviceSpec, 0);
  if (audio.device == 0) {
    std::cerr << "Audio device could not be opened: " << SDL_GetError()
              << '\n';
    return false;
  }
  audio.available = true;
  SDL_PauseAudioDevice(audio.device, 0);
  return true;
}

void setAudioVoice(AudioEngine& audio, size_t voiceIndex,
                   const AudioClip& clip, int volume, bool loop) {
  if (!audio.available || voiceIndex >= audio.voices.size() || !clip.data) {
    return;
  }
  SDL_LockAudioDevice(audio.device);
  audio.voices[voiceIndex] =
      AudioVoice{&clip, 0, volume, loop, true, 1};
  SDL_UnlockAudioDevice(audio.device);
}

void setStageMusicFast(AudioEngine& audio, bool fast) {
  if (!audio.available) return;
  SDL_LockAudioDevice(audio.device);
  auto& musicVoice = audio.voices[0];
  if (musicVoice.active &&
      (musicVoice.clip == &audio.stageMusic ||
       musicVoice.clip == &audio.stage2Music ||
       musicVoice.clip == &audio.stage3Music ||
       musicVoice.clip == &audio.stage4Music)) {
    musicVoice.playbackStep = fast ? 2U : 1U;
  }
  SDL_UnlockAudioDevice(audio.device);
}

void playStageMusic(AudioEngine& audio, int selectedEvent, bool fast) {
  // Event 5 ($13) shares Event 1's piece; Event 6 ($16) has its own, the
  // aerial-bar piece, which loops like the others so a long act is never
  // left in silence.
  const bool trapeze = selectedEvent == 5 && audio.stage6Music.data;
  const AudioClip& music =
      selectedEvent == 1 ? audio.stage2Music
      : selectedEvent == 2 ? audio.stage3Music
      : selectedEvent == 3 ? audio.stage4Music
      : trapeze ? audio.stage6Music
                : audio.stageMusic;
  setAudioVoice(audio, 0, music,
                static_cast<int>(SDL_MIX_MAXVOLUME * 0.58F), true);
  setStageMusicFast(audio, fast);
}

void playEventSelectMusic(AudioEngine& audio) {
  setAudioVoice(audio, 0, audio.eventSelectMusic,
                static_cast<int>(SDL_MIX_MAXVOLUME * 0.66F), false);
}

void playBootAudio(AudioEngine& audio) {
  setAudioVoice(audio, 0, audio.boot, SDL_MIX_MAXVOLUME, false);
}

void playBetweenAudio(AudioEngine& audio) {
  if (!audio.between.data) return;
  setAudioVoice(audio, 0, audio.between, SDL_MIX_MAXVOLUME, false);
}

void stopStageMusic(AudioEngine& audio) {
  if (!audio.available) return;
  SDL_LockAudioDevice(audio.device);
  audio.voices[0].active = false;
  SDL_UnlockAudioDevice(audio.device);
}

void playJumpSound(AudioEngine& audio) {
  setAudioVoice(audio, 1, audio.jump, SDL_MIX_MAXVOLUME, false);
}

void playFailMusic(AudioEngine& audio) {
  // Failure owns the music voice: stage music has already stopped, and this
  // complete arcade cue determines exactly when Charlie may respawn.
  setAudioVoice(audio, 0, audio.fail, SDL_MIX_MAXVOLUME, false);
}

void playCrowdCheer(AudioEngine& audio, bool loop = false) {
  setAudioVoice(audio, 4, audio.crowdCheer,
                static_cast<int>(SDL_MIX_MAXVOLUME * 0.86F), loop);
}

void stopCrowdCheer(AudioEngine& audio) {
  if (!audio.available) return;
  SDL_LockAudioDevice(audio.device);
  audio.voices[4].active = false;
  SDL_UnlockAudioDevice(audio.device);
}

void playBirdCoinDrop(AudioEngine& audio) {
  setAudioVoice(audio, 5, audio.birdCoinDrop, SDL_MIX_MAXVOLUME, false);
}

void playBonusCount(AudioEngine& audio) {
  setAudioVoice(audio, 6, audio.bonusCount,
                static_cast<int>(SDL_MIX_MAXVOLUME * 0.92F), false);
}

void playExtraCharlieSound(AudioEngine& audio) {
  setAudioVoice(audio, 7, audio.extraCharlie, SDL_MIX_MAXVOLUME, false);
}

void playPrizeBagSound(AudioEngine& audio) {
  setAudioVoice(audio, 8, audio.prizeBag, SDL_MIX_MAXVOLUME, false);
}

void playHiddenCoinSound(AudioEngine& audio) {
  setAudioVoice(audio, 9, audio.hiddenCoin, SDL_MIX_MAXVOLUME, false);
}

void playCreditInsertSound(AudioEngine& audio) {
  setAudioVoice(audio, 10, audio.creditInsert, SDL_MIX_MAXVOLUME, false);
}

void playEventSelectMoveSound(AudioEngine& audio) {
  setAudioVoice(audio, 11, audio.eventSelectMove, SDL_MIX_MAXVOLUME, false);
}

void playEventSelectConfirmSound(AudioEngine& audio) {
  setAudioVoice(audio, 11, audio.eventSelectConfirm,
                SDL_MIX_MAXVOLUME, false);
}

void playStage3BounceSound(AudioEngine& audio) {
  setAudioVoice(audio, 1, audio.stage3Bounce, SDL_MIX_MAXVOLUME, false);
}

void playStage5SpringSound(AudioEngine& audio) {
  if (!audio.stage5Springboard.data) return;
  setAudioVoice(audio, 1, audio.stage5Springboard, SDL_MIX_MAXVOLUME, false);
}

void playStage6BounceSound(AudioEngine& audio) {
  if (!audio.stage6Bounce.data) return;
  setAudioVoice(audio, 1, audio.stage6Bounce, SDL_MIX_MAXVOLUME, false);
}

void playStage5FallSound(AudioEngine& audio) {
  if (!audio.stage5Fall.data) return;
  setAudioVoice(audio, 1, audio.stage5Fall, SDL_MIX_MAXVOLUME, false);
}

void playStage3OverjumpSound(AudioEngine& audio) {
  setAudioVoice(audio, 1, audio.miss,
                static_cast<int>(SDL_MIX_MAXVOLUME * 0.92F), false);
}

void playStage4BallCollisionSound(AudioEngine& audio) {
  setAudioVoice(audio, 11, audio.stage4BallCollision,
                SDL_MIX_MAXVOLUME, false);
}

int audioDurationInBoardFrames(const AudioClip& clip) {
  const int bytesPerSample = SDL_AUDIO_BITSIZE(clip.spec.format) / 8;
  const int bytesPerFrame = bytesPerSample * clip.spec.channels;
  if (!clip.data || clip.spec.freq <= 0 || bytesPerFrame <= 0) return 1;
  const double seconds =
      static_cast<double>(clip.length) /
      static_cast<double>(clip.spec.freq * bytesPerFrame);
  return std::max(1, static_cast<int>(std::ceil(seconds * kBoardRefresh)));
}

void destroyAudio(AudioEngine& audio) {
  if (audio.device != 0) {
    SDL_PauseAudioDevice(audio.device, 1);
    SDL_CloseAudioDevice(audio.device);
  }
  if (audio.stageMusic.data) SDL_FreeWAV(audio.stageMusic.data);
  if (audio.stage2Music.data) SDL_FreeWAV(audio.stage2Music.data);
  if (audio.stage3Music.data) SDL_FreeWAV(audio.stage3Music.data);
  if (audio.stage4Music.data) SDL_FreeWAV(audio.stage4Music.data);
  if (audio.boot.data) SDL_FreeWAV(audio.boot.data);
  if (audio.stage3Bounce.data) SDL_FreeWAV(audio.stage3Bounce.data);
  if (audio.stage4BallCollision.data)
    SDL_FreeWAV(audio.stage4BallCollision.data);
  if (audio.jump.data) SDL_FreeWAV(audio.jump.data);
  if (audio.fail.data) SDL_FreeWAV(audio.fail.data);
  if (audio.miss.data) SDL_FreeWAV(audio.miss.data);
  if (audio.missTwo.data) SDL_FreeWAV(audio.missTwo.data);
  if (audio.crowdCheer.data) SDL_FreeWAV(audio.crowdCheer.data);
  if (audio.birdCoinDrop.data) SDL_FreeWAV(audio.birdCoinDrop.data);
  if (audio.bonusCount.data) SDL_FreeWAV(audio.bonusCount.data);
  if (audio.extraCharlie.data) SDL_FreeWAV(audio.extraCharlie.data);
  if (audio.prizeBag.data) SDL_FreeWAV(audio.prizeBag.data);
  if (audio.hiddenCoin.data) SDL_FreeWAV(audio.hiddenCoin.data);
  if (audio.creditInsert.data) SDL_FreeWAV(audio.creditInsert.data);
  if (audio.eventSelectMusic.data) SDL_FreeWAV(audio.eventSelectMusic.data);
  if (audio.eventSelectMove.data) SDL_FreeWAV(audio.eventSelectMove.data);
  if (audio.eventSelectConfirm.data)
    SDL_FreeWAV(audio.eventSelectConfirm.data);
  audio = {};
}

void fillRect(SDL_Renderer* renderer, float x, float y, float width,
              float height, SDL_Color value) {
  setColor(renderer, value);
  const SDL_FRect rectangle{x, y, width, height};
  SDL_RenderFillRectF(renderer, &rectangle);
}

void line(SDL_Renderer* renderer, float x1, float y1, float x2, float y2,
          SDL_Color value) {
  setColor(renderer, value);
  SDL_RenderDrawLineF(renderer, x1, y1, x2, y2);
}

void filledCircle(SDL_Renderer* renderer, float cx, float cy, float radius,
                  SDL_Color value) {
  setColor(renderer, value);
  const int integerRadius = std::max(1, static_cast<int>(std::ceil(radius)));
  for (int y = -integerRadius; y <= integerRadius; ++y) {
    const float span =
        std::sqrt(std::max(0.0F, radius * radius - static_cast<float>(y * y)));
    SDL_RenderDrawLineF(renderer, cx - span, cy + static_cast<float>(y),
                       cx + span, cy + static_cast<float>(y));
  }
}

void ellipse(SDL_Renderer* renderer, float cx, float cy, float rx, float ry,
             SDL_Color value, int thickness = 1) {
  setColor(renderer, value);
  constexpr int kSegments = 72;
  for (int layer = 0; layer < thickness; ++layer) {
    const float layerRx = rx + static_cast<float>(layer);
    const float layerRy = ry + static_cast<float>(layer);
    float previousX = cx + layerRx;
    float previousY = cy;
    for (int segment = 1; segment <= kSegments; ++segment) {
      const float angle =
          (2.0F * kPi * static_cast<float>(segment)) /
          static_cast<float>(kSegments);
      const float nextX = cx + std::cos(angle) * layerRx;
      const float nextY = cy + std::sin(angle) * layerRy;
      SDL_RenderDrawLineF(renderer, previousX, previousY, nextX, nextY);
      previousX = nextX;
      previousY = nextY;
    }
  }
}

const std::array<uint8_t, 7>& glyph(char character) {
  static const std::array<uint8_t, 7> blank{};
  static const std::array<uint8_t, 7> a{14, 17, 17, 31, 17, 17, 17};
  static const std::array<uint8_t, 7> b{30, 17, 17, 30, 17, 17, 30};
  static const std::array<uint8_t, 7> c{14, 17, 16, 16, 16, 17, 14};
  static const std::array<uint8_t, 7> d{30, 17, 17, 17, 17, 17, 30};
  static const std::array<uint8_t, 7> e{31, 16, 16, 30, 16, 16, 31};
  static const std::array<uint8_t, 7> f{31, 16, 16, 30, 16, 16, 16};
  static const std::array<uint8_t, 7> g{14, 17, 16, 23, 17, 17, 14};
  static const std::array<uint8_t, 7> h{17, 17, 17, 31, 17, 17, 17};
  static const std::array<uint8_t, 7> i{14, 4, 4, 4, 4, 4, 14};
  static const std::array<uint8_t, 7> j{7, 2, 2, 2, 18, 18, 12};
  static const std::array<uint8_t, 7> k{17, 18, 20, 24, 20, 18, 17};
  static const std::array<uint8_t, 7> l{16, 16, 16, 16, 16, 16, 31};
  static const std::array<uint8_t, 7> m{17, 27, 21, 21, 17, 17, 17};
  static const std::array<uint8_t, 7> n{17, 25, 21, 19, 17, 17, 17};
  static const std::array<uint8_t, 7> o{14, 17, 17, 17, 17, 17, 14};
  static const std::array<uint8_t, 7> p{30, 17, 17, 30, 16, 16, 16};
  static const std::array<uint8_t, 7> q{14, 17, 17, 17, 21, 18, 13};
  static const std::array<uint8_t, 7> r{30, 17, 17, 30, 20, 18, 17};
  static const std::array<uint8_t, 7> s{15, 16, 16, 14, 1, 1, 30};
  static const std::array<uint8_t, 7> t{31, 4, 4, 4, 4, 4, 4};
  static const std::array<uint8_t, 7> u{17, 17, 17, 17, 17, 17, 14};
  static const std::array<uint8_t, 7> v{17, 17, 17, 17, 17, 10, 4};
  static const std::array<uint8_t, 7> w{17, 17, 17, 21, 21, 21, 10};
  static const std::array<uint8_t, 7> x{17, 17, 10, 4, 10, 17, 17};
  static const std::array<uint8_t, 7> y{17, 17, 10, 4, 4, 4, 4};
  static const std::array<uint8_t, 7> z{31, 1, 2, 4, 8, 16, 31};
  static const std::array<uint8_t, 7> zero{14, 17, 19, 21, 25, 17, 14};
  static const std::array<uint8_t, 7> one{4, 12, 4, 4, 4, 4, 14};
  static const std::array<uint8_t, 7> two{14, 17, 1, 2, 4, 8, 31};
  static const std::array<uint8_t, 7> three{30, 1, 1, 14, 1, 1, 30};
  static const std::array<uint8_t, 7> four{2, 6, 10, 18, 31, 2, 2};
  static const std::array<uint8_t, 7> five{31, 16, 16, 30, 1, 1, 30};
  static const std::array<uint8_t, 7> six{14, 16, 16, 30, 17, 17, 14};
  static const std::array<uint8_t, 7> seven{31, 1, 2, 4, 8, 8, 8};
  static const std::array<uint8_t, 7> eight{14, 17, 17, 14, 17, 17, 14};
  static const std::array<uint8_t, 7> nine{14, 17, 17, 15, 1, 1, 14};
  static const std::array<uint8_t, 7> colon{0, 4, 4, 0, 4, 4, 0};
  static const std::array<uint8_t, 7> dot{0, 0, 0, 0, 0, 4, 4};
  static const std::array<uint8_t, 7> dash{0, 0, 0, 31, 0, 0, 0};
  static const std::array<uint8_t, 7> exclamation{4, 4, 4, 4, 4, 0, 4};

  static const std::array<const std::array<uint8_t, 7>*, 26> letters{
      &a, &b, &c, &d, &e, &f, &g, &h, &i, &j, &k, &l, &m,
      &n, &o, &p, &q, &r, &s, &t, &u, &v, &w, &x, &y, &z};
  static const std::array<const std::array<uint8_t, 7>*, 10> numbers{
      &zero, &one, &two, &three, &four,
      &five, &six, &seven, &eight, &nine};

  if (character >= 'A' && character <= 'Z') {
    return *letters[static_cast<size_t>(character - 'A')];
  }
  if (character >= 'a' && character <= 'z') {
    return *letters[static_cast<size_t>(character - 'a')];
  }
  if (character >= '0' && character <= '9') {
    return *numbers[static_cast<size_t>(character - '0')];
  }
  if (character == ':') return colon;
  if (character == '.') return dot;
  if (character == '-') return dash;
  if (character == '!') return exclamation;
  return blank;
}

void drawText(SDL_Renderer* renderer, std::string_view text, float x, float y,
              float scale, SDL_Color value, bool centered = false) {
  const float glyphAdvance = 6.0F * scale;
  if (centered) {
    const float inkWidth = text.empty()
                               ? 0.0F
                               : (static_cast<float>(text.size() - 1U) *
                                      6.0F +
                                  5.0F) *
                                     scale;
    x -= inkWidth * 0.5F;
  }
  for (const char character : text) {
    const auto& bitmap = glyph(character);
    for (int row = 0; row < 7; ++row) {
      for (int column = 0; column < 5; ++column) {
        if ((bitmap[static_cast<size_t>(row)] &
             (1U << static_cast<unsigned>(4 - column))) != 0) {
          fillRect(renderer, x + static_cast<float>(column) * scale,
                   y + static_cast<float>(row) * scale, scale, scale, value);
        }
      }
    }
    x += glyphAdvance;
  }
}

[[maybe_unused]] std::uint32_t nextRandom(Game& game) {
  std::uint32_t value = game.randomState;
  value ^= value << 13;
  value ^= value >> 17;
  value ^= value << 5;
  game.randomState = value;
  return value;
}

// Convert board object X (8.8, rider reference $40) into world units.
float level1ObjectWorldX(const Game& game, std::uint16_t sourceXFixed) {
  return game.cameraX + kLevel1RiderCollisionScreenX +
         (static_cast<float>(sourceXFixed) / 256.0F - 64.0F) *
             kSourceToWorldX;
}

// $2203 page byte: zero only while the course has not moved right of the
// start line.  Progress is (page << 16) - ($2204:$2205).
int level1Page(std::int32_t progressFixed) {
  if (progressFixed <= 0) return 0;
  return static_cast<int>((progressFixed + 0xffff) >> 16);
}

std::uint16_t level1PageOffset(std::int32_t progressFixed) {
  const std::int32_t page = level1Page(progressFixed);
  return static_cast<std::uint16_t>((page << 16) - progressFixed);
}

// $2204 alone: the descending page offset byte used by the fire-pot and goal
// windows and by the grounded run-cycle sampler <$B3.
std::uint8_t level1PageOffsetByte(std::int32_t progressFixed) {
  return static_cast<std::uint8_t>(level1PageOffset(progressFixed) >> 8U);
}

float level1ProgressPixels(const Game& game) {
  return static_cast<float>(game.level1ProgressFixed) / 256.0F;
}

// Derive the world/camera presentation from the board progress value.
void syncLevel1World(Game& game) {
  game.player.position.x =
      kLevel1PlayerStartX + level1ProgressPixels(game) * kSourceToWorldX;
  game.cameraX = std::max(0.0F, game.player.position.x - kLevel1PlayerStartX);
  for (std::size_t index = 0; index < game.hoops.size(); ++index) {
    auto& hoop = game.hoops[index];
    hoop.worldX = level1ObjectWorldX(game, hoop.sourceXFixed);
    auto& ring = game.bonusRings[index];
    ring.active = hoop.active && hoop.kind == Level1HoopKind::PrizeRing;
    // The visible rear cells are staged 16 source pixels left of the
    // logical $2760 origin; collision continues to use $2766.
    ring.sourceXFixed = static_cast<std::uint16_t>(
        (hoop.sourceXFixed & 0xff00U) - 0x1000U);
    ring.worldX = level1ObjectWorldX(game, ring.sourceXFixed);
    ring.containsPrize = ring.active && game.level1BagState == 0xff;
    ring.collected = !ring.containsPrize;
  }
  for (auto& pot : game.level1Pots) {
    pot.worldX = level1ObjectWorldX(game, pot.sourceXFixed);
  }
}

// $6E9F-$70DB: Level 1 board initialisation, executed at the course start
// and again after every failure (the board clears $2400-$27FF first).
void initializeLevel1Board(Game& game) {
  game.hoops.assign(4, {});
  for (auto& hoop : game.hoops) {
    hoop.openingBottom = kBigHoopOpeningBottom;
    hoop.openingTop = kBigHoopOpeningTop;
  }
  game.bonusRings.assign(game.hoops.size(), {});
  for (auto& ring : game.bonusRings) {
    ring.height = kBonusRingCenterHeight;
    ring.active = false;
    ring.containsPrize = false;
  }
  game.firePots.clear();
  for (auto& pot : game.level1Pots) pot = FirePot{};
  // <$B0-<$CB are cleared, then <$BB and <$C1 sample the free-running frame
  // byte ($6EAC-$6EB7): the small-ring phase and the coin pot are random.
  game.level1HoopCourseOffset =
      game.replayCourseOffsetOverride >= 0
          ? static_cast<std::uint8_t>(game.replayCourseOffsetOverride)
          : game.level1BoardFrameByte;
  {
    int selector = game.level1BoardFrameByte & 0x03;
    if (selector == 3) selector = 0;
    game.level1CoinPotSelector = game.replayCoinSelectorOverride >= 0
                                     ? game.replayCoinSelectorOverride
                                     : selector;
  }
  game.level1RetireDistance = 0;
  game.level1ReservedRetired = false;
  game.level1CoinPot = -1;
  game.level1CoinArmed = 0;
  game.level1CoinX = 0;
  game.level1CoinYFixed = 0xd200;
  game.level1CoinVelocityFixed = 0;
  game.level1CoinSpin = 0;
  game.level1CoinPopupTimer = 0;
  game.level1BagState = 0;
  game.level1PotMarker = -1;
  game.level1PotPopupTimer = 0;
  game.level1RiderState = Level1RiderState::RunA;
  game.level1RiderBackward = false;
  game.level1RiderPositionSample = 0;
  game.level1RiderCourseFixed = 0xfe80;
  game.level1LandingDirection = 0;
  game.level1ForwardProgressed = level1Page(game.level1ProgressFixed) != 0;
  // $70A2-$70B0: fixed-pot pointers and a $10xx activation accumulator.
  game.level1FixedPotC6 = 2;
  game.level1FixedPotC8 = 1;
  game.level1HoopActivationAccumulator = static_cast<std::uint16_t>(
      0x1000U | (game.level1HoopActivationAccumulator & 0x00ffU));
  game.level1HoopCourseState = 0x10;
  // $70B2-$70C5: a restart inside pages two to four schedules the first chain
  // pot 64 source pixels ahead immediately.
  const int page = level1Page(game.level1ProgressFixed);
  if (page >= 2 && page <= 4) {
    game.level1Pots[0].status = 1;
    game.level1Pots[0].countdown = 0x4000;
  }
  game.extraCharlieActive = false;
  game.extraCharlieHoopIndex = -1;
  game.player.level1JumpPending = false;
  game.player.level1JumpBuffered = false;
  game.player.level1AirborneDirection = 0;
  game.player.grounded = true;
  game.player.jumpFrame = -1;
  game.player.verticalVelocity = 0.0F;
  game.player.runSpeed = 0.0F;
  game.player.position.y = kGroundY;
  game.meterMarkers.clear();
  // $EF07 sign table: 60M-10M sit at page * 256 - 24 for pages one to six.
  for (int signPage = 1; signPage <= 6; ++signPage) {
    game.meterMarkers.push_back(
        {kLevel1RiderCollisionScreenX +
             (static_cast<float>(signPage) * 256.0F - 24.0F) *
                 kSourceToWorldX,
         70 - signPage * 10});
  }
}

void initializeLevel3Board(Game& game, int pageByte);
// A four-way cabinet stick cannot close two opposite contacts, so the board
// never sees a joystick byte with both bits set and has no case for it.  A
// keyboard produces one every time the player turns around, so the key that
// went down last wins and the game only ever sees a value the hardware could
// have produced.
int resolveStick(int stick, int previous, int& latched) {
  if (stick != 3) {
    latched = 0;
    return stick;
  }
  if (latched == 0) latched = previous == 1 ? 2 : 1;
  return latched;
}

void syncLevel3World(Game& game);
void initializeLevel4Board(Game& game, int page);
void syncLevel4World(Game& game);
void updateLevel4(Game& game, const Uint8* keyboard, bool jumpPressed,
                  float controllerAxis);
void initializeLevel2Board(Game& game, int page);
void syncLevel2World(Game& game);
void level5Init(Game& game);
void syncLevel5World(Game& game);
void updateLevel5(Game& game, const Uint8* keyboard, bool jumpPressed,
                  float controllerAxis);
void level6Init(Game& game);
void syncLevel6World(Game& game);
void updateLevel6(Game& game, const Uint8* keyboard, bool jumpPressed,
                  float controllerAxis);
void updateLevel2(Game& game, const Uint8* keyboard, bool jumpPressed,
                  float controllerAxis, bool restartFrame = false);

void resetCourse(Game& game) {
  game.player = Player{};
  game.cameraX = 0.0F;
  game.previousCameraX = 0.0F;
  game.score = 0;
  game.bonus = 6000;
  game.nextScoreLife = kFirstScoreLife;
  game.goalFrame = 0;
  game.tallyFrame = 0;
  game.crashFrame = 0;
  game.clearBonus = 0;
  game.rewardCoinsAwarded = 0;
  game.prizeBagsAvailable = 0;
  game.prizeBagsCollected = 0;
  game.level1PrizeState = 0;
  game.deathOccurred = false;
  game.perfectClear = false;
  game.hiddenCoinTriggered = false;
  game.timeScoreApplied = false;
  game.openingBackwardJumps = 0;
  game.extraCharlieActive = false;
  game.level1ExtraCharlieState = 0;
  game.extraCharlieHoopIndex = -1;
  game.stage2JumpClears = 0;
  game.stage2JumpBrown = false;
  game.stage2JumpPurple = false;
  game.stage2ScorePopup = 0;
  game.stage2ScorePopupFrame = 0;
  game.stage2ScorePopupWorldX = 0.0F;
  game.stage1ScorePopup = 0;
  game.stage1ScorePopupFrame = 0;
  game.stage1ScorePopupWorldX = 0.0F;
  game.stage1ScorePopupY = 0.0F;
  game.level1PendingHoopScore = 0;
  game.level1HoopScoreAwarded = 0;
  game.level1PendingHoopScoreWorldX = 0.0F;
  game.level1PendingHoopScoreY = 0.0F;
  game.stage4Balls.clear();
  game.stage4CurrentBall = 0;
  game.stage4Airborne = false;
  game.stage4PinnedCrash = false;
  game.stage4FallFrame = 0;
  game.stage4RespawnGraceFrames = 0;
  game.stage4NextBallToActivate = 0;
  game.stage4JumpDirection = 0;
  game.stage4IdleFrame = 0;

  if (game.selectedEvent == 1) {
    game.hoops.clear();
    game.firePots.clear();
    game.bonusRings.clear();
    game.stage2Monkeys.clear();
    game.meterMarkers.clear();
    game.prizeBagsAvailable = 0;
    game.level2Started = false;
    game.level3BirdActive = false;
    initializeLevel2Board(game, 0);
    syncLevel2World(game);
    return;
  }

  if (game.selectedEvent == 2) {
    game.hoops.clear();
    game.firePots.clear();
    game.bonusRings.clear();
    game.stage2Monkeys.clear();
    game.meterMarkers.clear();
    // $FB7A: the Level 3 bonus digits start at 5830 on a fresh start.
    game.bonus = 5830;
    game.level3Died = false;
    game.level3Missed = 0;
    game.level3BagValueIndex = 0;
    game.level3BagsTotal = 0;
    // Seven bags can be collected ($FA43 has eight entries but $8AC7 never
    // reaches the second $F9 pair); the count is only used by the HUD/tally.
    game.prizeBagsAvailable = 7;
    initializeLevel3Board(game, 0);
    syncLevel3World(game);
    return;
  }

  if (game.selectedEvent == 3) {
    game.hoops.clear();
    game.firePots.clear();
    game.bonusRings.clear();
    game.stage2Monkeys.clear();
    game.stage4Balls.clear();
    game.prizeBagsAvailable = 0;
    game.level4Started = false;
    initializeLevel4Board(game, 0);
    syncLevel4World(game);
    return;
  }
  if (game.selectedEvent == 4) {
    game.hoops.clear();
    game.firePots.clear();
    game.bonusRings.clear();
    game.stage2Monkeys.clear();
    game.stage4Balls.clear();
    game.prizeBagsAvailable = 0;
    game.level5 = Level5{};
    level5Init(game);
    syncLevel5World(game);
    return;
  }
  if (game.selectedEvent == 5) {
    game.hoops.clear();
    game.firePots.clear();
    game.bonusRings.clear();
    game.stage2Monkeys.clear();
    game.stage4Balls.clear();
    game.prizeBagsAvailable = 0;
    {
      const int visits = game.level6.visits;   // $2213 survives the course reset
      game.level6 = Level6{};
      game.level6.visits = visits;
    }
    game.level6BonusHold = 0;
    level6Init(game);
    syncLevel6World(game);
    return;
  }


  game.stage2Monkeys.clear();
  // The board initialises the Event 1 bonus digits to 5800 on the frame
  // that hands over control (headless capture: $227C-$227F = 5,8,0,0).
  game.bonus = 5800;
  game.level1ProgressFixed = 0;
  game.level1HoopCourseIndex = 0;
  game.level1HoopActivations = 0;
  game.level1PotCounter = 0;
  game.level1CoinState = 0;
  game.level1MissedRewards = 0;
  game.level1LatePotLandings = 0;
  // The manual full-course capture starts with no object live: $2208 is
  // zero, <$C2 holds $1000 and the first large hoop is admitted by the
  // ordinary scheduler nine frames after RIGHT is first held.
  initializeLevel1Board(game);
  syncLevel1World(game);
}

bool eventLocked(const Game& game, int event) {
  return event != kTrapezeEvent &&
         game.playCount[static_cast<std::size_t>(event)] >= kEventPlayLimit;
}

// Start an event: its visit count (one more than the plays so far) sets the
// board's difficulty tables, and the run's score and lives carry over when
// `keepProgress` is set.
void startEvent(Game& game, int event, bool keepProgress, bool water = false) {
  const int score = game.score;
  const int lives = game.lives;
  const int nextLife = game.nextScoreLife;
  game.selectedEvent = event;
  if (water) {
    // $8FF9: the goal of an even visit of Event 3 toggles $2207 and $6BD5
    // leaves the visit count alone, so the water round runs on the same
    // visit, straight after the drums, and is not a play of its own.
    game.level3Water = true;
  } else {
    const int visits = game.playCount[static_cast<std::size_t>(event)] + 1;
    game.level3Water = false;
    switch (event) {
      case 1: game.level2Visits = visits; break;
      case 2:
        game.level3Visits = visits;
        game.level3Level2Visits = game.level2Visits;
        break;
      case 3: game.level4Visits = visits; break;
      case 4: game.level5.visits = visits; break;
      case 5: game.level6.visits = visits; break;
      default: break;
    }
    ++game.playCount[static_cast<std::size_t>(event)];
  }
  game.scene = Scene::Playing;
  if (!keepProgress) game.lives = 3;
  resetCourse(game);
  if (keepProgress) {
    game.score = score;
    game.lives = lives;
    game.nextScoreLife = nextLife;
  }
}

void startGame(Game& game) {
  startEvent(game, game.selectedEvent, false);
}

// The next event of the run after a clear, or the event screen once the
// trapeze has been cleared.
void advanceAfterClear(Game& game) {
  if (game.selectedEvent == 2 && !game.level3Water && game.level3Visits % 2 == 0) {
    // The drums cleared on an even visit: the dolphins follow at once.
    startEvent(game, 2, true, true);
    return;
  }
  if (game.selectedEvent >= kTrapezeEvent) game.selectUnlocked = true;
  if (game.selectUnlocked) {
    game.scene = Scene::EventSelect;
    game.eventSelectFrame = 0;
    game.selectContinuing = true;
    int next = (game.selectedEvent + 1) % kEventCount;
    for (int tries = 0; tries < kEventCount && eventLocked(game, next); ++tries) {
      next = (next + 1) % kEventCount;
    }
    game.selectedEvent = next;
    return;
  }
  startEvent(game, game.selectedEvent + 1, true);
}

void enterEventSelect(Game& game) {
  game.scene = Scene::EventSelect;
  game.selectedEvent = 0;
  game.eventSelectFrame = 0;
}

void confirmEventSelection(Game& game) {
  // Events 1 through 4 are playable. Later selections remain routed to Event 1
  // until their own ROM-measured implementations are ready. Do not substitute
  // another effect for the still-unidentified arcade confirmation sound.
  if (game.selectedEvent > 5) game.selectedEvent = 0;
  if (eventLocked(game, game.selectedEvent)) return;
  if (game.selectContinuing) {
    // Coming off a cleared trapeze: the same game goes on with its lives
    // and score, no credit needed.
    ++game.eventSelectConfirmAudioSerial;
    game.selectContinuing = false;
    startEvent(game, game.selectedEvent, true);
    return;
  }
  if (game.credits <= 0) {
    game.scene = Scene::Title;
    return;
  }
  ++game.eventSelectConfirmAudioSerial;
  --game.credits;
  startGame(game);
}

void moveEventSelection(Game& game, int direction) {
  if (game.scene != Scene::EventSelect || direction == 0) return;
  const int oldEvent = game.selectedEvent;
  const int step = direction < 0 ? -1 : 1;
  int next = (oldEvent + step + kEventCount) % kEventCount;
  for (int tries = 0; tries < kEventCount && eventLocked(game, next); ++tries) {
    next = (next + step + kEventCount) % kEventCount;
  }
  game.selectedEvent = next;
  ++game.eventSelectMoveAudioSerial;
}

void insertCoin(Game& game) {
  if (game.credits >= 99) return;
  ++game.credits;
  ++game.coinAudioSerial;
}

bool startWithCredit(Game& game) {
  if (game.credits <= 0) return false;
  // A new game is the cabinet's: every event back at its first visit and the
  // run from Event 1 in order.  The event screen only opens again once this
  // player clears the trapeze (advanceAfterClear).
  game.playCount.fill(0);
  game.selectUnlocked = false;
  game.selectContinuing = false;
  --game.credits;
  game.selectedEvent = 0;
  startGame(game);
  return true;
}

// One START button for the keyboard (1 or Enter) and the pad. On the event
// screen it confirms the choice; anywhere a game can begin it spends a credit.
// The boot footage counts as one of those places on purpose: on the real
// cabinet you sit through the power-on sequence, but here it is only mood, so
// once a coin is in (5) a press of START cuts the video short and goes straight
// to the event screen. With no credit the press is ignored, just like the
// cabinet, so a stray key can't skip the video by accident.
void pressStart(Game& game) {
  if (game.scene == Scene::EventSelect) {
    confirmEventSelection(game);
  } else if (game.scene == Scene::Attract || game.scene == Scene::GameOver) {
    game.scene = Scene::Title;
    game.titleIdleFrames = 0;
    startWithCredit(game);
  } else if (game.scene == Scene::Boot || game.scene == Scene::Title) {
    startWithCredit(game);
  }
}

void awardScoreLives(Game& game) {
  while (game.score >= game.nextScoreLife) {
    ++game.lives;
    game.nextScoreLife += kRecurringScoreLife;
    ++game.extraCharlieAudioSerial;
  }
}

void restartAfterCrash(Game& game) {
  if (game.lives <= 0) {
    game.scene = Scene::GameOver;
    game.gameOverFrame = 0;
    return;
  }
  game.scene = Scene::Playing;
  if (game.selectedEvent == 0) {
    // $7CC5-$7CD4 steps the course page back by one (two from page seven
    // onward); the restart phase then zeroes the page offset, clears every
    // object record and re-runs $6E9F (headless MAME run: death at $0378
    // restarts at $0200).  Course index, extra-Charlie and coin states
    // persist.
    int page = level1Page(game.level1ProgressFixed);
    page = page >= 7 ? page - 2 : std::max(0, page - 1);
    game.level1ProgressFixed = static_cast<std::int32_t>(page) << 16;
    ++game.level1MissedRewards;
    // $BB25/$BCFA: the re-initialisation restarts the bonus digits at 4000
    // for pages zero ($2258 set by the first start) and one, 3500 for pages
    // two and three, and 3000 beyond (Level 3 capture: 5552 -> 3999).
    game.bonus = page <= 1 ? 4000 : (page <= 3 ? 3500 : 3000);
    initializeLevel1Board(game);
    game.player.alive = true;
    game.crashFrame = 0;
    syncLevel1World(game);
    game.player.previous = game.player.position;
    game.previousCameraX = game.cameraX;
    return;
  }
  if (game.selectedEvent == 2) {
    // $8517-$8525 stepped the page byte back when the fallen pose ended; the
    // restart phase re-runs $8C61 with the saved page and a zero offset.
    // $BB25 then restarts the bonus at 4000 (pages 0-2), 3500 (3-4) or 3000.
    const int pageByte = game.level3RestartPage;
    game.bonus = pageByte == 0 || pageByte >= 0xfe ? 4000
                 : (pageByte >= 0xfc ? 3500 : 3000);
    initializeLevel3Board(game, pageByte);
    game.player.alive = true;
    game.crashFrame = 0;
    syncLevel3World(game);
    game.player.previous = game.player.position;
    game.previousCameraX = game.cameraX;
    return;
  }
  if (game.selectedEvent == 1) {
    // $8509/$8517 stepped the page byte back; the board is re-initialised by
    // the Crashed dispatch, which also runs the first walking tick.
    game.player.alive = true;
    game.crashFrame = 0;
    return;
  }
  game.player.position.x = std::max(78.0F, game.player.position.x - 145.0F);
  game.player.position.y = game.selectedEvent == 1
                               ? kStage2RopeY
                               : (game.selectedEvent == 3
                                      ? kStage4CharlieBaselineY
                                      : kGroundY);
  game.player.previous = game.player.position;
  game.player.verticalVelocity = 0.0F;
  game.player.runSpeed = 0.0F;
  game.player.jumpFrame = -1;
  game.player.grounded = true;
  game.player.alive = true;
  game.crashFrame = 0;
  game.cameraX = std::max(0.0F, game.player.position.x - 78.0F);
  game.previousCameraX = game.cameraX;
  if (game.selectedEvent == 3 && !game.stage4Balls.empty()) {
    game.stage4CurrentBall = std::clamp(
        game.stage4CurrentBall, 0,
        static_cast<int>(game.stage4Balls.size()) - 1);
    auto& ball = game.stage4Balls[static_cast<std::size_t>(game.stage4CurrentBall)];
    ball.velocity = 0.0F;
    game.player.position = {ball.worldX, kStage4CharlieBaselineY};
    game.player.previous = game.player.position;
    game.player.grounded = true;
    game.stage4Airborne = false;
    game.stage4PinnedCrash = false;
    game.stage4FallFrame = 0;
    game.stage4RespawnGraceFrames = 90;
    game.stage4JumpDirection = 0;
    game.stage4IdleFrame = 0;
    game.player.facingRight = true;
    // A squeeze failure can leave another rolling ball occupying the active
    // ball's spawn circle. The board grants a short restart window; move only
    // immediate overlaps out of that circle while preserving the live stream.
    for (auto& candidate : game.stage4Balls) candidate.active = false;
    ball.active = true;
    game.stage4NextBallToActivate = std::min(
        game.stage4CurrentBall + 1,
        static_cast<int>(game.stage4Balls.size()));
    int relocated = 0;
    for (std::size_t index = 0; index < game.stage4Balls.size(); ++index) {
      if (static_cast<int>(index) == game.stage4CurrentBall) continue;
      auto& other = game.stage4Balls[index];
      if (std::abs(other.worldX - ball.worldX) <
          kStage4BallRadius * 2.35F) {
        ++relocated;
        other.worldX = ball.worldX + 190.0F +
                       static_cast<float>(relocated - 1) * 118.0F;
      }
    }
    if (game.stage4NextBallToActivate <
        static_cast<int>(game.stage4Balls.size())) {
      auto& next = game.stage4Balls[static_cast<std::size_t>(
          game.stage4NextBallToActivate++)];
      next.worldX = std::max(next.worldX, ball.worldX + 286.0F);
      // A ball may have inherited the stationary ridden-ball velocity in a
      // collision before the crash. It must resume as an incoming object on
      // respawn or the lane can appear permanently empty.
      if (next.velocity > -10.0F) next.velocity = -62.0F;
      next.collisionCooldown = 0;
      next.active = true;
    }
    game.cameraX = std::max(0.0F, ball.worldX - kStage4PlayerScreenX);
    game.previousCameraX = game.cameraX;
  }
}

// $C34D-$C3F8: row = 11 - 2 * (thousands + 1) + (hundreds < 5), values
// from the word table at $FCC9.
int timeBonusFor(int bonus) {
  if (bonus >= 4500) return 10000;
  if (bonus >= 4000) return 5000;
  if (bonus >= 3500) return 4000;
  if (bonus >= 3000) return 3000;
  if (bonus >= 2500) return 2000;
  if (bonus >= 2000) return 1000;
  if (bonus >= 1500) return 800;
  if (bonus >= 1000) return 600;
  if (bonus >= 500) return 400;
  return 200;
}

int level1RiderCollisionSourceY(const Player& player) {
  // The native ground contact is source row 236, while $2644 is the upper
  // rider-composite row and rests at $D0. All Level 1 jump samples are exact
  // source-pixel displacements, so this recovers the byte read at $713D
  // without introducing a second jump table or modifying its samples.
  return static_cast<int>(std::lround(player.position.y / kSourceToLogicalY)) -
         (236 - kLevel1RiderGroundSourceY);
}

bool overlapsLevel1LargeHoop(const Player& player, const Hoop& hoop,
                             std::size_t hoopSlot, bool trackedHoop) {
  const int hoopSourceX = static_cast<int>(hoop.sourceXFixed >> 8U);
  const int horizontalDistance =
      std::abs(hoopSourceX - kLevel1RiderCollisionSourceX);

  // $7137 CMPA #$0E / $7139 BCC: equality is outside the collision test.
  if (horizontalDistance >= kLevel1HoopHorizontalLimit) return false;

  // $7140 sends the tracked/cleared hoop to its reward-state branch at
  // $71A0. That branch never reaches the failure jump at $7192.
  if (trackedHoop) return false;

  int riderSourceY = level1RiderCollisionSourceY(player);
  // The fourth reusable hardware slot ($2760) uses the same test after the
  // explicit +$10 adjustment at $714B.
  if (hoopSlot == 3) riderSourceY += kLevel1FourthHoopYOffset;
  const int verticalDistance = riderSourceY - kLevel1RiderCollisionBaseY;

  // $714F BPL; a rider above $B6 cannot hit an ordinary large hoop.
  if (verticalDistance < 0) return false;

  // $718C-$7192: failure is the inclusive Manhattan boundary
  // (riderY-$B6)+abs(hoopX-$40) <= $1C.
  return verticalDistance + horizontalDistance <=
         kLevel1HoopCombinedLimit;
}

void crashPlayer(Game& game) {
  if (game.scene != Scene::Playing) return;
  // $7CB6-$7CBD: a converted hanging Charlie (state two) becomes pending
  // again, so it is offered once more after the restart.
  if (game.selectedEvent == 0 && game.level1ExtraCharlieState == 2) {
    game.level1ExtraCharlieState = 1;
  }
  game.player.alive = false;
  game.player.runSpeed = 0.0F;
  game.player.verticalVelocity = 0.0F;
  game.player.level1JumpPending = false;
  game.level1PendingHoopScore = 0;
  game.scene = Scene::Crashed;
  game.crashFrame = 0;
  game.deathOccurred = true;
  ++game.crashAudioSerial;
  --game.lives;
}

// Board rows map onto the logical canvas through the rider's ground row.
float level1RowToWorldY(float row) {
  return kGroundY + (row - static_cast<float>(kLevel1RiderGroundSourceY)) *
                        kSourceToLogicalY;
}

void showStage1Score(Game& game, int points, float worldX, float y) {
  game.stage1ScorePopup = points;
  game.stage1ScorePopupFrame = 52;
  game.stage1ScorePopupWorldX = worldX;
  game.stage1ScorePopupY = y;
}

void armStage1ExtraCharlie(Game& game) {
  // $729D: $220A becomes 1. $7670-$7684 converts the next newly allocated
  // ordinary hoop record into the hanging Charlie. It never repurposes an
  // object already active in the course and never controls the $2760
  // small/prize-ring branch.
  if (game.level1ExtraCharlieState != 0) return;
  game.level1ExtraCharlieState = 1;
}

void finishStage(Game& game) {
  // Level 3 enters its goal state inside updateLevel3 ($9160).
  const bool stage2 = game.selectedEvent == 1;
  const bool stage3 = false;
  const bool stage4 = game.selectedEvent == 3;
  const float finishX = stage2 ? kStage2GoalX
                               : (stage4 ? kStage4CourseLength
                                         : kCourseLength);
  const float finishY = stage2 ? kStage2GoalTopY
                               : (stage4 ? kStage4GoalTopY
                                         : kGoalLandingY);
  if (stage2 || stage4) {
    game.player.position = {finishX, finishY};
    game.cameraX = finishX - (stage2 ? 340.0F : kStage4GoalScreenX);
  } else {
    // Level 1 keeps the course position of the landing frame ($79DA does not
    // move the scroll); only the row snaps onto the platform top.
    game.player.position.y = finishY;
    game.cameraX = std::max(0.0F, game.player.position.x - kLevel1PlayerStartX);
  }
  game.player.previous = game.player.position;
  game.player.runSpeed = 0.0F;
  game.player.verticalVelocity = 0.0F;
  game.player.jumpFrame = -1;
  game.player.grounded = true;
  game.previousCameraX = game.cameraX;
  const bool stage1 = !stage2 && !stage3 && !stage4;
  if (stage1) {
    // $220C: any missed bag or failure cancels the bird; the goal itself
    // awards no points on this board.
    game.perfectClear = game.level1MissedRewards == 0;
  } else {
    game.perfectClear =
        !stage2 && !game.deathOccurred && game.prizeBagsAvailable > 0 &&
        game.prizeBagsCollected == game.prizeBagsAvailable;
    game.score += stage2 ? 5000 : (stage3 ? 3000 : 4000);
  }
  game.goalFrame = 0;
  game.scene = Scene::Goal;
}

float stage2MonkeyY(const Stage2Monkey& monkey) {
  if (!monkey.leaping) return kStage2RopeY;
  constexpr int kLeapFrames = 54;
  const float progress = std::clamp(
      static_cast<float>(monkey.leapFrame) /
          static_cast<float>(kLeapFrames),
      0.0F, 1.0F);
  return kStage2RopeY - std::sin(progress * kPi) * 82.0F;
}

// ===========================================================================
// circusc4 Level 3: the trampoline stage ($8A6C-$97D7).
//
// The board keeps Charlie at column $50 while $2203:$2204 scrolls two
// columns per frame.  Every rebound is a fixed 8.8 vertical launch: $0420
// moving (44 frames = 88 columns, one drum), $03C0 for the 80-column gap
// between the third drum of a page and the first of the next, or the
// stationary series $0450/$0510/$0630/$0810 whose fourth apex is the roof.
// Fire breathers and jugglers spawn from $F517 at fixed scroll positions,
// bags hang at row $50 over seven drums, and the goal is the landing on
// column $A0-$C9 of page $F8.  See docs/LEVEL3_ROM_MODEL.md.
// ===========================================================================

int level3Progress(const Game& game) {
  return static_cast<int>((0x10000 - game.level3Scroll) & 0xffff);
}

std::uint8_t level3PageByte(const Game& game) {
  return static_cast<std::uint8_t>(game.level3Scroll >> 8);
}

std::uint8_t level3OffsetByte(const Game& game) {
  return static_cast<std::uint8_t>(game.level3Scroll & 0xff);
}

int level3PageIndex(const Game& game) {
  return static_cast<int>(static_cast<std::uint8_t>(-level3PageByte(game)));
}

std::uint8_t level3Abs8(std::uint8_t value) {
  return value >= 0x80 ? static_cast<std::uint8_t>(-value) : value;
}

// Drum n (0-based) is centred on world column 80 + 88 * (n % 3) + 256 * (n / 3).
int level3DrumCentre(int index) {
  return kLevel3DrumCentres[static_cast<std::size_t>(index % 3)] +
         256 * (index / 3);
}

constexpr int kLevel3DrumCount = 23;  // the last reachable drum is the goal

void syncLevel3World(Game& game) {
  const float progress = static_cast<float>(level3Progress(game));
  game.player.position = {
      (progress + static_cast<float>(game.level3X)) * kSourceToWorldX,
      level3RowToY(static_cast<float>(game.level3Y) + 16.0F)};
  game.cameraX = progress * kSourceToWorldX;
  game.player.runSpeed = game.level3Direction == 2
                             ? 2.0F * static_cast<float>(kBoardRefresh) * kSourceToWorldX
                             : (game.level3Direction == 1
                                    ? -2.0F * static_cast<float>(kBoardRefresh) * kSourceToWorldX
                                    : 0.0F);
  game.player.facingRight = game.level3Direction != 1;
  game.player.grounded = false;
  game.player.jumpFrame = game.level3PoseFrame;
}

// $6B32/$8C61: (re)initialise the board with the page byte kept from a
// restart ($2204 zeroed) and Charlie launched from the standing row.
void initializeLevel3Board(Game& game, int pageByte) {
  game.level3Scroll = static_cast<std::uint16_t>((pageByte & 0xff) << 8);
  game.level3State = 1;
  game.level3MissCued = false;
  game.level3Y = kLevel3StandRow;
  game.level3X = kLevel3PlayerColumn;
  game.level3Phase = 1;
  game.level3Velocity = 0x0420;
  game.level3Target = 0x0420;
  game.level3Bounce = 0;
  game.level3Direction = 0;
  game.level3Stick = 0;
  game.level3Countdown = 0;
  game.level3Pose = Level3Pose::Stationary;
  game.level3PoseFrame = 0;
  for (auto& performer : game.level3Performers) performer = Level3Performer{};
  for (auto& flame : game.level3Flames) flame = Level3Flame{};
  for (auto& knife : game.level3Knives) knife = Level3Knife{};
  for (auto& bag : game.level3Bags) bag = Level3Bag{};
  for (auto& coin : game.level3Coins) coin = Level3Coin{};
  game.level3TileTimer = 0;
  game.level3PressedDrum = -1;
  game.level3DeathKind = 0;
  game.level3RestartPage = pageByte;
  game.level3BonusTimeout = 0;
  game.level3BirdActive = false;
  game.level3BirdState = 0;
  game.level3BirdX = 0;
  game.level3BirdBagX = 0;
  game.level3CoinIndex = 0;
  game.level3CoinCount = 0;
  game.level3CoinStarted = false;
  game.level3PileFrame = 0;
  game.player.alive = true;
  game.player.verticalVelocity = 0.0F;
  game.goalFrame = 0;
}

// $8B93: a flame or knife hit.  The record freezes (state 4) and Charlie
// enters state 7: seven frames still, then a fall to row $D8.
void level3Hit(Game& game, int kind) {
  game.level3State = 7;
  // $BC8B (the bonus running out) only writes the state, so $2D stays zero
  // and the fall starts at once.
  game.level3Countdown = kind == 4 ? 0 : 7;
  game.level3Direction = 0;
  game.level3DeathKind = kind;
  game.player.alive = false;
  game.deathOccurred = true;
  // The music stops here and the knife or flame plays the miss at once;
  // the fail piece waits for the floor.  The bonus running out is silent
  // until the floor, where the miss plays first (as on Event 2).
  if (kind != 4) {
    ++game.stage3OverjumpAudioSerial;
    game.level3MissCued = true;
  }
}

// $8B3E/$8BA9: projectile collisions run before anything moves, so they use
// the previous frame's positions.
void level3Collisions(Game& game) {
  if (game.level3State >= 2 || !game.player.alive) return;
  if (game.level3Water) {
    // $8BF9: in the water round only the dolphins' bodies are tested:
    // |columns| < 11, |rows| < 8 and their sum < 17.
    for (auto& dolphin : game.level3Performers) {
      if (!dolphin.active) continue;
      const std::uint8_t dx = level3Abs8(static_cast<std::uint8_t>(
          static_cast<std::uint8_t>(game.level3X + 0x10) -
          static_cast<std::uint8_t>(dolphin.x + 0x10)));
      if (dx >= 0x0b) continue;
      const std::uint8_t dy = level3Abs8(static_cast<std::uint8_t>(
          game.level3Y - dolphin.y));
      if (dy >= 8) continue;
      if (static_cast<std::uint8_t>(dx + dy) >= 0x11) continue;
      if (game.level3Invulnerable) continue;
      dolphin.state = 4;   // $8BA2: frozen where it is ($97D4)
      level3Hit(game, 5);
      return;
    }
    return;
  }
  for (auto& flame : game.level3Flames) {
    if (!flame.active) continue;
    const std::uint8_t dx = level3Abs8(static_cast<std::uint8_t>(
        game.level3X + 0x10 - static_cast<std::uint8_t>(flame.x + 8)));
    if (dx >= 8) continue;
    const std::uint8_t dy = level3Abs8(static_cast<std::uint8_t>(
        static_cast<std::uint8_t>(flame.y - 4) - game.level3Y));
    if (dy >= 0x0a) continue;
    // $8B8C adds the stale $28FF (0 or 1) and tests < $10: always true.
    if (game.level3Invulnerable) continue;
    flame.state = 4;
    level3Hit(game, 1);
    return;
  }
  for (auto& knife : game.level3Knives) {
    if (!knife.active) continue;
    const std::uint8_t dx = level3Abs8(static_cast<std::uint8_t>(
        game.level3X + 0x10 - static_cast<std::uint8_t>(knife.x + 8)));
    if (dx >= 9) continue;
    const std::uint8_t dy = level3Abs8(static_cast<std::uint8_t>(
        game.level3Y - static_cast<std::uint8_t>(knife.y + 8)));
    if (dy >= 8) continue;
    if (game.level3Invulnerable) continue;
    knife.state = 4;
    level3Hit(game, 2);
    return;
  }
}

// $93F8-$9549: the first free performer record is the only candidate; it is
// filled when $2204 (before this frame's scroll) equals a slot key and the
// difficulty table has a performer for the current page and slot.
// $91DE: load the next pose of a dolphin script ($C), looping at its end.
// The capture logs $C, which points past the loaded pose.
void level3DolphinLoadPose(Level3Performer& dolphin, int script, int index) {
  dolphin.script = script;
  dolphin.scriptIndex = index;
  dolphin.scriptHold = kLevel3DolphinHolds[static_cast<std::size_t>(script)]
                                          [static_cast<std::size_t>(index)];
  if (script == kDolphinUnderWater) {
    // $EE66: a blank pose held one frame, then $EDFE's fourth pose for 129.
    dolphin.scriptHold = index == 0 ? 0 : 0x80;
  }
}

void level3DolphinAnimate(Level3Performer& dolphin) {
  if (dolphin.scriptHold != 0) {
    --dolphin.scriptHold;
    return;
  }
  const int count = dolphin.script == kDolphinUnderWater ? 2 : 5;
  level3DolphinLoadPose(dolphin, dolphin.script,
                        (dolphin.scriptIndex + 1) % count);
}

// $9735/$9731: the launch velocity of the next leap or dive, $0480 or $0420.
void level3DolphinSetLaunch(Level3Performer& dolphin, bool low) {
  if (!low) {
    dolphin.launch = 0x0480;
    dolphin.velocity = 0x0480;
  } else {
    dolphin.launch = static_cast<std::uint16_t>((dolphin.launch & 0xff00) | 0x20);
    dolphin.velocity = static_cast<std::uint16_t>((dolphin.velocity & 0xff00) | 0x20);
  }
}

// $94F0/$9500: fill a record with a dolphin: row $C8, column $F1, phase 1
// (leap) for a type-1 byte or 3 (dive) otherwise, the launch velocity and the
// slot number as the dive counter; slot 0 starts with the low leap.
void level3FillDolphin(Level3Performer& dolphin, int kind, int slot) {
  dolphin.active = true;
  dolphin.state = 3;
  dolphin.x = 0xf1;
  dolphin.y = 0xc8;
  dolphin.poseFrame = 0;
  level3DolphinSetLaunch(dolphin, false);
  dolphin.phase = kind == 1 ? 1 : 3;
  dolphin.counter = slot;
  int script = kind == 1 ? kDolphinLeap : kDolphinDive;
  if (slot == 0) {
    level3DolphinSetLaunch(dolphin, true);
    script = dolphin.phase == 1 ? kDolphinLeapLow : kDolphinDiveLow;
  }
  dolphin.script = script;
  dolphin.scriptIndex = -1;   // nothing loaded until the first $91DE
  dolphin.scriptHold = 0;
}

void level3SpawnPerformer(Game& game) {
  if (game.level3Direction == 0) return;
  // $93FB: dolphins only come while Charlie moves right.
  if (game.level3Water && game.level3Direction != 2) return;
  for (std::size_t index = 0; index < game.level3Performers.size(); ++index) {
    auto& performer = game.level3Performers[index];
    if (performer.active) continue;
    const std::uint8_t low = level3OffsetByte(game);
    int slot = -1;
    if (low == 0xc4) slot = 0;
    else if (low == 0x18) slot = 2;
    else if (low == 0x74) slot = 1;
    if (slot < 0) return;
    int difficulty = game.level3Difficulty +
                     (game.level3Visits == 0 ? 0 : (game.level3Visits - 1) * 2);
    if (game.level3Water) {
      // $958B: two more per bag threshold already passed in this game.
      for (const int threshold : kLevel3BagThresholds) {
        if (game.level3BagsTotal >= threshold) difficulty += 2;
      }
    }
    difficulty = std::min(difficulty, 10);
    int page = level3PageIndex(game);
    if (game.level3Direction == 1 && page != 0) --page;
    if (page > 8) return;
    const auto& tables = game.level3Water ? kLevel3WaterTables
                                          : kLevel3PerformerTables;
    const int byte = tables[static_cast<std::size_t>(difficulty / 2)]
                           [static_cast<std::size_t>(page)]
                           [static_cast<std::size_t>(slot)];
    if (byte == 0) return;
    const int kind = byte & 0x7f;
    performer.active = true;
    performer.x = 0xf1;
    performer.type = kind - 1;
    performer.remaining = kind - 1;
    int period = (game.level3Difficulty >> 1) +
                 (game.level3Visits == 0 ? 0 : game.level3Visits - 1);
    for (const int threshold : kLevel3BagThresholds) {
      if (game.level3BagsTotal >= threshold) ++period;
    }
    period = std::min(period, 9);
    performer.timerReload = kLevel3AttackPeriods[static_cast<std::size_t>(period)];
    performer.timer = performer.timerReload;
    performer.key = static_cast<std::uint8_t>(
        game.level3Visits == 0 ? 0 : game.level3Visits - 1);
    performer.pose = 0;
    performer.poseFrame = 0;
    if (!game.level3Water) return;
    // $9500: the dolphin ($38/$3C keep kind - 1 like the performers).
    level3FillDolphin(performer, kind, slot);
    if (kind == 3) {
      // $9533: a pair: the first free record (searched from the first) gets
      // a leaper in the same spot, without a type, timer or key of its own.
      for (auto& second : game.level3Performers) {
        if (second.active) continue;
        second = Level3Performer{};
        level3FillDolphin(second, 1, slot);
        break;
      }
    } else if ((byte & 0x80) != 0) {
      // $9556: moved under water and to the left edge, where it waits with
      // no velocity, invisible until its first wrap.
      performer.y = static_cast<std::uint8_t>(performer.y + (slot == 0 ? 0x25 : 0x2d));
      performer.x = static_cast<std::uint8_t>(performer.x + (slot == 0 ? 0x20 : 0x28));
      performer.phase = 4;
      performer.velocity = 0;
      performer.offset = true;
      level3DolphinLoadPose(performer, kDolphinUnderWater, 0);
    }
    return;
  }
}

void level3ClearPerformer(Level3Performer& performer) {
  performer = Level3Performer{};
}

// $95B7: attack timer, then the performer follows the scroll and retires
// once it wraps back to column $F1.
// $9799: a splash in the first free flame record (all four), four rows above
// the dolphin, for 32 frames.
void level3SpawnSplash(Game& game, const Level3Performer& dolphin) {
  for (auto& flame : game.level3Flames) {
    if (flame.active) continue;
    flame = Level3Flame{};
    flame.active = true;
    flame.state = 3;
    flame.y = static_cast<std::uint8_t>(dolphin.y - 4);
    flame.x = dolphin.x;
    flame.diving = dolphin.phase != 1;
    flame.lifetime = 0x20;
    return;
  }
}

// $969C: a dolphin.  Odd phases decelerate (a leap up, a dive down) until the
// velocity is spent; even phases accelerate back until the launch velocity
// returns, when the dive counter picks the next launch and a splash appears.
void level3UpdateDolphin(Game& game, Level3Performer& dolphin) {
  ++dolphin.poseFrame;
  if (!dolphin.offset) level3DolphinAnimate(dolphin);
  // $96B2
  const int phase = dolphin.phase;
  const std::uint8_t high = static_cast<std::uint8_t>(dolphin.velocity >> 8);
  if ((phase & 1) != 0) {
    // $9745
    dolphin.y = static_cast<std::uint8_t>(phase == 3 ? dolphin.y + high
                                                     : dolphin.y - high);
    bool spent = high == 0;
    if (!spent) {
      dolphin.velocity = static_cast<std::uint16_t>(dolphin.velocity - 0x30);
      spent = dolphin.velocity == 0;
    }
    if (spent) {
      dolphin.velocity &= 0xff00;
      ++dolphin.phase;
    }
  } else {
    dolphin.y = static_cast<std::uint8_t>(phase == 4 ? dolphin.y - high
                                                     : dolphin.y + high);
    dolphin.velocity = static_cast<std::uint16_t>(dolphin.velocity + 0x30);
    if (dolphin.velocity == dolphin.launch) {
      const std::uint8_t launchHigh = static_cast<std::uint8_t>(dolphin.launch >> 8);
      dolphin.y = static_cast<std::uint8_t>(phase == 4 ? dolphin.y - launchHigh
                                                       : dolphin.y + launchHigh);
      --dolphin.counter;
      int script;
      if (dolphin.counter == 0) {
        // $971D: the low leap.
        level3DolphinSetLaunch(dolphin, true);
        script = phase == 4 ? kDolphinLeapLow : kDolphinDiveLow;
      } else {
        if (dolphin.counter == -1) dolphin.counter = 2;
        level3DolphinSetLaunch(dolphin, false);
        script = phase == 4 ? kDolphinLeap : kDolphinDive;
      }
      dolphin.script = script;
      dolphin.scriptIndex = -1;
      dolphin.scriptHold = 0;
      dolphin.phase = phase + 1 >= 5 ? 1 : phase + 1;
      level3SpawnSplash(game, dolphin);
    }
  }
  // $9769: the dolphins swim left two columns a frame in the world: on the
  // first page or with Charlie stopped two on screen, none while he walks
  // back, four while he moves right.
  int step = -2;
  if (level3PageByte(game) != 0 && game.level3Direction != 0) {
    step = game.level3Direction == 1 ? 0 : -4;
  }
  dolphin.x = static_cast<std::uint8_t>(dolphin.x + step);
  if (dolphin.x >= 0xf1 && dolphin.x < 0xf4) {
    if (!dolphin.offset) {
      dolphin = Level3Performer{};
      return;
    }
    dolphin.offset = false;
  }
}

void level3UpdatePerformer(Game& game, std::size_t index) {
  auto& performer = game.level3Performers[index];
  if (!performer.active) return;
  if (performer.state == 3) {
    level3UpdateDolphin(game, performer);
    return;
  }
  if (performer.state == 4) return;   // a dolphin frozen by the hit
  ++performer.poseFrame;
  if (performer.type == 0) {
    performer.timer = static_cast<std::uint8_t>(performer.timer - 1);
    if (performer.timer == 0) {
      // $95F0: only the first three flame records are candidates.
      for (std::size_t slot = 0; slot < 3; ++slot) {
        auto& flame = game.level3Flames[slot];
        if (flame.active) continue;
        flame = Level3Flame{};
        flame.active = true;
        flame.y = static_cast<std::uint8_t>(kLevel3PerformerRow - 14);
        flame.x = static_cast<std::uint8_t>(performer.x + 4);
        flame.state = 0;
        flame.hold = 8;
        performer.pose = 1;
        performer.poseFrame = 0;
        performer.timer = performer.timerReload;
        break;
      }
    }
  } else if (performer.remaining != 0) {
    performer.timer = static_cast<std::uint8_t>(performer.timer - 1);
    if (performer.timer == 0) {
      for (auto& knife : game.level3Knives) {
        if (knife.active) continue;
        knife = Level3Knife{};
        knife.active = true;
        knife.y = static_cast<std::uint8_t>(kLevel3PerformerRow - 16);
        knife.x = performer.x;
        knife.state = 0;
        knife.hold = 8;
        knife.velocity = 0x0400;
        knife.owner = static_cast<int>(index);
        performer.pose = 2;
        performer.poseFrame = 0;
        performer.timer = performer.timerReload;
        --performer.remaining;
        break;
      }
    }
  }
  const std::uint8_t page = level3PageByte(game);
  if (page == 0 || page == 0xf8 || game.level3Direction == 0) return;
  performer.x = static_cast<std::uint8_t>(
      performer.x + (game.level3Direction == 1 ? 2 : -2));
  if (performer.x == 0xf1) level3ClearPerformer(performer);
}

// $9293: projectiles and bags follow the scroll; a column in [$F0,$F4)
// (wrapped past the left edge) retires the record.  Returns false when
// the record was cleared.
template <typename Record>
bool level3ScrollFollow(const Game& game, Record& record) {
  if (record.x >= 0xf0 && record.x < 0xf4) {
    record = Record{};
    return false;
  }
  const std::uint8_t page = level3PageByte(game);
  if (page == 0 || page == 0xf8 || game.level3Direction == 0) return true;
  record.x = static_cast<std::uint8_t>(
      record.x + (game.level3Direction == 1 ? 2 : -2));
  return true;
}

// $92E4: rise by the velocity high byte, minus $10 per frame; the apex is
// the first frame with a zero high byte, followed by an eight-frame hold.
template <typename Record>
bool level3Rise(Record& record) {
  if (!record.apex) {
    record.y = static_cast<std::uint8_t>(record.y - (record.velocity >> 8));
    if ((record.velocity >> 8) != 0) {
      record.velocity = static_cast<std::uint16_t>(record.velocity - 0x10);
      if (record.velocity != 0) return true;
    }
    record.velocity = 0;
    record.apex = true;
    record.hold = 8;
    return true;
  }
  record.hold = static_cast<std::uint8_t>(record.hold - 1);
  if (record.hold == 0) {
    record = Record{};
    return false;
  }
  return true;
}

// $9273: eight frames at the mouth, then a 100-row rise and an eight-frame
// hover before the record clears (65 frames in all).
void level3UpdateFlame(Game& game, Level3Flame& flame) {
  if (!flame.active) return;
  ++flame.age;
  if (flame.state == 3) {
    // $9320: a dolphin's splash, retired after its 32 frames or at the wrap.
    flame.lifetime = static_cast<std::uint8_t>(flame.lifetime - 1);
    if (flame.lifetime == 0) {
      flame = Level3Flame{};
      return;
    }
    level3ScrollFollow(game, flame);
    return;
  }
  if (flame.state == 0) {
    if (!level3ScrollFollow(game, flame)) return;
    flame.hold = static_cast<std::uint8_t>(flame.hold - 1);
    if (flame.hold == 0) {
      flame.state = 1;
      flame.apex = false;
      flame.velocity = 0x0400;
    }
  } else if (flame.state == 1) {
    if (!level3Rise(flame)) return;
    level3ScrollFollow(game, flame);
  }
}

void level3KnifeSway(Level3Knife& knife) {
  knife.sway = static_cast<std::uint8_t>(knife.sway + 0x20);
  if (knife.sway == 0) knife.x = static_cast<std::uint8_t>(knife.x + 1);
}

// $9343: rise ($0400 minus $10 per frame), fall until the velocity is back
// at $0400 (caught), sixteen frames in the hand, then thrown again.
void level3UpdateKnife(Game& game, Level3Knife& knife) {
  if (!knife.active) return;
  ++knife.age;
  auto ownerActive = [&]() {
    return knife.owner >= 0 &&
           game.level3Performers[static_cast<std::size_t>(knife.owner)].active;
  };
  if (knife.state == 0) {
    level3Rise(knife);
    if (!knife.active) return;
    level3KnifeSway(knife);
    if (!level3ScrollFollow(game, knife)) return;
    if (knife.apex) knife.state = 1;
  } else if (knife.state == 1) {
    level3KnifeSway(knife);
    if (!level3ScrollFollow(game, knife)) return;
    knife.y = static_cast<std::uint8_t>(knife.y + (knife.velocity >> 8));
    knife.velocity = static_cast<std::uint16_t>(knife.velocity + 0x10);
    if (knife.velocity == 0x0300) {
      if (!ownerActive()) {
        knife = Level3Knife{};
        return;
      }
      auto& owner = game.level3Performers[static_cast<std::size_t>(knife.owner)];
      owner.pose = 3;
      owner.poseFrame = 0;
    }
    if ((knife.velocity >> 8) == 4) {
      knife.apex = false;
      knife.velocity = 0x0400;
      if (ownerActive() &&
          game.level3Performers[static_cast<std::size_t>(knife.owner)].type != 0) {
        const auto& owner = game.level3Performers[static_cast<std::size_t>(knife.owner)];
        knife.y = static_cast<std::uint8_t>(kLevel3PerformerRow - 16);
        knife.x = owner.x;
        knife.hold = 0x10;
        knife.state = 2;
      } else {
        knife = Level3Knife{};
      }
    } else if (!ownerActive()) {
      knife = Level3Knife{};
    }
  } else if (knife.state == 2) {
    if (!level3ScrollFollow(game, knife)) return;
    if (!ownerActive()) {
      knife = Level3Knife{};
      return;
    }
    knife.hold = static_cast<std::uint8_t>(knife.hold - 1);
    if (knife.hold == 0) knife.state = 0;
  }
}

// $929D/$7DBD: bags follow the scroll; one that wraps to column $F2/$F4
// while moving right counts as missed ($220A).
void level3BagFollow(Game& game, Level3Bag& bag) {
  const std::uint8_t page = level3PageByte(game);
  if (!(page == 0 || page == 0xf8 || game.level3Direction == 0)) {
    bag.x = static_cast<std::uint8_t>(
        bag.x + (game.level3Direction == 1 ? 2 : -2));
  }
  if (bag.x == 0xf2 || bag.x == 0xf4) {
    if (game.level3Direction != 1) ++game.level3Missed;
    bag = Level3Bag{};
  }
}

void level3UpdateBag(Game& game, Level3Bag& bag) {
  if (!bag.active) return;
  ++bag.age;
  if (bag.state == 3) {
    level3BagFollow(game, bag);
  } else if (bag.state == 4) {
    bag.timer = static_cast<std::uint8_t>(bag.timer - 1);
    if (bag.timer == 0) {
      bag = Level3Bag{};
      return;
    }
    level3BagFollow(game, bag);
  }
}

// $8AA8: after the scroll update, moving right, the first $FA43 entry whose
// page byte matches must also match the offset byte exactly.
void level3SpawnBag(Game& game) {
  if (game.level3Direction != 2) return;
  const int limit = game.level3Visits < 2 ? 0x20 : 0x40;
  if (limit < game.level3BagsTotal) return;
  const std::uint8_t high = level3PageByte(game);
  const std::uint8_t low = level3OffsetByte(game);
  bool matched = false;
  for (const auto& spawn : kLevel3BagSpawns) {
    if (spawn.first != high) continue;
    matched = spawn.second == low;
    break;
  }
  if (!matched) return;
  for (const auto& bag : game.level3Bags) {
    if (bag.active && bag.key == low) return;
  }
  for (auto& bag : game.level3Bags) {
    if (bag.active) continue;
    bag = Level3Bag{};
    bag.active = true;
    bag.key = low;
    bag.state = 3;
    bag.x = 0xf0;
    bag.y = static_cast<std::uint8_t>(kLevel3BagRow);
    return;
  }
}

// $8E5A-$8ED1: on the third stationary rebound, while rising through rows
// $50-$67, a hanging bag within reach becomes a score popup worth
// (index + 3) * 100, capped at 900.
void level3CollectBag(Game& game) {
  if (game.level3Y < 0x50 || game.level3Y >= 0x68) return;
  for (auto& bag : game.level3Bags) {
    if (!bag.active || bag.state != 3) continue;
    if (level3Abs8(static_cast<std::uint8_t>(bag.x - game.level3X)) >= 0x10) continue;
    const std::uint8_t dx = level3Abs8(static_cast<std::uint8_t>(
        static_cast<std::uint8_t>(bag.x + 8) -
        static_cast<std::uint8_t>(game.level3X + 0x10)));
    if (dx >= 0x13) continue;
    const int valueIndex = std::min(game.level3BagValueIndex + 2, 8);
    bag.timer = 0x20;
    bag.state = 4;
    bag.value = (valueIndex + 1) * 100;
    bag.age = 0;
    ++game.level3BagValueIndex;
    ++game.level3BagsTotal;
    game.score += bag.value;
    ++game.prizeBagsCollected;
    ++game.prizeBagAudioSerial;
    return;
  }
}

// $9160/$7F14: the goal landing.  Charlie hops onto the last drum, the
// crowd callouts appear for 160 frames and, with no bag missed or still
// hanging, the bird brings the coin shower in from the left.
void level3EnterGoal(Game& game) {
  game.level3Y = static_cast<std::uint8_t>(game.level3Y - 10);
  game.level3X = static_cast<std::uint8_t>(game.level3X + 4);
  game.level3State = 4;
  game.level3Countdown = 0xa0;
  game.level3Pose = Level3Pose::Cheer;
  game.level3PoseFrame = 0;
  game.goalFrame = 0;
  game.scene = Scene::Goal;
  game.perfectClear = false;
  if (game.level3Missed != 0) return;
  for (const auto& bag : game.level3Bags) {
    if (bag.active && bag.state == 3) {
      ++game.level3Missed;
      return;
    }
  }
  game.perfectClear = true;
  game.level3BirdActive = true;
  game.level3BirdState = 3;
  game.level3BirdX = 0xe8;
  game.level3BirdBagX = 0xf0;
}

// $8D9D + $8DE7: scroll or walk, then the vertical rebound.
void level3MovePlayer(Game& game) {
  const std::uint8_t page = level3PageByte(game);
  if (game.level3Direction == 2) {
    if (page == 0xf8) {
      if (game.level3X < 0xc0) game.level3X = static_cast<std::uint8_t>(game.level3X + 2);
    } else {
      game.level3Scroll = static_cast<std::uint16_t>(game.level3Scroll - 2);
    }
  } else if (game.level3Direction == 1) {
    if (page != 0) {
      if (page == 0xf8 && game.level3X != kLevel3PlayerColumn) {
        game.level3X = static_cast<std::uint8_t>(game.level3X - 2);
      } else {
        game.level3Scroll = static_cast<std::uint16_t>(game.level3Scroll + 2);
      }
    }
  }
  if (game.level3Phase == 0) return;
  if (game.level3Phase == 2) {
    // Falling ($8DEE).
    game.level3Y = static_cast<std::uint8_t>(game.level3Y + (game.level3Velocity >> 8));
    game.level3Velocity = static_cast<std::uint16_t>(game.level3Velocity + 0x30);
    if (game.level3Velocity != game.level3Target) return;
    game.level3Y = static_cast<std::uint8_t>(game.level3Y + (game.level3Target >> 8));
    // $8ED2: twenty points for every landing while moving right.
    if (game.level3Direction == 2) game.score += 20;
    // $8F8D: the drum under Charlie shows its pressed tiles for 8 frames.
    game.level3TileTimer = 8;
    {
      const int charlieWorld = level3Progress(game) + game.level3X;
      int nearest = 0;
      int nearestDistance = 1 << 30;
      for (int index = 0; index < kLevel3DrumCount + 1; ++index) {
        const int distance = std::abs(level3DrumCentre(index) - charlieWorld);
        if (distance < nearestDistance) {
          nearestDistance = distance;
          nearest = index;
        }
      }
      game.level3PressedDrum = nearest;
    }
    // $8F54: the next rebound takes the joystick sampled this frame.
    game.level3Phase = 1;
    game.level3Direction = game.level3Stick;
    game.level3Pose = game.level3Stick == 2 ? Level3Pose::MovingRight
                      : (game.level3Stick == 1 ? Level3Pose::MovingLeft
                                               : Level3Pose::Stationary);
    game.level3PoseFrame = 0;
    ++game.stage3BounceAudioSerial;
    // $8EFC: launch velocity.
    const std::uint8_t low = level3OffsetByte(game);
    if (game.level3Direction != 0) {
      game.level3Velocity = game.level3Target = 0x0420;
      game.level3Bounce = 0;
      const bool shortHop = game.level3Direction == 1
                                ? (low >= 0xf8 || low < 0x08)
                                : (low >= 0x48 && low < 0x58);
      if (shortHop) game.level3Velocity = game.level3Target = 0x03c0;
    } else {
      const int index = std::min(game.level3Bounce, 4);
      ++game.level3Bounce;
      game.level3Velocity = game.level3Target =
          kLevel3StationaryLaunch[static_cast<std::size_t>(index)];
    }
    // $9160: the goal.
    if (page == 0xf8 && game.level3X >= 0xa0 && game.level3X < 0xca) {
      level3EnterGoal(game);
    }
    return;
  }
  // Rising ($8E1D).
  game.level3Y = static_cast<std::uint8_t>(game.level3Y - (game.level3Velocity >> 8));
  bool apex = false;
  if (game.level3Bounce != 0 && (game.level3Velocity >> 8) == 0) {
    apex = true;
  } else {
    game.level3Velocity = static_cast<std::uint16_t>(game.level3Velocity - 0x30);
    if (game.level3Velocity == 0) apex = true;
  }
  if (apex) {
    game.level3Velocity = 0;
    game.level3Phase = 2;
    if (game.level3Bounce == 4) {
      // $8E43: the fourth stationary apex meets the roof.
      game.level3Y = static_cast<std::uint8_t>(game.level3Y + 2);
      game.level3State = 8;
      game.level3Countdown = kLevel3FallenFrames;
      game.level3Pose = Level3Pose::Roof;
      game.level3PoseFrame = 0;
      game.level3DeathKind = 3;
      game.player.alive = false;
      game.deathOccurred = true;
      // The roof is the floor of this fall: the miss, then the fail piece.
      game.floorFallMiss = true;
      ++game.floorFallAudioSerial;
    }
    return;
  }
  if (game.level3Bounce == 3) level3CollectBag(game);
}

// $8FD7: the celebration lasts 160 frames and then waits for $220A, which
// the coin shower raises after forty scoring coins.
void level3UpdateGoal(Game& game) {
  ++game.goalFrame;
  game.level3Countdown = (game.level3Countdown - 1) & 0xff;
  if (game.level3Countdown != 0) return;
  if (game.level3Missed != 0) {
    game.scene = Scene::Tally;
    game.tallyFrame = 0;
    return;
  }
  game.level3Countdown = 1;
}

// $9015 (state 7) and $8509 (state 8).
void level3UpdateDeath(Game& game) {
  if (game.level3State == 7) {
    if (game.level3Countdown != 0) {
      --game.level3Countdown;
      game.level3Velocity &= 0xff;
      return;
    }
    game.level3Velocity = static_cast<std::uint16_t>(game.level3Velocity + 0x20);
    game.level3Y = static_cast<std::uint8_t>(game.level3Y + (game.level3Velocity >> 8));
    if (game.level3Y >= 0xd8) {
      game.level3State = 8;
      game.level3Countdown = kLevel3FallenFrames;
      game.level3Pose = Level3Pose::Fallen;
      game.level3PoseFrame = 0;
      // The fail piece; the miss too unless the hit already played it.
      game.floorFallMiss = !game.level3MissCued;
      game.level3MissCued = false;
      ++game.floorFallAudioSerial;
    }
    return;
  }
  if (game.level3State != 8) return;
  --game.level3Countdown;
  if (game.level3Countdown > 0) return;
  // $8517-$8525: one page back, two from page byte $F9/$F8.
  int pageByte = level3PageByte(game);
  if (pageByte != 0) {
    if (pageByte < 0xfa) pageByte = (pageByte + 1) & 0xff;
    pageByte = (pageByte + 1) & 0xff;
  }
  game.level3RestartPage = pageByte;
  game.level3Died = true;
  game.scene = Scene::Crashed;
  game.crashFrame = 0;
  // The board's $28 frames on the floor are done; the rest of the miss and
  // the fail piece run through the crash scene, so the stage restarts as
  // the piece ends (the board's own 96 frames in replays and silent builds).
  game.crashDurationFrames = kLevel3RestartFrames;
  if (game.failPieceFrames > 0) {
    game.crashDurationFrames = std::max(
        1, game.failPieceFrames + (game.floorFallMiss ? kFloorMissFrames : 0) -
               kLevel3FallenFrames);
  }
  --game.lives;
}

// $7E67/$7E7B/$7F3B: the bird carries the bag from the left edge to column
// $B4 (196 frames), then a coin leaves the bag every eighth frame.
void level3UpdatePresentation(Game& game) {
  if (!game.level3BirdActive) return;
  if (game.level3BirdState == 3) {
    game.level3BirdX = static_cast<std::uint8_t>(game.level3BirdX + 1);
    game.level3BirdBagX = static_cast<std::uint8_t>(game.level3BirdBagX + 1);
    if (game.level3BirdBagX == 0xb4) game.level3BirdState = 4;
  }
  if (game.level3CoinStarted) ++game.level3PileFrame;
  for (auto& coin : game.level3Coins) {
    if (!coin.active || coin.bagCopy) continue;
    ++coin.age;
    coin.yFraction = static_cast<std::uint8_t>(coin.yFraction + 8);
    if (coin.yFraction < 8) ++coin.yVelocity;
    coin.y = static_cast<std::uint8_t>(coin.y + coin.yVelocity);
    // $7E8E: the fraction carry moves the coin one column sideways.
    const std::uint8_t previous = coin.xFraction;
    coin.xFraction = static_cast<std::uint8_t>(coin.xFraction + coin.xSpeed);
    if (coin.xFraction < previous) {
      coin.x = static_cast<std::uint8_t>(coin.x + (coin.xNegative ? -1 : 1));
    }
    if (coin.y >= 0xb0) coin = Level3Coin{};
  }
}

void level3SpawnCoin(Game& game) {
  if (!game.level3BirdActive || game.level3BirdState != 4) return;
  if ((game.level1BoardFrameByte & 7) != 0) return;
  Level3Coin* free = nullptr;
  for (auto& coin : game.level3Coins) {
    if (!coin.active) {
      free = &coin;
      break;
    }
  }
  if (!free) return;
  const auto lane = kLevel3CoinLanes[static_cast<std::size_t>(game.level3CoinIndex)];
  *free = Level3Coin{};
  free->active = true;
  free->x = static_cast<std::uint8_t>(lane.first + game.level3BirdBagX);
  free->xNegative = lane.second >= 0x80;
  free->xSpeed = free->xNegative ? static_cast<std::uint8_t>(-lane.second) : lane.second;
  free->y = static_cast<std::uint8_t>(kLevel3BagRow);
  if (!game.level3CoinStarted) {
    // $7F94: the bag opens, the coin pile records take bag slots 1 and 2 and
    // the bag record is copied into coin slot 8, which stays occupied.
    game.level3CoinStarted = true;
    game.level3PileFrame = 0;
    for (std::size_t index = 1; index <= 2; ++index) {
      auto& pile = game.level3Bags[index];
      pile = Level3Bag{};
      pile.active = true;
      pile.state = 6;
      pile.x = static_cast<std::uint8_t>(game.level3BirdX + (index == 1 ? 0 : 0x10));
      pile.y = 0xac;
    }
    auto& copy = game.level3Coins[8];
    copy = Level3Coin{};
    copy.active = true;
    copy.bagCopy = true;
    copy.x = game.level3BirdBagX;
    copy.y = static_cast<std::uint8_t>(kLevel3BagRow);
    ++game.level3ShowerAudioSerial;
    return;
  }
  game.level3CoinIndex = (game.level3CoinIndex + 1) % 13;
  const int value = std::min(game.level3Level2Visits, 5);
  game.score += value > 0 ? value * 100 : 20;
  ++game.rewardCoinsAwarded;
  ++game.level3CoinCount;
  if (game.level3CoinCount >= 0x28) ++game.level3Missed;
}

void updateLevel3(Game& game, const Uint8* keyboard, float controllerAxis) {
  game.player.previous = game.player.position;
  game.previousCameraX = game.cameraX;
  if (game.stage1ScorePopupFrame > 0) --game.stage1ScorePopupFrame;

  const bool moveLeft = keyboard[SDL_SCANCODE_LEFT] ||
                        controllerAxis < -0.35F;
  const bool moveRight = keyboard[SDL_SCANCODE_RIGHT] ||
                         controllerAxis > 0.35F;

  if (game.level3TileTimer > 0) --game.level3TileTimer;
  ++game.level3PoseFrame;

  // $8B3E/$8BA9
  level3Collisions(game);
  // $923E
  level3SpawnPerformer(game);
  for (std::size_t index = 0; index < game.level3Performers.size(); ++index) {
    level3UpdatePerformer(game, index);
  }
  for (auto& flame : game.level3Flames) level3UpdateFlame(game, flame);
  for (auto& knife : game.level3Knives) level3UpdateKnife(game, knife);
  // $7D65
  for (auto& bag : game.level3Bags) level3UpdateBag(game, bag);
  // $8C50
  if (game.level3State == 1) {
    // $8532: the stick is latched every frame; $8F54 reads it at a landing.
    game.level3Stick = resolveStick((moveLeft ? 1 : 0) | (moveRight ? 2 : 0),
                                    game.level3Stick, game.level3BothStick);
    level3MovePlayer(game);
  } else if (game.level3State == 4) {
    level3UpdateGoal(game);
  } else if (game.level3State == 7 || game.level3State == 8) {
    level3UpdateDeath(game);
  }
  // $7DF2/$7F3B
  level3UpdatePresentation(game);
  level3SpawnCoin(game);
  // $8AA8
  level3SpawnBag(game);
  if (game.level3ClearProjectiles) {
    for (auto& flame : game.level3Flames) flame = Level3Flame{};
    for (auto& knife : game.level3Knives) knife = Level3Knife{};
  }

  // $BB73/$BC12: the bonus counts down every frame except during the
  // celebration; sixty-two frames after it reaches zero Charlie is dropped
  // like a projectile hit.
  if (game.level3State != 4) {
    if (game.bonus > 0) --game.bonus;
    if (game.bonus == 0 && game.level3State == 1) {
      // $BC64/$BC6D: the countdown ($2263, from $40) starts in the frame the
      // digits reach zero; the frame that reads two drops Charlie.
      if (game.level3BonusTimeout == 0) game.level3BonusTimeout = 0x40;
      const int before = game.level3BonusTimeout--;
      if (before == 2) level3Hit(game, 4);
    }
  }

  syncLevel3World(game);
  if (game.scene == Scene::Crashed) {
    game.player.previous = game.player.position;
    game.previousCameraX = game.cameraX;
  }
}

// ---- Event 4 board setup --------------------------------------------------

float level4RowToY(float row) { return level3RowToY(row); }

// The middle cell of a plaque names the distance ($EB5C-$EBBC).
std::string level4PlaqueText(int cell) {
  switch (cell) {
    case 0xb8: return "START";
    case 0xc5: return "60M";
    case 0xc1: return "50M";
    case 0xc0: return "40M";
    case 0xbf: return "30M";
    case 0xbe: return "20M";
    case 0xbd: return "10M";
    case 0xc2: return "GOAL";
    default: return {};
  }
}

void level4Animate(Level4Record& record);
void level4SetScript(Level4Record& record, int script);

Level4Record& level4BallRecord(Game& game, int owner) {
  if (owner < 0) return game.level4Ball;
  return game.level4Rolls[static_cast<std::size_t>(owner)];
}

// $8790/$8794: fill free object slots with one callout cell each.
template <std::size_t N>
void level4SpawnCallouts(Game& game,
                         const std::array<std::pair<int, int>, N>& cells,
                         int kind) {
  std::size_t cell = 0;
  for (auto& popup : game.level4Popups) {
    if (cell >= cells.size()) return;
    if (popup.active != 0) continue;
    popup.active = 1;
    popup.state = 2;
    popup.row = cells[cell].first;
    popup.col = cells[cell].second;
    popup.cell = static_cast<int>(cell);
    popup.kind = kind;
    popup.owner = -1;
    ++cell;
  }
}

// $82BA: a plaque re-enters from the right edge when the scroll reaches its
// trigger column.
void level4Plaques(Game& game) {
  int page = (-((game.level4Scroll >> 8) & 0xff)) & 0xff;
  if (game.level4Charlie.direction != 1) ++page;
  page = std::clamp(page, 0, 9);
  const int low = game.level4Scroll & 0xff;
  for (std::size_t i = 0; i < kLevel4PlaqueTriggers.size(); ++i) {
    if (kLevel4PlaqueTriggers[i] != low) continue;
    for (auto& plaque : game.level4Plaques) {
      if (plaque.trigger != kLevel4PlaqueTriggers[i]) continue;
      plaque.col = 0xf0;
      plaque.row = 0xe0;
      plaque.cell = kLevel4PlaqueCells[static_cast<std::size_t>(page)][i];
      break;
    }
    break;
  }
}

// $8267: the plaques enter the stage already scrolled off to the left.
void level4InitPlaques(Game& game) {
  int page = (-((game.level4Scroll >> 8) & 0xff)) & 0xff;
  ++page;
  page = std::clamp(page, 0, 9);
  for (std::size_t i = 0; i < kLevel4PlaqueTriggers.size(); ++i) {
    Level4Plaque& plaque = game.level4Plaques[i];
    plaque.trigger = kLevel4PlaqueTriggers[i];
    plaque.col = (-(kLevel4PlaqueTriggers[i] + 0x10)) & 0xff;
    plaque.row = 0xe0;
    plaque.cell = kLevel4PlaqueCells[static_cast<std::size_t>(page)][i];
  }
}

// $9883 plus $BB25: start (or restart) the stage on the given page.
void initializeLevel4Board(Game& game, int page) {
  game.level4Scroll = (page << 8) & 0xffff;
  game.level4Charlie = Level4Record{};
  game.level4Ball = Level4Record{};
  for (auto& roll : game.level4Rolls) roll = Level4Record{};
  for (auto& popup : game.level4Popups) popup = Level4Popup{};
  game.level4Flash = 0;
  game.level4Standing = 0;
  game.level4Pending = 0;
  game.level4PendingLead = -1;
  game.level4PendingTrail = -1;
  game.level4Buffered = 0;
  game.level4Held = 0;
  game.level4BonusHold = 0;
  game.level4Timeout = 0x40;
  game.level4Falls = 0;
  game.level4Doubles = 0;
  game.level4Landings = 0;
  game.level4Died = false;
  game.level4Finished = false;
  if (page == 0 && !game.level4Started) {
    game.bonus = kLevel4FirstBonus;
    game.level4Started = true;
  } else if (page == 0 || page >= 0xfe) {
    game.bonus = kLevel4RestartBonus[0];
  } else if (page >= 0xfc) {
    game.bonus = kLevel4RestartBonus[1];
  } else {
    game.bonus = kLevel4RestartBonus[2];
  }
  level4InitPlaques(game);
  Level4Record& ball = game.level4Ball;
  ball.active = 1;
  ball.state = 5;
  ball.row = kLevel4BallRow;
  ball.col = kLevel4StartColumn;
  Level4Record& charlie = game.level4Charlie;
  charlie.active = 1;
  charlie.state = 1;
  charlie.row = kLevel4CharlieRow;
  charlie.col = ball.col;
  charlie.launch = 4;
  charlie.landVelocity = 4;
  charlie.velocity = 0x0420;
  level4SetScript(charlie, kLevel4ScriptStanding);
}

// The board rows and columns become world pixels for the renderer.
void syncLevel4World(Game& game) {
  const int progress = (-game.level4Scroll) & 0xffff;
  const float scale = kWorldWidth / 224.0F;
  game.cameraX = static_cast<float>(progress) * scale;
  game.player.position = {
      static_cast<float>(progress + game.level4Charlie.col) * scale,
      level3RowToY(static_cast<float>(game.level4Charlie.row))};
  game.player.grounded = game.level4Charlie.phase == 0;
  game.player.facingRight = game.level4Charlie.direction != 1;
  game.player.alive = game.level4Charlie.state != 7 &&
                      game.level4Charlie.state != 8 &&
                      game.level4Charlie.state != 2;
}

// ---- circusc4 Event 4 (rolling balls) -------------------------------------
// The stage handler lives at $97D8-$9F0C.  The board keeps Charlie ($2400),
// the ball he rides ($2440), four rolling balls ($2480-$2540), twelve object
// slots ($2600-$26B0) and three distance plaques ($26C0-$26E0).  Everything
// below follows those records field by field; docs/LEVEL4_ROM_MODEL.md
// explains the model and lists the captures that verify it.

void level4ClearRecord(Level4Record& record) { record = Level4Record{}; }

void level4ClearPopup(Level4Popup& popup) {
  // $7D96 clears only ,X and $6,X.  The state, the flag and the timer of a
  // released slot survive into whatever object claims it next.
  popup.active = 0;
  popup.col = 0;
}

// $87CE: hold the current cell while $B,X counts down, then take the next
// step of the script.
void level4Animate(Level4Record& record) {
  if (record.animation > 0) {
    --record.animation;
    return;
  }
  int length = 0;
  const Level4Pose* script = level4Script(record.script, length);
  record.pose = (record.pose + 1) % length;
  record.animation = script[record.pose].delay;
}

// The board clears $B,X whenever it stores a new script pointer, so the next
// $87CE call shows the script's first cell.
void level4SetScript(Level4Record& record, int script) {
  record.script = script;
  record.pose = -1;
  record.animation = 0;
}

// $8347/$8354: the record's other cells mirror the first, so only the
// leading row and column are kept here.

// $6250 with the $D140 value table.
void level4AddScore(Game& game, int points) { game.score += points; }

// ---- $97F1: the ridden ball against the four rolling balls ----------------

void level4BallCollisions(Game& game) {
  Level4Record& ball = game.level4Ball;
  if (ball.active == 0 || ball.state >= 3) return;
  for (auto& roll : game.level4Rolls) {
    if (roll.active == 0) continue;
    int delta = ((ball.col + 0x10) & 0xff) - ((roll.col + 0x10) & 0xff);
    delta = static_cast<std::int8_t>(delta & 0xff);
    if (std::abs(delta) >= 0x1c) continue;
    // $9825: both balls are knocked out of their rolling states.  $FAC3
    // maps the ridden ball's state to the state the roller takes.
    static constexpr std::array<int, 3> kStruckStates{5, 7, 6};
    roll.state = kStruckStates[static_cast<std::size_t>(ball.state)];
    ball.state = ball.state == 1 ? 7 : 6;
    ball.active = 0;
    ball.flag = 1;
    ball.animation = 0;
    ++game.stage4BallCollisionAudioSerial;
    Level4Record& charlie = game.level4Charlie;
    if (charlie.phase != 0) return;
    charlie.state = 7;
    charlie.countdown = 0x20;
    charlie.velocity = 0;
    level4SetScript(charlie, kLevel4ScriptStruck);
    ++game.level4Falls;
    // The music stops here; the miss and the fail piece wait for the floor.
    return;
  }
}

// ---- $9D51: rolling ball spawning and motion ------------------------------

// $9D86-$9DAF plus $9E2D: which of the six $F1B5 schedules is in force.
int level4DifficultyIndex(const Game& game) {
  int index = (game.level4Dips & 0x60) >> 4;
  if (game.level4Visits != 0) index += (game.level4Visits - 1) * 2;
  index &= 0xff;
  static constexpr std::array<int, 8> kThresholds{0x14, 0x18, 0x1c, 0x20,
                                                  0x24, 0x28, 0x2c, 0x30};
  for (const int threshold : kThresholds) {
    if (game.level4Landings < threshold) break;
    index += 2;
  }
  return std::min(index, 0x0a);
}

int level4SchedulePage(const Game& game) {
  return (-((game.level4Scroll >> 8) & 0xff)) & 0xff;
}

// The five spawn columns of the current page, highest first.  The ROM
// indexes $F1B5 by a byte offset, so the six tables sit at even indices.
const std::array<int, 5>& level4PageSpawns(const Game& game) {
  const std::size_t table =
      static_cast<std::size_t>(level4DifficultyIndex(game) / 2);
  const std::size_t page =
      static_cast<std::size_t>(std::min(level4SchedulePage(game), 8));
  return kLevel4SpawnLists[static_cast<std::size_t>(
      kLevel4SchedulePages[table][page])];
}

// $9DEF: a ball may only enter when no other roller is near the right edge.
bool level4SpawnAllowed(const Game& game) {
  for (const auto& roll : game.level4Rolls) {
    if (roll.active == 0 && roll.flag == 0) continue;
    if (roll.col < 0x14) continue;
    int delta = static_cast<std::int8_t>((0xf1 - roll.col) & 0xff);
    if (std::abs(delta) <= 0x24) return false;
  }
  return true;
}

void level4Spawn(Game& game, Level4Record& roll) {
  game.level4Standing = 0;
  if (!level4SpawnAllowed(game)) return;
  roll.active = (roll.active + 1) & 0xff;
  roll.row = 0xc0;
  roll.col = 0xf1;
  roll.animation = 0;
  roll.half = 0;
}

// $9D73, once per slot until one of them answers.
void level4RollSpawn(Game& game) {
  const int direction = game.level4Charlie.direction;
  if (direction == 1) return;
  if (direction == 0) {
    // $9E3D: standing still, one ball rolls in every 256 frames.
    game.level4Standing = (game.level4Standing + 1) & 0xff;
    if (game.level4Standing != 0) return;
    for (auto& roll : game.level4Rolls) {
      if (roll.active == 0) {
        level4Spawn(game, roll);
        return;
      }
    }
    return;
  }
  for (auto& roll : game.level4Rolls) {
    if (roll.active != 0 || roll.flag != 0) continue;
    const std::array<int, 5>& spawns = level4PageSpawns(game);
    const int low = game.level4Scroll & 0xff;
    for (std::size_t i = 0; i < spawns.size(); ++i) {
      const int value = spawns[i];
      if (i != 0 && value == 0) return;
      if (value == low) {
        level4Spawn(game, roll);
        return;
      }
      if (i == 0 ? low > value : value < low) return;
    }
    return;
  }
}

// $9E51 with the $FAF2 state table.
void level4RollTail(Level4Record& roll) {
  if (roll.col == 0xf3 || roll.col == 0xf2) roll.state = 2;
}

void level4RollUpdate(Game& game) {
  const int direction = game.level4Charlie.direction;
  const bool walking = ((game.level4Scroll >> 8) & 0xff) == 0xf8;
  for (auto& roll : game.level4Rolls) {
    if (roll.active == 0 && roll.flag == 0) continue;
    switch (roll.state) {
      case 0: {  // $9E5F rolling toward Charlie
        level4Animate(roll);
        if (direction == 1) {
          roll.animation = 0;
          roll.state = (roll.state + 1) & 0xff;
          break;
        }
        const int half = (roll.half + 0x80) & 0xff;
        if (half == 0) roll.col = (roll.col - 1) & 0xff;
        roll.half = half;
        if (direction != 0 && !walking) roll.col = (roll.col - 1) & 0xff;
        level4RollTail(roll);
        break;
      }
      case 1: {  // $9E9B Charlie backtracks, the ball drifts away
        level4Animate(roll);
        if (direction != 1) {
          roll.animation = 0;
          roll.half = 0;
          roll.state = (roll.state - 1) & 0xff;
          break;
        }
        const int half = (roll.half + 0x80) & 0xff;
        if (half == 0) roll.col = (roll.col + 1) & 0xff;
        roll.half = half;
        level4RollTail(roll);
        break;
      }
      case 2:  // $9EBE the slot is wiped
        level4ClearRecord(roll);
        break;
      case 3:  // $9ED9 knocked to the right
        level4Animate(roll);
        level4Animate(roll);
        roll.col = (roll.col + 2) & 0xff;
        if (roll.col >= 0xf0) level4ClearRecord(roll);
        break;
      case 4:  // $9EEE knocked to the left
        level4Animate(roll);
        level4Animate(roll);
        roll.col = (roll.col - 2) & 0xff;
        if (roll.col >= 0xf0 && roll.col < 0xf2) level4ClearRecord(roll);
        break;
      case 6:  // $9F07 one cell per frame to the right
        level4Animate(roll);
        roll.col = (roll.col + 2) & 0xff;
        if (roll.col >= 0xf0) level4ClearRecord(roll);
        break;
      case 7:  // $9F0A one cell per frame to the left
        level4Animate(roll);
        roll.col = (roll.col - 2) & 0xff;
        if (roll.col >= 0xf0 && roll.col < 0xf2) level4ClearRecord(roll);
        break;
      default:
        break;
    }
  }
}

// ---- $8532: the joystick and the jump edge --------------------------------

void level4LatchInput(Game& game, int stick, int button) {
  Level4Record& charlie = game.level4Charlie;
  charlie.stick = stick & 0x03;
  game.level4LatchedButton = button;
  if (game.level4Buffered != 0) {
    // $8580: a jump buffered during the fall is discarded on the landing.
    if (charlie.phase != 0) return;
    game.level4Buffered = 0;
    return;
  }
  if (game.level4Held != 0) {
    if (button == 0) {
      game.level4Held = 0;
    } else {
      charlie.jump = 0;
    }
    return;
  }
  charlie.jump = button;
  if (button == 0) return;
  game.level4Held = (game.level4Held + 1) & 0xff;
  if (charlie.phase == 2 && charlie.row >= 0x68) game.level4Buffered = button;
}

// ---- $9A42/$9B10: the landing, its 100 points and the close-ball bonus ----

void level4SpawnDoublePopup(Game& game, int owner) {
  const Level4Record& source = level4BallRecord(game, owner);
  int first = -1;
  for (int i = 0; i < 4; ++i) {
    if (game.level4Popups[static_cast<std::size_t>(i)].active == 0) {
      first = i;
      break;
    }
  }
  if (first < 0) return;
  Level4Popup& lead = game.level4Popups[static_cast<std::size_t>(first)];
  game.level4Pending = 1;
  game.level4PendingLead = first;
  lead.active = 1;
  lead.flag = 0;
  lead.row = (source.row + 0xf8) & 0xff;
  lead.col = (source.col + 8) & 0xff;
  lead.owner = owner;
  lead.timer = 0x20;
  int second = -1;
  for (int i = 0; i < 4; ++i) {
    if (game.level4Popups[static_cast<std::size_t>(i)].active == 0) {
      second = i;
      break;
    }
  }
  if (second < 0) return;
  Level4Popup& trail = game.level4Popups[static_cast<std::size_t>(second)];
  game.level4PendingTrail = second;
  trail.active = 1;
  trail.flag = 1;
  trail.row = lead.row;
  trail.col = (lead.col + 0x0f) & 0xff;
  trail.owner = owner;
  trail.timer = 0x20;
}

// $9B10: how much the close ball is worth, decided by its distance.
void level4AwardDouble(Game& game, int ballColumn) {
  if (game.level4Pending == 0) return;
  const int delta = (ballColumn - game.level4Charlie.col) & 0xff;
  int points = 400;
  if (delta < 5) points = delta >= 2 ? 600 : 2000;
  if (game.level4PendingLead >= 0) {
    game.level4Popups[static_cast<std::size_t>(game.level4PendingLead)].value =
        points;
  }
  if (game.level4PendingTrail >= 0) {
    game.level4Popups[static_cast<std::size_t>(game.level4PendingTrail)].value =
        points;
  }
  level4AddScore(game, points);
  ++game.level4Doubles;
  ++game.prizeBagAudioSerial;
  game.level4Pending = 0;
}

// $9A42: find the ball Charlie came down on, or fall.  A landing ends with
// $9B0D pulling its own return address off the stack, so the goal test and
// the second joystick latch at $9A11/$9A14 only run when nothing was there.
bool level4Landing(Game& game) {
  Level4Record& charlie = game.level4Charlie;
  int landed = -2;
  if (charlie.direction == 0) {
    // Standing still he always comes back down on his own ball.
    const Level4Record& ball = game.level4Ball;
    if (ball.active != 0) {
      const int reach = (charlie.col + 0x18) & 0xff;
      int delta = static_cast<std::int8_t>(
          (((ball.col + 0x10) & 0xff) - reach) & 0xff);
      if (std::abs(delta) < 0x12) landed = -1;
    }
  }
  if (landed == -2) {
    for (int i = 0; i < 4; ++i) {
      const Level4Record& roll = game.level4Rolls[static_cast<std::size_t>(i)];
      if (roll.active == 0) continue;
      const int reach =
          (charlie.col + (charlie.direction == 1 ? 0x0c : 0x18)) & 0xff;
      int delta = static_cast<std::int8_t>(
          (((roll.col + 0x10) & 0xff) - reach) & 0xff);
      if (std::abs(delta) < 0x12) {
        landed = i;
        break;
      }
    }
  }
  if (landed == -2) {
    charlie.state = 2;  // $9A76: nothing underneath, Charlie drops
    return false;
  }
  // $8ED2: a landing while moving is worth 100 on this stage.
  if (charlie.direction != 0) level4AddScore(game, 100);
  ++game.level4Landings;
  // The board is silent on a plain landing: a MAME sound-latch trace of the
  // auto-clear run shows $44 (the jump) only at takeoff and $49 (the prize
  // bag) only when $9B10 pays the close-ball bonus below.
  // $9A84: a second ball close behind pays the double bonus.
  for (int i = 0; i < 4; ++i) {
    const Level4Record& roll = game.level4Rolls[static_cast<std::size_t>(i)];
    if (roll.active == 0) continue;
    if (i == landed) {
      if (charlie.direction != 0) game.level4Flash = 0x10;
      continue;
    }
    if (charlie.direction == 0) break;
    game.level4Flash = 0x10;
    if (charlie.direction == 2) {
      const int gap = (charlie.col - roll.col) & 0xff;
      if (gap >= 0x40) continue;
      level4SpawnDoublePopup(game, i);
      break;
    }
    if (roll.col < 0x50) continue;
    if (roll.col >= 0x78) continue;
    level4SpawnDoublePopup(game, i);
    break;
  }
  // $9AC8: the landed-on ball becomes the ridden ball; the records swap.
  if (landed >= 0) {
    std::swap(game.level4Ball,
              game.level4Rolls[static_cast<std::size_t>(landed)]);
  }
  // $9AFC: which way the ball has to travel to get back under Charlie.
  Level4Record& ball = game.level4Ball;
  const int diff = (ball.col - charlie.col) & 0xff;
  ball.align = (diff == 0 || diff >= 0x80) ? diff : 1;
  level4AwardDouble(game, ball.col);
  return true;
}

// $9BC0: the goal stand on the last page.
void level4GoalCheck(Game& game) {
  Level4Record& charlie = game.level4Charlie;
  if (((game.level4Scroll >> 8) & 0xff) != 0xf8) return;
  if (charlie.col < 0xa0 || charlie.col >= 0xca) {
    charlie.state = 2;
    return;
  }
  charlie.row = (charlie.row + 6) & 0xff;
  level4SetScript(charlie, kLevel4ScriptCheer);
  charlie.state = 4;
  charlie.countdown = 0xa0;
  level4SpawnCallouts(game, kLevel4GoalCells, 1);
  game.scene = Scene::Goal;
  game.goalFrame = 0;
}

// ---- $9943: walking, scrolling and the jump arc ---------------------------

void level4Move(Game& game) {  // $9943
  Level4Record& charlie = game.level4Charlie;
  if (charlie.phase == 0) {
    if (charlie.jump != 0) {
      charlie.phase = 1;
      ++game.jumpAudioSerial;
    }
    if (charlie.jump != 0 || charlie.stick != charlie.direction) {
      if (charlie.jump != 0) {
        level4SetScript(charlie, ((game.level4Scroll >> 8) & 0xff) == 0xf8
                                     ? kLevel4ScriptGoalJump
                                     : kLevel4ScriptJumping);
      } else {
        level4SetScript(charlie, charlie.stick == 0 ? kLevel4ScriptStanding
                                                    : kLevel4ScriptWalking);
      }
      charlie.direction = charlie.stick;
      charlie.idleHigh = 0;
      charlie.idleLow = 0;
      level4Plaques(game);
    }
  }
  const bool walking = ((game.level4Scroll >> 8) & 0xff) == 0xf8;
  if (charlie.direction == 2) {
    if (walking) {
      if (charlie.col < 0xe0) charlie.col = (charlie.col + 1) & 0xff;
    } else {
      game.level4Scroll = (game.level4Scroll - 1) & 0xffff;
      for (auto& plaque : game.level4Plaques)
        plaque.col = (plaque.col - 1) & 0xff;
    }
  } else if (charlie.direction == 1) {
    if (game.level4Scroll != 0) {
      if (walking && charlie.col != 0x50) {
        charlie.col = (charlie.col - 1) & 0xff;
      } else {
        game.level4Scroll = (game.level4Scroll + 1) & 0xffff;
        for (auto& plaque : game.level4Plaques)
          plaque.col = (plaque.col + 1) & 0xff;
      }
    }
  }
  if (charlie.phase == 0) return;
  if (charlie.phase == 1) {
    // $8499: rise until the velocity runs out.
    int velocity = charlie.velocity;
    if ((velocity >> 8) != 0) {
      velocity = (velocity - 0x20) & 0xffff;
      charlie.velocity = velocity;
      charlie.row = (charlie.row - (velocity >> 8)) & 0xff;
      return;
    }
    if ((velocity & 0xff) >= 0x40) {
      velocity = (velocity - 0x40) & 0xffff;
      charlie.velocity = velocity;
      charlie.row = (charlie.row - (velocity >> 8)) & 0xff;
      return;
    }
    charlie.velocity = 0;
    charlie.phase = 2;
    // $8425 runs the first falling step on the apex frame.
  }
  int velocity = charlie.velocity;
  if ((velocity >> 8) == 0) velocity = (velocity + 0x20) & 0xffff;
  velocity = (velocity + 0x20) & 0xffff;
  charlie.velocity = velocity;
  charlie.row = (charlie.row + (velocity >> 8)) & 0xff;
  if ((velocity >> 8) != charlie.landVelocity) return;
  charlie.phase = 0;
  charlie.jump = 0;
  level4SetScript(charlie, charlie.direction == 0 ? kLevel4ScriptStanding
                                                  : kLevel4ScriptWalking);
  charlie.landVelocity = charlie.launch;
  charlie.velocity = (charlie.launch << 8) | 0x20;
  if (level4Landing(game)) return;
  level4GoalCheck(game);
  level4LatchInput(game, charlie.stick, game.level4LatchedButton);
}

// ---- $9872: Charlie's state machine ($FAC6) -------------------------------

void level4Charlie(Game& game, int stick, int button) {
  Level4Record& charlie = game.level4Charlie;
  switch (charlie.state) {
    case 1: {  // $98BF riding
      // How many times the script advances this frame.  Airborne or moving
      // it is once ($9905); standing still the idle counter decides, and the
      // wobble speeds up the longer Charlie holds the ball ($98C7-$9905).
      int steps = 1;
      if (charlie.phase == 0 && charlie.direction == 0) {
        charlie.idleLow = (charlie.idleLow + 4) & 0xff;
        if (charlie.idleLow == 0)
          charlie.idleHigh = (charlie.idleHigh + 1) & 0xff;
        if (charlie.idleHigh >= 3) {
          // $98E3: three seconds of standing still and he slips off.
          charlie.state = 2;
          charlie.velocity = charlie.launch << 8;
          break;
        }
        if (charlie.idleHigh == 0) {
          // $98F6: only the frame that starts the count takes a step.
          steps = ((charlie.idleLow - 4) & 0xff) == 0 ? 1 : 0;
        } else if (charlie.idleHigh == 2) {
          steps = 3;   // $98FF runs $87CE three times
        }
      }
      for (int step = 0; step < steps; ++step) level4Animate(charlie);
      level4LatchInput(game, stick, button);
      level4Move(game);
      break;
    }
    case 2: {  // $8592 sliding off the ball
      if (charlie.direction != 0 && game.level4Scroll != 0) {
        charlie.col =
            (charlie.col + (charlie.direction == 2 ? 1 : -1)) & 0xff;
      }
      charlie.velocity = (charlie.velocity + 0x20) & 0xffff;
      charlie.row = (charlie.row + (charlie.velocity >> 8)) & 0xff;
      if (charlie.row >= 0xd0) {
        charlie.state = 8;
        charlie.countdown = 0x28;
        level4SetScript(charlie, kLevel4ScriptFallen);
        level4SpawnCallouts(game, kLevel4FailCells, 2);
        game.floorFallMiss = true;      // the miss on the floor, then the fail piece
        ++game.floorFallAudioSerial;
      }
      break;
    }
    case 4:  // $8FD7 the goal celebration
      if (charlie.countdown > 0) {
        --charlie.countdown;
        level4Animate(charlie);
        if (charlie.countdown == 0) {
          game.level4Finished = true;
          game.level4Scroll = 0;
        }
      }
      break;
    case 7:  // $84BC struck by a ball
      if (charlie.countdown != 0) {
        --charlie.countdown;
        level4Animate(charlie);
        charlie.velocity = 0;
        break;
      }
      charlie.velocity = (charlie.velocity + 0x20) & 0xffff;
      charlie.row = (charlie.row + (charlie.velocity >> 8)) & 0xff;
      if (charlie.row >= 0xd0) {
        charlie.state = 8;
        charlie.countdown = 0x28;
        level4SetScript(charlie, kLevel4ScriptFallen);
        level4SpawnCallouts(game, kLevel4FailCells, 2);
        game.floorFallMiss = true;      // the miss on the floor, then the fail piece
        ++game.floorFallAudioSerial;
      }
      break;
    case 8:  // $8509 the crowd's reaction, then the restart
      level4Animate(charlie);
      if (charlie.countdown > 0) --charlie.countdown;
      if (charlie.countdown == 0) {
        int page = (game.level4Scroll >> 8) & 0xff;
        if (page != 0) page = (page + (page >= 0xfa ? 1 : 2)) & 0xff;
        game.level4RestartPage = page;
        game.level4Died = true;
      }
      break;
    default:
      break;
  }
  level4Plaques(game);
}

// ---- $9BF8: the ball Charlie rides ($FAD9) --------------------------------

// $9CCB: the ball closes on Charlie's column.
void level4BallFollow(Game& game) {
  Level4Record& ball = game.level4Ball;
  const Level4Record& charlie = game.level4Charlie;
  const bool walking = ((game.level4Scroll >> 8) & 0xff) == 0xf8;
  if (!walking) {
    if (ball.align == 0) {
      ball.col = charlie.col;
      return;
    }
    ball.col = (ball.col + (ball.align == 1 ? -1 : 1)) & 0xff;
    if (ball.col == charlie.col) ball.align = 0;
    if (ball.state == 0) level4Animate(ball);
    return;
  }
  if (ball.align == 0) {
    ball.col = charlie.col;
    return;
  }
  if (ball.align == 1) {
    if (charlie.direction != 1) {
      if (ball.col == charlie.col) ball.align = 0;
      return;
    }
    level4Animate(ball);
    int col = ball.col;
    for (int step = 0; step < 3; ++step) {
      if (col == charlie.col) {
        ball.align = 0;
        return;
      }
      if (step < 2) col = (col - 1) & 0xff;
    }
    ball.col = col;
    return;
  }
  if (charlie.direction == 1) {
    if (ball.col == charlie.col) ball.align = 0;
    return;
  }
  level4Animate(ball);
  int col = ball.col;
  for (int step = 0; step < 3; ++step) {
    if (col == charlie.col) {
      ball.align = 0;
      return;
    }
    if (step < 2) col = (col + 1) & 0xff;
  }
  ball.col = col;
}

void level4BallState(Game& game) {
  Level4Record& ball = game.level4Ball;
  if (ball.active == 0 && ball.flag == 0) return;
  const Level4Record& charlie = game.level4Charlie;
  switch (ball.state) {
    case 0:
    case 1:
    case 2: {
      if (ball.state != 0) level4Animate(ball);
      level4BallFollow(game);
      if (charlie.phase != 0) {
        // $9C14: Charlie has left, the ball rolls out from under him.
        if (charlie.direction == 0) break;
        ball.state = charlie.direction + 2;
        ball.animation = 0;
        break;
      }
      if (charlie.direction == 0) {
        ball.state = 0;
      } else if (ball.state != charlie.direction) {
        ball.state = charlie.direction;
        ball.animation = 0;
      }
      break;
    }
    case 3:  // $9C79 rolling off to the right
      level4Animate(ball);
      level4Animate(ball);
      ball.col = (ball.col + 2) & 0xff;
      if (ball.col >= 0xf0) level4ClearRecord(ball);
      break;
    case 4:  // $9C9C rolling off to the left
      level4Animate(ball);
      level4Animate(ball);
      ball.col = (ball.col - 2) & 0xff;
      if (ball.col >= 0xf0 && ball.col < 0xf2) level4ClearRecord(ball);
      break;
    case 5:  // $9CB4 the opening frame
      level4Animate(ball);
      ball.state = 1;
      ball.animation = 0;
      break;
    case 6:  // $9CBD knocked left
      level4Animate(ball);
      ball.col = (ball.col - 1) & 0xff;
      if (ball.col >= 0xf0) level4ClearRecord(ball);
      break;
    case 7:  // $9CC4 knocked right
      level4Animate(ball);
      ball.col = (ball.col + 1) & 0xff;
      if (ball.col >= 0xf0 && ball.col < 0xf2) level4ClearRecord(ball);
      break;
    default:
      break;
  }
}

// ---- $7D65: the object slots, popups and callout cells --------------------

void level4Objects(Game& game) {
  for (auto& popup : game.level4Popups) {
    if (popup.active == 0) continue;
    if (popup.state == 0) {
      if (popup.timer > 0) --popup.timer;
      if (popup.timer == 0) {
        level4ClearPopup(popup);
        continue;
      }
      const Level4Record& owner = level4BallRecord(game, popup.owner);
      popup.col = (owner.col + 8 + (popup.flag != 0 ? 0x0f : 0)) & 0xff;
      if (popup.col == 0) level4ClearPopup(popup);
    } else if (popup.state == 2) {
      popup.timer = (popup.timer - 1) & 0xff;
      if (popup.timer == 0) level4ClearPopup(popup);
    }
  }
}

// ---- $BB73: the bonus countdown and its time-out --------------------------

void level4Bonus(Game& game) {
  if (game.level4BonusHold != 0) {
    --game.level4BonusHold;
    return;
  }
  // $BBBE: sixteen doubles and the clock starts running faster.
  int steps = 1;
  if (game.level4Doubles == 0x20) steps = 3;
  else if (game.level4Doubles == 0x15) steps = 2;
  if (game.level4Charlie.state == 4) return;
  for (int step = 0; step < steps; ++step) {
    if (game.bonus > 0) {
      --game.bonus;
      if (game.bonus != 0) continue;
    }
    const int before = game.level4Timeout--;
    if (before == 0) {
      game.level4Timeout = 0;
      continue;
    }
    if (before == 2 && game.level4Charlie.state == 1) {
      game.level4Charlie.state = 7;   // falls at once; the floor plays the rest
    }
  }
}

// ---- Event 4 frame ($97D8) ------------------------------------------------

void updateLevel4(Game& game, const Uint8* keyboard, bool jumpPressed,
                  float controllerAxis) {
  game.player.previous = game.player.position;
  game.previousCameraX = game.cameraX;

  const bool moveLeft = keyboard[SDL_SCANCODE_LEFT] || controllerAxis < -0.35F;
  const bool moveRight = keyboard[SDL_SCANCODE_RIGHT] || controllerAxis > 0.35F;
  // Without this, $9C55 would store a stick value of 3 as the ridden ball's
  // state -- the state a ball takes after being knocked away -- and the ball
  // would roll out from under Charlie and leave him riding thin air.
  const int stick = resolveStick((moveLeft ? 1 : 0) | (moveRight ? 2 : 0),
                                 game.level4Charlie.stick,
                                 game.level4BothStick);
  const bool button = jumpPressed || keyboard[SDL_SCANCODE_SPACE] ||
                      keyboard[SDL_SCANCODE_Z];

  level4BallCollisions(game);      // $97F1
  level4RollSpawn(game);           // $9D56
  level4RollUpdate(game);          // $9D64
  level4Charlie(game, stick, button ? 0x10 : 0);   // $9872
  level4BallState(game);           // $9BF8
  level4Objects(game);             // $7D65
  level4Bonus(game);               // $BB73
  // HD animation counters.
  if (game.level4Charlie.phase != 0) ++game.level4JumpAge; else game.level4JumpAge = 0;
  if (game.level4Charlie.script == kLevel4ScriptWalking) ++game.level4WalkAge;
  if ((game.level4Flash != 0 || game.level4Charlie.state == 4) &&
      (game.level1BoardFrameByte & 3) == 0) {
    game.level4Flash = (game.level4Flash - 1) & 0xff;   // $81FA
  }

  if (game.level4Finished) {
    game.level4Finished = false;
    game.scene = Scene::Tally;
    game.tallyFrame = 0;
  } else if (game.level4Died) {
    game.level4Died = false;
    game.scene = Scene::Crashed;
    game.crashFrame = 0;
    // As in Event 2: the rest of the miss and the fail piece run through
    // the crash scene after the board's $28 frames on the floor.
    game.crashDurationFrames = kLevel4RestartFrames;
    if (game.failPieceFrames > 0) {
      game.crashDurationFrames =
          std::max(1, game.failPieceFrames + kFloorMissFrames - 0x28);
    }
    if (game.lives > 0) --game.lives;
  }
  syncLevel4World(game);
}


#include "level5_logic.inc"
#include "level6_logic.inc"

// ============================================================================
// Event 2 (tightrope and monkeys): a clean-room port of the circusc4 stage
// handler at $7CE8-$8A6B.  docs/LEVEL2_ROM_MODEL.md explains the model and
// lists the captures in docs/diagnostics/level2-replay/ that verify it.
// ============================================================================
void level2StartPerfect(Game& game);
void level2SpawnScorePopup(Game& game, int record, int points);
void level2LandingBonus(Game& game);
void level2GoalCheck(Game& game);
void level2Move(Game& game);
void level2RaiseStand(Game& game);

// $F2C0: six difficulty tables of nine page pointers.  Each page
// points into a flat (column, purple) list that the spawner walks; a
// -1 ends the table.  kLevel2SchedulePageStart[d][page] is the entry
// index (pairs, not bytes) at which each page begins.
constexpr std::array<std::array<int, 9>, 6> kLevel2SchedulePageStart{{
    {0, 0, 4, 8, 12, 16, 20, 24, 28},
    {0, 0, 5, 9, 13, 18, 22, 26, 30},
    {0, 0, 4, 8, 12, 16, 20, 24, 28},
    {0, 0, 4, 9, 13, 17, 22, 26, 30},
    {0, 0, 4, 8, 12, 16, 20, 24, 28},
    {0, 0, 4, 8, 12, 15, 19, 24, 27},
}};
constexpr std::array<std::array<int, 512>, 6> kLevel2Schedule{{
    {48, 0, 128, 1, 184, 0, 254, 0, 48, 1, 128, 0, 184, 1, 254, 0, 48, 0, 128, 0, 184, 0, 254, 0, 48, 0, 128, 0, 184, 1, 254, 0, 48, 0, 128, 0, 184, 1, 254, 0, 48, 0, 128, 1, 184, 0, 254, 0, 48, 0, 128, 0, 184, 0, 254, 0, 48, 0, 128, 1, 184, 0, 254, 0, 48, 1, 64, 0, 120, 0, 176, 0, 254, 0, 48, 0, 112, 0, 192, 1, 254, 0, 48, 0, 112, 0, 192, 1, 216, 0, 8, 0, 72, 0, 128, 0, 208, 1, 225, 0, 48, 0, 128, 1, 144, 1, 254, 0, 48, 0, 112, 0, 192, 1, 254, 0, 48, 0, 104, 0, 184, 1, 254, 0, 48, 0, 128, 1, 254, 0, 48, 0, 128, 1, 184, 0, 254, 0, 48, 0, 128, 1, 184, 0, 254, 0, 48, 0, 128, 1, 184, 0, 254, 0, 48, 1, 128, 0, 184, 1, 254, 0, 48, 1, 128, 0, 184, 1, 254, 0, 48, 0, 128, 0, 184, 1, 254, 0, 48, 1, 128, 0, 184, 1, 254, 0, 48, 1, 128, 0, 184, 1, 254, 0, 16, 0, 80, 0, 144, 0, 208, 0, 48, 0, 112, 0, 152, 0, 192, 1, 254, 0, 32, 1, 112, 1, 176, 1, 240, 0, 48, 0, 128, 0, 184, 1, 254, 0, 48, 0, 128, 1, 144, 1, 168, 0, 254, 0, 48, 0, 128, 1, 184, 0, 254, 0, 48, 0, 128, 0, 184, 0, 254, 0, 48, 0, 128, 1, 184, 0, 254, 0, 48, 0, 128, 0, 184, 0, 254, 0, 48, 0, 128, 1, 184, 0, 254, 0, 48, 0, 152, 0, 192, 1, 254, 0, 48, 0, 128, 0, 184, 0, 254, 0, 48, 0, 128, 1, 184, 0, 254, 0, 48, 0, 128, 1, 184, 1, 254, 1, 48, 0, 128, 1, 184, 0, 254, 0, 48, 0, 128, 1, 184, 0, 254, 0, 16, 1, 96, 1, 168, 0, 232, 0, 32, 1, 128, 1, 184, 0, 254, 0, 48, 0, 128, 1, 184, 0, 254, 0, 48, 0, 128, 1, 184, 0, 16, 0, 80, 1, 152, 0, 208, 1, 48, 0, 112, 0, 152, 0, 192, 1, 254, 0, 32, 1, 112, 1, 176, 1, 16, 1, 80, 0, 184, 1, 254, 1, 48, 0, 112, 0, 152, 0, 192, 1, 254, 0, 48, 1, 128, 1, 184, 1, 254, 0, 48, 0, 128, 0, 184, 0, 254, 0, 48, 0, 128, 1, 184, 0, 254, 0, 48, 0, 128, 1, 184, 0, 254, 1, 48, 0, 128, 1, 184, 0, 254, 1, 48, 0, 112, 0, 152, 0, 192, 1, 254, 0, 48, 0, 128, 1, 184, 0, 254, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {48, 1, 64, 0, 120, 0, 176, 0, 254, 0, 48, 0, 112, 0, 192, 1, 254, 0, 48, 0, 112, 0, 192, 1, 216, 0, 8, 0, 72, 0, 128, 0, 208, 1, 225, 0, 48, 0, 128, 1, 144, 1, 254, 0, 48, 0, 112, 0, 192, 1, 254, 0, 48, 0, 104, 0, 184, 1, 254, 0, 48, 0, 128, 1, 254, 0, 48, 0, 128, 1, 184, 0, 254, 0, 48, 0, 128, 1, 184, 0, 254, 0, 48, 0, 128, 1, 184, 0, 254, 0, 48, 1, 128, 0, 184, 1, 254, 0, 48, 1, 128, 0, 184, 1, 254, 0, 48, 0, 128, 0, 184, 1, 254, 0, 48, 1, 128, 0, 184, 1, 254, 0, 48, 1, 128, 0, 184, 1, 254, 0, 16, 0, 80, 0, 144, 0, 208, 0, 48, 0, 112, 0, 152, 0, 192, 1, 254, 0, 32, 1, 112, 1, 176, 1, 240, 0, 48, 0, 128, 0, 184, 1, 254, 0, 48, 0, 128, 1, 144, 1, 168, 0, 254, 0, 48, 0, 128, 1, 184, 0, 254, 0, 48, 0, 128, 0, 184, 0, 254, 0, 48, 0, 128, 1, 184, 0, 254, 0, 48, 0, 128, 0, 184, 0, 254, 0, 48, 0, 128, 1, 184, 0, 254, 0, 48, 0, 152, 0, 192, 1, 254, 0, 48, 0, 128, 0, 184, 0, 254, 0, 48, 0, 128, 1, 184, 0, 254, 0, 48, 0, 128, 1, 184, 1, 254, 1, 48, 0, 128, 1, 184, 0, 254, 0, 48, 0, 128, 1, 184, 0, 254, 0, 16, 1, 96, 1, 168, 0, 232, 0, 32, 1, 128, 1, 184, 0, 254, 0, 48, 0, 128, 1, 184, 0, 254, 0, 48, 0, 128, 1, 184, 0, 16, 0, 80, 1, 152, 0, 208, 1, 48, 0, 112, 0, 152, 0, 192, 1, 254, 0, 32, 1, 112, 1, 176, 1, 16, 1, 80, 0, 184, 1, 254, 1, 48, 0, 112, 0, 152, 0, 192, 1, 254, 0, 48, 1, 128, 1, 184, 1, 254, 0, 48, 0, 128, 0, 184, 0, 254, 0, 48, 0, 128, 1, 184, 0, 254, 0, 48, 0, 128, 1, 184, 0, 254, 1, 48, 0, 128, 1, 184, 0, 254, 1, 48, 0, 112, 0, 152, 0, 192, 1, 254, 0, 48, 0, 128, 1, 184, 0, 254, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {48, 0, 128, 1, 184, 0, 254, 0, 48, 0, 128, 1, 184, 0, 254, 0, 48, 0, 128, 1, 184, 0, 254, 0, 48, 1, 128, 0, 184, 1, 254, 0, 48, 1, 128, 0, 184, 1, 254, 0, 48, 0, 128, 0, 184, 1, 254, 0, 48, 1, 128, 0, 184, 1, 254, 0, 48, 1, 128, 0, 184, 1, 254, 0, 16, 0, 80, 0, 144, 0, 208, 0, 48, 0, 112, 0, 152, 0, 192, 1, 254, 0, 32, 1, 112, 1, 176, 1, 240, 0, 48, 0, 128, 0, 184, 1, 254, 0, 48, 0, 128, 1, 144, 1, 168, 0, 254, 0, 48, 0, 128, 1, 184, 0, 254, 0, 48, 0, 128, 0, 184, 0, 254, 0, 48, 0, 128, 1, 184, 0, 254, 0, 48, 0, 128, 0, 184, 0, 254, 0, 48, 0, 128, 1, 184, 0, 254, 0, 48, 0, 152, 0, 192, 1, 254, 0, 48, 0, 128, 0, 184, 0, 254, 0, 48, 0, 128, 1, 184, 0, 254, 0, 48, 0, 128, 1, 184, 1, 254, 1, 48, 0, 128, 1, 184, 0, 254, 0, 48, 0, 128, 1, 184, 0, 254, 0, 16, 1, 96, 1, 168, 0, 232, 0, 32, 1, 128, 1, 184, 0, 254, 0, 48, 0, 128, 1, 184, 0, 254, 0, 48, 0, 128, 1, 184, 0, 16, 0, 80, 1, 152, 0, 208, 1, 48, 0, 112, 0, 152, 0, 192, 1, 254, 0, 32, 1, 112, 1, 176, 1, 16, 1, 80, 0, 184, 1, 254, 1, 48, 0, 112, 0, 152, 0, 192, 1, 254, 0, 48, 1, 128, 1, 184, 1, 254, 0, 48, 0, 128, 0, 184, 0, 254, 0, 48, 0, 128, 1, 184, 0, 254, 0, 48, 0, 128, 1, 184, 0, 254, 1, 48, 0, 128, 1, 184, 0, 254, 1, 48, 0, 112, 0, 152, 0, 192, 1, 254, 0, 48, 0, 128, 1, 184, 0, 254, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {16, 0, 80, 0, 144, 0, 208, 0, 48, 0, 112, 0, 152, 0, 192, 1, 254, 0, 32, 1, 112, 1, 176, 1, 240, 0, 48, 0, 128, 0, 184, 1, 254, 0, 48, 0, 128, 1, 144, 1, 168, 0, 254, 0, 48, 0, 128, 1, 184, 0, 254, 0, 48, 0, 128, 0, 184, 0, 254, 0, 48, 0, 128, 1, 184, 0, 254, 0, 48, 0, 128, 0, 184, 0, 254, 0, 48, 0, 128, 1, 184, 0, 254, 0, 48, 0, 152, 0, 192, 1, 254, 0, 48, 0, 128, 0, 184, 0, 254, 0, 48, 0, 128, 1, 184, 0, 254, 0, 48, 0, 128, 1, 184, 1, 254, 1, 48, 0, 128, 1, 184, 0, 254, 0, 48, 0, 128, 1, 184, 0, 254, 0, 16, 1, 96, 1, 168, 0, 232, 0, 32, 1, 128, 1, 184, 0, 254, 0, 48, 0, 128, 1, 184, 0, 254, 0, 48, 0, 128, 1, 184, 0, 16, 0, 80, 1, 152, 0, 208, 1, 48, 0, 112, 0, 152, 0, 192, 1, 254, 0, 32, 1, 112, 1, 176, 1, 16, 1, 80, 0, 184, 1, 254, 1, 48, 0, 112, 0, 152, 0, 192, 1, 254, 0, 48, 1, 128, 1, 184, 1, 254, 0, 48, 0, 128, 0, 184, 0, 254, 0, 48, 0, 128, 1, 184, 0, 254, 0, 48, 0, 128, 1, 184, 0, 254, 1, 48, 0, 128, 1, 184, 0, 254, 1, 48, 0, 112, 0, 152, 0, 192, 1, 254, 0, 48, 0, 128, 1, 184, 0, 254, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {48, 0, 128, 0, 184, 0, 254, 0, 48, 0, 128, 1, 184, 0, 254, 0, 48, 0, 152, 0, 192, 1, 254, 0, 48, 0, 128, 0, 184, 0, 254, 0, 48, 0, 128, 1, 184, 0, 254, 0, 48, 0, 128, 1, 184, 1, 254, 1, 48, 0, 128, 1, 184, 0, 254, 0, 48, 0, 128, 1, 184, 0, 254, 0, 16, 1, 96, 1, 168, 0, 232, 0, 32, 1, 128, 1, 184, 0, 254, 0, 48, 0, 128, 1, 184, 0, 254, 0, 48, 0, 128, 1, 184, 0, 16, 0, 80, 1, 152, 0, 208, 1, 48, 0, 112, 0, 152, 0, 192, 1, 254, 0, 32, 1, 112, 1, 176, 1, 16, 1, 80, 0, 184, 1, 254, 1, 48, 0, 112, 0, 152, 0, 192, 1, 254, 0, 48, 1, 128, 1, 184, 1, 254, 0, 48, 0, 128, 0, 184, 0, 254, 0, 48, 0, 128, 1, 184, 0, 254, 0, 48, 0, 128, 1, 184, 0, 254, 1, 48, 0, 128, 1, 184, 0, 254, 1, 48, 0, 112, 0, 152, 0, 192, 1, 254, 0, 48, 0, 128, 1, 184, 0, 254, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {16, 1, 96, 1, 168, 0, 232, 0, 32, 1, 128, 1, 184, 0, 254, 0, 48, 0, 128, 1, 184, 0, 254, 0, 48, 0, 128, 1, 184, 0, 16, 0, 80, 1, 152, 0, 208, 1, 48, 0, 112, 0, 152, 0, 192, 1, 254, 0, 32, 1, 112, 1, 176, 1, 16, 1, 80, 0, 184, 1, 254, 1, 48, 0, 112, 0, 152, 0, 192, 1, 254, 0, 48, 1, 128, 1, 184, 1, 254, 0, 48, 0, 128, 0, 184, 0, 254, 0, 48, 0, 128, 1, 184, 0, 254, 0, 48, 0, 128, 1, 184, 0, 254, 1, 48, 0, 128, 1, 184, 0, 254, 1, 48, 0, 112, 0, 152, 0, 192, 1, 254, 0, 48, 0, 128, 1, 184, 0, 254, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
}};

// ---- Event 2 board constants ----------------------------------------------

// Charlie and the monkeys walk the rope at row $88; a jump uses the shared
// $0420 arc.  These are the same board rows and columns as the other events,
// mapped onto the native canvas exactly as in Event 3/4.
constexpr int kLevel2RopeRow = 0x88;
constexpr int kLevel2SpawnRow = 0x88;
constexpr int kLevel2CharlieColumn = 0x40;

// The animation scripts $87CE walks: one HD cell and how many frames it is
// held.  Charlie mirrors on the walk (attribute $20 in the ROM), so the pose
// index is all the drawing needs.
enum Level2Script {
  kLevel2ScriptStanding = 0,   // $E9FF
  kLevel2ScriptWalking,        // $EBCE
  kLevel2ScriptJumping,        // $E9CF
  kLevel2ScriptGoalJump,       // $E9D8
  kLevel2ScriptCheer,          // $EAAA (goal)
  kLevel2ScriptFallen,         // $EA9E (OH NO)
  kLevel2ScriptMonkeyBrown,    // $EBFE
  kLevel2ScriptMonkeyPurple,   // $EC13
  kLevel2ScriptMonkeyLeap,     // $EBE9
  kLevel2ScriptMonkeyTurned,   // $EC28
};

inline const Level4Pose* level2Script(int script, int& length) {
  static constexpr std::array<Level4Pose, 4> standing{
      {{0, 16}, {1, 16}, {2, 16}, {0, 16}}};
  static constexpr std::array<Level4Pose, 4> walking{
      {{0, 6}, {1, 6}, {2, 6}, {3, 6}}};
  static constexpr std::array<Level4Pose, 1> jumping{{{0, 240}}};
  static constexpr std::array<Level4Pose, 6> goalJump{
      {{0, 8}, {1, 8}, {2, 8}, {3, 8}, {3, 8}, {4, 80}}};
  static constexpr std::array<Level4Pose, 2> cheer{{{0, 16}, {1, 16}}};
  static constexpr std::array<Level4Pose, 1> fallen{{{0, 8}}};
  static constexpr std::array<Level4Pose, 3> monkeyBrown{
      {{0, 8}, {1, 6}, {2, 8}}};
  static constexpr std::array<Level4Pose, 3> monkeyPurple{
      {{0, 6}, {2, 4}, {1, 6}}};
  static constexpr std::array<Level4Pose, 3> monkeyLeap{
      {{0, 8}, {1, 8}, {2, 8}}};
  static constexpr std::array<Level4Pose, 2> turned{{{0, 248}, {0, 248}}};
  switch (script) {
    case kLevel2ScriptWalking: length = 4; return walking.data();
    case kLevel2ScriptJumping: length = 1; return jumping.data();
    case kLevel2ScriptGoalJump: length = 6; return goalJump.data();
    case kLevel2ScriptCheer: length = 2; return cheer.data();
    case kLevel2ScriptFallen: length = 1; return fallen.data();
    case kLevel2ScriptMonkeyBrown: length = 3; return monkeyBrown.data();
    case kLevel2ScriptMonkeyPurple: length = 3; return monkeyPurple.data();
    case kLevel2ScriptMonkeyLeap: length = 3; return monkeyLeap.data();
    case kLevel2ScriptMonkeyTurned: length = 2; return turned.data();
    default: length = 4; return standing.data();
  }
}

// $EAFE and $EB23: the goal (FAR OUT / GREAT) and failure (OH NO) callout
// cells, as (row, column) pairs.  Outside Event 3 the cells sit $52 rows
// lower than the stored value ($87A8 adds $52 to the row): the captures show
// FAR OUT / GREAT at rows $AA/$BA and OH NO at $AA/$C2, on the crowd wall.
constexpr int kLevel2CalloutRowBias = 0x52;
constexpr std::array<std::pair<int, int>, 9> kLevel2GoalCells{{
    {0x58, 0x20}, {0x58, 0x30}, {0x58, 0x40}, {0x68, 0x6c}, {0x68, 0x7c},
    {0x68, 0x8c}, {0x58, 0xb0}, {0x58, 0xc0}, {0x58, 0xd0}}};
constexpr std::array<std::pair<int, int>, 6> kLevel2FailCells{{
    {0x58, 0x40}, {0x58, 0x50}, {0x58, 0x60}, {0x70, 0xa0}, {0x70, 0xb0},
    {0x70, 0xc0}}};

// $EB48 stage-2 plaques: three per page, row $E0.  Only the trigger column
// and the cell they show change.
constexpr std::array<int, 3> kLevel2PlaqueTriggers{0x80, 0x70, 0x60};
constexpr std::array<std::array<int, 3>, 10> kLevel2PlaqueCells{{
    {0xb8, 0xb9, 0xba}, {0xb8, 0xb9, 0xba}, {0xca, 0xc5, 0xbc},
    {0xca, 0xc1, 0xbc}, {0xca, 0xc0, 0xbc}, {0xca, 0xbf, 0xbc},
    {0xca, 0xbe, 0xbc}, {0xca, 0xbd, 0xbc}, {0xc2, 0xc3, 0xc4},
    {0xc2, 0xc3, 0xc4}}};

// ---- Event 2 board helpers ------------------------------------------------

void level2Animate(Level4Record& record) {
  if (record.animation > 0) {
    --record.animation;
    return;
  }
  int length = 0;
  const Level4Pose* script = level2Script(record.script, length);
  record.pose = (record.pose + 1) % length;
  record.animation = script[record.pose].delay;
}

void level2SetScript(Level4Record& record, int script) {
  record.script = script;
  record.pose = -1;
  record.animation = 0;
}

void level2AddScore(Game& game, int points) { game.score += points; }

// $8267/$82BA: a plaque re-enters from the right edge when the scroll reaches
// its trigger column.  $8267 seeds them already scrolled off to the left.
void level2InitPlaques(Game& game) {
  int page = (-((game.level2Scroll >> 8) & 0xff)) & 0xff;
  ++page;
  page = std::clamp(page, 0, 9);
  for (std::size_t i = 0; i < kLevel2PlaqueTriggers.size(); ++i) {
    Level4Plaque& plaque = game.level2Plaques[i];
    plaque.trigger = kLevel2PlaqueTriggers[i];
    plaque.col = (-(kLevel2PlaqueTriggers[i] + 0x10)) & 0xff;
    plaque.row = 0xe0;
    plaque.cell = kLevel2PlaqueCells[static_cast<std::size_t>(page)][i];
  }
}

void level2Plaques(Game& game) {
  int page = (-((game.level2Scroll >> 8) & 0xff)) & 0xff;
  if (game.level2Charlie.direction != 1) ++page;
  page = std::clamp(page, 0, 9);
  const int low = game.level2Scroll & 0xff;
  for (std::size_t i = 0; i < kLevel2PlaqueTriggers.size(); ++i) {
    if (kLevel2PlaqueTriggers[i] != low) continue;
    Level4Plaque& plaque = game.level2Plaques[i];
    plaque.col = 0xf0;
    plaque.row = 0xe0;
    plaque.cell = kLevel2PlaqueCells[static_cast<std::size_t>(page)][i];
    break;
  }
}

// $831D/$82F3: the plaques and the goal stand slide one column with the
// scroll.  Moving right the columns count down, moving left they count up.
void level2ScrollProps(Game& game, int delta) {
  for (auto& plaque : game.level2Plaques) plaque.col = (plaque.col + delta) & 0xff;
  Level4Record& stand = game.level2Stand;
  if (stand.active != 0) {
    stand.col = (stand.col + delta) & 0xff;
    // $82F3: a stand that scrolls off the right edge is cleared.
    if (delta > 0 && stand.col >= 0xf8) stand = Level4Record{};
  }
}

template <std::size_t N>
void level2SpawnCallouts(Game& game,
                         const std::array<std::pair<int, int>, N>& cells,
                         int kind) {
  std::size_t cell = 0;
  for (auto& popup : game.level2Popups) {
    if (cell >= cells.size()) return;
    if (popup.active != 0) continue;
    popup.active = 1;
    popup.state = 2;
    popup.row = (cells[cell].first + kLevel2CalloutRowBias) & 0xff;
    popup.col = cells[cell].second;
    popup.cell = static_cast<int>(cell);
    popup.kind = kind;
    popup.owner = -1;
    // $7D96 leaves $A,X alone when a slot is released, and $8794 does not
    // set it, so the countdown starts from whatever the slot held -- zero on
    // a fresh board -- and wraps: the cells stay for 256 frames.
    ++cell;
  }
}

// ---- $80C2: Charlie against the monkeys ------------------------------------

void level2CharlieMonkeys(Game& game) {
  Level4Record& charlie = game.level2Charlie;
  if (charlie.active == 0 || charlie.state >= 3) return;
  for (auto& monkey : game.level2Monkeys) {
    if (monkey.active == 0) continue;
    const int reach = (monkey.col + 0x0d) & 0xff;
    int colDelta = static_cast<std::int8_t>(
        (((charlie.col + 0x13) & 0xff) - reach) & 0xff);
    colDelta = std::abs(colDelta);
    if (colDelta >= 0x0d) continue;
    int rowDelta = static_cast<std::int8_t>(((charlie.row - monkey.row)) & 0xff);
    rowDelta = std::abs(rowDelta);
    if (rowDelta >= 0x10) continue;
    if (rowDelta + colDelta >= 0x14) continue;
    // $8111: both go down.  Charlie loses his direction and takes state 7;
    // the monkey freezes in state 4.
    charlie.state = 7;
    charlie.countdown = 7;
    charlie.direction = 0;
    monkey.countdown = 7;
    monkey.state = 4;
    ++game.stage2HitAudioSerial;   // $8121 -> $BAAC, the miss ($4F)
    game.level2CrashCued = true;
    return;
  }
}

// ---- $7D06: a purple monkey that catches a brown one leaps over it ---------

void level2MonkeyMonkeys(Game& game) {
  for (std::size_t x = 4; x < 7; ++x) {   // $2540-$25C0 purple slots
    Level4Record& purple = game.level2Monkeys[x];
    if (purple.active == 0 || purple.state != 0) continue;
    for (std::size_t u = 0; u < 4; ++u) {   // $2440-$2500 brown slots
      Level4Record& brown = game.level2Monkeys[u];
      if (brown.active == 0 || brown.state != 0) continue;
      if (purple.col >= 0xd0) continue;             // $7D29 CMPA #$D0 BCC
      if (purple.col < 0x30) continue;              // $7D2D CMPA #$30 BCS
      const int delta = (purple.col - brown.col) & 0xff;
      if (delta >= 0x18) continue;                  // $7D33 CMPA #$18 BCC
      // $7D37: the purple starts its leap (state 8) and the brown freezes
      // (state 9) until it lands.
      purple.state = 8;
      brown.state = 9;
      purple.owner = static_cast<int>(u);
      level2SetScript(purple, kLevel2ScriptMonkeyLeap);
      purple.launch = 3;
      purple.landVelocity = 3;
      purple.velocity = 0x0320;
      purple.phase = 0;
      return;
    }
  }
}

// ---- $8818: the monkey spawner ---------------------------------------------

int level2DifficultyIndex(const Game& game) {
  int a = 2;
  if (game.level2Dips != 0) a = (game.level2Dips & 0x60) >> 4;
  int b = game.level2Visits;
  if (b != 0) b = (b - 1) * 2;
  a += b;
  // $9E2D: two more per $FAE9 threshold of landings passed.
  static constexpr std::array<int, 8> thresholds{0x14, 0x18, 0x1c, 0x20,
                                                 0x24, 0x28, 0x2c, 0x30};
  for (int t : thresholds) {
    if (game.level2Landings >= t) a += 2; else break;
  }
  if (a > 10) a = 10;
  return a / 2;
}

Level4Record* level2FreeMonkey(Game& game, bool purple) {
  const std::size_t begin = purple ? 4 : 0;
  const std::size_t end = purple ? 7 : 4;
  for (std::size_t i = begin; i < end; ++i) {
    Level4Record& m = game.level2Monkeys[i];
    if (m.active == 0 && m.flag == 0) return &m;
  }
  return nullptr;
}

void level2Spawn(Game& game, int stick) {
  Level4Record& charlie = game.level2Charlie;
  if (charlie.state == 4) return;
  if (charlie.direction == 1) return;   // no spawning while moving left
  const auto& flat =
      kLevel2Schedule[static_cast<std::size_t>(game.level2Difficulty)];
  int index = game.level2SchedIndex;
  if (2 * index >= static_cast<int>(flat.size()) || flat[2 * index] < 0) return;
  // $8818 reads $288B, increments it (every frame moving right, three of every
  // four while standing still), then compares the value it read, so the logged
  // counter is one past the column a monkey spawns at.
  (void)stick;
  const int sampled = game.level2SchedCounter;
  if (charlie.direction == 2 || (game.level1BoardFrameByte & 3) != 0) {
    game.level2SchedCounter = (game.level2SchedCounter + 1) & 0xff;
  }
  const int column = flat[2 * index];
  if (sampled != column) return;
  const int purpleFlag = flat[2 * index + 1];
  // $8865: a free record must exist in the right block, or the spawn waits.
  Level4Record* slot = level2FreeMonkey(game, purpleFlag != 0);
  if (slot == nullptr) return;
  // $886B: a spawn is blocked while a brown monkey is in columns [$C8,$F2).
  for (const auto& brown : game.level2Monkeys) {
    if (brown.active == 0) continue;
    if (brown.kind != 0) continue;
    if (brown.col >= 0xf2) continue;
    if (brown.col >= 0xc8) return;
  }
  // $8887: on non-last pages moving right the spawn is unconditional; on the
  // last page (and standing) it is spaced by $28F8, forced after two skips.
  const bool gated =
      !(charlie.direction == 2 && ((game.level2Scroll >> 8) & 0xff) != 0xf8);
  game.level2SchedIndex = index + 1;   // $88CF advances past this entry
  if (gated && game.level2Skip != 2) {
    ++game.level2Skip;
    return;
  }
  game.level2Skip = 0;
  Level4Record& monkey = *slot;
  monkey.active = 1;
  monkey.kind = purpleFlag;
  monkey.row = kLevel2SpawnRow;
  monkey.col = 0xf1;
  monkey.half = 0;
  monkey.animation = 0;
  level2SetScript(monkey, purpleFlag != 0 ? kLevel2ScriptMonkeyPurple
                                          : kLevel2ScriptMonkeyBrown);
}

// ---- $8809/$88DB: the monkey records --------------------------------------

// The three move helpers advance a monkey's column and half-column exactly as
// $88E9 ($8900/$8943), $8A25 (a leaping monkey) and $8A40 (a monkey that has
// just landed a leap) do.  `dir` is Charlie's latched direction.
void level2WalkMove(Level4Record& m, int dir, bool lastPage) {
  auto dec = [&] { m.col = (m.col - 1) & 0xff; };
  if (m.kind != 0) {                        // purple
    if (!lastPage) {
      m.half = (m.half + 0x80) & 0xff;
      if (m.half == 0) dec();               // carry every other frame
      if (dir == 1) return;
      dec();
      if (dir == 0) return;
      dec();                                // dir 2: 2.5 columns a frame
    } else {
      if (dir == 0) { dec(); return; }
      m.half = (m.half + 0x80) & 0xff;
      if (m.half == 0) dec();
      if (dir == 1) return;
      dec();
    }
  } else {                                  // brown
    if (!lastPage) {
      const int step = dir == 0 ? 0xc0 : 0x40;
      const int sum = m.half + step;
      m.half = sum & 0xff;
      if (sum > 0xff) dec();
      if (dir != 2) return;
      dec();                                // dir 2: 1.25 columns a frame
    } else {
      if (dir == 0) {
        const int sum = m.half + 0xc0;
        m.half = sum & 0xff;
        if (sum > 0xff) dec();
      } else if (dir == 1) {
        const int sum = m.half + 0x80;
        m.half = sum & 0xff;
        if (sum > 0xff) dec();
      } else {                              // dir 2
        dec();
        m.half = 0x80;
      }
    }
  }
}

void level2LeapMove(Level4Record& m, int dir, bool lastPage) {
  auto dec = [&] { m.col = (m.col - 1) & 0xff; };
  if (lastPage || dir == 0) { dec(); return; }
  if (dir == 2) dec();                 // $8A34
  const int sum = m.half + 0xc0;       // $8A36
  if (sum > 0xff) { m.half = sum & 0xff; return; }  // carry: keep the half
  dec();                               // no carry: a second column, half kept
}

void level2TurnedMove(Level4Record& m, int dir, bool lastPage) {
  if (lastPage || dir == 0) return;
  if (dir == 1) {
    const int sum = m.half + 0x40;
    m.half = sum & 0xff;
    if (sum > 0xff) m.col = (m.col + 1) & 0xff;
  } else {
    const int sum = m.half + 0xc0;
    m.half = sum & 0xff;
    if (sum > 0xff) m.col = (m.col - 1) & 0xff;
  }
}

// $8922: a monkey that has wrapped past the left edge to columns [$F2,$F5) is
// wiped.
bool level2WipeCheck(Level4Record& m) {
  if (m.col < 0xf2 || m.col >= 0xf5) return false;
  m.state = 2;
  return true;
}

void level2Monkeys_(Game& game) {
  const int dir = game.level2Charlie.direction;
  const bool lastPage = ((game.level2Scroll >> 8) & 0xff) == 0xf8;
  for (auto& m : game.level2Monkeys) {
    if (m.active == 0 && m.flag == 0) continue;
    switch (m.state) {
      case 0: {   // $88E9 walking
        if (dir != 1 || (game.level1BoardFrameByte & 1) == 0) level2Animate(m);
        level2WalkMove(m, dir, lastPage);
        level2WipeCheck(m);
        break;
      }
      case 2:     // $899F wiped: the record is cleared
        m = Level4Record{};
        break;
      case 1: {   // $897F turned, then back to walking
        level2Animate(m);
        level2TurnedMove(m, dir, lastPage);
        if (level2WipeCheck(m)) break;
        if (--m.leapTimer <= 0) {
          m.leapTimer = 0;
          m.state = 0;
          level2SetScript(m, m.kind != 0 ? kLevel2ScriptMonkeyBrown
                                         : kLevel2ScriptMonkeyPurple);
        }
        break;
      }
      case 4:     // $89BB struck by Charlie: frozen until the board restarts
      case 5:     // $89BC also frozen
        break;
      case 8: {   // $89C2 leaping
        level2Animate(m);
        bool apexStep = false;
        if (m.phase == 0) {   // rising ($8A02)
          int velHi = (m.velocity >> 8) & 0xff;
          int velLo = m.velocity & 0xff;
          if (velHi != 0) {
            velLo -= 0x20;
            if (velLo < 0) { velLo += 0x100; velHi = (velHi - 1) & 0xff; }
            m.velocity = (velHi << 8) | velLo;
            m.row = (m.row - velHi) & 0xff;
            level2LeapMove(m, dir, lastPage);
            level2WipeCheck(m);
            break;
          }
          // $8A1A: SUBB #$40; without a borrow the rise continues (row -= 0),
          // with a borrow it is the apex and the code falls into $89D2 (no
          // hi==0 extra add), leaving the borrow in the low byte.
          velLo -= 0x40;
          if (velLo >= 0) {
            m.velocity = velLo;   // hi still zero
            level2LeapMove(m, dir, lastPage);
            level2WipeCheck(m);
            break;
          }
          velLo &= 0xff;
          m.velocity = velLo;   // hi still zero
          m.phase = 1;
          apexStep = true;
        }
        // Falling: $89C9 adds a second $20 when the high byte is zero, but the
        // apex frame reaches $89D2 directly and skips that extra add.
        int velHi = (m.velocity >> 8) & 0xff;
        int velLo = m.velocity & 0xff;
        if (velHi == 0 && !apexStep) velLo += 0x20;
        velLo += 0x20;
        if (velLo > 0xff) { velLo -= 0x100; velHi = (velHi + 1) & 0xff; }
        m.velocity = (velHi << 8) | velLo;
        m.row = (m.row + velHi) & 0xff;
        if (velHi == m.landVelocity) {
          m.phase = 0;
          m.state = 0;
          if (m.owner >= 0) {
            Level4Record& brown = game.level2Monkeys[static_cast<std::size_t>(m.owner)];
            if (brown.state != 1) brown.state = 0;
          }
          level2SetScript(m, kLevel2ScriptMonkeyBrown);
          // $89E3 leaves $31:$32 (the owner pointer) in place.
        }
        level2LeapMove(m, dir, lastPage);
        level2WipeCheck(m);
        break;
      }
      case 9:     // $8A40 frozen while a purple leaps over it, still drifting
        level2TurnedMove(m, dir, lastPage);
        level2WipeCheck(m);
        break;
      default:
        break;
    }
  }
}

// ---- Charlie ($8532, $8362, $8425, $8636) ---------------------------------

constexpr std::array<int, 24> kLevel2ScoreTable{
    {0, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 2000, 3000, 4000,
     5000, 6000, 7000, 8000, 9000, 10000, 20000, 30000, 10, 20}};

// $8532: latch the joystick into $1B and the button into the one-shot $1A,
// with the same held flag ($28F0) and buffered-jump ($28D3) rules as Event 4.
void level2LatchInput(Game& game, int stick, int button) {
  Level4Record& charlie = game.level2Charlie;
  charlie.stick = stick & 0x03;
  if (game.level2Buffered != 0) {
    if (charlie.phase != 0) return;
    game.level2Buffered = 0;
    return;
  }
  if (game.level2Held != 0) {
    if (button == 0) game.level2Held = 0; else charlie.jump = 0;
    return;
  }
  charlie.jump = button;
  if (button == 0) return;
  game.level2Held = 1;
  if (charlie.phase == 2 && charlie.row < 0x68) game.level2Buffered = button;
}

// $85BE: the browns ($28F9) and purples ($28FA) that are ahead of Charlie
// within the scoring windows at takeoff.
void level2ComputeBonus(Game& game) {
  Level4Record& charlie = game.level2Charlie;
  game.level2Near = 0;
  game.level2Far = 0;
  game.level2NearFirst = -1;
  game.level2FarFirst = -1;
  const int dir = charlie.direction;
  const bool lastPage = ((game.level2Scroll >> 8) & 0xff) == 0xf8;
  if (dir != 1) {
    const int window = dir == 0 ? 0x20 : 0x50;
    for (std::size_t i = 0; i < 4; ++i) {
      const Level4Record& m = game.level2Monkeys[i];
      if (m.active == 0) continue;
      const int gap = (m.col - charlie.col) & 0xff;
      if (gap >= 0x80) continue;         // behind
      if (gap >= window) continue;
      if (game.level2NearFirst < 0) game.level2NearFirst = static_cast<int>(i);
      ++game.level2Near;
    }
  } else if (lastPage) {
    return;
  }
  const int window = dir == 0 ? 0x40 : (dir == 1 ? 0x20 : 0x80);
  for (std::size_t i = 4; i < 7; ++i) {
    const Level4Record& m = game.level2Monkeys[i];
    if (m.active == 0) continue;
    const int gap = (m.col - charlie.col) & 0xff;
    if (gap >= 0x80) continue;
    if (gap >= window) continue;
    if (game.level2FarFirst < 0) game.level2FarFirst = static_cast<int>(i);
    ++game.level2Far;
  }
}

// $8636: score the browns and purples jumped and start the turned-monkey
// animation.
void level2LandingBonus(Game& game) {
  const int near = game.level2Near;
  const int far = game.level2Far;
  int index = 0;
  int turnRecord = -1;
  if (near > 0 && far > 0) {
    index = near + far + 8;
    game.level2Near = near + far;   // $8643 STB $28F9
    turnRecord = game.level2NearFirst;
  } else if (near > 0) {
    index = near + 1;
  } else if (far == 1) {
    index = 3;
  } else if (far >= 2) {
    index = far + 8;
    game.level2Near = far;          // $86BE STB $28F9
    turnRecord = game.level2FarFirst;
  }
  if (index == 0) return;
  if (index >= static_cast<int>(kLevel2ScoreTable.size())) index = 0;
  level2AddScore(game, kLevel2ScoreTable[static_cast<std::size_t>(index)]);
  ++game.level2Landings;
  game.level2Flash = 0x10;
  // A plain scoring landing is silent; only the combo paths ($8676 and the
  // $86BE branch that joins them at $8655) call $BA8F for the $49 sound.
  if (turnRecord >= 0) ++game.prizeBagAudioSerial;
  if (turnRecord >= 0) {
    Level4Record& m = game.level2Monkeys[static_cast<std::size_t>(turnRecord)];
    m.state = 1;
    m.half = 0;   // $8668 CLR $5,U
    m.leapTimer = 0x20;
    level2SetScript(m, kLevel2ScriptMonkeyTurned);
    level2SpawnScorePopup(game, turnRecord,
                          kLevel2ScoreTable[static_cast<std::size_t>(index)]);
  }
  // $8697: a scoring jump made while moving stores its direction in $220A
  // and spoils the perfect clear; a jump on the spot leaves it alone.
  if (game.level2Charlie.direction != 0 && !game.level2Perfect) {
    game.level2Clear = game.level2Charlie.direction;
  }
}

// $8752: the goal on the last page.
void level2GoalCheck(Game& game) {
  Level4Record& charlie = game.level2Charlie;
  if (((game.level2Scroll >> 8) & 0xff) != 0xf8) return;
  if (game.level2StandGoal == 0) return;
  charlie.row = (charlie.row - 6) & 0xff;
  level2SetScript(charlie, kLevel2ScriptCheer);
  charlie.state = 4;
  charlie.countdown = 0xa0;
  level2SpawnCallouts(game, kLevel2GoalCells, 1);
  // $8097: the 5000-point stand bonus, only if Charlie walked onto the stand.
  if (game.level2StandHit != 0) {
    if (game.level2Stand.active != 0) game.level2Stand.state = 1;
    level2AddScore(game, kLevel2ScoreTable[14]);
    game.level2StandHit = 0;
  }
  game.scene = Scene::Goal;
  game.goalFrame = 0;
  // $7EB1: a perfect clear (Charlie only ever jumped on the spot) brings the
  // bird and its coins; reuse the Event 3 presentation records.
  if (game.level2Clear == 0) level2StartPerfect(game);
}

// $8015: walking into column $C0 on the last page raises the goal stand.
void level2RaiseStand(Game& game) {
  Level4Record& charlie = game.level2Charlie;
  if (charlie.phase != 0) return;
  if (game.level2StandHit != 0) return;
  game.level2StandHit = 1;
  Level4Record& stand = game.level2Stand;
  stand.active = 1;
  stand.state = 0;
  stand.row = 0x66;
  stand.col = 0xc0;
}

// $8362 with $83AE: the grounded frame picks up a jump or a direction change,
// then walks and scrolls the world.
void level2Move(Game& game) {
  Level4Record& charlie = game.level2Charlie;
  const bool lastPage = ((game.level2Scroll >> 8) & 0xff) == 0xf8;
  if (charlie.phase == 0) {
    if (charlie.jump != 0) {                    // $836E: take off
      charlie.phase = 1;
      charlie.direction = charlie.stick;
      level2ComputeBonus(game);
      ++game.jumpAudioSerial;
      level2SetScript(charlie,
                      lastPage ? kLevel2ScriptGoalJump : kLevel2ScriptJumping);
      charlie.idleHigh = 0;
      charlie.idleLow = 0;
    } else if (charlie.stick != charlie.direction) {   // $838F: turn
      level2SetScript(charlie, charlie.stick == 0 ? kLevel2ScriptStanding
                                                  : kLevel2ScriptWalking);
      charlie.direction = charlie.stick;
      charlie.idleHigh = 0;
      charlie.idleLow = 0;
    }
  }
  // $83AE: walk and scroll.  The scroll runs whether or not Charlie is in the
  // air; only his own column walk is grounded.
  {
    const int dir = charlie.direction;
    const bool airborne = charlie.phase != 0;
    if (dir == 2) {
      if (lastPage) {
        if (charlie.col >= 0xc0) {
          level2RaiseStand(game);
        } else {
          charlie.col = (charlie.col + 1) & 0xff;
        }
      } else {
        if (!airborne && charlie.col < 0x40) {
          charlie.half = (charlie.half + 1) & 0xff;
          if ((charlie.half & 1) == 0) charlie.col = (charlie.col + 1) & 0xff;
        }
        game.level2Scroll = (game.level2Scroll - 1) & 0xffff;
        level2ScrollProps(game, -1);
      }
    } else if (dir == 1) {
      if (game.level2Scroll != 0) {
        if (lastPage) {
          if (charlie.col >= 0x40) charlie.col = (charlie.col - 1) & 0xff;
        } else {
          if (!airborne && charlie.col >= 0x20) {
            charlie.half = (charlie.half + 1) & 0xff;
            if ((charlie.half & 1) == 0) charlie.col = (charlie.col - 1) & 0xff;
          }
          game.level2HalfStep = (game.level2HalfStep + 1) & 0xff;
          if ((game.level2HalfStep & 1) == 0) {
            game.level2Scroll = (game.level2Scroll + 1) & 0xffff;
            level2ScrollProps(game, 1);
          }
        }
      }
    }
  }
  // $8425: the jump arc.
  if (charlie.phase == 0) return;
  if (charlie.phase == 1) {   // rising ($8499)
    int velHi = (charlie.velocity >> 8) & 0xff;
    int velLo = charlie.velocity & 0xff;
    if (velHi != 0) {
      velLo -= 0x20;
      if (velLo < 0) { velLo += 0x100; --velHi; }
      charlie.velocity = ((velHi & 0xff) << 8) | (velLo & 0xff);
      charlie.row = (charlie.row - velHi) & 0xff;
    } else {                  // apex ($84B0)
      velLo -= 0x40;
      if (velLo < 0) {
        charlie.velocity = velHi << 8;   // $18 cleared
        charlie.phase = 2;
        // fall straight into a falling step
      } else {
        charlie.velocity = (velHi << 8) | (velLo & 0xff);
        charlie.row = (charlie.row - velHi) & 0xff;
        return;
      }
    }
    if (charlie.phase == 1) return;
  }
  // falling ($842C)
  int velHi = (charlie.velocity >> 8) & 0xff;
  int velLo = charlie.velocity & 0xff;
  if (velHi == 0) velLo += 0x20;
  velLo += 0x20;
  if (velLo > 0xff) { velLo -= 0x100; ++velHi; }
  charlie.velocity = ((velHi & 0xff) << 8) | (velLo & 0xff);
  charlie.row = (charlie.row + velHi) & 0xff;
  const int newHi = (charlie.velocity >> 8) & 0xff;
  bool landed = false;
  if (((game.level2Scroll >> 8) & 0xff) == 0xf8 && charlie.col >= 0xa0 &&
      charlie.col < 0xca && newHi >= 3 && (charlie.velocity & 0xff) == 0x80) {
    game.level2StandGoal = 1;
    landed = true;
  }
  if (!landed && newHi != charlie.landVelocity) return;
  // $846C: grounded again.
  charlie.phase = 0;
  charlie.jump = 0;
  level2SetScript(charlie, charlie.direction != 0 ? kLevel2ScriptWalking
                                                  : kLevel2ScriptStanding);
  charlie.landVelocity = charlie.launch;
  charlie.velocity = (charlie.launch << 8) | 0x20;
  level2LandingBonus(game);   // $8636
  level2GoalCheck(game);      // $8752
}

// ---- $815B: Charlie's state handler ---------------------------------------

void level2CharlieState(Game& game, int stick, int button) {
  Level4Record& charlie = game.level2Charlie;
  switch (charlie.state) {
    case 1: {   // walking the rope
      int steps = 1;
      if (charlie.phase == 0 && charlie.direction == 0) {
        charlie.idleLow = (charlie.idleLow + 2) & 0xff;
        if (charlie.idleLow == 0) charlie.idleHigh = (charlie.idleHigh + 1) & 0xff;
        if (charlie.idleHigh >= 3) {
          charlie.state = 2;
          charlie.velocity = charlie.launch << 8;
          break;
        }
        if (charlie.idleHigh == 0) {
          steps = ((charlie.idleLow - 2) & 0xff) == 0 ? 1 : 0;
        } else if (charlie.idleHigh == 2) {
          steps = 2;
        }
      }
      for (int s = 0; s < steps; ++s) level2Animate(charlie);
      level2LatchInput(game, stick, button);
      level2Move(game);
      break;
    }
    case 2: {   // $8592 falling off the rope
      if (charlie.direction != 0 && game.level2Scroll != 0) {
        charlie.col =
            (charlie.col + (charlie.direction == 2 ? 1 : -1)) & 0xff;
      }
      charlie.velocity = (charlie.velocity + 0x20) & 0xffff;
      charlie.row = (charlie.row + (charlie.velocity >> 8)) & 0xff;
      if (charlie.row >= 0xd0) {
        charlie.state = 8;
        charlie.countdown = 0x28;
        level2SetScript(charlie, kLevel2ScriptFallen);
        level2SpawnCallouts(game, kLevel2FailCells, 2);
        game.floorFallMiss = true;      // nothing cued it before: miss, then fail
        ++game.floorFallAudioSerial;
      }
      break;
    }
    case 4:   // $8FD7 the goal celebration
      if (charlie.countdown > 0) {
        --charlie.countdown;
        level2Animate(charlie);
        if (charlie.countdown == 0) {
          // $8FE6: with $220A still clear the bird is on its way; hold.
          if (game.level2Clear == 0) {
            charlie.countdown = 1;
          } else {
            game.level2Finished = true;
          }
        }
      }
      break;
    case 7:   // $84BC struck by a monkey
      if (charlie.countdown != 0) {
        --charlie.countdown;
        level2Animate(charlie);
        charlie.velocity &= 0xff;   // $84C7 CLR $17,X keeps the low byte
        break;
      }
      charlie.velocity = (charlie.velocity + 0x20) & 0xffff;
      charlie.row = (charlie.row + (charlie.velocity >> 8)) & 0xff;
      if (charlie.row >= 0xd0) {
        charlie.state = 8;
        // $8500 plays the miss here; a monkey hit already cued it at $8121.
        // The fail piece then plays out before the restart (the board's own
        // $28 frames in replays and silent builds).
        game.floorFallMiss = !game.level2CrashCued;
        charlie.countdown = 0x28;
        level2SetScript(charlie, kLevel2ScriptFallen);
        level2SpawnCallouts(game, kLevel2FailCells, 2);
        ++game.floorFallAudioSerial;
        game.level2CrashCued = false;
      }
      break;
    case 8:   // $8509 the crowd's reaction, then the restart
      level2Animate(charlie);
      if (charlie.countdown > 0) --charlie.countdown;
      if (charlie.countdown == 0) {
        int page = (game.level2Scroll >> 8) & 0xff;
        if (page != 0) page = (page + (page >= 0xfa ? 1 : 2)) & 0xff;
        game.level2RestartPage = page;
        game.level2Died = true;
      }
      break;
    default:
      break;
  }
  level2Plaques(game);
}

// ---- $7D65: the object slots (score popups and callouts) ------------------

void level2Objects(Game& game) {
  const int dir = game.level2Charlie.direction;
  const bool lastPage = ((game.level2Scroll >> 8) & 0xff) == 0xf8;
  for (auto& popup : game.level2Popups) {
    if (popup.active == 0) continue;
    if (popup.state == 1) {          // $7D9B a floating score number
      if (--popup.timer <= 0) { popup.active = 0; popup.col = 0; continue; }
      Level4Record drift{};
      drift.col = popup.col;
      drift.half = popup.value;   // $5,X drift accumulator (not $1,X)
      level2TurnedMove(drift, dir, lastPage);
      popup.col = drift.col;
      popup.value = drift.half;
    } else if (popup.state == 2) {   // $7DA6 a callout cell
      popup.timer = (popup.timer - 1) & 0xff;
      if (popup.timer == 0) { popup.active = 0; popup.col = 0; }
    }
  }
}

// $8636 also spawns the two cells of the floating number.
void level2SpawnScorePopup(Game& game, int record, int points) {
  if (record < 0) return;
  const Level4Record& monkey = game.level2Monkeys[static_cast<std::size_t>(record)];
  Level4Popup* first = nullptr;
  for (int i = 0; i < 6; ++i) {
    if (game.level2Popups[static_cast<std::size_t>(i)].active == 0) {
      first = &game.level2Popups[static_cast<std::size_t>(i)];
      break;
    }
  }
  if (first == nullptr) return;
  first->active = 1;
  first->state = 1;
  first->row = (monkey.row - 0x13) & 0xff;
  first->col = (monkey.col + 9) & 0xff;
  first->flag = 0;
  first->timer = 0x20;
  first->value = 0;   // $8706 CLR $5,Y: the drift accumulator
  first->owner = 0;
  first->cell = points;   // the number the $EC3C cells spell out
  for (int i = 0; i < 6; ++i) {
    Level4Popup& slot = game.level2Popups[static_cast<std::size_t>(i)];
    if (slot.active != 0) continue;
    slot.active = 1;
    slot.state = 1;
    slot.row = first->row;
    slot.col = (first->col + 0x0f) & 0xff;
    slot.flag = 0;
    slot.value = 0;
    slot.owner = 1;   // the trailing cell; the drawing code skips it
    slot.timer = 0x20;
    break;
  }
}

// ---- $BB73: the bonus countdown and its time-out --------------------------

void level2Bonus(Game& game) {
  if (game.level2Charlie.state == 4) return;
  if (game.bonus > 0) {
    --game.bonus;
    if (game.bonus != 0) return;
  }
  const int before = game.level2Timeout--;
  if (before == 0) { game.level2Timeout = 0; return; }
  if (before == 2 && game.level2Charlie.state == 1) {
    // $BC79: on this stage the time-out only stores state 7.  $2D (the
    // freeze countdown) is left alone, so with nothing pending Charlie falls
    // at once, keeping whatever velocity and phase he had; the direction is
    // only cleared for Event 3, and no sound plays until state 8 ($8500).
    game.level2Charlie.state = 7;
  }
}

// $7EB1: the perfect-clear bird enters at column $E8 with its bag at $F0.
void level2StartPerfect(Game& game) {
  game.level2BirdActive = true;
  game.level2BirdState = 0;
  game.level2BirdX = 0xe8;
  game.level2BirdBagX = 0xf0;
}

// $7DF2: the bird crosses the screen leftwards one column a frame.  The
// perfect-clear capture shows the cell index step at $A0 and $80, the bag
// vanish once it has passed column $38 and $220A set when the bird reaches
// column 0, which releases the celebration.
void level2UpdateBird(Game& game) {
  if (!game.level2BirdActive) return;
  game.level2BirdX = (game.level2BirdX - 1) & 0xff;
  if (game.level2BirdBagX != 0) {
    game.level2BirdBagX = (game.level2BirdBagX - 1) & 0xff;
    if (game.level2BirdBagX < 0x38) {
      // The bag is the extra Charlie: the capture's life count goes up on
      // the frame it vanishes.
      game.level2BirdBagX = 0;
      ++game.lives;
      ++game.extraCharlieAudioSerial;
    }
  }
  game.level2BirdState = game.level2BirdX <= 0x80 ? 2
                         : (game.level2BirdX <= 0xa0 ? 1 : 0);
  if (game.level2BirdX == 0) {
    game.level2BirdActive = false;
    game.level2Clear = 1;
  }
}

// ---- $8136 init and the frame ($7CE8) -------------------------------------

void level2InitBonus(Game& game, int page) {
  if (page == 0 && !game.level2Started) {
    game.bonus = 6410;
    game.level2Started = true;
  } else if (page == 0 || page >= 0xfe) {
    game.bonus = 4000;
  } else if (page >= 0xfc) {
    game.bonus = 3500;
  } else {
    game.bonus = 3000;
  }
}

void initializeLevel2Board(Game& game, int page) {
  game.level2Scroll = (page << 8) & 0xffff;
  game.level2HalfStep = 0;
  game.level2Charlie = Level4Record{};
  for (auto& m : game.level2Monkeys) m = Level4Record{};
  for (auto& popup : game.level2Popups) popup = Level4Popup{};
  game.level2Stand = Level4Record{};
  game.level2Flash = 0;
  game.level2Buffered = 0;
  game.level2Held = 0;
  game.level2Timeout = 0x40;
  game.level2StandHit = 0;
  game.level2StandGoal = 0;
  game.level2Landings = 0;
  game.level2Skip = 0;
  game.level2SchedCounter = 0;
  game.level2Near = 0;    // $28F9/$28FA are cleared with the rest of the
  game.level2Far = 0;     // stage work RAM (the timeout capture shows it)
  game.level2BirdActive = false;
  game.level2BirdState = 0;
  game.level2BirdX = 0;
  game.level2BirdBagX = 0;
  game.level2Died = false;
  game.level2Finished = false;
  game.level2CrashCued = false;
  level2InitBonus(game, page);
  level2InitPlaques(game);
  // $81A6: pick the schedule table (fixed for this life) and its start page.
  const int difficulty = level2DifficultyIndex(game);
  game.level2Difficulty = difficulty;
  int schedPage = (-((game.level2Scroll >> 8) & 0xff)) & 0xff;
  if (schedPage != 5) ++schedPage;   // $81D1 quirk: page 5 is not advanced
  schedPage = std::clamp(schedPage, 0, 8);
  game.level2SchedIndex =
      kLevel2SchedulePageStart[static_cast<std::size_t>(difficulty)]
                              [static_cast<std::size_t>(schedPage)];
  // $8239/$8148: Charlie on the rope at column $40.
  Level4Record& charlie = game.level2Charlie;
  charlie.active = 1;
  charlie.state = 1;
  charlie.row = kLevel2RopeRow;
  charlie.col = kLevel2CharlieColumn;
  charlie.launch = 4;
  charlie.landVelocity = 4;
  charlie.velocity = 0x0420;
  level2SetScript(charlie, kLevel2ScriptStanding);
}

// The board rows and columns become world pixels for the renderer.
void syncLevel2World(Game& game) {
  const int progress = (-game.level2Scroll) & 0xffff;
  const float scale = kWorldWidth / 224.0F;
  game.cameraX = static_cast<float>(progress) * scale;
  game.player.position = {
      static_cast<float>(progress + game.level2Charlie.col) * scale,
      level3RowToY(static_cast<float>(game.level2Charlie.row))};
  game.player.grounded = game.level2Charlie.phase == 0;
  // Give the renderer a jump-arc position so it can pick the airborne pose.
  game.player.jumpFrame = game.level2Charlie.phase == 0 ? -1
                          : (game.level2Charlie.phase == 1 ? 14 : 42);
  game.player.facingRight = game.level2Charlie.direction != 1;
  game.player.alive = game.level2Charlie.state != 7 &&
                      game.level2Charlie.state != 8 &&
                      game.level2Charlie.state != 2;
  // Rebuild the legacy monkey list the renderer draws from the board records.
  game.stage2Monkeys.clear();
  for (const auto& m : game.level2Monkeys) {
    if (m.active == 0) continue;
    Stage2Monkey draw{};
    draw.worldX = static_cast<float>(progress + m.col) * scale;
    draw.kind = m.kind != 0 ? Stage2MonkeyKind::Purple : Stage2MonkeyKind::Brown;
    draw.leaping = m.state == 8;
    draw.leapFrame = m.pose >= 0 ? m.pose * 18 : 0;
    game.stage2Monkeys.push_back(draw);
  }
}

// `restartFrame`: $8136 re-initialises the board after a miss and falls
// straight into Charlie's handler, so on that frame the collisions, the
// spawner ($288B is not stepped), the monkeys and the object slots do not
// run; the bonus and the flash still do.
void updateLevel2(Game& game, const Uint8* keyboard, bool jumpPressed,
                  float controllerAxis, bool restartFrame) {
  game.player.previous = game.player.position;
  game.previousCameraX = game.cameraX;

  const bool moveLeft = keyboard[SDL_SCANCODE_LEFT] || controllerAxis < -0.35F;
  const bool moveRight = keyboard[SDL_SCANCODE_RIGHT] || controllerAxis > 0.35F;
  const int stick = resolveStick((moveLeft ? 1 : 0) | (moveRight ? 2 : 0),
                                 game.level2Charlie.stick, game.level2BothStick);
  const bool button = jumpPressed || keyboard[SDL_SCANCODE_SPACE] ||
                      keyboard[SDL_SCANCODE_Z];

  // HD animation counters (the board's scripts only step a few cells).
  if (game.level2Charlie.script == kLevel2ScriptWalking &&
      game.level2Charlie.direction != 0) {
    ++game.level2WalkAge;
  }
  if (game.level2Charlie.script == kLevel2ScriptJumping) {
    ++game.level2JumpAge;
  } else {
    game.level2JumpAge = 0;
  }
  ++game.level2MonkeyAge;

  if (!restartFrame) {
    level2CharlieMonkeys(game);      // $80C2
    level2MonkeyMonkeys(game);       // $7D06
    level2Spawn(game, stick);        // $8818
    level2Monkeys_(game);            // $8809
    level2UpdateBird(game);          // $7DF2
    level2Objects(game);             // $7D65
  }
  level2CharlieState(game, stick, button ? 0x10 : 0);          // $8125
  level2Bonus(game);               // $BB73
  if ((game.level2Flash != 0 || game.level2Charlie.state == 4) &&
      (game.level1BoardFrameByte & 3) == 0) {
    game.level2Flash = (game.level2Flash - 1) & 0xff;          // $81FA
  }

  if (game.level2Finished) {
    game.level2Finished = false;
    game.scene = Scene::Tally;
    game.tallyFrame = 0;
  } else if (game.level2Died) {
    game.level2Died = false;
    game.scene = Scene::Crashed;
    game.crashFrame = 0;
    // The board's $28 frames on the floor are done; the rest of the fail
    // piece (and the miss before it, on a time-out drop) runs through the
    // crash scene, so the stage restarts as the piece ends.
    game.crashDurationFrames = 96;
    if (game.failPieceFrames > 0) {
      const int piece = game.failPieceFrames +
                        (game.floorFallMiss ? kFloorMissFrames : 0);
      game.crashDurationFrames = std::max(1, piece - 0x28);
    }
    if (game.lives > 0) --game.lives;
  }
  syncLevel2World(game);
}

void updateStage2(Game& game, const Uint8* keyboard, bool jumpPressed,
                  float controllerAxis) {
  game.player.previous = game.player.position;
  game.previousCameraX = game.cameraX;

  const bool moveLeft = keyboard[SDL_SCANCODE_LEFT] ||
                        controllerAxis < -0.35F;
  const bool moveRight = keyboard[SDL_SCANCODE_RIGHT] ||
                         controllerAxis > 0.35F;
  float targetSpeed = 0.0F;
  if (moveLeft != moveRight) {
    targetSpeed = moveLeft ? kBackSpeed : kForwardSpeed;
    game.player.facingRight = moveRight;
  }
  game.player.runSpeed +=
      (targetSpeed - game.player.runSpeed) * static_cast<float>(kFixedDt) *
      (targetSpeed == 0.0F ? 12.0F : 9.0F);
  if (std::abs(game.player.runSpeed) < 0.5F && targetSpeed == 0.0F) {
    game.player.runSpeed = 0.0F;
  }
  game.player.position.x +=
      game.player.runSpeed * static_cast<float>(kFixedDt);
  if (game.player.position.x < 78.0F) {
    game.player.position.x = 78.0F;
    game.player.runSpeed = std::max(0.0F, game.player.runSpeed);
  }
  game.cameraX = std::max(0.0F, game.player.position.x - 78.0F);

  if (jumpPressed && game.player.grounded) {
    game.player.grounded = false;
    game.player.jumpFrame = 0;
    game.player.verticalVelocity = 0.0F;
    game.stage2JumpClears = 0;
    game.stage2JumpBrown = false;
    game.stage2JumpPurple = false;
    ++game.jumpAudioSerial;
  }
  if (!game.player.grounded) {
    const int previousFrame = game.player.jumpFrame;
    const int nextFrame = std::min(
        previousFrame + 1,
        static_cast<int>(kStage2JumpSourceDisplacement.size()) - 1);
    const float previousDisplacement =
        static_cast<float>(kStage2JumpSourceDisplacement[previousFrame]) *
        kSourceToLogicalY;
    const float displacement =
        static_cast<float>(kStage2JumpSourceDisplacement[nextFrame]) *
        kSourceToLogicalY;
    game.player.jumpFrame = nextFrame;
    game.player.position.y = kStage2RopeY - displacement;
    game.player.verticalVelocity =
        (previousDisplacement - displacement) /
        static_cast<float>(kFixedDt);
    if (nextFrame ==
        static_cast<int>(kStage2JumpSourceDisplacement.size()) - 1) {
      game.player.position.y = kStage2RopeY;
      game.player.verticalVelocity = 0.0F;
      game.player.jumpFrame = -1;
      game.player.grounded = true;
    }
  }

  for (auto& monkey : game.stage2Monkeys) {
    if (monkey.cleared) continue;
    const float screenX = monkey.worldX - game.cameraX;
    if (screenX < 700.0F && screenX > -120.0F) {
      const float speed = monkey.kind == Stage2MonkeyKind::Purple
                              ? kStage2PurpleSpeed
                              : kStage2MonkeySpeed;
      monkey.worldX -= speed * static_cast<float>(kFixedDt);
    }
    if (monkey.kind == Stage2MonkeyKind::Purple && !monkey.leaping) {
      const auto target = std::find_if(
          game.stage2Monkeys.begin(), game.stage2Monkeys.end(),
          [&monkey](const Stage2Monkey& candidate) {
            const float separation = monkey.worldX - candidate.worldX;
            return !candidate.cleared &&
                   candidate.kind == Stage2MonkeyKind::Brown &&
                   separation > 36.0F && separation < 92.0F;
          });
      if (target != game.stage2Monkeys.end()) {
        monkey.leaping = true;
        monkey.leapFrame = 0;
      }
    }
    if (monkey.leaping) {
      ++monkey.leapFrame;
      if (monkey.leapFrame >= 54) {
        monkey.leaping = false;
        monkey.leapFrame = 0;
      }
    }
  }

  for (auto& monkey : game.stage2Monkeys) {
    if (monkey.cleared) continue;
    const float monkeyY = stage2MonkeyY(monkey);
    const float horizontal = game.player.position.x - monkey.worldX;
    const bool touching = std::abs(horizontal) < 27.0F &&
                          std::abs(game.player.position.y - monkeyY) < 48.0F;
    if (touching) {
      crashPlayer(game);
      return;
    }
    if (horizontal > 38.0F) {
      monkey.cleared = true;
      if (game.player.grounded) continue;
      ++game.stage2JumpClears;
      if (monkey.kind == Stage2MonkeyKind::Purple) {
        game.stage2JumpPurple = true;
      } else {
        game.stage2JumpBrown = true;
      }
      int points = game.stage2JumpClears == 1 ? 100 : 1000;
      if (game.stage2JumpClears >= 2 && game.stage2JumpBrown &&
          game.stage2JumpPurple) {
        points = 2000;
      }
      game.score += points;
      game.stage2ScorePopup = points;
      game.stage2ScorePopupFrame = 52;
      game.stage2ScorePopupWorldX = monkey.worldX;
    }
  }

  if (game.stage2ScorePopupFrame > 0) --game.stage2ScorePopupFrame;
  if (game.bonus > 0) --game.bonus;
  game.score =
      std::max(game.score, static_cast<int>(game.player.position.x / 10.0F));

  const bool overGoal = game.player.position.x >= kStage2GoalX - 72.0F;
  const bool descendingToTop =
      overGoal && !game.player.grounded &&
      game.player.verticalVelocity >= 0.0F &&
      game.player.position.y >= kStage2GoalTopY - 8.0F;
  if (descendingToTop) {
    finishStage(game);
  } else if (game.player.grounded && overGoal) {
    game.player.position.x = kStage2GoalX - 72.0F;
    game.player.previous.x = game.player.position.x;
    game.player.runSpeed = std::min(0.0F, game.player.runSpeed);
    game.cameraX = std::max(0.0F, game.player.position.x - 78.0F);
    game.previousCameraX = game.cameraX;
  }
}

// Event 1 board model.  Every rule below names the circusc4 routine it
// reproduces; see docs/level1-remaining-rom-fidelity.md and
// docs/LEVEL1_ROM_MODEL.md.

std::int32_t level1MovementCommand(int direction) {
  if (direction > 0) return kLevel1RightCommand;
  if (direction < 0) return kLevel1LeftCommand;
  return 0;
}

int level1RiderDisplacement(const Player& player) {
  if (player.grounded || player.jumpFrame < 0) return 0;
  return kJumpSourceDisplacement[static_cast<std::size_t>(
      std::clamp(player.jumpFrame, 0,
                 static_cast<int>(kJumpSourceDisplacement.size()) - 1))];
}

void clearLevel1CoinObject(Game& game) {
  // $78ED: $2580, $2586 and $2576 are cleared; the coin pot must be chosen
  // again by a later chain spawn.
  game.level1CoinPot = -1;
  game.level1CoinX = 0;
}

void clearLevel1Pot(Game& game, int index) {
  // $78F7: status and countdown cleared; the coin pointer is dropped when it
  // referenced this record.
  auto& pot = game.level1Pots[static_cast<std::size_t>(index)];
  pot.status = 0;
  pot.countdown = 0;
  if (game.level1CoinPot == index) clearLevel1CoinObject(game);
}

void retireLevel1Hoop(Game& game, std::size_t index, bool exitedRight) {
  auto& hoop = game.hoops[index];
  if (exitedRight) {
    // $759E: an object carried past the right edge by backtracking is
    // un-admitted: the accumulator high byte is cleared and the course index
    // steps back so the same course byte is read again.
    game.level1HoopActivationAccumulator &= 0x00ffU;
    if (game.level1HoopCourseIndex > 0) --game.level1HoopCourseIndex;
    if (game.extraCharlieHoopIndex == static_cast<int>(index)) {
      // $75A3-$75B5: an uncollected doll returns to the pending state and
      // converts the next ordinary admission again.
      if (game.level1ExtraCharlieState == 2) game.level1ExtraCharlieState = 1;
      game.extraCharlieActive = false;
      game.extraCharlieHoopIndex = -1;
    }
  } else {
    // $7586: CLR <$C4 restarts only the high byte of the retirement
    // distance; a retiring reserved ring with its bag still attached counts
    // as a missed reward ($25EE == $FC).
    game.level1RetireDistance &= 0x00ffU;
    game.level1ReservedRetired = false;
    if (index == 3) {
      game.level1ReservedRetired = true;
      if (game.level1BagState == 0xff) ++game.level1MissedRewards;
    }
    if (game.extraCharlieHoopIndex == static_cast<int>(index)) {
      // $75B8-$75E2: the tracked object becomes an ordinary hoop again and
      // $220A stays at two, so the reward is gone unless a later failure
      // ($7CB6) returns the state to pending.
      game.extraCharlieActive = false;
      game.extraCharlieHoopIndex = -1;
    }
  }
  if (hoop.kind == Level1HoopKind::ExtraCharlie) {
    hoop.kind = Level1HoopKind::Large;
  }
  hoop.active = false;
  hoop.sourceXFixed = 0;
  game.bonusRings[index].active = false;
}

void admitLevel1Hoop(Game& game, std::int32_t objectDelta) {
  // $7633-$765A: course selector and byte.
  const std::uint8_t courseByte =
      level1CourseByte(game.level1HoopCourseIndex);
  std::uint8_t selector =
      static_cast<std::uint8_t>(0x10U + game.level1HoopCourseIndex);
  if (selector >= 0x68U) selector = static_cast<std::uint8_t>((selector & 0x07U) + 0x60U);
  selector = static_cast<std::uint8_t>(selector + game.level1HoopCourseOffset);
  const bool selectorBoundary = (selector & 0x03U) == 0;
  const bool reservedPrize =
      selectorBoundary && game.level1HoopCourseState >= 0x60U;
  if (selectorBoundary && !reservedPrize) ++game.level1HoopCourseOffset;

  if (reservedPrize) {
    // $76CA-$76F8: the reserved $2760 ring.
    auto& hoop = game.hoops[3];
    hoop.active = true;
    hoop.kind = Level1HoopKind::PrizeRing;
    hoop.sourceXFixed = 0xff80;
    const std::uint8_t reload =
        static_cast<std::uint8_t>(game.level1BoardFrameByte | 0x80U);
    game.level1HoopActivationAccumulator =
        static_cast<std::uint16_t>(reload) << 8U;
    game.level1HoopCourseState = reload;
    ++game.level1HoopCourseIndex;
    ++game.level1HoopActivations;
    game.level1BagState = 0xff;
    ++game.prizeBagsAvailable;
    game.bonusRings[3].active = true;
    game.bonusRings[3].collected = false;
    game.bonusRings[3].containsPrize = true;
    (void)objectDelta;
    return;
  }

  game.level1HoopActivationAccumulator =
      static_cast<std::uint16_t>(courseByte) << 8U;
  game.level1HoopCourseState = courseByte;
  ++game.level1HoopCourseIndex;
  ++game.level1HoopActivations;
  // $7731: only $26D0/$2700/$2730 are searched; a full pool admits nothing
  // but the stream still advanced.
  for (std::size_t index = 0; index < 3; ++index) {
    auto& hoop = game.hoops[index];
    if (hoop.active) continue;
    hoop.active = true;
    hoop.kind = Level1HoopKind::Large;
    hoop.sourceXFixed = 0xff80;
    if (game.level1ExtraCharlieState == 1) {
      // $767E-$76C8: a pending reward converts this new ordinary record.
      game.level1ExtraCharlieState = 2;
      game.extraCharlieActive = true;
      game.extraCharlieHoopIndex = static_cast<int>(index);
      hoop.kind = Level1HoopKind::ExtraCharlie;
    }
    return;
  }
}

// $7607-$76FD: activation accumulator and course-stream admission.
void updateLevel1Scheduler(Game& game, std::int32_t objectDelta) {
  const std::int32_t delta = objectDelta - 0x80;
  if (delta >= 0) {
    game.level1HoopActivationAccumulator = static_cast<std::uint16_t>(
        game.level1HoopActivationAccumulator + delta);
    return;
  }
  const int page = level1Page(game.level1ProgressFixed);
  const std::uint32_t sum =
      static_cast<std::uint32_t>(game.level1HoopActivationAccumulator) +
      static_cast<std::uint32_t>(static_cast<std::uint16_t>(delta));
  if (page >= 7 && game.level1LatePotLandings < 5) {
    game.level1HoopActivationAccumulator = static_cast<std::uint16_t>(sum);
    return;
  }
  if (sum > 0xffffU) {
    game.level1HoopActivationAccumulator = static_cast<std::uint16_t>(sum);
    return;
  }
  admitLevel1Hoop(game, objectDelta);
}

// $76FE-$774F: bring the last retired object back from the left edge once
// the rider has backtracked the distance it travelled since retirement.
void updateLevel1ReEntry(Game& game, std::int32_t objectDelta) {
  const std::int32_t delta = -objectDelta + 0x80;
  if (delta >= 0) {
    game.level1RetireDistance =
        static_cast<std::uint16_t>(game.level1RetireDistance + delta);
    return;
  }
  const std::int32_t sum = static_cast<std::int32_t>(game.level1RetireDistance) + delta;
  if (sum >= 0) {
    game.level1RetireDistance = static_cast<std::uint16_t>(sum);
    return;
  }
  for (const auto& hoop : game.hoops) {
    const std::uint8_t high = static_cast<std::uint8_t>(hoop.sourceXFixed >> 8U);
    if (static_cast<std::uint8_t>(high - 1U) < 0x40U) return;
  }
  std::size_t index = game.hoops.size();
  if (game.level1ReservedRetired) {
    game.level1ReservedRetired = false;
    index = 3;
  } else {
    for (std::size_t candidate = 0; candidate < 3; ++candidate) {
      if (!game.hoops[candidate].active) {
        index = candidate;
        break;
      }
    }
    if (index == game.hoops.size()) return;
  }
  auto& hoop = game.hoops[index];
  hoop.active = true;
  hoop.sourceXFixed = static_cast<std::uint16_t>(hoop.sourceXFixed + 0x0100U);
  if (index == 3) {
    hoop.kind = Level1HoopKind::PrizeRing;
    game.bonusRings[3].active = true;
  }
}

// $7539-$7585 for one active record.
void moveLevel1Hoop(Game& game, std::size_t index, std::int32_t objectDelta) {
  auto& hoop = game.hoops[index];
  const std::int32_t delta = objectDelta - 0x80;
  const std::int32_t x = hoop.sourceXFixed;
  if (delta < 0) {
    if (x + delta < 0) {
      retireLevel1Hoop(game, index, false);
      return;
    }
  } else if (x + delta > 0xffff) {
    retireLevel1Hoop(game, index, true);
    return;
  }
  hoop.sourceXFixed = static_cast<std::uint16_t>(x + delta);
}

void spawnLevel1Pot(Game& game, int index) {
  auto& pot = game.level1Pots[static_cast<std::size_t>(index)];
  // $780D-$7816: the counter-selected pot becomes the coin pot.
  if (game.level1PotCounter == game.level1CoinPotSelector) {
    game.level1CoinPot = index;
  }
  pot.status = 2;
  pot.animationTimer = 1;
  // $781F-$7868: schedule the next chain pot unless it would land beyond
  // course progress $5F8.
  int spacingIndex = 0x08 + game.level1PotCounter;
  if (spacingIndex >= 0x34) spacingIndex = (spacingIndex & 0x03) + 0x30;
  const std::uint8_t spacing =
      kLevel1PotSpacing[static_cast<std::size_t>(spacingIndex)];
  const int page = level1Page(game.level1ProgressFixed);
  const int offsetByte = level1PageOffsetByte(game.level1ProgressFixed);
  const int reach = page * 256 + (0xff - offsetByte) + spacing;
  if (reach < 0x05f8) {
    auto& next = game.level1Pots[static_cast<std::size_t>((index + 1) % 3)];
    ++next.status;
    next.countdown = static_cast<std::uint16_t>(spacing) << 8U;
  }
  ++game.level1PotCounter;
}

// $7750-$7902: the three fire-pot records and the fixed pots of the final
// pages.
void updateLevel1Pots(Game& game, std::int32_t command) {
  const int page = level1Page(game.level1ProgressFixed);
  const std::uint8_t offsetByte = level1PageOffsetByte(game.level1ProgressFixed);
  for (int index = 0; index < 3; ++index) {
    auto& pot = game.level1Pots[static_cast<std::size_t>(index)];
    if (pot.status == 0) continue;
    if (pot.status == 1) {
      if (command < 0) {
        if (page == 0) continue;
        const std::int32_t sum = static_cast<std::int32_t>(pot.countdown) + command;
        if (sum >= 0) {
          pot.countdown = static_cast<std::uint16_t>(sum);
        } else {
          spawnLevel1Pot(game, index);
        }
      } else {
        const std::int32_t sum = static_cast<std::int32_t>(pot.countdown) + command;
        if (sum > 0xffff) {
          clearLevel1Pot(game, index);
        } else {
          pot.countdown = static_cast<std::uint16_t>(sum);
        }
      }
      continue;
    }
    // Visible ($7875).
    if (command != 0) {
      if (command > 0 && page == 0) continue;
      const std::uint16_t next =
          static_cast<std::uint16_t>(static_cast<std::int32_t>(pot.sourceXFixed) + command);
      if ((next >> 8U) < 2U) {
        // $78AB-$78F7: the object reached an edge.
        if (page >= 6 && offsetByte < 0x80U) continue;
        pot.sourceXFixed = 0;
        if (page == 0 || command < 0) {
          clearLevel1Pot(game, index);
          continue;
        }
        // Carried past the right edge while backtracking: pending again,
        // and the pot scheduled after it is withdrawn.
        pot.status = 1;
        auto& following = game.level1Pots[static_cast<std::size_t>((index + 1) % 3)];
        if (following.status == 2) continue;
        following.status = 0;
        --game.level1PotCounter;
        if (game.level1PotCounter == game.level1CoinPotSelector) {
          clearLevel1CoinObject(game);
        }
        continue;
      }
      pot.sourceXFixed = next;
    }
    // $7893-$78A7 flame flicker.
    if (--pot.animationTimer == 0) {
      pot.animationTimer = 5;
      int variant = game.level1BoardFrameByte & 0x03;
      if (variant == 0) variant = 3;
      pot.flameVariant = static_cast<std::uint8_t>(variant + 2);
    }
  }

  // $776D-$7796: the chain restarts at the page boundaries of pages two to
  // four while every record is idle and the rider moves right.
  if (command < 0 && offsetByte < 2U && page >= 2 && page <= 4 &&
      game.level1Pots[0].status == 0 && game.level1Pots[1].status == 0 &&
      game.level1Pots[2].status == 0) {
    game.level1Pots[0].status = 1;
    game.level1Pots[0].countdown = 0x4000;
    return;
  }
  // $7799-$77FD: page six onward places two fixed pots per page through the
  // <$C8 and <$C6 pointers.
  if (page < 6) return;
  const auto activate = [&](int pointer) {
    auto& pot = game.level1Pots[static_cast<std::size_t>(pointer)];
    pot.status = 2;
    pot.countdown = 0;
    pot.animationTimer = 1;
  };
  const auto clearCells = [&](int pointer) {
    // $77E2 clears the status, +$01 and the X high bytes of the four cells;
    // the X low byte survives.
    auto& pot = game.level1Pots[static_cast<std::size_t>(pointer)];
    pot.status = 0;
    pot.countdown &= 0x00ffU;
    pot.sourceXFixed &= 0x00ffU;
  };
  const auto allocate = [&]() {
    // $77CD tests $24B0 and $24F0 only; when both are busy the pointer
    // falls through to $2530 untested.
    if (game.level1Pots[0].status == 0) return 0;
    if (game.level1Pots[1].status == 0) return 1;
    return 2;
  };
  if (offsetByte >= 0x04U && offsetByte <= 0x05U) {
    activate(game.level1FixedPotC8);
  } else if (offsetByte >= 0x06U && offsetByte <= 0x07U) {
    clearCells(game.level1FixedPotC8);
  } else if (offsetByte >= 0x08U && offsetByte <= 0x09U) {
    game.level1FixedPotC8 = allocate();
  } else if (offsetByte >= 0x5cU && offsetByte <= 0x5dU) {
    activate(game.level1FixedPotC6);
  } else if (offsetByte >= 0x5eU && offsetByte <= 0x5fU) {
    clearCells(game.level1FixedPotC6);
  } else if (offsetByte >= 0x60U && offsetByte <= 0x63U) {
    game.level1FixedPotC6 = allocate();
  }
}

// $7965-$79D9: hidden coin follow, flight and popup timers.
void updateLevel1Coin(Game& game) {
  if (game.level1CoinPopupTimer > 0) --game.level1CoinPopupTimer;
  if (game.level1CoinPot < 0) return;
  const auto& pot = game.level1Pots[static_cast<std::size_t>(game.level1CoinPot)];
  const std::uint8_t potX = static_cast<std::uint8_t>(pot.sourceXFixed >> 8U);
  if (game.level1CoinState > 1) {
    game.level1CoinX = static_cast<std::uint8_t>(potX + 0x0aU);
    return;
  }
  game.level1CoinX = static_cast<std::uint8_t>(potX - 0x08U);
  if (game.level1CoinState == 0) return;
  const int row = static_cast<int>((game.level1CoinYFixed >> 8) & 0xff);
  if (row > 0xd6) return;
  if (row == 0xd6) {
    // The coin has fallen back into the pot and is hidden ($7988).
    game.level1CoinYFixed = 0xd700;
    return;
  }
  game.level1CoinVelocityFixed += 0x1c;
  game.level1CoinYFixed += game.level1CoinVelocityFixed;
  game.level1CoinSpin = (game.level1CoinSpin + 1) % 0x18;
}

void updateLevel1RiderAnimation(Game& game, int liveDirection,
                                std::uint8_t offsetByteBeforeScroll) {
  // $73DC-$7422.  A stopped rider snaps to forward pose A; a moving one
  // advances A->B->C every 11 source pixels forward or 7 backward, reading
  // the second table while walking backward.
  if (liveDirection == 0) {
    game.level1RiderState = Level1RiderState::RunA;
    game.level1RiderBackward = false;
    return;
  }
  const std::uint8_t difference = static_cast<std::uint8_t>(
      game.level1RiderPositionSample - offsetByteBeforeScroll + 0x07U);
  if (difference < 0x12U) return;
  const std::int8_t signedDifference =
      static_cast<std::int8_t>(static_cast<std::uint8_t>(difference - 0x07U));
  bool backward = false;
  if (signedDifference >= 0) {
    game.level1RiderPositionSample =
        static_cast<std::uint8_t>(game.level1RiderPositionSample + 0xf5U);
  } else {
    backward = true;
    game.level1RiderPositionSample =
        static_cast<std::uint8_t>(game.level1RiderPositionSample + 0x07U);
  }
  int phase = static_cast<int>(game.level1RiderState) % 3;
  phase = (phase + 1) % 3;
  game.level1RiderBackward = backward;
  game.level1RiderState = static_cast<Level1RiderState>(phase + (backward ? 3 : 0));
}

void beginLevel1Jump(Game& game, int direction) {
  // $736C-$73DA.
  game.player.grounded = false;
  game.player.jumpFrame = 0;
  game.player.verticalVelocity = 0.0F;
  game.player.level1AirborneDirection = direction;
  game.level1RiderState = Level1RiderState::RunC;
  game.level1RiderBackward = false;
  ++game.jumpAudioSerial;
  // $7394: candidates are the objects currently in [$40,$BF].
  for (auto& hoop : game.hoops) {
    const std::uint8_t high = static_cast<std::uint8_t>(hoop.sourceXFixed >> 8U);
    hoop.takeoffCandidate = high >= 0x40U && high < 0xc0U;
  }
  // $73AA-$73C1: the first pot within [$40,$9F] is watched.
  game.level1PotMarker = -1;
  for (int index = 0; index < 3; ++index) {
    const std::uint8_t high = static_cast<std::uint8_t>(
        game.level1Pots[static_cast<std::size_t>(index)].sourceXFixed >> 8U);
    if (static_cast<std::uint8_t>(high - 0x40U) < 0x60U) {
      game.level1PotMarker = index;
      break;
    }
  }
  // $73CC-$73DA: the coin is armed when it already sits at or behind the
  // rider's reference column.
  game.level1CoinArmed = 0;
  if (static_cast<std::uint8_t>(game.level1CoinX - 1U) < 0x40U) {
    game.level1CoinArmed = 1;
  }
}

void finishLevel1Landing(Game& game, int direction) {
  // $7257-$7335.
  game.level1RiderState = Level1RiderState::RunA;
  game.level1RiderBackward = false;
  game.level1RiderPositionSample =
      level1PageOffsetByte(game.level1ProgressFixed);
  game.level1LandingDirection = direction;
  const int page = level1Page(game.level1ProgressFixed);
  if (direction < 0) {
    // $728B-$72A9: a backward jump that carried an object from behind the
    // rider to ahead of him earns the hanging Charlie.
    if (game.level1ExtraCharlieState == 0) {
      for (const auto& hoop : game.hoops) {
        if (hoop.takeoffCandidate) continue;
        const std::uint8_t high = static_cast<std::uint8_t>(hoop.sourceXFixed >> 8U);
        if (high >= 0x40U && high < 0xc0U) {
          armStage1ExtraCharlie(game);
          break;
        }
      }
    }
    // $72AB-$72D7: the hidden coin launches on the backward landing that
    // follows an armed takeoff.
    if (game.level1CoinState == 0) {
      --game.level1CoinArmed;
      if (game.level1CoinArmed == 0) {
        game.level1CoinState = 1;
        game.level1CoinVelocityFixed = -0x0460;
        game.level1CoinYFixed = 0xd200;
        game.level1CoinSpin = 0;
        game.level1CoinPopupTimer = 0x30;
        game.hiddenCoinTriggered = true;
        game.score += 800;
        showStage1Score(game, 800, level1ObjectWorldX(game, static_cast<std::uint16_t>(game.level1CoinX) << 8U), kGroundY - 118.0F);
        ++game.hiddenCoinAudioSerial;
      }
    }
    return;
  }
  if (direction == 0) return;
  // $72D9-$7335: forward landing scoring.
  int cleared = 0;
  float clearedWorldX = 0.0F;
  for (std::size_t index = 0; index < game.hoops.size(); ++index) {
    const auto& hoop = game.hoops[index];
    if (!hoop.takeoffCandidate) continue;
    const std::uint8_t high = static_cast<std::uint8_t>(hoop.sourceXFixed >> 8U);
    if (high >= 0x40U && high < 0xc0U) continue;
    if (game.extraCharlieHoopIndex == static_cast<int>(index)) continue;
    ++cleared;
    clearedWorldX = hoop.worldX;
  }
  int potBonus = 0;
  if (game.level1PotMarker >= 0) {
    const auto& pot = game.level1Pots[static_cast<std::size_t>(game.level1PotMarker)];
    if ((pot.sourceXFixed >> 8U) < 0x40U) potBonus = 2;
  }
  if (cleared > 0) {
    const int points = (cleared + potBonus) * 100;
    game.score += points;
    game.level1HoopScoreAwarded += points;
    showStage1Score(game, points, clearedWorldX,
                    (kBigHoopOpeningTop + kBigHoopOpeningBottom) * 0.5F - 5.0F);
    return;
  }
  if (potBonus == 0) return;
  if (page >= 7) ++game.level1LatePotLandings;
  game.score += 500;
  game.level1PotPopupTimer = 0x30;
  game.level1PotPopupWorldX =
      game.level1Pots[static_cast<std::size_t>(game.level1PotMarker)].worldX;
  showStage1Score(game, 500, game.level1PotPopupWorldX, kGroundY - 118.0F);
}

void enterLevel1Goal(Game& game) {
  // $79DA-$7B39.
  game.level1BagState = 0;
  game.perfectClear = game.level1MissedRewards == 0;
  game.level1GoalCounter = 0;
  game.level1GoalPhases = 1;
  game.level1BirdPhase = 0;
  game.level1ShowerStartFrame = 0;
  game.level1RewardCoinLaunches = 0;
  for (auto& coin : game.level1RewardCoins) coin = Game::RewardCoin{};
  if (game.perfectClear) {
    game.level1GoalCounter = 0x80;
    game.level1GoalPhases = 2;
    game.level1BirdPhase = 2;
    game.level1BirdX = 0;
    // $7A40-$7A64 and the $7BC9 loop: eleven coin records ($2520-$25C0).
    // $2520 keeps timer $2C and drift +$3C; each following record takes the
    // previous timer minus four and a drift chained from the previous low
    // byte: ((previous + $5D) & $7F) - $40.
    int timer = 0x2c;
    int driftSeed = 0x3c;
    game.level1RewardCoins[0].timer = timer;
    game.level1RewardCoins[0].driftFixed = 0x3c;
    for (std::size_t index = 1; index < game.level1RewardCoins.size(); ++index) {
      auto& coin = game.level1RewardCoins[index];
      timer -= 4;
      coin.timer = timer;
      driftSeed = (driftSeed + 0x5d) & 0x7f;
      coin.driftFixed = static_cast<std::int32_t>(driftSeed) - 0x40;
      driftSeed = coin.driftFixed & 0xff;
    }
  }
  finishStage(game);
}

void updateLevel1Goal(Game& game) {
  // $7B3A-$7C41 once per frame while the crowd cheers.
  ++game.level1GoalCounter;
  if (game.level1GoalCounter >= 0x100) {
    game.level1GoalCounter = 0;
    --game.level1GoalPhases;
  }
  if (game.level1GoalPhases <= 0) {
    // Keep the crowd on screen until the coin-shower piece has finished, so
    // it never plays over the bonus count (the counter keeps the wings
    // flapping meanwhile).
    const bool holdForShower =
        game.level1BirdPhase == 1 &&
        game.goalFrame < game.level1ShowerStartFrame +
                             game.level1ShowerHoldFrames;
    if (!holdForShower) {
      game.scene = Scene::Tally;
      game.tallyFrame = 0;
      return;
    }
  }
  if (game.level1BirdPhase == 0) return;
  if (game.level1BirdPhase == 2) {
    // $7B90-$7BAF: the bird enters from the right one pixel per frame until
    // its bag reaches column $31.
    game.level1BirdX = static_cast<std::uint8_t>(0x80 - game.level1GoalCounter);
    // The bag cell sits at bird X - $10 + $08; the tear starts at column $31.
    if (static_cast<std::uint8_t>(game.level1BirdX - 0x08U) != 0x31U) return;
    game.level1BirdPhase = 1;
    game.level1ShowerStartFrame = game.goalFrame;
    ++game.level1ShowerAudioSerial;
    return;
  }
  // $7BC9-$7C23: coin launches and flight.
  for (auto& coin : game.level1RewardCoins) {
    if (coin.timer > 0) {
      if (--coin.timer > 0) continue;
      game.score += 40;
      ++game.level1RewardCoinLaunches;
      coin.active = true;
      coin.yFixed = 0x6f00;
      coin.xFixed = 0x3100;
      coin.velocityFixed = 0;
      coin.spin = 0;
      continue;
    }
    if (!coin.active) continue;
    coin.velocityFixed += 0x1c;
    const std::int32_t nextY = coin.yFixed + coin.velocityFixed;
    if (((nextY >> 8) & 0xff) > 0xd8) {
      game.score += 40;
      ++game.level1RewardCoinLaunches;
      coin.yFixed = 0x6f00;
      coin.xFixed = 0x3100;
      coin.velocityFixed = 0;
      coin.spin = 0;
      continue;
    }
    coin.yFixed = nextY;
    coin.xFixed += coin.driftFixed;
    coin.spin = (coin.spin + 1) % 0x18;
  }
}

void updateLevel1(Game& game, const Uint8* keyboard, bool jumpPressed,
                  float controllerAxis) {
  if (game.stage1ScorePopupFrame > 0) --game.stage1ScorePopupFrame;
  if (game.level1PotPopupTimer > 0) --game.level1PotPopupTimer;
  if (std::getenv("CIRCUS_L1_GOAL_TEST") && game.scene == Scene::Playing) {
    // Debug: jump straight to the perfect-clear goal to check the bird scene.
    game.level1MissedRewards = 0;
    enterLevel1Goal(game);
    return;
  }

  game.player.previous = game.player.position;
  game.previousCameraX = game.cameraX;

  const bool moveLeft = keyboard[SDL_SCANCODE_LEFT] ||
                        controllerAxis < -0.35F;
  const bool moveRight = keyboard[SDL_SCANCODE_RIGHT] ||
                         controllerAxis > 0.35F;
  const int liveDirection = moveLeft != moveRight ? (moveLeft ? -1 : 1) : 0;

  // ---- $70EB: hidden coin catch ------------------------------------------
  const int riderRowBefore = kLevel1RiderGroundSourceY - level1RiderDisplacement(game.player);
  if (game.level1CoinState == 1) {
    const int coinRow = static_cast<int>((game.level1CoinYFixed >> 8) & 0xff);
    const std::uint8_t riderB7 = static_cast<std::uint8_t>(riderRowBefore + 8);
    if (static_cast<std::uint8_t>(game.level1CoinX - 0x26U) < 0x20U &&
        static_cast<std::uint8_t>(coinRow + 0x0c - riderB7) < 0x14U) {
      game.level1CoinState = 2;
      game.level1CoinPopupTimer = 0x30;
      game.score += 3000;
      showStage1Score(game, 3000, level1ObjectWorldX(game, static_cast<std::uint16_t>(game.level1CoinX) << 8U),
                      kGroundY - (0xd0 - coinRow) * kSourceToLogicalY - 40.0F);
      ++game.hiddenCoinAudioSerial;
    }
  }

  // ---- $712D-$71C8: hoop scan --------------------------------------------
  for (std::size_t hoopIndex = 0; hoopIndex < game.hoops.size(); ++hoopIndex) {
    auto& hoop = game.hoops[hoopIndex];
    if (!hoop.active) continue;
    const int hoopSourceX = static_cast<int>(hoop.sourceXFixed >> 8U);
    const int horizontalDistance =
        std::abs(hoopSourceX - kLevel1RiderCollisionSourceX);
    if (horizontalDistance >= kLevel1HoopHorizontalLimit) continue;
    const bool trackedHoop =
        game.extraCharlieHoopIndex == static_cast<int>(hoopIndex);
    if (trackedHoop) {
      // $71A0: collection needs the rider row above $A8.  The record keeps
      // its slot (invisible) until it retires, exactly like <$BF.
      if (game.level1ExtraCharlieState == 2 && riderRowBefore < 0xa8) {
        game.extraCharlieActive = false;
        game.level1ExtraCharlieState = 3;
        ++game.lives;
        ++game.extraCharlieAudioSerial;
      }
      continue;
    }
    int riderRow = riderRowBefore;
    if (hoopIndex == 3) riderRow += kLevel1FourthHoopYOffset;
    const int verticalDistance = riderRow - kLevel1RiderCollisionBaseY;
    if (verticalDistance < 0) {
      if (hoopIndex == 3 && game.level1BagState == 0xff) {
        // $7157-$718A: the bag reward.
        game.level1BagState = 0x30;
        const int points = (game.level1PrizeState + 1) * 1000;
        game.level1PrizeState = std::min(4, game.level1PrizeState + 1);
        game.score += points;
        showStage1Score(game, points, game.bonusRings[3].worldX,
                        kGroundY - kBonusRingCenterHeight - 5.0F);
        ++game.prizeBagsCollected;
        ++game.prizeBagAudioSerial;
      }
      continue;
    }
    if (verticalDistance + horizontalDistance <= kLevel1HoopCombinedLimit) {
      crashPlayer(game);
      return;
    }
  }
  if (game.level1BagState != 0 && game.level1BagState != 0xff) {
    --game.level1BagState;
  }

  // ---- $71C8-$71E2: fire-pot collision ------------------------------------
  for (const auto& pot : game.level1Pots) {
    const std::uint8_t high = static_cast<std::uint8_t>(pot.sourceXFixed >> 8U);
    if (static_cast<std::uint8_t>(high - 0x29U) >= 0x2eU) continue;
    if (riderRowBefore >= 0xc6) {
      crashPlayer(game);
      return;
    }
  }

  // ---- $71E4-$7335: jump physics, goal, landing ---------------------------
  const std::uint8_t offsetByteBeforeScroll =
      level1PageOffsetByte(game.level1ProgressFixed);
  bool jumpActivatedThisFrame = false;
  if (!game.player.grounded) {
    const int nextFrame = std::min(
        game.player.jumpFrame + 1,
        static_cast<int>(kJumpSourceDisplacement.size()) - 1);
    const float previousDisplacement =
        static_cast<float>(kJumpSourceDisplacement[static_cast<std::size_t>(game.player.jumpFrame)]) *
        kSourceToLogicalY;
    const int sample = kJumpSourceDisplacement[static_cast<std::size_t>(nextFrame)];
    const float displacement = static_cast<float>(sample) * kSourceToLogicalY;
    game.player.jumpFrame = nextFrame;
    game.player.position.y = kGroundY - displacement;
    game.player.verticalVelocity =
        (previousDisplacement - displacement) / static_cast<float>(kFixedDt);
    // $7202-$7213: goal arrival.
    if (game.level1ProgressFixed > kLevel1GoalProgressFixed &&
        kLevel1RiderGroundSourceY - sample >= 0xc5) {
      enterLevel1Goal(game);
      return;
    }
    // $7235-$7254: the joystick and a fresh jump press are re-sampled in the
    // last four source pixels of the descent.
    if (nextFrame >= 62 && sample <= 4) {
      game.player.level1BufferedDirection = liveDirection;
      if (jumpPressed) game.player.level1JumpBuffered = true;
    }
    if (nextFrame == static_cast<int>(kJumpSourceDisplacement.size()) - 1) {
      game.player.position.y = kGroundY;
      game.player.verticalVelocity = 0.0F;
      game.player.jumpFrame = -1;
      game.player.grounded = true;
      finishLevel1Landing(game, game.player.level1AirborneDirection);
      if (game.scene != Scene::Playing) return;
      if (game.player.level1JumpBuffered) {
        // $7344-$734E: $2246 restarts the jump on the landing tick using the
        // direction re-sampled during the descent.
        game.player.level1JumpBuffered = false;
        beginLevel1Jump(game, game.player.level1BufferedDirection);
        jumpActivatedThisFrame = true;
      }
    }
  }

  // ---- $7344-$7363: movement command ---------------------------------------
  int effectiveDirection = liveDirection;
  if (!game.player.grounded) {
    effectiveDirection = game.player.level1AirborneDirection;
  }
  if (game.player.grounded && !jumpActivatedThisFrame && jumpPressed) {
    // $7363-$736C: the press edge starts the jump in the same board update;
    // the first displacement sample follows one update later because $71E4
    // has already run.  The manual MAME capture shows $20B0 set in the frame
    // of the press and row $CC in the next.
    beginLevel1Jump(game, liveDirection);
    jumpActivatedThisFrame = true;
    effectiveDirection = liveDirection;
  }
  const std::int32_t command = level1MovementCommand(effectiveDirection);
  game.player.runSpeed =
      effectiveDirection > 0 ? kForwardSpeed : (effectiveDirection < 0 ? kBackSpeed : 0.0F);
  if (effectiveDirection > 0) game.level1ForwardProgressed = true;

  // ---- $73DC-$7422: grounded run cycle -------------------------------------
  if (game.player.grounded && !jumpActivatedThisFrame) {
    updateLevel1RiderAnimation(game, liveDirection, offsetByteBeforeScroll);
  }

  // ---- $7425-$744B: scroll -------------------------------------------------
  const int pageBeforeScroll = level1Page(game.level1ProgressFixed);
  if (command < 0) {
    game.level1ProgressFixed -= command;
  } else if (command > 0 && pageBeforeScroll != 0) {
    game.level1ProgressFixed -= command;
  }
  game.player.position.x =
      kLevel1PlayerStartX + level1ProgressPixels(game) * kSourceToWorldX;
  game.cameraX = std::max(0.0F, game.player.position.x - kLevel1PlayerStartX);
  game.level1ForwardProgressed = level1Page(game.level1ProgressFixed) != 0;

  // ---- $751E-$7536: scheduler, re-entry, object movement ------------------
  {
    // $7539/$7607/$76FE read the command through the page test of the
    // scroll value updated by this tick.
    std::int32_t objectDelta = command;
    if (command > 0 && level1Page(game.level1ProgressFixed) == 0) objectDelta = 0;
    updateLevel1Scheduler(game, objectDelta);
    updateLevel1ReEntry(game, objectDelta);
    for (std::size_t index = 0; index < game.hoops.size(); ++index) {
      if (!game.hoops[index].active) continue;
      moveLevel1Hoop(game, index, objectDelta);
    }
    // ---- $7750: pots ----
    std::int32_t potCommand = command;
    updateLevel1Pots(game, potCommand);
    updateLevel1Coin(game);
  }

  for (auto& hoop : game.hoops) hoop.previousWorldX = hoop.worldX;
  syncLevel1World(game);

  if (game.bonus > 0) --game.bonus;
}

// Attract clips: assets/attract/stageN/frame-NNNN.jpg, recorded with
// --record at kAttractFps.  The title waits kAttractIdleFrames with no
// coin before the first clip and kAttractTitleFrames between clips.
constexpr int kAttractFps = 20;
constexpr int kBetweenFps = 30;
constexpr float kAttractFadeFrames = 30.0F;   // half a second in and out
constexpr int kAttractIdleFrames = 60 * 3;
constexpr int kGameOverFrames = 60 * 4;

std::string attractFramePath(int clip, int frame) {
  std::array<char, 96> name{};
  std::snprintf(name.data(), name.size(), "attract/stage%d/frame-%04d.jpg", clip + 1, frame);
  return name.data();
}

std::string assetPathFor(const std::string& relative) {
  const std::string local = "assets/" + relative;
  if (std::filesystem::exists(local)) return local;
  char* basePath = SDL_GetBasePath();
  if (!basePath) return local;
  const std::string executablePath = std::string(basePath) + "assets/" + relative;
  SDL_free(basePath);
  return executablePath;
}

std::string betweenFramePath(int frame) {
  std::array<char, 64> name{};
  std::snprintf(name.data(), name.size(), "between/between-%04d.jpg", frame);
  return name.data();
}

int countFrames(const std::function<std::string(int)>& pathOf) {
  int count = 0;
  while (count < 20000 && std::filesystem::exists(assetPathFor(pathOf(count)))) ++count;
  return count;
}

int attractClipFrameCount(int clip) {
  int count = 0;
  while (count < 10000 && std::filesystem::exists(assetPathFor(attractFramePath(clip, count)))) {
    ++count;
  }
  return count;
}

// The cabinet's loop: the title footage (stars, ranking), a stage demo,
// the footage again, the next demo...  Events without a clip are skipped;
// with no footage at all the loop is the demos alone.
void nextAttractSegment(Game& game) {
  game.attractTick = 0;
  const bool wantDemo = game.attractBetween || gBetweenFrameCount <= 0;
  if (wantDemo) {
    // After the footage (or with none), the next event that has a clip.
    for (int tries = 0; tries < kEventCount; ++tries) {
      game.attractClip = (game.attractClip + 1) % kEventCount;
      const int count = attractClipFrameCount(game.attractClip);
      if (count > 0) {
        game.attractBetween = false;
        game.attractFrameCount = count;
        return;
      }
    }
    if (gBetweenFrameCount <= 0) {
      // Nothing to show at all: the coin screen.
      game.scene = Scene::Title;
      game.titleIdleFrames = 0;
      return;
    }
  }
  // After a demo (or with no demos), the title footage.
  game.attractBetween = true;
  game.attractFrameCount = gBetweenFrameCount;
  ++game.betweenAudioSerial;
}

void startAttract(Game& game) {
  game.scene = Scene::Attract;
  game.titleIdleFrames = 0;
  game.attractTick = 0;
  // A demo first (the boot footage already showed the title), then the
  // title footage between the demos.
  game.attractBetween = true;
  nextAttractSegment(game);
}

void updateGame(Game& game, const Uint8* keyboard, bool jumpPressed,
                float controllerAxis) {
  // <$14 is the board's free-running frame byte.  It is not reset when a
  // Level 1 course starts or after Charlie dies; $76E1 samples it when the
  // reserved $2760 small/prize-ring record is admitted.
  ++game.level1BoardFrameByte;

  if (game.scene == Scene::Boot) {
    ++game.bootFrame;
    if (game.bootFrame >= bootDurationFrames()) {
      if (game.credits > 0) {
        game.scene = Scene::Title;
      } else {
        startAttract(game);
      }
    }
    return;
  }
  if (game.scene == Scene::EventSelect) {
    ++game.eventSelectFrame;
    if (game.eventSelectFrame >= game.eventSelectDurationFrames) {
      confirmEventSelection(game);
    }
    return;
  }
  if (game.scene == Scene::Title) {
    // The coin screen: with no credit it falls back into the attract loop
    // after a moment, the way the cabinet goes on with its title footage.
    if (game.credits > 0) {
      game.titleIdleFrames = 0;
    } else if (++game.titleIdleFrames >= kAttractIdleFrames) {
      startAttract(game);
    }
    return;
  }
  if (game.scene == Scene::GameOver) {
    if (++game.gameOverFrame >= kGameOverFrames) {
      if (game.credits > 0) {
        game.scene = Scene::Title;
        game.titleIdleFrames = 0;
      } else {
        startAttract(game);
      }
    }
    return;
  }
  if (game.scene == Scene::Attract) {
    if (game.attractTick == 0 && std::getenv("CIRCUS_TRACE_SCENES")) {
      std::cerr << (game.attractBetween ? "attract: title footage" : "attract: demo of event ")
                << (game.attractBetween ? "" : std::to_string(game.attractClip + 1))
                << " (" << game.attractFrameCount << " frames)\n";
    }
    ++game.attractTick;
    const int fps = game.attractBetween ? kBetweenFps : kAttractFps;
    const int frame = game.attractTick * fps / static_cast<int>(kBoardRefresh);
    if (frame >= game.attractFrameCount) nextAttractSegment(game);
    return;
  }

  if (game.scene == Scene::Crashed) {
    game.player.previous = game.player.position;
    game.previousCameraX = game.cameraX;
    game.player.runSpeed = 0.0F;
    game.player.verticalVelocity = 0.0F;
    ++game.crashFrame;
    if (game.selectedEvent == 3)
      game.stage4FallFrame = std::min(game.stage4FallFrame + 1, 23);
    if (game.selectedEvent == 3) {
      // The original object engine keeps advancing the rolling-ball slots
      // during Charlie's fall. Freezing them here caused an immediate repeat
      // collision on restart and made the failure look unlike the cabinet.
      for (auto& ball : game.stage4Balls) {
        if (!ball.active) continue;
        ball.worldX += ball.velocity * static_cast<float>(kFixedDt);
        ball.rotation += ball.velocity * static_cast<float>(kFixedDt) /
                         kStage4BallRadius;
      }
    }
    // The cabinet owns the failure sequence. Player input cannot dismiss it;
    // Charlie returns only after the complete failure cue has played once.
    // Level 1: 64 burning frames (<$CB) plus the 96-frame restart phase
    // measured in the headless MAME failure run (death at 1463, control back
    // at 1623).
    const int failureFrames =
        game.selectedEvent == 0 ? kLevel1FailureFrames : game.crashDurationFrames;
    if (game.crashFrame >= failureFrames) {
      restartAfterCrash(game);
      if (game.selectedEvent == 3 && game.scene == Scene::Playing) {
        // $9883 initialises the board and runs the first riding tick in the
        // same frame, so the capture already shows one column of scroll.
        initializeLevel4Board(game, game.level4RestartPage);
        updateLevel4(game, keyboard, jumpPressed, controllerAxis);
      }
      if (game.selectedEvent == 2 && game.scene == Scene::Playing) {
        // $8C61 initialises the board and runs the first rebound tick in
        // the same frame (the capture shows row $B0 on the re-init frame).
        updateLevel3(game, keyboard, controllerAxis);
      }
      if (game.selectedEvent == 1 && game.scene == Scene::Playing) {
        // $8136 initialises the board and runs the first walking tick in the
        // same frame.
        initializeLevel2Board(game, game.level2RestartPage);
        updateLevel2(game, keyboard, jumpPressed, controllerAxis, true);
      }
      if (game.selectedEvent == 4 && game.scene == Scene::Playing) {
        // $9FF9 runs alone on its frame; the first tick is the next one.
        level5Init(game);
        syncLevel5World(game);
      }
      if (game.selectedEvent == 5 && game.scene == Scene::Playing) {
        level6Init(game);
        syncLevel6World(game);
      }
    }
    return;
  }

  if (game.scene == Scene::Goal && game.selectedEvent == 3) {
    updateLevel4(game, keyboard, jumpPressed, controllerAxis);
    return;
  }

  if (game.scene == Scene::Goal && game.selectedEvent == 4) {
    updateLevel5(game, keyboard, jumpPressed, controllerAxis);
    return;
  }
  if (game.scene == Scene::Goal && game.selectedEvent == 5) {
    updateLevel6(game, keyboard, jumpPressed, controllerAxis);
    return;
  }

  if (game.scene == Scene::Goal && game.selectedEvent == 1) {
    // $8FD7: the stage handler keeps running through the celebration.
    updateLevel2(game, keyboard, jumpPressed, controllerAxis);
    return;
  }

  if (game.scene == Scene::Goal && game.selectedEvent == 2) {
    // $8FD7: the stage handler keeps running through the celebration.
    updateLevel3(game, keyboard, controllerAxis);
    return;
  }

  if (game.scene == Scene::Goal) {
    game.player.previous = game.player.position;
    game.previousCameraX = game.cameraX;
    game.player.runSpeed = 0.0F;
    const bool stage2 = game.selectedEvent == 1;
    const bool stage3 = false;
    const bool stage4 = game.selectedEvent == 3;
    game.player.position.y = stage2 ? kStage2GoalTopY
                                    : (stage4 ? kStage4GoalTopY
                                              : kGoalLandingY);
    game.player.previous.y = game.player.position.y;
    game.player.grounded = true;
    game.player.jumpFrame = -1;
    game.cameraX =
        game.player.position.x -
        (stage2 ? 340.0F
                : (stage4 ? kStage4GoalScreenX : kGoalScreenX));
    if (game.selectedEvent == 0) {
      // The board freezes the scroll where the goal triggered; the rider stays
      // at his fixed column.
      game.cameraX = std::max(0.0F, game.player.position.x - kLevel1PlayerStartX);
    }
    game.previousCameraX = game.cameraX;
    ++game.goalFrame;
    if (game.selectedEvent == 0) {
      updateLevel1Goal(game);
      return;
    }

    const int showerStart =
        kGoalArrivalFrames + kBirdArrivalFrames + kBagDropFrames;
    if (game.perfectClear && game.goalFrame >= showerStart) {
      const int targetCoinCount = std::clamp(
          (game.goalFrame - showerStart) / 10, 0, kRewardCoinCount);
      if (targetCoinCount > game.rewardCoinsAwarded) {
        game.score +=
            (targetCoinCount - game.rewardCoinsAwarded) * 100;
        game.rewardCoinsAwarded = targetCoinCount;
      }
    }

    const int presentationFrames = stage2
        ? 210
        : game.perfectClear
            ? showerStart + kCoinShowerFrames
            : ((stage3 || stage4) ? 210 : kGoalArrivalFrames + 120);
    if (game.goalFrame >= presentationFrames) {
      game.scene = Scene::Tally;
      game.tallyFrame = 0;
    }
    return;
  }

  if (game.scene == Scene::Tally) {
    ++game.tallyFrame;
    if (!game.timeScoreApplied && game.tallyFrame >= 150) {
      game.clearBonus = timeBonusFor(game.bonus);
      game.score += game.clearBonus;
      game.timeScoreApplied = true;
    }
    if (game.tallyFrame >= 280) {
      game.scene = Scene::Complete;
      game.completeFrame = 0;
    }
    return;
  }
  if (game.scene == Scene::Complete) {
    // The run goes straight on to the next event.
    if (++game.completeFrame >= 90 && game.arcadeRun) advanceAfterClear(game);
    return;
  }

  if (game.scene != Scene::Playing) return;

  if (game.selectedEvent == 1) {
    updateLevel2(game, keyboard, jumpPressed, controllerAxis);
    return;
  }
  if (game.selectedEvent == 2) {
    updateLevel3(game, keyboard, controllerAxis);
    return;
  }
  if (game.selectedEvent == 3) {
    updateLevel4(game, keyboard, jumpPressed, controllerAxis);
    return;
  }
  if (game.selectedEvent == 4) {
    updateLevel5(game, keyboard, jumpPressed, controllerAxis);
    return;
  }
  if (game.selectedEvent == 5) {
    updateLevel6(game, keyboard, jumpPressed, controllerAxis);
    return;
  }

  updateLevel1(game, keyboard, jumpPressed, controllerAxis);
}

// The portrait size the frame is rendered at; 0 means the display's size.
int gRenderWidth = 480;
int gRenderHeight = 640;

RenderSurface buildRenderSurface(SDL_Renderer* renderer, SDL_Window* window,
                                 int rotation, RenderSurface oldSurface) {
  if (oldSurface.texture) SDL_DestroyTexture(oldSurface.texture);

  int outputWidth = 0;
  int outputHeight = 0;
  SDL_GetRendererOutputSize(renderer, &outputWidth, &outputHeight);
  if (outputWidth <= 0 || outputHeight <= 0) {
    SDL_GetWindowSize(window, &outputWidth, &outputHeight);
  }

  const bool rotated = rotation == 90 || rotation == 270;
  const float presentedAspect =
      rotated ? static_cast<float>(kWorldHeight) / kWorldWidth
              : static_cast<float>(kWorldWidth) / kWorldHeight;
  int destinationWidth = outputWidth;
  int destinationHeight =
      static_cast<int>(std::lround(destinationWidth / presentedAspect));
  if (destinationHeight > outputHeight) {
    destinationHeight = outputHeight;
    destinationWidth =
        static_cast<int>(std::lround(destinationHeight * presentedAspect));
  }

  RenderSurface surface;
  surface.destination = {
      (outputWidth - destinationWidth) / 2,
      (outputHeight - destinationHeight) / 2,
      destinationWidth,
      destinationHeight,
  };
  surface.width = rotated ? destinationHeight : destinationWidth;
  surface.height = rotated ? destinationWidth : destinationHeight;
  surface.width = std::max(surface.width, 240);
  surface.height = std::max(surface.height, 320);
  // Never render more pixels than the requested portrait resolution; the
  // scaled copy below fills the display.
  if (gRenderWidth > 0 && gRenderHeight > 0) {
    surface.width = std::min(surface.width, gRenderWidth);
    surface.height = std::min(surface.height, gRenderHeight);
  }
  surface.texture = SDL_CreateTexture(
      renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET,
      surface.width, surface.height);
  if (surface.texture) {
    SDL_SetTextureScaleMode(
        surface.texture,
        surface.height <= 320 ? SDL_ScaleModeNearest : SDL_ScaleModeLinear);
  }
  return surface;
}

void drawCeilingTrack(SDL_Renderer* renderer, float cameraX) {
  // The cabinet uses a slim blue multi-line tube under the crowd fascia.
  // Preserve that silhouette in HD instead of drawing railroad-like ties.
  fillRect(renderer, 0.0F, kTrackY - 2.0F, kWorldWidth, 8.0F,
           color(8, 24, 92));
  fillRect(renderer, 0.0F, kTrackY, kWorldWidth, 4.0F,
           color(31, 83, 202));
  fillRect(renderer, 0.0F, kTrackY, kWorldWidth, 1.25F,
           color(92, 206, 255));
  fillRect(renderer, 0.0F, kTrackY + 4.0F, kWorldWidth, 1.0F,
           color(17, 42, 132));
  for (int clamp = -1; clamp < 14; ++clamp) {
    const float x = static_cast<float>(clamp * 42) -
                    std::fmod(cameraX, 42.0F);
    fillRect(renderer, x, kTrackY - 3.0F, 4.0F, 10.0F,
             color(16, 35, 93));
    fillRect(renderer, x + 1.0F, kTrackY - 2.0F, 1.0F, 8.0F,
             color(76, 130, 225));
  }
}

void drawFerrisWheel(SDL_Renderer* renderer, SDL_Texture* wheelTexture,
                     SDL_Texture* gondolaTexture, double timeSeconds) {
  if (!wheelTexture || !gondolaTexture) return;

  constexpr float centerX = 424.0F;
  constexpr float centerY = 55.0F;
  constexpr float wheelSize = 96.0F;
  constexpr float gondolaRadius = 39.0F;
  constexpr int gondolaCount = 10;
  const float angleDegrees =
      std::fmod(static_cast<float>(timeSeconds) * 10.0F, 360.0F);
  const float angleRadians = angleDegrees * kPi / 180.0F;

  const SDL_FRect wheelDestination{
      centerX - wheelSize * 0.5F,
      centerY - wheelSize * 0.5F,
      wheelSize,
      wheelSize,
  };
  SDL_RenderCopyExF(renderer, wheelTexture, nullptr, &wheelDestination,
                    angleDegrees, nullptr, SDL_FLIP_NONE);

  // The wheel rotates, but gravity keeps every passenger gondola upright.
  // Drawing them independently preserves the original marquee's readable
  // animation instead of spinning the cabins upside down with the rim.
  for (int index = 0; index < gondolaCount; ++index) {
    const float theta =
        angleRadians +
        static_cast<float>(index) * 2.0F * kPi /
            static_cast<float>(gondolaCount);
    const float x = centerX + std::cos(theta) * gondolaRadius;
    const float y = centerY + std::sin(theta) * gondolaRadius;
    const SDL_FRect gondolaDestination{x - 7.5F, y - 5.0F, 15.0F, 22.5F};
    SDL_RenderCopyF(renderer, gondolaTexture, nullptr,
                    &gondolaDestination);
  }
}

void drawZeppelinBonus(SDL_Renderer* renderer, int bonus) {
  // The zeppelin's cream panel (stage1-marquee-v2.png) sits between the
  // light strings at 57-133 by 46-70 on the canvas: the number on its centre.
  constexpr float kZeppelinPanelCenterX = 95.0F;
  constexpr float kZeppelinPanelCenterY = 58.0F;
  constexpr float kDigitScale = 1.45F;
  drawText(renderer, "BONUS", kZeppelinPanelCenterX, 34.0F, 1.0F,
           color(74, 111, 229), true);
  if (bonus < 0) return;
  std::ostringstream value;
  value << std::setw(4) << std::setfill('0') << bonus;
  drawText(renderer, value.str(), kZeppelinPanelCenterX,
           kZeppelinPanelCenterY - 7.0F * kDigitScale * 0.5F, kDigitScale,
           color(255, 255, 255), true);
}

void drawReddishMarqueeSky(SDL_Renderer* renderer) {
  // A translucent carnival-red wash brings the HD night painting closer to
  // the original board's red sky without replacing the existing tent art.
  fillRect(renderer, 0.0F, 0.0F, kWorldWidth, kMarqueeHeight,
           color(112, 12, 18, 64));
}

// The fixed band above the score panel: the tent's crown and red sky, the
// wheel and the bonus zeppelin.  Every event shares it.
void drawMarqueeBand(SDL_Renderer* renderer, bool lowDetail,
                     const Assets& assets, const Game& game,
                     double timeSeconds) {
  if (assets.marquee) {
    const SDL_FRect marqueeDestination{
        0.0F, 0.0F, static_cast<float>(kWorldWidth), kMarqueeHeight};
    SDL_RenderCopyF(renderer, assets.marquee, nullptr,
                    &marqueeDestination);
    drawReddishMarqueeSky(renderer);
  } else {
    if (!lowDetail) {
      for (int index = 0; index < 24; ++index) {
        const float x =
            std::fmod(static_cast<float>(index * 79), 520.0F) - 20.0F;
        const float y = 8.0F + static_cast<float>((index * 47) % 92);
        filledCircle(renderer, x, y, index % 5 == 0 ? 1.5F : 0.8F,
                     color(170, 215, 255));
      }
    }
    fillRect(renderer, 0.0F, kMarqueeHeight - 8.0F, kWorldWidth, 8.0F,
             color(118, 22, 38));
  }
  drawFerrisWheel(renderer, assets.ferrisWheel, assets.ferrisGondola,
                  timeSeconds);
  drawZeppelinBonus(renderer, game.bonus);
}

// The arcade wall has the performers' entrance (the elephant's door) in
// every 256-column page; here it comes round every third page so it is a
// landmark rather than wallpaper, standing from the top of the stands
// (`top`) to the grass (`bottom`), in every event with the crowd wall.
void drawElephantDoors(SDL_Renderer* renderer, SDL_Texture* door, float cameraX,
                       float top, float bottom) {
  if (!door) return;
  int doorWidth = 0;
  int doorHeight = 0;
  SDL_QueryTexture(door, nullptr, nullptr, &doorWidth, &doorHeight);
  const float height = bottom - top;
  const float width = height * static_cast<float>(doorWidth) /
                      static_cast<float>(std::max(1, doorHeight));
  const float spacing = kElephantDoorSpacingColumns * kSourceToWorldX;
  const float first = kElephantDoorColumn * kSourceToWorldX;
  const int from = static_cast<int>(std::floor((cameraX - width - first) / spacing));
  const int to = static_cast<int>(std::ceil((cameraX + kWorldWidth + width - first) / spacing));
  for (int k = std::max(0, from); k <= to; ++k) {
    const float x = first + static_cast<float>(k) * spacing - cameraX;
    const SDL_FRect destination{x - width * 0.5F, top, width, height};
    SDL_RenderCopyF(renderer, door, nullptr, &destination);
  }
}

// The stands and the grass from the arena painting, without the valance
// under the grass (the cabinet has none): the crowd at `scaleY` units per
// painting row from `top`, then the grass strip stretched to the bottom.
void drawArenaBand(SDL_Renderer* renderer, SDL_Texture* arena, SDL_Texture* door,
                   float cameraX, float top, float scaleY) {
  int textureWidth = 0;
  int textureHeight = 0;
  SDL_QueryTexture(arena, nullptr, nullptr, &textureWidth, &textureHeight);
  const int sourceTop = static_cast<int>(static_cast<float>(textureHeight) * kArenaContentSourceTop);
  const int grassTop = static_cast<int>(static_cast<float>(textureHeight) * kArenaGrassSourceTop);
  const int grassBottom = static_cast<int>(static_cast<float>(textureHeight) * kArenaContentSourceBottom);
  const SDL_Rect crowd{0, sourceTop, textureWidth, grassTop - sourceTop};
  const SDL_Rect grass{0, grassTop, textureWidth, grassBottom - grassTop};
  const float crowdBottom = top + static_cast<float>(grassTop - sourceTop) * scaleY;
  const float tileWidth = static_cast<float>(kWorldWidth);
  float scroll = std::fmod(cameraX, tileWidth);
  if (scroll < 0.0F) scroll += tileWidth;
  for (int tile = -1; tile <= 1; ++tile) {
    const float x = static_cast<float>(tile) * tileWidth - scroll;
    const SDL_FRect crowdDst{x, top, tileWidth, crowdBottom - top};
    const SDL_FRect grassDst{x, crowdBottom, tileWidth, kWorldHeight - crowdBottom};
    SDL_RenderCopyF(renderer, arena, &crowd, &crowdDst);
    SDL_RenderCopyF(renderer, arena, &grass, &grassDst);
  }
  drawElephantDoors(renderer, door, cameraX, top, crowdBottom);
}

void drawBackdrop(SDL_Renderer* renderer, float cameraX, bool lowDetail,
                  const Assets& assets, const Game& game,
                  double timeSeconds, bool showCeilingTrack = true) {
  fillRect(renderer, 0.0F, 0.0F, kWorldWidth, kWorldHeight,
           color(5, 18, 51));

  // Original layer model: the zeppelin/tent crown, wheel, and black score
  // panel stay fixed. Only the lower arena wall, crowd, floor, and meter
  // markers travel with Charlie.
  drawMarqueeBand(renderer, lowDetail, assets, game, timeSeconds);

  if (assets.arena) {
    int textureWidth = 0;
    int textureHeight = 0;
    SDL_QueryTexture(assets.arena, nullptr, nullptr, &textureWidth,
                     &textureHeight);
    const int crowdSourceTop = static_cast<int>(
        static_cast<float>(textureHeight) * kArenaContentSourceTop);
    const int grassSourceTop = static_cast<int>(
        static_cast<float>(textureHeight) * kArenaGrassSourceTop);
    const int grassSourceBottom = static_cast<int>(
        static_cast<float>(textureHeight) * kArenaContentSourceBottom);
    const SDL_Rect crowdSource{
        0, crowdSourceTop, textureWidth,
        std::max(1, grassSourceTop - crowdSourceTop)};
    const SDL_Rect grassSource{
        0, grassSourceTop, textureWidth,
        std::max(1, grassSourceBottom - grassSourceTop)};
    const float tileWidth = static_cast<float>(kWorldWidth);
    const float scroll = std::fmod(cameraX, tileWidth);
    for (int tile = -1; tile <= 1; ++tile) {
      const float tileX = static_cast<float>(tile) * tileWidth - scroll;
      const SDL_FRect crowdDestination{
          tileX, kCrowdTop, tileWidth, kGrassTop - kCrowdTop};
      const SDL_FRect grassDestination{
          tileX, kGrassTop, tileWidth,
          static_cast<float>(kWorldHeight) - kGrassTop};
      SDL_RenderCopyF(renderer, assets.arena, &crowdSource,
                      &crowdDestination);
      SDL_RenderCopyF(renderer, assets.arena, &grassSource,
                      &grassDestination);
    }
    drawElephantDoors(renderer, assets.elephantDoor, cameraX, kCrowdTop, kGrassTop);
    const bool brightCheer =
        game.scene == Scene::Goal && game.goalFrame > 30 &&
        ((game.goalFrame / 8) & 1) != 0;
    if (brightCheer) {
      // The arcade flashes the same audience between its normal and brighter
      // palette during the finish cheer. Do not add invented spectator art.
      SDL_SetTextureBlendMode(assets.arena, SDL_BLENDMODE_ADD);
      SDL_SetTextureAlphaMod(assets.arena, 76);
      for (int tile = -1; tile <= 1; ++tile) {
        const float tileX = static_cast<float>(tile) * tileWidth - scroll;
        const SDL_FRect crowdDestination{
            tileX, kCrowdTop, tileWidth, kGrassTop - kCrowdTop};
        SDL_RenderCopyF(renderer, assets.arena, &crowdSource,
                        &crowdDestination);
      }
      SDL_SetTextureAlphaMod(assets.arena, 255);
      SDL_SetTextureBlendMode(assets.arena, SDL_BLENDMODE_BLEND);
    }
  } else {
    fillRect(renderer, 0.0F, kArenaTop, kWorldWidth,
             kWorldHeight - kArenaTop, color(24, 25, 45));
    const float crowdScroll = std::fmod(cameraX, 23.0F);
    for (int row = 0; row < (lowDetail ? 3 : 5); ++row) {
      for (int column = -1; column < 23; ++column) {
        const float x = static_cast<float>(column * 23) - crowdScroll +
                        static_cast<float>((row % 2) * 9);
        const float y = kCrowdTop + 18.0F + static_cast<float>(row * 21);
        const bool brightCheer =
            game.scene == Scene::Goal && game.goalFrame > 30 &&
            ((game.goalFrame / 8) & 1) != 0;
        filledCircle(renderer, x, y, 5.0F,
                     brightCheer ? color(110 + row * 8, 102, 118)
                                 : color(45 + row * 8, 50, 72));
      }
    }
    fillRect(renderer, 0.0F, kGrassTop - 12.0F, kWorldWidth, 12.0F,
             color(112, 23, 37));
    fillRect(renderer, 0.0F, kGrassTop, kWorldWidth,
             kWorldHeight - kGrassTop,
             color(38, 142, 46));
    for (int stripe = 0; stripe < kWorldWidth / 28 + 2; ++stripe) {
      const float x = static_cast<float>(stripe * 28) -
                      std::fmod(cameraX, 28.0F);
      line(renderer, x, kGrassTop + 4.0F, x - 22.0F,
           static_cast<float>(kWorldHeight),
           color(68, 171, 59));
    }
  }

  // This gameplay rail belongs to the scrolling arena, while every fire ring
  // also has its own measured independent motion along it.
  if (showCeilingTrack) drawCeilingTrack(renderer, cameraX);
}

void drawHoop(SDL_Renderer* renderer, const Hoop& hoop, float cameraX,
              bool lowDetail, SDL_Texture* hoopTexture) {
  const float x = hoop.worldX - cameraX;
  if (x < -80.0F || x > kWorldWidth + 80.0F) return;
  const float centerY = (hoop.openingTop + hoop.openingBottom) * 0.5F;
  const float radiusY = (hoop.openingBottom - hoop.openingTop) * 0.5F;

  if (hoopTexture) {
    int textureWidth = 0;
    int textureHeight = 0;
    SDL_QueryTexture(hoopTexture, nullptr, nullptr, &textureWidth,
                     &textureHeight);

    // The generated source includes a long decorative pole. The arcade ring
    // rides close beneath its ceiling tube, so render the trolley and hoop as
    // separate source regions with a short connecting hanger.
    const SDL_Rect trolleySource{
        static_cast<int>(textureWidth * 0.34F),
        static_cast<int>(textureHeight * 0.02F),
        static_cast<int>(textureWidth * 0.32F),
        static_cast<int>(textureHeight * 0.13F),
    };
    const SDL_FRect trolleyDestination{x - 18.0F, kTrackY - 12.0F, 36.0F,
                                      30.0F};
    SDL_RenderCopyF(renderer, hoopTexture, &trolleySource,
                    &trolleyDestination);

    const float ringTop = hoop.openingTop - 25.0F;
    fillRect(renderer, x - 2.0F, kTrackY + 10.0F, 4.0F,
             std::max(0.0F, ringTop - kTrackY - 6.0F),
             color(184, 151, 83));
    // The HD ring is painted in three-quarter view, so unlike the 16-pixel
    // arcade tile it has a visible near rim and far rim.  The far (left) half
    // is drawn here, behind the rider; the near (right) half follows him in
    // drawHoopForeground.  Drawing the whole oval in front read as the lion
    // passing beside the ring instead of through it.
    const SDL_Rect ringSource{
        0,
        static_cast<int>(textureHeight * (500.0F / 1774.0F)),
        textureWidth / 2,
        static_cast<int>(textureHeight * (1120.0F / 1774.0F)),
    };
    const float ringBottom = hoop.openingBottom + 20.0F;
    const SDL_FRect ringDestination{
        x - kBigRingVisualHalfWidth, ringTop,
        kBigRingVisualHalfWidth, ringBottom - ringTop};
    SDL_RenderCopyF(renderer, hoopTexture, &ringSource, &ringDestination);
  } else {
    // Each ring travels with a hanger riding the ceiling track.
    fillRect(renderer, x - 5.0F, kTrackY + 4.0F, 10.0F,
             hoop.openingTop - kTrackY - 4.0F, color(91, 101, 116));
    fillRect(renderer, x - 12.0F, kTrackY - 2.0F, 24.0F, 14.0F,
             color(58, 66, 81));
    fillRect(renderer, x - 8.0F, hoop.openingTop - 5.0F, 16.0F, 11.0F,
             color(119, 44, 34));
    ellipse(renderer, x, centerY, 34.0F, radiusY, color(255, 169, 42),
            lowDetail ? 3 : 5);
    ellipse(renderer, x, centerY, 29.0F, radiusY - 5.0F,
            color(159, 29, 27), 2);
  }
}

void drawCoin(SDL_Renderer* renderer, float x, float y,
              float squash = 1.0F);

void drawStageProps(SDL_Renderer* renderer, const Game& game, float cameraX,
                    SDL_Texture* propsTexture,
                    SDL_Texture* rewardBagTexture) {
  if (!propsTexture) return;
  int textureWidth = 0;
  int textureHeight = 0;
  SDL_QueryTexture(propsTexture, nullptr, nullptr, &textureWidth,
                   &textureHeight);
  const int cellWidth = textureWidth / 3;
  const SDL_Rect fireSource{0, 0, cellWidth, textureHeight};
  const SDL_Rect bagSource{cellWidth, 0, cellWidth, textureHeight};
  for (const auto& firePot : game.level1Pots) {
    if (!firePot.visible()) continue;
    const float screenX = firePot.worldX - cameraX;
    if (screenX < -80.0F || screenX > kWorldWidth + 80.0F) continue;
    const SDL_FRect destination{screenX - 31.0F, kGroundY - 102.0F, 62.0F,
                                124.0F};
    SDL_RenderCopyF(renderer, propsTexture, &fireSource, &destination);
  }
  // $2580 hidden coin: it rides with its pot before the launch and spins
  // through the $F9CE frames while in flight.
  if (game.level1CoinPot >= 0 && game.level1CoinState == 1) {
    const int row = static_cast<int>((game.level1CoinYFixed >> 8) & 0xff);
    if (row < 0xd6) {
      const float coinScreenX =
          kLevel1RiderCollisionScreenX +
          (static_cast<float>(game.level1CoinX) - 64.0F) * kSourceToWorldX;
      const float coinY = level1RowToWorldY(static_cast<float>(row)) - 12.0F;
      const float flip = std::max(
          0.16F, std::abs(std::cos(static_cast<float>(game.level1CoinSpin) *
                                  (kPi / 12.0F))));
      drawCoin(renderer, coinScreenX, coinY, flip);
    }
  }

  for (const auto& ring : game.bonusRings) {
    if (!ring.active) continue;
    const float screenX = ring.worldX - cameraX;
    if (screenX < -100.0F || screenX > kWorldWidth + 100.0F) continue;
    const float ringCenterY = kGroundY - ring.height;
    // The texture's target rectangle contains transparent vertical padding.
    // Connect the trolley to the visible flame crown, not the target top.
    line(renderer, screenX, kTrackY + 5.0F, screenX,
         ringCenterY - 58.0F, color(119, 101, 73));
    // Far (left) half of the small ring behind the bag and the rider; the
    // near half is drawn after the rider in drawBonusRingForegrounds.
    {
      const int ringCell = textureWidth / 3;
      const SDL_Rect farSource{ringCell * 2, 0, ringCell / 2, textureHeight};
      const SDL_FRect farDestination{
          screenX - kBonusRingVisualHalfWidth,
          ringCenterY - kBonusRingVisualHalfHeight,
          kBonusRingVisualHalfWidth, kBonusRingVisualHalfHeight * 2.0F};
      SDL_RenderCopyF(renderer, propsTexture, &farSource, &farDestination);
    }
    if (ring.containsPrize && !ring.collected) {
      const SDL_FRect bagDestination{screenX - 26.0F, ringCenterY - 24.0F,
                                     52.0F, 48.0F};
      if (rewardBagTexture) {
        SDL_RenderCopyF(renderer, rewardBagTexture, nullptr,
                        &bagDestination);
      } else {
        SDL_RenderCopyF(renderer, propsTexture, &bagSource,
                        &bagDestination);
      }
    }
  }
}

void drawHoopForeground(SDL_Renderer* renderer, const Hoop& hoop,
                        float cameraX, SDL_Texture* hoopTexture) {
  if (!hoopTexture) return;
  const float x = hoop.worldX - cameraX;
  if (x < -80.0F || x > kWorldWidth + 80.0F) return;

  int textureWidth = 0;
  int textureHeight = 0;
  SDL_QueryTexture(hoopTexture, nullptr, nullptr, &textureWidth,
                   &textureHeight);
  const SDL_Rect ringSource{
      textureWidth / 2,
      static_cast<int>(textureHeight * (500.0F / 1774.0F)),
      textureWidth - textureWidth / 2,
      static_cast<int>(textureHeight * (1120.0F / 1774.0F)),
  };
  const float ringTop = hoop.openingTop - 25.0F;
  const float ringBottom = hoop.openingBottom + 20.0F;
  const SDL_FRect ringDestination{
      x, ringTop, kBigRingVisualHalfWidth, ringBottom - ringTop};
  // Near rim only: the far half was drawn behind the rider in drawHoop.
  SDL_RenderCopyF(renderer, hoopTexture, &ringSource, &ringDestination);
}

void drawBonusRingForegrounds(SDL_Renderer* renderer, const Game& game,
                              float cameraX, SDL_Texture* propsTexture) {
  if (!propsTexture) return;
  int textureWidth = 0;
  int textureHeight = 0;
  SDL_QueryTexture(propsTexture, nullptr, nullptr, &textureWidth,
                   &textureHeight);
  const int cellWidth = textureWidth / 3;
  const SDL_Rect ringSource{cellWidth * 2 + cellWidth / 2, 0,
                            cellWidth - cellWidth / 2, textureHeight};

  for (const auto& ring : game.bonusRings) {
    if (!ring.active) continue;
    const float screenX = ring.worldX - cameraX;
    if (screenX < -110.0F || screenX > kWorldWidth + 110.0F) continue;
    const float ringCenterY = kGroundY - ring.height;
    const SDL_FRect destination{
        screenX, ringCenterY - kBonusRingVisualHalfHeight,
        kBonusRingVisualHalfWidth, kBonusRingVisualHalfHeight * 2.0F};
    // Near rim only; the far half is behind the bag and the rider.
    SDL_RenderCopyF(renderer, propsTexture, &ringSource, &destination);
  }
}

// The HD distance marker: a gilded sign on a brass foot, the distance
// painted on its cream panel, its foot on `bottom`.
void drawSignHD(SDL_Renderer* renderer, float screenX, float bottom,
                std::string_view label, float widthColumns = 52.0F) {
  int textureWidth = 0;
  int textureHeight = 0;
  SDL_QueryTexture(gFloorSign, nullptr, nullptr, &textureWidth, &textureHeight);
  const float width = widthColumns * kSourceToWorldX;
  const float height = width * static_cast<float>(textureHeight) /
                       static_cast<float>(std::max(1, textureWidth));
  const SDL_FRect destination{screenX - width * 0.5F, bottom - height, width, height};
  SDL_RenderCopyF(renderer, gFloorSign, nullptr, &destination);
  // The panel's middle is 42 % down the painting (above the brass foot).
  const float textScale = 1.25F * widthColumns / 52.0F;
  const float textY = bottom - height + height * 0.42F - 7.0F * textScale * 0.5F - 1.0F;
  drawText(renderer, label, screenX + 1.0F, textY + 1.0F, textScale,
           color(90, 40, 10), true);
  drawText(renderer, label, screenX, textY, textScale, color(190, 30, 20), true);
}

// A balloon callout over its three 16x16 board cells (left edge `col - 16`,
// top `row`): the outline, the cloud and the letters, each tinted.  The
// board cycles GREAT / FAR OUT through eight palettes two frames at a time
// ($EAFE); `phase` picks one, and the fixed OH NO!! colours are phase -1.
void drawCallout(SDL_Renderer* renderer, const std::array<SDL_Texture*, 3>& layers,
                 int col, int row, int phase) {
  if (!layers[0] || !layers[1] || !layers[2]) return;
  // (letters, cloud, outline) measured on the cabinet, frames 23767-23782
  // of the stage 6 capture.
  static constexpr SDL_Color kCycle[8][3] = {
      {{184, 184, 174, 255}, {255, 71, 0, 255}, {33, 104, 255, 255}},
      {{0, 222, 255, 255}, {33, 104, 255, 255}, {184, 0, 0, 255}},
      {{255, 255, 255, 255}, {255, 71, 0, 255}, {184, 0, 0, 255}},
      {{255, 71, 0, 255}, {255, 255, 81, 255}, {0, 0, 255, 255}},
      {{184, 184, 174, 255}, {184, 0, 0, 255}, {184, 0, 0, 255}},
      {{0, 222, 255, 255}, {184, 0, 0, 255}, {0, 0, 255, 255}},
      {{255, 71, 0, 255}, {33, 104, 255, 255}, {184, 0, 0, 255}},
      {{255, 255, 255, 255}, {255, 71, 0, 255}, {184, 184, 174, 255}},
  };
  static constexpr SDL_Color kOhNo[3] = {{255, 71, 0, 255}, {184, 255, 0, 255}, {184, 0, 0, 255}};
  const SDL_Color* palette = phase < 0 ? kOhNo : kCycle[phase & 7];
  const SDL_FRect destination{static_cast<float>(col - 16) * kSourceToWorldX,
                              level3RowToY(static_cast<float>(row)),
                              48.0F * kSourceToWorldX, 16.0F * kLevel3RowScale};
  // outline under the cloud under the letters
  const int order[3] = {2, 0, 1};
  for (int k : order) {
    SDL_SetTextureColorMod(layers[k], palette[k].r, palette[k].g, palette[k].b);
    SDL_RenderCopyF(renderer, layers[k], nullptr, &destination);
  }
}

void drawFloorPlaque(SDL_Renderer* renderer, float screenX,
                     std::string_view label, bool start) {
  if (gFloorSign) {
    // On the cabinet the floor plaque sits on the rows under a fire pot's
    // base, so where the $EF07 signs land on a pot the two never overlap:
    // the HD sign stands on the strip under the pots' feet (kGroundY + 22).
    drawSignHD(renderer, screenX, kGroundY + 48.0F, label, 38.0F);
    return;
  }
  const float width = start ? 56.0F : 48.0F;
  const float left = screenX - width * 0.5F;
  const float top = kGroundY - 8.0F;
  // Faithful to the cabinet: this is a painted floor decal, not a signpost.
  // The slight perspective shadow and inner highlight keep it crisp in HD.
  const std::array<SDL_Vertex, 4> shadow{{
      {{left + 4.0F, top + 4.0F}, color(19, 13, 12, 130), {0, 0}},
      {{left + width + 4.0F, top + 4.0F}, color(19, 13, 12, 130), {0, 0}},
      {{left + width - 1.0F, top + 21.0F}, color(19, 13, 12, 130), {0, 0}},
      {{left + 9.0F, top + 21.0F}, color(19, 13, 12, 130), {0, 0}},
  }};
  const int indices[]{0, 1, 2, 0, 2, 3};
  SDL_RenderGeometry(renderer, nullptr, shadow.data(),
                     static_cast<int>(shadow.size()), indices, 6);
  fillRect(renderer, left, top, width, 18.0F, color(46, 224, 247));
  fillRect(renderer, left + 2.0F, top + 2.0F, width - 4.0F, 14.0F,
           color(246, 72, 43));
  fillRect(renderer, left + 5.0F, top + 4.0F, width - 10.0F, 10.0F,
           color(255, 226, 74));
  drawText(renderer, label, screenX, top + 5.0F, start ? 1.0F : 1.05F,
           color(18, 97, 183), true);
}

void drawCourseMarkers(SDL_Renderer* renderer, const Game& game,
                       float cameraX, SDL_Texture* goalPlatform) {
  const float startX = 78.0F - cameraX;
  if (startX > -80.0F && startX < kWorldWidth + 80.0F) {
    drawFloorPlaque(renderer, startX, "START", true);
  }

  for (const auto& marker : game.meterMarkers) {
    const float screenX = marker.worldX - cameraX;
    if (screenX < -60.0F || screenX > kWorldWidth + 60.0F) continue;
    drawFloorPlaque(renderer, screenX,
                    std::to_string(marker.meters) + "M", false);
  }

  const float goalX = kCourseLength - cameraX;
  if (goalX < -100.0F || goalX > kWorldWidth + 100.0F) return;
  const float platformY = kGoalPlatformTop;
  if (goalPlatform) {
    const SDL_FRect destination{goalX - 72.0F, platformY, 144.0F, 58.0F};
    SDL_RenderCopyF(renderer, goalPlatform, nullptr, &destination);
    drawText(renderer, "GOAL", goalX, platformY + 42.0F, 1.35F,
             color(255, 248, 130), true);
    return;
  }
  fillRect(renderer, goalX - 62.0F, platformY - 4.0F, 124.0F, 10.0F,
           color(120, 242, 89));
  fillRect(renderer, goalX - 58.0F, platformY + 4.0F, 116.0F, 25.0F,
           color(255, 255, 255));
  const std::array<SDL_Color, 3> stripes{
      color(248, 113, 213), color(255, 255, 255), color(201, 104, 247)};
  for (int stripe = 0; stripe < 10; ++stripe) {
    fillRect(renderer, goalX - 56.0F + stripe * 11.2F,
             platformY + 4.0F, 11.2F, 25.0F,
             stripes[static_cast<size_t>(stripe % stripes.size())]);
  }
  fillRect(renderer, goalX - 34.0F, platformY + 29.0F, 68.0F, 27.0F,
           color(255, 221, 40));
  fillRect(renderer, goalX - 30.0F, platformY + 33.0F, 60.0F, 19.0F,
           color(234, 68, 35));
  drawText(renderer, "GOAL", goalX, platformY + 36.0F, 1.75F,
           color(255, 244, 124), true);
}

void drawFinishRider(SDL_Renderer* renderer, SDL_Texture* finishTexture,
                     float screenX, int goalFrame) {
  if (!finishTexture) return;
  int textureWidth = 0;
  int textureHeight = 0;
  SDL_QueryTexture(finishTexture, nullptr, nullptr, &textureWidth,
                   &textureHeight);
  const int cellWidth = textureWidth / 4;
  // The arcade finish animation changes on a slower celebratory cadence than
  // the run cycle. It is a distinct bowed-lion/upright-Charlie composite.
  const int frame = (goalFrame / 10) % 4;
  const SDL_Rect source{frame * cellWidth, 0, cellWidth, textureHeight};
  // The finish composite is larger than the running pose in MAME. Frame
  // 003329 measures about 16% larger than the old render while the regular
  // runner already matches, so only this non-gameplay presentation is scaled.
  constexpr float kFinishWidth = 134.0F;
  constexpr float kFinishHeight = 141.0F;
  const float landingProgress = std::clamp(
      static_cast<float>(goalFrame) / 18.0F, 0.0F, 1.0F);
  const float arrivalX = (1.0F - landingProgress) * -26.0F;
  const float arrivalY = (1.0F - landingProgress) * -19.0F;
  const SDL_FRect destination{
      screenX - kFinishWidth * 0.5F + arrivalX,
      // The generated sheet has a few transparent pixels below the paws.
      // Lower the composite onto the visible green cushion instead of
      // aligning that transparent source edge to the platform rectangle.
      kGoalPlatformTop - kFinishHeight + 12.0F + arrivalY,
      kFinishWidth, kFinishHeight};
  SDL_RenderCopyF(renderer, finishTexture, &source, &destination);
}

void drawGoalPlatformFrontRim(SDL_Renderer* renderer, float screenX) {
  // This narrow foreground edge gives the landing depth: paws remain on the
  // green top while the padded front lip passes in front of their lowest row.
  fillRect(renderer, screenX - 67.0F, kGoalPlatformTop + 17.0F, 134.0F,
           2.5F, color(46, 126, 13, 225));
  fillRect(renderer, screenX - 65.0F, kGoalPlatformTop + 16.0F, 130.0F,
           1.0F, color(220, 255, 91, 235));
}

void drawExtraCharlie(SDL_Renderer* renderer, const Game& game,
                      float cameraX, double timeSeconds,
                      SDL_Texture* charlieTexture) {
  if (!game.extraCharlieActive || game.extraCharlieHoopIndex < 0 ||
      game.extraCharlieHoopIndex >= static_cast<int>(game.hoops.size())) {
    return;
  }
  const Hoop& hoop = game.hoops[
      static_cast<std::size_t>(game.extraCharlieHoopIndex)];
  if (!hoop.active) return;
  const float x = hoop.worldX - cameraX;
  if (x < -60.0F || x > kWorldWidth + 60.0F) return;

  const float sway = std::sin(static_cast<float>(timeSeconds) * 3.4F);
  const float spriteX = x + sway * 2.4F;
  const float spriteTop = hoop.openingTop + 17.0F;
  const float ropeTop = hoop.openingTop - 8.0F;
  const float ropeBottom = spriteTop + 8.0F;
  // The circusc4 reward is a complete Charlie hanging by both hands, not a
  // face medallion. Keep the arcade's thick segmented hanger and let the
  // character's diagonal pose provide the gentle swinging silhouette.
  for (float segmentY = ropeTop; segmentY < ropeBottom; segmentY += 8.0F) {
    const float height = std::min(8.0F, ropeBottom - segmentY);
    const int segment = static_cast<int>((segmentY - ropeTop) / 8.0F);
    fillRect(renderer, spriteX - 4.0F, segmentY, 8.0F, height,
             (segment & 1) == 0 ? color(224, 228, 226)
                                : color(89, 137, 211));
    fillRect(renderer, spriteX - 4.0F, segmentY, 1.5F, height,
             color(70, 70, 76));
    fillRect(renderer, spriteX + 2.5F, segmentY, 1.5F, height,
             color(249, 249, 240));
  }
  if (charlieTexture) {
    const SDL_FRect destination{spriteX - 27.0F, spriteTop, 54.0F, 88.0F};
    SDL_RenderCopyF(renderer, charlieTexture, nullptr, &destination);
    return;
  }
  filledCircle(renderer, spriteX, spriteTop + 27.0F, 11.0F,
               color(250, 218, 186));
  fillRect(renderer, spriteX - 8.0F, spriteTop + 37.0F, 16.0F, 30.0F,
           color(218, 39, 42));
  line(renderer, spriteX - 3.0F, spriteTop + 65.0F,
       spriteX - 10.0F, spriteTop + 84.0F, color(44, 105, 190));
  line(renderer, spriteX + 3.0F, spriteTop + 65.0F,
       spriteX + 13.0F, spriteTop + 82.0F, color(44, 105, 190));
}

void drawCoin(SDL_Renderer* renderer, float x, float y, float squash) {
  const float width = 9.0F * std::max(0.18F, squash);
  ellipse(renderer, x, y, width + 1.5F, 9.5F, color(91, 48, 8), 3);
  ellipse(renderer, x, y, width, 8.0F, color(211, 127, 13), 3);
  ellipse(renderer, x, y, std::max(1.0F, width - 2.5F), 5.5F,
          color(247, 187, 45), 2);
  if (width > 4.5F) {
    line(renderer, x - 2.0F, y - 4.0F, x + 2.0F, y + 4.0F,
         color(134, 76, 7));
    line(renderer, x + 2.0F, y - 4.0F, x - 2.0F, y + 4.0F,
         color(255, 222, 99));
  }
}

void drawGoalPresentation(SDL_Renderer* renderer, const Game& game,
                          SDL_Texture* birdTexture,
                          SDL_Texture* rewardBagTexture,
                          SDL_Texture* propsTexture) {
  const auto drawCheerCallouts = [&]() {
    if (game.goalFrame <= 30) return;
    const SDL_Color cheerColor =
        ((game.goalFrame / 12) & 1) == 0 ? color(255, 93, 36)
                                         : color(87, 219, 255);
    const float bounce =
        ((game.goalFrame / 8) & 1) == 0 ? 0.0F : -5.0F;
    const auto outlinedCheer = [&](std::string_view text, float x, float y,
                                   float scale, SDL_Color value) {
      for (const auto& offset : std::array<Vec2, 4>{
               Vec2{-2.0F, 0.0F}, Vec2{2.0F, 0.0F},
               Vec2{0.0F, -2.0F}, Vec2{0.0F, 2.0F}}) {
        drawText(renderer, text, x + offset.x, y + offset.y, scale,
                 color(45, 10, 24), true);
      }
      drawText(renderer, text, x, y, scale, value, true);
    };
    // The arcade alternates emphasis between its two audience callouts
    // instead of leaving both words perfectly static.
    const bool emphasizeGreat = ((game.goalFrame / 12) & 1) == 0;
    outlinedCheer("GREAT", 110.0F, 282.0F + bounce,
                  emphasizeGreat ? 2.75F : 2.35F, cheerColor);
    outlinedCheer("FAROUT", 370.0F, 282.0F - bounce,
                  emphasizeGreat ? 2.35F : 2.75F,
                  color(255, 130, 42));
  };
  if (!game.perfectClear) {
    drawCheerCallouts();
    return;
  }

  if (game.selectedEvent == 0) {
    // circusc4 goal presentation ($79DA/$7B68-$7C23): the bird records
    // $25E0/$25F0 fly at row $5F carrying the bag ($25D0, row $6F) from the
    // right edge one column per frame; once the bag reaches column $31 the
    // eleven $2520-$25C0 coin records launch from the bag.
    if (game.level1BirdPhase == 0) {
      drawCheerCallouts();
      return;
    }
    const auto columnToScreen = [](float column) {
      return kLevel1RiderCollisionScreenX + (column - 64.0F) * kSourceToWorldX;
    };
    const float birdCenterX =
        columnToScreen(static_cast<float>(game.level1BirdX) - 8.0F);
    const float birdY = level1RowToWorldY(0x5f);
    const float bagY = level1RowToWorldY(0x6f);
    for (const auto& coin : game.level1RewardCoins) {
      if (!coin.active) continue;
      const float coinX =
          columnToScreen(static_cast<float>(coin.xFixed) / 256.0F);
      const float coinY = level1RowToWorldY(
          static_cast<float>(coin.yFixed) / 256.0F);
      drawCoin(renderer, coinX, coinY,
               std::abs(std::cos(static_cast<float>(coin.spin) *
                                 (kPi / 12.0F))));
    }
    if (birdTexture) {
      int textureWidth = 0;
      int textureHeight = 0;
      SDL_QueryTexture(birdTexture, nullptr, nullptr, &textureWidth,
                       &textureHeight);
      const int cellWidth = textureWidth / 4;
      // $7B72-$7B88: wing frames $44/$46/$48/$46 change every eight frames.
      // Cells 0/1 carry the bag in the talons; 2/3 pour it out.
      const int flap = (game.level1GoalCounter / 8) & 3;
      const int cell = game.level1BirdPhase == 2 ? (flap & 1) : 2 + (flap & 1);
      const SDL_Rect source{cell * cellWidth, 0, cellWidth, textureHeight};
      const SDL_FRect destination{birdCenterX - 45.0F, birdY - 60.0F, 90.0F,
                                  120.0F};
      SDL_RenderCopyExF(renderer, birdTexture, &source, &destination, 0.0,
                        nullptr, SDL_FLIP_NONE);
    }
    if (!birdTexture) {
      // The painted bird carries its bag in every cell; only the fallback
      // shape needs a separate one.
      ellipse(renderer, birdCenterX, bagY + 20.0F, 18.0F, 24.0F,
              color(223, 158, 39), 4);
    }
    drawCheerCallouts();
    return;
  }

  const int birdStart = kGoalArrivalFrames;
  const int bagDropStart = birdStart + kBirdArrivalFrames;
  const int showerStart = bagDropStart + kBagDropFrames;
  const bool stage3 = false;
  const float goalScreenX = kGoalScreenX;
  if (birdTexture && game.goalFrame >= birdStart) {
    int textureWidth = 0;
    int textureHeight = 0;
    SDL_QueryTexture(birdTexture, nullptr, nullptr, &textureWidth,
                     &textureHeight);
    const int cellWidth = textureWidth / 4;
    int cell = 0;
    float birdX = 0.0F;
    if (game.goalFrame < bagDropStart) {
      const float progress = std::clamp(
          static_cast<float>(game.goalFrame - birdStart) /
              static_cast<float>(kBirdArrivalFrames),
          0.0F, 1.0F);
      const float entranceX = stage3 ? -30.0F : 510.0F;
      birdX = entranceX + (goalScreenX - entranceX) * progress;
      cell = (game.goalFrame / 8) & 1;
    } else {
      // The reference bird remains beside the bag throughout the complete
      // coin shower. It does not fly away while coins fall from empty air.
      birdX = goalScreenX;
      cell = 2 + ((game.goalFrame / 10) & 1);
    }
    const SDL_Rect source{cell * cellWidth, 0, cellWidth, textureHeight};
    // Keep the reward bird in the arena below the persistent LED/HUD panel.
    const SDL_FRect destination{birdX - 45.0F, 205.0F, 90.0F, 120.0F};
    // Stage 1 enters right-to-left; Event 3's reference enters left-to-right.
    // The sprite is symmetrical enough to mirror without introducing a
    // second lower-quality atlas.
    SDL_RenderCopyExF(renderer, birdTexture, &source, &destination, 0.0,
                      nullptr,
                      stage3 ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
  }

  const float rewardBagX = goalScreenX;
  constexpr float kRewardBagRestY = 305.0F;
  float rewardBagY = kRewardBagRestY;
  const bool showRewardBag = game.goalFrame >= bagDropStart;
  if (showRewardBag && game.goalFrame < showerStart) {
    const float dropProgress = std::clamp(
        static_cast<float>(game.goalFrame - bagDropStart) /
            static_cast<float>(kBagDropFrames),
        0.0F, 1.0F);
    const float easedDrop = dropProgress * dropProgress;
    rewardBagY = 263.0F + (kRewardBagRestY - 263.0F) * easedDrop;
  }

  if (game.goalFrame >= showerStart) {
    for (int index = 0; index < kRewardCoinCount; ++index) {
      const int localFrame = game.goalFrame - showerStart - index * 10;
      if (localFrame < 0 || localFrame >= 105) continue;
      const float lane =
          static_cast<float>((index * 37) % 91) - 45.0F;
      const float spread = std::clamp(
          static_cast<float>(localFrame) / 32.0F, 0.0F, 1.0F);
      const float x = rewardBagX + lane * 0.55F * spread;
      const float y = kRewardBagRestY + 30.0F +
                      static_cast<float>(localFrame) * 2.05F;
      drawCoin(renderer, x, y,
               std::abs(std::sin(static_cast<float>(localFrame) * 0.24F)));
    }
  }

  if (showRewardBag) {
    if (rewardBagTexture) {
      const SDL_FRect bagDestination{rewardBagX - 24.0F,
                                     rewardBagY - 30.0F, 48.0F, 59.0F};
      SDL_RenderCopyF(renderer, rewardBagTexture, nullptr, &bagDestination);
    } else if (propsTexture) {
      int textureWidth = 0;
      int textureHeight = 0;
      SDL_QueryTexture(propsTexture, nullptr, nullptr, &textureWidth,
                       &textureHeight);
      const int cellWidth = textureWidth / 3;
      const SDL_Rect bagSource{cellWidth, 0, cellWidth, textureHeight};
      const SDL_FRect bagDestination{rewardBagX - 25.0F,
                                     rewardBagY - 35.0F, 50.0F, 100.0F};
      SDL_RenderCopyF(renderer, propsTexture, &bagSource, &bagDestination);
    } else {
      ellipse(renderer, rewardBagX, rewardBagY + 10.0F, 18.0F, 24.0F,
              color(223, 158, 39), 4);
      fillRect(renderer, rewardBagX - 12.0F, rewardBagY - 16.0F, 24.0F,
               7.0F, color(118, 68, 21));
    }
  }
  // Keep the finish words in front of the optional reward bird and shower;
  // the callouts are part of the scoreboard presentation, not background art.
  drawCheerCallouts();
}

void drawLionAndRider(SDL_Renderer* renderer, float screenX, float groundY,
                      double timeSeconds, bool alive, bool lowDetail,
                      SDL_Texture* riderRunA, SDL_Texture* riderRunB,
                      SDL_Texture* riderRunC, SDL_Texture* riderBackE,
                      SDL_Texture* riderBackF,
                      bool /*lionOnlyTest*/, float runSpeed, bool grounded,
                      bool facingRight, Level1RiderState riderState) {
  (void)timeSeconds;
  (void)lowDetail;
  (void)runSpeed;
  if (!grounded) riderState = Level1RiderState::RunC;
  SDL_Texture* riderTexture = nullptr;
  switch (riderState) {
    case Level1RiderState::RunA:
    case Level1RiderState::BackD:
      riderTexture = riderRunA;
      break;
    case Level1RiderState::RunB:
      riderTexture = riderRunB;
      break;
    case Level1RiderState::RunC:
      riderTexture = riderRunC;
      break;
    case Level1RiderState::BackE:
      riderTexture = riderBackE ? riderBackE : riderRunB;
      break;
    case Level1RiderState::BackF:
      riderTexture = riderBackF ? riderBackF : riderRunA;
      break;
  }
  if (riderTexture) {
    // Production A/B/C use a larger transparent canvas so the corrected
    // rider/lion set can match the burning composite without cropping Run C.
    // Pixel density is unchanged; only these three artwork files grow.
    constexpr float kProductionPixelToWorld = 118.8F / 1024.0F;
    constexpr float kProductionCanvasWidth = 1536.0F;
    constexpr float kProductionCanvasHeight = 1024.0F;
    constexpr float kProductionAnchorX = 768.0F;
    constexpr float kProductionAnchorY = 714.0F;
    constexpr float kRenderWidth =
        kProductionCanvasWidth * kProductionPixelToWorld;
    constexpr float kRenderHeight =
        kProductionCanvasHeight * kProductionPixelToWorld;
    const float sourceAnchorX = facingRight
        ? kProductionAnchorX
        : kProductionCanvasWidth - kProductionAnchorX;
    const float visualAnchorX = sourceAnchorX * kProductionPixelToWorld;
    const float visualAnchorY =
        kProductionAnchorY * kProductionPixelToWorld;
    // The painted frames are not quite one size: Charlie's hat is 117 px
    // tall in Run A and the two backing frames, 125 in Run B and C.  Grow
    // the small ones about the lion's feet so he stays one size while he
    // runs.
    // The two backing frames were painted with a smaller lion under the
    // same Charlie, so they get a further 9 % to keep the lion one size as
    // he backs up (Run A, E, F in turn).
    const bool backing = riderTexture == riderBackE || riderTexture == riderBackF;
    const float grow = (riderTexture == riderRunA || backing ? 125.0F / 117.0F : 1.0F) *
                       (backing ? 1.09F : 1.0F);
    const SDL_FRect destination{screenX + 10.0F - visualAnchorX * grow,
                                groundY - visualAnchorY * grow,
                                kRenderWidth * grow, kRenderHeight * grow};
    SDL_SetTextureColorMod(riderTexture, 255, alive ? 255 : 128,
                           alive ? 255 : 58);
    SDL_RenderCopyExF(renderer, riderTexture, nullptr, &destination, 0.0,
                      nullptr, facingRight ? SDL_FLIP_NONE
                                           : SDL_FLIP_HORIZONTAL);
    SDL_SetTextureColorMod(riderTexture, 255, 255, 255);
    return;
  }

  const float runPhase =
      std::sin(static_cast<float>(timeSeconds * (alive ? 14.0 : 2.0)));
  const SDL_Color gold = alive ? color(221, 151, 55) : color(122, 92, 72);
  const SDL_Color mane = alive ? color(122, 68, 25) : color(76, 58, 49);
  const SDL_Color jacket = alive ? color(202, 38, 45) : color(92, 68, 72);
  const SDL_Color trousers = alive ? color(22, 83, 164) : color(67, 71, 85);

  filledCircle(renderer, screenX + 19.0F, groundY - 48.0F, 29.0F, mane);
  filledCircle(renderer, screenX - 13.0F, groundY - 38.0F, 29.0F, gold);
  fillRect(renderer, screenX - 40.0F, groundY - 58.0F, 58.0F, 39.0F, gold);
  filledCircle(renderer, screenX + 33.0F, groundY - 45.0F, 18.0F, gold);
  filledCircle(renderer, screenX + 45.0F, groundY - 43.0F, 10.0F,
               color(232, 183, 98));
  filledCircle(renderer, screenX + 49.0F, groundY - 45.0F, 2.5F,
               color(20, 15, 12));

  const float frontLeg = runPhase * 11.0F;
  const float rearLeg = -runPhase * 11.0F;
  line(renderer, screenX + 12.0F, groundY - 26.0F,
       screenX + 28.0F + frontLeg, groundY - 4.0F, gold);
  line(renderer, screenX + 13.0F, groundY - 24.0F,
       screenX + 3.0F - frontLeg, groundY - 3.0F, gold);
  line(renderer, screenX - 28.0F, groundY - 24.0F,
       screenX - 42.0F + rearLeg, groundY - 3.0F, gold);
  line(renderer, screenX - 22.0F, groundY - 24.0F,
       screenX - 11.0F - rearLeg, groundY - 3.0F, gold);
  line(renderer, screenX - 39.0F, groundY - 47.0F,
       screenX - 62.0F, groundY - 60.0F + runPhase * 3.0F, mane);

  // Charlie remains unmistakably a circus clown: red coat, blue trousers,
  // white makeup, red side hair and nose, and a small pointed performance cap.
  fillRect(renderer, screenX - 13.0F, groundY - 81.0F, 25.0F, 29.0F,
           jacket);
  filledCircle(renderer, screenX - 1.0F, groundY - 94.0F, 11.0F,
               color(244, 236, 218));
  filledCircle(renderer, screenX - 12.0F, groundY - 94.0F, 5.5F,
               color(214, 61, 35));
  filledCircle(renderer, screenX + 10.0F, groundY - 94.0F, 5.5F,
               color(214, 61, 35));
  filledCircle(renderer, screenX + 7.0F, groundY - 93.0F, 3.0F,
               color(222, 38, 42));
  const std::array<SDL_Vertex, 3> clownCap{{
      {{screenX - 9.0F, groundY - 103.0F}, color(24, 93, 180), {0, 0}},
      {{screenX + 2.0F, groundY - 121.0F}, color(24, 93, 180), {0, 0}},
      {{screenX + 10.0F, groundY - 103.0F}, color(24, 93, 180), {0, 0}},
  }};
  SDL_RenderGeometry(renderer, nullptr, clownCap.data(),
                     static_cast<int>(clownCap.size()), nullptr, 0);
  filledCircle(renderer, screenX + 2.0F, groundY - 122.0F, 2.5F,
               color(244, 194, 51));
  line(renderer, screenX - 11.0F, groundY - 76.0F,
       screenX - 31.0F, groundY - 66.0F + runPhase * 2.0F, jacket);
  line(renderer, screenX + 7.0F, groundY - 77.0F,
       screenX + 26.0F, groundY - 69.0F - runPhase * 2.0F, jacket);
  line(renderer, screenX - 6.0F, groundY - 56.0F,
       screenX - 21.0F, groundY - 40.0F, trousers);
  line(renderer, screenX + 5.0F, groundY - 56.0F,
       screenX + 19.0F, groundY - 41.0F, trousers);
  fillRect(renderer, screenX - 12.0F, groundY - 87.0F, 23.0F, 4.0F,
           color(243, 190, 48));
  if (!lowDetail) {
    line(renderer, screenX - 1.0F, groundY - 105.0F,
         screenX + 10.0F, groundY - 122.0F, color(238, 238, 244));
  }
}

void drawBurningRider(SDL_Renderer* renderer, float screenX, float groundY,
                      SDL_Texture* burnTexture, int crashFrame) {
  if (!burnTexture) return;

  int textureWidth = 0;
  int textureHeight = 0;
  SDL_QueryTexture(burnTexture, nullptr, nullptr, &textureWidth,
                   &textureHeight);
  const int cellWidth = textureWidth / 4;
  const int frame = (crashFrame / 4) % 4;
  const SDL_Rect source{frame * cellWidth, 0, cellWidth, textureHeight};
  constexpr std::array<float, 4> kFrameBottoms{
      383.0F, 401.0F, 402.0F, 351.0F};
  constexpr float kBurnWidth = 108.0F;
  const float burnHeight =
      kBurnWidth * static_cast<float>(textureHeight) /
      static_cast<float>(cellWidth);
  const float visualGroundOffset =
      kFrameBottoms[static_cast<size_t>(frame)] /
      static_cast<float>(textureHeight) * burnHeight;
  const float arrival =
      std::clamp(static_cast<float>(crashFrame) / 10.0F, 0.15F, 1.0F);
  const float width = kBurnWidth * arrival;
  const float height = burnHeight * arrival;
  const SDL_FRect destination{
      screenX - width * 0.5F,
      groundY - visualGroundOffset * arrival,
      width,
      height,
  };
  SDL_RenderCopyF(renderer, burnTexture, &source, &destination);
}

void drawCharlieLifeIcon(SDL_Renderer* renderer, SDL_Texture* charlieTexture,
                         float x, float y) {
  if (charlieTexture) {
    const SDL_FRect destination{x - 38.0F, y - 5.0F, 76.0F, 76.0F};
    SDL_RenderCopyF(renderer, charlieTexture, nullptr, &destination);
    return;
  }
  filledCircle(renderer, x, y + 7.0F, 6.0F, color(249, 218, 187));
  filledCircle(renderer, x + 5.0F, y + 8.0F, 2.2F, color(225, 45, 48));
  fillRect(renderer, x - 5.0F, y + 13.0F, 10.0F, 9.0F,
           color(202, 37, 46));
}

void drawHudBulbs(SDL_Renderer* renderer) {
  const std::array<SDL_Color, 5> bulbs{
      color(255, 55, 41), color(255, 225, 45), color(45, 211, 80),
      color(57, 106, 255), color(244, 85, 206)};
  // Chase the five colors around the entire cabinet border. At roughly nine
  // updates per second it reads like the original moving circus bulbs rather
  // than a distracting rapid flash.
  const int phase = static_cast<int>((SDL_GetTicks64() / 110U) % bulbs.size());
  for (int index = 0; index < 60; ++index) {
    const float x = static_cast<float>(index * 8 + 2);
    const SDL_Color topGlow = bulbs[static_cast<size_t>(
        (index + phase) % static_cast<int>(bulbs.size()))];
    const SDL_Color bottomGlow = bulbs[static_cast<size_t>(
        (index - phase + static_cast<int>(bulbs.size()) * 2) %
        static_cast<int>(bulbs.size()))];
    filledCircle(renderer, x, kHudTop + 2.0F, 2.4F, color(20, 20, 25));
    filledCircle(renderer, x, kHudTop + 2.0F, 1.6F, topGlow);
    filledCircle(renderer, x, kHudTop + kHudHeight - 3.0F, 2.4F,
                 color(20, 20, 25));
    filledCircle(renderer, x, kHudTop + kHudHeight - 3.0F, 1.6F,
                 bottomGlow);
  }
  for (int index = 1; index < 11; ++index) {
    const float y = kHudTop + static_cast<float>(index * 8);
    const SDL_Color leftGlow = bulbs[static_cast<size_t>(
        (index + 1 - phase + static_cast<int>(bulbs.size()) * 2) %
        static_cast<int>(bulbs.size()))];
    const SDL_Color rightGlow = bulbs[static_cast<size_t>(
        (index + 3 + phase) % static_cast<int>(bulbs.size()))];
    filledCircle(renderer, 2.0F, y, 2.4F, color(20, 20, 25));
    filledCircle(renderer, 2.0F, y, 1.6F, leftGlow);
    filledCircle(renderer, kWorldWidth - 3.0F, y, 2.4F,
                 color(20, 20, 25));
    filledCircle(renderer, kWorldWidth - 3.0F, y, 1.6F, rightGlow);
  }
}

void drawHighScoreBulbs(SDL_Renderer* renderer) {
  constexpr float left = 190.0F;
  constexpr float top = kHudTop + 25.0F;
  constexpr float width = 100.0F;
  constexpr float height = 31.0F;
  const std::array<SDL_Color, 3> bulbs{
      color(48, 104, 255), color(53, 204, 255), color(76, 83, 239)};
  for (int index = 0; index <= 20; ++index) {
    const float x = left + static_cast<float>(index) * 5.0F;
    const SDL_Color glow = bulbs[static_cast<std::size_t>(index % 3)];
    filledCircle(renderer, x, top, 1.35F, glow);
    filledCircle(renderer, x, top + height, 1.35F,
                 bulbs[static_cast<std::size_t>((index + 1) % 3)]);
  }
  for (int index = 1; index < 6; ++index) {
    const float y = top + static_cast<float>(index) * 5.0F;
    filledCircle(renderer, left, y, 1.35F,
                 bulbs[static_cast<std::size_t>((index + 2) % 3)]);
    filledCircle(renderer, left + width, y, 1.35F,
                 bulbs[static_cast<std::size_t>(index % 3)]);
  }
}

void drawHud(SDL_Renderer* renderer, const Game& game,
             SDL_Texture* charlieTexture) {
  fillRect(renderer, 0.0F, kHudTop, kWorldWidth, kHudHeight,
           color(0, 0, 0, 248));
  drawHudBulbs(renderer);

  drawText(renderer, "1UP", 12.0F, kHudTop + 8.0F, 1.35F,
           color(255, 230, 34));
  const int displayedPlayerScore =
      (game.scene == Scene::Title || game.scene == Scene::EventSelect)
          ? game.highScore
          : game.score;
  drawText(renderer, std::to_string(displayedPlayerScore), 68.0F,
           kHudTop + 8.0F,
           1.45F, color(255, 255, 255));

  drawText(renderer, "HIGH SCORE", kWorldWidth * 0.5F, kHudTop + 8.0F,
           1.25F, color(245, 70, 37), true);
  drawHighScoreBulbs(renderer);
  // Centred in the bulb frame (top kHudTop + 25, 31 tall): the digits are
  // seven rows of 1.65.
  drawText(renderer, std::to_string(game.highScore),
           kWorldWidth * 0.5F, kHudTop + 25.0F + 31.0F * 0.5F - 7.0F * 1.65F * 0.5F,
           1.65F, color(51, 213, 57), true);

  const int waitingCharlies = std::clamp(game.lives - 1, 0, 5);
  for (int life = 0; life < waitingCharlies; ++life) {
    drawCharlieLifeIcon(renderer, charlieTexture, 32.0F + life * 52.0F,
                        kHudTop + 20.0F);
  }
  std::ostringstream creditText;
  creditText << "CREDIT " << std::setw(2) << std::setfill('0')
             << std::clamp(game.credits, 0, 99);
  drawText(renderer, creditText.str(), 312.0F, kHudTop + 46.0F, 2.9F,
           color(70, 202, 255));

}

void drawCrowdOhNo(SDL_Renderer* renderer) {
  const auto drawCall = [&](float x, float y) {
    constexpr float scale = 1.55F;
    const SDL_Color outline = color(255, 239, 26);
    drawText(renderer, "OH NO!!", x - 2.0F, y, scale, outline, true);
    drawText(renderer, "OH NO!!", x + 2.0F, y, scale, outline, true);
    drawText(renderer, "OH NO!!", x, y - 2.0F, scale, outline, true);
    drawText(renderer, "OH NO!!", x, y + 2.0F, scale, outline, true);
    drawText(renderer, "OH NO!!", x, y, scale, color(255, 82, 28), true);
  };
  drawCall(154.0F, 242.0F);
  drawCall(354.0F, 300.0F);
}

void drawCoinWaitingScreen(SDL_Renderer* renderer, const Game& game,
                           const Assets& assets, double timeSeconds) {
  fillRect(renderer, 0.0F, 0.0F, kWorldWidth, kWorldHeight,
           color(0, 0, 0));
  if (assets.marquee) {
    const SDL_FRect marqueeDestination{
        0.0F, 0.0F, static_cast<float>(kWorldWidth), kMarqueeHeight};
    SDL_RenderCopyF(renderer, assets.marquee, nullptr, &marqueeDestination);
    drawReddishMarqueeSky(renderer);
  }
  drawFerrisWheel(renderer, assets.ferrisWheel, assets.ferrisGondola,
                  timeSeconds);
  drawZeppelinBonus(renderer, -1);
  drawHud(renderer, game, assets.charlieLife);

  const bool promptVisible =
      (static_cast<int>(timeSeconds * 2.0) & 1) == 0;
  if (promptVisible) {
    const std::string_view prompt =
        game.credits > 0 ? "PRESS START BUTTON" : "INSERT COIN";
    const SDL_Color promptColor =
        game.credits > 0 ? color(238, 203, 255) : color(255, 224, 63);
    drawText(renderer, prompt, kWorldWidth * 0.5F, 246.0F,
             game.credits > 0 ? 2.25F : 2.8F, promptColor, true);
  }
  drawText(renderer, "ONE PLAYER ONLY", kWorldWidth * 0.5F, 315.0F,
           2.25F, color(42, 216, 69), true);
  drawText(renderer, "1ST BONUS AFTER 20000 PTS", kWorldWidth * 0.5F,
           433.0F, 1.55F, color(67, 201, 255), true);
  drawText(renderer, "AND BONUS EVERY 70000 PTS", kWorldWidth * 0.5F,
           486.0F, 1.55F, color(67, 201, 255), true);
}

// The card after the last life: the marquee, the panel with the final
// score and GAME OVER, then the title.
void drawGameOverScreen(SDL_Renderer* renderer, const Game& game,
                        const Assets& assets, double timeSeconds) {
  fillRect(renderer, 0.0F, 0.0F, kWorldWidth, kWorldHeight, color(0, 0, 0));
  if (assets.marquee) {
    const SDL_FRect marqueeDestination{
        0.0F, 0.0F, static_cast<float>(kWorldWidth), kMarqueeHeight};
    SDL_RenderCopyF(renderer, assets.marquee, nullptr, &marqueeDestination);
    drawReddishMarqueeSky(renderer);
  }
  drawFerrisWheel(renderer, assets.ferrisWheel, assets.ferrisGondola, timeSeconds);
  drawZeppelinBonus(renderer, -1);
  Game hudGame = game;
  hudGame.lives = 0;
  drawHud(renderer, hudGame, assets.charlieLife);
  const bool visible = (game.gameOverFrame / 30) % 2 == 0;
  if (visible) {
    drawText(renderer, "GAME OVER", kWorldWidth * 0.5F, 300.0F, 3.2F,
             color(255, 64, 64), true);
  }
  drawText(renderer, "SCORE " + std::to_string(game.score), kWorldWidth * 0.5F,
           370.0F, 1.8F, color(255, 230, 38), true);
  if (game.score >= game.highScore && game.score > 0) {
    drawText(renderer, "NEW HIGH SCORE", kWorldWidth * 0.5F, 410.0F, 1.55F,
             color(67, 201, 255), true);
  }
}

void drawEventSelectBulbs(SDL_Renderer* renderer, float x, float y,
                          float width, float height, int phase,
                          bool drawTop, bool drawBottom, bool drawLeft,
                          bool drawRight) {
  const std::array<SDL_Color, 5> bulbs{
      color(250, 46, 48), color(255, 226, 41), color(40, 216, 72),
      color(54, 133, 255), color(217, 73, 244),
  };
  constexpr float spacing = 8.0F;
  const int horizontalCount = static_cast<int>(width / spacing);
  const int verticalCount = static_cast<int>(height / spacing);
  for (int index = 0; index <= horizontalCount; ++index) {
    const float bulbX = x + std::min(width, index * spacing);
    const SDL_Color top =
        bulbs[static_cast<size_t>((index + phase) % bulbs.size())];
    const SDL_Color bottom =
        bulbs[static_cast<size_t>((index + phase + 2) % bulbs.size())];
    if (drawTop) filledCircle(renderer, bulbX, y, 1.75F, top);
    if (drawBottom) {
      filledCircle(renderer, bulbX, y + height, 1.75F, bottom);
    }
  }
  for (int index = 1; index < verticalCount; ++index) {
    const float bulbY = y + index * spacing;
    const SDL_Color left =
        bulbs[static_cast<size_t>((index + phase + 1) % bulbs.size())];
    const SDL_Color right =
        bulbs[static_cast<size_t>((index + phase + 3) % bulbs.size())];
    if (drawLeft) filledCircle(renderer, x, bulbY, 1.75F, left);
    if (drawRight) {
      filledCircle(renderer, x + width, bulbY, 1.75F, right);
    }
  }
}

void drawEventSelectCell(SDL_Renderer* renderer, SDL_Texture* texture,
                         int eventIndex, const SDL_FRect& destination) {
  if (!texture || eventIndex < 0 || eventIndex >= kEventCount) return;
  int textureWidth = 0;
  int textureHeight = 0;
  SDL_QueryTexture(texture, nullptr, nullptr, &textureWidth, &textureHeight);
  const int cellWidth = textureWidth / kEventColumns;
  const int cellHeight = textureHeight / 2;
  const SDL_Rect source{
      (eventIndex % kEventColumns) * cellWidth,
      (eventIndex / kEventColumns) * cellHeight,
      cellWidth,
      cellHeight,
  };
  SDL_RenderCopyF(renderer, texture, &source, &destination);
}

void drawEventSelectionScreen(SDL_Renderer* renderer, const Game& game,
                              const Assets& assets, double timeSeconds) {
  fillRect(renderer, 0.0F, 0.0F, kWorldWidth, kWorldHeight, color(0, 0, 0));
  if (assets.marquee) {
    const SDL_FRect marqueeDestination{
        0.0F, 0.0F, static_cast<float>(kWorldWidth), kMarqueeHeight};
    SDL_RenderCopyF(renderer, assets.marquee, nullptr, &marqueeDestination);
    drawReddishMarqueeSky(renderer);
  }
  drawFerrisWheel(renderer, assets.ferrisWheel, assets.ferrisGondola,
                  timeSeconds);
  drawZeppelinBonus(renderer, -1);

  Game hudGame = game;
  hudGame.lives = 1;
  drawHud(renderer, hudGame, assets.charlieLife);
  drawText(renderer, "CHOOSE THE SCREEN USING", kWorldWidth * 0.5F,
           201.0F, 1.25F, color(255, 255, 255), true);
  drawText(renderer, "JOYSTICK & BUTTON", kWorldWidth * 0.5F,
           216.0F, 1.25F, color(255, 255, 255), true);

  constexpr std::array<std::string_view, kEventCount> difficulty{
      "EASY", "NORMAL", "NORMAL", "HARD", "HARDER", "HARDEST",
  };
  constexpr float panelWidth = 152.0F;
  constexpr float panelHeight = 188.0F;
  constexpr float firstX = 12.0F;
  constexpr float firstY = 244.0F;
  constexpr float columnStep = panelWidth;
  constexpr float rowStep = panelHeight;

  for (int eventIndex = 0; eventIndex < kEventCount; ++eventIndex) {
    const int column = eventIndex % kEventColumns;
    const int row = eventIndex / kEventColumns;
    const float x = firstX + column * columnStep;
    const float y = firstY + row * rowStep;
    const SDL_FRect panel{x, y, panelWidth, panelHeight};
    SDL_Texture* panelTexture =
        eventIndex == game.selectedEvent ? assets.eventSelectChosen
                                         : assets.eventSelectProps;
    drawEventSelectCell(renderer, panelTexture, eventIndex, panel);
    drawEventSelectBulbs(renderer, x, y, panelWidth, panelHeight, eventIndex,
                         true, row == 1, true,
                         column == kEventColumns - 1);
    drawText(renderer, std::to_string(eventIndex + 1), x + 6.0F,
             y + 7.0F, 1.0F, color(62, 204, 255));
    drawText(renderer, difficulty[static_cast<size_t>(eventIndex)],
             x + 18.0F, y + 7.0F, 0.92F, color(255, 230, 38));
    const int plays = game.playCount[static_cast<std::size_t>(eventIndex)];
    if (eventLocked(game, eventIndex)) {
      fillRect(renderer, x, y, panelWidth, panelHeight, color(0, 0, 0, 170));
      drawText(renderer, "LOCKED", x + panelWidth * 0.5F, y + panelHeight * 0.5F - 6.0F,
               1.4F, color(255, 80, 80), true);
    } else if (plays > 0) {
      // Plays so far, out of the five the screen allows (the trapeze is open-ended).
      const std::string tally = eventIndex == kTrapezeEvent
                                    ? "ROUND " + std::to_string(plays + 1)
                                    : std::to_string(plays) + "/" + std::to_string(kEventPlayLimit);
      drawText(renderer, tally, x + panelWidth * 0.5F, y + panelHeight - 16.0F, 0.92F,
               color(255, 255, 255), true);
    }
  }
}

void drawStage1ScorePopup(SDL_Renderer* renderer, const Game& game,
                          float cameraX) {
  if (game.stage1ScorePopupFrame <= 0 || game.stage1ScorePopup <= 0) return;
  const float x = game.stage1ScorePopupWorldX - cameraX;
  const float lift = static_cast<float>(52 - game.stage1ScorePopupFrame) *
                     0.35F;
  drawText(renderer, std::to_string(game.stage1ScorePopup), x,
           game.stage1ScorePopupY - lift, 1.25F,
           color(255, 245, 96), true);
}

void drawLevel3BirdAndCoins(SDL_Renderer* renderer, const Game& game,
                            const Assets& assets);

// ---- Event 2 drawing ------------------------------------------------------
// Everything here reads the board records (docs/LEVEL2_ROM_MODEL.md).  A
// record's column is already a screen column, so a cell is drawn at
// `col * kSourceToWorldX`; the composites are centred on the column and
// their feet sit on `row + 16`, which is where MAME puts the sprites:
// Charlie standing at ($88, $40) covers source pixels x 51-73, y 125-151,
// a monkey at column c covers x c-15..c+5 with its feet on the same line.

// A pose from one of the measured sheets: the opaque bounds of the cell are
// scaled uniformly (`unitsPerPixel`), centred on `centerX` and stood on
// `feetY`.
void drawStage2Pose(SDL_Renderer* renderer, SDL_Texture* texture,
                    const SheetCells& cells, int frame, float centerX,
                    float feetY, float unitsPerPixel,
                    SDL_RendererFlip flip = SDL_FLIP_NONE) {
  if (!texture || !cells.valid()) return;
  frame = std::clamp(frame, 0, static_cast<int>(cells.bounds.size()) - 1);
  const SDL_Rect& source = cells.bounds[static_cast<std::size_t>(frame)];
  const float width = static_cast<float>(source.w) * unitsPerPixel;
  const float height = static_cast<float>(source.h) * unitsPerPixel;
  const SDL_FRect destination{centerX - width * 0.5F, feetY - height, width,
                              height};
  SDL_RenderCopyExF(renderer, texture, &source, &destination, 0.0, nullptr,
                    flip);
}

// The same, tilted about the feet (the balance wobble on the rope).
void drawStage2PoseTilted(SDL_Renderer* renderer, SDL_Texture* texture,
                          const SheetCells& cells, int frame, float centerX,
                          float feetY, float unitsPerPixel, double angle) {
  if (!texture || !cells.valid()) return;
  frame = std::clamp(frame, 0, static_cast<int>(cells.bounds.size()) - 1);
  const SDL_Rect& source = cells.bounds[static_cast<std::size_t>(frame)];
  const float width = static_cast<float>(source.w) * unitsPerPixel;
  const float height = static_cast<float>(source.h) * unitsPerPixel;
  const SDL_FRect destination{centerX - width * 0.5F, feetY - height, width,
                              height};
  const SDL_FPoint pivot{width * 0.5F, height};
  SDL_RenderCopyExF(renderer, texture, &source, &destination, angle, &pivot,
                    SDL_FLIP_NONE);
}

// The same, centred on (centerX, centerY) and spun about that centre (the
// trampoline somersault).
void drawPoseSpinning(SDL_Renderer* renderer, SDL_Texture* texture,
                      const SheetCells& cells, int frame, float centerX,
                      float centerY, float unitsPerPixel, double angle,
                      SDL_RendererFlip flip = SDL_FLIP_NONE) {
  if (!texture || !cells.valid()) return;
  frame = std::clamp(frame, 0, static_cast<int>(cells.bounds.size()) - 1);
  const SDL_Rect& source = cells.bounds[static_cast<std::size_t>(frame)];
  const float width = static_cast<float>(source.w) * unitsPerPixel;
  const float height = static_cast<float>(source.h) * unitsPerPixel;
  const SDL_FRect destination{centerX - width * 0.5F, centerY - height * 0.5F,
                              width, height};
  SDL_RenderCopyExF(renderer, texture, &source, &destination, angle, nullptr,
                    flip);
}

// The same, hung from the top of its opaque bounds (hands on a bar).
void drawPoseFromTop(SDL_Renderer* renderer, SDL_Texture* texture,
                     const SheetCells& cells, int frame, float centerX,
                     float topY, float unitsPerPixel, double angle = 0.0) {
  if (!texture || !cells.valid()) return;
  frame = std::clamp(frame, 0, static_cast<int>(cells.bounds.size()) - 1);
  const SDL_Rect& source = cells.bounds[static_cast<std::size_t>(frame)];
  const float width = static_cast<float>(source.w) * unitsPerPixel;
  const float height = static_cast<float>(source.h) * unitsPerPixel;
  // The hands are centred in the cell by the importer; the opaque bounds
  // may be offset, so centre on the cell rather than the bounds.
  const float cellCenter = static_cast<float>(
      (frame % cells.columns) * cells.cellWidth + cells.cellWidth / 2);
  const float offset = (static_cast<float>(source.x) + static_cast<float>(source.w) * 0.5F -
                        cellCenter) * unitsPerPixel;
  const SDL_FRect destination{centerX + offset - width * 0.5F, topY, width, height};
  // Rotate about the hands (the top centre of the cell).
  const SDL_FPoint pivot{-offset + width * 0.5F, 0.0F};
  SDL_RenderCopyExF(renderer, texture, &source, &destination, angle, &pivot,
                    SDL_FLIP_NONE);
}

// Source-pixel sizes measured in MAME: Charlie is 23x26, a monkey 21x18.
// The HD cells are scaled so those bodies keep their board size.
constexpr float kStage2CharlieUnitsPerPixel = 66.0F / 376.0F;
constexpr float kStage2MonkeyUnitsPerPixel = 45.0F / 228.0F;
// The v2 atlases (tools/import_sprites.py): Charlie stands 250 px tall in
// the walk cycle and the jump was scaled to match; a monkey is 160 px.
constexpr float kStage2CharlieV2UnitsPerPixel = 66.0F / 250.0F;
// The v3 atlases come from the Blender rig (art/blender/render_stage2.py,
// 70 px per unit): 350 px from sole to pompom, 253 px of that is Charlie
// himself, so the body keeps about its v2 size under the taller hat.
constexpr float kStage2CharlieV3UnitsPerPixel = 74.0F / 350.0F;
// The v4 atlases are cut from AI video clips of the painted Charlie
// (tools/clip_to_frames.py; art/source/incoming/clips/): about 400 px from
// sole to pompom in the walk, scaled to the same 74 units as v3.
constexpr float kStage2CharlieV4UnitsPerPixel = 74.0F / 400.0F;
// On the balls (80 units across) he stands a little taller, as the board's
// 26-row sprite does next to its 32-row ball.
constexpr float kStage4CharlieV4UnitsPerPixel = 80.0F / 400.0F;
constexpr float kStage2MonkeyV2UnitsPerPixel = 45.0F / 160.0F;

// The rope sits on source rows 152-153, the start platform on 147-153 over
// columns 0-43 with its truss at 17-30, the distance signs on rows 168-178,
// the blue rail on 181-183, the crowd on 185-207, the valance on 208-223 and
// the grass from 224.
constexpr float kStage2SignY = 2.5F * 168.0F - 8.0F;
constexpr float kStage2StartPlatformRight = 43.0F * kSourceToWorldX;
constexpr float kStage2StartTowerX = 24.0F * kSourceToWorldX;

void drawStage2Rope(SDL_Renderer* renderer, float cameraX, float fromX,
                    float toX) {
  if (toX <= fromX) return;
  const float braidPhase = std::fmod(cameraX, 12.0F);
  fillRect(renderer, fromX, kStage2RopeY - 1.0F, toX - fromX, 6.0F,
           color(47, 28, 18));
  fillRect(renderer, fromX, kStage2RopeY, toX - fromX, 4.0F,
           color(202, 166, 112));
  fillRect(renderer, fromX, kStage2RopeY + 0.8F, toX - fromX, 1.1F,
           color(250, 224, 174));
  for (int knot = -1; knot < 42; ++knot) {
    const float x = static_cast<float>(knot * 12) - braidPhase;
    if (x + 12.0F < fromX || x > toX) continue;
    line(renderer, std::max(x, fromX), kStage2RopeY,
         std::min(x + 6.0F, toX), kStage2RopeY + 4.0F, color(126, 79, 43));
    line(renderer, std::max(x + 6.0F, fromX), kStage2RopeY,
         std::min(x + 12.0F, toX), kStage2RopeY + 4.0F,
         color(234, 196, 137));
  }
}

// The start tower: a red and green platform at rope height carried by a
// steel truss down to the arena wall.
void drawStage2StartTower(SDL_Renderer* renderer, float towerX,
                          float platformLeft, float platformRight) {
  const float top = kStage2RopeY - 12.0F;
  fillRect(renderer, towerX - 11.0F, top + 12.0F, 5.0F, 180.0F,
           color(201, 209, 218));
  fillRect(renderer, towerX + 6.0F, top + 12.0F, 5.0F, 180.0F,
           color(201, 209, 218));
  for (int brace = 0; brace < 8; ++brace) {
    const float y = top + 16.0F + brace * 22.0F;
    line(renderer, towerX - 7.0F, y, towerX + 8.0F, y + 18.0F,
         color(115, 127, 143));
    line(renderer, towerX + 8.0F, y, towerX - 7.0F, y + 18.0F,
         color(244, 246, 249));
  }
  fillRect(renderer, platformLeft, top, platformRight - platformLeft, 7.0F,
           color(34, 194, 70));
  fillRect(renderer, platformLeft, top + 7.0F, platformRight - platformLeft,
           5.0F, color(234, 47, 49));
}

void drawStage2Marker(SDL_Renderer* renderer, float x,
                      std::string_view label) {
  if (gFloorSign) {
    drawSignHD(renderer, x, kStage2SignY + 29.0F, label);
    return;
  }
  fillRect(renderer, x - 27.0F, kStage2SignY, 54.0F, 25.0F,
           color(35, 122, 204));
  fillRect(renderer, x - 24.0F, kStage2SignY + 3.0F, 48.0F, 19.0F,
           color(255, 239, 105));
  drawText(renderer, label, x, kStage2SignY + 7.0F, 1.3F,
           color(227, 52, 37), true);
}

// The cyan-framed score number the board shows over a turned monkey and the
// 5000 over the goal stand.
void drawStage2ScoreBox(SDL_Renderer* renderer, float centerX, float top,
                        int value) {
  const std::string text = std::to_string(value);
  const float width = 12.0F + static_cast<float>(text.size()) * 12.0F;
  fillRect(renderer, centerX - width * 0.5F, top, width, 22.0F,
           color(46, 224, 247));
  fillRect(renderer, centerX - width * 0.5F + 2.0F, top + 2.0F, width - 4.0F,
           18.0F, color(0, 0, 0));
  drawText(renderer, text, centerX, top + 5.0F, 1.8F, color(87, 219, 255),
           true);
}

// The middle cell of a stage-2 plaque names the distance ($EB48 table).
std::string level2PlaqueText(int cell) {
  switch (cell) {
    case 0xb9: return "START";
    case 0xc5: return "60M";
    case 0xc1: return "50M";
    case 0xc0: return "40M";
    case 0xbf: return "30M";
    case 0xbe: return "20M";
    case 0xbd: return "10M";
    case 0xc3: return "GOAL";
    default: return {};
  }
}

// $2760: the goal stand.  Its three cells are at col, col-8 and col+8, so
// the pole stands on column col-8; the bags cover rows $66-$86 and Charlie
// lands on them with his feet on row $85.  The HD rig's platforms are cut
// apart so the upper one sits where the bags stand, the lower one at the
// rope, and the truss runs on down to the arena floor.
// The scroll stops at page $F8: progress $10000 - $F8FF.
constexpr int kStage2GoalProgress = 0x10000 - 0xf8ff;

// The goal rig at the end of the rope, as measured on MAME: a lattice tower
// 16 columns wide from the grass up to a high platform (48 columns, rows
// 132-135, green over red) that carries the bag pile; a lower platform
// (rows 146-153, red over green) 96 columns wide starts 40 columns left of
// the tower at the rope's height, and the rope ends at its left edge.  A
// weight hangs on a string under the high platform's right end.
constexpr float kStage2TowerColumn = static_cast<float>(kStage2GoalProgress + 0xc0 - 9);
void drawStage2GoalTower(SDL_Renderer* renderer, const Assets& assets,
                         float x) {
  const float scale = kSourceToWorldX;
  const float highTop = level3RowToY(132.0F);
  const float highBottom = level3RowToY(136.0F);
  const float lowTop = level3RowToY(146.0F);
  const float lowBottom = level3RowToY(154.0F);
  const float floorY = level3RowToY(224.0F);
  const float towerWidth = 16.0F * scale;
  if (assets.stage6Lattice) {
    const float bay = 16.0F * kLevel3RowScale;
    for (float y = highBottom; y < floorY; y += bay) {
      const float h = std::min(bay, floorY - y);
      const SDL_Rect source{0, 0, 300, static_cast<int>(160.0F * h / bay)};
      const SDL_FRect destination{x - towerWidth * 0.5F, y, towerWidth, h};
      SDL_RenderCopyF(renderer, assets.stage6Lattice, &source, &destination);
    }
  } else {
    fillRect(renderer, x - 8.0F, highBottom, 16.0F, floorY - highBottom,
             color(201, 209, 218));
  }
  // The lower platform: red over green.
  const float lowLeft = x - 40.0F * scale;
  const float lowWidth = 96.0F * scale;
  fillRect(renderer, lowLeft, lowTop, lowWidth, (lowBottom - lowTop) * 0.5F,
           color(234, 47, 49));
  fillRect(renderer, lowLeft, lowTop + (lowBottom - lowTop) * 0.5F, lowWidth,
           (lowBottom - lowTop) * 0.5F, color(60, 214, 90));
  fillRect(renderer, lowLeft, lowTop, lowWidth, 2.0F, color(255, 150, 150));
  // The high platform: green over red.
  const float highLeft = x - 24.0F * scale;
  const float highWidth = 48.0F * scale;
  fillRect(renderer, highLeft, highTop, highWidth, (highBottom - highTop) * 0.5F,
           color(60, 214, 90));
  fillRect(renderer, highLeft, highTop + (highBottom - highTop) * 0.5F, highWidth,
           (highBottom - highTop) * 0.5F, color(234, 47, 49));
  fillRect(renderer, highLeft, highTop, highWidth, 2.0F, color(170, 255, 180));
  // The weight on its string under the right end.
  const float stringX = highLeft + highWidth - 4.0F * scale;
  fillRect(renderer, stringX - 0.7F, highBottom, 1.4F, 8.0F * kLevel3RowScale,
           color(230, 230, 240));
  filledCircle(renderer, stringX, highBottom + 8.0F * kLevel3RowScale + 4.0F, 5.0F,
               color(250, 205, 40));
  filledCircle(renderer, stringX - 1.5F, highBottom + 8.0F * kLevel3RowScale + 2.5F,
               1.6F, color(255, 245, 190));
}

void drawStage2GoalStand(SDL_Renderer* renderer, const Game& game,
                         const Assets& assets) {
  // $2760: the bag pile on the high platform (two bags below, one on top;
  // the cells sit at col-8, col+8 and col, nine columns left of the record)
  // and, once Charlie has landed on it, the 5000 sign.
  const Level4Record& stand = game.level2Stand;
  if (stand.active == 0) return;
  const float scale = kSourceToWorldX;
  const float x = (static_cast<float>(stand.col) - 9.0F) * scale;
  const float platformTop = level3RowToY(132.0F);
  const float bagW = 16.0F * scale;
  const float bagH = 15.0F * kLevel3RowScale;
  if (stand.state == 0) {
    const auto bag = [&](float cx, float bottom) {
      if (assets.rewardBag) {
        const SDL_FRect destination{cx - bagW * 0.5F, bottom - bagH, bagW, bagH};
        SDL_RenderCopyF(renderer, assets.rewardBag, nullptr, &destination);
      } else {
        fillRect(renderer, cx - bagW * 0.5F, bottom - bagH, bagW, bagH,
                 color(232, 214, 160));
      }
    };
    bag(x - 8.0F * scale, platformTop);
    bag(x + 8.0F * scale, platformTop);
    bag(x, platformTop - bagH + 2.0F);
  } else {
    drawStage2ScoreBox(renderer, x - 12.0F * scale, level3RowToY(118.0F), 5000);
  }
}

void drawStage2Backdrop(SDL_Renderer* renderer, const Game& game,
                        float cameraX, const Assets& assets,
                        double timeSeconds) {
  fillRect(renderer, 0.0F, 0.0F, kWorldWidth, kWorldHeight, color(0, 0, 0));
  if (assets.marquee) {
    const SDL_FRect marqueeDestination{
        0.0F, 0.0F, static_cast<float>(kWorldWidth), kMarqueeHeight};
    SDL_RenderCopyF(renderer, assets.marquee, nullptr, &marqueeDestination);
    drawReddishMarqueeSky(renderer);
  }
  drawFerrisWheel(renderer, assets.ferrisWheel, assets.ferrisGondola,
                  timeSeconds);
  drawZeppelinBonus(renderer, game.bonus);

  // The arena wall starts at the blue rail (source row 181).
  const float arenaTop = level3RowToY(181.0F);
  if (assets.stage2Backdrop) {
    // The big top behind the rope, scrolling at a third of the rope's pace.
    const float tileWidth = static_cast<float>(kWorldWidth);
    float offset = std::fmod(cameraX * 0.33F, tileWidth);
    if (offset < 0.0F) offset += tileWidth;
    for (float x = -offset; x < kWorldWidth; x += tileWidth) {
      const SDL_FRect destination{x, kArenaTop, tileWidth, arenaTop - kArenaTop};
      SDL_RenderCopyF(renderer, assets.stage2Backdrop, nullptr, &destination);
    }
  }
  if (assets.arena) {
    int textureWidth = 0;
    int textureHeight = 0;
    SDL_QueryTexture(assets.arena, nullptr, nullptr, &textureWidth,
                     &textureHeight);
    const int sourceTop = static_cast<int>(
        static_cast<float>(textureHeight) * kArenaContentSourceTop);
    drawArenaBand(renderer, assets.arena, assets.elephantDoor, cameraX, arenaTop,
                  (kWorldHeight - arenaTop) / static_cast<float>(textureHeight - sourceTop));
  } else {
    fillRect(renderer, 0.0F, arenaTop, kWorldWidth, kWorldHeight - arenaTop,
             color(36, 142, 49));
  }

  // The start platform and the rope that leaves it.
  const float startTowerX = kStage2StartTowerX - cameraX;
  const float platformRight = kStage2StartPlatformRight - cameraX;
  float ropeFrom = 0.0F;
  if (platformRight > -20.0F) {
    drawStage2StartTower(renderer, startTowerX, std::min(0.0F, -cameraX),
                         platformRight);
    ropeFrom = std::max(0.0F, platformRight);
  }
  float ropeTo = static_cast<float>(kWorldWidth);
  // The goal rig on the last page, placed by the stand record's column.
  const float towerX = kStage2TowerColumn * kSourceToWorldX - cameraX;
  const float lowLeft = towerX - 40.0F * kSourceToWorldX;
  if (lowLeft < kWorldWidth + 20.0F) ropeTo = std::min(ropeTo, lowLeft);
  drawStage2Rope(renderer, cameraX, ropeFrom, ropeTo);
  if (towerX > -260.0F && towerX < kWorldWidth + 260.0F) {
    drawStage2GoalTower(renderer, assets, towerX);
  }

  // $26C0-$26E0: the distance sign is three cells; the middle one names it.
  {
    const Level4Plaque& middle = game.level2Plaques[1];
    const std::string label = level2PlaqueText(middle.cell);
    const float x = static_cast<float>(middle.col - 8) * kSourceToWorldX;
    if (!label.empty() && x > -60.0F && x < kWorldWidth + 60.0F) {
      drawStage2Marker(renderer, x, label);
    }
  }
  drawStage2GoalStand(renderer, game, assets);
}

// $2440-$25C0: the monkeys, from their board records and $87CE scripts.
void drawStage2Monkeys(SDL_Renderer* renderer, const Game& game,
                       const Assets& assets) {
  for (const auto& m : game.level2Monkeys) {
    if (m.active == 0) continue;
    const float centerX = static_cast<float>(m.col - 5) * kSourceToWorldX;
    if (centerX < -80.0F || centerX > kWorldWidth + 80.0F) continue;
    const float feetY = level3RowToY(static_cast<float>(m.row) + 16.0F);
    const int pose = std::max(m.pose, 0);
    SDL_Texture* texture = assets.stage2BrownWalk;
    const SheetCells* cells = &assets.stage2BrownCells;
    // The knuckle-walk (three poses, the middle one twice) faces left in
    // the atlas, the way the apes come at Charlie, and plods at the
    // cabinet's pace: a new pose every ten frames.  Only the turned one is
    // mirrored.
    const std::size_t index = static_cast<std::size_t>(&m - game.level2Monkeys.data());
    const int walkFrames = std::max(1, static_cast<int>(cells->bounds.size()));
    int frame = ((game.level2MonkeyAge + static_cast<int>(index) * 7) / 10) % walkFrames;
    SDL_RendererFlip flip = SDL_FLIP_NONE;
    float scale = kStage2MonkeyV2UnitsPerPixel;
    if (m.kind != 0) {
      texture = assets.stage2PurpleWalk;
      cells = &assets.stage2PurpleCells;
    }
    switch (m.state) {
      case 8: {   // $89C2 leaping: the $EBE9 arc's three cells
        // Crouch, flight and landing, at the walk's scale.
        texture = assets.stage2PurpleJump;
        cells = &assets.stage2PurpleJumpCells;
        frame = pose % 3;
        flip = SDL_FLIP_NONE;
        break;
      }
      case 1:     // $897F turned after being jumped: faces the other way
        frame = 0;
        flip = SDL_FLIP_HORIZONTAL;
        break;
      default:
        break;
    }
    drawStage2Pose(renderer, texture, *cells, frame, centerX, feetY, scale,
                   flip);
  }
}

// $2600-$26B0: the score numbers (state 1) and the callout cells (state 2).
void drawStage2Popups(SDL_Renderer* renderer, const Game& game) {
  for (const auto& popup : game.level2Popups) {
    if (popup.active == 0) continue;
    const float x = static_cast<float>(popup.col) * kSourceToWorldX;
    const float y = level3RowToY(static_cast<float>(popup.row));
    if (popup.state == 1) {
      // Two cells: the number spans col-16 .. col+15.
      if (popup.owner == 1) continue;
      drawStage2ScoreBox(renderer, x, y, popup.cell);
      continue;
    }
    if (popup.state != 2 || popup.cell % 3 != 0) continue;
    // Each word is three 16x16 cells starting at this one.
    const int group = popup.cell / 3;
    const char* word = popup.kind == 1 ? (group == 1 ? "GREAT" : "FAR OUT")
                                       : (group == 1 ? "NO!!" : "OH");
    const float centerX = static_cast<float>(popup.col + 8) * kSourceToWorldX;
    const SDL_Color value = popup.kind == 1
                                ? (group == 1 ? color(255, 93, 36)
                                              : color(87, 219, 255))
                                : color(255, 82, 28);
    for (const auto& offset : std::array<Vec2, 4>{
             Vec2{-2.0F, 0.0F}, Vec2{2.0F, 0.0F}, Vec2{0.0F, -2.0F},
             Vec2{0.0F, 2.0F}}) {
      drawText(renderer, word, centerX + offset.x, y + 8.0F + offset.y, 2.2F,
               color(45, 10, 24), true);
    }
    drawText(renderer, word, centerX, y + 8.0F, 2.2F, value, true);
  }
}

// $2400: Charlie.  The board never mirrors him on the rope -- the sprite
// attribute stays $20 walking either way, so walking left is the same
// forward-facing walk in reverse, as on the lion.  The scripts run four
// walking cells; the HD sheet has two, so they alternate.
void drawStage2Charlie(SDL_Renderer* renderer, const Game& game,
                       const Assets& assets) {
  const Level4Record& charlie = game.level2Charlie;
  if (charlie.active == 0) return;
  const float centerX = static_cast<float>(charlie.col) * kSourceToWorldX;
  const float feetY = level3RowToY(static_cast<float>(charlie.row) + 16.0F);
  const int pose = std::max(charlie.pose, 0);
  int frame = 0;
  if (charlie.script == kLevel2ScriptWalking && assets.stage2CharlieWalk) {
    // The nine-frame cycle advances with the walk; standing still holds.
    frame = (game.level2WalkAge / 3) % 9;
    drawStage2Pose(renderer, assets.stage2CharlieWalk,
                   assets.stage2CharlieWalkCells, frame, centerX, feetY,
                   kStage2CharlieV4UnitsPerPixel * game.charlieScale);
    return;
  }
  if (charlie.script == kLevel2ScriptJumping && assets.stage2CharlieJump) {
    // The 57-frame arc over the ten jump frames: crouch, launch, tuck, land.
    frame = std::min(9, game.level2JumpAge * 10 / 57);
    drawStage2Pose(renderer, assets.stage2CharlieJump,
                   assets.stage2CharlieJumpCells, frame, centerX, feetY,
                   kStage2CharlieV4UnitsPerPixel * game.charlieScale);
    return;
  }
  if (assets.stage2CharliePoses) {
    // The painted poses atlas: 0 stand, 1 arms up, 2 fallen, 3 tuck.
    constexpr int kStand = 0, kArmsUp = 1, kFallen = 2, kTuck = 3;
    double tilt = 0.0;
    switch (charlie.script) {
      case kLevel2ScriptJumping: frame = kTuck; break;              // $E9CF
      case kLevel2ScriptGoalJump:                                   // $E9D8 the flip
        frame = (pose % 6) == 5 ? kArmsUp : kTuck;
        break;
      case kLevel2ScriptCheer: frame = pose & 1 ? kStand : kArmsUp; break;   // $EAAA
      case kLevel2ScriptFallen: frame = kFallen; break;             // $EA9E
      default:                                                      // $E9FF standing
        // The script's three cells are the balance wobble: upright, then
        // leaning back and forward; the idle timer plays it faster and
        // faster until he drops.
        frame = kStand;
        tilt = pose == 1 ? -12.0 : pose == 2 ? 12.0 : 0.0;
        break;
    }
    drawStage2PoseTilted(renderer, assets.stage2CharliePoses,
                         assets.stage2CharliePosesCells, frame, centerX, feetY,
                         kStage2CharlieV4UnitsPerPixel * game.charlieScale, tilt);
    return;
  }
  switch (charlie.script) {
    case kLevel2ScriptWalking: frame = pose & 1; break;
    case kLevel2ScriptJumping: frame = 4; break;       // $E9CF one cell
    case kLevel2ScriptGoalJump: {                      // $E9D8 the flip
      static constexpr std::array<int, 6> cells{4, 3, 3, 3, 3, 5};
      frame = cells[static_cast<std::size_t>(pose % 6)];
      break;
    }
    case kLevel2ScriptCheer: frame = pose & 1 ? 0 : 5; break;   // $EAAA
    case kLevel2ScriptFallen: frame = 2; break;        // $EA9E
    default: frame = 0; break;                         // $E9FF standing
  }
  drawStage2Pose(renderer, assets.stage2Charlie, assets.stage2CharlieCells,
                 frame, centerX, feetY, kStage2CharlieUnitsPerPixel);
}

// $2700/$2720: the perfect-clear bird flying left with its bag.
void drawStage2Bird(SDL_Renderer* renderer, const Game& game,
                    const Assets& assets) {
  if (!game.level2BirdActive) return;
  const float birdX = static_cast<float>(game.level2BirdX) * kSourceToWorldX;
  const float birdY = level3RowToY(0x40) + 60.0F;
  if (assets.bird) {
    int textureWidth = 0;
    int textureHeight = 0;
    SDL_QueryTexture(assets.bird, nullptr, nullptr, &textureWidth,
                     &textureHeight);
    const int cellWidth = textureWidth / 4;
    const int flap = (game.level2BirdX / 8) & 1;
    const SDL_Rect source{flap * cellWidth, 0, cellWidth, textureHeight};
    const SDL_FRect destination{birdX - 45.0F, birdY - 60.0F, 90.0F, 120.0F};
    SDL_RenderCopyF(renderer, assets.bird, &source, &destination);
  }
  if (game.level2BirdBagX != 0 && assets.rewardBag) {
    const float bagX = static_cast<float>(game.level2BirdBagX) * kSourceToWorldX;
    const float bagY = level3RowToY(0x50) + 60.0F;
    const SDL_FRect destination{bagX - 24.0F, bagY - 10.0F, 48.0F, 59.0F};
    SDL_RenderCopyF(renderer, assets.rewardBag, nullptr, &destination);
  }
}

void drawStage2Scene(SDL_Renderer* renderer, const Game& game,
                     const Assets& assets, double timeSeconds,
                     double interpolation) {
  (void)interpolation;   // the board moves in whole columns
  const float camera = game.cameraX;
  drawStage2Backdrop(renderer, game, camera, assets, timeSeconds);
  const bool restarting =
      game.scene == Scene::Crashed && game.crashFrame >= 24;
  // The cabinet's sprites lie over the score panel, so a high jump shows
  // Charlie in front of its bulbs.
  drawHud(renderer, game, assets.charlieLife);
  if (!restarting) {
    drawStage2Monkeys(renderer, game, assets);
    drawStage2Charlie(renderer, game, assets);
    drawStage2Popups(renderer, game);
    drawStage2Bird(renderer, game, assets);
  }
}

void drawSheetFrame(SDL_Renderer* renderer, SDL_Texture* texture,
                    int columns, int rows, int frame, float centerX,
                    float baselineY, float width, float height,
                    SDL_RendererFlip flip = SDL_FLIP_NONE,
                    double angle = 0.0) {
  if (!texture || columns <= 0 || rows <= 0) return;
  int textureWidth = 0;
  int textureHeight = 0;
  SDL_QueryTexture(texture, nullptr, nullptr, &textureWidth, &textureHeight);
  const int cellWidth = textureWidth / columns;
  const int cellHeight = textureHeight / rows;
  frame = std::clamp(frame, 0, columns * rows - 1);
  const SDL_Rect source{(frame % columns) * cellWidth,
                        (frame / columns) * cellHeight,
                        cellWidth, cellHeight};
  const SDL_FRect destination{centerX - width * 0.5F,
                              baselineY - height, width, height};
  SDL_RenderCopyExF(renderer, texture, &source, &destination, angle, nullptr,
                    flip);
}

// ---- Level 3 drawing ------------------------------------------------------
// Board columns are drawn relative to the interpolated camera: every scrolled
// record keeps a constant world column (progress + column), so only the camera
// needs interpolating.

float level3ScreenX(int progress, int column, float camera) {
  return static_cast<float>(progress + column) * kSourceToWorldX - camera;
}

void drawStage3Tambourine(SDL_Renderer* renderer, SDL_Texture* texture,
                          float x, float squash, bool goal) {
  if (!texture) {
    const float width = 48.0F * kSourceToWorldX;
    fillRect(renderer, x - width * 0.5F, kStage3TambourineTopY, width,
             kStage3GroundY - kStage3TambourineTopY, color(236, 88, 173));
    return;
  }
  const float normalHeight = kStage3GroundY - kStage3TambourineTopY;
  const float height = normalHeight * (1.0F - squash * 0.18F);
  // The arcade drum is six tiles (48 columns) wide; the goal painting keeps
  // the wider silhouette chosen for the finish.
  const float width = goal ? 172.0F : 48.0F * kSourceToWorldX;
  const SDL_FRect destination{x - width * 0.5F, kStage3GroundY - height,
                              width, height};
  SDL_RenderCopyF(renderer, texture, nullptr, &destination);
}

void drawLevel3Sign(SDL_Renderer* renderer, float screenX,
                    std::string_view label) {
  if (gFloorSign) {
    drawSignHD(renderer, screenX, level3RowToY(0xf0) + 34.0F, label);
    return;
  }
  // $26C0-$26E0 props: three 16-column cells at row $F0, the bottom of the
  // board's screen.
  const float width = 48.0F * kSourceToWorldX;
  const float top = level3RowToY(0xf0) + 4.0F;
  const float height = 30.0F;
  fillRect(renderer, screenX - width * 0.5F, top, width, height,
           color(29, 179, 239));
  fillRect(renderer, screenX - width * 0.5F + 4.0F, top + 4.0F,
           width - 8.0F, height - 8.0F, color(239, 238, 196));
  drawText(renderer, label, screenX, top + 8.0F, 1.1F,
           color(245, 83, 24), true);
}

int level3PerformerFrame(const Level3Performer& performer) {
  // Fire breather: $ECE7 idle (torch flicker every 8 frames), $ECC3 breathing
  // (32 frames).  Juggler: $ED17 idle, $ED0E throw (14 frames), $ECFC catch
  // (14 catch, 10 idle, 14 throw).
  const int frame = performer.poseFrame;
  if (performer.type == 0) {
    if (performer.pose == 1 && frame < 32) {
      return 3 + std::min(frame / 8, 3);
    }
    constexpr std::array<int, 4> idle{0, 8, 10, 11};
    return idle[static_cast<std::size_t>((frame / 8) & 3)];
  }
  if (performer.pose == 2) {
    if (frame < 7) return 3;
    if (frame < 14) return 4;
    return 0;
  }
  if (performer.pose == 3) {
    if (frame < 14) return 6;
    if (frame < 24) return 5;
    if (frame < 31) return 3;
    if (frame < 38) return 4;
    return 0;
  }
  return 0;
}

void drawLevel3Charlie(SDL_Renderer* renderer, const Game& game,
                       const Assets& assets, float screenX, float feetY) {
  // Poses: $EC93/$EC6C flip through five cells for twenty frames and then
  // hold the upright cell; $EC4E stays on one cell for 24 frames and then
  // alternates two cells every four frames; $EAAA alternates every sixteen.
  // The v4 atlases (tools/build_stage3_charlie.py) hold the board's front
  // view of the bounce and the Stage 2 tuck, at the Stage 2 v4 scale, so
  // Charlie keeps his rope size here.
  SDL_Texture* texture = assets.stage3Charlie;
  const SheetCells* cells = &assets.stage3CharlieCells;
  const float unitsPerPixel = kStage2CharlieV4UnitsPerPixel * game.charlieScale;
  int frame = 0;
  float baseline = feetY;
  const int poseFrame = game.level3PoseFrame;
  // $9015: seven frozen frames, then the fall.
  const bool struck = game.level3State == 7 && game.level3Countdown == 0;
  if (game.level3Pose == Level3Pose::Roof) return;
  switch (struck ? Level3Pose::Fallen : game.level3Pose) {
    case Level3Pose::MovingRight:
    case Level3Pose::MovingLeft: {
      if (poseFrame < 20) {
        // The board's five flip cells, four frames each, are one forward
        // somersault in twenty frames (launch, dive, tipping over, the
        // upside-down tuck, unrolling): cells 3-7, drawn about his middle
        // and mirrored when he goes left.
        if (!texture || !cells->valid()) break;
        const float standing =
            static_cast<float>(cells->bounds[0].h) * unitsPerPixel;
        drawPoseSpinning(renderer, texture, *cells, 3 + poseFrame / 4, screenX,
                         feetY - standing * 0.5F, unitsPerPixel, 0.0,
                         game.level3Pose == Level3Pose::MovingLeft
                             ? SDL_FLIP_HORIZONTAL
                             : SDL_FLIP_NONE);
        return;
      }
      frame = 0;
      break;
    }
    case Level3Pose::Stationary:
      texture = assets.stage3CharlieVertical;
      cells = &assets.stage3CharlieVerticalCells;
      if (poseFrame < 24) {
        frame = std::min(poseFrame / 4, 3);
      } else {
        frame = ((poseFrame / 4) & 1) == 0 ? 4 : 6;
      }
      break;
    case Level3Pose::Cheer:
      // $9173 lifts the celebration cells ten rows; the painted cheer keeps
      // its shoes on the drum instead.
      frame = ((poseFrame / 16) & 1) == 0 ? 11 : 10;
      // ... and the goal drum's painted top surface sits 22 units under the
      // drum's top edge.
      baseline += 10.0F * kLevel3RowScale + 22.0F;
      break;
    case Level3Pose::Fallen:
      frame = struck ? 10 : 9;
      break;
    case Level3Pose::Roof:
      return;
  }
  if (!texture || !cells->valid()) {
    fillRect(renderer, screenX - 20.0F, feetY - 80.0F, 40.0F, 80.0F,
             color(220, 40, 40));
    return;
  }
  const SDL_RendererFlip flip = game.level3Pose == Level3Pose::MovingLeft
                                    ? SDL_FLIP_HORIZONTAL
                                    : SDL_FLIP_NONE;
  drawStage2Pose(renderer, texture, *cells, frame, screenX, baseline,
                 unitsPerPixel, flip);
}

// $7DF2/$7F3B: the perfect-clear bird, its bag and the coins it pours out.
// Event 2 shares these records with Event 3.
void drawLevel3BirdAndCoins(SDL_Renderer* renderer, const Game& game,
                            const Assets& assets) {
  if (game.level3BirdActive) {
    // Keep the bird just below the scoreboard instead of over it.
    const float birdX = static_cast<float>(game.level3BirdX + 8) * kSourceToWorldX;
    const float birdY = level3RowToY(0x40) + 60.0F;
    if (assets.bird) {
      int textureWidth = 0;
      int textureHeight = 0;
      SDL_QueryTexture(assets.bird, nullptr, nullptr, &textureWidth,
                       &textureHeight);
      const int cellWidth = textureWidth / 4;
      const int flap = (game.level3PoseFrame / 8) & 1;
      const int cell = game.level3BirdState == 3 ? flap : 2 + flap;
      const SDL_Rect source{cell * cellWidth, 0, cellWidth, textureHeight};
      const SDL_FRect destination{birdX - 45.0F, birdY - 60.0F, 90.0F, 120.0F};
      SDL_RenderCopyExF(renderer, assets.bird, &source, &destination, 0.0,
                        nullptr, SDL_FLIP_HORIZONTAL);
    }
    if (!game.level3CoinStarted) {
      const float bagX = static_cast<float>(game.level3BirdBagX - 8) * kSourceToWorldX;
      const float bagY = level3RowToY(0x50) + 60.0F;
      if (assets.rewardBag) {
        const SDL_FRect destination{bagX - 24.0F, bagY - 10.0F, 48.0F, 59.0F};
        SDL_RenderCopyF(renderer, assets.rewardBag, nullptr, &destination);
      }
    }
    for (const auto& coin : game.level3Coins) {
      if (!coin.active || coin.bagCopy) continue;
      const float x = static_cast<float>(coin.x - 8) * kSourceToWorldX;
      const float y = level3RowToY(static_cast<float>(coin.y) + 8.0F) + 40.0F;
      drawCoin(renderer, x, y,
               std::abs(std::cos(static_cast<float>(coin.age) * (kPi / 12.0F))));
    }
  }
}

// ---- The water round ($2207) ---------------------------------------------
// The board swaps the grass below tile row 26 for water (tile $B6) with the
// drums standing in it; the bank keeps two tile rows of grass above the
// surface.  Board row 208 is the surface.
constexpr float kStage3WaterSurfaceY = 208.0F * kLevel3RowScale + kLevel3RowOffset;  // 512
constexpr int kStage3LadderColumn = 40;   // tile row 3 of every 256-column page

void drawStage3WaterTiles(SDL_Renderer* renderer, const Assets& assets,
                          float camera, double timeSeconds, Uint8 alpha) {
  const float top = kStage3WaterSurfaceY;
  const float height = kWorldHeight - top;
  if (!assets.stage3WaterTile) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 36, 120, 210, alpha);
    const SDL_FRect rect{0.0F, top, kWorldWidth, height};
    SDL_RenderFillRectF(renderer, &rect);
    return;
  }
  SDL_SetTextureBlendMode(assets.stage3WaterTile, SDL_BLENDMODE_BLEND);
  SDL_SetTextureAlphaMod(assets.stage3WaterTile, alpha);
  const float tileWidth = 256.0F;
  const float drift = static_cast<float>(std::fmod(timeSeconds * 18.0, tileWidth));
  float offset = std::fmod(camera * 0.85F + drift, tileWidth);
  if (offset < 0.0F) offset += tileWidth;
  for (float x = -offset; x < kWorldWidth; x += tileWidth) {
    const SDL_FRect destination{x, top, tileWidth, height};
    SDL_RenderCopyF(renderer, assets.stage3WaterTile, nullptr, &destination);
  }
  SDL_SetTextureAlphaMod(assets.stage3WaterTile, 255);
}

void drawStage3WaterSurface(SDL_Renderer* renderer, const Assets& assets,
                            float camera, double timeSeconds) {
  // The pool's coping along the bank.
  fillRect(renderer, 0.0F, kStage3WaterSurfaceY - 10.0F, kWorldWidth, 10.0F,
           color(214, 206, 178));
  fillRect(renderer, 0.0F, kStage3WaterSurfaceY - 3.0F, kWorldWidth, 3.0F,
           color(156, 146, 118));
  if (!assets.stage3WaterSurface) return;
  const float tileWidth = 256.0F;
  const float drift = static_cast<float>(std::fmod(timeSeconds * 26.0, tileWidth));
  float offset = std::fmod(camera * 0.85F - drift, tileWidth);
  if (offset < 0.0F) offset += tileWidth;
  for (float x = -offset; x < kWorldWidth; x += tileWidth) {
    const SDL_FRect destination{x, kStage3WaterSurfaceY - 2.0F, tileWidth, 16.0F};
    SDL_RenderCopyF(renderer, assets.stage3WaterSurface, nullptr, &destination);
  }
}

void drawStage3Dolphin(SDL_Renderer* renderer, const Assets& assets,
                       const Level3Performer& dolphin, float screenX) {
  // The composite is 32 rows tall with its lower cells at the record row,
  // so the body is centred on the row itself.
  const float centerY = level3RowToY(static_cast<float>(dolphin.y));
  // Nose along the swim: two columns a frame left, the velocity's high byte
  // up (phases 1 and 4) or down (2 and 3) in rows.
  const float rows = static_cast<float>(dolphin.velocity >> 8);
  const bool up = dolphin.phase == 1 || dolphin.phase == 4;
  const float dx = -2.0F * kSourceToWorldX;
  const float dy = (up ? -rows : rows) * kLevel3RowScale;
  double angle = 0.0;
  if (dolphin.state == 3) {
    angle = (std::atan2(dy, dx) - kPi) * 180.0 / kPi;
    while (angle < -180.0) angle += 360.0;
    while (angle > 180.0) angle -= 360.0;
  }
  if (!assets.stage3Dolphin) {
    ellipse(renderer, screenX, centerY, 40.0F, 16.0F, color(120, 170, 210), 4);
    return;
  }
  // Six painted poses along the arc (nose up ... nose down), each turned by
  // the rest of the angle so the motion stays smooth.
  static constexpr std::array<double, 6> poseAngles{70.0, 40.0, 5.0, -20.0, -60.0, -85.0};
  int frame = 0;
  double best = 1e9;
  for (int index = 0; index < 6; ++index) {
    const double distance = std::abs(angle - poseAngles[static_cast<std::size_t>(index)]);
    if (distance < best) {
      best = distance;
      frame = index;
    }
  }
  const double residual = angle - poseAngles[static_cast<std::size_t>(frame)];
  // Cells are 246x248 with the body centred; 32 board columns wide on the
  // board, which the square cell keeps in proportion.
  // The painted sheet has the dolphins swimming right; they cross the pool
  // leftward, so mirror the cell.
  const float size = 32.0F * kSourceToWorldX * 1.35F;
  drawSheetFrame(renderer, assets.stage3Dolphin, 6, 1, frame, screenX,
                 centerY + size * 0.5F, size, size, SDL_FLIP_HORIZONTAL,
                 residual);
}

void drawStage3Splash(SDL_Renderer* renderer, const Assets& assets,
                      const Level3Flame& splash, float screenX) {
  // Four painted crowns, growing over the first 24 frames and fading out.
  const int frame = std::min(3, splash.age / 6);
  const float width = 96.0F;
  const float height = 59.0F;
  if (!assets.stage3Splash) return;
  const Uint8 alpha = static_cast<Uint8>(
      splash.age < 20 ? 255 : std::max(0, 255 - (splash.age - 20) * 21));
  SDL_SetTextureAlphaMod(assets.stage3Splash, alpha);
  drawSheetFrame(renderer, assets.stage3Splash, 4, 1, frame, screenX,
                 kStage3WaterSurfaceY + 8.0F, width, height);
  SDL_SetTextureAlphaMod(assets.stage3Splash, 255);
}

void drawStage3Scene(SDL_Renderer* renderer, const Game& game,
                     const Assets& assets, double timeSeconds,
                     double interpolation) {
  // The 96-frame restart phase wipes the playfield and redraws the board at
  // the restart page ($6D2C); after the first quarter show that fresh board.
  const bool restarting = game.scene == Scene::Crashed && game.crashFrame >= 24;
  const int progress =
      restarting ? static_cast<int>((0x10000 - ((game.level3RestartPage & 0xff) << 8)) & 0xffff)
                 : level3Progress(game);
  const float camera = restarting
                           ? static_cast<float>(progress) * kSourceToWorldX
                           : game.previousCameraX +
                                 (game.cameraX - game.previousCameraX) *
                                     static_cast<float>(interpolation);
  drawBackdrop(renderer, camera, false, assets, game, timeSeconds);
  if (game.level3Water) {
    drawStage3WaterTiles(renderer, assets, camera, timeSeconds, 255);
    // Pool ladders on tile row 3 of every page.
    for (int page = 0; page < 8; ++page) {
      const float x = static_cast<float>(kStage3LadderColumn + 256 * page) *
                          kSourceToWorldX - camera;
      if (x < -40.0F || x > kWorldWidth + 40.0F) continue;
      if (assets.stage3PoolLadder) {
        const SDL_FRect destination{x - 17.0F, kStage3WaterSurfaceY - 60.0F,
                                    34.0F, 85.0F};
        SDL_RenderCopyF(renderer, assets.stage3PoolLadder, nullptr, &destination);
      }
    }
  }

  // Drums: tile rows 6-11, 17-22 and 27-0 of every 256-column page.
  for (int index = 0; index < kLevel3DrumCount; ++index) {
    const float x = static_cast<float>(level3DrumCentre(index)) *
                        kSourceToWorldX - camera;
    if (x < -120.0F || x > kWorldWidth + 120.0F) continue;
    const bool goal = index == kLevel3DrumCount - 1;
    const bool pressed =
        game.level3TileTimer > 0 && game.level3PressedDrum == index;
    drawStage3Tambourine(renderer,
                         goal ? assets.stage3GoalTambourine
                              : assets.stage3Tambourine,
                         x, pressed ? 1.0F : 0.0F, goal);
    if (game.level3Water && assets.stage3DrumRing) {
      const float width = (goal ? 180.0F : 48.0F * kSourceToWorldX) + 16.0F;
      const SDL_FRect destination{x - width * 0.5F, kStage3WaterSurfaceY - 8.0F,
                                  width, 24.0F};
      SDL_RenderCopyF(renderer, assets.stage3DrumRing, nullptr, &destination);
    }
  }

  for (const auto& sign : kLevel3Signs) {
    const float x = static_cast<float>(sign.worldColumn) * kSourceToWorldX - camera;
    if (x < -80.0F || x > kWorldWidth + 80.0F) continue;
    drawLevel3Sign(renderer, x, sign.text);
  }

  // Bags: $ED5C blinks four frames on, two off; the popup shows the value
  // for 32 frames; state 6 is the coin pile of the perfect clear.
  for (const auto& bag : game.level3Bags) {
    if (!bag.active || restarting) continue;
    const float x = level3ScreenX(progress, bag.x - 8, camera);
    if (bag.state == 3) {
      if (bag.age % 6 >= 4) continue;
      const float centerY = level3RowToY(static_cast<float>(bag.y) + 8.0F);
      if (assets.rewardBag) {
        const SDL_FRect destination{x - 24.0F, centerY - 26.0F, 48.0F, 52.0F};
        SDL_RenderCopyF(renderer, assets.rewardBag, nullptr, &destination);
      } else {
        ellipse(renderer, x, centerY, 16.0F, 20.0F, color(223, 158, 39), 4);
      }
    } else if (bag.state == 4) {
      const float centerY = level3RowToY(static_cast<float>(bag.y) + 8.0F);
      drawText(renderer, std::to_string(bag.value), x, centerY - 8.0F, 1.25F,
               color(255, 245, 96), true);
    } else if (bag.state == 6) {
      // $F9F5: blank for 96 frames, then three growing stages of 64 frames.
      const int stage = game.level3PileFrame < 96
                            ? 0
                            : std::min(3, 1 + (game.level3PileFrame - 96) / 64);
      const float baseY = level3RowToY(static_cast<float>(bag.y) + 16.0F);
      for (int coin = 0; coin < stage * 3; ++coin) {
        const float offsetX = static_cast<float>((coin % 3) - 1) * 12.0F +
                              static_cast<float>(coin / 3) * 4.0F;
        const float offsetY = -static_cast<float>(coin / 3) * 7.0F;
        drawCoin(renderer, x + offsetX, baseY + offsetY - 6.0F, 1.0F);
      }
    }
  }

  // Performers stand on the grass at row $E0 + 32.
  for (const auto& performer : game.level3Performers) {
    if (!performer.active || restarting) continue;
    const float x = level3ScreenX(progress, performer.x, camera);
    if (x < -80.0F || x > kWorldWidth + 80.0F) continue;
    if (performer.state == 3 || performer.state == 4) {
      // A dolphin; a moved one stays invisible until its first wrap ($EE66).
      if (performer.offset) continue;
      drawStage3Dolphin(renderer, assets, performer, x);
      continue;
    }
    const bool juggler = performer.type != 0;
    SDL_Texture* texture = juggler ? assets.stage3KnifeThrower
                                   : assets.stage3FlameThrower;
    const int frame = level3PerformerFrame(performer);
    // The v4 atlases (tools/import_sprites.py) are 189x268 and 200x309
    // cells drawn at the sheets' old 0.42 / 0.435 units per pixel.
    drawSheetFrame(renderer, texture, 4, 3, frame, x, kStage3GroundY + 3.0F,
                   juggler ? 79.4F : 87.0F, juggler ? 112.6F : 134.4F);
  }

  // Flames: two cells, the lower one at the record row.
  for (const auto& flame : game.level3Flames) {
    if (!flame.active || restarting) continue;
    if (flame.state == 3) {
      drawStage3Splash(renderer, assets, flame,
                       level3ScreenX(progress, flame.x, camera));
      continue;
    }
    const float x = level3ScreenX(progress, flame.x - 8, camera);
    const float centerY = level3RowToY(static_cast<float>(flame.y));
    const int frame = (flame.age / 4) & 3;
    if (assets.stage3FlameProjectile) {
      drawSheetFrame(renderer, assets.stage3FlameProjectile, 4, 1, frame, x,
                     centerY + 46.0F, 92.0F, 112.0F);
    } else {
      ellipse(renderer, x, centerY, 14.0F, 36.0F, color(255, 160, 30), 6);
    }
  }

  // Knives: one cell; hidden for the sixteen held frames ($ED4A).
  for (const auto& knife : game.level3Knives) {
    if (!knife.active || knife.state == 2 || restarting) continue;
    const float x = level3ScreenX(progress, knife.x - 8, camera);
    const float centerY = level3RowToY(static_cast<float>(knife.y) + 8.0F);
    const int frame = (knife.age / 4) & 3;
    if (assets.stage3Projectiles) {
      drawSheetFrame(renderer, assets.stage3Projectiles, 4, 2, frame, x,
                     centerY + 18.0F, 28.0F, 36.0F);
    } else {
      fillRect(renderer, x - 3.0F, centerY - 16.0F, 6.0F, 32.0F,
               color(220, 220, 240));
    }
  }

  if (game.level3Water) {
    // Everything drawn so far that reaches below the surface is under water.
    drawStage3WaterTiles(renderer, assets, camera, timeSeconds, 110);
    drawStage3WaterSurface(renderer, assets, camera, timeSeconds);
  }

  const float playerScreenX =
      game.player.previous.x +
      (game.player.position.x - game.player.previous.x) *
          static_cast<float>(interpolation) - camera;
  const float playerFeetY =
      game.player.previous.y +
      (game.player.position.y - game.player.previous.y) *
          static_cast<float>(interpolation);
  const bool charlieVisible = game.scene != Scene::Crashed;
  // The scoreboard ends at kArenaTop.  The board's own Charlie is 32 rows
  // tall and his head stops just below it on the third rebound, but the HD
  // art is taller and does reach in, so he is drawn after the scoreboard
  // whenever his sprite would otherwise be cut off by it.
  const bool charlieAboveHud = playerFeetY - 110.0F < kArenaTop;
  if (charlieVisible && !charlieAboveHud) {
    drawLevel3Charlie(renderer, game, assets, playerScreenX, playerFeetY);
  }

  // Goal presentation: crowd callouts ($EAFE) blink 16 frames on and off;
  // the bird enters from the left along row $40 and the coins fall from
  // the bag at column $B4.
  if (game.level3State == 4) {
    const bool visible = ((game.level3PoseFrame / 16) & 1) == 0;
    if (visible) {
      const auto outlinedCheer = [&](std::string_view text, float x, float y,
                                     float scale, SDL_Color value) {
        for (const auto& offset : std::array<Vec2, 4>{
                 Vec2{-2.0F, 0.0F}, Vec2{2.0F, 0.0F},
                 Vec2{0.0F, -2.0F}, Vec2{0.0F, 2.0F}}) {
          drawText(renderer, text, x + offset.x, y + offset.y, scale,
                   color(45, 10, 24), true);
        }
        drawText(renderer, text, x, y, scale, value, true);
      };
      outlinedCheer("FAR OUT", 40.0F * kSourceToWorldX, 222.0F, 2.4F,
                    color(87, 219, 255));
      outlinedCheer("FAR OUT", 184.0F * kSourceToWorldX, 222.0F, 2.4F,
                    color(87, 219, 255));
      outlinedCheer("GREAT", 116.0F * kSourceToWorldX, 262.0F, 2.6F,
                    color(255, 93, 36));
    }
  }
  drawLevel3BirdAndCoins(renderer, game, assets);

  drawStage1ScorePopup(renderer, game, camera);
  drawHud(renderer, game, assets.charlieLife);
  // The fourth stationary rebound carries Charlie up through the scoreboard;
  // at the apex only the roof burst remains.
  if (charlieVisible && charlieAboveHud && game.level3Pose != Level3Pose::Roof) {
    drawLevel3Charlie(renderer, game, assets, playerScreenX, playerFeetY);
  }
  if (game.level3Pose == Level3Pose::Roof &&
      (game.level3State == 8 || game.scene == Scene::Crashed)) {
    const int roofFrame = std::min(game.level3PoseFrame / 5, 3);
    drawSheetFrame(renderer, assets.stage3CharlieRoofHead, 4, 1, roofFrame,
                   playerScreenX, 132.0F, 78.0F, 70.0F);
  }
  if (game.level3State == 8 || (game.scene == Scene::Crashed && !restarting)) {
    drawCrowdOhNo(renderer);
  }
}

void drawStage4Scene(SDL_Renderer* renderer, const Game& game,
                     const Assets& assets, double timeSeconds,
                     double interpolation) {
  const float camera = game.previousCameraX +
                       (game.cameraX - game.previousCameraX) *
                           static_cast<float>(interpolation);
  drawBackdrop(renderer, camera, false, assets, game, timeSeconds, false);
  const float scale = kWorldWidth / 224.0F;
  const bool restarting =
      game.scene == Scene::Crashed && game.crashFrame >= 24;

  // The distance plaques ride the tilemap, so their board column is already
  // a screen column.
  // The three records are the three cells of one sign; the middle one
  // names it, so one plaque is drawn centred on that cell.
  {
    const Level4Plaque& middle = game.level4Plaques[1];
    const std::string label = level4PlaqueText(middle.cell);
    const float x = (static_cast<float>(middle.col) + 8.0F) * scale;
    if (!label.empty() && x > -80.0F && x < kWorldWidth + 80.0F) {
      drawLevel3Sign(renderer, x, label);
    }
  }

  // The goal stand sits on the last page, 181 columns past the point where
  // the scroll stops ($9BC0 accepts a landing between columns $A0 and $CA).
  const float goalX =
      (static_cast<float>(kLevel4ScrollEnd + 181) * scale) - camera;
  if (goalX > -130.0F && goalX < kWorldWidth + 130.0F && assets.goalPlatform) {
    const SDL_FRect destination{goalX - 86.0F, level4RowToY(0xb4), 172.0F,
                                48.0F};
    SDL_RenderCopyF(renderer, assets.goalPlatform, nullptr, &destination);
  }
  // The score panel is fixed on every stage; the board's own OH NO cells
  // ($EB23) come from the object slots below, so no separate overlay.
  drawHud(renderer, game, assets.charlieLife);
  if (restarting) return;

  auto drawBall = [&](const Level4Record& record) {
    if (record.active == 0 && record.flag == 0) return;
    const float x = static_cast<float>(record.col) * scale;
    if (x < -100.0F || x > kWorldWidth + 100.0F) return;
    // The board animates the ball with sprite cells; rotating the painted
    // ball by the distance it has covered reads the same at 60 Hz.
    const float world = camera + x;
    drawSheetFrame(renderer, assets.stage4Ball, 1, 1, 0, x,
                   level4RowToY(static_cast<float>(record.row)) +
                       kLevel4BallRadius,
                   kLevel4BallRadius * 2.0F, kLevel4BallRadius * 2.0F,
                   SDL_FLIP_NONE,
                   static_cast<double>(world / kLevel4BallRadius) * 57.2958);
  };
  for (const auto& roll : game.level4Rolls) drawBall(roll);
  drawBall(game.level4Ball);

  // $2600-$26B0: the double-bonus number follows its ball, the callout cells
  // stay where they were placed.
  for (const auto& popup : game.level4Popups) {
    if (popup.active == 0) continue;
    const float x = static_cast<float>(popup.col) * scale;
    const float y = level4RowToY(static_cast<float>(popup.row));
    if (popup.state == 0) {
      if (popup.flag != 0 || popup.value == 0) continue;
      drawText(renderer, std::to_string(popup.value), x - 6.0F, y, 1.6F,
               color(255, 236, 120), false);
      continue;
    }
    // Each callout is three 16x16 cells; the word is drawn once per group.
    if (popup.cell % 3 != 0) continue;
    const int group = popup.cell / 3;
    const char* word = popup.kind == 1
                           ? (group == 1 ? "GREAT" : "FAR OUT")
                           : (group == 1 ? "NO!" : "OH");
    drawText(renderer, word, x - 12.0F, y, 2.0F,
             group == 1 ? color(255, 236, 120) : color(255, 255, 255), false);
  }

  const Level4Record& charlie = game.level4Charlie;
  const float playerX = static_cast<float>(charlie.col) * scale;
  int length = 0;
  const Level4Pose* script = level4Script(charlie.script, length);
  const int frame = script[static_cast<std::size_t>(std::max(charlie.pose, 0))]
                        .frame;
  // The board never mirrors Charlie on this stage: both directions run the
  // same $E9B4 cells and the attribute byte stays zero, so only the ball's
  // spin tells you which way he is rolling.  The painted cell carries a
  // transparent border, and his feet belong four rows into the ball's top
  // cell, where the board's own composite puts them.
  if (assets.stage2CharliePoses && assets.stage2CharlieWalk && assets.stage2CharlieJump) {
    // The painted Charlie of the rope: the board's $E9B4/$E9FF cells map onto
    // the same poses, and the 57-frame jump runs the ten jump frames.  His
    // feet sit on the ball's top cell, row + 16.
    const float feetY = charlie.script == kLevel4ScriptCheer
                            ? level4RowToY(0xb4) + 6.0F
                            : level4RowToY(static_cast<float>(charlie.row + 16));
    const int pose = std::max(charlie.pose, 0);
    constexpr int kStand = 0, kArmsUp = 1, kFallen = 2, kTuck = 3;
    switch (charlie.script) {
      case kLevel4ScriptWalking: {
        // Stepping on top of the ball: the walk cycle, a step per script cell.
        const int walkFrame = (game.level4WalkAge / 3) % 9;
        drawStage2Pose(renderer, assets.stage2CharlieWalk, assets.stage2CharlieWalkCells,
                       walkFrame, playerX, feetY, kStage4CharlieV4UnitsPerPixel * game.charlieScale);
        return;
      }
      case kLevel4ScriptJumping: {
        const int jumpFrame = std::min(9, game.level4JumpAge * 10 / 57);
        drawStage2Pose(renderer, assets.stage2CharlieJump, assets.stage2CharlieJumpCells,
                       jumpFrame, playerX, feetY, kStage4CharlieV4UnitsPerPixel * game.charlieScale);
        return;
      }
      case kLevel4ScriptGoalJump:       // $E9D8: the flip onto the stand, then arms up
        drawStage2Pose(renderer, assets.stage2CharliePoses, assets.stage2CharliePosesCells,
                       pose >= 5 ? kArmsUp : kTuck, playerX, feetY,
                       kStage4CharlieV4UnitsPerPixel * game.charlieScale);
        return;
      case kLevel4ScriptFallen:
        drawStage2Pose(renderer, assets.stage2CharliePoses, assets.stage2CharliePosesCells,
                       kFallen, playerX, feetY, kStage4CharlieV4UnitsPerPixel * game.charlieScale);
        return;
      case kLevel4ScriptCheer:          // $EAAA: arms up / stand every sixteen frames
        drawStage2Pose(renderer, assets.stage2CharliePoses, assets.stage2CharliePosesCells,
                       (pose & 1) == 0 ? kArmsUp : kStand, playerX, feetY,
                       kStage4CharlieV4UnitsPerPixel * game.charlieScale);
        return;
      default: {
        // Standing ($E9FF, cells 0/4/1/0) and the struck wobble ($EA1A,
        // 0/4/1): upright, lean back, lean forward.
        const int cell = frame;
        const double tilt = cell == 4 ? -10.0 : cell == 1 ? 10.0 : 0.0;
        drawStage2PoseTilted(renderer, assets.stage2CharliePoses,
                             assets.stage2CharliePosesCells, kStand, playerX, feetY,
                             kStage4CharlieV4UnitsPerPixel * game.charlieScale, tilt);
        return;
      }
    }
  }
  if (charlie.script == kLevel4ScriptCheer && assets.stage6CharlieFloor) {
    // On the goal stand: the $EAAA cheer alternates two cells every sixteen
    // frames; his shoes stand on the platform's top.
    const float feetY = level4RowToY(0xb4) + 6.0F;
    const bool armsUp = (std::max(charlie.pose, 0) & 1) == 0;
    // The stage-4 cells draw him 133 units tall on the ball; the standing
    // atlases are about 230 px for the same figure.
    const float unit = 96.0F / 230.0F;
    if (armsUp) {
      drawStage2Pose(renderer, assets.stage6CharlieFloor, assets.stage6CharlieFloorCells,
                     2, playerX, feetY, unit);
    } else {
      drawStage2Pose(renderer, assets.stage5CharlieFall, assets.stage5CharlieFallCells,
                     0, playerX, feetY, unit);
    }
    return;
  }
  drawSheetFrame(renderer, assets.stage4Charlie, 4, 3, frame, playerX,
                 level4RowToY(static_cast<float>(charlie.row + 16)) + 18.0F,
                 133.0F, 133.0F, SDL_FLIP_NONE, 0.0);
}

#include "level5_draw.inc"
#include "level6_draw.inc"

void drawTallyScreen(SDL_Renderer* renderer, const Game& game,
                     bool complete, const Assets& assets,
                     double timeSeconds) {
  fillRect(renderer, 0.0F, 0.0F, kWorldWidth, kWorldHeight,
           color(0, 0, 0));
  if (assets.marquee) {
    const SDL_FRect marqueeDestination{
        0.0F, 0.0F, static_cast<float>(kWorldWidth), kMarqueeHeight};
    SDL_RenderCopyF(renderer, assets.marquee, nullptr, &marqueeDestination);
    drawReddishMarqueeSky(renderer);
  }
  drawFerrisWheel(renderer, assets.ferrisWheel, assets.ferrisGondola,
                  timeSeconds);
  drawZeppelinBonus(renderer, game.bonus);
  drawHud(renderer, game, assets.charlieLife);
  drawText(renderer, "FINE!!", kWorldWidth * 0.5F, 218.0F, 3.5F,
           color(81, 222, 255), true);
  const int displayedBonus = complete
      ? game.bonus
      : std::min(game.bonus, std::max(0, game.tallyFrame) * 50);
  std::ostringstream bonusText;
  bonusText << "YOUR BONUS IS " << std::setw(4) << std::setfill('0')
            << displayedBonus;
  drawText(renderer, bonusText.str(),
           kWorldWidth * 0.5F, 260.0F, 1.7F, color(255, 255, 255), true);

  constexpr std::array<std::string_view, 10> ranges{
      "5800-4500", "4499-4000", "3999-3500", "3499-3000",
      "2999-2500", "2499-2000", "1999-1500", "1499-1000",
      "999-500",   "499-0",
  };
  constexpr std::array<int, 10> awards{
      10000, 5000, 4000, 3000, 2000, 1000, 800, 600, 400, 200,
  };
  const bool showAwardTable =
      complete || displayedBonus >= game.bonus || game.timeScoreApplied;
  for (size_t index = 0; showAwardTable && index < ranges.size(); ++index) {
    const float y = 306.0F + static_cast<float>(index) * 23.0F;
    const int tierAward = awards[index];
    const bool selected =
        timeBonusFor(game.bonus) == tierAward;
    const SDL_Color value =
        selected ? color(255, 224, 58) : color(255, 255, 255);
    drawText(renderer, ranges[index], 84.0F, y, 1.2F, value);
    drawText(renderer, std::to_string(tierAward), 385.0F, y, 1.2F, value,
             true);
  }

  if (complete) {
    drawText(renderer,
             "EVENT " + std::to_string(game.selectedEvent + 1) +
                 " COMPLETE",
             kWorldWidth * 0.5F, 552.0F,
             2.0F, color(92, 235, 139), true);
    drawText(renderer,
             game.credits > 0 ? "PRESS START BUTTON" : "INSERT COIN",
             kWorldWidth * 0.5F, 588.0F, 1.8F,
             color(255, 255, 255), true);
  } else {
    const std::string status =
        game.timeScoreApplied
            ? "BONUS SCORE " + std::to_string(game.clearBonus)
            : "CHECKING TIME";
    drawText(renderer, status, kWorldWidth * 0.5F, 560.0F, 1.6F,
             color(92, 235, 139), true);
  }
}

void drawDebug(SDL_Renderer* renderer, const Game& game,
               const RenderSurface& surface) {
  fillRect(renderer, 8.0F, kArenaTop + 6.0F, 246.0F, 77.0F,
           color(0, 0, 0, 200));
  drawText(renderer, "FIXED 60.606 HZ", 15.0F, kArenaTop + 13.0F, 1.4F,
           color(93, 224, 255));
  drawText(renderer, "SPEED " + std::to_string(static_cast<int>(
                                   std::lround(game.player.runSpeed))),
           15.0F, kArenaTop + 30.0F, 1.4F, color(255, 255, 255));
  drawText(renderer, "JUMP " + std::to_string(static_cast<int>(
                                   std::lround((game.selectedEvent == 1
                                                   ? kStage2RopeY
                                                   : (game.selectedEvent == 2
                                                          ? kStage3TambourineTopY
                                                          : (game.selectedEvent == 3
                                                                 ? kStage4CharlieBaselineY
                                                                 : kGroundY))) -
                                              game.player.position.y))),
           15.0F, kArenaTop + 47.0F, 1.4F, color(255, 255, 255));
  drawText(renderer,
           "RAIL " + std::to_string(static_cast<int>(
                         std::lround(kRingRailSpeed))) +
               " RENDER " + std::to_string(surface.width) + "X" +
               std::to_string(surface.height),
           15.0F, kArenaTop + 64.0F, 1.05F, color(255, 255, 255));
}

void drawBootScreen(SDL_Renderer* renderer, const Game& game,
                    const Assets& assets) {
  fillRect(renderer, 0.0F, 0.0F, kWorldWidth, kWorldHeight,
           color(0, 0, 0));
  const int frameIndex = std::clamp(
      static_cast<int>(static_cast<float>(game.bootFrame) *
                       kBootFramesPerSecond /
                       static_cast<float>(kBoardRefresh)),
      0, gBootFrameCount - 1);
  if (assets.bootFrameTextureIndex != frameIndex) {
    if (assets.bootFrameTexture) {
      SDL_DestroyTexture(assets.bootFrameTexture);
      assets.bootFrameTexture = nullptr;
    }
    assets.bootFrameTexture = loadBootFrame(renderer, frameIndex);
    assets.bootFrameTextureIndex = frameIndex;
  }
  if (assets.bootFrameTexture) {
    const SDL_FRect destination{0.0F, 0.0F,
                                static_cast<float>(kWorldWidth),
                                static_cast<float>(kWorldHeight)};
    SDL_RenderCopyF(renderer, assets.bootFrameTexture, nullptr,
                    &destination);
  }
}

// One clip frame at a time, decoded as it comes up: the frames are the
// playfield itself, so they fill the canvas.
void drawAttractFrame(SDL_Renderer* renderer, const Game& game, const Assets& assets) {
  const int fps = game.attractBetween ? kBetweenFps : kAttractFps;
  const int frame = std::min(game.attractFrameCount - 1,
                             game.attractTick * fps / static_cast<int>(kBoardRefresh));
  const int clipKey = game.attractBetween ? -2 : game.attractClip;
  if (assets.attractFrameClip != clipKey || assets.attractFrameIndex != frame) {
    if (assets.attractFrame) SDL_DestroyTexture(assets.attractFrame);
    const std::string path = game.attractBetween ? betweenFramePath(frame)
                                                 : attractFramePath(game.attractClip, frame);
    assets.attractFrame = loadAsset(renderer, path.c_str());
    if (assets.attractFrame && game.attractBetween) {
      SDL_SetTextureScaleMode(assets.attractFrame, SDL_ScaleModeNearest);
    }
    assets.attractFrameClip = clipKey;
    assets.attractFrameIndex = frame;
  }
  fillRect(renderer, 0.0F, 0.0F, kWorldWidth, kWorldHeight, color(0, 0, 0));
  if (assets.attractFrame) {
    // A soft cut: each segment fades in from black and out to black over
    // its first and last kAttractFadeFrames board frames.
    const int total = game.attractFrameCount * static_cast<int>(kBoardRefresh) / fps;
    const float in = static_cast<float>(game.attractTick) / kAttractFadeFrames;
    const float out = static_cast<float>(total - game.attractTick) / kAttractFadeFrames;
    const float alpha = std::clamp(std::min(in, out), 0.0F, 1.0F);
    SDL_SetTextureAlphaMod(assets.attractFrame, static_cast<Uint8>(alpha * 255.0F));
    const SDL_FRect destination{0.0F, 0.0F, kWorldWidth, kWorldHeight};
    SDL_RenderCopyF(renderer, assets.attractFrame, nullptr, &destination);
    SDL_SetTextureAlphaMod(assets.attractFrame, 255);
  }
}

void renderScene(SDL_Renderer* renderer, const Game& game,
                 const RenderSurface& surface, const Assets& assets,
                 double timeSeconds, double interpolation) {
  const bool lowDetail = surface.height <= 320;
  if (game.scene == Scene::Boot) {
    drawBootScreen(renderer, game, assets);
    return;
  }
  if (game.scene == Scene::Title) {
    drawCoinWaitingScreen(renderer, game, assets, timeSeconds);
    if (game.debug) drawDebug(renderer, game, surface);
    return;
  }
  if (game.scene == Scene::Attract) {
    drawAttractFrame(renderer, game, assets);
    return;
  }
  if (game.scene == Scene::GameOver) {
    drawGameOverScreen(renderer, game, assets, timeSeconds);
    return;
  }
  if (game.scene == Scene::EventSelect) {
    drawEventSelectionScreen(renderer, game, assets, timeSeconds);
    if (game.debug) drawDebug(renderer, game, surface);
    return;
  }
  if (game.scene == Scene::Tally || game.scene == Scene::Complete) {
    drawTallyScreen(renderer, game, game.scene == Scene::Complete, assets,
                    timeSeconds);
    if (game.debug) drawDebug(renderer, game, surface);
    return;
  }
  if (game.selectedEvent == 1) {
    drawStage2Scene(renderer, game, assets, timeSeconds, interpolation);
    if (game.debug) drawDebug(renderer, game, surface);
    return;
  }
  if (game.selectedEvent == 2) {
    drawStage3Scene(renderer, game, assets, timeSeconds, interpolation);
    if (game.debug) drawDebug(renderer, game, surface);
    return;
  }
  if (game.selectedEvent == 3) {
    drawStage4Scene(renderer, game, assets, timeSeconds, interpolation);
    if (game.debug) drawDebug(renderer, game, surface);
    return;
  }
  if (game.selectedEvent == 4) {
    drawStage5Scene(renderer, game, assets, timeSeconds, interpolation);
    if (game.debug) drawDebug(renderer, game, surface);
    return;
  }
  if (game.selectedEvent == 5) {
    drawStage6Scene(renderer, game, assets, timeSeconds, interpolation);
    if (game.debug) drawDebug(renderer, game, surface);
    return;
  }
  const float camera =
      game.previousCameraX +
      (game.cameraX - game.previousCameraX) * static_cast<float>(interpolation);
  const bool flareFrame =
      (static_cast<int>(timeSeconds * kBoardRefresh) / 6 & 1) != 0;
  SDL_Texture* hoopFrame =
      flareFrame && assets.hoopFlare ? assets.hoopFlare : assets.hoop;
  SDL_Texture* propsFrame =
      flareFrame && assets.propsFlare ? assets.propsFlare : assets.props;
  drawBackdrop(renderer, camera, lowDetail, assets, game, timeSeconds);
  drawCourseMarkers(renderer, game, camera, assets.goalPlatform);

  for (const auto& hoop : game.hoops) {
    if (!hoop.active || hoop.kind != Level1HoopKind::Large) continue;
    drawHoop(renderer, hoop, camera, lowDetail, hoopFrame);
  }
  drawStageProps(renderer, game, camera, propsFrame, assets.rewardBag);
  drawExtraCharlie(renderer, game, camera, timeSeconds,
                   assets.extraCharlie);

  if (game.scene != Scene::Title) {
    const float playerWorldX =
        game.player.previous.x +
        (game.player.position.x - game.player.previous.x) *
            static_cast<float>(interpolation);
    const float playerY =
        game.player.previous.y +
        (game.player.position.y - game.player.previous.y) *
            static_cast<float>(interpolation);
    if (game.scene == Scene::Goal && assets.finishRider) {
      drawFinishRider(renderer, assets.finishRider, playerWorldX - camera,
                      game.goalFrame);
    } else if (!(game.scene == Scene::Crashed &&
                 game.crashFrame < kCrashBurnFrames && assets.burnRider)) {
      drawLionAndRider(renderer, playerWorldX - camera, playerY, timeSeconds,
                       game.player.alive, lowDetail, assets.riderRunA,
                       assets.riderRunB, assets.riderRunC, assets.riderBackE,
                       assets.riderBackF, game.lionOnlyTest,
                       game.player.runSpeed, game.player.grounded,
                       game.player.facingRight, game.level1RiderState);
    }
    for (const auto& hoop : game.hoops) {
      if (!hoop.active || hoop.kind != Level1HoopKind::Large) continue;
      drawHoopForeground(renderer, hoop, camera, hoopFrame);
    }
    drawBonusRingForegrounds(renderer, game, camera, propsFrame);
    if (game.scene == Scene::Goal) {
      drawGoalPlatformFrontRim(renderer, playerWorldX - camera);
    }
    if (game.scene == Scene::Crashed &&
        game.crashFrame < kCrashBurnFrames) {
      drawBurningRider(renderer, playerWorldX - camera, playerY,
                       assets.burnRider, game.crashFrame);
    }
    if (game.scene == Scene::Goal) {
      drawGoalPresentation(renderer, game, assets.bird, assets.rewardBag,
                           assets.props);
    }
  }
  drawStage1ScorePopup(renderer, game, camera);
  drawHud(renderer, game, assets.charlieLife);

  if (game.scene == Scene::Crashed) drawCrowdOhNo(renderer);

  if (game.debug) drawDebug(renderer, game, surface);
}

void setFullscreen(SDL_Window* window, bool enabled) {
  SDL_SetWindowFullscreen(window, enabled ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}

bool captureRenderer(SDL_Renderer* renderer, const std::string& path) {
  int width = 0;
  int height = 0;
  SDL_GetRendererOutputSize(renderer, &width, &height);
  SDL_Surface* screenshot =
      SDL_CreateRGBSurfaceWithFormat(0, width, height, 32,
                                     SDL_PIXELFORMAT_RGBA32);
  if (!screenshot) return false;
  const bool readSucceeded =
      SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_RGBA32,
                           screenshot->pixels, screenshot->pitch) == 0;
  const bool saveSucceeded =
      readSucceeded && IMG_SavePNG(screenshot, path.c_str()) == 0;
  SDL_FreeSurface(screenshot);
  return saveSucceeded;
}

}  // namespace

int main(int argc, char** argv) {
  const auto parsedOptions = parseOptions(argc, argv);
  if (!parsedOptions) return argc > 1 ? 1 : 0;
  const Options options = *parsedOptions;
  if (options.showHelp) return 0;
  gRenderWidth = options.renderWidth;
  gRenderHeight = options.renderHeight;

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO) != 0) {
    std::cerr << "SDL initialization failed: " << SDL_GetError() << '\n';
    return 1;
  }
  if ((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) == 0) {
    std::cerr << "PNG support initialization failed: " << IMG_GetError()
              << '\n';
  }

  const Uint32 windowFlags =
      SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE |
      (options.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
  SDL_Window* window = SDL_CreateWindow(
      "Circus Charlie HD", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
      options.width, options.height, windowFlags);
  if (!window) {
    std::cerr << "Window creation failed: " << SDL_GetError() << '\n';
    IMG_Quit();
    SDL_Quit();
    return 1;
  }

  SDL_Renderer* renderer = SDL_CreateRenderer(
      window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC |
                      SDL_RENDERER_TARGETTEXTURE);
  if (!renderer) {
    renderer =
        SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE |
                                           SDL_RENDERER_TARGETTEXTURE);
  }
  if (!renderer) {
    std::cerr << "Renderer creation failed: " << SDL_GetError() << '\n';
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();
    return 1;
  }
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  Assets assets = loadAssets(renderer);
  AudioEngine audio;
  loadAudio(audio);

  SDL_GameController* controller = nullptr;
  for (int index = 0; index < SDL_NumJoysticks(); ++index) {
    if (SDL_IsGameController(index)) {
      controller = SDL_GameControllerOpen(index);
      if (controller) break;
    }
  }

  Game game;
  const std::string highScorePath = highScoreMemoryPath();
  game.highScore = loadHighScore(highScorePath);
  gBootFrameCount = std::max(1, countFrames([](int frame) {
    std::array<char, 64> name{};
    std::snprintf(name.data(), name.size(), "boot/boot-%04d.png", frame);
    return std::string(name.data());
  }));
  gBetweenFrameCount = countFrames(betweenFramePath);
  const std::string progressPath = progressMemoryPath(highScorePath);
  const std::string charlieScaleFile = charlieScalePath(highScorePath);
  if (options.replayPath.empty()) loadCharlieScale(charlieScaleFile, game);
  // A test run (--event, --replay, --record) starts every event fresh.
  if (options.startEvent == 0 && options.replayPath.empty() && options.recordDir.empty()) {
    loadProgress(progressPath, game);
  }
  game.eventSelectDurationFrames =
      audioDurationInBoardFrames(audio.eventSelectMusic);
  game.crashDurationFrames = audioDurationInBoardFrames(audio.fail);
  if (audio.fail.data && options.replayPath.empty()) {
    game.failPieceFrames = audioDurationInBoardFrames(audio.fail);
  }
  if (audio.birdCoinDrop.data && options.replayPath.empty()) {
    game.level1ShowerHoldFrames =
        audioDurationInBoardFrames(audio.birdCoinDrop);
  }
  game.randomState ^=
      static_cast<std::uint32_t>(SDL_GetPerformanceCounter());
  game.debug = options.debug;
  game.lionOnlyTest = options.lionTest;
  resetCourse(game);
  if (!options.tracePath.empty()) {
    game.selectedEvent = 0;
    startGame(game);
  }
  // ---- MAME replay --------------------------------------------------------
  struct ReplayRow {
    int frame = 0;
    int input = 0xff;
    int playerState = 0;
    int rel = -1;
    int frameByte = 0;
    int bonus = -1;
  };
  std::vector<ReplayRow> replayRows;
  std::size_t replayIndex = 0;
  std::ofstream replayOutput;
  if (!options.replayPath.empty() &&
      (options.replayEvent >= 1 && options.replayEvent <= 5)) {
    // Level 3 capture: one row per emulated frame, "rel" counts from the
    // frame that runs $8C61 (initialisation plus the first rebound tick),
    // which is also the first native update.
    std::ifstream replayFile(options.replayPath);
    std::string line;
    int inputColumn = -1;
    int relColumn = -1;
    int frameColumn = -1;
    int frameByteColumn = -1;
    int bonusColumn = -1;
    while (std::getline(replayFile, line)) {
      std::vector<std::string> cells;
      std::string cell;
      std::stringstream lineStream(line);
      while (std::getline(lineStream, cell, ',')) cells.push_back(cell);
      if (inputColumn < 0) {
        for (std::size_t column = 0; column < cells.size(); ++column) {
          if (cells[column] == "input") inputColumn = static_cast<int>(column);
          if (cells[column] == "rel") relColumn = static_cast<int>(column);
          if (cells[column] == "frame") frameColumn = static_cast<int>(column);
          if (cells[column] == "frame_byte") frameByteColumn = static_cast<int>(column);
          if (cells[column] == "bonus") bonusColumn = static_cast<int>(column);
        }
        if (inputColumn < 0 || relColumn < 0 || frameColumn < 0) {
          std::cerr << "Replay file lacks frame/rel/input columns.\n";
          return 1;
        }
        continue;
      }
      if (cells.empty() || cells[0].empty() || cells[0][0] == '#') continue;
      if (static_cast<int>(cells.size()) <= std::max(inputColumn, relColumn)) continue;
      ReplayRow row;
      row.frame = std::stoi(cells[static_cast<std::size_t>(frameColumn)]);
      row.input = std::stoi(cells[static_cast<std::size_t>(inputColumn)], nullptr, 16);
      row.rel = std::stoi(cells[static_cast<std::size_t>(relColumn)]);
      if (frameByteColumn >= 0) {
        row.frameByte = std::stoi(cells[static_cast<std::size_t>(frameByteColumn)]);
      }
      if (bonusColumn >= 0 && static_cast<int>(cells.size()) > bonusColumn) {
        row.bonus = std::stoi(cells[static_cast<std::size_t>(bonusColumn)]);
      }
      replayRows.push_back(row);
    }
    bool found = false;
    // Event 5's init frame ($9FF9) does not tick; the first native update is
    // the capture's rel 1.
    const int firstRel = ((options.replayEvent == 4 || options.replayEvent == 5) &&
                          options.replayOffset == 0)
                             ? 1 : options.replayOffset;
    for (std::size_t row = 0; row < replayRows.size(); ++row) {
      if (replayRows[row].rel == firstRel) {
        replayIndex = row;
        found = true;
        break;
      }
    }
    if (!found) {
      std::cerr << "Replay file has no rel " << options.replayOffset << " row.\n";
      return 1;
    }
    game.selectedEvent = options.replayEvent;
    // startEvent derives the visit count from the plays so far ($A8FB spawns
    // at set-up from it), so the capture's count is seeded through them.
    game.playCount[static_cast<std::size_t>(options.replayEvent)] =
        std::max(0, options.replayVisits - 1);
    if (options.replayWater && options.replayEvent == 2) game.playCount[2] = 1;
    game.level6.visits = options.replayVisits;
    startGame(game);
    // <$14 advances before the first update; the capture logs it after.
    game.level1BoardFrameByte = static_cast<std::uint8_t>(
        options.replayFrameByte >= 0 ? options.replayFrameByte
                                     : replayRows[replayIndex].frameByte - 1);
    game.level3Invulnerable = options.replayInvulnerable;
    game.level2Perfect = options.replayPerfect;
    game.level5.visits = options.replayVisits;
    game.level6.visits = options.replayVisits;
    game.level3ClearProjectiles = options.replayClearProjectiles;
    game.level3Water = options.replayWater;
    if (options.replayWater) game.level3Visits = 2;
    if (!options.replayOutput.empty() && options.replayEvent == 1) {
      replayOutput.open(options.replayOutput);
      replayOutput << "frame,rel,input,state,y,x,half,phase,landvel,launch,dir,"
                      "vel,jump,stick,idle_hi,idle_lo,countdown,code,scroll,"
                      "halfstep,score,bonus,lives,sched,schedptr,buffered,stand,"
                      "goalhit,jumps,held,skip,browns,purples,flash,timeout,"
                      "missed,leftbonus,perfect,visits";
      for (int index = 0; index < 7; ++index) {
        for (const char* name : {"active", "flag", "state", "y", "x", "half",
                                 "phase", "landvel", "vel", "timer", "kind",
                                 "owner", "code"}) {
          replayOutput << ",mk" << index << '_' << name;
        }
      }
      replayOutput << ",stand_active,stand_state,stand_y,stand_x,stand_x2,"
                      "stand_x3";
      for (int index = 0; index < 12; ++index) {
        for (const char* name : {"state", "flag", "y", "x", "timer"}) {
          replayOutput << ",fx" << index << '_' << name;
        }
      }
      replayOutput << ",plaque0_x,plaque1_x,plaque2_x,bird_state,bird_x,bird_bagx";
      for (int index = 0; index < 13; ++index) {
        replayOutput << ",coin" << index << "_y,coin" << index << "_x";
      }
      replayOutput << ",coinidx,coincount,phase05,phase06,frame_byte,dips\n";
    } else if (!options.replayOutput.empty() && options.replayEvent == 5) {
      replayOutput.open(options.replayOutput);
      replayOutput << "frame,rel,input,phase,stage,scroll,page,low,spawncount,perfect,"
                      "v220b,lives,score,bonus";
      for (int a = 0xb0; a <= 0xff; ++a) {
        replayOutput << ",v" << std::hex << a << std::dec;
      }
      replayOutput << ",ch_row,ch_col,ch_code,ch2_row,ch2_col,ch2_code,"
                      "tr0_row,tr0_col,tr0_code,tr1_row,tr1_col,tr1_code,"
                      "goal_row,goal_col,goal_code,pl_row,pl_col,pl_code";
      for (int i = 0; i < 5; ++i) {
        for (int b = 0; b < 16; ++b) {
          replayOutput << ",t" << i << '_' << std::hex << b << std::dec;
        }
      }
      for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 16; ++c) {
          replayOutput << ",r" << r << '_' << std::hex << c << std::dec;
        }
      }
      replayOutput << ",v287d,v287e,v287f,v2213,phase05,phase06,frame_byte,dips\n";
    } else if (!options.replayOutput.empty() && options.replayEvent == 4) {
      replayOutput.open(options.replayOutput);
      replayOutput << "frame,rel,input,phase,stage,scroll,page,low,spawncount,perfect,"
                      "v220b,lives,score,bonus";
      for (int a = 0xb0; a <= 0xc2; ++a) {
        replayOutput << ",v" << std::hex << a << std::dec;
      }
      replayOutput << ",ch_row,ch_col,ch_code,pony_row,pony_col,pony_code,"
                      "pl0_row,pl0_col,pl0_code,pl1_row,pl1_col,pl1_code,"
                      "pl2_row,pl2_col,pl2_code";
      for (int i = 0; i < 8; ++i) {
        for (int b = 0; b < 15; ++b) {
          replayOutput << ",h" << i << '_' << std::hex << b << std::dec;
        }
      }
      replayOutput << ",v287d,v287e,v287f,v2212,phase05,phase06,frame_byte,dips\n";
    } else if (!options.replayOutput.empty() && options.replayEvent == 3) {
      replayOutput.open(options.replayOutput);
      replayOutput << "frame,rel,input,state,y,x,phase,landvel,launch,dir,vel,jump,"
                      "stick,idle_hi,idle_lo,countdown,ball_active,ball_flag,"
                      "ball_state,ball_y,ball_x,ball_half,ball_align,scroll,score,"
                      "bonus,lives,buffered,doubles,pending,falls,landings,held,"
                      "standing,flash,timeout,visits";
      for (int index = 0; index < 4; ++index) {
        for (const char* name : {"active", "flag", "state", "y", "x", "half"}) {
          replayOutput << ",roll" << index << '_' << name;
        }
      }
      for (int index = 0; index < 12; ++index) {
        for (const char* name : {"state", "flag", "y", "x", "timer"}) {
          replayOutput << ",fx" << index << '_' << name;
        }
      }
      replayOutput << ",plaque0_x,plaque1_x,plaque2_x,phase05,phase06,frame_byte,"
                      "dips\n";
    } else if (!options.replayOutput.empty()) {
      replayOutput.open(options.replayOutput);
      replayOutput << "frame,rel,input,state,y,x,phase,vel,target,bnc,dir,stick,scroll,"
                      "score,missed,bagidx,bagtot,tile";
      for (int index = 0; index < 4; ++index) {
        for (const char* name : {"active", "x", "timer", "rem", "type"}) {
          replayOutput << ",perf" << index << '_' << name;
        }
      }
      for (int index = 0; index < 3; ++index) {
        for (const char* name : {"state", "y", "x", "vel", "apex", "hold"}) {
          replayOutput << ",flame" << index << '_' << name;
        }
      }
      for (int index = 0; index < 4; ++index) {
        for (const char* name : {"state", "y", "x", "vel", "apex", "sway", "hold"}) {
          replayOutput << ",knife" << index << '_' << name;
        }
      }
      for (int index = 0; index < 3; ++index) {
        for (const char* name : {"state", "x", "key", "timer"}) {
          replayOutput << ",bag" << index << '_' << name;
        }
      }
      if (options.replayWater) {
        for (int index = 0; index < 4; ++index) {
          for (const char* name : {"st", "y", "x", "b7", "v17", "v18", "v23", "v25",
                                   "v38", "v39", "v3b", "v3c", "spr"}) {
            replayOutput << ",dol" << index << '_' << name;
          }
        }
      }
      replayOutput << ",bird_state,bird_x,bird_bagx";
      for (int index = 0; index < 13; ++index) {
        replayOutput << ",coin" << index << "_y,coin" << index << "_x";
      }
      replayOutput << ",coinidx,coincount,bonus,lives,phase05,phase06,frame_byte\n";
    }
    if (!options.replayCaptureDir.empty()) {
      std::error_code error;
      std::filesystem::create_directories(options.replayCaptureDir, error);
    }
  } else if (!options.replayPath.empty()) {
    std::ifstream replayFile(options.replayPath);
    std::string line;
    int inputColumn = -1;
    int stateColumn = -1;
    int frameColumn = -1;
    while (std::getline(replayFile, line)) {
      std::vector<std::string> cells;
      std::string cell;
      std::stringstream lineStream(line);
      while (std::getline(lineStream, cell, ',')) cells.push_back(cell);
      if (inputColumn < 0) {
        for (std::size_t column = 0; column < cells.size(); ++column) {
          if (cells[column] == "player_input") inputColumn = static_cast<int>(column);
          if (cells[column] == "player_state_2800") stateColumn = static_cast<int>(column);
          if (cells[column] == "frame") frameColumn = static_cast<int>(column);
        }
        if (inputColumn < 0 || stateColumn < 0 || frameColumn < 0) {
          std::cerr << "Replay file lacks frame/player_input/player_state_2800 columns.\n";
          return 1;
        }
        continue;
      }
      if (static_cast<int>(cells.size()) <= std::max(inputColumn, stateColumn)) continue;
      ReplayRow row;
      row.frame = std::stoi(cells[static_cast<std::size_t>(frameColumn)]);
      row.input = std::stoi(cells[static_cast<std::size_t>(inputColumn)], nullptr, 16);
      row.playerState = std::stoi(cells[static_cast<std::size_t>(stateColumn)], nullptr, 16);
      replayRows.push_back(row);
    }
    int startFrame = options.replayStart;
    if (startFrame < 0) {
      for (std::size_t row = 1; row < replayRows.size(); ++row) {
        if (replayRows[row].playerState == 1 && replayRows[row - 1].playerState != 1) {
          // The frame that turns $2800 to one runs the board initialisation;
          // the first movement tick is the following frame.
          startFrame = replayRows[row].frame + 1;
          break;
        }
      }
    }
    if (startFrame < 0) {
      std::cerr << "Replay file has no player-state transition.\n";
      return 1;
    }
    for (std::size_t row = 0; row < replayRows.size(); ++row) {
      if (replayRows[row].frame == startFrame + options.replayOffset) {
        replayIndex = row;
        break;
      }
    }
    // $7363 compares the port with its previous-frame copy, so a button held
    // across the course start is not an edge.
    if (replayIndex > 0) {
      game.replayInitialInput = replayRows[replayIndex - 1].input;
    }
    if (options.replayFrameByte >= 0) {
      game.level1BoardFrameByte = static_cast<std::uint8_t>(options.replayFrameByte);
    }
    game.replayCourseOffsetOverride = options.replayCourseOffset;
    game.replayCoinSelectorOverride = options.replayCoinSelector;
    game.selectedEvent = 0;
    startGame(game);
    if (!options.replayOutput.empty()) {
      replayOutput.open(options.replayOutput);
      replayOutput << "native_frame,mame_frame,input,left,right,jump,progress_fixed,"
                      "page,offset_byte,progress_px,airborne,jump_frame,rider_row,"
                      "score,bonus,lives,scene,course_index,course_state,"
                      "course_offset,activation,retire_distance,"
                      "hoop0,hoop0_x,hoop1,hoop1_x,hoop2,hoop2_x,hoop3,hoop3_x,"
                      "pot0,pot0_x,pot0_cd,pot1,pot1_x,pot1_cd,pot2,pot2_x,pot2_cd,"
                      "pot_counter,coin_pot,coin_state,coin_x,coin_row,"
                      "extra_state,bag_state,rider_state,goal_counter\n";
    }
    if (!options.replayCaptureDir.empty()) {
      std::error_code error;
      std::filesystem::create_directories(options.replayCaptureDir, error);
    }
  }
  const bool replaying = !options.replayPath.empty();
  int replayFrame = 0;
  int recordBoardFrame = 0;
  int recordedFrames = 0;
  bool recordSawEnd = false;
  bool recordFinish = false;
  int replayPreviousInput = game.replayInitialInput;
  if (!options.capturePath.empty()) {
    if (options.captureScene == "start") {
      game.credits = 1;
      game.scene = Scene::Title;
    } else if (options.captureScene == "select") {
      game.credits = 1;
      game.scene = Scene::EventSelect;
      game.selectedEvent = 0;
      game.eventSelectFrame = 0;
    } else {
      if (options.captureScene == "stage2" ||
          options.captureScene == "stage2-goal") {
        game.selectedEvent = 1;
      } else if (options.captureScene == "stage3" ||
                 options.captureScene == "stage3-transfer" ||
                 options.captureScene == "stage3-approach" ||
                 options.captureScene == "stage3-goal" ||
                 options.captureScene == "stage3-roof" ||
                 options.captureScene == "stage3-water" ||
                 options.captureScene == "stage3-water-2") {
        game.selectedEvent = 2;
        if (options.captureScene == "stage3-water" ||
            options.captureScene == "stage3-water-2") {
          game.level3Water = true;
          game.level3Visits = 2;
        }
      } else if (options.captureScene == "stage4" ||
                 options.captureScene == "stage4-jump" ||
                 options.captureScene == "stage4-fall" ||
                 options.captureScene == "stage4-goal") {
        game.selectedEvent = 3;
      }
      startGame(game);
    }
    if (options.captureScene == "stage2") {
      // Play the rope with the joystick held right, jumping over each monkey,
      // so the capture shows a real board frame from the ROM model.
      std::array<Uint8, SDL_NUM_SCANCODES> scripted{};
      scripted[SDL_SCANCODE_RIGHT] = 1;
      for (int frame = 0; frame < 360; ++frame) {
        bool jump = false;
        if (game.level2Charlie.phase == 0 && game.level2Charlie.state == 1) {
          for (const auto& m : game.level2Monkeys) {
            if (m.active == 0) continue;
            const int gap = (m.col - game.level2Charlie.col) & 0xff;
            if (gap < 0x2c && gap >= 0x1c) jump = true;
          }
        }
        ++game.level1BoardFrameByte;
        updateLevel2(game, scripted.data(), jump, 0.0F);
      }
    }
    if (options.captureScene == "stage4" ||
        options.captureScene == "stage4-jump" ||
        options.captureScene == "stage4-fall" ||
        options.captureScene == "stage4-goal") {
      // Run the board model with the joystick held right, jumping onto the
      // rolling balls, so the capture shows a real playing frame.
      std::array<Uint8, SDL_NUM_SCANCODES> scripted{};
      scripted[SDL_SCANCODE_RIGHT] = 1;
      const int frames = options.captureScene == "stage4-goal" ? 1888
                         : options.captureScene == "stage4-fall" ? 130
                         : options.captureScene == "stage4-jump" ? 90
                                                                 : 60;
      for (int frame = 0; frame < frames; ++frame) {
        bool jump = false;
        if (options.captureScene != "stage4-fall" &&
            game.level4Charlie.phase == 0 && game.level4Charlie.state == 1) {
          for (const auto& roll : game.level4Rolls) {
            if (roll.active == 0 || roll.state > 1) continue;
            if (roll.col > 0x60 && roll.col <= 173) jump = true;
          }
        }
        updateGame(game, scripted.data(), jump, 0.0F);
        if (game.scene == Scene::Crashed && options.captureScene != "stage4-fall")
          break;
      }
      game.player.previous = game.player.position;
      game.previousCameraX = game.cameraX;
    } else if (options.captureScene == "stage3-goal" ||
               options.captureScene == "stage3-approach" ||
               options.captureScene == "stage3-roof" ||
               options.captureScene == "stage3-transfer" ||
               options.captureScene == "stage3-water" ||
               options.captureScene == "stage3-water-2" ||
               options.captureScene == "stage3") {
      // Run the board model with scripted joystick input so every capture
      // shows a state the arcade can actually reach.
      std::array<Uint8, SDL_NUM_SCANCODES> scripted{};
      auto stepFrames = [&](int frames, bool right) {
        scripted[SDL_SCANCODE_RIGHT] = right ? 1 : 0;
        for (int frame = 0; frame < frames; ++frame) {
          ++game.level1BoardFrameByte;
          updateLevel3(game, scripted.data(), 0.0F);
        }
      };
      game.level3Invulnerable = true;
      if (options.captureScene == "stage3-goal") {
        // No bag can be missed when none spawns ($8ABA limit), so the bird
        // and coin shower follow the celebration.
        game.level3BagsTotal = 0x21;
        while (game.level3State != 4) stepFrames(1, true);
        game.level3BagsTotal = 7;
        stepFrames(330, false);
      } else if (options.captureScene == "stage3-approach") {
        stepFrames(930, true);
      } else if (options.captureScene == "stage3-roof") {
        stepFrames(60, true);
        while (game.level3State != 8) stepFrames(1, false);
        stepFrames(12, false);
      } else if (options.captureScene == "stage3-transfer") {
        stepFrames(304, true);
      } else if (options.captureScene == "stage3-water") {
        stepFrames(130, true);
      } else if (options.captureScene == "stage3-water-2") {
        stepFrames(420, true);
      } else {
        stepFrames(60, true);
        stepFrames(60, false);
      }
      game.level3Invulnerable = false;
      game.player.previous = game.player.position;
      game.previousCameraX = game.cameraX;
    } else if (options.captureScene == "layout") {
      game.player.position = {78.0F, kGroundY};
      game.player.previous = game.player.position;
      game.player.runSpeed = 0.0F;
      game.player.grounded = true;
      game.cameraX = 0.0F;
      game.previousCameraX = 0.0F;
      for (auto& hoop : game.hoops) {
        hoop.worldX = -10000.0F;
        hoop.previousWorldX = hoop.worldX;
        hoop.active = false;
        hoop.kind = Level1HoopKind::Large;
      }
      for (auto& ring : game.bonusRings) ring.worldX = -10000.0F;
      for (auto& firePot : game.level1Pots) firePot = FirePot{};
      game.extraCharlieActive = false;
    } else if (options.captureScene == "large") {
      game.player.position = {800.0F, kGroundY};
      game.player.previous = game.player.position;
      game.player.runSpeed = 0.0F;
      game.player.grounded = true;
      game.cameraX = game.player.position.x - 78.0F;
      game.previousCameraX = game.cameraX;
      for (auto& hoop : game.hoops) {
        hoop.worldX = -10000.0F;
        hoop.previousWorldX = hoop.worldX;
        hoop.active = false;
      }
      game.hoops.front().worldX = game.player.position.x + 150.0F;
      game.hoops.front().previousWorldX = game.hoops.front().worldX;
      game.hoops.front().active = true;
      game.hoops.front().kind = Level1HoopKind::Large;
      for (auto& ring : game.bonusRings) ring.worldX = -10000.0F;
      for (auto& firePot : game.level1Pots) firePot = FirePot{};
      game.extraCharlieActive = false;
    } else if (options.captureScene == "prize") {
      game.player.position = {800.0F, kGroundY};
      game.player.previous = game.player.position;
      game.player.runSpeed = 0.0F;
      game.player.grounded = true;
      game.cameraX = game.player.position.x - 78.0F;
      game.previousCameraX = game.cameraX;
      for (auto& hoop : game.hoops) {
        hoop.worldX = -10000.0F;
        hoop.previousWorldX = hoop.worldX;
        hoop.active = false;
      }
      for (auto& ring : game.bonusRings) ring.worldX = -10000.0F;
      game.bonusRings.front().worldX = game.player.position.x + 150.0F;
      game.bonusRings.front().active = true;
      game.bonusRings.front().containsPrize = true;
      game.bonusRings.front().collected = false;
      for (auto& firePot : game.level1Pots) firePot = FirePot{};
      game.extraCharlieActive = false;
    } else if (options.captureScene == "ring") {
      game.player.position = {800.0F, kGroundY - 137.0F};
      game.player.previous = game.player.position;
      game.player.grounded = false;
      game.player.jumpFrame = 31;
      game.player.verticalVelocity = 0.0F;
      game.cameraX = game.player.position.x - 78.0F;
      game.previousCameraX = game.cameraX;
      for (auto& hoop : game.hoops) {
        hoop.worldX = -10000.0F;
        hoop.previousWorldX = hoop.worldX;
        hoop.active = false;
      }
      game.bonusRings.front().worldX =
          game.player.position.x + kLionCollisionCenterOffset;
      game.bonusRings.front().active = true;
      game.bonusRings.front().containsPrize = true;
      game.bonusRings.front().collected = false;
    } else if (options.captureScene == "extra") {
      game.player.position = {800.0F, kGroundY};
      game.player.previous = game.player.position;
      game.cameraX = game.player.position.x - 78.0F;
      game.previousCameraX = game.cameraX;
      for (auto& ring : game.bonusRings) ring.worldX = -10000.0F;
      for (std::size_t index = 0; index < game.hoops.size(); ++index) {
        game.hoops[index].worldX = index == 0 ? game.player.position.x + 76.0F
                                              : -10000.0F;
        game.hoops[index].previousWorldX = game.hoops[index].worldX;
        game.hoops[index].active = index == 0;
      }
      game.hoops.front().cleared = true;
      game.hoops.front().kind = Level1HoopKind::ExtraCharlie;
      game.level1ExtraCharlieState = 2;
      game.extraCharlieActive = true;
      game.extraCharlieHoopIndex = 0;
    } else if (options.captureScene == "crash") {
      game.scene = Scene::Crashed;
      game.player.position = {800.0F, kGroundY};
      game.player.previous = game.player.position;
      game.player.alive = false;
      game.player.grounded = true;
      game.player.runSpeed = 0.0F;
      game.cameraX = game.player.position.x - 78.0F;
      game.previousCameraX = game.cameraX;
      game.crashFrame = 34;
    } else if (options.captureScene == "goal" ||
               options.captureScene == "stage2-goal") {
      game.scene = Scene::Goal;
      const bool stage2Goal = options.captureScene == "stage2-goal";
      game.player.position = {
          stage2Goal ? kStage2GoalX : kCourseLength,
          stage2Goal ? kStage2GoalTopY : kGoalLandingY};
      game.player.previous = game.player.position;
      game.player.grounded = true;
      game.player.runSpeed = 0.0F;
      game.cameraX = game.player.position.x -
                     (stage2Goal ? 340.0F : kGoalScreenX);
      game.previousCameraX = game.cameraX;
      game.perfectClear = !stage2Goal;
      game.goalFrame = stage2Goal ? 100 : 45;
      for (auto& hoop : game.hoops) {
        hoop.worldX = -10000.0F;
        hoop.previousWorldX = hoop.worldX;
        hoop.active = false;
      }
      for (auto& ring : game.bonusRings) ring.collected = true;
    } else if (options.captureScene == "tally") {
      game.scene = Scene::Tally;
      game.bonus = 3722;
      game.score = 15440;
      game.timeScoreApplied = true;
      game.clearBonus = timeBonusFor(game.bonus);
      game.score += game.clearBonus;
      game.tallyFrame = 190;
    } else if (options.captureScene != "start") {
      game.player.position.x = 800.0F;
      game.player.previous = game.player.position;
      game.cameraX = game.player.position.x - 78.0F;
      game.previousCameraX = game.cameraX;
    }
  }
  RenderSurface surface =
      buildRenderSurface(renderer, window, options.rotation, {});
  if (!surface.texture) {
    std::cerr << "Render target creation failed: " << SDL_GetError() << '\n';
    if (controller) SDL_GameControllerClose(controller);
    destroyAudio(audio);
    destroyAssets(assets);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();
    return 1;
  }

  bool running = true;
  bool fullscreen = options.fullscreen;
  int rotation = options.rotation;
  bool jumpQueued = false;
  bool eventSelectMusicPlaying = false;
  bool stageMusicPlaying = false;
  bool stageMusicFast = false;
  if (options.startEvent > 0) {
    // Straight into one event, for testing a stage without sitting through
    // the boot video and the selection screen.
    insertCoin(game);
    enterEventSelect(game);
    game.selectedEvent = options.startEvent - 1;
    if (options.startWater && game.selectedEvent == 2) game.playCount[2] = 1;
    confirmEventSelection(game);
    if (options.startWater && game.selectedEvent == 2) {
      startEvent(game, 2, false, true);
    }
  }
  if (game.scene == Scene::Boot) playBootAudio(audio);
  std::uint32_t observedJumpAudioSerial = game.jumpAudioSerial;
  std::uint32_t observedCrashAudioSerial = game.crashAudioSerial;
  std::uint32_t observedStage6GoalAudioSerial = game.stage6GoalAudioSerial;
  std::uint32_t observedBetweenAudioSerial = game.betweenAudioSerial;
  bool attractAudioScene = false;
  std::uint32_t observedStage6FloorAudioSerial = game.stage6FloorAudioSerial;
  std::uint32_t observedStage2HitAudioSerial = game.stage2HitAudioSerial;
  std::uint32_t observedFloorFallAudioSerial = game.floorFallAudioSerial;
  int floorFailCountdown = 0;
  // Event 6's floor: the miss cue first, the fail piece once it has sounded
  // (18 frames), and the restart when the piece ends.
  constexpr int kStage6MissFrames = 18;
  int stage6FailCountdown = 0;
  bool stage6Cheering = false;
  std::uint32_t observedExtraCharlieAudioSerial =
      game.extraCharlieAudioSerial;
  std::uint32_t observedPrizeBagAudioSerial = game.prizeBagAudioSerial;
  std::uint32_t observedHiddenCoinAudioSerial = game.hiddenCoinAudioSerial;
  std::uint32_t observedCoinAudioSerial = game.coinAudioSerial;
  std::uint32_t observedEventSelectMoveAudioSerial =
      game.eventSelectMoveAudioSerial;
  std::uint32_t observedEventSelectConfirmAudioSerial =
      game.eventSelectConfirmAudioSerial;
  std::uint32_t observedStage5SpringAudioSerial = game.stage5SpringAudioSerial;
  std::uint32_t observedStage5FallAudioSerial = game.stage5FallAudioSerial;
  std::uint32_t observedStage6BounceAudioSerial = game.stage6BounceAudioSerial;
  std::uint32_t observedStage3BounceAudioSerial =
      game.stage3BounceAudioSerial;
  std::uint32_t observedLevel3ShowerAudioSerial = 0;
  std::uint32_t observedStage3OverjumpAudioSerial =
      game.stage3OverjumpAudioSerial;
  std::uint32_t observedStage4BallCollisionAudioSerial =
      game.stage4BallCollisionAudioSerial;
  Scene observedScene = game.scene;
  int observedGoalFrame = game.goalFrame;
  std::uint32_t observedLevel1ShowerAudioSerial = game.level1ShowerAudioSerial;
  double accumulator = 0.0;
  const Uint64 frequency = SDL_GetPerformanceFrequency();
  Uint64 previousCounter = SDL_GetPerformanceCounter();
  const Uint64 startCounter = previousCounter;
  std::ofstream movementTrace;
  int movementTraceFrame = 0;
  int movementTracePreviousScore = game.score;
  int movementTracePreviousHoopScore = game.level1HoopScoreAwarded;
  bool movementTracePreviousGrounded = game.player.grounded;
  int lastRiderDiagnosticFrame = -1;
  if (!options.riderDiagnosticDir.empty()) {
    std::error_code error;
    std::filesystem::create_directories(options.riderDiagnosticDir, error);
    if (error) {
      std::cerr << "Could not create rider diagnostic directory: "
                << error.message() << '\n';
      running = false;
    }
  }
  if (!options.tracePath.empty()) {
    movementTrace.open(options.tracePath);
    if (!movementTrace) {
      std::cerr << "Could not open movement trace: " << options.tracePath
                << '\n';
      running = false;
    } else {
      movementTrace
          << "frame,input_left,input_right,input_jump,player_x,player_y,"
             "delta_x,run_speed,camera_x,grounded,jump_frame,scene,alive,"
             "crash_frame,nearest_hoop,hoop_world_x,hoop_screen_x,"
             "hoop_previous_screen_x,hoop_opening_top,hoop_opening_bottom,"
             "hoop_cleared,hoop_overlap,rider_source_x,rider_source_y,"
             "jump_pending,jump_accumulator,hoop_active,hoop_x_8_8,"
             "hoop_animation_state,activation_accumulator,course_index,"
             "course_state,scroll_command,scroll_accumulator,"
             "collision_result,score,score_event,landing_transition,"
             "pending_hoop_score,hoop_score_event,"
             "rider_animation_state,rider_hd_frame,rider_anchor_x,"
             "rider_anchor_y,rider_animation_position_sample,"
             "rider_animation_course_8_8,"
             "rider0_status,rider0_y,rider0_x,rider0_code,rider0_attr,"
             "rider1_status,rider1_y,rider1_x,rider1_code,rider1_attr,"
             "rider2_status,rider2_y,rider2_x,rider2_code,rider2_attr,"
             "rider3_status,rider3_y,rider3_x,rider3_code,rider3_attr,"
             "rider4_status,rider4_y,rider4_x,rider4_code,rider4_attr,"
             "rider5_status,rider5_y,rider5_x,rider5_code,rider5_attr\n";
    }
  }

  while (running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) {
        running = false;
      } else if (event.type == SDL_WINDOWEVENT &&
                 (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
                  event.window.event == SDL_WINDOWEVENT_RESIZED)) {
        surface =
            buildRenderSurface(renderer, window, rotation, surface);
      } else if (event.type == SDL_CONTROLLERDEVICEADDED && !controller &&
                 SDL_IsGameController(event.cdevice.which)) {
        controller = SDL_GameControllerOpen(event.cdevice.which);
      } else if (event.type == SDL_CONTROLLERDEVICEREMOVED && controller &&
                 event.cdevice.which ==
                     SDL_JoystickInstanceID(
                         SDL_GameControllerGetJoystick(controller))) {
        SDL_GameControllerClose(controller);
        controller = nullptr;
      } else if (event.type == SDL_CONTROLLERBUTTONDOWN) {
        if (event.cbutton.button == SDL_CONTROLLER_BUTTON_BACK) {
          insertCoin(game);
        } else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_START) {
          pressStart(game);
        } else if (game.scene == Scene::EventSelect &&
                   event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_LEFT) {
          moveEventSelection(game, -1);
        } else if (game.scene == Scene::EventSelect &&
                   event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) {
          moveEventSelection(game, 1);
        } else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_A) {
          if (game.scene == Scene::EventSelect) {
            confirmEventSelection(game);
          } else {
            jumpQueued = true;
          }
        }
      } else if (event.type == SDL_KEYDOWN && !event.key.repeat) {
        switch (event.key.keysym.sym) {
          case SDLK_ESCAPE:
            if (game.scene == Scene::GameOver) {
              game.scene = Scene::Title;
              game.titleIdleFrames = 0;
            } else if (game.scene == Scene::Attract || game.scene == Scene::Boot ||
                       game.scene == Scene::Title) {
              // Nothing in play: Esc switches the cabinet off.
              running = false;
            } else if (game.scene == Scene::EventSelect) {
              game.scene = Scene::Title;
            } else {
              game.scene = Scene::Title;
              resetCourse(game);
            }
            break;
          case SDLK_RETURN:
          case SDLK_1:
            pressStart(game);
            break;
          case SDLK_LEFT:
            moveEventSelection(game, -1);
            break;
          case SDLK_RIGHT:
            moveEventSelection(game, 1);
            break;
          case SDLK_5:
          case SDLK_c:
            insertCoin(game);
            if (game.scene == Scene::Attract) game.scene = Scene::Title;
            break;
          case SDLK_SPACE:
          case SDLK_z:
            if (game.scene == Scene::EventSelect) {
              confirmEventSelection(game);
            } else {
              jumpQueued = true;
            }
            break;
          case SDLK_r:
            if (game.scene != Scene::Title &&
                game.scene != Scene::Complete) {
              resetCourse(game);
              game.scene = Scene::Playing;
            }
            break;
          case SDLK_F1:
            game.debug = !game.debug;
            break;
          case SDLK_F2:
            game.lionOnlyTest = !game.lionOnlyTest;
            break;
          case SDLK_u:
          case SDLK_d:
            // U grows and D shrinks the painted Charlie by 2% a press, 50%-200%.
            game.charlieScale = std::clamp(
                game.charlieScale * (event.key.keysym.sym == SDLK_u ? 1.02F : 1.0F / 1.02F),
                0.5F, 2.0F);
            game.charlieScaleNotice = 150;
            saveCharlieScale(charlieScaleFile, game);
            break;
          case SDLK_F10:
            // Turning a cabinet monitor on its side is easier to judge with
            // the game running, so the rotation can be cycled live.
            rotation = (rotation + 90) % 360;
            surface = buildRenderSurface(renderer, window, rotation, surface);
            break;
          case SDLK_F11:
            fullscreen = !fullscreen;
            setFullscreen(window, fullscreen);
            surface =
                buildRenderSurface(renderer, window, rotation, surface);
            break;
          default:
            break;
        }
      }
    }

    const Uint64 currentCounter = SDL_GetPerformanceCounter();
    double frameTime =
        static_cast<double>(currentCounter - previousCounter) /
        static_cast<double>(frequency);
    previousCounter = currentCounter;
    frameTime = std::min(frameTime, 0.1);
    if (!options.tracePath.empty() || replaying) frameTime = kFixedDt;
    accumulator += frameTime;

    const Uint8* keyboard = SDL_GetKeyboardState(nullptr);
    std::array<Uint8, SDL_NUM_SCANCODES> traceKeyboard{};
    bool traceLeft = false;
    bool traceRight = false;
    bool traceJump = false;
    if (!options.tracePath.empty()) {
      if (options.traceMode == "hold-right") {
        traceRight = true;
      } else if (options.traceMode == "right-release") {
        traceRight = movementTraceFrame < 60;
      } else if (options.traceMode == "right-left") {
        traceRight = movementTraceFrame < 60;
        traceLeft = movementTraceFrame >= 60;
      } else if (options.traceMode == "forward-jump" ||
                 options.traceMode == "successful-hoop") {
        traceRight = true;
        traceJump = movementTraceFrame ==
                    (options.traceMode == "successful-hoop" ? 24 : 10);
      } else if (options.traceMode == "air-right-hold") {
        traceRight = true;
        traceJump = movementTraceFrame == 24;
      } else if (options.traceMode == "air-right-left") {
        traceRight = movementTraceFrame < 27;
        traceLeft = movementTraceFrame >= 27;
        traceJump = movementTraceFrame == 24;
      } else if (options.traceMode == "air-right-release") {
        traceRight = movementTraceFrame < 27;
        traceJump = movementTraceFrame == 24;
      } else if (options.traceMode == "air-left-right") {
        traceLeft = movementTraceFrame < 13;
        traceRight = movementTraceFrame >= 13;
        traceJump = movementTraceFrame == 10;
      } else if (options.traceMode == "air-left-release") {
        traceLeft = movementTraceFrame < 13;
        traceJump = movementTraceFrame == 10;
      } else if (options.traceMode == "air-neutral") {
        traceJump = movementTraceFrame == 10;
      } else if (options.traceMode == "start-left") {
        traceLeft = true;
      } else if (options.traceMode == "start-right") {
        traceRight = true;
      } else if (options.traceMode == "start-right-left") {
        traceRight = movementTraceFrame < 25;
        traceLeft = movementTraceFrame >= 25;
      }
      traceKeyboard[SDL_SCANCODE_LEFT] = traceLeft ? 1 : 0;
      traceKeyboard[SDL_SCANCODE_RIGHT] = traceRight ? 1 : 0;
      keyboard = traceKeyboard.data();
    }
    int replayInput = 0xff;
    if (replaying) {
      if (replayIndex >= replayRows.size()) {
        running = false;
        break;
      }
      replayInput = replayRows[replayIndex].input;
      // Active-low P1 port: bit 0 LEFT, bit 1 RIGHT, bit 4 JUMP.
      traceLeft = (replayInput & 0x01) == 0;
      traceRight = (replayInput & 0x02) == 0;
      traceJump = (replayInput & 0x10) == 0 && (replayPreviousInput & 0x10) != 0;
      replayPreviousInput = replayInput;
      traceKeyboard[SDL_SCANCODE_LEFT] = traceLeft ? 1 : 0;
      traceKeyboard[SDL_SCANCODE_RIGHT] = traceRight ? 1 : 0;
      // The Event 2 and 4 handlers ($8532) do their own edge detection from
      // the held button, so they get the level of the port bit, not an edge.
      traceKeyboard[SDL_SCANCODE_SPACE] =
          (options.replayEvent == 1 || options.replayEvent == 3 ||
           options.replayEvent == 4 || options.replayEvent == 5) &&
                  (replayInput & 0x10) == 0
              ? 1
              : 0;
      keyboard = traceKeyboard.data();
      accumulator = kFixedDt;
    }
    float controllerAxis = 0.0F;
    if (controller) {
      controllerAxis =
          static_cast<float>(
              SDL_GameControllerGetAxis(controller,
                                        SDL_CONTROLLER_AXIS_LEFTX)) /
          32767.0F;
      if (SDL_GameControllerGetButton(controller,
                                      SDL_CONTROLLER_BUTTON_DPAD_LEFT)) {
        controllerAxis = -1.0F;
      } else if (SDL_GameControllerGetButton(
                     controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) {
        controllerAxis = 1.0F;
      }
    }
    bool jumpForStep = jumpQueued || traceJump;
    int boardStepsThisFrame = 0;
    while (accumulator >= kFixedDt) {
      ++boardStepsThisFrame;
      const float previousPlayerX = game.player.position.x;
      updateGame(game, keyboard, jumpForStep, controllerAxis);
      awardScoreLives(game);
      if (game.score > game.highScore) {
        game.highScore = game.score;
        game.highScoreDirty = true;
      }
      jumpForStep = false;
      jumpQueued = false;
      accumulator -= kFixedDt;
      if (replaying &&
          (options.replayEvent >= 1 && options.replayEvent <= 5)) {
        if (replayFrame == 0 && replayRows[replayIndex].bonus >= 0) {
          // The capture may poke the bonus digits on its first stage frame.
          game.bonus = replayRows[replayIndex].bonus;
        }
        if (replayOutput && options.replayEvent == 1) {
          const Level4Record& c = game.level2Charlie;
          const int phase6 = game.scene == Scene::Crashed ? 5
                             : (game.scene == Scene::Tally ? 4 : 3);
          auto owner = [&](const Level4Record& m) {
            return m.owner;
          };
          replayOutput << replayFrame << ',' << replayRows[replayIndex].rel
                       << ',' << std::hex << replayInput << std::dec << ','
                       << c.state << ',' << c.row << ',' << c.col << ','
                       << c.half << ',' << c.phase << ',' << c.landVelocity
                       << ',' << c.launch << ',' << c.direction << ','
                       << c.velocity << ',' << c.jump << ',' << c.stick << ','
                       << c.idleHigh << ',' << c.idleLow << ',' << c.countdown
                       << ",0," << game.level2Scroll << ',' << game.level2HalfStep
                       << ',' << game.score << ',' << game.bonus << ','
                       << game.lives << ',' << game.level2SchedCounter
                       << ",0000," << game.level2Buffered << ','
                       << game.level2StandHit << ',' << game.level2StandGoal
                       << ',' << game.level2Landings << ',' << game.level2Held
                       << ',' << game.level2Skip << ',' << game.level2Near
                       << ',' << game.level2Far << ',' << game.level2Flash
                       << ',' << game.level2Timeout << ',' << game.level2Clear
                       << ",0,0," << game.level2Visits;
          for (const auto& m : game.level2Monkeys) {
            if (m.active == 0 && m.flag == 0) {
              replayOutput << ",0,0,0,0,0,0,0,0,0,0,0,0,0";
            } else {
              replayOutput << ',' << m.active << ',' << m.flag << ','
                           << m.state << ',' << m.row << ',' << m.col << ','
                           << m.half << ',' << m.phase << ',' << m.landVelocity
                           << ',' << m.velocity << ',' << m.leapTimer << ','
                           << m.kind << ',' << owner(m) << ",0";
            }
          }
          // $2760's three cells sit at col, col-8 and col+8; the landing
          // ($8097) nudges the third one a column left.
          const Level4Record& st = game.level2Stand;
          replayOutput << ',' << st.active << ',' << st.state << ',' << st.row
                       << ',' << st.col << ',' << (st.active ? st.col - 8 : 0)
                       << ',' << (st.active ? st.col + 8 - st.state : 0);
          for (const auto& popup : game.level2Popups) {
            if (popup.active == 0) {
              replayOutput << ",-1,0,0,0,0";
            } else {
              replayOutput << ',' << popup.state << ',' << popup.flag << ','
                           << popup.row << ',' << popup.col << ',' << popup.timer;
            }
          }
          for (const auto& plaque : game.level2Plaques) {
            replayOutput << ',' << plaque.col;
          }
          if (!game.level2BirdActive) {
            replayOutput << ",-1,0,0";
          } else {
            replayOutput << ',' << game.level2BirdState << ','
                         << game.level2BirdX << ',' << game.level2BirdBagX;
          }
          // The stand's bags are the fourth coin record ($2760 and $2730+$30
          // overlap); the rest stay empty on this stage.
          for (int index = 0; index < 13; ++index) {
            if (index == 3 && st.active != 0) {
              replayOutput << ',' << st.row << ',' << st.col;
            } else {
              replayOutput << ",-1,0";
            }
          }
          replayOutput << ",0,0,0," << phase6 << ','
                       << static_cast<int>(game.level1BoardFrameByte) << ",180\n";
        } else if (replayOutput && options.replayEvent == 5) {
          const Level6& l = game.level6;
          const auto m = [&](int a) { return static_cast<int>(l.ram[static_cast<std::size_t>(a & 0x3fff)]); };
          const int phase6 = game.scene == Scene::Crashed ? 5
                             : (game.scene == Scene::Tally ? 4 : 3);
          replayOutput << replayFrame << ',' << replayRows[replayIndex].rel
                       << ',' << std::hex << replayInput << std::dec << ','
                       << m(0x2800) << ",5," << ((m(0x2203) << 8) | m(0x2204)) << ','
                       << m(0x2203) << ',' << m(0x2204) << ',' << m(0x2208) << ','
                       << m(0x220a) << ',' << m(0x220b) << ',' << game.lives << ','
                       << game.score << ',' << game.bonus;
          for (int a = 0x20b0; a <= 0x20ff; ++a) replayOutput << ',' << m(a);
          for (int a : {0x2774, 0x2776, 0x277e, 0x2764, 0x2766, 0x276e,
                        0x2734, 0x2736, 0x273e, 0x2724, 0x2726, 0x272e,
                        0x27e4, 0x27e6, 0x27ee, 0x2714, 0x2716, 0x271e}) {
            replayOutput << ',' << m(a);
          }
          for (int i = 0; i < 5; ++i) {
            for (int b = 0; b < 16; ++b) replayOutput << ',' << m(0x2900 + i * 0x30 + b);
          }
          for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 16; ++c) replayOutput << ',' << m(0x24f6 + r * 0x100 - c * 0x10);
          }
          replayOutput << ',' << m(0x287d) << ',' << m(0x287e) << ',' << m(0x287f)
                       << ',' << l.visits << ",0," << phase6 << ','
                       << static_cast<int>(game.level1BoardFrameByte) << ",180\n";
        } else if (replayOutput && options.replayEvent == 4) {
          const Level5& l5 = game.level5;
          const int phase6 = game.scene == Scene::Crashed ? 5
                             : (game.scene == Scene::Tally ? 4 : 3);
          replayOutput << replayFrame << ',' << replayRows[replayIndex].rel
                       << ',' << std::hex << replayInput << std::dec << ','
                       << l5.phase << ",4," << l5.scroll << ','
                       << ((l5.scroll >> 8) & 0xff) << ',' << (l5.scroll & 0xff)
                       << ',' << l5.spawnCount << ',' << l5.perfect << ','
                       << l5.chain << ',' << game.lives << ',' << game.score
                       << ',' << game.bonus;
          for (int a = 0; a < 0x13; ++a) replayOutput << ',' << l5.v[static_cast<std::size_t>(a)];
          replayOutput << ',' << l5.chRow << ',' << l5.chCol << ',' << l5.chCode
                       << ',' << l5.ponyRow << ',' << l5.ponyCol << ','
                       << l5.ponyCode;
          for (int k = 0; k < 3; ++k) {
            replayOutput << ',' << l5.plRow[static_cast<std::size_t>(k)] << ','
                         << l5.plCol[static_cast<std::size_t>(k)] << ','
                         << l5.plCode[static_cast<std::size_t>(k)];
          }
          for (const auto& s : l5.springs) {
            replayOutput << ',' << s.state << ',' << s.gap << ',' << s.height
                         << ',' << s.cols << ',' << s.type << ',' << s.col << ','
                         << (s.vram >> 8) << ',' << (s.vram & 0xff) << ',' << s.x
                         << ',' << s.row << ',' << s.halfA << ',' << s.halfB
                         << ',' << s.halfC << ',' << s.tileD << ',' << s.tileE;
          }
          replayOutput << ',' << l5.flashState << ',' << l5.flashTimer << ','
                       << l5.flashPhase << ',' << l5.visits << ",0," << phase6
                       << ',' << static_cast<int>(game.level1BoardFrameByte)
                       << ",180\n";
        } else if (replayOutput && options.replayEvent == 3) {
          const Level4Record& charlie = game.level4Charlie;
          const Level4Record& ball = game.level4Ball;
          const int phase6 = game.scene == Scene::Crashed ? 5
                             : (game.scene == Scene::Tally ? 4 : 3);
          replayOutput << replayFrame << ',' << replayRows[replayIndex].rel
                       << ',' << std::hex << replayInput << std::dec << ','
                       << charlie.state << ',' << charlie.row << ','
                       << charlie.col << ',' << charlie.phase << ','
                       << charlie.landVelocity << ',' << charlie.launch << ','
                       << charlie.direction << ',' << charlie.velocity << ','
                       << charlie.jump << ',' << charlie.stick << ','
                       << charlie.idleHigh << ',' << charlie.idleLow << ','
                       << charlie.countdown << ',' << ball.active << ','
                       << ball.flag << ',' << ball.state << ',' << ball.row
                       << ',' << ball.col << ',' << ball.half << ','
                       << ball.align << ',' << game.level4Scroll << ','
                       << game.score << ',' << game.bonus << ',' << game.lives
                       << ',' << game.level4Buffered << ',' << game.level4Doubles
                       << ',' << game.level4Pending << ',' << game.level4Falls
                       << ',' << game.level4Landings << ',' << game.level4Held
                       << ',' << game.level4Standing << ',' << game.level4Flash
                       << ',' << game.level4Timeout << ',' << game.level4Visits;
          for (const auto& roll : game.level4Rolls) {
            replayOutput << ',' << roll.active << ',' << roll.flag << ','
                         << roll.state << ',' << roll.row << ',' << roll.col
                         << ',' << roll.half;
          }
          for (const auto& popup : game.level4Popups) {
            if (popup.active == 0) {
              replayOutput << ",-1,0,0,0,0";
            } else {
              replayOutput << ',' << popup.state << ',' << popup.flag << ','
                           << popup.row << ',' << popup.col << ','
                           << popup.timer;
            }
          }
          for (const auto& plaque : game.level4Plaques) {
            replayOutput << ',' << plaque.col;
          }
          replayOutput << ",0," << phase6 << ','
                       << static_cast<int>(game.level1BoardFrameByte) << ','
                       << game.level4Dips << '\n';
        } else if (replayOutput) {
          const int phase6 = game.scene == Scene::Crashed ? 5
                             : (game.scene == Scene::Tally ? 4 : 3);
          replayOutput << replayFrame << ',' << replayRows[replayIndex].rel
                       << ',' << std::hex << replayInput << std::dec << ','
                       << game.level3State << ',' << static_cast<int>(game.level3Y)
                       << ',' << static_cast<int>(game.level3X) << ','
                       << game.level3Phase << ',' << game.level3Velocity << ','
                       << game.level3Target << ',' << game.level3Bounce << ','
                       << game.level3Direction << ',' << game.level3Stick << ','
                       << game.level3Scroll << ',' << game.score << ','
                       << game.level3Missed << ',' << game.level3BagValueIndex
                       << ',' << game.level3BagsTotal << ',' << game.level3TileTimer;
          for (const auto& performer : game.level3Performers) {
            if (!performer.active) {
              replayOutput << ",0,0,0,0,0";
            } else {
              replayOutput << ",1," << static_cast<int>(performer.x) << ','
                           << static_cast<int>(performer.timer) << ','
                           << performer.remaining << ',' << performer.type;
            }
          }
          for (std::size_t slot = 0; slot < 3; ++slot) {
            const auto& flame = game.level3Flames[slot];
            if (!flame.active) {
              replayOutput << ",-1,0,0,0,0,0";
            } else if (flame.state == 3) {
              // A splash: $7:$8 are the launch bytes $9735 never touched,
              // $9 and $3 stay zero.
              replayOutput << ",3," << static_cast<int>(flame.y) << ','
                           << static_cast<int>(flame.x) << ",0,0,0";
            } else {
              replayOutput << ',' << flame.state << ',' << static_cast<int>(flame.y)
                           << ',' << static_cast<int>(flame.x) << ',' << flame.velocity
                           << ',' << (flame.apex ? 1 : 0) << ','
                           << static_cast<int>(flame.hold);
            }
          }
          for (const auto& knife : game.level3Knives) {
            if (!knife.active) {
              replayOutput << ",-1,0,0,0,0,0,0";
            } else {
              replayOutput << ',' << knife.state << ',' << static_cast<int>(knife.y)
                           << ',' << static_cast<int>(knife.x) << ',' << knife.velocity
                           << ',' << (knife.apex ? 1 : 0) << ','
                           << static_cast<int>(knife.sway) << ','
                           << static_cast<int>(knife.hold);
            }
          }
          for (const auto& bag : game.level3Bags) {
            if (!bag.active) {
              replayOutput << ",-1,0,0,0";
            } else {
              replayOutput << ',' << bag.state << ',' << static_cast<int>(bag.x) << ','
                           << static_cast<int>(bag.key) << ','
                           << static_cast<int>(bag.timer);
            }
          }
          if (options.replayWater) {
            constexpr std::array<int, 5> scriptBase{0xed9e, 0xedce, 0xedfe, 0xee2e, 0xee66};
            for (const auto& dolphin : game.level3Performers) {
              if (!dolphin.active) {
                replayOutput << ",0,0,0,0,0,0,0,0,0,0,0,0,0";
                continue;
              }
              const int pointer = scriptBase[static_cast<std::size_t>(dolphin.script)] +
                                  9 * (dolphin.scriptIndex + 1);
              replayOutput << ',' << dolphin.state << ',' << static_cast<int>(dolphin.y)
                           << ',' << static_cast<int>(dolphin.x) << ',' << dolphin.phase
                           << ',' << (dolphin.velocity >> 8) << ',' << (dolphin.velocity & 0xff)
                           << ',' << (dolphin.counter & 0xff) << ',' << (dolphin.offset ? 1 : 0)
                           << ',' << dolphin.remaining << ',' << static_cast<int>(dolphin.timer)
                           << ',' << static_cast<int>(dolphin.key) << ',' << dolphin.type
                           << ',' << pointer;
            }
          }
          if (!game.level3BirdActive) {
            replayOutput << ",-1,0,0";
          } else {
            replayOutput << ',' << game.level3BirdState << ','
                         << static_cast<int>(game.level3BirdX) << ','
                         << static_cast<int>(game.level3BirdBagX);
          }
          for (const auto& coin : game.level3Coins) {
            if (!coin.active) {
              replayOutput << ",-1,0";
            } else {
              replayOutput << ',' << static_cast<int>(coin.y) << ','
                           << static_cast<int>(coin.x);
            }
          }
          replayOutput << ',' << game.level3CoinIndex << ',' << game.level3CoinCount
                       << ',' << game.bonus << ',' << game.lives << ",0," << phase6
                       << ',' << static_cast<int>(game.level1BoardFrameByte) << '\n';
        }
        ++replayFrame;
        ++replayIndex;
        accumulator = 0.0;
        if (game.scene == Scene::Title) running = false;
      } else if (replaying) {
        if (replayOutput) {
          const int riderRow =
              kLevel1RiderGroundSourceY - level1RiderDisplacement(game.player);
          replayOutput << replayFrame << ',' << replayRows[replayIndex].frame
                       << ',' << std::hex << replayInput << std::dec << ','
                       << (traceLeft ? 1 : 0) << ',' << (traceRight ? 1 : 0)
                       << ',' << (traceJump ? 1 : 0) << ','
                       << game.level1ProgressFixed << ','
                       << level1Page(game.level1ProgressFixed) << ','
                       << static_cast<int>(level1PageOffsetByte(game.level1ProgressFixed))
                       << ',' << std::fixed << std::setprecision(4)
                       << level1ProgressPixels(game) << ','
                       << (game.player.grounded ? 0 : 1) << ','
                       << game.player.jumpFrame << ',' << riderRow << ','
                       << game.score << ',' << game.bonus << ',' << game.lives
                       << ',' << static_cast<int>(game.scene) << ','
                       << game.level1HoopCourseIndex << ','
                       << static_cast<int>(game.level1HoopCourseState) << ','
                       << static_cast<int>(game.level1HoopCourseOffset) << ','
                       << game.level1HoopActivationAccumulator << ','
                       << game.level1RetireDistance;
          for (const auto& hoop : game.hoops) {
            replayOutput << ',' << (hoop.active ? static_cast<int>(hoop.kind) + 1 : 0)
                         << ',' << hoop.sourceXFixed;
          }
          for (const auto& pot : game.level1Pots) {
            replayOutput << ',' << static_cast<int>(pot.status) << ','
                         << pot.sourceXFixed << ',' << pot.countdown;
          }
          replayOutput << ',' << static_cast<int>(game.level1PotCounter) << ','
                       << game.level1CoinPot << ',' << game.level1CoinState
                       << ',' << static_cast<int>(game.level1CoinX) << ','
                       << ((game.level1CoinYFixed >> 8) & 0xff) << ','
                       << game.level1ExtraCharlieState
                       << ',' << static_cast<int>(game.level1BagState) << ','
                       << level1RiderStateName(game.level1RiderState) << ','
                       << game.level1GoalCounter << '\n';
        }
        ++replayFrame;
        ++replayIndex;
        accumulator = 0.0;
      }
      if (movementTrace) {
        std::size_t nearestHoop = 0;
        float nearestDistance = std::numeric_limits<float>::max();
        for (std::size_t index = 0; index < game.hoops.size(); ++index) {
          if (!game.hoops[index].active) continue;
          const float distance = std::abs(
              game.hoops[index].worldX - game.cameraX -
              (game.player.position.x - game.cameraX +
               kLionCollisionCenterOffset));
          if (distance < nearestDistance) {
            nearestDistance = distance;
            nearestHoop = index;
          }
        }
        const Hoop& hoop = game.hoops[nearestHoop];
        const int riderSourceY = static_cast<int>(std::lround(
            game.player.position.y / kSourceToLogicalY)) - 28;
        const int hoopSourceX = hoop.sourceXFixed >> 8U;
        const int hoopHorizontalDistance = std::abs(hoopSourceX - 0x40);
        const int hoopVerticalDistance = riderSourceY - 0xb6;
        const char* collisionResult =
            hoopHorizontalDistance >= 0x0e
                ? "safe_x"
                : (hoopVerticalDistance < 0
                       ? "safe_above"
                       : (hoopVerticalDistance + hoopHorizontalDistance <=
                                  0x1c
                              ? "failure"
                              : "safe_boundary"));
        const int scoreEvent = game.score - movementTracePreviousScore;
        const int hoopScoreEvent =
            game.level1HoopScoreAwarded - movementTracePreviousHoopScore;
        const bool landingTransition =
            !movementTracePreviousGrounded && game.player.grounded;
        const Level1RiderState visibleRiderState =
            game.player.grounded ? game.level1RiderState
                                 : Level1RiderState::RunC;
        const auto riderCodes = level1RiderCodes(visibleRiderState);
        const auto& riderAnchor = kLevel1RiderProductionAnchor;
        movementTrace << movementTraceFrame << ',' << (traceLeft ? 1 : 0)
                      << ',' << (traceRight ? 1 : 0) << ','
                      << (traceJump ? 1 : 0) << ',' << std::fixed
                      << std::setprecision(6) << game.player.position.x << ','
                      << game.player.position.y << ','
                      << (game.player.position.x - previousPlayerX) << ','
                      << game.player.runSpeed << ',' << game.cameraX << ','
                      << (game.player.grounded ? 1 : 0) << ','
                      << game.player.jumpFrame << ','
                      << static_cast<int>(game.scene) << ','
                      << (game.player.alive ? 1 : 0) << ','
                      << game.crashFrame << ',' << nearestHoop << ','
                      << hoop.worldX << ',' << hoop.worldX - game.cameraX
                      << ',' << hoop.previousWorldX - game.previousCameraX
                      << ',' << hoop.openingTop << ',' << hoop.openingBottom
                      << ',' << (hoop.cleared ? 1 : 0) << ','
                      << (overlapsLevel1LargeHoop(
                              game.player, hoop, nearestHoop,
                              game.extraCharlieActive &&
                                  game.extraCharlieHoopIndex ==
                                      static_cast<int>(nearestHoop))
                              ? 1
                              : 0)
                      << ',' << 0x25 << ',' << riderSourceY << ','
                      << (game.player.level1JumpPending ? 1 : 0) << ','
                      << (game.player.grounded ? 0 : game.player.jumpFrame)
                      << ',' << (hoop.active ? 1 : 0) << ','
                      << hoop.sourceXFixed << ',' << "native_composite" << ','
                      << game.level1HoopActivationAccumulator << ','
                      << game.level1HoopCourseIndex << ','
                      << static_cast<int>(
                             game.level1HoopActivationAccumulator >> 8U)
                      << ','
                      << (game.player.grounded
                              ? (traceRight ? -384 : (traceLeft ? 304 : 0))
                              : (game.player.level1AirborneDirection > 0
                                     ? -384
                                     : (game.player.level1AirborneDirection < 0
                                            ? 304
                                            : 0)))
                      << ',' << static_cast<int>(std::lround(game.cameraX))
                      << ',' << collisionResult << ',' << game.score << ','
                      << scoreEvent << ',' << (landingTransition ? 1 : 0)
                      << ',' << game.level1PendingHoopScore << ','
                      << hoopScoreEvent << ','
                      << level1RiderStateName(visibleRiderState) << ','
                      << level1RiderProductionAsset(visibleRiderState) << ','
                      << riderAnchor[0] << ',' << riderAnchor[1] << ','
                      << static_cast<int>(game.level1RiderPositionSample)
                      << ',' << game.level1RiderCourseFixed;
        for (std::size_t slot = 0; slot < riderCodes.size(); ++slot) {
          const int slotY = riderSourceY + (slot < 3 ? 0x10 : 0x00);
          const int slotX = 0x45 - static_cast<int>(slot % 3) * 0x10;
          movementTrace << ',' << 0 << ',' << slotY << ',' << slotX << ','
                        << riderCodes[slot] << ',' << 0;
        }
        movementTrace << '\n';
        movementTracePreviousScore = game.score;
        movementTracePreviousHoopScore = game.level1HoopScoreAwarded;
        movementTracePreviousGrounded = game.player.grounded;
        ++movementTraceFrame;
        if (movementTraceFrame >= 120) running = false;
      }
    }

    const bool shouldPlayEventSelectMusic =
        game.scene == Scene::EventSelect;
    if (shouldPlayEventSelectMusic != eventSelectMusicPlaying) {
      stopStageMusic(audio);
      if (shouldPlayEventSelectMusic) {
        playEventSelectMusic(audio);
        stageMusicPlaying = false;
        stageMusicFast = false;
      }
      eventSelectMusicPlaying = shouldPlayEventSelectMusic;
    }

    // Event 5 silences the music at the fall ($A65C -> $BA6C), before the
    // miss cue and the restart.
    const bool shouldPlayStageMusic =
        game.scene == Scene::Playing &&
        !(game.selectedEvent == 4 && game.level5.phase == 2) &&
        !(game.selectedEvent == 1 && (game.level2Charlie.state == 2 ||
                                      game.level2Charlie.state >= 7)) &&
        !(game.selectedEvent == 2 && game.level3State >= 7) &&
        !(game.selectedEvent == 3 && (game.level4Charlie.state == 2 ||
                                      game.level4Charlie.state >= 7)) &&
        !(game.selectedEvent == 5 && game.level6.ram[0x2800] >= 2);
    const bool shouldUseFastStageMusic =
        shouldPlayStageMusic && game.bonus <= 499;
    if (!shouldPlayEventSelectMusic &&
        shouldPlayStageMusic != stageMusicPlaying) {
      if (shouldPlayStageMusic) {
        playStageMusic(audio, game.selectedEvent, shouldUseFastStageMusic);
        stageMusicFast = shouldUseFastStageMusic;
      } else {
        stopStageMusic(audio);
        stageMusicFast = false;
      }
      stageMusicPlaying = shouldPlayStageMusic;
    } else if (shouldPlayStageMusic &&
               shouldUseFastStageMusic != stageMusicFast) {
      setStageMusicFast(audio, shouldUseFastStageMusic);
      stageMusicFast = shouldUseFastStageMusic;
    }
    if (observedJumpAudioSerial != game.jumpAudioSerial) {
      playJumpSound(audio);
      observedJumpAudioSerial = game.jumpAudioSerial;
    }
    if (observedCrashAudioSerial != game.crashAudioSerial) {
      playFailMusic(audio);
      observedCrashAudioSerial = game.crashAudioSerial;
    }
    if (observedStage6FloorAudioSerial != game.stage6FloorAudioSerial) {
      stopStageMusic(audio);
      stageMusicPlaying = false;
      playStage3OverjumpSound(audio);
      stage6FailCountdown = kStage6MissFrames;
      // The board waits $80 frames on the floor before the wipe; the rest
      // of the piece runs through the crash scene.
      // Replays and silent builds keep the board's own 96-frame restart.
      if (audio.fail.data && !replaying) {
        game.level6FloorRestartFrames = std::max(
            1, kStage6MissFrames + audioDurationInBoardFrames(audio.fail) - 0x80);
      }
      observedStage6FloorAudioSerial = game.stage6FloorAudioSerial;
    }
    if (stage6FailCountdown > 0 && --stage6FailCountdown == 0) {
      playFailMusic(audio);
    }
    if (observedStage2HitAudioSerial != game.stage2HitAudioSerial) {
      playStage3OverjumpSound(audio);     // the miss, at the hit
      observedStage2HitAudioSerial = game.stage2HitAudioSerial;
    }
    if (observedFloorFallAudioSerial != game.floorFallAudioSerial) {
      stopStageMusic(audio);
      stageMusicPlaying = false;
      if (game.floorFallMiss) {
        playStage3OverjumpSound(audio);   // the miss on the floor...
        floorFailCountdown = kFloorMissFrames;   // ...then the fail piece
      } else {
        playFailMusic(audio);
      }
      observedFloorFallAudioSerial = game.floorFallAudioSerial;
    }
    if (floorFailCountdown > 0 && --floorFailCountdown == 0) {
      playFailMusic(audio);
    }
    if (observedBetweenAudioSerial != game.betweenAudioSerial) {
      playBetweenAudio(audio);
      observedBetweenAudioSerial = game.betweenAudioSerial;
    }
    if (game.scene != Scene::Attract && game.scene != Scene::Boot && attractAudioScene) {
      stopStageMusic(audio);   // the footage's sound stops with the footage
    }
    attractAudioScene = game.scene == Scene::Attract;
    if (observedStage6GoalAudioSerial != game.stage6GoalAudioSerial) {
      // The landing on the plank: the piece stops and the crowd cheers
      // for the whole celebration, until the stage is wiped.
      stopStageMusic(audio);
      stageMusicPlaying = false;
      playCrowdCheer(audio, true);
      stage6Cheering = true;
      observedStage6GoalAudioSerial = game.stage6GoalAudioSerial;
    }
    if (stage6Cheering &&
        !(game.selectedEvent == 5 && game.scene == Scene::Playing &&
          game.level6.ram[0x2800] == 2)) {
      stopCrowdCheer(audio);
      stage6Cheering = false;
    }
    if (observedStage4BallCollisionAudioSerial !=
        game.stage4BallCollisionAudioSerial) {
      playStage4BallCollisionSound(audio);
      observedStage4BallCollisionAudioSerial =
          game.stage4BallCollisionAudioSerial;
    }
    if (observedExtraCharlieAudioSerial !=
        game.extraCharlieAudioSerial) {
      playExtraCharlieSound(audio);
      observedExtraCharlieAudioSerial = game.extraCharlieAudioSerial;
    }
    if (observedPrizeBagAudioSerial != game.prizeBagAudioSerial) {
      playPrizeBagSound(audio);
      observedPrizeBagAudioSerial = game.prizeBagAudioSerial;
    }
    if (observedHiddenCoinAudioSerial != game.hiddenCoinAudioSerial) {
      playHiddenCoinSound(audio);
      observedHiddenCoinAudioSerial = game.hiddenCoinAudioSerial;
    }
    if (observedCoinAudioSerial != game.coinAudioSerial) {
      playCreditInsertSound(audio);
      observedCoinAudioSerial = game.coinAudioSerial;
    }
    if (observedEventSelectMoveAudioSerial !=
        game.eventSelectMoveAudioSerial) {
      playEventSelectMoveSound(audio);
      observedEventSelectMoveAudioSerial =
          game.eventSelectMoveAudioSerial;
    }
    if (observedEventSelectConfirmAudioSerial !=
        game.eventSelectConfirmAudioSerial) {
      playEventSelectConfirmSound(audio);
      observedEventSelectConfirmAudioSerial =
          game.eventSelectConfirmAudioSerial;
    }
    if (observedStage3BounceAudioSerial !=
        game.stage3BounceAudioSerial) {
      playStage3BounceSound(audio);
      observedStage3BounceAudioSerial = game.stage3BounceAudioSerial;
    }
    if (observedStage5SpringAudioSerial != game.stage5SpringAudioSerial) {
      playStage5SpringSound(audio);
      observedStage5SpringAudioSerial = game.stage5SpringAudioSerial;
    }
    if (observedStage5FallAudioSerial != game.stage5FallAudioSerial) {
      playStage5FallSound(audio);
      observedStage5FallAudioSerial = game.stage5FallAudioSerial;
    }
    if (observedStage6BounceAudioSerial != game.stage6BounceAudioSerial) {
      playStage6BounceSound(audio);
      observedStage6BounceAudioSerial = game.stage6BounceAudioSerial;
    }
    if (observedStage3OverjumpAudioSerial !=
        game.stage3OverjumpAudioSerial) {
      playStage3OverjumpSound(audio);
      observedStage3OverjumpAudioSerial =
          game.stage3OverjumpAudioSerial;
    }
    if (observedLevel3ShowerAudioSerial != game.level3ShowerAudioSerial) {
      playBirdCoinDrop(audio);
      observedLevel3ShowerAudioSerial = game.level3ShowerAudioSerial;
    }
    if (observedLevel1ShowerAudioSerial != game.level1ShowerAudioSerial) {
      playBirdCoinDrop(audio);
      observedLevel1ShowerAudioSerial = game.level1ShowerAudioSerial;
    }
    if (game.scene != observedScene) {
      if (std::getenv("CIRCUS_TRACE_SCENES")) {
        std::cerr << "scene " << static_cast<int>(observedScene) << " -> "
                  << static_cast<int>(game.scene) << " event " << game.selectedEvent
                  << " credits " << game.credits << '\n';
      }
      if (game.highScoreDirty &&
          (game.scene == Scene::Crashed || game.scene == Scene::Goal ||
           game.scene == Scene::Complete || game.scene == Scene::Title)) {
        if (saveHighScore(highScorePath, game.highScore)) {
          game.highScoreDirty = false;
        }
      }
      if (game.scene == Scene::Goal) playCrowdCheer(audio);
      if (game.scene == Scene::Tally) playBonusCount(audio);
      if (game.scene == Scene::Playing || game.scene == Scene::EventSelect ||
          game.scene == Scene::GameOver) {
        if (!replaying && options.startEvent == 0 && options.recordDir.empty()) {
          saveProgress(progressPath, game);
        }
      }
      observedScene = game.scene;
    }
    const int showerStart =
        kGoalArrivalFrames + kBirdArrivalFrames + kBagDropFrames;
    if (game.scene == Scene::Goal && game.perfectClear &&
        game.selectedEvent != 0 && game.selectedEvent != 2 &&
        observedGoalFrame < showerStart && game.goalFrame >= showerStart) {
      playBirdCoinDrop(audio);
    }
    observedGoalFrame = game.goalFrame;

    const double timeSeconds =
        static_cast<double>(currentCounter - startCounter) /
        static_cast<double>(frequency);
    const double interpolation = accumulator / kFixedDt;

    SDL_SetRenderTarget(renderer, surface.texture);
    SDL_RenderSetViewport(renderer, nullptr);
    SDL_RenderSetScale(renderer,
                       static_cast<float>(surface.width) / kWorldWidth,
                       static_cast<float>(surface.height) / kWorldHeight);
    renderScene(renderer, game, surface, assets, timeSeconds, interpolation);

    const bool recordingScene = game.scene == Scene::Playing ||
                                game.scene == Scene::Crashed || game.scene == Scene::Goal;
    if (!options.recordDir.empty() && recordingScene) {
      // The playfield straight off the render target, one JPEG per clip
      // frame, until the clip is long enough.
      if (recordBoardFrame == 0) {
        std::error_code error;
        std::filesystem::create_directories(options.recordDir, error);
        setAudioCapture(audio, true);
      }
      // Clip time is board time, so a display faster than the board (or a
      // slow one) neither stretches nor skips the clip: this frame stands in
      // for every clip frame the board steps since the last one call for.
      recordBoardFrame += boardStepsThisFrame;
      const int wanted = recordBoardFrame * options.recordFps / static_cast<int>(kBoardRefresh);
      if (wanted > recordedFrames) {
        SDL_Surface* shot = SDL_CreateRGBSurfaceWithFormat(0, surface.width, surface.height,
                                                          32, SDL_PIXELFORMAT_RGBA32);
        const bool read = shot && SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_RGBA32,
                                                       shot->pixels, shot->pitch) == 0;
        while (recordedFrames < wanted) {
          std::array<char, 64> name{};
          std::snprintf(name.data(), name.size(), "/frame-%04d.jpg", recordedFrames);
          if (read) IMG_SaveJPG(shot, (options.recordDir + name.data()).c_str(), 92);
          ++recordedFrames;
        }
        if (shot) SDL_FreeSurface(shot);
      }
      // The clip ends when the fall (or the goal) has played out: the scene
      // leaves the crash for the restart, or the stage ends.
      if (game.scene == Scene::Crashed || game.scene == Scene::Goal) recordSawEnd = true;
      if (recordBoardFrame >= options.recordSeconds * static_cast<int>(kBoardRefresh)) {
        recordSawEnd = true;
        recordFinish = true;
      }
    } else if (!options.recordDir.empty() && recordSawEnd) {
      recordFinish = true;
    }
    if (!options.recordDir.empty() && recordSawEnd && game.scene == Scene::Playing &&
        recordBoardFrame > 0) {
      recordFinish = true;   // the restart after the fall
    }
    if (recordFinish) {
      setAudioCapture(audio, false);
      const bool wroteAudio = writeAudioCapture(audio, options.recordDir + "/audio.wav");
      std::cout << "Recorded " << recordedFrames << " frames ("
                << recordBoardFrame / static_cast<int>(kBoardRefresh) << " s) to "
                << options.recordDir << (wroteAudio ? " with audio.wav" : "") << '\n';
      running = false;
    }
    SDL_RenderSetScale(renderer, 1.0F, 1.0F);
    SDL_SetRenderTarget(renderer, nullptr);
    setColor(renderer, color(0, 0, 0));
    SDL_RenderClear(renderer);
    SDL_RenderCopyEx(renderer, surface.texture, nullptr, &surface.destination,
                     static_cast<double>(rotation), nullptr,
                     SDL_FLIP_NONE);
    if (!options.recordDir.empty()) {
      // The clock of the clip, on the window only -- the frames were read
      // off the render target above, so it never lands in the recording.
      const int tenths = recordBoardFrame * 10 / static_cast<int>(kBoardRefresh);
      std::array<char, 32> clock{};
      std::snprintf(clock.data(), clock.size(), "REC %d.%d", tenths / 10, tenths % 10);
      const float sx = static_cast<float>(surface.destination.w) / kWorldWidth;
      const float sy = static_cast<float>(surface.destination.h) / kWorldHeight;
      SDL_RenderSetScale(renderer, sx, sy);
      const float ox = static_cast<float>(surface.destination.x) / sx;
      const float oy = static_cast<float>(surface.destination.y) / sy;
      fillRect(renderer, ox + 8.0F, oy + 8.0F, 118.0F, 26.0F, color(0, 0, 0, 180));
      drawText(renderer, clock.data(), ox + 14.0F, oy + 13.0F, 2.4F,
               game.scene == Scene::Playing ? color(255, 60, 60) : color(255, 220, 80));
      SDL_RenderSetScale(renderer, 1.0F, 1.0F);
    }
    if (game.charlieScaleNotice > 0) {
      --game.charlieScaleNotice;
      std::array<char, 40> note{};
      std::snprintf(note.data(), note.size(), "CHARLIE SIZE %d%%",
                    static_cast<int>(std::lround(game.charlieScale * 100.0F)));
      const float sx = static_cast<float>(surface.destination.w) / kWorldWidth;
      const float sy = static_cast<float>(surface.destination.h) / kWorldHeight;
      SDL_RenderSetScale(renderer, sx, sy);
      const float ox = static_cast<float>(surface.destination.x) / sx;
      const float oy = static_cast<float>(surface.destination.y) / sy;
      fillRect(renderer, ox + 8.0F, oy + 40.0F, 236.0F, 26.0F, color(0, 0, 0, 180));
      drawText(renderer, note.data(), ox + 14.0F, oy + 45.0F, 2.4F, color(255, 220, 80));
      SDL_RenderSetScale(renderer, 1.0F, 1.0F);
    }
    if (!options.riderDiagnosticDir.empty() && movementTraceFrame > 0) {
      const int tracedFrame = movementTraceFrame - 1;
      const std::array<int, 12> captureFrames{
          0, 6, 14, 21, 25, 33, 57, 73, 87, 88, 96, 103};
      const bool wanted =
          options.riderDiagnosticFrames.empty()
              ? std::find(captureFrames.begin(), captureFrames.end(),
                          tracedFrame) != captureFrames.end()
              : std::find(options.riderDiagnosticFrames.begin(),
                          options.riderDiagnosticFrames.end(),
                          tracedFrame) != options.riderDiagnosticFrames.end();
      if (tracedFrame != lastRiderDiagnosticFrame && wanted) {
        const int mameFrame = tracedFrame + 1322;
        std::ostringstream filename;
        filename << options.riderDiagnosticDir << "/native-frame-"
                 << std::setw(3) << std::setfill('0') << tracedFrame
                 << "-mame-" << mameFrame << ".png";
        if (!captureRenderer(renderer, filename.str())) {
          std::cerr << "Rider diagnostic capture failed: "
                    << IMG_GetError() << '\n';
        }
        lastRiderDiagnosticFrame = tracedFrame;
      }
    }
    if (replaying && !options.replayCaptureDir.empty() && replayFrame > 0 &&
        std::find(options.replayCaptureFrames.begin(),
                  options.replayCaptureFrames.end(), replayFrame - 1) !=
            options.replayCaptureFrames.end()) {
      std::ostringstream filename;
      filename << options.replayCaptureDir << "/replay-" << std::setw(5)
               << std::setfill('0') << (replayFrame - 1) << "-mame-"
               << replayRows[replayIndex - 1].frame << ".png";
      captureRenderer(renderer, filename.str());
    }
    if (!options.capturePath.empty()) {
      if (!captureRenderer(renderer, options.capturePath)) {
        std::cerr << "Screenshot capture failed: " << IMG_GetError() << '\n';
      }
      running = false;
    }
    SDL_RenderPresent(renderer);

    SDL_SetWindowTitle(
        window,
        ("Circus Charlie HD | " + std::to_string(surface.width) + "x" +
         std::to_string(surface.height) + " render | " +
         std::to_string(static_cast<int>(std::lround(kBoardRefresh))) + " Hz")
            .c_str());
  }

  if (!saveHighScore(highScorePath, game.highScore)) {
    std::cerr << "High score could not be saved.\n";
  }
  if (surface.texture) SDL_DestroyTexture(surface.texture);
  if (controller) SDL_GameControllerClose(controller);
  destroyAudio(audio);
  destroyAssets(assets);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  IMG_Quit();
  SDL_Quit();
  return 0;
}
