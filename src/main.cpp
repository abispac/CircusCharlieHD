#include <SDL.h>
#include <SDL_image.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
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
constexpr float kGroundY = 532.0F;
constexpr float kTrackY = 282.0F;
constexpr float kBackSpeed = -150.0F;
constexpr float kForwardSpeed = 195.0F;
constexpr float kRingRailSpeed = 65.0F;
constexpr float kRingActivationLead = 900.0F;
constexpr float kCourseLength = 6000.0F;
constexpr float kGoalScreenX = 150.0F;
constexpr float kGoalPlatformTop = kGroundY - 25.0F;
constexpr float kGoalLandingY = kGoalPlatformTop + 12.0F;
constexpr float kPi = 3.14159265358979323846F;
constexpr float kMarqueeHeight = 105.0F;
constexpr float kHudTop = kMarqueeHeight;
constexpr float kHudHeight = 90.0F;
constexpr float kArenaTop = kHudTop + kHudHeight;
constexpr float kCrowdTop = 310.0F;
constexpr float kBigHoopOpeningTop = kGroundY - 205.0F;
constexpr float kBigHoopOpeningBottom = kGroundY - 80.0F;
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
constexpr float kLionCollisionLeft = 26.0F;
constexpr float kLionCollisionRight = 34.0F;
constexpr float kLionCollisionTop = 58.0F;
constexpr float kLionCollisionBottom = 7.0F;
constexpr float kFirePotCollisionHalfWidth = 32.0F;
constexpr float kFirePotClearance = 42.0F;
constexpr float kHoopPotSafetyDistance = 76.0F;
constexpr float kBigRingVisualHalfWidth = 24.0F;
constexpr float kBonusRingCollisionHalfWidth = 6.0F;
constexpr float kBonusRingOpeningHalfHeight = 72.0F;
constexpr float kBonusRingVisualHalfWidth = 30.0F;
constexpr float kBonusRingVisualHalfHeight = 115.0F;
// The MAME sequence places the coin near its apex about 50 frames after the
// launch and catches it on the descending half at frame 66. A 96-frame arc
// matches both measurements and still returns visibly to the pot when missed.
constexpr int kCoinFlightFrames = 96;
constexpr float kCoinArcHeight = 151.0F;
constexpr int kCrashBurnFrames = 72;
constexpr int kGoalArrivalFrames = 90;
constexpr int kBirdArrivalFrames = 170;
constexpr int kBagDropFrames = 45;
constexpr int kBirdExitFrames = 36;
constexpr int kCoinShowerFrames = 220;
constexpr int kRewardCoinCount = 18;
constexpr float kStrideAnimationSpeedScale = 0.85F;

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
};

struct FirePot {
  float worldX = 0.0F;
  bool coinChanceResolved = false;
  bool coinPending = false;
  bool coinActive = false;
  bool coinCollected = false;
  int coinFrame = 0;
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
  float cameraX = 0.0F;
  float previousCameraX = 0.0F;
  int score = 0;
  int lives = 3;
  int bonus = 6000;
  int instructionFrames = 0;
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
  float extraCharlieWorldX = 0.0F;
  std::uint32_t jumpAudioSerial = 0;
  std::uint32_t crashAudioSerial = 0;
  std::uint32_t extraCharlieAudioSerial = 0;
  std::uint32_t prizeBagAudioSerial = 0;
  std::uint32_t hiddenCoinAudioSerial = 0;
  std::uint32_t randomState = 0x6d2b79f5U;
  bool debug = false;
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
  SDL_Texture* hoop = nullptr;
  SDL_Texture* hoopFlare = nullptr;
  SDL_Texture* props = nullptr;
  SDL_Texture* propsFlare = nullptr;
  SDL_Texture* bird = nullptr;
  SDL_Texture* rewardBag = nullptr;
  SDL_Texture* charlieLife = nullptr;
  SDL_Texture* goalPlatform = nullptr;
  SDL_Texture* finishRider = nullptr;
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
  AudioClip jump;
  AudioClip miss;
  AudioClip missTwo;
  AudioClip crowdCheer;
  AudioClip birdCoinDrop;
  AudioClip bonusCount;
  AudioClip extraCharlie;
  AudioClip prizeBag;
  AudioClip hiddenCoin;
  std::array<AudioVoice, 10> voices{};
  bool available = false;
};

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
      << "  --capture FILE.png\n"
      << "  --capture-scene start|gameplay|ring|crash|goal|tally\n";
}

std::optional<Options> parseOptions(int argc, char** argv) {
  Options options;
  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];
    if (argument == "--fullscreen") {
      options.fullscreen = true;
    } else if (argument == "--debug") {
      options.debug = true;
    } else if (argument == "--capture" && index + 1 < argc) {
      options.capturePath = argv[++index];
    } else if (argument == "--capture-scene" && index + 1 < argc) {
      options.captureScene = argv[++index];
      if (options.captureScene != "start" &&
          options.captureScene != "gameplay" &&
          options.captureScene != "ring" &&
          options.captureScene != "crash" &&
          options.captureScene != "goal" &&
          options.captureScene != "tally") {
        std::cerr
            << "Capture scene must be start, gameplay, ring, crash, goal, or tally.\n";
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
  assets.arena = loadAsset(renderer, "stage1-arena.png");
  assets.marquee = loadAsset(renderer, "stage1-marquee-v2.png");
  assets.ferrisWheel = loadAsset(renderer, "stage1-ferris-wheel.png");
  assets.ferrisGondola = loadAsset(renderer, "stage1-ferris-gondola.png");
  assets.rider = loadAsset(renderer, "stage1-rider-sheet-v7.png");
  assets.hoop = loadAsset(renderer, "stage1-hoop.png");
  assets.hoopFlare = loadAsset(renderer, "stage1-hoop-flare.png");
  assets.props = loadAsset(renderer, "stage1-props.png");
  assets.propsFlare = loadAsset(renderer, "stage1-props-flare.png");
  assets.bird = loadAsset(renderer, "stage1-bird-sheet.png");
  assets.rewardBag = loadAsset(renderer, "stage1-reward-bag.png");
  assets.charlieLife = loadAsset(renderer, "stage1-charlie-life-v2.png");
  assets.goalPlatform = loadAsset(renderer, "stage1-goal-platform-v4.png");
  assets.finishRider =
      loadAsset(renderer, "stage1-finish-rider-sheet.png");
  if (!assets.arena || !assets.marquee || !assets.ferrisWheel ||
      !assets.ferrisGondola || !assets.rider || !assets.hoop ||
      !assets.hoopFlare || !assets.props || !assets.propsFlare ||
      !assets.bird || !assets.rewardBag || !assets.charlieLife ||
      !assets.goalPlatform || !assets.finishRider) {
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
  if (assets.hoop) SDL_DestroyTexture(assets.hoop);
  if (assets.hoopFlare) SDL_DestroyTexture(assets.hoopFlare);
  if (assets.props) SDL_DestroyTexture(assets.props);
  if (assets.propsFlare) SDL_DestroyTexture(assets.propsFlare);
  if (assets.bird) SDL_DestroyTexture(assets.bird);
  if (assets.rewardBag) SDL_DestroyTexture(assets.rewardBag);
  if (assets.charlieLife) SDL_DestroyTexture(assets.charlieLife);
  if (assets.goalPlatform) SDL_DestroyTexture(assets.goalPlatform);
  if (assets.finishRider) SDL_DestroyTexture(assets.finishRider);
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
      loadAudioAsset("jump.wav", audio.jump) &&
      loadAudioAsset("miss.wav", audio.miss) &&
      loadAudioAsset("miss-2.wav", audio.missTwo) &&
      loadAudioAsset("crowd-cheer.wav", audio.crowdCheer) &&
      loadAudioAsset("bird-coin-drop.wav", audio.birdCoinDrop) &&
      loadAudioAsset("bonus-count.wav", audio.bonusCount);
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
  // These effects intentionally remain optional until their exact arcade
  // command IDs have been verified. In particular, 0x41 is the credit-insert
  // sound and must not be used as a generic reward sound.
  loadAudioAsset("extra-charlie.wav", audio.extraCharlie);
  loadAudioAsset("prize-bag.wav", audio.prizeBag);
  loadAudioAsset("hidden-coin.wav", audio.hiddenCoin);

  if (!matchesReference(audio.jump) || !matchesReference(audio.miss) ||
      !matchesReference(audio.missTwo) ||
      !matchesReference(audio.crowdCheer) ||
      !matchesReference(audio.birdCoinDrop) ||
      !matchesReference(audio.bonusCount) ||
      (audio.extraCharlie.data && !matchesReference(audio.extraCharlie)) ||
      (audio.prizeBag.data && !matchesReference(audio.prizeBag)) ||
      (audio.hiddenCoin.data && !matchesReference(audio.hiddenCoin))) {
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
  if (musicVoice.active && musicVoice.clip == &audio.stageMusic) {
    musicVoice.playbackStep = fast ? 2U : 1U;
  }
  SDL_UnlockAudioDevice(audio.device);
}

void playStageMusic(AudioEngine& audio, bool fast) {
  setAudioVoice(audio, 0, audio.stageMusic,
                static_cast<int>(SDL_MIX_MAXVOLUME * 0.58F), true);
  setStageMusicFast(audio, fast);
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

void destroyAudio(AudioEngine& audio) {
  if (audio.device != 0) {
    SDL_PauseAudioDevice(audio.device, 1);
    SDL_CloseAudioDevice(audio.device);
  }
  if (audio.stageMusic.data) SDL_FreeWAV(audio.stageMusic.data);
  if (audio.jump.data) SDL_FreeWAV(audio.jump.data);
  if (audio.miss.data) SDL_FreeWAV(audio.miss.data);
  if (audio.missTwo.data) SDL_FreeWAV(audio.missTwo.data);
  if (audio.crowdCheer.data) SDL_FreeWAV(audio.crowdCheer.data);
  if (audio.birdCoinDrop.data) SDL_FreeWAV(audio.birdCoinDrop.data);
  if (audio.bonusCount.data) SDL_FreeWAV(audio.bonusCount.data);
  if (audio.extraCharlie.data) SDL_FreeWAV(audio.extraCharlie.data);
  if (audio.prizeBag.data) SDL_FreeWAV(audio.prizeBag.data);
  if (audio.hiddenCoin.data) SDL_FreeWAV(audio.hiddenCoin.data);
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
    x -= static_cast<float>(text.size()) * glyphAdvance * 0.5F;
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
  game.extraCharlieWorldX = 0.0F;

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
      {railStartForIntercept(1360.0F), 152.0F, false, false},
      {railStartForIntercept(3100.0F), 152.0F, false, false},
      {railStartForIntercept(3500.0F), 152.0F, false, false},
      {railStartForIntercept(4650.0F), 152.0F, false, false},
      {railStartForIntercept(5250.0F), 152.0F, false, false},
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
  game.instructionFrames = 420;
}

void restartAfterCrash(Game& game) {
  if (game.lives <= 0) {
    game.scene = Scene::Title;
    return;
  }
  game.scene = Scene::Playing;
  game.player.position.x = std::max(78.0F, game.player.position.x - 145.0F);
  game.player.position.y = kGroundY;
  game.player.previous = game.player.position;
  game.player.verticalVelocity = 0.0F;
  game.player.runSpeed = 0.0F;
  game.player.jumpFrame = -1;
  game.player.grounded = true;
  game.player.alive = true;
  game.crashFrame = 0;
  game.cameraX = std::max(0.0F, game.player.position.x - 78.0F);
  game.previousCameraX = game.cameraX;
  game.instructionFrames = 180;
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
  const float playerLeft = player.position.x - kLionCollisionLeft;
  const float playerRight = player.position.x + kLionCollisionRight;
  const float hoopLeft = hoop.worldX - 15.0F;
  const float hoopRight = hoop.worldX + 15.0F;
  if (playerRight < hoopLeft || playerLeft > hoopRight) return false;

  const float playerTop = player.position.y - kLionCollisionTop;
  const float playerBottom = player.position.y - kLionCollisionBottom;
  const bool withinOpening =
      playerTop > hoop.openingTop + 8.0F &&
      playerBottom < hoop.openingBottom - 3.0F;
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

float firePotCoinY(const FirePot& firePot) {
  const float progress = std::clamp(
      static_cast<float>(firePot.coinFrame) /
          static_cast<float>(kCoinFlightFrames),
      0.0F, 1.0F);
  return kGroundY - 30.0F - std::sin(progress * kPi) * kCoinArcHeight;
}

void finishStage(Game& game) {
  game.player.position = {kCourseLength, kGoalLandingY};
  game.player.previous = game.player.position;
  game.player.runSpeed = 0.0F;
  game.player.verticalVelocity = 0.0F;
  game.player.jumpFrame = -1;
  game.player.grounded = true;
  game.cameraX = kCourseLength - kGoalScreenX;
  game.previousCameraX = game.cameraX;
  game.perfectClear =
      !game.deathOccurred && game.prizeBagsAvailable > 0 &&
      game.prizeBagsCollected == game.prizeBagsAvailable;
  game.score += 500;
  game.goalFrame = 0;
  game.scene = Scene::Goal;
}

void updateGame(Game& game, const Uint8* keyboard, bool jumpPressed,
                float controllerAxis) {
  if (game.scene == Scene::Crashed) {
    game.player.previous = game.player.position;
    game.previousCameraX = game.cameraX;
    game.player.runSpeed = 0.0F;
    game.player.verticalVelocity = 0.0F;
    game.crashFrame =
        std::min(game.crashFrame + 1, kCrashBurnFrames);
    return;
  }

  if (game.scene == Scene::Goal) {
    game.player.previous = game.player.position;
    game.previousCameraX = game.cameraX;
    game.player.runSpeed = 0.0F;
    game.player.position.y = kGoalLandingY;
    game.player.previous.y = game.player.position.y;
    game.player.grounded = true;
    game.player.jumpFrame = -1;
    game.cameraX = game.player.position.x - kGoalScreenX;
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

    const int presentationFrames =
        game.perfectClear
            ? showerStart + kCoinShowerFrames
            : kGoalArrivalFrames + 120;
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
  if (game.instructionFrames > 0) --game.instructionFrames;

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
  game.cameraX =
      std::max(0.0F, game.player.position.x - 78.0F);

  const float ringTravel =
      kRingRailSpeed * static_cast<float>(kFixedDt);
  for (auto& hoop : game.hoops) {
    if (!hoop.cleared &&
        hoop.worldX - game.cameraX <= kRingActivationLead) {
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

  if (game.extraCharlieActive) {
    game.extraCharlieWorldX -= ringTravel;
    if (std::abs(game.extraCharlieWorldX - game.player.position.x) < 42.0F) {
      game.extraCharlieActive = false;
      game.extraCharlieCollected = true;
      ++game.lives;
      game.score += 1000;
      ++game.extraCharlieAudioSerial;
    } else if (game.extraCharlieWorldX < game.player.position.x - 95.0F) {
      game.extraCharlieActive = false;
    }
  }

  if (jumpPressed && game.player.grounded) {
    // In the recorded Event 1 opening, three reverse jumps summon a Charlie
    // doll on the overhead rail. The player earns the extra life only by
    // intercepting that moving doll, not at the instant of the third jump.
    const bool openingReverseJump =
        moveLeft && game.player.position.x < 240.0F &&
        !game.extraCharlieActive && !game.extraCharlieCollected;
    if (openingReverseJump) {
      ++game.openingBackwardJumps;
      if (game.openingBackwardJumps >= 3) {
        game.extraCharlieActive = true;
        game.extraCharlieWorldX = game.player.position.x + 430.0F;
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

  for (auto& hoop : game.hoops) {
    if (!hoop.cleared && game.player.position.x > hoop.worldX + 46.0F) {
      hoop.cleared = true;
      game.score += 100;
    }
    if (!hoop.cleared && overlapsHoop(game.player, hoop)) {
      crashPlayer(game);
      return;
    }
  }

  for (auto& firePot : game.firePots) {
    const float potDistance =
        game.player.position.x - firePot.worldX;
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
        game.player.position.x <
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
          std::abs(game.player.position.x - firePot.worldX) < 44.0F &&
          std::abs(riderCenterY - coinY) < 44.0F) {
        firePot.coinCollected = true;
        firePot.coinActive = false;
        game.score += 500;
        ++game.hiddenCoinAudioSerial;
      } else if (firePot.coinFrame >= kCoinFlightFrames) {
        firePot.coinActive = false;
      }
    }

    if (!sharesHoopLane &&
        std::abs(game.player.position.x - firePot.worldX) <
            kFirePotCollisionHalfWidth &&
        game.player.position.y > kGroundY - kFirePotClearance) {
      crashPlayer(game);
      return;
    }
  }

  for (auto& ring : game.bonusRings) {
    // In the arcade game Charlie can run safely beneath a small suspended
    // ring. Its flame rim is tested only when the player commits to a jump.
    if (game.player.grounded) continue;

    const float ringCenterY = kGroundY - ring.height;
    const float playerLeft =
        game.player.position.x - kLionCollisionLeft;
    const float playerRight =
        game.player.position.x + kLionCollisionRight;
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
      game.score += ring.containsPrize ? 500 : 200;
      if (ring.containsPrize) {
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
  fillRect(renderer, 0.0F, kTrackY, kWorldWidth, 8.0F,
           color(104, 117, 133));
  fillRect(renderer, 0.0F, kTrackY + 2.0F, kWorldWidth, 2.0F,
           color(214, 222, 226));
  for (int clamp = -1; clamp < 14; ++clamp) {
    const float x = static_cast<float>(clamp * 42) -
                    std::fmod(cameraX, 42.0F);
    fillRect(renderer, x, kTrackY - 6.0F, 8.0F, 18.0F,
             color(70, 76, 88));
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
  std::ostringstream value;
  value << std::setw(4) << std::setfill('0') << bonus;
  drawText(renderer, value.str(), kZeppelinPanelCenterX, 50.0F, 1.45F,
           color(255, 255, 255), true);
}

void drawBackdrop(SDL_Renderer* renderer, float cameraX, bool lowDetail,
                  const Assets& assets, const Game& game,
                  double timeSeconds) {
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
    const int sourceTop = static_cast<int>(
        static_cast<float>(textureHeight) * kCrowdTop /
        static_cast<float>(kWorldHeight));
    const SDL_Rect source{0, sourceTop, textureWidth,
                          textureHeight - sourceTop};
    const float tileWidth = static_cast<float>(kWorldWidth);
    const float scroll = std::fmod(cameraX, tileWidth);
    for (int tile = -1; tile <= 1; ++tile) {
      const SDL_FRect destination{
          static_cast<float>(tile) * tileWidth - scroll,
          kCrowdTop,
          tileWidth,
          static_cast<float>(kWorldHeight) - kCrowdTop,
      };
      SDL_RenderCopyF(renderer, assets.arena, &source, &destination);
    }
  } else {
    fillRect(renderer, 0.0F, kArenaTop, kWorldWidth,
             kWorldHeight - kArenaTop, color(24, 25, 45));
    const float crowdScroll = std::fmod(cameraX, 23.0F);
    for (int row = 0; row < (lowDetail ? 3 : 5); ++row) {
      for (int column = -1; column < 23; ++column) {
        const float x = static_cast<float>(column * 23) - crowdScroll +
                        static_cast<float>((row % 2) * 9);
        const float y = 310.0F + static_cast<float>(row * 21);
        filledCircle(renderer, x, y, 5.0F,
                     color(45 + row * 8, 50, 72));
      }
    }
    fillRect(renderer, 0.0F, 462.0F, kWorldWidth, 12.0F,
             color(112, 23, 37));
    fillRect(renderer, 0.0F, 474.0F, kWorldWidth, 68.0F,
             color(190, 126, 52));
    for (int stripe = 0; stripe < kWorldWidth / 28 + 2; ++stripe) {
      const float x = static_cast<float>(stripe * 28) -
                      std::fmod(cameraX, 28.0F);
      line(renderer, x, 478.0F, x - 22.0F, 537.0F,
           color(211, 151, 72));
    }
    fillRect(renderer, 0.0F, kGroundY, kWorldWidth, 108.0F,
             color(89, 35, 30));
    fillRect(renderer, 0.0F, kGroundY, kWorldWidth, 5.0F,
             color(245, 183, 79));
  }

  // This gameplay rail belongs to the scrolling arena, while every fire ring
  // also has its own measured independent motion along it.
  drawCeilingTrack(renderer, cameraX);
}

void drawHoop(SDL_Renderer* renderer, const Hoop& hoop, float cameraX,
              bool lowDetail, SDL_Texture* hoopTexture) {
  const float x = hoop.worldX - cameraX;
  if (x < -80.0F || x > kWorldWidth + 80.0F || hoop.cleared) return;
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

void drawStageProps(SDL_Renderer* renderer, const Game& game, float cameraX,
                    SDL_Texture* propsTexture) {
  if (!propsTexture) return;
  int textureWidth = 0;
  int textureHeight = 0;
  SDL_QueryTexture(propsTexture, nullptr, nullptr, &textureWidth,
                   &textureHeight);
  const int cellWidth = textureWidth / 3;
  const SDL_Rect fireSource{0, 0, cellWidth, textureHeight};
  const SDL_Rect bagSource{cellWidth, 0, cellWidth, textureHeight};
  for (const auto& firePot : game.firePots) {
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
      const float coinWidth = 9.0F * flip;
      ellipse(renderer, screenX, coinY, coinWidth, 9.0F,
              color(255, 246, 115), 3);
      ellipse(renderer, screenX, coinY, std::max(1.0F, coinWidth - 2.5F),
              6.0F, color(226, 146, 20), 2);
      fillRect(renderer, screenX - coinWidth * 0.35F, coinY - 5.0F,
               std::max(1.0F, coinWidth * 0.20F), 3.0F,
               color(255, 255, 221));
    }
  }

  for (const auto& ring : game.bonusRings) {
    const float screenX = ring.worldX - cameraX;
    if (screenX < -100.0F || screenX > kWorldWidth + 100.0F) continue;
    const float ringCenterY = kGroundY - ring.height;
    line(renderer, screenX, kTrackY + 5.0F, screenX,
         ringCenterY - 55.0F, color(119, 101, 73));
    // Like the original board's category-0 tiles, the animated flame rim is
    // deferred to the foreground pass. The hanger and prize remain here.
    if (ring.containsPrize && !ring.collected) {
      const SDL_FRect bagDestination{screenX - 22.0F, ringCenterY - 44.0F,
                                     44.0F, 88.0F};
      SDL_RenderCopyF(renderer, propsTexture, &bagSource, &bagDestination);
    }
  }
}

void drawHoopForeground(SDL_Renderer* renderer, const Hoop& hoop,
                        float cameraX, SDL_Texture* hoopTexture) {
  if (!hoopTexture || hoop.cleared) return;
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
  constexpr float kFinishWidth = 116.0F;
  constexpr float kFinishHeight = 122.0F;
  const float landingProgress = std::clamp(
      static_cast<float>(goalFrame) / 18.0F, 0.0F, 1.0F);
  const float arrivalX = (1.0F - landingProgress) * -26.0F;
  const float arrivalY = (1.0F - landingProgress) * -19.0F;
  const SDL_FRect destination{
      screenX - kFinishWidth * 0.5F + arrivalX,
      kGoalLandingY - 103.0F + arrivalY, kFinishWidth, kFinishHeight};
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
  if (!game.extraCharlieActive) return;
  const float x = game.extraCharlieWorldX - cameraX;
  if (x < -60.0F || x > kWorldWidth + 60.0F) return;

  const float sway = std::sin(static_cast<float>(timeSeconds) * 5.0F) * 3.0F;
  const float headY = 394.0F + sway;
  line(renderer, x, kTrackY + 4.0F, x, headY - 19.0F,
       color(220, 224, 230));
  if (charlieTexture) {
    const SDL_FRect destination{x - 30.0F, headY - 25.0F, 60.0F, 60.0F};
    SDL_RenderCopyF(renderer, charlieTexture, nullptr, &destination);
    return;
  }
  filledCircle(renderer, x, headY, 14.0F, color(250, 218, 186));
  filledCircle(renderer, x + 12.0F, headY + 1.0F, 4.5F,
               color(224, 44, 47));
  filledCircle(renderer, x - 5.0F, headY + 2.0F, 3.0F,
               color(55, 126, 213));
  fillRect(renderer, x - 12.0F, headY + 14.0F, 24.0F, 31.0F,
           color(190, 31, 43));
  line(renderer, x - 8.0F, headY + 42.0F, x - 14.0F, headY + 61.0F,
       color(45, 106, 202));
  line(renderer, x + 8.0F, headY + 42.0F, x + 14.0F, headY + 61.0F,
       color(45, 106, 202));
  const std::array<SDL_Vertex, 3> cap{{
      {{x - 13.0F, headY - 10.0F}, color(43, 99, 190), {0, 0}},
      {{x - 3.0F, headY - 34.0F}, color(72, 142, 224), {0, 0}},
      {{x + 11.0F, headY - 11.0F}, color(43, 99, 190), {0, 0}},
  }};
  SDL_RenderGeometry(renderer, nullptr, cap.data(),
                     static_cast<int>(cap.size()), nullptr, 0);
  filledCircle(renderer, x - 4.0F, headY - 35.0F, 4.0F,
               color(248, 204, 45));
}

void drawCoin(SDL_Renderer* renderer, float x, float y, float squash = 1.0F) {
  const float width = 9.0F * std::max(0.25F, squash);
  ellipse(renderer, x, y, width, 9.0F, color(255, 255, 123), 3);
  ellipse(renderer, x, y, std::max(1.0F, width - 3.0F), 6.0F,
          color(235, 155, 27), 2);
}

void drawGoalPresentation(SDL_Renderer* renderer, const Game& game,
                          SDL_Texture* birdTexture,
                          SDL_Texture* rewardBagTexture,
                          SDL_Texture* propsTexture) {
  const SDL_Color cheerColor =
      ((game.goalFrame / 12) & 1) == 0 ? color(255, 93, 36)
                                       : color(87, 219, 255);
  if (game.goalFrame > 30) {
    const float bounce =
        ((game.goalFrame / 8) & 1) == 0 ? 0.0F : -5.0F;
    for (int spectator = 0; spectator < 18; ++spectator) {
      const float x = 10.0F + spectator * 27.0F;
      const float wave =
          std::sin(static_cast<float>(game.goalFrame + spectator * 5) *
                   0.18F);
      const float y = 387.0F + wave * 7.0F;
      line(renderer, x, y + 13.0F, x - 5.0F, y - 4.0F, cheerColor);
      line(renderer, x, y + 13.0F, x + 5.0F, y - 7.0F,
           color(255, 213, 70));
      filledCircle(renderer, x, y + 4.0F, 3.0F,
                   color(246, 184, 130));
    }
    const auto outlinedCheer = [&](std::string_view text, float x, float y,
                                   SDL_Color value) {
      for (const auto& offset : std::array<Vec2, 4>{
               Vec2{-2.0F, 0.0F}, Vec2{2.0F, 0.0F},
               Vec2{0.0F, -2.0F}, Vec2{0.0F, 2.0F}}) {
        drawText(renderer, text, x + offset.x, y + offset.y, 2.0F,
                 color(45, 10, 24), true);
      }
      drawText(renderer, text, x, y, 2.0F, value, true);
    };
    outlinedCheer("GREAT", 96.0F, 326.0F + bounce, cheerColor);
    outlinedCheer("FAROUT", 383.0F, 346.0F - bounce,
                  color(255, 130, 42));
  }
  if (!game.perfectClear) return;

  const int birdStart = kGoalArrivalFrames;
  const int bagDropStart = birdStart + kBirdArrivalFrames;
  const int showerStart = bagDropStart + kBagDropFrames;
  if (birdTexture && game.goalFrame >= birdStart &&
      game.goalFrame < bagDropStart + kBirdExitFrames) {
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
      birdX = 510.0F + (kGoalScreenX - 510.0F) * progress;
      cell = (game.goalFrame / 8) & 1;
    } else {
      const float exitProgress = std::clamp(
          static_cast<float>(game.goalFrame - bagDropStart) /
              static_cast<float>(kBirdExitFrames),
          0.0F, 1.0F);
      birdX = kGoalScreenX - exitProgress * 220.0F;
      cell = 3;
    }
    const SDL_Rect source{cell * cellWidth, 0, cellWidth, textureHeight};
    // Keep the reward bird in the arena below the persistent LED/HUD panel.
    const SDL_FRect destination{birdX - 45.0F, 205.0F, 90.0F, 120.0F};
    SDL_RenderCopyF(renderer, birdTexture, &source, &destination);
  }

  constexpr float kRewardBagX = kGoalScreenX;
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
      const float x = kRewardBagX + lane * 0.55F * spread;
      const float y = kRewardBagRestY + 30.0F +
                      static_cast<float>(localFrame) * 2.05F;
      drawCoin(renderer, x, y,
               std::abs(std::sin(static_cast<float>(localFrame) * 0.24F)));
    }
  }

  if (showRewardBag) {
    if (rewardBagTexture) {
      const SDL_FRect bagDestination{kRewardBagX - 24.0F,
                                     rewardBagY - 30.0F, 48.0F, 59.0F};
      SDL_RenderCopyF(renderer, rewardBagTexture, nullptr, &bagDestination);
    } else if (propsTexture) {
      int textureWidth = 0;
      int textureHeight = 0;
      SDL_QueryTexture(propsTexture, nullptr, nullptr, &textureWidth,
                       &textureHeight);
      const int cellWidth = textureWidth / 3;
      const SDL_Rect bagSource{cellWidth, 0, cellWidth, textureHeight};
      const SDL_FRect bagDestination{kRewardBagX - 25.0F,
                                     rewardBagY - 35.0F, 50.0F, 100.0F};
      SDL_RenderCopyF(renderer, propsTexture, &bagSource, &bagDestination);
    } else {
      ellipse(renderer, kRewardBagX, rewardBagY + 10.0F, 18.0F, 24.0F,
              color(223, 158, 39), 4);
      fillRect(renderer, kRewardBagX - 12.0F, rewardBagY - 16.0F, 24.0F,
               7.0F, color(118, 68, 21));
    }
  }
}

void drawLionAndRider(SDL_Renderer* renderer, float screenX, float groundY,
                      double timeSeconds, bool alive, bool lowDetail,
                      SDL_Texture* riderTexture, float runSpeed, bool grounded,
                      bool facingRight) {
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
      // Cycling all three grounded poses preserves that cadence while making
      // a complete stride slower and more readable than the old two-frame
      // toggle.
      frame = static_cast<int>(timeSeconds * (kBoardRefresh / 7.5) *
                               kStrideAnimationSpeedScale) %
              3;
    }

    const SDL_Rect source{(frame % 3) * cellWidth, (frame / 3) * cellHeight,
                          cellWidth, cellHeight};
    // The original six-tile composite is 48x32 source pixels and its visible
    // grounded pose is about 47x28. These non-square dimensions preserve that
    // measured silhouette on the 480x640 logical canvas.
    constexpr float kSpriteWidth = 116.0F;
    constexpr float kSpriteHeight = 96.0F;
    constexpr std::array<float, 6> kAnchorCorrection{
        12.0F, 12.0F, 12.0F, 12.0F, 12.0F, 12.0F};
    const SDL_FRect destination{
        screenX - 38.0F,
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
                      SDL_Texture* propsTexture, int crashFrame) {
  if (!propsTexture) return;

  int textureWidth = 0;
  int textureHeight = 0;
  SDL_QueryTexture(propsTexture, nullptr, nullptr, &textureWidth,
                   &textureHeight);
  const int cellWidth = textureWidth / 3;
  // Crop only the living flame from the first prop cell, leaving its floor
  // cauldron behind. Three overlapping copies engulf Charlie and the lion as
  // one crash animation rather than making either character simply vanish.
  const SDL_Rect flameSource{
      static_cast<int>(cellWidth * 0.34F),
      static_cast<int>(textureHeight * 0.15F),
      static_cast<int>(cellWidth * 0.56F),
      static_cast<int>(textureHeight * 0.47F),
  };
  const float arrival =
      std::clamp(static_cast<float>(crashFrame) / 10.0F, 0.15F, 1.0F);
  const float flicker = ((crashFrame / 5) & 1) == 0 ? 0.0F : 5.0F;
  SDL_SetTextureAlphaMod(
      propsTexture, static_cast<Uint8>(100.0F + arrival * 45.0F));

  const SDL_FRect lionRear{
      screenX - 58.0F, groundY - 94.0F - flicker * 0.35F,
      70.0F * arrival, 100.0F * arrival};
  const SDL_FRect lionFront{
      screenX - 3.0F, groundY - 121.0F + flicker * 0.25F,
      77.0F * arrival, 129.0F * arrival};
  const SDL_FRect charlie{
      screenX - 28.0F, groundY - 165.0F - flicker,
      61.0F * arrival, 116.0F * arrival};
  SDL_RenderCopyExF(renderer, propsTexture, &flameSource, &lionRear, 0.0,
                    nullptr, SDL_FLIP_HORIZONTAL);
  SDL_RenderCopyF(renderer, propsTexture, &flameSource, &lionFront);
  SDL_RenderCopyExF(renderer, propsTexture, &flameSource, &charlie, 0.0,
                    nullptr, SDL_FLIP_HORIZONTAL);
  SDL_SetTextureAlphaMod(propsTexture, 255);
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

void drawHud(SDL_Renderer* renderer, const Game& game,
             SDL_Texture* charlieTexture) {
  fillRect(renderer, 0.0F, kHudTop, kWorldWidth, kHudHeight,
           color(0, 0, 0, 248));
  drawHudBulbs(renderer);

  drawText(renderer, "1UP", 12.0F, kHudTop + 8.0F, 1.35F,
           color(255, 230, 34));
  drawText(renderer, std::to_string(game.score), 160.0F, kHudTop + 27.0F,
           1.65F, color(255, 255, 255));

  drawText(renderer, "HIGH SCORE", kWorldWidth * 0.5F, kHudTop + 8.0F,
           1.25F, color(245, 70, 37), true);
  drawText(renderer, std::to_string(std::max(19830, game.score)),
           kWorldWidth * 0.5F, kHudTop + 28.0F, 1.65F,
           color(51, 213, 57), true);

  const int waitingCharlies = std::clamp(game.lives - 1, 0, 5);
  for (int life = 0; life < waitingCharlies; ++life) {
    drawCharlieLifeIcon(renderer, charlieTexture, 32.0F + life * 52.0F,
                        kHudTop + 20.0F);
  }
  drawText(renderer, "CREDIT 00", 312.0F, kHudTop + 46.0F, 2.9F,
           color(70, 202, 255));

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
    drawText(renderer, "EVENT 1 COMPLETE", kWorldWidth * 0.5F, 552.0F,
             2.0F, color(92, 235, 139), true);
    drawText(renderer, "PRESS ENTER", kWorldWidth * 0.5F, 588.0F,
             1.8F, color(255, 255, 255), true);
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
                                  std::lround(kGroundY -
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
  if (game.scene == Scene::Tally || game.scene == Scene::Complete) {
    drawTallyScreen(renderer, game, game.scene == Scene::Complete, assets,
                    timeSeconds);
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
  drawStageProps(renderer, game, camera, propsFrame);
  drawExtraCharlie(renderer, game, camera, timeSeconds, assets.charlieLife);

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
    } else {
      drawLionAndRider(renderer, playerWorldX - camera, playerY, timeSeconds,
                       game.player.alive, lowDetail, assets.rider,
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
      drawBurningRider(renderer, playerWorldX - camera, playerY, propsFrame,
                       game.crashFrame);
      // Redraw the heat-tinted rider over the fire bed so both Charlie and the
      // lion remain readable while the flames surround and cross their edges.
      drawLionAndRider(renderer, playerWorldX - camera, playerY, timeSeconds,
                       false, lowDetail, assets.rider, 0.0F, true,
                       game.player.facingRight);
    }
    if (game.scene == Scene::Goal) {
      drawGoalPresentation(renderer, game, assets.bird, assets.rewardBag,
                           assets.props);
    }
  }
  drawHud(renderer, game, assets.charlieLife);

  if (game.scene == Scene::Title) {
    fillRect(renderer, 33.0F, 210.0F, 414.0F, 270.0F,
             color(3, 9, 29, 225));
    drawText(renderer, "BIG TOP", kWorldWidth * 0.5F, 246.0F, 6.0F,
             color(255, 202, 56), true);
    drawText(renderer, "RUN", kWorldWidth * 0.5F, 305.0F, 8.0F,
             color(225, 52, 54), true);
    drawText(renderer, "NATIVE ARCADE PROTOTYPE", kWorldWidth * 0.5F,
             371.0F, 1.6F, color(160, 211, 255), true);
    drawText(renderer, "LEFT RIGHT MOVE LION", kWorldWidth * 0.5F, 399.0F,
             1.5F, color(255, 255, 255), true);
    drawText(renderer, "SPACE OR Z JUMP", kWorldWidth * 0.5F, 422.0F,
             1.5F, color(255, 255, 255), true);
    drawText(renderer, "PRESS ENTER OR 1", kWorldWidth * 0.5F, 449.0F,
             2.0F, color(255, 255, 255), true);
  } else if (game.scene == Scene::Crashed &&
             game.crashFrame >= kCrashBurnFrames) {
    fillRect(renderer, 55.0F, 220.0F, 370.0F, 134.0F, color(33, 5, 8, 232));
    drawText(renderer, "MISSED THE HOOP", kWorldWidth * 0.5F, 246.0F, 2.4F,
             color(255, 96, 64), true);
    drawText(renderer, "SPACE OR Z TO RETRY", kWorldWidth * 0.5F, 300.0F,
             1.8F, color(255, 255, 255), true);
  }

  if (game.scene == Scene::Playing && game.instructionFrames > 0) {
    fillRect(renderer, 61.0F, kArenaTop + 12.0F, 358.0F, 66.0F,
             color(2, 8, 23, 218));
    drawText(renderer, "LEFT RIGHT MOVE LION", kWorldWidth * 0.5F,
             kArenaTop + 25.0F,
             1.6F, color(255, 227, 119), true);
    drawText(renderer, "SPACE OR Z JUMP", kWorldWidth * 0.5F,
             kArenaTop + 51.0F,
             1.7F, color(255, 255, 255), true);
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
  game.randomState ^=
      static_cast<std::uint32_t>(SDL_GetPerformanceCounter());
  game.debug = options.debug;
  resetCourse(game);
  if (!options.capturePath.empty()) {
    startGame(game);
    game.instructionFrames = 0;
    if (options.captureScene == "ring") {
      game.player.position = {800.0F, kGroundY - 137.0F};
      game.player.previous = game.player.position;
      game.player.grounded = false;
      game.player.jumpFrame = 31;
      game.player.verticalVelocity = 0.0F;
      game.cameraX = game.player.position.x - 78.0F;
      game.previousCameraX = game.cameraX;
      for (auto& hoop : game.hoops) hoop.cleared = true;
      game.bonusRings.front().worldX = game.player.position.x;
      game.bonusRings.front().containsPrize = true;
      game.bonusRings.front().collected = true;
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
    } else if (options.captureScene == "goal") {
      game.scene = Scene::Goal;
      game.player.position = {kCourseLength, kGoalLandingY};
      game.player.previous = game.player.position;
      game.player.grounded = true;
      game.player.runSpeed = 0.0F;
      game.cameraX = kCourseLength - kGoalScreenX;
      game.previousCameraX = game.cameraX;
      game.perfectClear = true;
      game.goalFrame = kGoalArrivalFrames + kBirdArrivalFrames +
                       kBagDropFrames + 30;
      for (auto& hoop : game.hoops) hoop.cleared = true;
      for (auto& ring : game.bonusRings) ring.collected = true;
    } else if (options.captureScene == "tally") {
      game.scene = Scene::Tally;
      game.bonus = 3722;
      game.score = 15440;
      game.timeScoreApplied = true;
      game.clearBonus = timeBonusFor(game.bonus);
      game.score += game.clearBonus;
      game.tallyFrame = 190;
    } else if (options.captureScene == "start") {
      game.player.position = {78.0F, kGroundY};
      game.player.previous = game.player.position;
      game.player.grounded = true;
      game.player.runSpeed = 0.0F;
      game.cameraX = 0.0F;
      game.previousCameraX = 0.0F;
    } else {
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
  bool stageMusicPlaying = false;
  bool stageMusicFast = false;
  std::uint32_t observedJumpAudioSerial = game.jumpAudioSerial;
  std::uint32_t observedCrashAudioSerial = game.crashAudioSerial;
  std::uint32_t observedExtraCharlieAudioSerial =
      game.extraCharlieAudioSerial;
  std::uint32_t observedPrizeBagAudioSerial = game.prizeBagAudioSerial;
  std::uint32_t observedHiddenCoinAudioSerial = game.hiddenCoinAudioSerial;
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
      } else if (event.type == SDL_CONTROLLERBUTTONDOWN &&
                 event.cbutton.button == SDL_CONTROLLER_BUTTON_A) {
        jumpQueued = true;
      } else if (event.type == SDL_KEYDOWN && !event.key.repeat) {
        switch (event.key.keysym.sym) {
          case SDLK_ESCAPE:
            if (game.scene == Scene::Title) {
              running = false;
            } else {
              game.scene = Scene::Title;
              resetCourse(game);
            }
            break;
          case SDLK_RETURN:
          case SDLK_1:
            if (game.scene == Scene::Title) {
              startGame(game);
            } else if (game.scene == Scene::Complete) {
              startGame(game);
            }
            break;
          case SDLK_SPACE:
          case SDLK_z:
            jumpQueued = true;
            break;
          case SDLK_r:
            resetCourse(game);
            game.scene = Scene::Playing;
            game.instructionFrames = 180;
            break;
          case SDLK_F1:
            game.debug = !game.debug;
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
      jumpForStep = false;
      jumpQueued = false;
      accumulator -= kFixedDt;
    }

    const bool shouldPlayStageMusic = game.scene == Scene::Playing;
    const bool shouldUseFastStageMusic =
        shouldPlayStageMusic && game.bonus <= 999;
    if (shouldPlayStageMusic != stageMusicPlaying) {
      if (shouldPlayStageMusic) {
        playStageMusic(audio, shouldUseFastStageMusic);
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
    if (game.scene != observedScene) {
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
