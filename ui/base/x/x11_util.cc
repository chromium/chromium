// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// This file defines utility functions for X11 (Linux only). This code has been
// ported from XCB since we can't use XCB on Ubuntu while its 32-bit support
// remains woefully incomplete.

#include "ui/base/x/x11_util.h"

#include <sys/ipc.h>
#include <sys/shm.h>

#include <bitset>
#include <limits>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/display/util/gpu_info_util.h"
#include "ui/events/devices/x11/device_data_manager_x11.h"
#include "ui/events/devices/x11/touch_factory_x11.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/switches.h"
#include "ui/gfx/x/atom_cache.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/screensaver.h"
#include "ui/gfx/x/shm.h"
#include "ui/gfx/x/xproto.h"

#if BUILDFLAG(IS_FREEBSD)
#include <sys/sysctl.h>
#include <sys/types.h>
#endif

namespace ui {
namespace {

// Constants that are part of EWMH.
constexpr int kNetWMStateAdd = 1;
constexpr int kNetWMStateRemove = 0;

// Returns whether the X11 Screen Saver Extension can be used to disable the
// screen saver.
bool IsX11ScreenSaverAvailable() {
  // X Screen Saver isn't accessible in headless mode.
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kHeadless) &&
         x11::Connection::Get()->screensaver_version() >=
             std::pair<uint32_t, uint32_t>{1, 1};
}

// Returns true if the event has event_x and event_y fields.
bool EventHasCoordinates(const x11::Event& event) {
  return event.As<x11::KeyEvent>() || event.As<x11::ButtonEvent>() ||
         event.As<x11::MotionNotifyEvent>() || event.As<x11::CrossingEvent>() ||
         event.As<x11::Input::LegacyDeviceEvent>() ||
         event.As<x11::Input::DeviceEvent>() ||
         event.As<x11::Input::CrossingEvent>();
}

}  // namespace

size_t RowBytesForVisualWidth(const x11::Connection::VisualInfo& visual_info,
                              int width) {
  auto bpp = visual_info.format->bits_per_pixel;
  auto align = visual_info.format->scanline_pad;
  size_t row_bits = bpp * width;
  row_bits += (align - (row_bits % align)) % align;
  return (row_bits + 7) / 8;
}

void DrawPixmap(x11::Connection* connection,
                x11::VisualId visual,
                x11::Drawable drawable,
                x11::GraphicsContext gc,
                const SkPixmap& skia_pixmap,
                int src_x,
                int src_y,
                int dst_x,
                int dst_y,
                int width,
                int height) {
  // 24 bytes for the PutImage header, an additional 4 bytes in case this is an
  // extended size request, and an additional 4 bytes in case padding is needed.
  constexpr size_t kPutImageExtraSize = 32;

  const auto* visual_info = connection->GetVisualInfoFromId(visual);
  if (!visual_info) {
    return;
  }

  size_t row_bytes = RowBytesForVisualWidth(*visual_info, width);

  auto color_type = ColorTypeForVisual(visual);
  if (color_type == kUnknown_SkColorType) {
    // TODO(crbug.com/40124639): Add a fallback path in case any users
    // are running a server that uses visual types for which Skia doesn't have
    // a corresponding color format.
    return;
  }
  SkImageInfo image_info =
      SkImageInfo::Make(width, height, color_type, kPremul_SkAlphaType);

  std::vector<uint8_t> vec(row_bytes * height);
  SkPixmap pixmap(image_info, vec.data(), row_bytes);
  skia_pixmap.readPixels(pixmap, src_x, src_y);

  DCHECK_GT(connection->MaxRequestSizeInBytes(), kPutImageExtraSize);
  int rows_per_request =
      (connection->MaxRequestSizeInBytes() - kPutImageExtraSize) / row_bytes;
  DCHECK_GT(rows_per_request, 1);
  for (int row = 0; row < height; row += rows_per_request) {
    size_t n_rows = std::min<size_t>(rows_per_request, height - row);
    auto data = base::MakeRefCounted<base::RefCountedStaticMemory>(
        base::span(vec).subspan(row * row_bytes, n_rows * row_bytes));
    connection->PutImage({
        .format = x11::ImageFormat::ZPixmap,
        .drawable = drawable,
        .gc = gc,
        .width = static_cast<uint16_t>(width),
        .height = static_cast<uint16_t>(n_rows),
        .dst_x = static_cast<int16_t>(dst_x),
        .dst_y = static_cast<int16_t>(dst_y + row),
        .left_pad = 0,
        .depth = visual_info->format->depth,
        .data = data,
    });
  }
  // Flush since the PutImage requests depend on |vec| being alive.
  connection->Flush();
}

bool IsXInput2Available() {
  return DeviceDataManagerX11::GetInstance()->IsXInput2Available();
}

bool QueryShmSupport() {
  return x11::Connection::Get()->shm_version() >
         std::pair<uint32_t, uint32_t>{0, 0};
}

int CoalescePendingMotionEvents(const x11::Event& x11_event,
                                x11::Event* last_event) {
  auto* conn = x11::Connection::Get();
  auto* ddmx11 = ui::DeviceDataManagerX11::GetInstance();
  int num_coalesced = 0;

  const auto* motion = x11_event.As<x11::MotionNotifyEvent>();
  const auto* device = x11_event.As<x11::Input::DeviceEvent>();
  DCHECK(motion || device);
  DCHECK(!device || device->opcode == x11::Input::DeviceEvent::Motion ||
         device->opcode == x11::Input::DeviceEvent::TouchUpdate);

  conn->ReadResponses();
  for (auto& event : conn->events()) {
    // There may be non-input events such as ConfigureNotifyEvents and
    // PropertyNotifyEvents that get interleaved between mouse events, so it is
    // necessary to skip over those to coalesce as many pending motion events as
    // possible so mouse dragging is smooth.
    if (!EventHasCoordinates(event)) {
      continue;
    }

    if (motion) {
      const auto* next_motion = event.As<x11::MotionNotifyEvent>();

      // Discard all but the most recent motion event that targets the same
      // window with unchanged state.
      if (next_motion && next_motion->event == motion->event &&
          next_motion->child == motion->child &&
          next_motion->state == motion->state) {
        *last_event = std::move(event);
        continue;
      }
    } else {
      auto* next_device = event.As<x11::Input::DeviceEvent>();
      if (!next_device) {
        break;
      }

      // If this isn't from a valid device, throw the event away, as
      // that's what the message pump would do. Device events come in pairs
      // with one from the master and one from the slave so there will
      // always be at least one pending.
      if (!ui::TouchFactory::GetInstance()->ShouldProcessDeviceEvent(
              *next_device)) {
        event = x11::Event();
        continue;
      }

      // Confirm that the motion event is of the same type, is
      // targeted at the same window, and that no buttons or modifiers
      // have changed.
      if (next_device->opcode == device->opcode &&
          !ddmx11->IsCMTGestureEvent(event) &&
          ddmx11->GetScrollClassEventDetail(event) == SCROLL_TYPE_NO_SCROLL &&
          device->event == next_device->event &&
          device->child == next_device->child &&
          device->detail == next_device->detail &&
          device->button_mask == next_device->button_mask &&
          device->mods.base == next_device->mods.base &&
          device->mods.latched == next_device->mods.latched &&
          device->mods.locked == next_device->mods.locked &&
          device->mods.effective == next_device->mods.effective) {
        *last_event = std::move(event);
        num_coalesced++;
        continue;
      }
    }
    break;
  }

  return num_coalesced;
}

void SetUseOSWindowFrame(x11::Window window, bool use_os_window_frame) {
  // This data structure represents additional hints that we send to the window
  // manager and has a direct lineage back to Motif, which defined this de facto
  // standard. We define this struct to match the wire-format (32-bit fields)
  // rather than the Xlib API (XChangeProperty) format (long fields).
  typedef struct {
    uint32_t flags;
    uint32_t functions;
    uint32_t decorations;
    int32_t input_mode;
    uint32_t status;
  } MotifWmHints;

  MotifWmHints motif_hints;
  memset(&motif_hints, 0, sizeof(motif_hints));
  // Signals that the reader of the _MOTIF_WM_HINTS property should pay
  // attention to the value of |decorations|.
  motif_hints.flags = (1u << 1);
  motif_hints.decorations = use_os_window_frame ? 1 : 0;

  std::vector<uint32_t> hints(sizeof(MotifWmHints) / sizeof(uint32_t));
  memcpy(hints.data(), &motif_hints, sizeof(MotifWmHints));
  x11::Atom hint_atom = x11::GetAtom("_MOTIF_WM_HINTS");
  x11::Connection::Get()->SetArrayProperty(window, hint_atom, hint_atom, hints);
}

bool IsShapeExtensionAvailable() {
  return x11::Connection::Get()->shape().present();
}

x11::Window GetX11RootWindow() {
  return x11::Connection::Get()->default_screen().root;
}

bool GetCurrentDesktop(int32_t* desktop) {
  return x11::Connection::Get()->GetPropertyAs(
      GetX11RootWindow(), x11::GetAtom("_NET_CURRENT_DESKTOP"), desktop);
}

void SetHideTitlebarWhenMaximizedProperty(x11::Window window,
                                          HideTitlebarWhenMaximized property) {
  x11::Connection::Get()->SetProperty(
      window, x11::GetAtom("_GTK_HIDE_TITLEBAR_WHEN_MAXIMIZED"),
      x11::Atom::CARDINAL, static_cast<uint32_t>(property));
}

bool GetRawBytesOfProperty(x11::Window window,
                           x11::Atom property,
                           scoped_refptr<base::RefCountedMemory>* out_data,
                           x11::Atom* out_type) {
  auto future = x11::Connection::Get()->GetProperty({
      .window = window,
      .property = property,
      // Don't limit the amount of returned data.
      .long_length = std::numeric_limits<uint32_t>::max(),
  });
  auto response = future.Sync();
  if (!response || !response->format) {
    return false;
  }
  // SAFETY: The GetProperty response has a `format` which specified the number
  // of bits per object in the `value` and `value_len` for the number of
  // objects, so `value_len * format / 8` gives the number of bytes in `value`.
  *out_data = UNSAFE_BUFFERS(x11::SizedRefCountedMemory::From(
      response->value, response->value_len * response->format / 8u));
  if (out_type) {
    *out_type = response->type;
  }
  return true;
}

void SetWindowClassHint(x11::Connection* connection,
                        x11::Window window,
                        const std::string& res_name,
                        const std::string& res_class) {
  auto str =
      base::StringPrintf("%s%c%s", res_name.c_str(), '\0', res_class.c_str());
  std::vector<char> data(str.data(), str.data() + str.size() + 1);
  x11::Connection::Get()->SetArrayProperty(window, x11::Atom::WM_CLASS,
                                           x11::Atom::STRING, data);
}

void SetWindowRole(x11::Window window, const std::string& role) {
  x11::Atom prop = x11::GetAtom("WM_WINDOW_ROLE");
  if (role.empty()) {
    x11::Connection::Get()->DeleteProperty(window, prop);
  } else {
    x11::Connection::Get()->SetStringProperty(window, prop, x11::Atom::STRING,
                                              role);
  }
}

void SetWMSpecState(x11::Window window,
                    bool enabled,
                    x11::Atom state1,
                    x11::Atom state2) {
  SendClientMessage(
      window, GetX11RootWindow(), x11::GetAtom("_NET_WM_STATE"),
      {static_cast<uint32_t>(enabled ? kNetWMStateAdd : kNetWMStateRemove),
       static_cast<uint32_t>(state1), static_cast<uint32_t>(state2), 1, 0});
}

void DoWMMoveResize(x11::Connection* connection,
                    x11::Window root_window,
                    x11::Window window,
                    const gfx::Point& location_px,
                    int direction) {
  // This handler is usually sent when the window has the implicit grab.  We
  // need to dump it because what we're about to do is tell the window manager
  // that it's now responsible for moving the window around; it immediately
  // grabs when it receives the event below.
  connection->UngrabPointer({x11::Time::CurrentTime});

  SendClientMessage(window, root_window, x11::GetAtom("_NET_WM_MOVERESIZE"),
                    {static_cast<uint32_t>(location_px.x()),
                     static_cast<uint32_t>(location_px.y()),
                     static_cast<uint32_t>(direction), 0, 0});
}

bool HasWMSpecProperty(const base::flat_set<x11::Atom>& properties,
                       x11::Atom atom) {
  return properties.find(atom) != properties.end();
}

bool GetCustomFramePrefDefault() {
  // _NET_WM_MOVERESIZE is needed for frame-drag-initiated window movement.
  if (!x11::Connection::Get()->WmSupportsHint(
          x11::GetAtom("_NET_WM_MOVERESIZE"))) {
    return false;
  }

  ui::WindowManagerName wm = GuessWindowManager();
  // If we don't know which WM is active, conservatively disable custom frames.
  if (wm == WM_OTHER || wm == WM_UNNAMED) {
    return false;
  }

  // Stacking WMs should use custom frames.
  return !IsWmTiling(wm);
}

bool IsWmTiling(WindowManagerName window_manager) {
  switch (window_manager) {
    case WM_BLACKBOX:
    case WM_COMPIZ:
    case WM_ENLIGHTENMENT:
    case WM_FLUXBOX:
    case WM_ICE_WM:
    case WM_KWIN:
    case WM_MATCHBOX:
    case WM_METACITY:
    case WM_MUFFIN:
    case WM_MUTTER:
    case WM_OPENBOX:
    case WM_XFWM4:
      // Stacking window managers.
      return false;

    case WM_I3:
    case WM_ION3:
    case WM_NOTION:
    case WM_RATPOISON:
    case WM_STUMPWM:
      // Tiling window managers.
      return true;

    case WM_AWESOME:
    case WM_QTILE:
    case WM_XMONAD:
    case WM_WMII:
      // Dynamic (tiling and stacking) window managers.  Assume tiling.
      return true;

    case WM_OTHER:
    case WM_UNNAMED:
      // Unknown.  Assume stacking.
      return false;
  }
}

bool GetWindowDesktop(x11::Window window, int32_t* desktop) {
  return x11::Connection::Get()->GetPropertyAs(
      window, x11::GetAtom("_NET_WM_DESKTOP"), desktop);
}

WindowManagerName GuessWindowManager() {
  std::string name = x11::Connection::Get()->GetWmName();
  if (name.empty()) {
    return WM_UNNAMED;
  }
  // These names are taken from the WMs' source code.
  if (name == "awesome") {
    return WM_AWESOME;
  }
  if (name == "Blackbox") {
    return WM_BLACKBOX;
  }
  if (name == "Compiz" || name == "compiz") {
    return WM_COMPIZ;
  }
  if (name == "e16" || name == "Enlightenment") {
    return WM_ENLIGHTENMENT;
  }
  if (name == "Fluxbox") {
    return WM_FLUXBOX;
  }
  if (name == "i3") {
    return WM_I3;
  }
  if (base::StartsWith(name, "IceWM", base::CompareCase::SENSITIVE)) {
    return WM_ICE_WM;
  }
  if (name == "ion3") {
    return WM_ION3;
  }
  if (name == "KWin") {
    return WM_KWIN;
  }
  if (name == "matchbox") {
    return WM_MATCHBOX;
  }
  if (name == "Metacity") {
    return WM_METACITY;
  }
  if (name == "Mutter (Muffin)") {
    return WM_MUFFIN;
  }
  if (name == "GNOME Shell") {
    return WM_MUTTER;  // GNOME Shell uses Mutter
  }
  if (name == "Mutter") {
    return WM_MUTTER;
  }
  if (name == "notion") {
    return WM_NOTION;
  }
  if (name == "Openbox") {
    return WM_OPENBOX;
  }
  if (name == "qtile") {
    return WM_QTILE;
  }
  if (name == "ratpoison") {
    return WM_RATPOISON;
  }
  if (name == "stumpwm") {
    return WM_STUMPWM;
  }
  if (name == "wmii") {
    return WM_WMII;
  }
  if (name == "Xfwm4") {
    return WM_XFWM4;
  }
  if (name == "xmonad") {
    return WM_XMONAD;
  }
  return WM_OTHER;
}

std::string GuessWindowManagerName() {
  std::string name = x11::Connection::Get()->GetWmName();
  return name.empty() ? "Unknown" : name;
}

UMALinuxWindowManager GetWindowManagerUMA() {
  switch (GuessWindowManager()) {
    case WM_OTHER:
      return UMALinuxWindowManager::kOther;
    case WM_UNNAMED:
      return UMALinuxWindowManager::kUnnamed;
    case WM_AWESOME:
      return UMALinuxWindowManager::kAwesome;
    case WM_BLACKBOX:
      return UMALinuxWindowManager::kBlackbox;
    case WM_COMPIZ:
      return UMALinuxWindowManager::kCompiz;
    case WM_ENLIGHTENMENT:
      return UMALinuxWindowManager::kEnlightenment;
    case WM_FLUXBOX:
      return UMALinuxWindowManager::kFluxbox;
    case WM_I3:
      return UMALinuxWindowManager::kI3;
    case WM_ICE_WM:
      return UMALinuxWindowManager::kIceWM;
    case WM_ION3:
      return UMALinuxWindowManager::kIon3;
    case WM_KWIN:
      return UMALinuxWindowManager::kKWin;
    case WM_MATCHBOX:
      return UMALinuxWindowManager::kMatchbox;
    case WM_METACITY:
      return UMALinuxWindowManager::kMetacity;
    case WM_MUFFIN:
      return UMALinuxWindowManager::kMuffin;
    case WM_MUTTER:
      return UMALinuxWindowManager::kMutter;
    case WM_NOTION:
      return UMALinuxWindowManager::kNotion;
    case WM_OPENBOX:
      return UMALinuxWindowManager::kOpenbox;
    case WM_QTILE:
      return UMALinuxWindowManager::kQtile;
    case WM_RATPOISON:
      return UMALinuxWindowManager::kRatpoison;
    case WM_STUMPWM:
      return UMALinuxWindowManager::kStumpWM;
    case WM_WMII:
      return UMALinuxWindowManager::kWmii;
    case WM_XFWM4:
      return UMALinuxWindowManager::kXfwm4;
    case WM_XMONAD:
      return UMALinuxWindowManager::kXmonad;
  }
  NOTREACHED();
}

bool IsX11WindowFullScreen(x11::Window window) {
  // If _NET_WM_STATE_FULLSCREEN is in _NET_SUPPORTED, use the presence or
  // absence of _NET_WM_STATE_FULLSCREEN in _NET_WM_STATE to determine
  // whether we're fullscreen.
  x11::Atom fullscreen_atom = x11::GetAtom("_NET_WM_STATE_FULLSCREEN");
  if (x11::Connection::Get()->WmSupportsHint(fullscreen_atom)) {
    std::vector<x11::Atom> atom_properties;
    if (x11::Connection::Get()->GetArrayProperty(
            window, x11::GetAtom("_NET_WM_STATE"), &atom_properties)) {
      return base::Contains(atom_properties, fullscreen_atom);
    }
  }

  auto* connection = x11::Connection::Get();
  gfx::Rect window_rect;
  if (auto geometry = connection->GetGeometry(window).Sync()) {
    window_rect =
        gfx::Rect(geometry->x, geometry->y, geometry->width, geometry->height);
  } else {
    return false;
  }

  // TODO(thomasanderson): We should use
  // display::Screen::GetDisplayNearestWindow() instead of using the
  // connection screen size, which encompasses all displays.
  int width = connection->default_screen().width_in_pixels;
  int height = connection->default_screen().height_in_pixels;
  return window_rect.size() == gfx::Size(width, height);
}

bool SuspendX11ScreenSaver(bool suspend) {
  static const bool kScreenSaverAvailable = IsX11ScreenSaverAvailable();
  if (!kScreenSaverAvailable) {
    return false;
  }

  x11::Connection::Get()->screensaver().Suspend({suspend});
  return true;
}

SkColorType ColorTypeForVisual(x11::VisualId visual) {
  struct {
    SkColorType color_type;
    unsigned long red_mask;
    unsigned long green_mask;
    unsigned long blue_mask;
    int bpp;
  } color_infos[] = {
      {kRGB_565_SkColorType, 0xf800, 0x7e0, 0x1f, 16},
      {kARGB_4444_SkColorType, 0xf000, 0xf00, 0xf0, 16},
      {kRGBA_8888_SkColorType, 0xff, 0xff00, 0xff0000, 32},
      {kBGRA_8888_SkColorType, 0xff0000, 0xff00, 0xff, 32},
      {kRGBA_1010102_SkColorType, 0x3ff, 0xffc00, 0x3ff00000, 32},
      {kBGRA_1010102_SkColorType, 0x3ff00000, 0xffc00, 0x3ff, 32},
  };
  auto* connection = x11::Connection::Get();
  const auto* vis = connection->GetVisualInfoFromId(visual);
  if (!vis) {
    return kUnknown_SkColorType;
  }
  // We don't currently support anything other than TrueColor and DirectColor.
  if (!vis->visual_type->red_mask || !vis->visual_type->green_mask ||
      !vis->visual_type->blue_mask) {
    return kUnknown_SkColorType;
  }
  for (const auto& color_info : color_infos) {
    if (vis->visual_type->red_mask == color_info.red_mask &&
        vis->visual_type->green_mask == color_info.green_mask &&
        vis->visual_type->blue_mask == color_info.blue_mask &&
        vis->format->bits_per_pixel == color_info.bpp) {
      return color_info.color_type;
    }
  }
  LOG(ERROR) << "Unsupported visual with rgb mask 0x" << std::hex
             << vis->visual_type->red_mask << ", 0x"
             << vis->visual_type->green_mask << ", 0x"
             << vis->visual_type->blue_mask
             << ".  Please report this to https://crbug.com/1025266";
  return kUnknown_SkColorType;
}

x11::Future<void> SendClientMessage(x11::Window window,
                                    x11::Window target,
                                    x11::Atom type,
                                    const std::array<uint32_t, 5> data,
                                    x11::EventMask event_mask) {
  x11::ClientMessageEvent event{.format = 32, .window = window, .type = type};
  event.data.data32 = data;
  return x11::Connection::Get()->SendEvent(event, target, event_mask);
}

bool IsVulkanSurfaceSupported() {
  static const char* extensions[] = {
      "DRI3",         // open source driver.
      "ATIFGLRXDRI",  // AMD proprietary driver.
      "NV-CONTROL",   // NVidia proprietary driver.
  };
  auto* connection = x11::Connection::Get();
  for (const auto* extension : extensions) {
    if (connection->QueryExtension(extension).Sync()) {
      return true;
    }
  }
  return false;
}

gfx::ImageSkia GetNativeWindowIcon(intptr_t target_window_id) {
  std::vector<uint32_t> data;
  if (!x11::Connection::Get()->GetArrayProperty(
          static_cast<x11::Window>(target_window_id),
          x11::GetAtom("_NET_WM_ICON"), &data)) {
    return gfx::ImageSkia();
  }

  // The format of |data| is concatenation of sections like
  // [width, height, pixel data of size width * height], and the total bytes
  // number of |data| is |size|. And here we are picking the largest icon.
  int width = 0;
  int height = 0;
  int start = 0;
  size_t i = 0;
  while (i + 1 < data.size()) {
    if ((static_cast<int>(data[i] * data[i + 1]) > width * height) &&
        (i + 1 + data[i] * data[i + 1] < data.size())) {
      width = static_cast<int>(data[i]);
      height = static_cast<int>(data[i + 1]);
      start = i + 2;
    }
    i += 2 + static_cast<int>(data[i] * data[i + 1]);
  }

  if (width == 0 || height == 0) {
    return gfx::ImageSkia();
  }

  SkBitmap result;
  SkImageInfo info = SkImageInfo::MakeN32(width, height, kUnpremul_SkAlphaType);
  result.allocPixels(info);

  uint32_t* pixels_data = reinterpret_cast<uint32_t*>(result.getPixels());

  for (long y = 0; y < height; ++y) {
    for (long x = 0; x < width; ++x) {
      pixels_data[result.rowBytesAsPixels() * y + x] =
          static_cast<uint32_t>(data[start + width * y + x]);
    }
  }

  return gfx::ImageSkia::CreateFrom1xBitmap(result);
}

}  // namespace ui
