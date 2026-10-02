#include "window_bridge.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <vector>

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
    HDC surface{};
    HBITMAP surface_bitmap{};
    HGDIOBJ previous_bitmap{};
    int surface_width{};
    int surface_height{};
    double view_center_x{};
    double view_center_y{};
    double pixels_per_unit{1.0};
    bool frame_ready{};
    bool capture_written{};
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

  auto color(const std::int64_t red, const std::int64_t green,
             const std::int64_t blue) -> COLORREF
  {
    fail_if_channel_invalid(red);
    fail_if_channel_invalid(green);
    fail_if_channel_invalid(blue);
    return RGB(static_cast<BYTE>(red), static_cast<BYTE>(green), static_cast<BYTE>(blue));
  }

  auto checked_int(const double value, const char *description) -> int
  {
    if (!std::isfinite(value) || value < static_cast<double>(std::numeric_limits<int>::min()) ||
        value > static_cast<double>(std::numeric_limits<int>::max()))
      throw std::runtime_error(std::string(description) + " is outside the drawable range");
    return static_cast<int>(std::lround(value));
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
    auto &window = state();
    if (window.surface && window.frame_ready)
    {
      BitBlt(device, paint_state.rcPaint.left, paint_state.rcPaint.top,
             paint_state.rcPaint.right - paint_state.rcPaint.left,
             paint_state.rcPaint.bottom - paint_state.rcPaint.top,
             window.surface, paint_state.rcPaint.left, paint_state.rcPaint.top, SRCCOPY);
    }
    else
    {
      HBRUSH brush = CreateSolidBrush(window.clear_color);
      if (brush)
      {
        FillRect(device, &paint_state.rcPaint, brush);
        DeleteObject(brush);
      }
    }
    EndPaint(handle, &paint_state);
  }

  auto release_surface() -> void
  {
    auto &window = state();
    if (window.surface && window.previous_bitmap)
      SelectObject(window.surface, window.previous_bitmap);
    if (window.surface_bitmap) DeleteObject(window.surface_bitmap);
    if (window.surface) DeleteDC(window.surface);
    window.surface = nullptr;
    window.surface_bitmap = nullptr;
    window.previous_bitmap = nullptr;
    window.surface_width = 0;
    window.surface_height = 0;
    window.frame_ready = false;
  }

  auto ensure_surface() -> void
  {
    auto &window = state();
    if (!window.handle) throw std::runtime_error("Cannot draw without an open window");
    RECT client{};
    if (!GetClientRect(window.handle, &client))
      throw std::runtime_error("Could not read the window client size");
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    if (width < 1 || height < 1) throw std::runtime_error("Cannot draw to a minimized window");
    if (window.surface && width == window.surface_width && height == window.surface_height) return;
    release_surface();
    HDC device = GetDC(window.handle);
    if (!device) throw std::runtime_error("Could not access the native drawing device");
    window.surface = CreateCompatibleDC(device);
    window.surface_bitmap = CreateCompatibleBitmap(device, width, height);
    ReleaseDC(window.handle, device);
    if (!window.surface || !window.surface_bitmap)
    {
      release_surface();
      throw std::runtime_error("Could not create the native drawing surface");
    }
    window.previous_bitmap = SelectObject(window.surface, window.surface_bitmap);
    window.surface_width = width;
    window.surface_height = height;
  }

  struct screen_point { int x{}; int y{}; };

  auto world_to_screen(const double x, const double y) -> screen_point
  {
    const auto &window = state();
    return {
      checked_int(static_cast<double>(window.surface_width) * 0.5 +
                  (x - window.view_center_x) * window.pixels_per_unit, "World X coordinate"),
      checked_int(static_cast<double>(window.surface_height) * 0.5 -
                  (y - window.view_center_y) * window.pixels_per_unit, "World Y coordinate")
    };
  }

  auto validate_point_size(const std::int64_t point_size) -> int
  {
    if (point_size < 1 || point_size > 512)
      throw std::runtime_error("Text point size must be between 1 and 512");
    return static_cast<int>(point_size);
  }

  auto draw_text_at(const int x, const int y, const std::string &value,
                    const std::int64_t point_size, const COLORREF text_color) -> void
  {
    auto &window = state();
    ensure_surface();
    const auto converted = utf16(value);
    const int dpi = GetDeviceCaps(window.surface, LOGPIXELSY);
    HFONT font = CreateFontW(-MulDiv(validate_point_size(point_size), dpi, 72), 0, 0, 0,
                            FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                            DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    if (!font) throw std::runtime_error("Could not create the native text font");
    HGDIOBJ previous_font = SelectObject(window.surface, font);
    SetBkMode(window.surface, TRANSPARENT);
    SetTextColor(window.surface, text_color);
    if (!TextOutW(window.surface, x, y, converted.data(), static_cast<int>(converted.size())))
    {
      SelectObject(window.surface, previous_font);
      DeleteObject(font);
      throw std::runtime_error("Could not draw text");
    }
    SelectObject(window.surface, previous_font);
    DeleteObject(font);
  }

  auto capture_frame_if_requested() -> void
  {
    auto &window = state();
    const char *path = std::getenv("SAGAN_RENDER_CAPTURE_BMP");
    if (!path || *path == '\0' || window.capture_written) return;
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = window.surface_width;
    info.bmiHeader.biHeight = -window.surface_height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    const std::size_t pixel_bytes = static_cast<std::size_t>(window.surface_width) *
                                    static_cast<std::size_t>(window.surface_height) * 4;
    std::vector<std::byte> pixels(pixel_bytes);
    if (!GetDIBits(window.surface, window.surface_bitmap, 0,
                   static_cast<UINT>(window.surface_height), pixels.data(), &info, DIB_RGB_COLORS))
      throw std::runtime_error("Could not capture the rendered frame");
    BITMAPFILEHEADER file_header{};
    file_header.bfType = 0x4d42;
    file_header.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    file_header.bfSize = file_header.bfOffBits + static_cast<DWORD>(pixel_bytes);
    std::ofstream output(path, std::ios::binary);
    if (!output) throw std::runtime_error("Could not create the requested BMP capture");
    output.write(reinterpret_cast<const char *>(&file_header), sizeof(file_header));
    output.write(reinterpret_cast<const char *>(&info.bmiHeader), sizeof(info.bmiHeader));
    output.write(reinterpret_cast<const char *>(pixels.data()), static_cast<std::streamsize>(pixels.size()));
    if (!output) throw std::runtime_error("Could not write the requested BMP capture");
    window.capture_written = true;
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
    release_surface();
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
  SetProcessDPIAware();
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
  auto &window = state();
  window.clear_color = color(red, green, blue);
  ensure_surface();
  HBRUSH brush = CreateSolidBrush(window.clear_color);
  RECT bounds{0, 0, window.surface_width, window.surface_height};
  if (!brush || !FillRect(window.surface, &bounds, brush))
  {
    if (brush) DeleteObject(brush);
    throw std::runtime_error("Could not clear the native drawing surface");
  }
  DeleteObject(brush);
  window.frame_ready = true;
}

auto sagan_5f5f72656e6465725f77696e646f775f636c6f7365() -> void
{
  release_window();
}

auto sagan_5f5f72656e6465725f7365745f76696577(
    const double center_x, const double center_y, const double pixels_per_unit) -> void
{
  if (!std::isfinite(center_x) || !std::isfinite(center_y) ||
      !std::isfinite(pixels_per_unit) || pixels_per_unit <= 0.0)
    throw std::runtime_error("The render view requires finite coordinates and a positive scale");
  auto &window = state();
  window.view_center_x = center_x;
  window.view_center_y = center_y;
  window.pixels_per_unit = pixels_per_unit;
}

auto sagan_5f5f72656e6465725f70726573656e74() -> void
{
  auto &window = state();
  ensure_surface();
  if (!window.frame_ready) throw std::runtime_error("Clear the frame before presenting it");
  HDC device = GetDC(window.handle);
  if (!device) throw std::runtime_error("Could not access the window for presentation");
  const BOOL copied = BitBlt(device, 0, 0, window.surface_width, window.surface_height,
                             window.surface, 0, 0, SRCCOPY);
  ReleaseDC(window.handle, device);
  if (!copied) throw std::runtime_error("Could not present the rendered frame");
  capture_frame_if_requested();
}

auto sagan_5f5f72656e6465725f636972636c65(
    const double x, const double y, const double radius, const std::int64_t red,
    const std::int64_t green, const std::int64_t blue) -> void
{
  if (!std::isfinite(radius) || radius <= 0.0)
    throw std::runtime_error("Circle radius must be finite and positive");
  auto &window = state();
  ensure_surface();
  const auto center = world_to_screen(x, y);
  const int screen_radius = checked_int(radius * window.pixels_per_unit, "Circle radius");
  if (screen_radius < 1) throw std::runtime_error("Circle radius is smaller than one screen pixel");
  HBRUSH brush = CreateSolidBrush(color(red, green, blue));
  HPEN pen = CreatePen(PS_SOLID, 1, color(red, green, blue));
  if (!brush || !pen)
  {
    if (brush) DeleteObject(brush);
    if (pen) DeleteObject(pen);
    throw std::runtime_error("Could not create the circle drawing resources");
  }
  HGDIOBJ old_brush = SelectObject(window.surface, brush);
  HGDIOBJ old_pen = SelectObject(window.surface, pen);
  const BOOL drawn = Ellipse(window.surface, center.x - screen_radius, center.y - screen_radius,
                             center.x + screen_radius + 1, center.y + screen_radius + 1);
  SelectObject(window.surface, old_pen);
  SelectObject(window.surface, old_brush);
  DeleteObject(pen);
  DeleteObject(brush);
  if (!drawn) throw std::runtime_error("Could not draw the circle");
}

auto sagan_5f5f72656e6465725f6c696e65(
    const double start_x, const double start_y, const double end_x, const double end_y,
    const double width, const std::int64_t red, const std::int64_t green,
    const std::int64_t blue) -> void
{
  if (!std::isfinite(width) || width <= 0.0)
    throw std::runtime_error("Line width must be finite and positive");
  auto &window = state();
  ensure_surface();
  const auto start = world_to_screen(start_x, start_y);
  const auto end = world_to_screen(end_x, end_y);
  const int screen_width = std::max(1, checked_int(width, "Line width"));
  HPEN pen = CreatePen(PS_SOLID, screen_width, color(red, green, blue));
  if (!pen) throw std::runtime_error("Could not create the line drawing resource");
  HGDIOBJ old_pen = SelectObject(window.surface, pen);
  const BOOL moved = MoveToEx(window.surface, start.x, start.y, nullptr);
  const BOOL drawn = moved && LineTo(window.surface, end.x, end.y);
  SelectObject(window.surface, old_pen);
  DeleteObject(pen);
  if (!drawn) throw std::runtime_error("Could not draw the line");
}

auto sagan_5f5f72656e6465725f74657874(
    const double x, const double y, const std::string &value, const std::int64_t point_size,
    const std::int64_t red, const std::int64_t green, const std::int64_t blue) -> void
{
  ensure_surface();
  const auto anchor = world_to_screen(x, y);
  draw_text_at(anchor.x, anchor.y, value, point_size, color(red, green, blue));
}

auto sagan_5f5f72656e6465725f746578745f73637265656e(
    const std::int64_t x, const std::int64_t y, const std::string &value,
    const std::int64_t point_size, const std::int64_t red, const std::int64_t green,
    const std::int64_t blue) -> void
{
  if (x < std::numeric_limits<int>::min() || x > std::numeric_limits<int>::max() ||
      y < std::numeric_limits<int>::min() || y > std::numeric_limits<int>::max())
    throw std::runtime_error("Screen text position is outside the drawable range");
  draw_text_at(static_cast<int>(x), static_cast<int>(y), value, point_size,
               color(red, green, blue));
}

#else

namespace
{
  [[noreturn]] auto unsupported() -> void
  {
    throw std::runtime_error("sagan-render 0.2.0 currently supports Windows only");
  }
}

auto sagan_5f5f72656e6465725f77696e646f775f6f70656e(
    const std::string &, std::int64_t, std::int64_t) -> bool { unsupported(); }
auto sagan_5f5f72656e6465725f77696e646f775f706f6c6c() -> bool { unsupported(); }
auto sagan_5f5f72656e6465725f77696e646f775f636c656172(
    std::int64_t, std::int64_t, std::int64_t) -> void { unsupported(); }
auto sagan_5f5f72656e6465725f77696e646f775f636c6f7365() -> void { unsupported(); }
auto sagan_5f5f72656e6465725f7365745f76696577(double, double, double) -> void { unsupported(); }
auto sagan_5f5f72656e6465725f70726573656e74() -> void { unsupported(); }
auto sagan_5f5f72656e6465725f636972636c65(
    double, double, double, std::int64_t, std::int64_t, std::int64_t) -> void { unsupported(); }
auto sagan_5f5f72656e6465725f6c696e65(
    double, double, double, double, double, std::int64_t, std::int64_t, std::int64_t) -> void { unsupported(); }
auto sagan_5f5f72656e6465725f74657874(
    double, double, const std::string &, std::int64_t, std::int64_t, std::int64_t, std::int64_t) -> void { unsupported(); }
auto sagan_5f5f72656e6465725f746578745f73637265656e(
    std::int64_t, std::int64_t, const std::string &, std::int64_t,
    std::int64_t, std::int64_t, std::int64_t) -> void { unsupported(); }

#endif
