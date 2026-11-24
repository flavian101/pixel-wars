#pragma once

#include <windows.h>
#include <functional>

// Win32 window that owns the message loop and forwards input/resize events
// through callbacks set by the app.
class Window {
public:
    using KeyCallback    = std::function<void(WPARAM key, bool down)>;
    using MouseCallback  = std::function<void(int x, int y, bool leftDown)>;
    using ResizeCallback = std::function<void(int w, int h)>;

    Window() = default;
    ~Window() = default;
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool create(HINSTANCE instance, int width, int height, const wchar_t* title);

    HWND handle() const { return hwnd_; }
    bool isOpen() const { return !shouldClose_; }

    // Pump messages; returns false when the window wants to close.
    bool pumpMessages();

    KeyCallback    onKey;
    MouseCallback  onMouse;
    ResizeCallback onResize;

private:
    static LRESULT CALLBACK wndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT handleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    HWND hwnd_ = nullptr;
    bool shouldClose_ = false;
};
