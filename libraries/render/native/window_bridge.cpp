#include "window_bridge.hpp"

#include <chrono>
#include <cstdlib>
#include <limits>
#include <stdexcept>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace
{
  constexpr wchar_t window_class_name[] = L"SaganRenderWindowR0";

  struct window_state
  {
    HINSTANCE instance{};
    HWND handle{};
    COLORREF clear_color{RGB(22, 30, 46)};
    bool class_registered{};
    bool close_requested{};
    std::chrono::steady_clock::time_point opened_at{};
    std::chrono::milliseconds auto_close_after{};
  };

  auto state() -> window_state &
  {
    static window_state value;
    return value;
  }

  auto fail_if_channel_invalid(const std::int64_t value) -> void
  {
    if (value < 0 || value > 255)
      throw std::runtime_error("Window clear color channels must be between 0 and 255");
  }

  auto utf16(const std::string &value) -> std::wstring
  {
    if (value.empty()) return {};
    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                           static_cast<int>(value.size()), nullptr, 0);
    if (length <= 0) throw std::runtime_error("Window title is not valid UTF-8");
    std::wstring result(static_cast<std::size_t>(length), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                            static_cast<int>(value.size()), result.data(), length) != length)
      throw std::runtime_error("Could not convert the window title to UTF-16");
    return result;
  }

  auto paint(const HWND handle) -> void
  {
    PAINTSTRUCT paint_state{};
    HDC device = BeginPaint(handle, &paint_state);
    HBRUSH brush = CreateSolidBrush(state().clear_color);
    if (brush)
    {
      FillRect(device, &paint_state.rcPaint, brush);
      DeleteObject(brush);
    }
    EndPaint(handle, &paint_state);
  }

  auto CALLBACK window_procedure(const HWND handle, const UINT message,
                                 const WPARAM word, const LPARAM data) -> LRESULT
  {
    switch (message)
    {
    case WM_CLOSE:
      state().close_requested = true;
      DestroyWindow(handle);
      return 0;
    case WM_DESTROY:
      state().handle = nullptr;
      state().close_requested = true;
      return 0;
    case WM_ERASEBKGND:
      return 1;
    case WM_PAINT:
      paint(handle);
      return 0;
    default:
      return DefWindowProcW(handle, message, word, data);
    }
  }

  auto configured_auto_close() -> std::chrono::milliseconds
  {
    const char *raw = std::getenv("SAGAN_RENDER_AUTOCLOSE_MS");
    if (!raw || *raw == '\0') return {};
    char *end{};
    const long value = std::strtol(raw, &end, 10);
    if (*end != '\0' || value < 1 || value > 60000)
      throw std::runtime_error("SAGAN_RENDER_AUTOCLOSE_MS must be between 1 and 60000");
    return std::chrono::milliseconds(value);
  }

  auto release_window() -> void
  {
    auto &window = state();
    if (window.handle) DestroyWindow(window.handle);
    window.handle = nullptr;
    if (window.class_registered)
    {
      UnregisterClassW(window_class_name, window.instance);
      window.class_registered = false;
    }
    window.close_requested = true;
  }
}

auto sagan_5f5f72656e6465725f77696e646f775f6f70656e(
    const std::string &title, const std::int64_t width, const std::int64_t height) -> bool
{
  if (width < 1 || height < 1 || width > std::numeric_limits<int>::max() ||
      height > std::numeric_limits<int>::max())
    throw std::runtime_error("Window width and height must be positive Int32-sized values");

  auto &window = state();
  if (window.handle) throw std::runtime_error("R0 supports only one open window");
  window = {};
  window.instance = GetModuleHandleW(nullptr);
  window.clear_color = RGB(22, 30, 46);
  window.auto_close_after = configured_auto_close();

  WNDCLASSEXW descriptor{};
  descriptor.cbSize = sizeof(descriptor);
  descriptor.hInstance = window.instance;
  descriptor.lpfnWndProc = window_procedure;
  descriptor.lpszClassName = window_class_name;
  descriptor.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
  if (!RegisterClassExW(&descriptor))
    throw std::runtime_error("Could not register the native window class");
  window.class_registered = true;

  RECT bounds{0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
  if (!AdjustWindowRectEx(&bounds, WS_OVERLAPPEDWINDOW, FALSE, 0))
  {
    release_window();
    throw std::runtime_error("Could not calculate the native window size");
  }

  const auto converted_title = utf16(title);
  window.handle = CreateWindowExW(0, window_class_name, converted_title.c_str(),
                                  WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                                  bounds.right - bounds.left, bounds.bottom - bounds.top,
                                  nullptr, nullptr, window.instance, nullptr);
  if (!window.handle)
  {
    release_window();
    throw std::runtime_error("Could not create the native window");
  }
  window.opened_at = std::chrono::steady_clock::now();
  ShowWindow(window.handle, SW_SHOW);
  UpdateWindow(window.handle);
  return true;
}

auto sagan_5f5f72656e6465725f77696e646f775f706f6c6c() -> bool
{
  auto &window = state();
  if (!window.handle || window.close_requested) return false;
  MSG message{};
  while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
  {
    TranslateMessage(&message);
    DispatchMessageW(&message);
  }
  if (window.auto_close_after.count() > 0 &&
      std::chrono::steady_clock::now() - window.opened_at >= window.auto_close_after)
    release_window();
  return window.handle && !window.close_requested;
}

auto sagan_5f5f72656e6465725f77696e646f775f636c656172(
    const std::int64_t red, const std::int64_t green, const std::int64_t blue) -> void
{
  fail_if_channel_invalid(red);
  fail_if_channel_invalid(green);
  fail_if_channel_invalid(blue);
  auto &window = state();
  if (!window.handle) throw std::runtime_error("Cannot clear a window that is not open");
  window.clear_color = RGB(static_cast<BYTE>(red), static_cast<BYTE>(green), static_cast<BYTE>(blue));
  InvalidateRect(window.handle, nullptr, FALSE);
  UpdateWindow(window.handle);
}

auto sagan_5f5f72656e6465725f77696e646f775f636c6f7365() -> void
{
  release_window();
}

#else

namespace
{
  [[noreturn]] auto unsupported() -> void
  {
    throw std::runtime_error("sagan-render 0.1.0 R0 currently supports Windows only");
  }
}

auto sagan_5f5f72656e6465725f77696e646f775f6f70656e(
    const std::string &, std::int64_t, std::int64_t) -> bool { unsupported(); }
auto sagan_5f5f72656e6465725f77696e646f775f706f6c6c() -> bool { unsupported(); }
auto sagan_5f5f72656e6465725f77696e646f775f636c656172(
    std::int64_t, std::int64_t, std::int64_t) -> void { unsupported(); }
auto sagan_5f5f72656e6465725f77696e646f775f636c6f7365() -> void { unsupported(); }

#endif
