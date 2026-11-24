#pragma once

#include <vector>
#include <random>
#include <cstdint>

class Renderer;

// All gameplay state and rules. Rendering is done by pushing quads into the
// Renderer passed to draw() -- the game never touches D3D11 directly.
class Game {
public:
    Game();

    // Input
    void setMove(float x, float y);            // -1..1 direction
    void setAimWorld(float x, float y);
    void setFiring(bool firing);

    void update(float dt);                      // dt in seconds
    void draw(Renderer& renderer) const;

    int score() const { return score_; }
    bool gameOver() const { return player_.hp <= 0.0f; }

private:
    // --- entities ---------------------------------------------------------
    struct Player {
        float x = 0, y = 0;
        float halfSize = 0.5f;   // grows as you absorb pixels
        float speed    = 30.0f;
        float hp       = 100.0f;
        float maxHp    = 100.0f;
        float attack   = 10.0f;
        float defense  = 0.0f;
        float fireCooldown = 0.0f;
    };

    struct Enemy {
        float x, y;
        float halfSize;
        float hp;
        float speed;
        float damage;
        float touchCooldown = 0.0f;
        int   tier = 0;           // 0 early .. 2 late
    };

    struct Projectile {
        float x, y, vx, vy;
        float life;
        float damage;
    };

    // Temporary floating buffs found in the world (not alpha equipment).
    enum class BuffKind { Speed, Damage, Shield, Regen };

    struct BuffPickup {
        float x, y;
        BuffKind kind;
        float respawnTimer = 0.0f;   // 0 = on the map
    };

    struct ActiveBuff {
        BuffKind kind;
        float remaining;
    };

    // --- helpers ----------------------------------------------------------
    void updatePlayer(float dt);
    void updateEnemies(float dt);
    void updateProjectiles(float dt);
    void updateBuffs(float dt);
    void updateSpawning(float dt);
    void handleCombat();

    void spawnEnemy();
    void killEnemy(size_t index);
    void applyBuff(BuffKind kind);
    bool circlesOverlap(float ax, float ay, float ar, float bx, float by, float br) const;

    // --- state ------------------------------------------------------------
    Player player_;
    std::vector<Enemy>      enemies_;
    std::vector<Projectile> projectiles_;
    std::vector<BuffPickup> pickups_;
    std::vector<ActiveBuff> activeBuffs_;

    float inputX_ = 0, inputY_ = 0;
    float aimX_ = 0, aimY_ = 0;
    bool  firing_ = false;

    float spawnTimer_   = 0.0f;
    float elapsed_      = 0.0f;    // drives difficulty tier
    int   score_        = 0;

    std::mt19937 rng_;
};
