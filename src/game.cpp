#include "game.h"
#include "renderer.h"
#include "constants.h"

#include <algorithm>
#include <cmath>

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------
Game::Game() : rng_(std::random_device{}()) {
    player_.x = WORLD_WIDTH  * 0.5f;
    player_.y = WORLD_HEIGHT * 0.5f;

    // Scatter a few starting buff pickups on the map. More respawn over time.
    std::uniform_real_distribution<float> rx(10.0f, WORLD_WIDTH - 10.0f);
    std::uniform_real_distribution<float> ry(10.0f, WORLD_HEIGHT - 10.0f);
    for (int i = 0; i < 6; ++i) {
        BuffKind kind = static_cast<BuffKind>(i % 4);
        pickups_.push_back({ rx(rng_), ry(rng_), kind, 0.0f });
    }
}

void Game::setMove(float x, float y) { inputX_ = x; inputY_ = y; }
void Game::setAimWorld(float x, float y) { aimX_ = x; aimY_ = y; }
void Game::setFiring(bool firing) { firing_ = firing; }

bool Game::circlesOverlap(float ax, float ay, float ar,
                          float bx, float by, float br) const {
    float dx = ax - bx, dy = ay - by, r = ar + br;
    return dx * dx + dy * dy <= r * r;
}

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------
void Game::update(float dt) {
    if (dt <= 0.0f || dt > 0.25f) dt = 0.016f;   // clamp pathological frames
    elapsed_ += dt;
    if (gameOver()) return;

    updatePlayer(dt);
    updateSpawning(dt);
    updateEnemies(dt);
    updateProjectiles(dt);
    updateBuffs(dt);
    handleCombat();
}

void Game::updatePlayer(float dt) {
    float speedMul = 1.0f;
    for (const auto& b : activeBuffs_)
        if (b.kind == BuffKind::Speed) speedMul = 1.5f;

    float len = std::sqrt(inputX_ * inputX_ + inputY_ * inputY_);
    if (len > 0.001f) {
        float inv = (player_.speed * speedMul * dt) / len;
        player_.x += inputX_ * inv;
        player_.y += inputY_ * inv;
    }
    player_.x = std::clamp(player_.x, player_.halfSize, WORLD_WIDTH  - player_.halfSize);
    player_.y = std::clamp(player_.y, player_.halfSize, WORLD_HEIGHT - player_.halfSize);

    // Regen buff
    for (const auto& b : activeBuffs_)
        if (b.kind == BuffKind::Regen) player_.hp = std::min(player_.maxHp, player_.hp + 5.0f * dt);

    // Shooting (ranged attack)
    player_.fireCooldown -= dt;
    float dmgMul = 1.0f;
    for (const auto& b : activeBuffs_)
        if (b.kind == BuffKind::Damage) dmgMul = 2.0f;

    if (firing_ && player_.fireCooldown <= 0.0f) {
        float dx = aimX_ - player_.x, dy = aimY_ - player_.y;
        float d = std::sqrt(dx * dx + dy * dy);
        if (d > 0.001f) {
            float projSpeed = 120.0f;
            projectiles_.push_back({
                player_.x, player_.y,
                dx / d * projSpeed, dy / d * projSpeed,
                1.5f,                       // life in seconds
                player_.attack * dmgMul });
            player_.fireCooldown = 0.25f;
        }
    }
}

void Game::updateSpawning(float dt) {
    spawnTimer_ -= dt;
    int maxEnemies = 4 + static_cast<int>(elapsed_ / 20.0f) * 2;   // ramps over time
    if (spawnTimer_ <= 0.0f && static_cast<int>(enemies_.size()) < maxEnemies) {
        spawnEnemy();
        spawnTimer_ = std::max(0.5f, 3.0f - elapsed_ / 30.0f);      // spawns accelerate
    }
}

void Game::spawnEnemy() {
    int tier = elapsed_ < 60.0f ? 0 : (elapsed_ < 150.0f ? 1 : 2);

    // Tier -> stats (difficulty scaling)
    float size   = tier == 0 ? 0.5f  : tier == 1 ? 0.8f  : 1.2f;
    float hp     = tier == 0 ? 20.0f : tier == 1 ? 40.0f : 80.0f;
    float speed  = tier == 0 ? 18.0f : tier == 1 ? 14.0f : 10.0f;
    float damage = tier == 0 ?  8.0f : tier == 1 ? 15.0f : 25.0f;

    // Spawn at a random point on the map edge
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    int edge = static_cast<int>(dist(rng_) * 4.0f);
    float x, y;
    switch (edge) {
    case 0: x = dist(rng_) * WORLD_WIDTH;  y = -2.0f; break;
    case 1: x = dist(rng_) * WORLD_WIDTH;  y = WORLD_HEIGHT + 2.0f; break;
    case 2: x = -2.0f; y = dist(rng_) * WORLD_HEIGHT; break;
    default: x = WORLD_WIDTH + 2.0f; y = dist(rng_) * WORLD_HEIGHT; break;
    }

    enemies_.push_back({ x, y, size, hp, speed, damage, 0.0f, tier });
}

void Game::updateEnemies(float dt) {
    for (auto& e : enemies_) {
        e.touchCooldown -= dt;
        // Chase the player
        float dx = player_.x - e.x, dy = player_.y - e.y;
        float d = std::sqrt(dx * dx + dy * dy);
        if (d > 0.001f) {
            e.x += dx / d * e.speed * dt;
            e.y += dy / d * e.speed * dt;
        }
    }
}

void Game::updateProjectiles(float dt) {
    for (size_t i = projectiles_.size(); i-- > 0;) {
        auto& p = projectiles_[i];
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        p.life -= dt;

        bool hit = p.life <= 0.0f;
        if (!hit) {
            for (auto& e : enemies_) {
                if (circlesOverlap(p.x, p.y, 0.25f, e.x, e.y, e.halfSize)) {
                    e.hp -= p.damage;
                    hit = true;
                    break;
                }
            }
        }
        if (hit) projectiles_.erase(projectiles_.begin() + static_cast<ptrdiff_t>(i));
    }
}

void Game::updateBuffs(float dt) {
    // Expire active buffs
    for (size_t i = activeBuffs_.size(); i-- > 0;) {
        activeBuffs_[i].remaining -= dt;
        if (activeBuffs_[i].remaining <= 0.0f)
            activeBuffs_.erase(activeBuffs_.begin() + static_cast<ptrdiff_t>(i));
    }

    // Respawn consumed pickups after 20s
    for (auto& p : pickups_) {
        if (p.respawnTimer > 0.0f)
            p.respawnTimer -= dt;
    }
}

void Game::applyBuff(BuffKind kind) {
    activeBuffs_.push_back({ kind, 8.0f });
}

void Game::handleCombat() {
    bool shielded = false;
    for (const auto& b : activeBuffs_)
        if (b.kind == BuffKind::Shield) shielded = true;

    // Enemies touch the player
    float dmgMul = shielded ? 0.25f : 1.0f;
    for (auto& e : enemies_) {
        if (e.touchCooldown <= 0.0f &&
            circlesOverlap(e.x, e.y, e.halfSize,
                           player_.x, player_.y, player_.halfSize)) {
            player_.hp -= std::max(1.0f, e.damage * dmgMul - player_.defense);
            e.touchCooldown = 0.5f;   // hit i-frames for the enemy
        }
    }

    // Player picks up buffs
    for (auto& p : pickups_) {
        if (p.respawnTimer <= 0.0f &&
            circlesOverlap(p.x, p.y, 0.6f, player_.x, player_.y, player_.halfSize)) {
            applyBuff(p.kind);
            p.respawnTimer = 20.0f;
        }
    }

    // Killed enemies -> absorb their pixels (grow)
    for (size_t i = enemies_.size(); i-- > 0;) {
        if (enemies_[i].hp <= 0.0f) killEnemy(i);
    }
}

void Game::killEnemy(size_t index) {
    const Enemy& e = enemies_[index];
    ++score_;

    // Growth: size, attack, defense
    player_.halfSize = std::min(2.5f, player_.halfSize + 0.03f);
    player_.attack  += 1.0f;
    player_.defense += 0.2f;
    player_.maxHp   += 2.0f;
    player_.hp = std::min(player_.maxHp, player_.hp + 4.0f);

    enemies_.erase(enemies_.begin() + static_cast<ptrdiff_t>(index));
}

// ---------------------------------------------------------------------------
// Draw
// ---------------------------------------------------------------------------
void Game::draw(Renderer& r) const {
    auto tierColor = [](int t, float& rr, float& gg, float& bb) {
        switch (t) {
        case 0: rr = 0.9f; gg = 0.2f; bb = 0.2f; break;   // red
        case 1: rr = 1.0f; gg = 0.55f; bb = 0.1f; break;  // orange
        default: rr = 0.6f; gg = 0.1f; bb = 0.8f; break;  // purple
        }
    };
    auto buffColor = [](BuffKind k, float& rr, float& gg, float& bb) {
        switch (k) {
        case BuffKind::Speed:  rr = 0.2f; gg = 0.9f; bb = 0.9f; break; // cyan
        case BuffKind::Damage: rr = 1.0f; gg = 0.9f; bb = 0.1f; break; // yellow
        case BuffKind::Shield: rr = 0.3f; gg = 0.5f; bb = 1.0f; break; // blue
        case BuffKind::Regen:  rr = 0.2f; gg = 1.0f; bb = 0.3f; break; // green
        }
    };

    // Pickups
    for (const auto& p : pickups_) {
        if (p.respawnTimer > 0.0f) continue;
        float rr, gg, bb; buffColor(p.kind, rr, gg, bb);
        r.drawQuad(p.x, p.y, 0.45f, rr, gg, bb);
    }

    // Enemies
    for (const auto& e : enemies_) {
        float rr, gg, bb; tierColor(e.tier, rr, gg, bb);
        r.drawQuad(e.x, e.y, e.halfSize, rr, gg, bb);
    }

    // Projectiles
    for (const auto& p : projectiles_)
        r.drawQuad(p.x, p.y, 0.2f, 1.0f, 1.0f, 1.0f);

    // Player (blue ring while shielded is drawn first, white core on top)
    for (const auto& b : activeBuffs_) {
        if (b.kind == BuffKind::Shield)
            r.drawQuad(player_.x, player_.y, player_.halfSize + 0.25f,
                       0.3f, 0.5f, 1.0f);
    }
    r.drawQuad(player_.x, player_.y, player_.halfSize, 1.0f, 1.0f, 1.0f);
}
