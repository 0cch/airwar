#include <SFML/Graphics.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr int kWindowWidth = 800;
constexpr int kWindowHeight = 600;
constexpr float kPi = 3.14159265358979323846f;
constexpr float kPlayerSpeed = 260.f;
constexpr float kPlayerHitRadius = 8.f;
constexpr float kFixedTimeStep = 1.f / 120.f;
constexpr float kMaximumFrameTime = 0.25f;
constexpr int kMaximumSimulationSteps = 30;
constexpr float kGrazeDistance = 34.f;
constexpr float kMilestoneTime = 30.f;

sf::Vector2f Normalize(const sf::Vector2f& vector) {
  const float length = std::sqrt(vector.x * vector.x + vector.y * vector.y);
  if (length <= 0.0001f) return {0.f, 0.f};
  return vector / length;
}

float DistanceSquared(const sf::Vector2f& a, const sf::Vector2f& b) {
  const float dx = a.x - b.x;
  const float dy = a.y - b.y;
  return dx * dx + dy * dy;
}

float DistanceSquaredToSegment(const sf::Vector2f& start,
                               const sf::Vector2f& end,
                               const sf::Vector2f& point) {
  const sf::Vector2f segment = end - start;
  const float length_squared =
      segment.x * segment.x + segment.y * segment.y;
  if (length_squared <= 0.0001f) return DistanceSquared(start, point);

  const sf::Vector2f offset = point - start;
  const float projection =
      std::clamp((offset.x * segment.x + offset.y * segment.y) /
                     length_squared,
                 0.f, 1.f);
  const sf::Vector2f closest = start + segment * projection;
  return DistanceSquared(closest, point);
}

float Lerp(float from, float to, float amount) {
  return from + (to - from) * std::clamp(amount, 0.f, 1.f);
}

std::wstring FormatTime(float seconds) {
  const int total_tenths =
      std::max(0, static_cast<int>(std::floor(seconds * 10.f + 0.5f)));
  const int minutes = total_tenths / 600;
  const int remaining_seconds = (total_tenths / 10) % 60;
  const int tenths = total_tenths % 10;

  std::wostringstream stream;
  stream << std::setfill(L'0') << std::setw(2) << minutes << L':'
         << std::setw(2) << remaining_seconds << L'.' << tenths;
  return stream.str();
}

struct RunEvaluation {
  std::wstring grade;
  std::wstring title;
  std::wstring style;
  sf::Color color;
  int score = 0;
};

RunEvaluation EvaluateRun(float seconds, int graze_count, int risky_graze_count,
                          int perfect_graze_count) {
  RunEvaluation result;
  result.score = static_cast<int>(std::lround(seconds * 100.f)) +
                 graze_count * 30 + risky_graze_count * 10 +
                 perfect_graze_count * 20;

  if (seconds >= 60.f) {
    result.grade = L"S";
    result.title = L"离谱";
    result.color = sf::Color(255, 118, 211);
  } else if (seconds >= 45.f) {
    result.grade = L"A";
    result.title = L"真男人";
    result.color = sf::Color(255, 196, 92);
  } else if (seconds >= 30.f) {
    result.grade = L"B";
    result.title = L"牛";
    result.color = sf::Color(255, 139, 84);
  } else if (seconds >= 20.f) {
    result.grade = L"C";
    result.title = L"不错";
    result.color = sf::Color(111, 224, 214);
  } else if (seconds >= 10.f) {
    result.grade = L"D";
    result.title = L"还行";
    result.color = sf::Color(174, 195, 220);
  } else {
    result.grade = L"E";
    result.title = L"菜";
    result.color = sf::Color(255, 103, 112);
  }

  const float graze_rate = seconds > 0.1f ? graze_count / seconds : 0.f;
  const int precision_graze_count = risky_graze_count + perfect_graze_count;
  if (perfect_graze_count >= 3 && graze_rate >= 0.25f) {
    result.style = L"刀尖舞者";
  } else if (precision_graze_count >= 3 && graze_rate >= 0.2f) {
    result.style = L"险中求胜";
  } else if (graze_count >= 3) {
    result.style = L"稳中带浪";
  } else if (seconds >= 20.f) {
    result.style = L"稳字当头";
  } else {
    result.style = L"再来一次";
  }

  return result;
}

enum class GrazeQuality { None, Nice, Close, Risky, Perfect };

std::wstring GrazeLabel(GrazeQuality quality) {
  switch (quality) {
    case GrazeQuality::Perfect:
      return L"完美闪避";
    case GrazeQuality::Risky:
      return L"惊险";
    case GrazeQuality::Close:
      return L"擦弹";
    case GrazeQuality::Nice:
      return L"漂亮";
    default:
      return L"";
  }
}

sf::Color GrazeColor(GrazeQuality quality) {
  switch (quality) {
    case GrazeQuality::Perfect:
      return sf::Color(255, 216, 112);
    case GrazeQuality::Risky:
      return sf::Color(255, 126, 103);
    case GrazeQuality::Close:
      return sf::Color(111, 224, 214);
    default:
      return sf::Color(174, 195, 220);
  }
}

unsigned int GrazeTextSize(GrazeQuality quality) {
  if (quality == GrazeQuality::Perfect) return 18;
  if (quality == GrazeQuality::Risky) return 17;
  return 14;
}

int GrazeParticleCount(GrazeQuality quality) {
  if (quality == GrazeQuality::Perfect) return 12;
  if (quality == GrazeQuality::Risky) return 9;
  if (quality == GrazeQuality::Close) return 6;
  return 4;
}

class Random {
 public:
  Random() : engine_(std::random_device{}()) {}

  float Float(float minimum, float maximum) {
    return std::uniform_real_distribution<float>(minimum, maximum)(engine_);
  }

  int Int(int minimum, int maximum) {
    return std::uniform_int_distribution<int>(minimum, maximum)(engine_);
  }

  bool Chance(float probability) {
    return Float(0.f, 1.f) < probability;
  }

 private:
  std::mt19937 engine_;
};

class StarField {
 public:
  explicit StarField(Random& random) {
    stars_.reserve(96);
    for (int index = 0; index < 96; ++index) {
      stars_.push_back(
          {{random.Float(0.f, kWindowWidth), random.Float(0.f, kWindowHeight)},
           random.Float(0.6f, 1.9f),
           random.Float(5.f, 26.f),
           static_cast<std::uint8_t>(random.Int(30, 105))});
    }
  }

  void Update(float delta_time, float speed_multiplier) {
    for (auto& star : stars_) {
      star.position.y += star.speed * speed_multiplier * delta_time;
      if (star.position.y > kWindowHeight + 4.f) {
        star.position.y = -4.f;
      }
    }
  }

  void Render(sf::RenderWindow& window) {
    for (std::size_t index = 0; index < stars_.size(); ++index) {
      const Star& star = stars_[index];
      const float size = star.radius * 2.f;
      const sf::Color color(148, 183, 220, star.alpha);
      const std::size_t vertex_index = index * 4;

      star_vertices_[vertex_index] = sf::Vertex(star.position, color);
      star_vertices_[vertex_index + 1] =
          sf::Vertex({star.position.x + size, star.position.y}, color);
      star_vertices_[vertex_index + 2] =
          sf::Vertex({star.position.x + size, star.position.y + size}, color);
      star_vertices_[vertex_index + 3] =
          sf::Vertex({star.position.x, star.position.y + size}, color);
    }
    window.draw(star_vertices_);
  }

 private:
  struct Star {
    sf::Vector2f position;
    float radius;
    float speed;
    std::uint8_t alpha;
  };

  std::vector<Star> stars_;
  sf::VertexArray star_vertices_{sf::Quads, 96 * 4};
};

class Bullet {
 public:
  Bullet(const sf::Vector2f& position, const sf::Vector2f& velocity,
         bool high_speed)
      : position_(position),
        previous_position_(position),
        velocity_(velocity),
        high_speed_(high_speed) {}

  void Update(float delta_time) {
    previous_position_ = position_;
    position_ += velocity_ * delta_time;
    age_ += delta_time;
  }

  bool IsExpired() const {
    constexpr float margin = 60.f;
    return age_ > 10.f || position_.x < -margin ||
           position_.x > kWindowWidth + margin ||
           position_.y < -margin || position_.y > kWindowHeight + margin;
  }

  bool CollidesWith(const sf::Vector2f& player_position,
                    float player_radius) const {
    const float collision_distance = kRadius + player_radius;
    return DistanceSquaredToSegment(previous_position_, position_,
                                    player_position) <
           collision_distance * collision_distance;
  }

  GrazeQuality TryGraze(const sf::Vector2f& player_position) {
    if (grazed_) return GrazeQuality::None;

    const float distance_squared = DistanceSquaredToSegment(
        previous_position_, position_, player_position);
    if (distance_squared > kGrazeDistance * kGrazeDistance) {
      return GrazeQuality::None;
    }

    grazed_ = true;
    const float distance = std::sqrt(distance_squared);
    if (distance <= 17.f) return GrazeQuality::Perfect;
    if (distance <= 23.f) return GrazeQuality::Risky;
    if (distance <= 29.f) return GrazeQuality::Close;
    return GrazeQuality::Nice;
  }

  const sf::Vector2f& Position() const { return position_; }

  void Render(sf::RenderWindow& window) const {
    const float angle = std::atan2(velocity_.y, velocity_.x) * 180.f / kPi;

    const float trail_length = high_speed_ ? 34.f : 17.f;
    const float trail_width = high_speed_ ? 3.f : 2.f;
    sf::RectangleShape trail(sf::Vector2f(trail_length, trail_width));
    trail.setOrigin(0.f, trail_width / 2.f);
    trail.setPosition(position_);
    trail.setRotation(angle + 180.f);
    trail.setFillColor(high_speed_ ? sf::Color(255, 202, 92, 145)
                                   : sf::Color(255, 91, 71, 65));
    window.draw(trail);

    sf::CircleShape glow(kRadius + 6.f);
    glow.setOrigin(kRadius + 6.f, kRadius + 6.f);
    glow.setPosition(position_);
    glow.setFillColor(high_speed_ ? sf::Color(255, 164, 48, 75)
                                  : sf::Color(255, 69, 52, 45));
    window.draw(glow);

    sf::CircleShape halo(kRadius + 2.5f);
    halo.setOrigin(kRadius + 2.5f, kRadius + 2.5f);
    halo.setPosition(position_);
    halo.setFillColor(high_speed_ ? sf::Color(255, 211, 108, 185)
                                  : sf::Color(255, 99, 74, 115));
    window.draw(halo);

    sf::CircleShape core(kRadius);
    core.setOrigin(kRadius, kRadius);
    core.setPosition(position_);
    core.setFillColor(high_speed_ ? sf::Color(255, 245, 205)
                                  : sf::Color(255, 117, 88));
    core.setOutlineThickness(1.f);
    core.setOutlineColor(high_speed_ ? sf::Color(255, 184, 76, 235)
                                     : sf::Color(255, 231, 204, 195));
    window.draw(core);
  }

 private:
  static constexpr float kRadius = 5.5f;

  sf::Vector2f position_;
  sf::Vector2f previous_position_;
  sf::Vector2f velocity_;
  float age_ = 0.f;
  bool grazed_ = false;
  bool high_speed_ = false;
};

class ParticleSystem {
 public:
  ParticleSystem() { particles_.reserve(512); }

  void SpawnTrail(const sf::Vector2f& position, Random& random) {
    AddParticle(position + sf::Vector2f(random.Float(-2.f, 2.f), 18.f),
                {random.Float(-8.f, 8.f), random.Float(35.f, 65.f)},
                random.Float(1.5f, 2.8f), random.Float(0.22f, 0.38f),
                sf::Color(102, 220, 235, 130));
  }

  void SpawnImpact(const sf::Vector2f& position, Random& random) {
    for (int index = 0; index < 32; ++index) {
      const float angle = random.Float(0.f, kPi * 2.f);
      const float speed = random.Float(45.f, 235.f);
      AddParticle(position,
                  {std::cos(angle) * speed, std::sin(angle) * speed},
                  random.Float(1.4f, 4.2f), random.Float(0.3f, 0.8f),
                  index % 3 == 0 ? sf::Color(255, 220, 116, 235)
                                 : sf::Color(255, 91, 67, 225));
    }
  }

  void SpawnGraze(const sf::Vector2f& position, Random& random,
                  int particle_count) {
    for (int index = 0; index < particle_count; ++index) {
      const float angle = random.Float(0.f, kPi * 2.f);
      const float speed = random.Float(25.f, 95.f);
      AddParticle(position,
                  {std::cos(angle) * speed, std::sin(angle) * speed},
                  random.Float(0.8f, 1.8f), random.Float(0.12f, 0.24f),
                  sf::Color(255, 222, 132, 225));
    }
  }

  void Clear() { particles_.clear(); }

  void Update(float delta_time) {
    for (auto& particle : particles_) {
      particle.life -= delta_time;
      particle.position += particle.velocity * delta_time;
      particle.velocity *= std::pow(0.08f, delta_time);
    }
    particles_.erase(
        std::remove_if(particles_.begin(), particles_.end(),
                       [](const Particle& particle) {
                         return particle.life <= 0.f;
                       }),
        particles_.end());
  }

  void Render(sf::RenderWindow& window) const {
    for (const auto& particle : particles_) {
      const float alpha = std::clamp(particle.life / particle.max_life, 0.f, 1.f);
      sf::Color color = particle.color;
      color.a = static_cast<std::uint8_t>(static_cast<float>(color.a) * alpha);

      sf::CircleShape shape(particle.radius * (0.45f + alpha * 0.55f));
      shape.setOrigin(shape.getRadius(), shape.getRadius());
      shape.setPosition(particle.position);
      shape.setFillColor(color);
      window.draw(shape);
    }
  }

 private:
  struct Particle {
    sf::Vector2f position;
    sf::Vector2f velocity;
    float radius;
    float life;
    float max_life;
    sf::Color color;
  };

  void AddParticle(const sf::Vector2f& position,
                   const sf::Vector2f& velocity, float radius, float life,
                   const sf::Color& color) {
    particles_.push_back({position, velocity, radius, life, life, color});
  }

  std::vector<Particle> particles_;
};

class Game {
 public:
  explicit Game(const std::filesystem::path& executable_path)
      : window_(sf::VideoMode(kWindowWidth, kWindowHeight),
                sf::String::fromUtf8("Air War",
                                     "Air War"),
                sf::Style::Titlebar | sf::Style::Close),
        executable_directory_(executable_path.parent_path()),
        best_time_path_(executable_directory_ / "best_time.txt"),
        stars_(random_) {
    window_.setFramerateLimit(60);
    bullets_.reserve(88);
    graze_feedbacks_.reserve(24);
    LoadResources();
    LoadBestTime();
    ResetRun();
    state_ = GameState::Ready;
  }

  int Run() {
    if (!resources_ready_) return 1;

    sf::Clock frame_clock;
    float simulation_accumulator = 0.f;
    while (window_.isOpen()) {
      const float delta_time =
          std::min(frame_clock.restart().asSeconds(), kMaximumFrameTime);
      ProcessEvents();
      UpdateAmbient(delta_time);

      if (state_ == GameState::Playing) {
        simulation_accumulator += delta_time;
        int simulation_steps = 0;
        while (simulation_accumulator >= kFixedTimeStep &&
               simulation_steps < kMaximumSimulationSteps) {
          UpdatePlaying(kFixedTimeStep);
          simulation_accumulator -= kFixedTimeStep;
          ++simulation_steps;
          if (state_ != GameState::Playing) {
            simulation_accumulator = 0.f;
            break;
          }
        }
        if (simulation_steps == kMaximumSimulationSteps &&
            simulation_accumulator >= kFixedTimeStep) {
          simulation_accumulator = 0.f;
        }
      } else {
        simulation_accumulator = 0.f;
      }

      Render();
    }
    return 0;
  }

 private:
  enum class GameState { Ready, Playing, Paused, GameOver };

  struct GrazeFeedback {
    std::wstring label;
    sf::Color color;
    sf::Vector2f position;
    float life = 0.f;
    float max_life = 0.f;
    unsigned int character_size = 14;
  };

  sf::RenderWindow window_;
  sf::Texture player_texture_;
  sf::Sprite player_;
  sf::Font font_;
  sf::Clock ui_clock_;
  sf::Clock trail_clock_;
  std::vector<Bullet> bullets_;
  Random random_;
  StarField stars_;
  ParticleSystem particles_;
  std::filesystem::path executable_directory_;
  std::filesystem::path best_time_path_;
  GameState state_ = GameState::Ready;
  GameState state_before_pause_ = GameState::Playing;
  bool resources_ready_ = false;
  bool new_best_ = false;
  float elapsed_time_ = 0.f;
  float last_time_ = 0.f;
  float best_time_ = 0.f;
  float spawn_timer_ = 0.f;
  float game_over_timer_ = 0.f;
  float flash_timer_ = 0.f;
  float milestone_timer_ = 0.f;
  bool milestone_shown_ = false;
  float bank_angle_ = 0.f;
  int graze_count_ = 0;
  int risky_graze_count_ = 0;
  int perfect_graze_count_ = 0;
  std::vector<GrazeFeedback> graze_feedbacks_;

  std::filesystem::path FindResource(const std::string& name) const {
    const std::array<std::filesystem::path, 2> candidates = {
        executable_directory_ / name, std::filesystem::current_path() / name};
    for (const auto& candidate : candidates) {
      std::error_code error;
      if (std::filesystem::exists(candidate, error)) return candidate;
    }
    return candidates.front();
  }

  void LoadResources() {
    const auto texture_path = FindResource("plane.png");
    const auto font_path = FindResource("font.ttf");
    if (!player_texture_.loadFromFile(texture_path.string())) {
      std::cerr << "Unable to load " << texture_path << '\n';
      window_.close();
      return;
    }
    if (!font_.loadFromFile(font_path.string())) {
      std::cerr << "Unable to load " << font_path << '\n';
      window_.close();
      return;
    }

    player_texture_.setSmooth(true);
    player_.setTexture(player_texture_);
    const sf::Vector2u texture_size = player_texture_.getSize();
    const float scale =
        70.f / static_cast<float>(std::max(texture_size.x, texture_size.y));
    player_.setScale(scale, scale);
    const sf::FloatRect bounds = player_.getLocalBounds();
    player_.setOrigin(bounds.width / 2.f, bounds.height / 2.f);
    resources_ready_ = true;
  }

  void LoadBestTime() {
    std::ifstream input(best_time_path_);
    float value = 0.f;
    if (input >> value && std::isfinite(value) && value >= 0.f) {
      best_time_ = value;
    }
  }

  void SaveBestTime() const {
    std::ofstream output(best_time_path_, std::ios::trunc);
    output << std::fixed << std::setprecision(1) << best_time_;
  }

  void ResetRun() {
    bullets_.clear();
    particles_.Clear();
    graze_feedbacks_.clear();
    elapsed_time_ = 0.f;
    last_time_ = 0.f;
    spawn_timer_ = 0.25f;
    game_over_timer_ = 0.f;
    flash_timer_ = 0.f;
    milestone_timer_ = 0.f;
    milestone_shown_ = false;
    bank_angle_ = 0.f;
    graze_count_ = 0;
    risky_graze_count_ = 0;
    perfect_graze_count_ = 0;
    new_best_ = false;
    player_.setPosition(kWindowWidth / 2.f, kWindowHeight / 2.f);
    player_.setRotation(0.f);
    trail_clock_.restart();
  }

  void StartRun() {
    ResetRun();
    state_ = GameState::Playing;
  }

  void ProcessEvents() {
    sf::Event event;
    while (window_.pollEvent(event)) {
      if (event.type == sf::Event::Closed) {
        window_.close();
      } else if (event.type == sf::Event::LostFocus &&
                 state_ == GameState::Playing) {
        Pause();
      } else if (event.type == sf::Event::KeyReleased) {
        HandleKey(event.key.code);
      } else if (event.type == sf::Event::MouseButtonReleased &&
                 event.mouseButton.button == sf::Mouse::Left) {
        HandleConfirm();
      }
    }
  }

  void HandleKey(sf::Keyboard::Key key) {
    if (key == sf::Keyboard::Escape) {
      if (state_ == GameState::Playing) {
        Pause();
      } else if (state_ == GameState::Paused) {
        Resume();
      } else {
        window_.close();
      }
      return;
    }

    if (key == sf::Keyboard::Enter || key == sf::Keyboard::Space) {
      HandleConfirm();
    } else if (key == sf::Keyboard::R && state_ == GameState::GameOver) {
      StartRun();
    }
  }

  void HandleConfirm() {
    if (state_ == GameState::Ready) {
      StartRun();
    } else if (state_ == GameState::Paused) {
      Resume();
    } else if (state_ == GameState::GameOver &&
               game_over_timer_ >= 0.25f) {
      StartRun();
    }
  }

  void Pause() {
    state_before_pause_ = state_;
    state_ = GameState::Paused;
  }

  void Resume() {
    state_ = state_before_pause_;
  }

  void UpdateAmbient(float delta_time) {
    const float background_speed =
        state_ == GameState::Playing ? 1.f : 0.3f;
    stars_.Update(delta_time, background_speed);
    particles_.Update(delta_time);
    flash_timer_ = std::max(0.f, flash_timer_ - delta_time);
    milestone_timer_ = std::max(0.f, milestone_timer_ - delta_time);

    for (auto& feedback : graze_feedbacks_) {
      feedback.life -= delta_time;
      feedback.position.y -= 22.f * delta_time;
    }
    graze_feedbacks_.erase(
        std::remove_if(graze_feedbacks_.begin(), graze_feedbacks_.end(),
                       [](const GrazeFeedback& feedback) {
                         return feedback.life <= 0.f;
                       }),
        graze_feedbacks_.end());

    if (state_ == GameState::GameOver) {
      game_over_timer_ += delta_time;
    }
  }

  void UpdatePlaying(float delta_time) {
    elapsed_time_ += delta_time;
    UpdatePlayer(delta_time);
    UpdateMilestone();
    UpdateBullets(delta_time);
    CheckCollision();
  }

  void UpdatePlayer(float delta_time) {
    sf::Vector2f direction;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Left) ||
        sf::Keyboard::isKeyPressed(sf::Keyboard::A)) {
      direction.x -= 1.f;
    }
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Right) ||
        sf::Keyboard::isKeyPressed(sf::Keyboard::D)) {
      direction.x += 1.f;
    }
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Up) ||
        sf::Keyboard::isKeyPressed(sf::Keyboard::W)) {
      direction.y -= 1.f;
    }
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Down) ||
        sf::Keyboard::isKeyPressed(sf::Keyboard::S)) {
      direction.y += 1.f;
    }

    direction = Normalize(direction);
    player_.move(direction * kPlayerSpeed * delta_time);

    const sf::FloatRect bounds = player_.getGlobalBounds();
    sf::Vector2f position = player_.getPosition();
    position.x = std::clamp(position.x, bounds.width / 2.f + 10.f,
                            kWindowWidth - bounds.width / 2.f - 10.f);
    position.y = std::clamp(position.y, bounds.height / 2.f + 10.f,
                            kWindowHeight - bounds.height / 2.f - 10.f);
    player_.setPosition(position);

    const float target_bank = direction.x * 7.f;
    bank_angle_ = Lerp(bank_angle_, target_bank, delta_time * 12.f);
    player_.setRotation(bank_angle_);

    if (trail_clock_.getElapsedTime().asSeconds() > 0.035f) {
      trail_clock_.restart();
      particles_.SpawnTrail(player_.getPosition(), random_);
    }
  }

  void UpdateMilestone() {
    if (!milestone_shown_ && elapsed_time_ >= kMilestoneTime) {
      milestone_shown_ = true;
      milestone_timer_ = 1.8f;
      flash_timer_ = 0.16f;
    }
  }

  void UpdateBullets(float delta_time) {
    spawn_timer_ += delta_time;
    const float interval = SpawnInterval();
    const int maximum_bullets = MaximumBullets();

    if (spawn_timer_ >= interval &&
        static_cast<int>(bullets_.size()) < maximum_bullets) {
      spawn_timer_ = 0.f;
      SpawnBullet();
    }

    for (auto& bullet : bullets_) {
      bullet.Update(delta_time);
    }
    bullets_.erase(
        std::remove_if(bullets_.begin(), bullets_.end(),
                       [](const Bullet& bullet) {
                         return bullet.IsExpired();
                       }),
        bullets_.end());
  }

  float SpawnInterval() const {
    if (elapsed_time_ < 10.f) {
      return Lerp(0.30f, 0.19f, elapsed_time_ / 10.f);
    }
    if (elapsed_time_ < 20.f) {
      return Lerp(0.19f, 0.13f, (elapsed_time_ - 10.f) / 10.f);
    }
    return std::max(0.085f, 0.13f - (elapsed_time_ - 20.f) * 0.001f);
  }

  int MaximumBullets() const {
    if (elapsed_time_ < 10.f) return 29;
    if (elapsed_time_ < 20.f) return 44;
    return std::min(
        80, 44 + static_cast<int>((elapsed_time_ - 20.f) * 0.6f));
  }

  float BulletSpeed() {
    if (elapsed_time_ < 10.f) return random_.Float(140.f, 185.f);
    if (elapsed_time_ < 30.f) return random_.Float(165.f, 220.f);
    return random_.Float(190.f, 250.f);
  }

  float HighSpeedChance() const {
    if (elapsed_time_ < 6.f) return 0.f;
    if (elapsed_time_ < 20.f) return 0.12f;
    if (elapsed_time_ < 40.f) return 0.18f;
    return 0.24f;
  }

  void SpawnBullet() {
    const sf::Vector2f player_position = player_.getPosition();
    const bool high_speed = random_.Chance(HighSpeedChance());
    const float minimum_spawn_distance = high_speed ? 330.f : 275.f;
    const float minimum_spawn_distance_squared =
        minimum_spawn_distance * minimum_spawn_distance;
    sf::Vector2f spawn_position{};
    sf::Vector2f best_spawn_position{};
    float best_spawn_distance_squared = -1.f;

    for (int attempt = 0; attempt < 32; ++attempt) {
      const int edge = random_.Int(0, 3);
      if (edge == 0) {
        spawn_position = {random_.Float(30.f, kWindowWidth - 30.f), -18.f};
      } else if (edge == 1) {
        spawn_position = {kWindowWidth + 18.f,
                          random_.Float(30.f, kWindowHeight - 30.f)};
      } else if (edge == 2) {
        spawn_position = {random_.Float(30.f, kWindowWidth - 30.f),
                          kWindowHeight + 18.f};
      } else {
        spawn_position = {-18.f, random_.Float(30.f, kWindowHeight - 30.f)};
      }

      const float distance_squared =
          DistanceSquared(spawn_position, player_position);
      if (distance_squared > best_spawn_distance_squared) {
        best_spawn_distance_squared = distance_squared;
        best_spawn_position = spawn_position;
      }
      if (distance_squared >= minimum_spawn_distance_squared) {
        best_spawn_position = spawn_position;
        break;
      }
    }
    spawn_position = best_spawn_position;

    sf::Vector2f target =
        {random_.Float(80.f, kWindowWidth - 80.f),
         random_.Float(70.f, kWindowHeight - 70.f)};
    const bool aimed =
        elapsed_time_ >= 6.f &&
        random_.Chance(std::min(0.48f, elapsed_time_ / 70.f));
    if (aimed) {
      const float aim_jitter = high_speed ? 90.f : 65.f;
      target = player_position +
               sf::Vector2f(random_.Float(-aim_jitter, aim_jitter),
                            random_.Float(-aim_jitter, aim_jitter));
    }

    const sf::Vector2f direction = Normalize(target - spawn_position);
    const float speed =
        high_speed ? random_.Float(300.f, 380.f) : BulletSpeed();
    bullets_.emplace_back(spawn_position, direction * speed, high_speed);
  }

  void CheckCollision() {
    const sf::Vector2f player_position = player_.getPosition();
    for (auto& bullet : bullets_) {
      if (bullet.CollidesWith(player_position, kPlayerHitRadius)) {
        FinishRun();
        return;
      }
      const GrazeQuality quality = bullet.TryGraze(player_position);
      if (quality != GrazeQuality::None) {
        ++graze_count_;
        if (quality == GrazeQuality::Risky) ++risky_graze_count_;
        if (quality == GrazeQuality::Perfect) ++perfect_graze_count_;
        particles_.SpawnGraze(bullet.Position(), random_,
                              GrazeParticleCount(quality));
        AddGrazeFeedback(quality);
      }
    }
  }

  void AddGrazeFeedback(GrazeQuality quality) {
    if (quality == GrazeQuality::None) return;

    GrazeFeedback feedback;
    feedback.label = GrazeLabel(quality);
    feedback.color = GrazeColor(quality);
    feedback.character_size = GrazeTextSize(quality);
    feedback.life = quality == GrazeQuality::Perfect ? 0.9f : 0.72f;
    feedback.max_life = feedback.life;

    const int feedback_index =
        static_cast<int>(graze_feedbacks_.size() % 4);
    const float horizontal_direction = feedback_index % 2 == 0 ? -1.f : 1.f;
    const float horizontal_offset =
        horizontal_direction * random_.Float(22.f, 48.f);
    const float vertical_offset = -38.f - feedback_index * 20.f;
    feedback.position =
        player_.getPosition() + sf::Vector2f(horizontal_offset, vertical_offset);
    feedback.position.x =
        std::clamp(feedback.position.x, 48.f, kWindowWidth - 48.f);
    feedback.position.y =
        std::clamp(feedback.position.y, 26.f, kWindowHeight - 42.f);
    graze_feedbacks_.push_back(feedback);
  }

  void FinishRun() {
    last_time_ = elapsed_time_;
    new_best_ = last_time_ > best_time_;
    if (new_best_) {
      best_time_ = last_time_;
      SaveBestTime();
    }
    particles_.SpawnImpact(player_.getPosition(), random_);
    flash_timer_ = 0.22f;
    game_over_timer_ = 0.f;
    state_ = GameState::GameOver;
  }

  void Render() {
    window_.clear(sf::Color(7, 12, 22));
    RenderBackground();
    RenderWorld();
    RenderHud();
    RenderOverlay();
    RenderFlash();
    window_.display();
  }

  void RenderBackground() {
    sf::RectangleShape glow(sf::Vector2f(kWindowWidth, kWindowHeight));
    glow.setFillColor(sf::Color(10, 19, 34));
    window_.draw(glow);

    sf::CircleShape horizon(260.f);
    horizon.setOrigin(260.f, 260.f);
    horizon.setPosition(kWindowWidth * 0.5f, kWindowHeight * 0.54f);
    horizon.setFillColor(sf::Color(15, 31, 51, 115));
    window_.draw(horizon);

    stars_.Render(window_);

    sf::RectangleShape line(sf::Vector2f(kWindowWidth - 40.f, 1.f));
    line.setPosition(20.f, kWindowHeight - 24.f);
    line.setFillColor(sf::Color(91, 145, 173, 35));
    window_.draw(line);
  }

  void RenderWorld() {
    for (const auto& bullet : bullets_) {
      bullet.Render(window_);
    }
    particles_.Render(window_);
    RenderPlayer();
    RenderGrazeFeedbacks();
  }

  void RenderGrazeFeedbacks() {
    for (const auto& feedback : graze_feedbacks_) {
      const float alpha =
          std::clamp(feedback.life / feedback.max_life, 0.f, 1.f);
      sf::Color color = feedback.color;
      color.a = static_cast<std::uint8_t>(static_cast<float>(color.a) * alpha);

      sf::Text text;
      text.setFont(font_);
      text.setCharacterSize(feedback.character_size);
      text.setFillColor(color);
      text.setOutlineThickness(1.f);
      text.setOutlineColor(
          sf::Color(5, 10, 18, static_cast<std::uint8_t>(alpha * 190.f)));
      text.setString(feedback.label);
      const sf::FloatRect bounds = text.getLocalBounds();
      text.setOrigin(bounds.left + bounds.width / 2.f,
                     bounds.top + bounds.height / 2.f);
      text.setPosition(feedback.position);
      window_.draw(text);
    }
  }

  void RenderPlayer() {
    const sf::Vector2f position = player_.getPosition();

    sf::CircleShape engine_glow(23.f);
    engine_glow.setOrigin(23.f, 23.f);
    engine_glow.setPosition(position + sf::Vector2f(0.f, 8.f));
    engine_glow.setFillColor(sf::Color(68, 194, 218, 24));
    window_.draw(engine_glow);

    window_.draw(player_);

    sf::CircleShape hit_core(3.4f);
    hit_core.setOrigin(3.4f, 3.4f);
    hit_core.setPosition(position);
    hit_core.setFillColor(sf::Color(239, 250, 255));
    hit_core.setOutlineThickness(1.4f);
    hit_core.setOutlineColor(sf::Color(93, 219, 224, 220));
    window_.draw(hit_core);
  }

  void RenderHud() {
    if (state_ == GameState::Ready) return;

    sf::Text label;
    label.setFont(font_);
    label.setCharacterSize(14);
    label.setFillColor(sf::Color(137, 169, 194));
    label.setString(L"存活时间");
    label.setPosition(28.f, 22.f);
    window_.draw(label);

    sf::Text time;
    time.setFont(font_);
    time.setCharacterSize(42);
    time.setFillColor(sf::Color(235, 247, 255));
    time.setString(FormatTime(elapsed_time_));
    time.setPosition(26.f, 39.f);
    window_.draw(time);

    sf::Text best;
    best.setFont(font_);
    best.setCharacterSize(18);
    best.setFillColor(sf::Color(147, 178, 201));
    best.setString(L"BEST  " + FormatTime(best_time_));
    const sf::FloatRect best_bounds = best.getLocalBounds();
    best.setPosition(kWindowWidth - best_bounds.width - 28.f, 25.f);
    window_.draw(best);

    sf::Text graze;
    graze.setFont(font_);
    graze.setCharacterSize(15);
    graze.setFillColor(sf::Color(208, 184, 116));
    graze.setString(L"擦弹  " + std::to_wstring(graze_count_));
    const sf::FloatRect graze_bounds = graze.getLocalBounds();
    graze.setPosition(kWindowWidth - graze_bounds.width - 28.f, 53.f);
    window_.draw(graze);

    if (milestone_timer_ > 0.f) {
      const float fade_in = (1.8f - milestone_timer_) / 0.35f;
      const float fade_out = milestone_timer_ / 0.35f;
      const float alpha = std::clamp(std::min(fade_in, fade_out), 0.f, 1.f);
      sf::Text milestone;
      milestone.setFont(font_);
      milestone.setCharacterSize(34);
      milestone.setFillColor(
          sf::Color(255, 196, 92, static_cast<std::uint8_t>(alpha * 255.f)));
      milestone.setString(L"30 秒达成");
      const sf::FloatRect bounds = milestone.getLocalBounds();
      milestone.setPosition((kWindowWidth - bounds.width) / 2.f, 112.f);
      window_.draw(milestone);
    }
  }

  void RenderOverlay() {
    if (state_ == GameState::Playing) return;

    sf::RectangleShape scrim(sf::Vector2f(kWindowWidth, kWindowHeight));
    scrim.setFillColor(sf::Color(4, 8, 16, 188));
    window_.draw(scrim);

    if (state_ == GameState::Ready) {
      RenderReadyOverlay();
    } else if (state_ == GameState::Paused) {
      RenderPausedOverlay();
    } else {
      RenderGameOverOverlay();
    }
  }

  void RenderReadyOverlay() {
    DrawCenteredText(L"是男人就撑过 30 秒", 48,
                     sf::Color(240, 248, 255), 98.f);
    DrawCenteredText(L"坚持得越久，评价越高", 21,
                     sf::Color(143, 185, 208), 166.f);

    sf::RectangleShape accent(sf::Vector2f(52.f, 2.f));
    accent.setPosition((kWindowWidth - 52.f) / 2.f, 206.f);
    accent.setFillColor(sf::Color(255, 163, 92));
    window_.draw(accent);

    DrawCenteredText(L"移动  WASD / 方向键", 20,
                     sf::Color(191, 214, 229), 240.f);
    DrawCenteredText(L"碰到子弹即结束", 18,
                     sf::Color(129, 160, 181), 275.f);
    DrawCenteredText(L"贴身擦弹可触发即时评级 · 亮黄色子弹极快", 15,
                     sf::Color(180, 160, 105), 307.f);

    const float pulse =
        0.65f + 0.35f * std::sin(ui_clock_.getElapsedTime().asSeconds() * 4.f);
    DrawCenteredText(
        L"按 ENTER / SPACE 开始", 22,
        sf::Color(255, 196, 108, static_cast<std::uint8_t>(pulse * 255.f)),
        355.f);

    DrawCenteredText(L"BEST  " + FormatTime(best_time_), 17,
                     sf::Color(110, 145, 169), 438.f);
  }

  void RenderPausedOverlay() {
    DrawCenteredText(L"已暂停", 56, sf::Color(235, 247, 255), 180.f);
    DrawCenteredText(L"按 ENTER / SPACE 继续", 20,
                     sf::Color(156, 191, 213), 276.f);
    DrawCenteredText(L"ESC 返回游戏", 16, sf::Color(102, 137, 159), 318.f);
  }

  void RenderGameOverOverlay() {
    const RunEvaluation evaluation =
        EvaluateRun(last_time_, graze_count_, risky_graze_count_,
                    perfect_graze_count_);

    DrawCenteredText(L"G R A D E", 13, sf::Color(112, 143, 164), 22.f);
    DrawCenteredText(evaluation.grade, 96, evaluation.color, 41.f);
    DrawCenteredText(evaluation.title, 34, sf::Color(236, 246, 253), 158.f);
    DrawCenteredText(evaluation.style, 20, evaluation.color, 204.f);

    sf::RectangleShape accent(sf::Vector2f(52.f, 2.f));
    accent.setPosition((kWindowWidth - 52.f) / 2.f, 236.f);
    accent.setFillColor(sf::Color(evaluation.color.r, evaluation.color.g,
                                  evaluation.color.b, 150));
    window_.draw(accent);

    DrawCenteredText(L"存活  " + FormatTime(last_time_) + L"     擦弹  " +
                         std::to_wstring(graze_count_) + L"     完美  " +
                         std::to_wstring(perfect_graze_count_),
                     19, sf::Color(206, 222, 233), 253.f);
    DrawCenteredText(L"综合评分  " + std::to_wstring(evaluation.score), 20,
                     sf::Color(255, 209, 126), 289.f);

    if (new_best_) {
      DrawCenteredText(L"新纪录", 18, sf::Color(255, 200, 100), 329.f);
    } else {
      DrawCenteredText(L"BEST  " + FormatTime(best_time_), 17,
                       sf::Color(132, 166, 188), 329.f);
    }

    if (game_over_timer_ >= 0.25f) {
      DrawCenteredText(L"按 ENTER / SPACE 再来一局", 19,
                       sf::Color(162, 199, 219), 388.f);
    }
    DrawCenteredText(L"ESC 退出", 15, sf::Color(91, 123, 144), 472.f);
  }

  void DrawCenteredText(const sf::String& content,
                        unsigned int character_size, const sf::Color& color,
                        float y) {
    sf::Text text;
    text.setFont(font_);
    text.setCharacterSize(character_size);
    text.setFillColor(color);
    text.setString(content);
    const sf::FloatRect bounds = text.getLocalBounds();
    text.setPosition((kWindowWidth - bounds.width) / 2.f - bounds.left, y);
    window_.draw(text);
  }

  void RenderFlash() {
    if (flash_timer_ <= 0.f) return;

    const float duration = state_ == GameState::GameOver ? 0.22f : 0.16f;
    const float strength = std::clamp(flash_timer_ / duration, 0.f, 1.f);
    sf::RectangleShape flash(sf::Vector2f(kWindowWidth, kWindowHeight));
    flash.setFillColor(sf::Color(
        state_ == GameState::GameOver ? 255 : 105,
        state_ == GameState::GameOver ? 75 : 210,
        state_ == GameState::GameOver ? 68 : 226,
        static_cast<std::uint8_t>(strength * 48.f)));
    window_.draw(flash);
  }
};

std::filesystem::path ExecutablePath(const char* argument) {
  std::error_code error;
  const auto path = std::filesystem::absolute(argument, error);
  return error ? std::filesystem::current_path() / "airwar.exe" : path;
}

}  // namespace

int main(int argc, char** argv) {
  const std::filesystem::path executable_path =
      argc > 0 ? ExecutablePath(argv[0]) : ExecutablePath("airwar.exe");
  Game game(executable_path);
  return game.Run();
}
