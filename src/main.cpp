#include <windows.h>
#include <chrono>

#include "window.h"
#include "renderer.h"
#include "game.h"
#include "constants.h"

// Top-level app: owns the window, the renderer, and the game; runs the
// frame loop shape described in the skill:
//   1. Pump OS/input events
//   2. Advance game state with real delta time
//   3. Clear + draw
//   4. Present

namespace {

class App {
public:
    bool init(HINSTANCE instance) {
        if (!window_.create(instance, WINDOW_WIDTH, WINDOW_HEIGHT, L"Pixel Wars"))
            return false;
        if (!renderer_.initialize(window_.handle(), WINDOW_WIDTH, WINDOW_HEIGHT))
            return false;

        window_.onKey    = [this](WPARAM key, bool down) { onKey(key, down); };
        window_.onMouse  = [this](int x, int y, bool l)  { onMouse(x, y, l); };
        window_.onResize = [this](int w, int h)          { renderer_.resize(w, h); };

        lastTime_ = std::chrono::steady_clock::now();
        return true;
    }

    int run() {
        while (window_.isOpen()) {
            if (!window_.pumpMessages()) break;

            auto now = std::chrono::steady_clock::now();
            float dt = std::chrono::duration<float>(now - lastTime_).count();
            lastTime_ = now;

            game_.update(dt);

            renderer_.beginFrame(0.05f, 0.05f, 0.08f);
            game_.draw(renderer_);
            renderer_.endFrame();
        }
        return 0;
    }

private:
    void onKey(WPARAM key, bool down) {
        float v = down ? 1.0f : -1.0f;
        switch (key) {
        case 'W': inputY_ -= v; break;   // y is down-positive; W moves up
        case 'S': inputY_ += v; break;
        case 'A': inputX_ -= v; break;
        case 'D': inputX_ += v; break;
        default: break;
        }
        inputX_ = std::clamp(inputX_, -1.0f, 1.0f);
        inputY_ = std::clamp(inputY_, -1.0f, 1.0f);
        game_.setMove(inputX_, inputY_);
    }

    void onMouse(int px, int py, bool leftDown) {
        // Screen pixels -> world units (window is letterboxed to world aspect).
        game_.setAimWorld(px / static_cast<float>(WINDOW_WIDTH)  * WORLD_WIDTH,
                          py / static_cast<float>(WINDOW_HEIGHT) * WORLD_HEIGHT);
        game_.setFiring(leftDown);
    }

    Window   window_;
    Renderer renderer_;
    Game     game_;

    float inputX_ = 0.0f, inputY_ = 0.0f;
    std::chrono::steady_clock::time_point lastTime_{};
};

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    App app;
    if (!app.init(instance)) {
        MessageBoxW(nullptr, L"Failed to initialize Pixel Wars.", L"Error", MB_ICONERROR);
        return 1;
    }
    return app.run();
}
