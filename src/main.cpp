#include <SDL.h>
#include <SDL_image.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
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
constexpr float kStage2RopeY = 382.0F;
constexpr float kStage2MonkeySpeed = 52.0F;
constexpr float kStage2PurpleSpeed = 61.0F;
constexpr float kStage2GoalTopY = kStage2RopeY - 76.0F;
constexpr float kStage2GoalX = 5700.0F;
constexpr float kStage3GroundY = 592.0F;
constexpr float kStage3TambourineTopY = 482.0F;
// The bag's painted bottom clears bounce two but overlaps Charlie's hands and
// head on bounce three, matching the collection rule visually.
constexpr float kStage3PrizeBagY = kStage3TambourineTopY - 236.0F;
constexpr float kStage3TambourineSpacing = 180.0F;
// The arcade opening frames keep Charlie on the left drum of one centered
// pair.  Holding that drum at x=150 leaves its partner at x=330.
constexpr float kStage3PlayerScreenX = 150.0F;
// The arcade stops scrolling on the final approach. The last tambourine
// remains against the right edge while Charlie crosses the final two gaps.
constexpr float kStage3GoalScreenX = 390.0F;
constexpr float kStage3FirstTambourineX = kStage3PlayerScreenX;
constexpr float kStage3CourseLength =
    kStage3FirstTambourineX + 24.0F * kStage3TambourineSpacing;
// A slightly quicker rebound keeps Charlie buoyant without changing any of
// the ROM-calibrated apex heights or collision windows.
constexpr int kStage3BounceFrames = 44;
// Transfers are ten percent quicker than a stationary rebound, while still
// using one continuous center-to-center arc.
constexpr int kStage3TransferFrames = 40;
constexpr int kStage3DirectionWindowFrames = 8;
// Trigger height for the fourth rebound. The ROM then removes the arena
// sprite and presents the separate roof-burst tile in the fixed marquee.
constexpr float kStage3RoofImpactY = 251.0F;
constexpr float kStage4BallCenterY = 526.0F;
constexpr float kStage4BallRadius = 40.0F;
constexpr float kStage4CharlieBaselineY = kStage4BallCenterY - kStage4BallRadius;
constexpr float kStage4PlayerScreenX = 118.0F;
constexpr float kStage4GoalScreenX = 396.0F;
constexpr float kStage4CourseLength = 6320.0F;
constexpr float kStage4GoalTopY = kStage4CharlieBaselineY + 18.0F;

float stage3CameraFor(float playerWorldX) {
  const float followingCamera =
      std::max(0.0F, playerWorldX - kStage3PlayerScreenX);
  const float finalCamera =
      std::max(0.0F, kStage3CourseLength - kStage3GoalScreenX);
  return std::min(followingCamera, finalCamera);
}
// The original board places the ring tube directly below the crowd fascia
// (about source y=140), not at the top of the crowd.
constexpr float kTrackY = 350.0F;
constexpr float kBackSpeed = -150.0F;
constexpr float kForwardSpeed = 195.0F;
constexpr float kRingRailSpeed = 65.0F;
constexpr float kRingActivationLead = 900.0F;
constexpr float kCourseLength = 6000.0F;
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
constexpr float kLionCollisionLeft = 26.0F;
constexpr float kLionCollisionRight = 34.0F;
constexpr float kLionCollisionCenterOffset = 20.0F;
constexpr float kLionCollisionTop = 58.0F;
constexpr float kLionCollisionBottom = 7.0F;
constexpr float kFirePotCollisionHalfWidth = 32.0F;
constexpr float kFirePotClearance = 42.0F;
constexpr float kHoopPotSafetyDistance = 76.0F;
// The HD hoop source has about 50% transparent horizontal padding. A
// 108-unit destination produces the measured 46-unit visible MAME width.
constexpr float kBigRingVisualHalfWidth = 54.0F;
// Both ring types use the arcade board's narrow foreground collision plane.
// Testing the full visible width makes reverse crossings fail before the
// rider has actually reached the flame edge.
constexpr float kBigRingCollisionHalfWidth = 7.0F;
constexpr float kBonusRingCollisionHalfWidth = 3.5F;
// Measured from hoop-extra.avi frame 800 after correcting the original
// 224x256 board image to the game's 480x640 logical display: the prize hoop
// has a visible flame oval about 50x110, centred 170 units above the grass
// contact line. Keep its collision plane thinner than the flames.
constexpr float kBonusRingOpeningHalfHeight = 49.0F;
// The source cell has generous transparent padding on both axes. A 112x198
// target produces the measured 53x118 visible flame oval; using the visible
// measurements as the destination size makes the rendered hoop much smaller.
constexpr float kBonusRingVisualHalfWidth = 56.0F;
constexpr float kBonusRingVisualHalfHeight = 99.0F;
constexpr float kBonusRingCenterHeight = 170.0F;
// The MAME sequence places the coin near its apex about 50 frames after the
// launch and catches it on the descending half at frame 66. A 96-frame arc
// matches both measurements and still returns visibly to the pot when missed.
constexpr int kCoinFlightFrames = 96;
constexpr float kCoinArcHeight = 170.0F;
constexpr int kCrashBurnFrames = 72;
constexpr int kGoalArrivalFrames = 90;
constexpr int kBirdArrivalFrames = 170;
constexpr int kBagDropFrames = 45;
constexpr int kCoinShowerFrames = 220;
constexpr int kRewardCoinCount = 18;
// Preserve the measured arcade cadence. This was briefly reduced to 0.85,
// which made the lion's full stride cycle 15% slower than normal.
constexpr float kStrideAnimationSpeedScale = 1.00F;
constexpr int kDefaultHighScore = 19830;
constexpr int kFirstScoreLife = 20000;
constexpr int kRecurringScoreLife = 70000;
constexpr int kEventCount = 6;
constexpr int kEventColumns = 3;

constexpr float railStartForIntercept(float playerWorldX) {
  // Rings begin travelling once they are kRingActivationLead units ahead of
  // the camera. Compensate their start position so a full-speed player still
  // meets each one at the authored course coordinate.
  return playerWorldX +
         (kRingActivationLead - 78.0F) * kRingRailSpeed /
             (kForwardSpeed + kRingRailSpeed);
}

struct Options {
  int width = 480;
  int height = 640;
  int rotation = 0;
  bool fullscreen = false;
  bool debug = false;
  bool lionTest = false;
  bool showHelp = false;
  std::string capturePath;
  std::string captureScene = "gameplay";
};

struct Vec2 {
  float x = 0.0F;
  float y = 0.0F;
};

struct Hoop {
  float worldX = 0.0F;
  float openingBottom = kGroundY - 20.0F;
  float openingTop = kGroundY - 154.0F;
  bool cleared = false;
  float previousWorldX = 0.0F;
};

struct FirePot {
  float worldX = 0.0F;
  bool coinChanceResolved = false;
  bool coinPending = false;
  bool coinActive = false;
  bool coinCollected = false;
  int coinFrame = 0;
  bool retired = false;
  bool scored = false;
};

struct BonusRing {
  float worldX = 0.0F;
  float height = 0.0F;
  bool collected = false;
  bool containsPrize = false;
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

struct Stage3Tambourine {
  float worldX = 0.0F;
  bool bagAvailable = false;
  int compressionFrame = 0;
};

enum class Stage3PerformerKind {
  KnifeThrower,
  FlameThrower,
};

struct Stage3Performer {
  float worldX = 0.0F;
  Stage3PerformerKind kind = Stage3PerformerKind::KnifeThrower;
  int actionFrame = 0;
};

struct Stage3Projectile {
  Vec2 position{};
  Vec2 velocity{};
  Stage3PerformerKind kind = Stage3PerformerKind::KnifeThrower;
  int age = 0;
  bool active = false;
};

struct Stage4Ball {
  float worldX = 0.0F;
  float velocity = 0.0F;
  float rotation = 0.0F;
};

struct Player {
  Vec2 position{78.0F, kGroundY};
  Vec2 previous = position;
  float verticalVelocity = 0.0F;
  float runSpeed = 0.0F;
  int jumpFrame = -1;
  bool grounded = true;
  bool alive = true;
  bool facingRight = true;
};

enum class Scene {
  Title,
  EventSelect,
  Playing,
  Crashed,
  Goal,
  Tally,
  Complete,
};

struct Game {
  Scene scene = Scene::Title;
  Player player;
  std::vector<Hoop> hoops;
  std::vector<FirePot> firePots;
  std::vector<BonusRing> bonusRings;
  std::vector<MeterMarker> meterMarkers;
  std::vector<Stage2Monkey> stage2Monkeys;
  std::vector<Stage3Tambourine> stage3Tambourines;
  std::vector<Stage3Performer> stage3Performers;
  std::vector<Stage3Projectile> stage3Projectiles;
  std::vector<Stage4Ball> stage4Balls;
  float cameraX = 0.0F;
  float previousCameraX = 0.0F;
  int score = 0;
  int highScore = kDefaultHighScore;
  int credits = 0;
  int lives = 3;
  int selectedEvent = 0;
  int eventSelectFrame = 0;
  int eventSelectDurationFrames =
      static_cast<int>(17.0 * kBoardRefresh);
  int bonus = 6000;
  int nextScoreLife = kFirstScoreLife;
  int goalFrame = 0;
  int tallyFrame = 0;
  int crashFrame = 0;
  int clearBonus = 0;
  int rewardCoinsAwarded = 0;
  int prizeBagsAvailable = 0;
  int prizeBagsCollected = 0;
  bool deathOccurred = false;
  bool perfectClear = false;
  bool hiddenCoinTriggered = false;
  bool timeScoreApplied = false;
  int openingBackwardJumps = 0;
  bool extraCharlieActive = false;
  bool extraCharlieCollected = false;
  bool extraCharlieTriggered = false;
  int extraCharlieHoopIndex = -1;
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
  int stage3BounceLevel = 0;
  int stage3BounceFrame = 0;
  float stage3BounceBaseY = kStage3GroundY;
  bool stage3OnTambourine = false;
  int stage3CurrentTambourine = 0;
  int stage3TargetTambourine = 0;
  bool stage3Traveling = false;
  bool stage3DirectionArmed = true;
  int stage3TravelStartFrame = 0;
  float stage3TravelStartX = 78.0F;
  bool stage3RoofCrash = false;
  int stage4CurrentBall = 0;
  bool stage4Airborne = false;
  bool stage4PinnedCrash = false;
  int stage4FallFrame = 0;
  int stage4RespawnGraceFrames = 0;
  std::uint32_t jumpAudioSerial = 0;
  std::uint32_t crashAudioSerial = 0;
  std::uint32_t extraCharlieAudioSerial = 0;
  std::uint32_t prizeBagAudioSerial = 0;
  std::uint32_t hiddenCoinAudioSerial = 0;
  std::uint32_t coinAudioSerial = 0;
  std::uint32_t eventSelectMoveAudioSerial = 0;
  std::uint32_t eventSelectConfirmAudioSerial = 0;
  std::uint32_t stage3BounceAudioSerial = 0;
  std::uint32_t stage3OverjumpAudioSerial = 0;
  std::uint32_t randomState = 0x6d2b79f5U;
  bool highScoreDirty = false;
  bool debug = false;
  bool lionOnlyTest = false;
};

struct RenderSurface {
  SDL_Texture* texture = nullptr;
  int width = 0;
  int height = 0;
  SDL_Rect destination{0, 0, 0, 0};
};

struct Assets {
  SDL_Texture* arena = nullptr;
  SDL_Texture* marquee = nullptr;
  SDL_Texture* ferrisWheel = nullptr;
  SDL_Texture* ferrisGondola = nullptr;
  SDL_Texture* rider = nullptr;
  SDL_Texture* riderWalkTest = nullptr;
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
  SDL_Texture* stage3Charlie = nullptr;
  SDL_Texture* stage3CharlieTuck = nullptr;
  SDL_Texture* stage3CharlieVertical = nullptr;
  SDL_Texture* stage3CharlieRoofHead = nullptr;
  SDL_Texture* stage3Tambourine = nullptr;
  SDL_Texture* stage3GoalTambourine = nullptr;
  SDL_Texture* stage3KnifeThrower = nullptr;
  SDL_Texture* stage3FlameThrower = nullptr;
  SDL_Texture* stage3Projectiles = nullptr;
  SDL_Texture* stage3FlameProjectile = nullptr;
  SDL_Texture* stage4Charlie = nullptr;
  SDL_Texture* stage4Ball = nullptr;
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
  AudioClip stage3Bounce;
  AudioClip jump;
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
};

std::string highScoreMemoryPath() {
  char* preferencePath =
      SDL_GetPrefPath("BigTopRun", "BigTopRunNative");
  if (!preferencePath) return {};
  const std::string path =
      std::string(preferencePath) + "high-score.txt";
  SDL_free(preferencePath);
  return path;
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
      << "Big Top Run Native\n"
      << "  --mode WIDTHxHEIGHT\n"
      << "  --rotate 0|90|270\n"
      << "  --fullscreen\n"
      << "  --debug\n"
      << "  --lion-test\n"
      << "  --capture FILE.png\n"
      << "  --capture-scene start|select|layout|large|prize|gameplay|stage2|stage2-goal|stage3|stage3-transfer|stage3-approach|stage3-goal|stage3-roof|stage4|stage4-jump|stage4-fall|stage4-goal|ring|extra|crash|goal|tally\n";
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
    } else if (argument == "--rotate" && index + 1 < argc) {
      options.rotation = std::atoi(argv[++index]);
      if (options.rotation != 0 && options.rotation != 90 &&
          options.rotation != 270) {
        std::cerr << "Rotation must be 0, 90, or 270.\n";
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
  return options;
}

SDL_Color color(Uint8 red, Uint8 green, Uint8 blue, Uint8 alpha = 255) {
  return SDL_Color{red, green, blue, alpha};
}

void setColor(SDL_Renderer* renderer, SDL_Color value) {
  SDL_SetRenderDrawColor(renderer, value.r, value.g, value.b, value.a);
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

Assets loadAssets(SDL_Renderer* renderer) {
  Assets assets;
  assets.arena = loadAsset(renderer, "stage1-arena-green.png");
  assets.marquee = loadAsset(renderer, "stage1-marquee-v2.png");
  assets.ferrisWheel = loadAsset(renderer, "stage1-ferris-wheel.png");
  assets.ferrisGondola = loadAsset(renderer, "stage1-ferris-gondola.png");
  assets.rider = loadAsset(renderer, "stage1-rider-sheet-v8.png");
  assets.riderWalkTest =
      loadAsset(renderer, "stage1-rider-walk-12-v9.png");
  assets.burnRider = loadAsset(renderer, "stage1-burn-rider-v1.png");
  assets.hoop = loadAsset(renderer, "stage1-hoop.png");
  assets.hoopFlare = loadAsset(renderer, "stage1-hoop-flare.png");
  assets.props = loadAsset(renderer, "stage1-props.png");
  assets.propsFlare = loadAsset(renderer, "stage1-props-flare.png");
  assets.bird = loadAsset(renderer, "stage1-bird-sheet.png");
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
  assets.stage2BrownWalk =
      loadAsset(renderer, "stage2-brown-walk-v1.png");
  assets.stage2PurpleWalk =
      loadAsset(renderer, "stage2-purple-walk-v1.png");
  assets.stage2PurpleJump =
      loadAsset(renderer, "stage2-purple-jump-v1.png");
  assets.stage2GoalRig =
      loadAsset(renderer, "stage2-goal-rig-v1.png");
  assets.stage3Charlie =
      loadAsset(renderer, "stage3-charlie-bounce-12-v1.png");
  assets.stage3CharlieTuck =
      loadAsset(renderer, "stage3-charlie-wam-tuck-hd-v1.png");
  assets.stage3CharlieVertical =
      loadAsset(renderer, "stage3-charlie-vertical-front-12-v2.png");
  assets.stage3CharlieRoofHead =
      loadAsset(renderer, "stage3-charlie-roof-head-4-v2.png");
  assets.stage3Tambourine =
      loadAsset(renderer, "stage3-tambourine-v1.png");
  assets.stage3GoalTambourine =
      loadAsset(renderer, "stage3-goal-tambourine-v1.png");
  assets.stage3KnifeThrower =
      loadAsset(renderer, "stage3-knife-thrower-12-v3.png");
  assets.stage3FlameThrower =
      loadAsset(renderer, "stage3-fire-breather-vertical-12-v3.png");
  assets.stage3Projectiles =
      loadAsset(renderer, "stage3-projectiles-8-v1.png");
  assets.stage3FlameProjectile =
      loadAsset(renderer, "stage3-flame-projectile-4-v3.png");
  assets.stage4Charlie = loadAsset(renderer, "stage4-charlie-12-v2.png");
  assets.stage4Ball = loadAsset(renderer, "stage4-ball-centered-v2.png");
  if (!assets.arena || !assets.marquee || !assets.ferrisWheel ||
      !assets.ferrisGondola || !assets.rider || !assets.riderWalkTest ||
      !assets.burnRider || !assets.hoop ||
      !assets.hoopFlare || !assets.props || !assets.propsFlare ||
      !assets.bird || !assets.rewardBag || !assets.charlieLife ||
      !assets.extraCharlie ||
      !assets.goalPlatform || !assets.finishRider ||
      !assets.eventSelectProps || !assets.eventSelectChosen ||
      !assets.stage2Charlie || !assets.stage2BrownWalk ||
      !assets.stage2PurpleWalk || !assets.stage2PurpleJump ||
      !assets.stage2GoalRig || !assets.stage3Charlie ||
      !assets.stage3CharlieTuck ||
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
  if (assets.arena) SDL_DestroyTexture(assets.arena);
  if (assets.marquee) SDL_DestroyTexture(assets.marquee);
  if (assets.ferrisWheel) SDL_DestroyTexture(assets.ferrisWheel);
  if (assets.ferrisGondola) SDL_DestroyTexture(assets.ferrisGondola);
  if (assets.rider) SDL_DestroyTexture(assets.rider);
  if (assets.riderWalkTest) SDL_DestroyTexture(assets.riderWalkTest);
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
  if (assets.stage2BrownWalk) SDL_DestroyTexture(assets.stage2BrownWalk);
  if (assets.stage2PurpleWalk) SDL_DestroyTexture(assets.stage2PurpleWalk);
  if (assets.stage2PurpleJump) SDL_DestroyTexture(assets.stage2PurpleJump);
  if (assets.stage2GoalRig) SDL_DestroyTexture(assets.stage2GoalRig);
  if (assets.stage3Charlie) SDL_DestroyTexture(assets.stage3Charlie);
  if (assets.stage3CharlieTuck)
    SDL_DestroyTexture(assets.stage3CharlieTuck);
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
}

bool loadAudio(AudioEngine& audio) {
  const bool loaded =
      loadAudioAsset("event1-stage.wav", audio.stageMusic) &&
      loadAudioAsset("event2-stage.wav", audio.stage2Music) &&
      loadAudioAsset("event3-stage.wav", audio.stage3Music) &&
      loadAudioAsset("stage3-bounce.wav", audio.stage3Bounce) &&
      loadAudioAsset("jump.wav", audio.jump) &&
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

  if (!matchesReference(audio.jump) || !matchesReference(audio.miss) ||
      !matchesReference(audio.missTwo) ||
      !matchesReference(audio.crowdCheer) ||
      !matchesReference(audio.birdCoinDrop) ||
      !matchesReference(audio.bonusCount) ||
      !matchesReference(audio.eventSelectMusic) ||
      !matchesReference(audio.eventSelectMove) ||
      !matchesReference(audio.eventSelectConfirm) ||
      !matchesReference(audio.stage2Music) ||
      !matchesReference(audio.stage3Music) ||
      !matchesReference(audio.stage3Bounce) ||
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
       musicVoice.clip == &audio.stage3Music)) {
    musicVoice.playbackStep = fast ? 2U : 1U;
  }
  SDL_UnlockAudioDevice(audio.device);
}

void playStageMusic(AudioEngine& audio, int selectedEvent, bool fast) {
  const AudioClip& music =
      selectedEvent == 1 ? audio.stage2Music
                         : (selectedEvent == 2 ? audio.stage3Music
                                               : audio.stageMusic);
  setAudioVoice(audio, 0, music,
                static_cast<int>(SDL_MIX_MAXVOLUME * 0.58F), true);
  setStageMusicFast(audio, fast);
}

void playEventSelectMusic(AudioEngine& audio) {
  setAudioVoice(audio, 0, audio.eventSelectMusic,
                static_cast<int>(SDL_MIX_MAXVOLUME * 0.66F), false);
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

void playMissSounds(AudioEngine& audio) {
  setAudioVoice(audio, 2, audio.miss,
                static_cast<int>(SDL_MIX_MAXVOLUME * 0.90F), false);
  setAudioVoice(audio, 3, audio.missTwo,
                static_cast<int>(SDL_MIX_MAXVOLUME * 0.90F), false);
}

void playCrowdCheer(AudioEngine& audio) {
  setAudioVoice(audio, 4, audio.crowdCheer,
                static_cast<int>(SDL_MIX_MAXVOLUME * 0.86F), false);
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

void playStage3OverjumpSound(AudioEngine& audio) {
  setAudioVoice(audio, 1, audio.miss,
                static_cast<int>(SDL_MIX_MAXVOLUME * 0.92F), false);
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
  if (audio.stage3Bounce.data) SDL_FreeWAV(audio.stage3Bounce.data);
  if (audio.jump.data) SDL_FreeWAV(audio.jump.data);
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

std::uint32_t nextRandom(Game& game) {
  std::uint32_t value = game.randomState;
  value ^= value << 13;
  value ^= value >> 17;
  value ^= value << 5;
  game.randomState = value;
  return value;
}

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
  game.deathOccurred = false;
  game.perfectClear = false;
  game.hiddenCoinTriggered = false;
  game.timeScoreApplied = false;
  game.openingBackwardJumps = 0;
  game.extraCharlieActive = false;
  game.extraCharlieCollected = false;
  game.extraCharlieTriggered = false;
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
  game.stage3BounceLevel = 0;
  game.stage3BounceFrame = 0;
  game.stage3BounceBaseY = kStage3GroundY;
  game.stage3OnTambourine = false;
  game.stage3CurrentTambourine = 0;
  game.stage3TargetTambourine = 0;
  game.stage3Traveling = false;
  game.stage3DirectionArmed = true;
  game.stage3TravelStartFrame = 0;
  game.stage3TravelStartX = 78.0F;
  game.stage3RoofCrash = false;
  game.stage3Tambourines.clear();
  game.stage3Performers.clear();
  game.stage3Projectiles.clear();
  game.stage4Balls.clear();
  game.stage4CurrentBall = 0;
  game.stage4Airborne = false;
  game.stage4PinnedCrash = false;
  game.stage4FallFrame = 0;
  game.stage4RespawnGraceFrames = 0;

  if (game.selectedEvent == 1) {
    game.player.position = {78.0F, kStage2RopeY};
    game.player.previous = game.player.position;
    game.hoops.clear();
    game.firePots.clear();
    game.bonusRings.clear();
    game.meterMarkers = {
        {780.0F, 50}, {1740.0F, 40}, {2700.0F, 30},
        {3660.0F, 20}, {4620.0F, 10},
    };
    // Authored from the Event 2 frame sequence. Brown monkeys form the
    // walking line; faster purple monkeys begin behind selected groups and
    // leapfrog them with their distinct ROM animation family.
    game.stage2Monkeys = {
        {650.0F, Stage2MonkeyKind::Brown},
        {1020.0F, Stage2MonkeyKind::Brown},
        {1090.0F, Stage2MonkeyKind::Brown},
        {1510.0F, Stage2MonkeyKind::Brown},
        {1600.0F, Stage2MonkeyKind::Purple},
        {1980.0F, Stage2MonkeyKind::Brown},
        {2065.0F, Stage2MonkeyKind::Purple},
        {2400.0F, Stage2MonkeyKind::Brown},
        {2470.0F, Stage2MonkeyKind::Brown},
        {2820.0F, Stage2MonkeyKind::Brown},
        {2910.0F, Stage2MonkeyKind::Purple},
        {3270.0F, Stage2MonkeyKind::Brown},
        {3340.0F, Stage2MonkeyKind::Brown},
        {3430.0F, Stage2MonkeyKind::Purple},
        {3860.0F, Stage2MonkeyKind::Brown},
        {3945.0F, Stage2MonkeyKind::Purple},
        {4280.0F, Stage2MonkeyKind::Brown},
        {4350.0F, Stage2MonkeyKind::Brown},
        {4440.0F, Stage2MonkeyKind::Purple},
        {4830.0F, Stage2MonkeyKind::Brown},
        {4900.0F, Stage2MonkeyKind::Brown},
        {4980.0F, Stage2MonkeyKind::Brown},
        {5070.0F, Stage2MonkeyKind::Purple},
        {5410.0F, Stage2MonkeyKind::Brown},
        {5500.0F, Stage2MonkeyKind::Purple},
    };
    return;
  }

  if (game.selectedEvent == 2) {
    game.player.position = {78.0F, kStage3TambourineTopY};
    game.player.previous = game.player.position;
    game.player.grounded = false;
    game.player.jumpFrame = 0;
    game.stage3BounceFrame = 0;
    game.stage3BounceLevel = 1;
    game.stage3BounceBaseY = kStage3TambourineTopY;
    game.stage3OnTambourine = true;
    game.stage3CurrentTambourine = 0;
    game.stage3TargetTambourine = 0;
    game.stage3Traveling = false;
    game.stage3DirectionArmed = true;
    game.stage3TravelStartFrame = 0;
    game.stage3TravelStartX = game.player.position.x;
    game.stage3RoofCrash = false;
    game.hoops.clear();
    game.firePots.clear();
    game.bonusRings.clear();
    game.stage2Monkeys.clear();
    game.meterMarkers = {
        {780.0F, 50}, {1500.0F, 40}, {2220.0F, 30},
        {2940.0F, 20}, {3660.0F, 10},
    };
    // Event 3's drums are tall leather cylinders, not flattened versions of
    // the Event 1 goal pad. Their measured spacing leaves a clear performer
    // lane between successive bounce targets.
    game.stage3Tambourines.reserve(25);
    for (int index = 0; index < 25; ++index) {
      const bool bag = index > 0 && index < 24 && index % 4 == 3;
      game.stage3Tambourines.push_back(
          {kStage3FirstTambourineX +
               static_cast<float>(index) * kStage3TambourineSpacing,
           bag});
      if (bag) ++game.prizeBagsAvailable;
    }
    // The ROM frames place each act exactly halfway between its surrounding
    // drums. The opening gap remains clear; acts then alternate every other
    // gap through the final approach.
    game.stage3Performers.reserve(12);
    for (int index = 0; index < 12; ++index) {
      game.stage3Performers.push_back({
          kStage3FirstTambourineX + 1.5F * kStage3TambourineSpacing +
              static_cast<float>(index) *
                  (kStage3TambourineSpacing * 2.0F),
          (index & 1) == 0 ? Stage3PerformerKind::KnifeThrower
                           : Stage3PerformerKind::FlameThrower});
      // Keep every lane on a deterministic but staggered 150-frame cycle.
      // The ROM does not wait for a hazard to enter the camera before it
      // starts its toss; approaching players must read the existing rhythm.
      game.stage3Performers.back().actionFrame =
          static_cast<int>((index * 37U) % 150U);
    }
    game.stage3Projectiles.resize(game.stage3Performers.size());
    return;
  }

  if (game.selectedEvent == 3) {
    game.hoops.clear();
    game.firePots.clear();
    game.bonusRings.clear();
    game.stage2Monkeys.clear();
    game.player.position = {kStage4PlayerScreenX, kStage4CharlieBaselineY};
    game.player.previous = game.player.position;
    game.player.grounded = true;
    game.player.runSpeed = 82.0F;
    game.player.facingRight = true;
    game.stage4RespawnGraceFrames = 90;
    game.meterMarkers = {
        {kStage4PlayerScreenX, -1}, {500.0F, 60},
        {1150.0F, 50}, {2190.0F, 40},
        {3230.0F, 30}, {4270.0F, 20}, {5310.0F, 10},
    };
    // The arcade object stream uses a few repeating separations rather than
    // the steadily shrinking provisional layout. These values are scaled
    // from the supplied native 224-pixel Stage 4 capture; the velocities are
    // likewise converted from the board's per-frame object motion.
    constexpr std::array<float, 24> gaps{
        258, 226, 286, 214, 254, 224, 278, 218,
        246, 232, 272, 216, 258, 224, 282, 220,
        248, 230, 270, 218, 256, 226, 276, 222};
    constexpr std::array<float, 6> incomingSpeeds{
        -58.0F, -66.0F, -54.0F, -62.0F, -56.0F, -68.0F};
    float x = kStage4PlayerScreenX;
    game.stage4Balls.push_back({x, 82.0F, 0.0F});
    for (std::size_t index = 0; index < gaps.size(); ++index) {
      x += gaps[index];
      // Unridden balls enter from the right and roll left toward Charlie in
      // the recorded board sequence. Their staggered speeds create the
      // bunching and separation patterns visible in the arcade footage.
      const float drift = incomingSpeeds[index % incomingSpeeds.size()];
      game.stage4Balls.push_back({x, drift, static_cast<float>(index * 29U)});
    }
    // Keep the final approach fixed like the arcade board: the last rolling
    // ball leads into a stationary goal at the right edge instead of letting
    // the camera carry the finish platform across the whole screen.
    game.stage4Balls.back().worldX = kStage4CourseLength - 165.0F;
    game.prizeBagsAvailable = 0;
    return;
  }

  game.stage2Monkeys.clear();

  // Event 1 is laid out in the same difficulty progression visible in the
  // recorded 60M-to-GOAL run: two introductory rings, alternating floor fire
  // and moving rings, a close double-ring test, then the final 10M gauntlet.
  // Moving obstacles start farther ahead so their measured 65-unit rail motion
  // meets a lion running near the measured 195-unit forward speed.
  game.hoops = {
      {railStartForIntercept(620.0F), kBigHoopOpeningBottom,
       kBigHoopOpeningTop, false},
      {railStartForIntercept(1040.0F), kBigHoopOpeningBottom,
       kBigHoopOpeningTop, false},
      {railStartForIntercept(1780.0F), kBigHoopOpeningBottom,
       kBigHoopOpeningTop, false},
      {railStartForIntercept(2260.0F), kBigHoopOpeningBottom,
       kBigHoopOpeningTop, false},
      {railStartForIntercept(2700.0F), kBigHoopOpeningBottom,
       kBigHoopOpeningTop, false},
      {railStartForIntercept(3940.0F), kBigHoopOpeningBottom,
       kBigHoopOpeningTop, false},
      {railStartForIntercept(3990.0F), kBigHoopOpeningBottom,
       kBigHoopOpeningTop, false},
      {railStartForIntercept(4480.0F), kBigHoopOpeningBottom,
       kBigHoopOpeningTop, false},
      {railStartForIntercept(5050.0F), kBigHoopOpeningBottom,
       kBigHoopOpeningTop, false},
      {railStartForIntercept(5650.0F), kBigHoopOpeningBottom,
       kBigHoopOpeningTop, false},
  };
  for (auto& hoop : game.hoops) hoop.previousWorldX = hoop.worldX;
  game.firePots = {
      {1560.0F},
      {2040.0F},
      {2470.0F},
      {2920.0F},
      {3300.0F},
      {3650.0F},
      // The delayed second member of the close double-hoop set occupies the
      // old 4260 lane at common play speeds. Leaving that pot in place creates
      // the impossible hoop-over-fire combination seen in playtesting.
      {4860.0F},
      // The original final approach puts one pot beneath the last hoop and a
      // second pot almost against the goal platform. At the native fixed-jump
      // distance, these centers permit two distinct jumps, with the second
      // jump descending directly onto the padded platform.
      {5650.0F},
      {5870.0F},
  };
  game.bonusRings = {
      {railStartForIntercept(1360.0F), kBonusRingCenterHeight, false, false},
      {railStartForIntercept(3100.0F), kBonusRingCenterHeight, false, false},
      {railStartForIntercept(3500.0F), kBonusRingCenterHeight, false, false},
      {railStartForIntercept(4650.0F), kBonusRingCenterHeight, false, false},
      {railStartForIntercept(5250.0F), kBonusRingCenterHeight, false, false},
  };
  game.meterMarkers = {
      {620.0F, 60},  {1480.0F, 50}, {2340.0F, 40},
      {3200.0F, 30}, {4060.0F, 20}, {4920.0F, 10},
  };
  int prizeCount = 0;
  for (auto& ring : game.bonusRings) {
    ring.containsPrize = nextRandom(game) % 5U < 3U;
    if (ring.containsPrize) ++prizeCount;
  }
  if (prizeCount == 0) {
    game.bonusRings.front().containsPrize = true;
    prizeCount = 1;
  } else if (prizeCount == static_cast<int>(game.bonusRings.size())) {
    game.bonusRings.back().containsPrize = false;
    --prizeCount;
  }
  game.prizeBagsAvailable = prizeCount;
}

void startGame(Game& game) {
  game.scene = Scene::Playing;
  game.lives = 3;
  resetCourse(game);
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
  if (game.credits <= 0) {
    game.scene = Scene::Title;
    return;
  }
  if (game.selectedEvent > 3) game.selectedEvent = 0;
  ++game.eventSelectConfirmAudioSerial;
  --game.credits;
  startGame(game);
}

void moveEventSelection(Game& game, int direction) {
  if (game.scene != Scene::EventSelect || direction == 0) return;
  const int oldEvent = game.selectedEvent;
  const int step = direction < 0 ? -1 : 1;
  game.selectedEvent =
      (oldEvent + step + kEventCount) % kEventCount;
  ++game.eventSelectMoveAudioSerial;
}

void insertCoin(Game& game) {
  if (game.credits >= 99) return;
  ++game.credits;
  ++game.coinAudioSerial;
}

bool startWithCredit(Game& game) {
  if (game.credits <= 0) return false;
  enterEventSelect(game);
  return true;
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
    game.scene = Scene::Title;
    return;
  }
  game.scene = Scene::Playing;
  game.player.position.x = std::max(78.0F, game.player.position.x - 145.0F);
  game.player.position.y = game.selectedEvent == 1
                               ? kStage2RopeY
                               : (game.selectedEvent == 2
                                      ? kStage3TambourineTopY
                                      : (game.selectedEvent == 3
                                             ? kStage4CharlieBaselineY
                                             : kGroundY));
  game.player.previous = game.player.position;
  game.player.verticalVelocity = 0.0F;
  game.player.runSpeed = 0.0F;
  game.player.jumpFrame = -1;
  game.player.grounded = true;
  game.player.alive = true;
  game.crashFrame = 0;
  game.cameraX = std::max(0.0F, game.player.position.x - 78.0F);
  game.previousCameraX = game.cameraX;
  if (game.selectedEvent == 2) {
    game.player.grounded = false;
    game.stage3CurrentTambourine = std::clamp(
        game.stage3CurrentTambourine, 0,
        static_cast<int>(game.stage3Tambourines.size()) - 1);
    game.stage3TargetTambourine = game.stage3CurrentTambourine;
    game.player.position.x =
        game.stage3Tambourines[static_cast<std::size_t>(
            game.stage3CurrentTambourine)].worldX;
    game.player.previous = game.player.position;
    game.stage3BounceLevel = 1;
    game.stage3BounceFrame = 0;
    game.stage3BounceBaseY = kStage3TambourineTopY;
    game.stage3Traveling = false;
    game.stage3DirectionArmed = true;
    game.stage3TravelStartFrame = 0;
    game.stage3TravelStartX = game.player.position.x;
    game.stage3RoofCrash = false;
    game.cameraX = stage3CameraFor(game.player.position.x);
    game.previousCameraX = game.cameraX;
  }
  if (game.selectedEvent == 3 && !game.stage4Balls.empty()) {
    game.stage4CurrentBall = std::clamp(
        game.stage4CurrentBall, 0,
        static_cast<int>(game.stage4Balls.size()) - 1);
    auto& ball = game.stage4Balls[static_cast<std::size_t>(game.stage4CurrentBall)];
    game.player.position = {ball.worldX, kStage4CharlieBaselineY};
    game.player.previous = game.player.position;
    game.player.grounded = true;
    game.stage4Airborne = false;
    game.stage4PinnedCrash = false;
    game.stage4FallFrame = 0;
    game.stage4RespawnGraceFrames = 90;
    game.player.facingRight = true;
    // A squeeze failure can leave another rolling ball occupying the active
    // ball's spawn circle. The board grants a short restart window; move only
    // immediate overlaps out of that circle while preserving the live stream.
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
    game.cameraX = std::max(0.0F, ball.worldX - kStage4PlayerScreenX);
    game.previousCameraX = game.cameraX;
  }
}

int timeBonusFor(int bonus) {
  if (bonus >= 4500) return 10000;
  if (bonus >= 4000) return 5000;
  if (bonus >= 3500) return 4000;
  if (bonus >= 3000) return 3000;
  if (bonus >= 2500) return 2000;
  if (bonus >= 2000) return 1000;
  if (bonus >= 1500) return 800;
  if (bonus >= 1001) return 600;
  if (bonus >= 500) return 400;
  return 200;
}

bool overlapsHoop(const Player& player, const Hoop& hoop) {
  const float playerCenterX =
      player.position.x + kLionCollisionCenterOffset;
  const float playerLeft = playerCenterX - kLionCollisionLeft;
  const float playerRight = playerCenterX + kLionCollisionRight;
  const float hoopLeft = hoop.worldX - kBigRingCollisionHalfWidth;
  const float hoopRight = hoop.worldX + kBigRingCollisionHalfWidth;
  if (playerRight < hoopLeft || playerLeft > hoopRight) return false;

  // A reverse jump through an already-passed hoop is a deliberate arcade
  // secret, not a precision challenge. Because both the rider and rail hoop
  // travel left, their overlap can occur at either edge of the fixed jump
  // arc. Treat the complete airborne reverse arc as a valid crossing. Merely
  // walking backward into the flames remains fatal, and forward collision is
  // completely unchanged.
  if (!player.grounded && player.runSpeed < -20.0F) return false;

  const float playerTop = player.position.y - kLionCollisionTop;
  const float playerBottom = player.position.y - kLionCollisionBottom;
  const bool withinOpening =
      playerTop > hoop.openingTop + 3.0F &&
      playerBottom < hoop.openingBottom - 2.0F;
  return !withinOpening;
}

void crashPlayer(Game& game) {
  if (game.scene != Scene::Playing) return;
  for (auto& firePot : game.firePots) {
    firePot.coinPending = false;
    firePot.coinActive = false;
  }
  game.player.alive = false;
  game.player.runSpeed = 0.0F;
  game.player.verticalVelocity = 0.0F;
  game.scene = Scene::Crashed;
  game.crashFrame = 0;
  game.deathOccurred = true;
  ++game.crashAudioSerial;
  --game.lives;
}

void crashStage3Roof(Game& game) {
  if (game.scene != Scene::Playing) return;
  game.stage3RoofCrash = true;
  game.stage3Traveling = false;
  game.player.alive = false;
  game.player.position.y = kStage3RoofImpactY;
  game.player.previous = game.player.position;
  game.player.runSpeed = 0.0F;
  game.player.verticalVelocity = 0.0F;
  game.scene = Scene::Crashed;
  game.crashFrame = 0;
  game.deathOccurred = true;
  ++game.stage3OverjumpAudioSerial;
  --game.lives;
}

float firePotCoinY(const FirePot& firePot) {
  const float progress = std::clamp(
      static_cast<float>(firePot.coinFrame) /
          static_cast<float>(kCoinFlightFrames),
      0.0F, 1.0F);
  return kGroundY - 30.0F - std::sin(progress * kPi) * kCoinArcHeight;
}

void showStage1Score(Game& game, int points, float worldX, float y) {
  game.stage1ScorePopup = points;
  game.stage1ScorePopupFrame = 52;
  game.stage1ScorePopupWorldX = worldX;
  game.stage1ScorePopupY = y;
}

void finishStage(Game& game) {
  const bool stage2 = game.selectedEvent == 1;
  const bool stage3 = game.selectedEvent == 2;
  const bool stage4 = game.selectedEvent == 3;
  const float finishX = stage2 ? kStage2GoalX
                               : (stage3 ? kStage3CourseLength
                                         : (stage4 ? kStage4CourseLength
                                                   : kCourseLength));
  const float finishY = stage2 ? kStage2GoalTopY
                               : (stage3 ? kStage3TambourineTopY
                                         : (stage4 ? kStage4GoalTopY
                                                   : kGoalLandingY));
  game.player.position = {finishX, finishY};
  game.player.previous = game.player.position;
  game.player.runSpeed = 0.0F;
  game.player.verticalVelocity = 0.0F;
  game.player.jumpFrame = -1;
  game.player.grounded = true;
  game.cameraX = finishX -
      (stage2 ? 340.0F : (stage3 ? kStage3GoalScreenX
                                 : (stage4 ? kStage4GoalScreenX
                                           : kGoalScreenX)));
  game.previousCameraX = game.cameraX;
  game.perfectClear =
      !stage2 && !game.deathOccurred && game.prizeBagsAvailable > 0 &&
      game.prizeBagsCollected == game.prizeBagsAvailable;
  game.score += stage2 ? 5000 : (stage3 ? 3000 : (stage4 ? 4000 : 500));
  game.goalFrame = 0;
  if (stage3 && !game.stage3Tambourines.empty()) {
    // updateStage3 compresses a drum on the landing tick. Goal scenes no
    // longer run the stage update, so leaving this set freezes the final drum
    // below Charlie and creates the large floating gap seen in play tests.
    game.stage3Tambourines.back().compressionFrame = 0;
  }
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

float stage3BounceHeight(int level) {
  constexpr std::array<float, 4> heights{72.0F, 108.0F, 152.0F, 245.0F};
  return heights[static_cast<std::size_t>(std::clamp(level, 1, 4) - 1)];
}

void updateStage3(Game& game, const Uint8* keyboard, bool,
                  float controllerAxis) {
  game.player.previous = game.player.position;
  game.previousCameraX = game.cameraX;
  if (game.stage3Tambourines.empty()) return;

  for (auto& drum : game.stage3Tambourines) {
    if (drum.compressionFrame > 0) --drum.compressionFrame;
  }

  const bool moveLeft = keyboard[SDL_SCANCODE_LEFT] ||
                        keyboard[SDL_SCANCODE_A] ||
                        controllerAxis < -0.35F;
  const bool moveRight = keyboard[SDL_SCANCODE_RIGHT] ||
                         keyboard[SDL_SCANCODE_D] ||
                         controllerAxis > 0.35F;

  if (!moveLeft && !moveRight) game.stage3DirectionArmed = true;

  // Event 3 is node based in the ROM: the joystick chooses an adjacent
  // tambourine during the first beats of a rebound. Once chosen, the whole
  // arc is committed and ends exactly at that drum's center. There is no
  // free horizontal drift and the grass is never a landing surface.
  if (!game.stage3Traveling &&
      game.stage3DirectionArmed &&
      game.stage3BounceFrame < kStage3DirectionWindowFrames &&
      moveLeft != moveRight) {
    const int direction = moveLeft ? -1 : 1;
    const int requested = game.stage3CurrentTambourine + direction;
    if (requested >= 0 &&
        requested < static_cast<int>(game.stage3Tambourines.size())) {
      game.stage3TargetTambourine = requested;
      game.stage3Traveling = true;
      game.stage3TravelStartFrame = game.stage3BounceFrame;
      game.stage3TravelStartX = game.player.position.x;
      game.player.facingRight = direction > 0;
      game.stage3DirectionArmed = false;
    }
  }

  const int bounceDuration =
      game.stage3Traveling ? kStage3TransferFrames : kStage3BounceFrames;
  const int previousBounceFrame = game.stage3BounceFrame;
  game.stage3BounceFrame =
      std::min(game.stage3BounceFrame + 1, bounceDuration);
  const float progress = static_cast<float>(game.stage3BounceFrame) /
                         static_cast<float>(bounceDuration);
  const float oldProgress = static_cast<float>(previousBounceFrame) /
                            static_cast<float>(bounceDuration);
  const auto& currentDrum = game.stage3Tambourines[static_cast<std::size_t>(
      game.stage3CurrentTambourine)];
  const auto& targetDrum = game.stage3Tambourines[static_cast<std::size_t>(
      game.stage3TargetTambourine)];
  if (game.stage3Traveling) {
    // Input is accepted only during the opening rise, but the committed
    // transfer itself lasts for the complete bounce.  Finishing all sideways
    // travel at the apex produced the visible "rocket launch" followed by a
    // straight drop that the arcade never performs.
    const int remainingFrames =
        std::max(1, kStage3TransferFrames - game.stage3TravelStartFrame);
    const float travelProgress = std::clamp(
        static_cast<float>(game.stage3BounceFrame -
                           game.stage3TravelStartFrame) /
            static_cast<float>(remainingFrames),
        0.0F, 1.0F);
    const float eased = 0.5F - 0.5F * std::cos(travelProgress * kPi);
    game.player.position.x =
        game.stage3TravelStartX +
        (targetDrum.worldX - game.stage3TravelStartX) * eased;
  } else {
    game.player.position.x = currentDrum.worldX;
    game.stage3TargetTambourine = game.stage3CurrentTambourine;
    game.stage3TravelStartFrame = 0;
    game.stage3TravelStartX = game.player.position.x;
  }
  game.player.runSpeed = 0.0F;
  game.cameraX = stage3CameraFor(game.player.position.x);

  const float height = game.stage3Traveling
                           ? 118.0F
                           : stage3BounceHeight(game.stage3BounceLevel);
  game.player.position.y =
      game.stage3BounceBaseY - std::sin(progress * kPi) * height;
  game.player.verticalVelocity =
      (std::sin(oldProgress * kPi) - std::sin(progress * kPi)) * height /
      static_cast<float>(kFixedDt);
  game.player.jumpFrame = game.stage3BounceFrame;
  game.player.grounded = false;

  // The fourth stationary rebound stays front-facing and ends when Charlie's
  // head reaches the underside of the circus fascia. Moving to a neighboring
  // drum before that rebound commits instead produces the normal spiral arc.
  if (!game.stage3Traveling && game.stage3BounceLevel == 4 &&
      (game.player.position.y <= kStage3RoofImpactY || progress >= 0.5F)) {
    crashStage3Roof(game);
    return;
  }

  // Only the third stationary bounce reaches the suspended money bag.
  if (!game.stage3Traveling && game.stage3BounceLevel == 3) {
    for (auto& drum : game.stage3Tambourines) {
      if (!drum.bagAvailable) continue;
      const float bagY = kStage3PrizeBagY;
      if (std::abs(game.player.position.x - drum.worldX) < 34.0F &&
          std::abs((game.player.position.y - 60.0F) - bagY) < 40.0F) {
        drum.bagAvailable = false;
        game.score += 500;
        showStage1Score(game, 500, drum.worldX, bagY);
        ++game.prizeBagsCollected;
        ++game.prizeBagAudioSerial;
      }
    }
  }

  if (game.stage3BounceFrame >= bounceDuration) {
    if (game.stage3Traveling) {
      game.stage3CurrentTambourine = game.stage3TargetTambourine;
      game.stage3Traveling = false;
      // A held direction is the arcade's continuous-travel control: landing
      // re-arms the next adjacent transfer. Releasing the stick still leaves
      // Charlie bouncing vertically on the current drum.
      game.stage3DirectionArmed = true;
      game.stage3BounceLevel = 1;
    } else {
      game.stage3BounceLevel =
          std::min(4, game.stage3BounceLevel + 1);
    }
    auto& landingDrum = game.stage3Tambourines[static_cast<std::size_t>(
        game.stage3CurrentTambourine)];
    landingDrum.compressionFrame = 10;
    game.player.position = {landingDrum.worldX, kStage3TambourineTopY};
    game.player.previous = game.player.position;
    game.stage3TargetTambourine = game.stage3CurrentTambourine;
    game.stage3BounceBaseY = kStage3TambourineTopY;
    game.stage3BounceFrame = 0;
    ++game.stage3BounceAudioSerial;
    if (game.stage3CurrentTambourine ==
        static_cast<int>(game.stage3Tambourines.size()) - 1) {
      finishStage(game);
      return;
    }
  }

  // Both Event 3 performers throw straight upward in their own lane. Knives
  // complete the arcade up/down toss, but the fire-breather's flame is a
  // one-way burst: it rises to its cutoff height, disappears, and leaves a
  // deliberate clear interval long enough for Charlie to cross one drum.
  constexpr int kKnifeProjectileFrames = 108;
  constexpr int kFlameProjectileFrames = 46;
  constexpr int kHazardCycleFrames = 150;
  for (std::size_t index = 0; index < game.stage3Performers.size(); ++index) {
    auto& performer = game.stage3Performers[index];
    auto& projectile = game.stage3Projectiles[index];
    ++performer.actionFrame;
    if (!projectile.active &&
        performer.actionFrame >= kHazardCycleFrames) {
      projectile.active = true;
      projectile.age = 0;
      projectile.kind = performer.kind;
      projectile.position = {performer.worldX, kStage3GroundY - 72.0F};
      performer.actionFrame = 0;
    }
    if (!projectile.active) continue;
    ++projectile.age;
    const bool knife =
        performer.kind == Stage3PerformerKind::KnifeThrower;
    const int projectileFrames =
        knife ? kKnifeProjectileFrames : kFlameProjectileFrames;
    const float projectileProgress = std::clamp(
        static_cast<float>(projectile.age) /
            static_cast<float>(projectileFrames),
        0.0F, 1.0F);
    projectile.position.x = performer.worldX;
    projectile.position.y =
        kStage3GroundY - 72.0F -
        (knife ? std::sin(projectileProgress * kPi) * 286.0F
               : projectileProgress * 220.0F);
    // The original fireball is narrow, and only its bright core is lethal.
    // Keeping the collision area inside the painted flame leaves the same
    // pass-by timing window visible in the arcade recording.
    const float collisionHalfWidth = knife ? 18.0F : 11.0F;
    const float collisionHalfHeight = knife ? 24.0F : 28.0F;
    if (std::abs(game.player.position.x - projectile.position.x) <
            collisionHalfWidth &&
        std::abs((game.player.position.y - 44.0F) -
                 projectile.position.y) < collisionHalfHeight) {
      crashPlayer(game);
      return;
    }
    if (projectile.age >= projectileFrames) {
      projectile.active = false;
      // Keep counting from the end of the active flight. The remainder of
      // the fixed cycle is the readable clear window before the next toss.
    }
  }

  if (game.stage1ScorePopupFrame > 0) --game.stage1ScorePopupFrame;
  if (game.bonus > 0) --game.bonus;
  if (game.bonus <= 0) {
    // Event 3's timer is an absolute game limit in the recorded ROM run.
    // Do not restart into an already-expired timer and crash repeatedly.
    game.lives = 1;
    crashPlayer(game);
    return;
  }
  awardScoreLives(game);
}

void updateStage4(Game& game, const Uint8* keyboard, bool jumpPressed,
                  float controllerAxis) {
  game.player.previous = game.player.position;
  game.previousCameraX = game.cameraX;
  if (game.stage4Balls.empty()) return;

  const bool moveLeft = keyboard[SDL_SCANCODE_LEFT] ||
                        keyboard[SDL_SCANCODE_A] || controllerAxis < -0.35F;
  const bool moveRight = keyboard[SDL_SCANCODE_RIGHT] ||
                         keyboard[SDL_SCANCODE_D] || controllerAxis > 0.35F;

  if (game.stage4RespawnGraceFrames > 0)
    --game.stage4RespawnGraceFrames;
  // Like the original event, Charlie backs up while still facing the course.
  // Left changes ball direction; it never mirrors the rider artwork.
  game.player.facingRight = true;

  for (std::size_t index = 0; index < game.stage4Balls.size(); ++index) {
    auto& ball = game.stage4Balls[index];
    if (static_cast<int>(index) != game.stage4CurrentBall ||
        game.stage4Airborne) {
      ball.worldX += ball.velocity * static_cast<float>(kFixedDt);
    }
    ball.rotation += ball.velocity * static_cast<float>(kFixedDt) /
                     kStage4BallRadius;
  }

  if (!game.stage4Airborne) {
    auto& ball = game.stage4Balls[static_cast<std::size_t>(game.stage4CurrentBall)];
    float target = 0.0F;
    if (moveLeft != moveRight) target = moveRight ? 142.0F : -112.0F;
    ball.velocity += (target - ball.velocity) *
                     static_cast<float>(kFixedDt) * 5.5F;
    if (!moveLeft && !moveRight) ball.velocity *= 0.975F;
    ball.worldX += ball.velocity * static_cast<float>(kFixedDt);
    game.player.position = {ball.worldX, kStage4CharlieBaselineY};
    game.player.runSpeed = ball.velocity;

    for (std::size_t index = 0;
         game.stage4RespawnGraceFrames == 0 &&
         index < game.stage4Balls.size(); ++index) {
      if (static_cast<int>(index) == game.stage4CurrentBall) continue;
      if (std::abs(game.stage4Balls[index].worldX - ball.worldX) <
          kStage4BallRadius * 1.82F) {
        game.stage4PinnedCrash = true;
        game.stage4FallFrame = 0;
        crashPlayer(game);
        return;
      }
    }

    if (jumpPressed) {
      game.stage4Airborne = true;
      game.player.grounded = false;
      game.player.verticalVelocity = -330.0F;
      game.player.runSpeed = std::clamp(ball.velocity, -120.0F, 175.0F);
      if (std::abs(game.player.runSpeed) < 62.0F)
        game.player.runSpeed = moveLeft && !moveRight ? -104.0F : 104.0F;
      game.player.jumpFrame = 0;
      ++game.jumpAudioSerial;
    }
  } else {
    ++game.player.jumpFrame;
    game.player.verticalVelocity += 735.0F * static_cast<float>(kFixedDt);
    game.player.position.x += game.player.runSpeed * static_cast<float>(kFixedDt);
    game.player.position.y +=
        game.player.verticalVelocity * static_cast<float>(kFixedDt);

    if (game.player.verticalVelocity > 0.0F) {
      int landing = -1;
      float best = 44.0F;
      for (std::size_t index = 0; index < game.stage4Balls.size(); ++index) {
        if (static_cast<int>(index) == game.stage4CurrentBall) continue;
        const float distance = std::abs(
            game.player.position.x - game.stage4Balls[index].worldX);
        if (distance < best &&
            game.player.position.y >= kStage4CharlieBaselineY - 17.0F &&
            game.player.position.y <= kStage4CharlieBaselineY + 16.0F) {
          best = distance;
          landing = static_cast<int>(index);
        }
      }
      if (landing >= 0) {
        const int skipped = std::abs(landing - game.stage4CurrentBall) - 1;
        if (skipped > 0) game.score += skipped * 500;
        game.stage4CurrentBall = landing;
        auto& ball = game.stage4Balls[static_cast<std::size_t>(landing)];
        game.player.position = {ball.worldX, kStage4CharlieBaselineY};
        game.player.previous = game.player.position;
        game.player.runSpeed = ball.velocity;
        game.player.verticalVelocity = 0.0F;
        game.player.grounded = true;
        game.stage4Airborne = false;
        game.player.jumpFrame = -1;
        if (landing == static_cast<int>(game.stage4Balls.size()) - 1) {
          finishStage(game);
          return;
        }
      }
    }
    if (game.player.position.y > kStage4BallCenterY + 90.0F) {
      game.stage4PinnedCrash = false;
      crashPlayer(game);
      return;
    }
  }

  const float following = std::max(0.0F,
      game.player.position.x - kStage4PlayerScreenX);
  const float finalCamera = std::max(0.0F,
      kStage4CourseLength - kStage4GoalScreenX);
  game.cameraX = std::min(following, finalCamera);
  if (game.bonus > 0) --game.bonus;
  if (game.bonus <= 0) crashPlayer(game);
  awardScoreLives(game);
}

void updateStage2(Game& game, const Uint8* keyboard, bool jumpPressed,
                  float controllerAxis) {
  game.player.previous = game.player.position;
  game.previousCameraX = game.cameraX;

  const bool moveLeft = keyboard[SDL_SCANCODE_LEFT] ||
                        keyboard[SDL_SCANCODE_A] ||
                        controllerAxis < -0.35F;
  const bool moveRight = keyboard[SDL_SCANCODE_RIGHT] ||
                         keyboard[SDL_SCANCODE_D] ||
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

void updateGame(Game& game, const Uint8* keyboard, bool jumpPressed,
                float controllerAxis) {
  if (game.scene == Scene::EventSelect) {
    ++game.eventSelectFrame;
    if (game.eventSelectFrame >= game.eventSelectDurationFrames) {
      confirmEventSelection(game);
    }
    return;
  }

  if (game.scene == Scene::Crashed) {
    game.player.previous = game.player.position;
    game.previousCameraX = game.cameraX;
    game.player.runSpeed = 0.0F;
    game.player.verticalVelocity = 0.0F;
    game.crashFrame =
        std::min(game.crashFrame + 1, kCrashBurnFrames);
    if (game.selectedEvent == 3)
      game.stage4FallFrame = std::min(game.stage4FallFrame + 1, 23);
    if (game.selectedEvent == 3) {
      // The original object engine keeps advancing the rolling-ball slots
      // during Charlie's fall. Freezing them here caused an immediate repeat
      // collision on restart and made the failure look unlike the cabinet.
      for (auto& ball : game.stage4Balls) {
        ball.worldX += ball.velocity * static_cast<float>(kFixedDt);
        ball.rotation += ball.velocity * static_cast<float>(kFixedDt) /
                         kStage4BallRadius;
      }
    }
    return;
  }

  if (game.scene == Scene::Goal) {
    game.player.previous = game.player.position;
    game.previousCameraX = game.cameraX;
    game.player.runSpeed = 0.0F;
    const bool stage2 = game.selectedEvent == 1;
    const bool stage3 = game.selectedEvent == 2;
    const bool stage4 = game.selectedEvent == 3;
    game.player.position.y = stage2 ? kStage2GoalTopY
                                    : (stage3 ? kStage3TambourineTopY
                                              : (stage4 ? kStage4GoalTopY
                                                        : kGoalLandingY));
    game.player.previous.y = game.player.position.y;
    game.player.grounded = true;
    game.player.jumpFrame = -1;
    game.cameraX =
        game.player.position.x -
        (stage2 ? 340.0F
                : (stage3 ? kStage3GoalScreenX
                          : (stage4 ? kStage4GoalScreenX : kGoalScreenX)));
    game.previousCameraX = game.cameraX;
    ++game.goalFrame;

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
    if (game.tallyFrame >= 280) game.scene = Scene::Complete;
    return;
  }

  if (game.scene != Scene::Playing) return;

  if (game.selectedEvent == 1) {
    updateStage2(game, keyboard, jumpPressed, controllerAxis);
    return;
  }
  if (game.selectedEvent == 2) {
    updateStage3(game, keyboard, jumpPressed, controllerAxis);
    return;
  }
  if (game.selectedEvent == 3) {
    updateStage4(game, keyboard, jumpPressed, controllerAxis);
    return;
  }

  if (game.stage1ScorePopupFrame > 0) --game.stage1ScorePopupFrame;

  game.player.previous = game.player.position;
  game.previousCameraX = game.cameraX;

  const bool moveLeft = keyboard[SDL_SCANCODE_LEFT] ||
                        keyboard[SDL_SCANCODE_A] ||
                        controllerAxis < -0.35F;
  const bool moveRight = keyboard[SDL_SCANCODE_RIGHT] ||
                         keyboard[SDL_SCANCODE_D] ||
                         controllerAxis > 0.35F;
  float targetSpeed = 0.0F;
  if (moveLeft != moveRight) {
    targetSpeed = moveLeft ? kBackSpeed : kForwardSpeed;
  }
  const bool reversing =
      targetSpeed != 0.0F && game.player.runSpeed != 0.0F &&
      ((targetSpeed < 0.0F) != (game.player.runSpeed < 0.0F));
  // A direction change on the original board brakes almost immediately.
  // The previous generic easing carried Charlie forward for several frames
  // after LEFT was pressed, creating the visible "walking on ice" slide and
  // making a backward hoop jump unnecessarily hard to time.
  const float movementResponse =
      targetSpeed == 0.0F ? 24.0F : (reversing ? 32.0F : 18.0F);
  game.player.runSpeed +=
      (targetSpeed - game.player.runSpeed) * static_cast<float>(kFixedDt) *
      movementResponse;
  if (std::abs(game.player.runSpeed) < 0.5F && targetSpeed == 0.0F) {
    game.player.runSpeed = 0.0F;
  }

  game.player.position.x +=
      game.player.runSpeed * static_cast<float>(kFixedDt);
  if (game.player.position.x < 78.0F) {
    game.player.position.x = 78.0F;
    game.player.runSpeed = std::max(0.0F, game.player.runSpeed);
  }
  game.cameraX =
      std::max(0.0F, game.player.position.x - 78.0F);

  const float ringTravel =
      kRingRailSpeed * static_cast<float>(kFixedDt);
  for (auto& hoop : game.hoops) {
    hoop.previousWorldX = hoop.worldX;
    // Cleared hoops remain physical, visible, and movable. The arcade lets
    // Charlie cross the same hoop in either direction after passing it.
    if (hoop.worldX - game.cameraX <= kRingActivationLead) {
      hoop.worldX -= ringTravel;
    }
  }
  for (auto& ring : game.bonusRings) {
    // Passing through removes only the prize. The physical ring stays on its
    // ceiling rail and remains visible, matching the original arcade behavior.
    if (ring.worldX - game.cameraX <= kRingActivationLead) {
      ring.worldX -= ringTravel;
    }
  }

  if (jumpPressed && game.player.grounded) {
    // Commit to reverse motion on the same board sample as LEFT + JUMP.
    // Without this, the direction easing could leave one residual forward
    // frame and let the hoop collision run before the reverse-jump exemption
    // became active.
    if (moveLeft && !moveRight) {
      game.player.runSpeed = std::min(game.player.runSpeed, -75.0F);
    }
    // In the recorded Event 1 opening, three reverse jumps summon a Charlie
    // doll on the overhead rail. The player earns the extra life only by
    // intercepting that moving doll, not at the instant of the third jump.
    const bool openingReverseJump =
        moveLeft && game.player.position.x < 240.0F &&
        !game.extraCharlieTriggered;
    if (openingReverseJump) {
      ++game.openingBackwardJumps;
      if (game.openingBackwardJumps >= 3) {
        const auto nextHoop = std::find_if(
            game.hoops.begin(), game.hoops.end(),
            [&game](const Hoop& hoop) {
              return hoop.worldX > game.player.position.x + 80.0F;
            });
        if (nextHoop != game.hoops.end()) {
          game.extraCharlieTriggered = true;
          game.extraCharlieActive = true;
          game.extraCharlieHoopIndex = static_cast<int>(
              std::distance(game.hoops.begin(), nextHoop));
        }
      }
    }
    game.player.grounded = false;
    game.player.jumpFrame = 0;
    game.player.verticalVelocity = 0.0F;
    ++game.jumpAudioSerial;
  }

  if (!game.player.grounded) {
    const int previousFrame = game.player.jumpFrame;
    const int nextFrame = std::min(
        previousFrame + 1,
        static_cast<int>(kJumpSourceDisplacement.size()) - 1);
    const float previousDisplacement =
        static_cast<float>(kJumpSourceDisplacement[previousFrame]) *
        kSourceToLogicalY;
    const float displacement =
        static_cast<float>(kJumpSourceDisplacement[nextFrame]) *
        kSourceToLogicalY;
    game.player.jumpFrame = nextFrame;
    game.player.position.y = kGroundY - displacement;
    game.player.verticalVelocity =
        (previousDisplacement - displacement) /
        static_cast<float>(kFixedDt);
    if (nextFrame ==
        static_cast<int>(kJumpSourceDisplacement.size()) - 1) {
      game.player.position.y = kGroundY;
      game.player.verticalVelocity = 0.0F;
      game.player.jumpFrame = -1;
      game.player.grounded = true;
    }
  }

  for (std::size_t hoopIndex = 0; hoopIndex < game.hoops.size();
       ++hoopIndex) {
    auto& hoop = game.hoops[hoopIndex];
    if (overlapsHoop(game.player, hoop)) {
      crashPlayer(game);
      return;
    }

    const float previousRelativeX =
        game.player.previous.x + kLionCollisionCenterOffset -
        hoop.previousWorldX;
    const float relativeX =
        game.player.position.x + kLionCollisionCenterOffset - hoop.worldX;
    const bool crossedForward =
        previousRelativeX <= 0.0F && relativeX > 0.0F;
    const bool crossedBackward =
        previousRelativeX >= 0.0F && relativeX < 0.0F;
    if (!crossedForward && !crossedBackward) continue;

    const bool previouslyCleared = hoop.cleared;
    hoop.cleared = true;
    game.score += 100;
    showStage1Score(game, 100, hoop.worldX,
                    (hoop.openingTop + hoop.openingBottom) * 0.5F - 5.0F);

    // The verified circusc4 recording shows the secret Charlie hanging inside
    // the next approaching hoop after a successful backward crossing. It can
    // be triggered only once for the whole game, even after a death/restart.
    if (crossedBackward && previouslyCleared &&
        !game.extraCharlieTriggered) {
      const auto nextHoop = std::find_if(
          game.hoops.begin(), game.hoops.end(),
          [&game](const Hoop& candidate) {
            return candidate.worldX > game.player.position.x + 40.0F;
          });
      if (nextHoop != game.hoops.end()) {
        game.extraCharlieTriggered = true;
        game.extraCharlieActive = true;
        game.extraCharlieHoopIndex = static_cast<int>(
            std::distance(game.hoops.begin(), nextHoop));
      }
    } else if (crossedForward && game.extraCharlieActive &&
               game.extraCharlieHoopIndex ==
                   static_cast<int>(hoopIndex)) {
      game.extraCharlieActive = false;
      game.extraCharlieCollected = true;
      ++game.lives;
      ++game.extraCharlieAudioSerial;
    }
  }

  for (auto& firePot : game.firePots) {
    if (firePot.retired) continue;
    if (firePot.worldX - game.cameraX < -80.0F) {
      firePot.retired = true;
      firePot.coinPending = false;
      firePot.coinActive = false;
      continue;
    }
    const float playerCenterX =
        game.player.position.x + kLionCollisionCenterOffset;
    const float potDistance = playerCenterX - firePot.worldX;
    const bool sharesHoopLane = std::any_of(
        game.hoops.begin(), game.hoops.end(),
        [&firePot](const Hoop& hoop) {
          return std::abs(hoop.worldX - firePot.worldX) <
                 kHoopPotSafetyDistance;
        });
    const bool backwardJump =
        !game.player.grounded && game.player.runSpeed < -20.0F;
    if (!sharesHoopLane && !game.hiddenCoinTriggered &&
        !firePot.coinChanceResolved &&
        backwardJump && potDistance > -20.0F && potDistance < 62.0F) {
      firePot.coinChanceResolved = true;
      const bool revealCoin = nextRandom(game) % 3U == 0U;
      if (revealCoin) {
        // MAME's Event 1 trace issues the reverse-jump command at frame 2775
        // and the coin command at frame 2838: exactly one complete 63-frame
        // jump later. Arm the reward while crossing, but do not launch it
        // until Charlie has landed on the far side of the pot.
        firePot.coinPending = true;
        game.hiddenCoinTriggered = true;
      }
    }

    if (firePot.coinPending && game.player.grounded &&
        playerCenterX <
            firePot.worldX - kFirePotCollisionHalfWidth) {
      firePot.coinPending = false;
      firePot.coinActive = true;
      firePot.coinFrame = 0;
      ++game.hiddenCoinAudioSerial;
    }

    if (firePot.coinActive) {
      ++firePot.coinFrame;
      const float coinY = firePotCoinY(firePot);
      const float riderCenterY = game.player.position.y - 40.0F;
      // The launch happens after the reverse jump has finished. Collection
      // therefore requires a distinct forward jump, just as the MAME demo
      // does at frames 2886–2904. Running underneath or waiting lets the coin
      // finish its arc and fall silently back into the pot.
      const bool forwardCatchAttempt =
          !game.player.grounded && game.player.runSpeed > 20.0F;
      if (!firePot.coinCollected && forwardCatchAttempt &&
          std::abs(playerCenterX - firePot.worldX) < 44.0F &&
          std::abs(riderCenterY - coinY) < 44.0F) {
        firePot.coinCollected = true;
        firePot.coinActive = false;
        game.score += 5000;
        showStage1Score(game, 5000, firePot.worldX, coinY - 18.0F);
        ++game.hiddenCoinAudioSerial;
      } else if (firePot.coinFrame >= kCoinFlightFrames) {
        firePot.coinActive = false;
      }
    }

    if (!sharesHoopLane &&
        std::abs(playerCenterX - firePot.worldX) <
            kFirePotCollisionHalfWidth &&
        game.player.position.y > kGroundY - kFirePotClearance) {
      crashPlayer(game);
      return;
    }
    if (!firePot.scored &&
        playerCenterX >
            firePot.worldX + kFirePotCollisionHalfWidth) {
      firePot.scored = true;
      game.score += 200;
      showStage1Score(game, 200, firePot.worldX, kGroundY - 118.0F);
    }
  }

  for (auto& ring : game.bonusRings) {
    // In the arcade game Charlie can run safely beneath a small suspended
    // ring. Its flame rim is tested only when the player commits to a jump.
    if (game.player.grounded) continue;

    const float ringCenterY = kGroundY - ring.height;
    const float playerCenterX =
        game.player.position.x + kLionCollisionCenterOffset;
    const float playerLeft = playerCenterX - kLionCollisionLeft;
    const float playerRight = playerCenterX + kLionCollisionRight;
    const float ringLeft =
        ring.worldX - kBonusRingCollisionHalfWidth;
    const float ringRight =
        ring.worldX + kBonusRingCollisionHalfWidth;
    const bool crossingRing =
        playerRight >= ringLeft && playerLeft <= ringRight;
    if (!crossingRing) continue;

    const float playerTop =
        game.player.position.y - kLionCollisionTop;
    const float playerBottom =
        game.player.position.y - kLionCollisionBottom;
    // The collision plane is intentionally thin: the original fixed jump only
    // has to be centered as the rider crosses the hoop, not for the entire
    // width of the lion. The opening follows the rendered rider/lion pose.
    const bool safelyInside =
        playerTop > ringCenterY - kBonusRingOpeningHalfHeight &&
        playerBottom < ringCenterY + kBonusRingOpeningHalfHeight;
    if (!safelyInside) {
      crashPlayer(game);
      return;
    }

    if (!ring.collected) {
      ring.collected = true;
      const int points = ring.containsPrize
                             ? ((nextRandom(game) & 1U) == 0U ? 500 : 1000)
                             : 0;
      game.score += points;
      if (ring.containsPrize) {
        showStage1Score(game, points, ring.worldX, ringCenterY - 5.0F);
        ++game.prizeBagsCollected;
        ++game.prizeBagAudioSerial;
      }
    }
  }

  if (game.bonus > 0) --game.bonus;
  game.score =
      std::max(game.score, static_cast<int>(game.player.position.x / 10.0F));

  constexpr float kGoalPlatformHalfWidth = 72.0F;
  const float goalLeft = kCourseLength - kGoalPlatformHalfWidth;
  const float goalRight = kCourseLength + kGoalPlatformHalfWidth;
  const float groundedStopX = goalLeft - kLionCollisionRight;
  const bool horizontallyOverPlatform =
      game.player.position.x + kLionCollisionRight >= goalLeft &&
      game.player.position.x - kLionCollisionLeft <= goalRight;
  const bool descendingOntoPlatform =
      !game.player.grounded && game.player.verticalVelocity >= 0.0F &&
      game.player.position.y >= kGoalLandingY &&
      horizontallyOverPlatform;

  if (descendingOntoPlatform) {
    finishStage(game);
  } else if (game.player.grounded &&
             game.player.position.x > groundedStopX) {
    // A grounded lion meets the padded side of the platform instead of
    // walking through it. The player must jump; the descending jump then
    // lands on the green top and starts the goal presentation.
    game.player.position.x = groundedStopX;
    game.player.previous.x = groundedStopX;
    game.player.runSpeed = std::min(0.0F, game.player.runSpeed);
    game.cameraX = std::max(0.0F, game.player.position.x - 78.0F);
    game.previousCameraX = game.cameraX;
  }
}

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
  constexpr float kZeppelinPanelCenterX = 92.0F;
  drawText(renderer, "BONUS", kZeppelinPanelCenterX, 34.0F, 1.0F,
           color(74, 111, 229), true);
  if (bonus < 0) return;
  std::ostringstream value;
  value << std::setw(4) << std::setfill('0') << bonus;
  drawText(renderer, value.str(), kZeppelinPanelCenterX, 50.0F, 1.45F,
           color(255, 255, 255), true);
}

void drawReddishMarqueeSky(SDL_Renderer* renderer) {
  // A translucent carnival-red wash brings the HD night painting closer to
  // the original board's red sky without replacing the existing tent art.
  fillRect(renderer, 0.0F, 0.0F, kWorldWidth, kMarqueeHeight,
           color(112, 12, 18, 64));
}

void drawBackdrop(SDL_Renderer* renderer, float cameraX, bool lowDetail,
                  const Assets& assets, const Game& game,
                  double timeSeconds, bool showCeilingTrack = true) {
  fillRect(renderer, 0.0F, 0.0F, kWorldWidth, kWorldHeight,
           color(5, 18, 51));

  // Original layer model: the zeppelin/tent crown, wheel, and black score
  // panel stay fixed. Only the lower arena wall, crowd, floor, and meter
  // markers travel with Charlie.
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
    // The animated ring itself is drawn in the foreground tile pass after the
    // rider. This pass supplies only its rail hardware and hanger.
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
  for (const auto& firePot : game.firePots) {
    if (firePot.retired) continue;
    const float screenX = firePot.worldX - cameraX;
    if (screenX < -80.0F || screenX > kWorldWidth + 80.0F) continue;
    const SDL_FRect destination{screenX - 31.0F, kGroundY - 102.0F, 62.0F,
                                124.0F};
    SDL_RenderCopyF(renderer, propsTexture, &fireSource, &destination);
    if (firePot.coinActive && !firePot.coinCollected) {
      const float coinY = firePotCoinY(firePot);
      const float flip = std::max(
          0.16F, std::abs(std::cos(static_cast<float>(firePot.coinFrame) *
                                  0.34F)));
      drawCoin(renderer, screenX, coinY, flip);
    }
  }

  for (const auto& ring : game.bonusRings) {
    const float screenX = ring.worldX - cameraX;
    if (screenX < -100.0F || screenX > kWorldWidth + 100.0F) continue;
    const float ringCenterY = kGroundY - ring.height;
    // The texture's target rectangle contains transparent vertical padding.
    // Connect the trolley to the visible flame crown, not the target top.
    line(renderer, screenX, kTrackY + 5.0F, screenX,
         ringCenterY - 58.0F, color(119, 101, 73));
    // Like the original board's category-0 tiles, the animated flame rim is
    // deferred to the foreground pass. The hanger and prize remain here.
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
      0,
      static_cast<int>(textureHeight * (500.0F / 1774.0F)),
      textureWidth,
      static_cast<int>(textureHeight * (1120.0F / 1774.0F)),
  };
  const float ringTop = hoop.openingTop - 25.0F;
  const float ringBottom = hoop.openingBottom + 20.0F;
  const SDL_FRect ringDestination{
      x - kBigRingVisualHalfWidth, ringTop,
      kBigRingVisualHalfWidth * 2.0F, ringBottom - ringTop};
  // MAME confirms the original board draws foreground-category tiles after
  // all sprites. The hoop is therefore a complete, tall, narrow foreground
  // element; its transparent center exposes the rider during the crossing.
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
  const SDL_Rect ringSource{cellWidth * 2, 0, cellWidth, textureHeight};

  for (const auto& ring : game.bonusRings) {
    const float screenX = ring.worldX - cameraX;
    if (screenX < -110.0F || screenX > kWorldWidth + 110.0F) continue;
    const float ringCenterY = kGroundY - ring.height;
    const SDL_FRect destination{
        screenX - kBonusRingVisualHalfWidth,
        ringCenterY - kBonusRingVisualHalfHeight,
        kBonusRingVisualHalfWidth * 2.0F,
        kBonusRingVisualHalfHeight * 2.0F};
    // The narrow full-height foreground rim crosses only a thin slice of the
    // much wider lion/rider composite, reproducing the arcade depth illusion.
    SDL_RenderCopyF(renderer, propsTexture, &ringSource, &destination);
  }
}

void drawFloorPlaque(SDL_Renderer* renderer, float screenX,
                     std::string_view label, bool start) {
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

  const int birdStart = kGoalArrivalFrames;
  const int bagDropStart = birdStart + kBirdArrivalFrames;
  const int showerStart = bagDropStart + kBagDropFrames;
  const bool stage3 = game.selectedEvent == 2;
  const float goalScreenX = stage3 ? kStage3GoalScreenX : kGoalScreenX;
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

void drawRiderWalkTest(SDL_Renderer* renderer, float screenX, float groundY,
                       double timeSeconds, bool alive,
                       SDL_Texture* riderTexture, float runSpeed,
                       bool grounded, bool facingRight) {
  int textureWidth = 0;
  int textureHeight = 0;
  SDL_QueryTexture(riderTexture, nullptr, nullptr, &textureWidth,
                   &textureHeight);
  const int cellWidth = textureWidth / 4;
  const int cellHeight = textureHeight / 3;

  int frame = 0;
  if (!grounded) {
    frame = 3;
  } else if (std::abs(runSpeed) > 5.0F) {
    constexpr double kRiderTestFramesPerSecond = 12.0;
    frame = static_cast<int>(timeSeconds * kRiderTestFramesPerSecond) % 12;
  }

  const SDL_Rect source{(frame % 4) * cellWidth, (frame / 4) * cellHeight,
                        cellWidth, cellHeight};
  // Side-by-side calibration against the 224x256 MAME frame puts the visible
  // lion/rider composite near 110 source-corrected logical pixels wide. The
  // old 170-unit render made Charlie sit near mid-screen and dwarfed hoops.
  // Ten percent more visual presence, requested after the calibrated Stage 1
  // layout pass. Collision remains on the measured logical body above.
  constexpr float kTestWidth = 118.8F;
  const float kTestHeight =
      kTestWidth * static_cast<float>(cellHeight) /
      static_cast<float>(cellWidth);
  constexpr float kAtlasGroundLine = 348.0F;
  constexpr float kJumpFrameBottom = 312.0F;
  const float sourceGroundLine = grounded ? kAtlasGroundLine
                                          : kJumpFrameBottom;
  const float visualGroundOffset =
      sourceGroundLine / static_cast<float>(cellHeight) * kTestHeight;
  // Preserve the old composite's visual center at screenX + 10 while scaling.
  const SDL_FRect destination{screenX + 10.0F - kTestWidth * 0.5F,
                              groundY - visualGroundOffset,
                              kTestWidth, kTestHeight};
  SDL_SetTextureColorMod(riderTexture, 255, alive ? 255 : 128,
                         alive ? 255 : 58);
  SDL_RenderCopyExF(renderer, riderTexture, &source, &destination, 0.0,
                    nullptr,
                    facingRight ? SDL_FLIP_NONE : SDL_FLIP_HORIZONTAL);
  SDL_SetTextureColorMod(riderTexture, 255, 255, 255);
}

void drawLionAndRider(SDL_Renderer* renderer, float screenX, float groundY,
                      double timeSeconds, bool alive, bool lowDetail,
                      SDL_Texture* riderTexture,
                      SDL_Texture* riderWalkTestTexture,
                      bool /*lionOnlyTest*/, float runSpeed, bool grounded,
                      bool facingRight) {
  // The calibrated 12-frame sheet has replaced the earlier six-frame
  // prototype. Keep the old texture only as a loading fallback.
  if (riderWalkTestTexture) {
    drawRiderWalkTest(renderer, screenX, groundY, timeSeconds, alive,
                      riderWalkTestTexture, runSpeed, grounded, facingRight);
    return;
  }
  if (riderTexture) {
    int textureWidth = 0;
    int textureHeight = 0;
    SDL_QueryTexture(riderTexture, nullptr, nullptr, &textureWidth,
                     &textureHeight);
    const int cellWidth = textureWidth / 3;
    const int cellHeight = textureHeight / 2;

    int frame = 0;
    if (!grounded) {
      // The original Event 1 rider keeps one stable composite throughout the
      // fixed jump. Vertical movement comes only from the measured jump table;
      // swapping generated poses here introduced the visible shake.
      frame = 4;
    } else if (std::abs(runSpeed) > 5.0F) {
      // The original composite advances one pose every 7–8 board frames.
      // Cycling all three grounded poses preserves that measured cadence and
      // avoids the choppy old two-frame toggle.
      frame = static_cast<int>(timeSeconds * (kBoardRefresh / 7.5) *
                               kStrideAnimationSpeedScale) %
              3;
    }

    const SDL_Rect source{(frame % 3) * cellWidth, (frame / 3) * cellHeight,
                          cellWidth, cellHeight};
    // The original six-tile composite is 48x32 source pixels and its visible
    // grounded pose is about 47x28. These non-square dimensions preserve that
    // measured silhouette on the 480x640 logical canvas.
    constexpr float kSpriteWidth = 140.0F;
    constexpr float kSpriteHeight = 132.0F;
    constexpr std::array<float, 6> kAnchorCorrection{
        30.0F, 30.0F, 30.0F, 30.0F, 30.0F, 30.0F};
    const SDL_FRect destination{
        screenX - 46.0F,
        groundY - kSpriteHeight +
            kAnchorCorrection[static_cast<size_t>(frame)],
        kSpriteWidth,
        kSpriteHeight,
    };

    SDL_SetTextureColorMod(riderTexture, 255, alive ? 255 : 128,
                          alive ? 255 : 58);
    SDL_RenderCopyExF(renderer, riderTexture, &source, &destination, 0.0,
                      nullptr,
                      facingRight ? SDL_FLIP_NONE : SDL_FLIP_HORIZONTAL);
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
  for (int index = 0; index < 60; ++index) {
    const float x = static_cast<float>(index * 8 + 2);
    const SDL_Color glow = bulbs[static_cast<size_t>(index % bulbs.size())];
    filledCircle(renderer, x, kHudTop + 2.0F, 2.4F, color(20, 20, 25));
    filledCircle(renderer, x, kHudTop + 2.0F, 1.6F, glow);
    filledCircle(renderer, x, kHudTop + kHudHeight - 3.0F, 2.4F,
                 color(20, 20, 25));
    filledCircle(renderer, x, kHudTop + kHudHeight - 3.0F, 1.6F, glow);
  }
  for (int index = 1; index < 11; ++index) {
    const float y = kHudTop + static_cast<float>(index * 8);
    const SDL_Color leftGlow =
        bulbs[static_cast<size_t>((index + 1) % bulbs.size())];
    const SDL_Color rightGlow =
        bulbs[static_cast<size_t>((index + 3) % bulbs.size())];
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
  drawText(renderer, std::to_string(game.highScore),
           kWorldWidth * 0.5F, kHudTop + 28.0F, 1.65F,
           color(51, 213, 57), true);

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
  drawText(renderer, "BIG TOP RUN HD TRIBUTE", kWorldWidth * 0.5F,
           596.0F, 1.25F, color(245, 245, 245), true);
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
  }
}

void drawStage2Rope(SDL_Renderer* renderer, float cameraX) {
  const float braidPhase = std::fmod(cameraX, 12.0F);
  fillRect(renderer, 0.0F, kStage2RopeY - 3.0F, kWorldWidth, 7.0F,
           color(47, 28, 18));
  fillRect(renderer, 0.0F, kStage2RopeY - 2.0F, kWorldWidth, 4.0F,
           color(202, 166, 112));
  fillRect(renderer, 0.0F, kStage2RopeY - 1.2F, kWorldWidth, 1.1F,
           color(250, 224, 174));
  for (int knot = -1; knot < 42; ++knot) {
    const float x = static_cast<float>(knot * 12) - braidPhase;
    line(renderer, x, kStage2RopeY - 2.0F, x + 6.0F,
         kStage2RopeY + 2.0F, color(126, 79, 43));
    line(renderer, x + 6.0F, kStage2RopeY - 2.0F, x + 12.0F,
         kStage2RopeY + 2.0F, color(234, 196, 137));
  }
}

void drawStage2Tower(SDL_Renderer* renderer, float x, bool startTower) {
  const float top = kStage2RopeY - 2.0F;
  fillRect(renderer, x - 40.0F, top - 5.0F, 80.0F, 7.0F,
           color(34, 194, 70));
  fillRect(renderer, x - 40.0F, top + 2.0F, 80.0F, 5.0F,
           color(234, 47, 49));
  if (!startTower) return;
  fillRect(renderer, x - 11.0F, top + 7.0F, 5.0F, 116.0F,
           color(201, 209, 218));
  fillRect(renderer, x + 6.0F, top + 7.0F, 5.0F, 116.0F,
           color(201, 209, 218));
  for (int brace = 0; brace < 5; ++brace) {
    const float y = top + 12.0F + brace * 22.0F;
    line(renderer, x - 7.0F, y, x + 8.0F, y + 18.0F,
         color(115, 127, 143));
    line(renderer, x + 8.0F, y, x - 7.0F, y + 18.0F,
         color(244, 246, 249));
  }
}

void drawStage2Marker(SDL_Renderer* renderer, float x,
                      std::string_view label) {
  fillRect(renderer, x - 25.0F, kStage2RopeY + 13.0F, 50.0F, 18.0F,
           color(35, 122, 204));
  fillRect(renderer, x - 22.0F, kStage2RopeY + 16.0F, 44.0F, 12.0F,
           color(255, 239, 105));
  drawText(renderer, label, x, kStage2RopeY + 18.0F, 0.95F,
           color(227, 52, 37), true);
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

  if (assets.arena) {
    int textureWidth = 0;
    int textureHeight = 0;
    SDL_QueryTexture(assets.arena, nullptr, nullptr, &textureWidth,
                     &textureHeight);
    const int sourceTop = static_cast<int>(
        static_cast<float>(textureHeight) * kArenaContentSourceTop);
    const SDL_Rect source{0, sourceTop, textureWidth,
                          textureHeight - sourceTop};
    const float tileWidth = static_cast<float>(kWorldWidth);
    const float scroll = std::fmod(cameraX, tileWidth);
    for (int tile = -1; tile <= 1; ++tile) {
      const SDL_FRect destination{
          static_cast<float>(tile) * tileWidth - scroll,
          kStage2RopeY + 12.0F, tileWidth,
          kWorldHeight - kStage2RopeY - 12.0F};
      SDL_RenderCopyF(renderer, assets.arena, &source, &destination);
    }
  } else {
    fillRect(renderer, 0.0F, kStage2RopeY + 12.0F, kWorldWidth,
             kWorldHeight - kStage2RopeY - 12.0F, color(36, 142, 49));
  }
  drawStage2Rope(renderer, cameraX);

  const float startX = 10.0F - cameraX;
  if (startX > -100.0F && startX < kWorldWidth + 100.0F) {
    drawStage2Tower(renderer, startX, true);
    drawStage2Marker(renderer, 78.0F - cameraX, "START");
  }
  for (const auto& marker : game.meterMarkers) {
    const float markerX = marker.worldX - cameraX;
    if (markerX > -60.0F && markerX < kWorldWidth + 60.0F) {
      drawStage2Marker(renderer, markerX,
                       std::to_string(marker.meters) + "M");
    }
  }

  const float goalX = kStage2GoalX - cameraX;
  if (goalX > -100.0F && goalX < kWorldWidth + 100.0F) {
    if (assets.stage2GoalRig) {
      int textureWidth = 0;
      int textureHeight = 0;
      SDL_QueryTexture(assets.stage2GoalRig, nullptr, nullptr,
                       &textureWidth, &textureHeight);
      const SDL_Rect source{
          static_cast<int>(textureWidth * 0.285F),
          static_cast<int>(textureHeight * 0.035F),
          static_cast<int>(textureWidth * 0.43F),
          static_cast<int>(textureHeight * 0.93F)};
      const SDL_FRect destination{goalX - 58.0F, kStage2GoalTopY,
                                  116.0F,
                                  kStage2RopeY - kStage2GoalTopY + 7.0F};
      SDL_RenderCopyF(renderer, assets.stage2GoalRig, &source,
                      &destination);
    } else {
      drawStage2Tower(renderer, goalX, true);
    }
    drawStage2Marker(renderer, goalX - 70.0F, "GOAL");
  }
}

void drawStage2SheetFrame(SDL_Renderer* renderer, SDL_Texture* texture,
                          int frame, float x, float baselineY,
                          float width, float height, bool flip) {
  if (!texture) return;
  int textureWidth = 0;
  int textureHeight = 0;
  SDL_QueryTexture(texture, nullptr, nullptr, &textureWidth, &textureHeight);
  const int cellWidth = textureWidth / 3;
  const int cellHeight = textureHeight / 2;
  frame = std::clamp(frame, 0, 5);
  const SDL_Rect source{(frame % 3) * cellWidth,
                        (frame / 3) * cellHeight,
                        cellWidth, cellHeight};
  const SDL_FRect destination{x - width * 0.5F, baselineY - height,
                              width, height};
  SDL_RenderCopyExF(renderer, texture, &source, &destination, 0.0, nullptr,
                    flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
}

void drawStage2Monkeys(SDL_Renderer* renderer, const Game& game,
                       const Assets& assets, float cameraX,
                       double timeSeconds) {
  const int boardFrame =
      static_cast<int>(timeSeconds * kBoardRefresh);
  const int walkFrame = ((boardFrame % 19) * 6) / 19;
  for (const auto& monkey : game.stage2Monkeys) {
    if (monkey.cleared) continue;
    const float x = monkey.worldX - cameraX;
    if (x < -90.0F || x > kWorldWidth + 90.0F) continue;
    SDL_Texture* texture = assets.stage2BrownWalk;
    int frame = walkFrame;
    if (monkey.kind == Stage2MonkeyKind::Purple) {
      if (monkey.leaping) {
        texture = assets.stage2PurpleJump;
        frame = std::min(5, monkey.leapFrame * 6 / 54);
      } else {
        texture = assets.stage2PurpleWalk;
      }
    }
    drawStage2SheetFrame(renderer, texture, frame, x,
                         stage2MonkeyY(monkey) + 18.0F, 92.0F, 68.0F, false);
  }
  if (game.stage2ScorePopupFrame > 0) {
    const float x = game.stage2ScorePopupWorldX - cameraX;
    const float lift = static_cast<float>(52 - game.stage2ScorePopupFrame) *
                       0.35F;
    drawText(renderer, std::to_string(game.stage2ScorePopup), x,
             kStage2RopeY - 80.0F - lift, 1.25F,
             color(255, 245, 96), true);
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

void drawStage2Charlie(SDL_Renderer* renderer, const Game& game,
                       const Assets& assets, float screenX, float playerY,
                       double timeSeconds) {
  int frame = 0;
  if (game.scene == Scene::Goal) {
    frame = 5;
  } else if (!game.player.grounded) {
    const float progress = static_cast<float>(game.player.jumpFrame) /
                           static_cast<float>(
                               kStage2JumpSourceDisplacement.size() - 1);
    frame = progress < 0.34F ? 2 : (progress < 0.68F ? 3 : 4);
  } else if (std::abs(game.player.runSpeed) > 5.0F) {
    frame = static_cast<int>(timeSeconds * (kBoardRefresh / 14.0)) & 1;
  }
  if (!game.player.alive && assets.stage2Charlie) {
    SDL_SetTextureColorMod(assets.stage2Charlie, 255, 105, 75);
  }
  drawStage2SheetFrame(renderer, assets.stage2Charlie, frame, screenX,
                       playerY + 18.0F, 90.0F, 112.0F,
                       !game.player.facingRight);
  if (!game.player.alive && assets.stage2Charlie) {
    SDL_SetTextureColorMod(assets.stage2Charlie, 255, 255, 255);
  }
}

void drawStage2Scene(SDL_Renderer* renderer, const Game& game,
                     const Assets& assets, double timeSeconds,
                     double interpolation) {
  const float camera =
      game.previousCameraX +
      (game.cameraX - game.previousCameraX) *
          static_cast<float>(interpolation);
  drawStage2Backdrop(renderer, game, camera, assets, timeSeconds);
  drawStage2Monkeys(renderer, game, assets, camera, timeSeconds);
  const float playerWorldX =
      game.player.previous.x +
      (game.player.position.x - game.player.previous.x) *
          static_cast<float>(interpolation);
  const float playerY =
      game.player.previous.y +
      (game.player.position.y - game.player.previous.y) *
          static_cast<float>(interpolation);
  drawStage2Charlie(renderer, game, assets, playerWorldX - camera, playerY,
                    timeSeconds);
  if (game.scene == Scene::Goal) {
    drawGoalPresentation(renderer, game, nullptr, nullptr, nullptr);
  }
  drawHud(renderer, game, assets.charlieLife);
  if (game.scene == Scene::Crashed &&
      game.crashFrame >= kCrashBurnFrames) {
    fillRect(renderer, 55.0F, 220.0F, 370.0F, 134.0F,
             color(33, 5, 8, 232));
    drawText(renderer, "OH NO!!", kWorldWidth * 0.5F, 246.0F, 3.0F,
             color(255, 96, 64), true);
    drawText(renderer, "SPACE OR Z TO RETRY", kWorldWidth * 0.5F,
             300.0F, 1.8F, color(255, 255, 255), true);
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

void drawStage3Tambourine(SDL_Renderer* renderer, SDL_Texture* texture,
                          float x, int compressionFrame, bool goal) {
  if (!texture) return;
  const float normalHeight = kStage3GroundY - kStage3TambourineTopY;
  const float compression =
      static_cast<float>(std::clamp(compressionFrame, 0, 10)) / 10.0F;
  const float height = normalHeight * (1.0F - compression * 0.18F);
  const float width = goal ? 172.0F : 94.0F;
  // Hold the cylinder's bottom fixed while the padded top compresses and
  // recovers after every landing, matching the ROM's rebound sprite family.
  const SDL_FRect destination{x - width * 0.5F,
                              kStage3GroundY - height,
                              width, height};
  SDL_RenderCopyF(renderer, texture, nullptr, &destination);
}

void drawStage3Scene(SDL_Renderer* renderer, const Game& game,
                     const Assets& assets, double timeSeconds,
                     double interpolation) {
  const float camera = game.previousCameraX +
                       (game.cameraX - game.previousCameraX) *
                           static_cast<float>(interpolation);
  drawBackdrop(renderer, camera, false, assets, game, timeSeconds);

  for (std::size_t index = 0; index < game.stage3Tambourines.size(); ++index) {
    const auto& drum = game.stage3Tambourines[index];
    const float x = drum.worldX - camera;
    const bool goal = index + 1 == game.stage3Tambourines.size();
    if (x < -110.0F || x > kWorldWidth + 110.0F) continue;
    drawStage3Tambourine(renderer,
                         goal ? assets.stage3GoalTambourine
                              : assets.stage3Tambourine,
                         x, drum.compressionFrame, goal);
    if (drum.bagAvailable) {
      const float bagY = kStage3PrizeBagY;
      if (assets.rewardBag) {
        const SDL_FRect bagDestination{x - 24.0F, bagY - 31.0F,
                                       48.0F, 52.0F};
        SDL_RenderCopyF(renderer, assets.rewardBag, nullptr,
                        &bagDestination);
      }
    }
  }

  // In the original board the distance plaques sit in the performer lanes,
  // not underneath the tambourines. Draw them above the drums but below the
  // performers so an act's feet naturally overlap the ground sign.
  for (const auto& marker : game.meterMarkers) {
    const float x = marker.worldX - camera;
    if (x < -50.0F || x > kWorldWidth + 50.0F) continue;
    fillRect(renderer, x - 27.0F, kStage3GroundY - 20.0F, 54.0F, 21.0F,
             color(29, 179, 239));
    fillRect(renderer, x - 23.0F, kStage3GroundY - 17.0F, 46.0F, 15.0F,
             color(239, 238, 196));
    drawText(renderer, std::to_string(marker.meters) + "M", x,
             kStage3GroundY - 14.0F, 0.9F, color(245, 83, 24), true);
  }

  for (std::size_t index = 0; index < game.stage3Performers.size(); ++index) {
    const auto& performer = game.stage3Performers[index];
    const float x = performer.worldX - camera;
    if (x < -80.0F || x > kWorldWidth + 80.0F) continue;
    const bool knife =
        performer.kind == Stage3PerformerKind::KnifeThrower;
    // These 12-frame actions were authored and checked as continuous WAM
    // motion studies. Frame zero is not blindly tied to the projectile:
    // actionFrame resets at release, so the first portion is follow-through
    // and tracking, the middle is recovery, and the final portion anticipates
    // the next release. This keeps the hands, head, and projectile in one
    // readable rhythm instead of jumping among five unrelated poses.
    const int cycle = performer.actionFrame % 150;
    int frame = 0;
    if (knife) {
      frame = cycle < 6 ? 3 :
              cycle < 20 ? 4 :
              cycle < 34 ? 5 :
              cycle < 48 ? 6 :
              cycle < 62 ? 7 :
              cycle < 76 ? 8 :
              cycle < 90 ? 9 :
              cycle < 104 ? 10 :
              cycle < 118 ? 11 :
              cycle < 130 ? 0 :
              cycle < 138 ? 1 :
              cycle < 145 ? 2 : 3;
    } else {
      frame = cycle < 8 ? 5 :
              cycle < 20 ? 6 :
              cycle < 32 ? 7 :
              cycle < 44 ? 8 :
              cycle < 58 ? 9 :
              cycle < 72 ? 10 :
              cycle < 88 ? 11 :
              cycle < 110 ? 0 :
              cycle < 124 ? 1 :
              cycle < 136 ? 2 :
              cycle < 144 ? 3 :
              cycle < 148 ? 4 : 5;
    }
    SDL_Texture* texture = knife ? assets.stage3KnifeThrower
                                 : assets.stage3FlameThrower;
    drawSheetFrame(renderer, texture, 4, 3, frame, x, kStage3GroundY,
                   knife ? 105.6F : 112.8F,
                   knife ? 134.4F : 139.2F);

    const auto& projectile = game.stage3Projectiles[index];
    if (!projectile.active) continue;
    const int projectileFrame = (projectile.age / 4) & 3;
    if (knife) {
      drawSheetFrame(renderer, assets.stage3Projectiles, 4, 2,
                     projectileFrame, projectile.position.x - camera,
                     projectile.position.y + 16.0F, 28.0F, 36.0F,
                     SDL_FLIP_NONE,
                     static_cast<double>(projectile.age * 17));
    } else {
      // The ROM flame is a compact upright droplet. It never rotates into a
      // sideways fireball; its four frames only flicker internally before the
      // projectile is retired at the end of its vertical cycle.
      drawSheetFrame(renderer, assets.stage3FlameProjectile, 4, 1,
                     projectileFrame, projectile.position.x - camera,
                     projectile.position.y + 71.0F, 80.0F, 140.0F);
    }
  }

  const float playerWorldX = game.player.previous.x +
      (game.player.position.x - game.player.previous.x) *
          static_cast<float>(interpolation);
  const float playerY = game.player.previous.y +
      (game.player.position.y - game.player.previous.y) *
          static_cast<float>(interpolation);
  int charlieFrame = 0;
  int charlieRows = 3;
  int charlieColumns = 4;
  double charlieAngle = 0.0;
  // The original front-facing atlas has much wider transparent cell margins
  // than the WAM tuck. Match their painted widths while preserving the
  // established vertical bounce height; this prevents a size pop at takeoff
  // without making the extended apex pose unnaturally tall.
  float charlieWidth = 110.0F;
  float charlieHeight = 116.0F;
  SDL_Texture* charlieTexture = assets.stage3CharlieVertical;
  if (game.scene == Scene::Goal) {
    // A compact crouch-rise-cheer loop keeps Charlie celebrating on the goal
    // drum instead of freezing on one pose.
    constexpr std::array<int, 6> celebrationFrames{0, 1, 2, 3, 2, 1};
    charlieFrame = celebrationFrames[static_cast<std::size_t>(
        (game.goalFrame / 7) % static_cast<int>(celebrationFrames.size()))];
  } else if (game.stage3Traveling) {
    const int remainingFrames =
        std::max(1, kStage3TransferFrames - game.stage3TravelStartFrame);
    const float progress = std::clamp(
        static_cast<float>(game.stage3BounceFrame -
                           game.stage3TravelStartFrame) /
            static_cast<float>(remainingFrames),
        0.0F, 1.0F);
    // WAM defines the compact tuck and the board-derived 48-frame timing.
    // Rotate one centered HD tuck continuously instead of stepping through a
    // few unrelated painted poses; those frame swaps caused the visible
    // shaking/rocket-launch effect during every drum transfer.
    charlieTexture = assets.stage3CharlieTuck;
    charlieColumns = 1;
    charlieRows = 1;
    charlieFrame = 0;
    charlieAngle = game.player.facingRight
                       ? static_cast<double>(progress * 360.0F)
                       : static_cast<double>(progress * -360.0F);
    charlieWidth = 100.0F;
    charlieHeight = 100.0F;
  } else {
    const float progress = static_cast<float>(game.stage3BounceFrame) /
                           static_cast<float>(kStage3BounceFrames);
    // Drive the pose directly from physical bounce height: compress at both
    // drum contacts and extend at the apex. The previous linear 0..11 walk
    // showed crouched frames at the apex and upright frames at landing.
    const float extension = std::sin(progress * kPi);
    charlieFrame = std::clamp(
        static_cast<int>(std::lround(extension * 5.0F)), 0, 5);
  }
  // On an over-jump the board removes Charlie from the arena and draws a
  // separate shocked head bursting through the fixed circus marquee.  Moving
  // a full body through the HUD is both visually wrong and unlike the ROM.
  if (!game.stage3RoofCrash) {
    // The goal artwork contains transparent pixels above its white cushion.
    // Its visible top is 22 logical pixels below the texture rectangle; use
    // that actual surface as Charlie's baseline so he no longer floats.
    // Every frame in the vertical atlas has a six-logical-pixel transparent
    // margin below Charlie's shoes. The goal cushion begins 22 pixels below
    // its destination rectangle, so 22 alone still leaves him visibly
    // floating. Anchor the painted shoe bottom, not the sprite cell bottom.
    const float playerBaseline =
        game.scene == Scene::Goal ? playerY + 28.0F : playerY + 15.0F;
    drawSheetFrame(renderer, charlieTexture, charlieColumns, charlieRows,
                   charlieFrame,
                   playerWorldX - camera, playerBaseline,
                   charlieWidth, charlieHeight,
                   game.player.facingRight ? SDL_FLIP_NONE
                                           : SDL_FLIP_HORIZONTAL,
                   charlieAngle);
  }

  if (game.scene == Scene::Goal) {
    // Event 3 shares the board's perfect-clear reward: the bird appears only
    // when every suspended bag was collected without losing a life. A missed
    // bag or any crash leaves perfectClear false, so no partial reward leaks
    // into the goal presentation.
    drawGoalPresentation(renderer, game, assets.bird, assets.rewardBag,
                         assets.props);
  }

  drawStage1ScorePopup(renderer, game, camera);
  drawHud(renderer, game, assets.charlieLife);
  if (game.stage3RoofCrash) {
    const int roofFrame = std::min(game.crashFrame / 5, 3);
    drawSheetFrame(renderer, assets.stage3CharlieRoofHead, 4, 1,
                   roofFrame, kStage3PlayerScreenX, 132.0F,
                   78.0F, 70.0F);

    const auto drawOhNo = [&](float x, float y) {
      constexpr float scale = 1.55F;
      const SDL_Color outline = color(255, 239, 26);
      drawText(renderer, "OH NO!!", x - 2.0F, y, scale, outline, true);
      drawText(renderer, "OH NO!!", x + 2.0F, y, scale, outline, true);
      drawText(renderer, "OH NO!!", x, y - 2.0F, scale, outline, true);
      drawText(renderer, "OH NO!!", x, y + 2.0F, scale, outline, true);
      drawText(renderer, "OH NO!!", x, y, scale,
               color(255, 82, 28), true);
    };
    drawOhNo(154.0F, 242.0F);
    drawOhNo(354.0F, 300.0F);
  }
  if (game.scene == Scene::Crashed &&
      game.crashFrame >= kCrashBurnFrames && !game.stage3RoofCrash) {
    const float overlayY = 220.0F;
    fillRect(renderer, 55.0F, overlayY, 370.0F, 134.0F,
             color(33, 5, 8, 232));
    drawText(renderer, "OH NO!!", kWorldWidth * 0.5F,
             overlayY + 26.0F, 3.0F,
             color(255, 96, 64), true);
    drawText(renderer, "SPACE OR Z TO RETRY", kWorldWidth * 0.5F,
             overlayY + 80.0F, 1.8F, color(255, 255, 255), true);
  }
}

void drawStage4Scene(SDL_Renderer* renderer, const Game& game,
                     const Assets& assets, double timeSeconds,
                     double interpolation) {
  const float camera = game.previousCameraX +
                       (game.cameraX - game.previousCameraX) *
                           static_cast<float>(interpolation);
  drawBackdrop(renderer, camera, false, assets, game, timeSeconds, false);

  for (const auto& marker : game.meterMarkers) {
    const float x = marker.worldX - camera;
    if (x < -50.0F || x > kWorldWidth + 50.0F) continue;
    drawFloorPlaque(renderer, x,
                    marker.meters < 0
                        ? std::string("START")
                        : std::to_string(marker.meters) + "M",
                    marker.meters < 0);
  }

  for (std::size_t index = 0; index < game.stage4Balls.size(); ++index) {
    const auto& ball = game.stage4Balls[index];
    const float x = ball.worldX - camera;
    if (x < -80.0F || x > kWorldWidth + 80.0F) continue;
    // Rotate the painted ball continuously. Swapping among eight nearly
    // identical atlas cells made the first version appear static.
    drawSheetFrame(renderer, assets.stage4Ball, 1, 1, 0, x,
                   kStage4BallCenterY + kStage4BallRadius,
                   kStage4BallRadius * 2.0F, kStage4BallRadius * 2.0F,
                   SDL_FLIP_NONE,
                   static_cast<double>(ball.rotation * 180.0F / kPi));
  }

  const float goalX = kStage4CourseLength - camera;
  if (goalX > -130.0F && goalX < kWorldWidth + 130.0F &&
      assets.goalPlatform) {
    const SDL_FRect destination{goalX - 86.0F, kStage4GoalTopY,
                                172.0F, 48.0F};
    SDL_RenderCopyF(renderer, assets.goalPlatform, nullptr, &destination);
  }

  const float playerWorldX = game.player.previous.x +
      (game.player.position.x - game.player.previous.x) *
          static_cast<float>(interpolation);
  const float playerY = game.player.previous.y +
      (game.player.position.y - game.player.previous.y) *
          static_cast<float>(interpolation);
  int frame = 0;
  // The generated atlas retains transparent padding below Charlie's shoes.
  // Anchor the painted feet to the physical ball surface, not the cell edge.
  float baseline = playerY + 43.0F;
  float width = 150.0F;
  float height = 150.0F;
  if (game.scene == Scene::Crashed && game.stage4PinnedCrash) {
    frame = 8 + std::min(3, game.stage4FallFrame / 6);
    baseline = kStage4BallCenterY + 70.0F;
  } else if (game.stage4Airborne) {
    frame = 4 + std::min(3, (game.player.jumpFrame / 5) & 3);
  } else if (game.scene == Scene::Goal) {
    constexpr std::array<int, 8> celebration{0, 1, 2, 3, 2, 1, 0, 1};
    frame = celebration[static_cast<std::size_t>((game.goalFrame / 7) % 8)];
    baseline = kStage4GoalTopY + 49.0F;
  } else {
    frame = (static_cast<int>(timeSeconds * 60.606F) / 7) & 3;
  }
  drawSheetFrame(renderer, assets.stage4Charlie, 4, 3, frame,
                 playerWorldX - camera, baseline, width, height,
                 // Charlie keeps looking toward the course even when the
                 // player rolls backward, matching the cabinet animation.
                 SDL_FLIP_NONE);

  drawHud(renderer, game, assets.charlieLife);
  if (game.scene == Scene::Crashed && game.stage4PinnedCrash) {
    drawText(renderer, "OH NO!!", 150.0F, 260.0F, 1.8F,
             color(255, 82, 28), true);
    drawText(renderer, "OH NO!!", 354.0F, 306.0F, 1.8F,
             color(255, 82, 28), true);
  }
  if (game.scene == Scene::Crashed &&
      game.crashFrame >= kCrashBurnFrames) {
    fillRect(renderer, 55.0F, 220.0F, 370.0F, 134.0F,
             color(33, 5, 8, 232));
    drawText(renderer, "OH NO!!", kWorldWidth * 0.5F, 246.0F, 3.0F,
             color(255, 96, 64), true);
    drawText(renderer, "SPACE OR Z TO RETRY", kWorldWidth * 0.5F,
             300.0F, 1.8F, color(255, 255, 255), true);
  }
}

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
      "6000-4500", "4499-4000", "3999-3500", "3499-3000",
      "2999-2500", "2499-2000", "1999-1500", "1499-1001",
      "1000-500",  "499-0",
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

void renderScene(SDL_Renderer* renderer, const Game& game,
                 const RenderSurface& surface, const Assets& assets,
                 double timeSeconds, double interpolation) {
  const bool lowDetail = surface.height <= 320;
  if (game.scene == Scene::Title) {
    drawCoinWaitingScreen(renderer, game, assets, timeSeconds);
    if (game.debug) drawDebug(renderer, game, surface);
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
                       game.player.alive, lowDetail, assets.rider,
                       assets.riderWalkTest, game.lionOnlyTest,
                       game.player.runSpeed, game.player.grounded,
                       game.player.facingRight);
    }
    for (const auto& hoop : game.hoops) {
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

  if (game.scene == Scene::Crashed &&
             game.crashFrame >= kCrashBurnFrames) {
    fillRect(renderer, 55.0F, 220.0F, 370.0F, 134.0F, color(33, 5, 8, 232));
    drawText(renderer, "MISSED THE HOOP", kWorldWidth * 0.5F, 246.0F, 2.4F,
             color(255, 96, 64), true);
    drawText(renderer, "SPACE OR Z TO RETRY", kWorldWidth * 0.5F, 300.0F,
             1.8F, color(255, 255, 255), true);
  }

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
      "Big Top Run Native", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
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
  game.eventSelectDurationFrames =
      audioDurationInBoardFrames(audio.eventSelectMusic);
  game.randomState ^=
      static_cast<std::uint32_t>(SDL_GetPerformanceCounter());
  game.debug = options.debug;
  game.lionOnlyTest = options.lionTest;
  resetCourse(game);
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
                 options.captureScene == "stage3-roof") {
        game.selectedEvent = 2;
      } else if (options.captureScene == "stage4" ||
                 options.captureScene == "stage4-jump" ||
                 options.captureScene == "stage4-fall" ||
                 options.captureScene == "stage4-goal") {
        game.selectedEvent = 3;
      }
      startGame(game);
    }
    if (options.captureScene == "stage4-goal") {
      game.stage4CurrentBall =
          static_cast<int>(game.stage4Balls.size()) - 1;
      finishStage(game);
      game.goalFrame = 96;
    } else if (options.captureScene == "stage4-fall") {
      game.stage4CurrentBall = 5;
      const auto& ball = game.stage4Balls[5];
      game.player.position = {ball.worldX, kStage4BallCenterY + 30.0F};
      game.player.previous = game.player.position;
      game.player.alive = false;
      game.player.grounded = false;
      game.stage4Airborne = false;
      game.stage4PinnedCrash = true;
      game.stage4FallFrame = 18;
      game.scene = Scene::Crashed;
      game.crashFrame = 18;
      game.cameraX = std::max(0.0F, ball.worldX - kStage4PlayerScreenX);
      game.previousCameraX = game.cameraX;
    } else if (options.captureScene == "stage4-jump") {
      game.stage4CurrentBall = 4;
      const auto& from = game.stage4Balls[4];
      const auto& to = game.stage4Balls[5];
      game.stage4Airborne = true;
      game.player.grounded = false;
      game.player.jumpFrame = 11;
      game.player.verticalVelocity = 18.0F;
      game.player.position = {(from.worldX + to.worldX) * 0.5F,
                              kStage4CharlieBaselineY - 92.0F};
      game.player.previous = game.player.position;
      game.cameraX = std::max(
          0.0F, game.player.position.x - kStage4PlayerScreenX);
      game.previousCameraX = game.cameraX;
    } else if (options.captureScene == "stage4") {
      game.stage4CurrentBall = 4;
      const auto& ball = game.stage4Balls[4];
      game.player.position = {ball.worldX, kStage4CharlieBaselineY};
      game.player.previous = game.player.position;
      game.player.runSpeed = ball.velocity;
      game.cameraX = std::max(0.0F, ball.worldX - kStage4PlayerScreenX);
      game.previousCameraX = game.cameraX;
    } else if (options.captureScene == "stage3-goal") {
      game.stage3CurrentTambourine =
          static_cast<int>(game.stage3Tambourines.size()) - 1;
      game.stage3TargetTambourine = game.stage3CurrentTambourine;
      game.prizeBagsCollected = game.prizeBagsAvailable;
      game.deathOccurred = false;
      finishStage(game);
      game.goalFrame = 180;
    } else if (options.captureScene == "stage3-approach") {
      const int finalIndex =
          static_cast<int>(game.stage3Tambourines.size()) - 1;
      game.stage3CurrentTambourine = finalIndex - 1;
      game.stage3TargetTambourine = finalIndex;
      game.stage3BounceLevel = 1;
      game.stage3BounceFrame = kStage3TransferFrames / 2;
      game.stage3BounceBaseY = kStage3TambourineTopY;
      game.stage3Traveling = true;
      game.stage3TravelStartFrame = 0;
      game.stage3TravelStartX =
          game.stage3Tambourines[static_cast<std::size_t>(finalIndex - 1)]
              .worldX;
      game.player.position = {
          (game.stage3Tambourines[static_cast<std::size_t>(finalIndex - 1)]
                   .worldX +
           game.stage3Tambourines[static_cast<std::size_t>(finalIndex)]
               .worldX) *
              0.5F,
          kStage3TambourineTopY - 118.0F};
      game.player.previous = game.player.position;
      game.cameraX = stage3CameraFor(game.player.position.x);
      game.previousCameraX = game.cameraX;
    } else if (options.captureScene == "stage3-roof") {
      game.stage3CurrentTambourine = 3;
      game.stage3TargetTambourine = 3;
      game.player.position = {
          game.stage3Tambourines[3].worldX, kStage3RoofImpactY};
      game.player.previous = game.player.position;
      game.player.alive = false;
      game.player.grounded = false;
      game.player.runSpeed = 0.0F;
      game.stage3BounceLevel = 4;
      game.stage3RoofCrash = true;
      game.stage3Traveling = false;
      game.scene = Scene::Crashed;
      game.crashFrame = 16;
      game.cameraX = std::max(
          0.0F, game.player.position.x - kStage3PlayerScreenX);
      game.previousCameraX = game.cameraX;
    } else if (options.captureScene == "stage3-transfer") {
      game.stage3CurrentTambourine = 3;
      game.stage3TargetTambourine = 4;
      game.stage3BounceLevel = 1;
      game.stage3BounceFrame = kStage3BounceFrames / 2;
      game.stage3BounceBaseY = kStage3TambourineTopY;
      game.stage3Traveling = true;
      game.stage3TravelStartFrame = 0;
      game.stage3TravelStartX = game.stage3Tambourines[3].worldX;
      game.player.position = {
          (game.stage3Tambourines[3].worldX +
           game.stage3Tambourines[4].worldX) * 0.5F,
          kStage3TambourineTopY - 118.0F};
      game.player.previous = game.player.position;
      game.player.facingRight = true;
      game.cameraX = std::max(
          0.0F, game.player.position.x - kStage3PlayerScreenX);
      game.previousCameraX = game.cameraX;
    } else if (options.captureScene == "stage3") {
      game.stage3CurrentTambourine = 3;
      game.stage3TargetTambourine = 3;
      game.player.position = {
          game.stage3Tambourines[3].worldX,
          kStage3TambourineTopY - stage3BounceHeight(3)};
      game.player.previous = game.player.position;
      game.player.runSpeed = 0.0F;
      game.player.grounded = false;
      game.stage3BounceLevel = 3;
      game.stage3BounceFrame = kStage3BounceFrames / 2;
      game.stage3BounceBaseY = kStage3TambourineTopY;
      game.stage3Traveling = false;
      game.cameraX = std::max(
          0.0F, game.player.position.x - kStage3PlayerScreenX);
      game.previousCameraX = game.cameraX;
      for (std::size_t index = 0;
           index < game.stage3Projectiles.size(); ++index) {
        game.stage3Projectiles[index].active = index < 2;
        game.stage3Projectiles[index].age = index == 0 ? 27 : 42;
        game.stage3Projectiles[index].kind =
            game.stage3Performers[index].kind;
        game.stage3Projectiles[index].position = {
            game.stage3Performers[index].worldX,
            kStage3GroundY - (index == 0 ? 210.0F : 188.0F)};
      }
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
      }
      for (auto& ring : game.bonusRings) ring.worldX = -10000.0F;
      for (auto& firePot : game.firePots) firePot.retired = true;
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
      }
      game.hoops.front().worldX = game.player.position.x + 150.0F;
      game.hoops.front().previousWorldX = game.hoops.front().worldX;
      for (auto& ring : game.bonusRings) ring.worldX = -10000.0F;
      for (auto& firePot : game.firePots) firePot.retired = true;
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
      }
      for (auto& ring : game.bonusRings) ring.worldX = -10000.0F;
      game.bonusRings.front().worldX = game.player.position.x + 150.0F;
      game.bonusRings.front().containsPrize = true;
      game.bonusRings.front().collected = false;
      for (auto& firePot : game.firePots) firePot.retired = true;
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
      }
      game.bonusRings.front().worldX =
          game.player.position.x + kLionCollisionCenterOffset;
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
      }
      game.hoops.front().cleared = true;
      game.extraCharlieTriggered = true;
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
  bool jumpQueued = false;
  bool eventSelectMusicPlaying = false;
  bool stageMusicPlaying = false;
  bool stageMusicFast = false;
  std::uint32_t observedJumpAudioSerial = game.jumpAudioSerial;
  std::uint32_t observedCrashAudioSerial = game.crashAudioSerial;
  std::uint32_t observedExtraCharlieAudioSerial =
      game.extraCharlieAudioSerial;
  std::uint32_t observedPrizeBagAudioSerial = game.prizeBagAudioSerial;
  std::uint32_t observedHiddenCoinAudioSerial = game.hiddenCoinAudioSerial;
  std::uint32_t observedCoinAudioSerial = game.coinAudioSerial;
  std::uint32_t observedEventSelectMoveAudioSerial =
      game.eventSelectMoveAudioSerial;
  std::uint32_t observedEventSelectConfirmAudioSerial =
      game.eventSelectConfirmAudioSerial;
  std::uint32_t observedStage3BounceAudioSerial =
      game.stage3BounceAudioSerial;
  std::uint32_t observedStage3OverjumpAudioSerial =
      game.stage3OverjumpAudioSerial;
  Scene observedScene = game.scene;
  int observedGoalFrame = game.goalFrame;
  double accumulator = 0.0;
  const Uint64 frequency = SDL_GetPerformanceFrequency();
  Uint64 previousCounter = SDL_GetPerformanceCounter();
  const Uint64 startCounter = previousCounter;

  while (running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) {
        running = false;
      } else if (event.type == SDL_WINDOWEVENT &&
                 (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
                  event.window.event == SDL_WINDOWEVENT_RESIZED)) {
        surface =
            buildRenderSurface(renderer, window, options.rotation, surface);
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
          if (game.scene == Scene::EventSelect) {
            confirmEventSelection(game);
          } else if (game.scene == Scene::Title ||
                     game.scene == Scene::Complete) {
            startWithCredit(game);
          }
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
            if (game.scene == Scene::Title) {
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
            if (game.scene == Scene::EventSelect) {
              confirmEventSelection(game);
            } else if (game.scene == Scene::Title ||
                game.scene == Scene::Complete) {
              startWithCredit(game);
            }
            break;
          case SDLK_LEFT:
          case SDLK_a:
            moveEventSelection(game, -1);
            break;
          case SDLK_RIGHT:
          case SDLK_d:
            moveEventSelection(game, 1);
            break;
          case SDLK_5:
          case SDLK_c:
            insertCoin(game);
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
          case SDLK_F11:
            fullscreen = !fullscreen;
            setFullscreen(window, fullscreen);
            surface =
                buildRenderSurface(renderer, window, options.rotation, surface);
            break;
          default:
            break;
        }
      }
    }

    if (jumpQueued && game.scene == Scene::Crashed &&
        game.crashFrame >= kCrashBurnFrames) {
      restartAfterCrash(game);
      jumpQueued = false;
    }

    const Uint64 currentCounter = SDL_GetPerformanceCounter();
    double frameTime =
        static_cast<double>(currentCounter - previousCounter) /
        static_cast<double>(frequency);
    previousCounter = currentCounter;
    frameTime = std::min(frameTime, 0.1);
    accumulator += frameTime;

    const Uint8* keyboard = SDL_GetKeyboardState(nullptr);
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
    bool jumpForStep = jumpQueued;
    while (accumulator >= kFixedDt) {
      updateGame(game, keyboard, jumpForStep, controllerAxis);
      awardScoreLives(game);
      if (game.score > game.highScore) {
        game.highScore = game.score;
        game.highScoreDirty = true;
      }
      jumpForStep = false;
      jumpQueued = false;
      accumulator -= kFixedDt;
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

    const bool shouldPlayStageMusic = game.scene == Scene::Playing;
    const bool shouldUseFastStageMusic =
        shouldPlayStageMusic && game.bonus <= 999;
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
      playMissSounds(audio);
      observedCrashAudioSerial = game.crashAudioSerial;
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
    if (observedStage3OverjumpAudioSerial !=
        game.stage3OverjumpAudioSerial) {
      playStage3OverjumpSound(audio);
      observedStage3OverjumpAudioSerial =
          game.stage3OverjumpAudioSerial;
    }
    if (game.scene != observedScene) {
      if (game.highScoreDirty &&
          (game.scene == Scene::Crashed || game.scene == Scene::Goal ||
           game.scene == Scene::Complete || game.scene == Scene::Title)) {
        if (saveHighScore(highScorePath, game.highScore)) {
          game.highScoreDirty = false;
        }
      }
      if (game.scene == Scene::Goal) playCrowdCheer(audio);
      if (game.scene == Scene::Tally) playBonusCount(audio);
      observedScene = game.scene;
    }
    const int showerStart =
        kGoalArrivalFrames + kBirdArrivalFrames + kBagDropFrames;
    if (game.scene == Scene::Goal && game.perfectClear &&
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

    SDL_RenderSetScale(renderer, 1.0F, 1.0F);
    SDL_SetRenderTarget(renderer, nullptr);
    setColor(renderer, color(0, 0, 0));
    SDL_RenderClear(renderer);
    SDL_RenderCopyEx(renderer, surface.texture, nullptr, &surface.destination,
                     static_cast<double>(options.rotation), nullptr,
                     SDL_FLIP_NONE);
    if (!options.capturePath.empty()) {
      if (!captureRenderer(renderer, options.capturePath)) {
        std::cerr << "Screenshot capture failed: " << IMG_GetError() << '\n';
      }
      running = false;
    }
    SDL_RenderPresent(renderer);

    SDL_SetWindowTitle(
        window,
        ("Big Top Run Native | " + std::to_string(surface.width) + "x" +
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
