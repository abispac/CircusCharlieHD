#include <SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
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
constexpr float kTrackY = 344.0F;
constexpr float kGravity = 550.0F;
constexpr float kJumpImpulse = -365.0F;
constexpr float kSlowSpeed = 120.0F;
constexpr float kCruiseSpeed = 185.0F;
constexpr float kFastSpeed = 260.0F;
constexpr float kCourseLength = 4100.0F;
constexpr float kPi = 3.14159265358979323846F;

struct Options {
  int width = 480;
  int height = 640;
  int rotation = 0;
  bool fullscreen = false;
  bool debug = false;
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

struct Player {
  Vec2 position{78.0F, kGroundY};
  Vec2 previous = position;
  float verticalVelocity = 0.0F;
  float runSpeed = kCruiseSpeed;
  bool grounded = true;
  bool alive = true;
};

enum class Scene {
  Title,
  Playing,
  Crashed,
  Complete,
};

struct Game {
  Scene scene = Scene::Title;
  Player player;
  std::vector<Hoop> hoops;
  float cameraX = 0.0F;
  float previousCameraX = 0.0F;
  int score = 0;
  int lives = 3;
  bool debug = false;
};

struct RenderSurface {
  SDL_Texture* texture = nullptr;
  int width = 0;
  int height = 0;
  SDL_Rect destination{0, 0, 0, 0};
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
      << "  --debug\n";
}

std::optional<Options> parseOptions(int argc, char** argv) {
  Options options;
  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];
    if (argument == "--fullscreen") {
      options.fullscreen = true;
    } else if (argument == "--debug") {
      options.debug = true;
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
      return std::nullopt;
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

void resetCourse(Game& game) {
  game.player = Player{};
  game.cameraX = 0.0F;
  game.previousCameraX = 0.0F;
  game.score = 0;
  game.hoops = {
      {560.0F, kGroundY - 18.0F, kGroundY - 150.0F, false},
      {890.0F, kGroundY - 18.0F, kGroundY - 138.0F, false},
      {1220.0F, kGroundY - 18.0F, kGroundY - 158.0F, false},
      {1580.0F, kGroundY - 18.0F, kGroundY - 142.0F, false},
      {1940.0F, kGroundY - 18.0F, kGroundY - 150.0F, false},
      {2300.0F, kGroundY - 18.0F, kGroundY - 132.0F, false},
      {2700.0F, kGroundY - 18.0F, kGroundY - 160.0F, false},
      {3100.0F, kGroundY - 18.0F, kGroundY - 142.0F, false},
      {3480.0F, kGroundY - 18.0F, kGroundY - 154.0F, false},
  };
}

void startGame(Game& game) {
  game.scene = Scene::Playing;
  game.lives = 3;
  resetCourse(game);
}

void restartAfterCrash(Game& game) {
  if (game.lives <= 0) {
    game.scene = Scene::Title;
    return;
  }
  game.scene = Scene::Playing;
  game.player.position.y = kGroundY;
  game.player.previous = game.player.position;
  game.player.verticalVelocity = 0.0F;
  game.player.grounded = true;
  game.player.alive = true;
}

bool overlapsHoop(const Player& player, const Hoop& hoop) {
  const float playerLeft = player.position.x - 34.0F;
  const float playerRight = player.position.x + 42.0F;
  const float hoopLeft = hoop.worldX - 15.0F;
  const float hoopRight = hoop.worldX + 15.0F;
  if (playerRight < hoopLeft || playerLeft > hoopRight) return false;

  const float playerTop = player.position.y - 82.0F;
  const float playerBottom = player.position.y - 6.0F;
  const bool withinOpening =
      playerTop > hoop.openingTop + 6.0F &&
      playerBottom < hoop.openingBottom - 3.0F;
  return !withinOpening;
}

void updateGame(Game& game, const Uint8* keyboard, bool jumpPressed,
                float controllerAxis) {
  if (game.scene != Scene::Playing) return;

  game.player.previous = game.player.position;
  game.previousCameraX = game.cameraX;

  const bool slow = keyboard[SDL_SCANCODE_LEFT] ||
                    keyboard[SDL_SCANCODE_A] || controllerAxis < -0.35F;
  const bool fast = keyboard[SDL_SCANCODE_RIGHT] ||
                    keyboard[SDL_SCANCODE_D] || controllerAxis > 0.35F;
  const float targetSpeed =
      slow ? kSlowSpeed : (fast ? kFastSpeed : kCruiseSpeed);
  game.player.runSpeed +=
      (targetSpeed - game.player.runSpeed) * static_cast<float>(kFixedDt) * 8.0F;

  game.player.position.x +=
      game.player.runSpeed * static_cast<float>(kFixedDt);
  game.cameraX =
      std::max(0.0F, game.player.position.x - 78.0F);

  if (jumpPressed && game.player.grounded) {
    game.player.grounded = false;
    game.player.verticalVelocity = kJumpImpulse;
  }

  if (!game.player.grounded) {
    game.player.verticalVelocity +=
        kGravity * static_cast<float>(kFixedDt);
    game.player.position.y +=
        game.player.verticalVelocity * static_cast<float>(kFixedDt);
    if (game.player.position.y >= kGroundY) {
      game.player.position.y = kGroundY;
      game.player.verticalVelocity = 0.0F;
      game.player.grounded = true;
    }
  }

  for (auto& hoop : game.hoops) {
    if (!hoop.cleared && game.player.position.x > hoop.worldX + 46.0F) {
      hoop.cleared = true;
      game.score += 100;
    }
    if (!hoop.cleared && overlapsHoop(game.player, hoop)) {
      game.player.alive = false;
      game.scene = Scene::Crashed;
      --game.lives;
      return;
    }
  }

  game.score =
      std::max(game.score, static_cast<int>(game.player.position.x / 10.0F));
  if (game.player.position.x >= kCourseLength) {
    game.score += 1000;
    game.scene = Scene::Complete;
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

void drawBackdrop(SDL_Renderer* renderer, float cameraX, bool lowDetail) {
  fillRect(renderer, 0.0F, 0.0F, kWorldWidth, kWorldHeight,
           color(5, 18, 51));

  const float farScroll = std::fmod(cameraX * 0.08F, 260.0F);
  if (!lowDetail) {
    for (int index = 0; index < 34; ++index) {
      const float x =
          std::fmod(static_cast<float>(index * 79) - farScroll + 520.0F,
                    520.0F) -
          20.0F;
      const float y = 25.0F + static_cast<float>((index * 47) % 245);
      filledCircle(renderer, x, y, index % 5 == 0 ? 1.5F : 0.8F,
                   color(170, 215, 255));
    }
  }

  const float tentScroll = std::fmod(cameraX * 0.2F, 340.0F);
  for (int tent = -1; tent < 3; ++tent) {
    const float center = static_cast<float>(tent * 340) - tentScroll + 145.0F;
    const std::array<SDL_Vertex, 3> roof{{
        {{center - 155.0F, 355.0F}, color(92, 17, 35), {0, 0}},
        {{center, 165.0F}, color(196, 47, 46), {0, 0}},
        {{center + 155.0F, 355.0F}, color(92, 17, 35), {0, 0}},
    }};
    SDL_RenderGeometry(renderer, nullptr, roof.data(),
                       static_cast<int>(roof.size()), nullptr, 0);
    line(renderer, center, 148.0F, center, 185.0F, color(230, 174, 52));
    fillRect(renderer, center, 146.0F, 38.0F, 10.0F, color(176, 34, 41));
  }

  fillRect(renderer, 0.0F, 352.0F, kWorldWidth, 125.0F, color(24, 25, 45));
  const float crowdScroll = std::fmod(cameraX * 0.34F, 23.0F);
  for (int row = 0; row < (lowDetail ? 3 : 5); ++row) {
    for (int column = -1; column < 23; ++column) {
      const float x = static_cast<float>(column * 23) - crowdScroll +
                      static_cast<float>((row % 2) * 9);
      const float y = 372.0F + static_cast<float>(row * 21);
      filledCircle(renderer, x, y, 5.0F, color(45 + row * 8, 50, 72));
    }
  }

  fillRect(renderer, 0.0F, 462.0F, kWorldWidth, 12.0F, color(112, 23, 37));

  // Stage 1 obstacle hardware: a fixed overhead pipe/track carries the
  // moving fire-ring hangers. It is part of gameplay, not decorative scenery.
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

  fillRect(renderer, 0.0F, 474.0F, kWorldWidth, 68.0F, color(190, 126, 52));
  for (int stripe = 0; stripe < kWorldWidth / 28 + 2; ++stripe) {
    const float x = static_cast<float>(stripe * 28) -
                    std::fmod(cameraX * 0.7F, 28.0F);
    line(renderer, x, 478.0F, x - 22.0F, 537.0F, color(211, 151, 72));
  }
  fillRect(renderer, 0.0F, kGroundY, kWorldWidth, 108.0F,
           color(89, 35, 30));
  fillRect(renderer, 0.0F, kGroundY, kWorldWidth, 5.0F,
           color(245, 183, 79));
}

void drawHoop(SDL_Renderer* renderer, const Hoop& hoop, float cameraX,
              double timeSeconds, bool lowDetail) {
  const float x = hoop.worldX - cameraX;
  if (x < -80.0F || x > kWorldWidth + 80.0F || hoop.cleared) return;
  const float centerY = (hoop.openingTop + hoop.openingBottom) * 0.5F;
  const float radiusY = (hoop.openingBottom - hoop.openingTop) * 0.5F;

  // Each ring travels with a hanger riding the ceiling track. This preserves
  // the recognizable Stage 1 mechanical detail instead of using floor stands.
  fillRect(renderer, x - 5.0F, kTrackY + 4.0F, 10.0F,
           hoop.openingTop - kTrackY - 4.0F, color(91, 101, 116));
  fillRect(renderer, x - 12.0F, kTrackY - 2.0F, 24.0F, 14.0F,
           color(58, 66, 81));
  fillRect(renderer, x - 8.0F, hoop.openingTop - 5.0F, 16.0F, 11.0F,
           color(119, 44, 34));
  ellipse(renderer, x, centerY, 34.0F, radiusY, color(255, 169, 42),
          lowDetail ? 3 : 5);
  ellipse(renderer, x, centerY, 29.0F, radiusY - 5.0F, color(159, 29, 27), 2);

  const int flameCount = lowDetail ? 9 : 17;
  for (int flame = 0; flame < flameCount; ++flame) {
    const float angle =
        (2.0F * kPi * static_cast<float>(flame)) /
        static_cast<float>(flameCount);
    const float wave =
        std::sin(static_cast<float>(timeSeconds * 11.0) + flame * 1.7F);
    const float flameX = x + std::cos(angle) * 36.0F;
    const float flameY = centerY + std::sin(angle) * (radiusY + 3.0F);
    const float flameRadius = lowDetail ? 3.0F : 4.0F + wave;
    filledCircle(renderer, flameX, flameY, flameRadius,
                 flame % 2 == 0 ? color(255, 211, 57)
                                : color(255, 83, 26));
  }
}

void drawLionAndRider(SDL_Renderer* renderer, float screenX, float groundY,
                      double timeSeconds, bool alive, bool lowDetail) {
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

void drawHud(SDL_Renderer* renderer, const Game& game) {
  fillRect(renderer, 0.0F, 0.0F, kWorldWidth, 31.0F, color(4, 8, 20, 235));
  drawText(renderer, "SCORE " + std::to_string(game.score), 12.0F, 10.0F,
           2.0F, color(255, 214, 75));
  drawText(renderer, "LIVES " + std::to_string(game.lives), 342.0F, 10.0F,
           2.0F, color(255, 255, 255));
}

void drawDebug(SDL_Renderer* renderer, const Game& game,
               const RenderSurface& surface) {
  fillRect(renderer, 8.0F, 42.0F, 246.0F, 77.0F, color(0, 0, 0, 200));
  drawText(renderer, "FIXED 60.606 HZ", 15.0F, 49.0F, 1.4F,
           color(93, 224, 255));
  drawText(renderer, "SPEED " + std::to_string(static_cast<int>(
                                   std::lround(game.player.runSpeed))),
           15.0F, 66.0F, 1.4F, color(255, 255, 255));
  drawText(renderer, "JUMP " + std::to_string(static_cast<int>(
                                  std::lround(kGroundY -
                                              game.player.position.y))),
           15.0F, 83.0F, 1.4F, color(255, 255, 255));
  drawText(renderer,
           "RENDER " + std::to_string(surface.width) + "X" +
               std::to_string(surface.height),
           15.0F, 100.0F, 1.4F, color(255, 255, 255));
}

void renderScene(SDL_Renderer* renderer, const Game& game,
                 const RenderSurface& surface, double timeSeconds,
                 double interpolation) {
  const bool lowDetail = surface.height <= 320;
  const float camera =
      game.previousCameraX +
      (game.cameraX - game.previousCameraX) * static_cast<float>(interpolation);
  drawBackdrop(renderer, camera, lowDetail);

  for (const auto& hoop : game.hoops) {
    drawHoop(renderer, hoop, camera, timeSeconds, lowDetail);
  }

  if (game.scene != Scene::Title) {
    const float playerWorldX =
        game.player.previous.x +
        (game.player.position.x - game.player.previous.x) *
            static_cast<float>(interpolation);
    const float playerY =
        game.player.previous.y +
        (game.player.position.y - game.player.previous.y) *
            static_cast<float>(interpolation);
    drawLionAndRider(renderer, playerWorldX - camera, playerY, timeSeconds,
                     game.player.alive, lowDetail);
    drawHud(renderer, game);
  }

  if (game.scene == Scene::Title) {
    fillRect(renderer, 33.0F, 158.0F, 414.0F, 238.0F, color(3, 9, 29, 225));
    drawText(renderer, "BIG TOP", kWorldWidth * 0.5F, 194.0F, 6.0F,
             color(255, 202, 56), true);
    drawText(renderer, "RUN", kWorldWidth * 0.5F, 253.0F, 8.0F,
             color(225, 52, 54), true);
    drawText(renderer, "NATIVE ARCADE PROTOTYPE", kWorldWidth * 0.5F,
             334.0F, 1.6F, color(160, 211, 255), true);
    drawText(renderer, "PRESS ENTER OR 1", kWorldWidth * 0.5F, 365.0F,
             2.0F, color(255, 255, 255), true);
  } else if (game.scene == Scene::Crashed) {
    fillRect(renderer, 55.0F, 220.0F, 370.0F, 134.0F, color(33, 5, 8, 232));
    drawText(renderer, "MISSED THE HOOP", kWorldWidth * 0.5F, 246.0F, 2.4F,
             color(255, 96, 64), true);
    drawText(renderer, "PRESS JUMP TO RETRY", kWorldWidth * 0.5F, 300.0F,
             1.8F, color(255, 255, 255), true);
  } else if (game.scene == Scene::Complete) {
    fillRect(renderer, 48.0F, 218.0F, 384.0F, 140.0F, color(4, 35, 24, 232));
    drawText(renderer, "EVENT COMPLETE", kWorldWidth * 0.5F, 246.0F, 2.6F,
             color(85, 236, 136), true);
    drawText(renderer, "PRESS ENTER", kWorldWidth * 0.5F, 304.0F, 2.0F,
             color(255, 255, 255), true);
  }

  if (game.debug) drawDebug(renderer, game, surface);
}

void setFullscreen(SDL_Window* window, bool enabled) {
  SDL_SetWindowFullscreen(window, enabled ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}

}  // namespace

int main(int argc, char** argv) {
  const auto parsedOptions = parseOptions(argc, argv);
  if (!parsedOptions) return argc > 1 ? 1 : 0;
  const Options options = *parsedOptions;

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO) != 0) {
    std::cerr << "SDL initialization failed: " << SDL_GetError() << '\n';
    return 1;
  }

  const Uint32 windowFlags =
      SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE |
      (options.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
  SDL_Window* window = SDL_CreateWindow(
      "Big Top Run Native", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
      options.width, options.height, windowFlags);
  if (!window) {
    std::cerr << "Window creation failed: " << SDL_GetError() << '\n';
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
    SDL_Quit();
    return 1;
  }
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

  SDL_GameController* controller = nullptr;
  for (int index = 0; index < SDL_NumJoysticks(); ++index) {
    if (SDL_IsGameController(index)) {
      controller = SDL_GameControllerOpen(index);
      if (controller) break;
    }
  }

  Game game;
  game.debug = options.debug;
  resetCourse(game);
  RenderSurface surface =
      buildRenderSurface(renderer, window, options.rotation, {});
  if (!surface.texture) {
    std::cerr << "Render target creation failed: " << SDL_GetError() << '\n';
    if (controller) SDL_GameControllerClose(controller);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  bool running = true;
  bool fullscreen = options.fullscreen;
  bool jumpQueued = false;
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

    if (jumpQueued && game.scene == Scene::Crashed) {
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
    }

    bool jumpForStep = jumpQueued;
    while (accumulator >= kFixedDt) {
      updateGame(game, keyboard, jumpForStep, controllerAxis);
      jumpForStep = false;
      jumpQueued = false;
      accumulator -= kFixedDt;
    }

    const double timeSeconds =
        static_cast<double>(currentCounter - startCounter) /
        static_cast<double>(frequency);
    const double interpolation = accumulator / kFixedDt;

    SDL_SetRenderTarget(renderer, surface.texture);
    SDL_RenderSetViewport(renderer, nullptr);
    SDL_RenderSetScale(renderer,
                       static_cast<float>(surface.width) / kWorldWidth,
                       static_cast<float>(surface.height) / kWorldHeight);
    renderScene(renderer, game, surface, timeSeconds, interpolation);

    SDL_RenderSetScale(renderer, 1.0F, 1.0F);
    SDL_SetRenderTarget(renderer, nullptr);
    setColor(renderer, color(0, 0, 0));
    SDL_RenderClear(renderer);
    SDL_RenderCopyEx(renderer, surface.texture, nullptr, &surface.destination,
                     static_cast<double>(options.rotation), nullptr,
                     SDL_FLIP_NONE);
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
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
