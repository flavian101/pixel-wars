#include "window.h"

bool Window::create(HINSTANCE instance, int width, int height, const wchar_t* title) {
    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = &Window::wndProc;
    wc.hInstance     = instance;
    wc.lpszClassName = L"PixelWarsWindowClass";
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);

    if (!RegisterClassExW(&wc)) return false;

    hwnd_ = CreateWindowExW(
        0, wc.lpszClassName, title,
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME,   // fixed size for now
        CW_USEDEFAULT, CW_USEDEFAULT, width, height,
        nullptr, nullptr, instance, this);

    if (!hwnd_) return false;

    ShowWindow(hwnd_, SW_SHOWDEFAULT);
    SetForegroundWindow(hwnd_);
    return true;
}

bool Window::pumpMessages() {
    MSG msg{};
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            shouldClose_ = true;
            break;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return !shouldClose_;
}

LRESULT CALLBACK Window::wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    Window* self = nullptr;

    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<Window*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = hwnd;
    } else {
        self = reinterpret_cast<Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self) return self->handleMessage(msg, wParam, lParam);
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT Window::handleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_DESTROY:
        shouldClose_ = true;
        PostQuitMessage(0);
        return 0;

    case WM_KEYDOWN:
        if (onKey) onKey(wParam, true);
        return 0;
    case WM_KEYUP:
        if (onKey) onKey(wParam, false);
        return 0;

    case WM_MOUSEMOVE:
        if (onMouse) onMouse(LOWORD(lParam), HIWORD(lParam),
                             (wParam & MK_LBUTTON) != 0);
        return 0;
    case WM_LBUTTONDOWN:
        if (onMouse) onMouse(LOWORD(lParam), HIWORD(lParam), true);
        return 0;
    case WM_LBUTTONUP:
        if (onMouse) onMouse(LOWORD(lParam), HIWORD(lParam), false);
        return 0;

    case WM_SIZE:
        if (onResize && wParam != SIZE_MINIMIZED)
            onResize(LOWORD(lParam), HIWORD(lParam));
        return 0;
    }
    return DefWindowProcW(hwnd_, msg, wParam, lParam);
}
