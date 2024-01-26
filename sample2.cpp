#include <SFML/Graphics.hpp>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <vector>

const int kWindowWidth = 800;
const int kWindowHeight = 600;
const float kPlayerSpeed = 200.f;
const sf::Time kBulletSpawnTime = sf::seconds(1.f);
const int MAX_BULLETS = 20;

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
      // Start from horizontal edges
      shape.setPosition(rand() % kWindowWidth,
                        rand() % 2 == 0 ? 0 : kWindowHeight);
    } else {
      // Start from vertical edges
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

int main() {
  srand(static_cast<unsigned int>(time(NULL)));
  sf::RenderWindow window(sf::VideoMode(kWindowWidth, kWindowHeight),
                          "Plane Dodge Game");
  window.setFramerateLimit(60);

  sf::RectangleShape player(sf::Vector2f(50.f, 50.f));
  player.setFillColor(sf::Color::Green);
  player.setPosition(kWindowWidth / 2.f, kWindowHeight / 2.f);

  std::vector<Bullet> bullets;
  sf::Clock clock;
  sf::Time time_since_last_bullet = sf::Time::Zero;

  while (window.isOpen()) {
    sf::Time deltaTime = clock.restart();
    time_since_last_bullet += deltaTime;

    sf::Event event;
    while (window.pollEvent(event)) {
      if (event.type == sf::Event::Closed) window.close();
    }

    // Player movement
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Left)) {
      player.move(-kPlayerSpeed * deltaTime.asSeconds(), 0.f);
    }
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Right)) {
      player.move(kPlayerSpeed * deltaTime.asSeconds(), 0.f);
    }
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Up)) {
      player.move(0.f, -kPlayerSpeed * deltaTime.asSeconds());
    }
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Down)) {
      player.move(0.f, kPlayerSpeed * deltaTime.asSeconds());
    }

    // Bullet spawn
    if (time_since_last_bullet > kBulletSpawnTime &&
        bullets.size() < MAX_BULLETS) {
      bullets.emplace_back();
      time_since_last_bullet = sf::Time::Zero;
    }

    // Bullet movement
    for (auto& bullet : bullets) {
      bullet.shape.move(bullet.velocity * deltaTime.asSeconds());
      if (bullet.IsOffScreen()) {
        bullet.SetRandomPositionAndVelocity();
      }
    }

    // Check for collisions
    for (auto& bullet : bullets) {
      if (bullet.shape.getGlobalBounds().intersects(player.getGlobalBounds())) {
        window.close();  // End the game
      }
    }

    window.clear();
    window.draw(player);

    for (auto& bullet : bullets) {
      window.draw(bullet.shape);
    }

    window.display();
  }

  return 0;
}
