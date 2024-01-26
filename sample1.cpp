#include <SFML/Graphics.hpp>
#include <cstdlib>
#include <ctime>
#include <vector>

const int kWindowWidth = 800;
const int kWindowHeight = 600;
const float kPlayerSpeed = 200.f;
const sf::Time kBulletSpawnTime = sf::seconds(1.f);

class Bullet {
 public:
  sf::CircleShape shape;
  sf::Vector2f velocity;

  Bullet(float radius = 5.f) : velocity(0.f, 150.f) {
    shape.setRadius(radius);
    shape.setFillColor(sf::Color::Red);
  }
};

int main() {
  sf::RenderWindow window(sf::VideoMode(kWindowWidth, kWindowHeight),
                          "sample1");
  window.setFramerateLimit(60);

  sf::RectangleShape player(sf::Vector2f(50.f, 50.f));
  player.setFillColor(sf::Color::Green);
  player.setPosition(kWindowWidth / 2.f, kWindowHeight - player.getSize().y);

  std::vector<Bullet> bullets;
  sf::Clock clock;
  sf::Time time_since_last_bullet = sf::Time::Zero;

  while (window.isOpen()) {
    sf::Time delta_time = clock.restart();
    time_since_last_bullet += delta_time;

    sf::Event event;
    while (window.pollEvent(event)) {
      if (event.type == sf::Event::Closed) window.close();
    }

    // Player movement
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Left)) {
      player.move(-kPlayerSpeed * delta_time.asSeconds(), 0.f);
    }
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Right)) {
      player.move(kPlayerSpeed * delta_time.asSeconds(), 0.f);
    }

    // Bullet spawn
    if (time_since_last_bullet > kBulletSpawnTime) {
      Bullet bullet;
      bullet.shape.setPosition(rand() % kWindowWidth, 0.f);
      bullets.push_back(bullet);
      time_since_last_bullet = sf::Time::Zero;
    }

    // Bullet movement
    for (auto& bullet : bullets) {
      bullet.shape.move(bullet.velocity * delta_time.asSeconds());
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
