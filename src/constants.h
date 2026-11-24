#pragma once

// World is measured in abstract "pixel units".
// One unit = one pixel block on screen (scaled by the renderer's ortho matrix).
inline constexpr float WORLD_WIDTH  = 160.0f;
inline constexpr float WORLD_HEIGHT = 100.0f;

// Window size in actual screen pixels.
inline constexpr int WINDOW_WIDTH  = 1280;
inline constexpr int WINDOW_HEIGHT = 800;
