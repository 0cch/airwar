#include <SFML/Graphics.hpp>
#include <cstdlib>
#include <ctime>
#include <sstream>
#include <vector>

const int kWindowWidth = 800;
const int kWindowHeight = 600;
const float kPlayerSpeed = 200.f;
const sf::Time kBulletSpawnTime = sf::milliseconds(100.f);
const int kMaxBullets = 80;

class Bullet {
 public:
  sf::CircleShape shape;
  sf::Vector2f velocity;

  Bullet(float radius = 5.f) {
    shape.setRadius(radius);
    shape.setFillColor(sf::Color::Red);
    SetRandomPositionAndVelocity();
  }

  void SetRandomPositionAndVelocity() {
    float angle = static_cast<float>(rand() % 360);
    float speed = static_cast<float>(100 + rand() % 150);
    velocity = sf::Vector2f(speed * std::cos(angle * 3.14159265f / 180.f),
                            speed * std::sin(angle * 3.14159265f / 180.f));

    if (rand() % 2 == 0) {
      shape.setPosition(rand() % kWindowWidth,
                        rand() % 2 == 0 ? 0 : kWindowHeight);
    } else {
      shape.setPosition(rand() % 2 == 0 ? 0 : kWindowWidth,
                        rand() % kWindowHeight);
    }
  }

  bool IsOffScreen() const {
    const sf::Vector2f& pos = shape.getPosition();
    return pos.x < 0 || pos.x > kWindowWidth || pos.y < 0 ||
           pos.y > kWindowHeight;
  }
};

class Game {
 private:
  sf::RenderWindow window_;
  sf::Sprite player_;
  sf::Texture playerTexture_;
  std::vector<Bullet> bullets_;
  sf::Clock game_clock_, clock_;
  sf::Font font_;
  sf::Text text_;
  bool is_game_over_;
  sf::Time time_since_last_bullet_;
  float game_time_;

  void ResetGame() {
    player_.setPosition(kWindowWidth / 2.f, kWindowHeight / 2.f);
    bullets_.clear();
    game_clock_.restart();
    is_game_over_ = false;
    time_since_last_bullet_ = sf::Time::Zero;
    game_time_ = 0.f;
  }

 public:
  Game()
      : window_(sf::VideoMode(kWindowWidth, kWindowHeight), "Air War"),
        is_game_over_(false),
        time_since_last_bullet_(sf::Time::Zero),
        game_time_(0.f) {
    window_.setFramerateLimit(60);

    if (!playerTexture_.loadFromFile("plane.png")) {
    }

    playerTexture_.setSmooth(true);

    player_.setTexture(playerTexture_);
    player_.setOrigin(player_.getLocalBounds().width / 2,
                      player_.getLocalBounds().height / 2);
    player_.setScale(0.1f, 0.1f);
    // player_.setColor(sf::Color(0, 255, 0));

    player_.setOrigin(player_.getLocalBounds().width / 2,
                      player_.getLocalBounds().height / 2);
    player_.setPosition(kWindowWidth / 2.f, kWindowHeight / 2.f);

    bullets_.reserve(kMaxBullets);

    if (!font_.loadFromFile("font.ttf")) {
    }
    text_.setFont(font_);
    text_.setCharacterSize(24);
    text_.setFillColor(sf::Color::White);
  }

  void Run() {
    while (window_.isOpen()) {
      sf::Time delta_time = clock_.restart();
      sf::Event event;
      while (window_.pollEvent(event)) {
        if (event.type == sf::Event::Closed) window_.close();
      }

      if (!is_game_over_) {
        HandlePlayerMovement(delta_time);
        HandleBullets(delta_time);
        CheckCollisions();
        UpdateGameTime();
      } else {
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Enter) ||
            sf::Mouse::isButtonPressed(sf::Mouse::Left)) {
          ResetGame();
        }
      }

      Render();
    }
  }

  void HandlePlayerMovement(sf::Time delta_time) {
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Left)) {
      player_.move(-kPlayerSpeed * delta_time.asSeconds(), 0.f);
    }
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Right)) {
      player_.move(kPlayerSpeed * delta_time.asSeconds(), 0.f);
    }
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Up)) {
      player_.move(0.f, -kPlayerSpeed * delta_time.asSeconds());
    }
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Down)) {
      player_.move(0.f, kPlayerSpeed * delta_time.asSeconds());
    }
  }

  void HandleBullets(sf::Time delta_time) {
    time_since_last_bullet_ += delta_time;
    if (time_since_last_bullet_ > kBulletSpawnTime &&
        bullets_.size() < kMaxBullets) {
      bullets_.emplace_back();
      time_since_last_bullet_ = sf::Time::Zero;
    }

    for (auto& bullet : bullets_) {
      bullet.shape.move(bullet.velocity * delta_time.asSeconds());
      if (bullet.IsOffScreen()) {
        bullet.SetRandomPositionAndVelocity();
      }
    }
  }

  std::vector<sf::FloatRect> CreateCollisionBoxes(const sf::Sprite& sprite,
                                                  float scale) {
    std::vector<sf::FloatRect> boxes;
    const sf::FloatRect bounds = sprite.getGlobalBounds();
    boxes.push_back(
        sf::FloatRect(sprite.getPosition().x - bounds.width / 2 + 228 * scale,
                      sprite.getPosition().y - bounds.height / 2 + 10 * scale,
                      95 * scale, 305 * scale));
    boxes.push_back(
        sf::FloatRect(sprite.getPosition().x - bounds.width / 2 + 20 * scale,
                      sprite.getPosition().y - bounds.height / 2 + 165 * scale,
                      470 * scale, 50 * scale));

    return boxes;
  }

  void CheckCollisions() {
    auto player_boxes = CreateCollisionBoxes(player_, 0.1f);

    for (const auto& bullet : bullets_) {
      sf::FloatRect bulletBounds = bullet.shape.getGlobalBounds();

      for (const auto& box : player_boxes) {
        if (bulletBounds.intersects(box)) {
          is_game_over_ = true;
          return;
        }
      }
    }
  }

  void UpdateGameTime() {
    std::ostringstream ss;
    game_time_ = game_clock_.getElapsedTime().asSeconds();
    ss << "Time: " << game_time_ << "s";
    text_.setString(ss.str());
    text_.setPosition(10, 10);
  }

  void Render() {
    window_.clear();
    if (!is_game_over_) {
      window_.draw(player_);
      for (const auto& bullet : bullets_) {
        window_.draw(bullet.shape);
      }
    } else {
      sf::Text end_text;
      end_text.setString(game_time_ < 30.f ? L"菜" : L"牛");
      end_text.setPosition(250, 100);
      end_text.setFont(font_);
      end_text.setFillColor(sf::Color::Red);
      end_text.setCharacterSize(300);
      window_.draw(end_text);
    }
    window_.draw(text_);
    window_.display();
  }
};

int main() {
  srand(static_cast<unsigned int>(time(NULL)));
  Game game;
  game.Run();
  return 0;
}
