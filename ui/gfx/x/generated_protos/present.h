// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file was automatically generated with:
// ../../ui/gfx/x/gen_xproto.py \
//    ../../third_party/xcbproto/src \
//    gen/ui/gfx/x \
//    bigreq \
//    composite \
//    damage \
//    dpms \
//    dri2 \
//    dri3 \
//    ge \
//    glx \
//    present \
//    randr \
//    record \
//    render \
//    res \
//    screensaver \
//    shape \
//    shm \
//    sync \
//    xc_misc \
//    xevie \
//    xf86dri \
//    xf86vidmode \
//    xfixes \
//    xinerama \
//    xinput \
//    xkb \
//    xprint \
//    xproto \
//    xselinux \
//    xtest \
//    xv \
//    xvmc

#ifndef UI_GFX_X_GENERATED_PROTOS_PRESENT_H_
#define UI_GFX_X_GENERATED_PROTOS_PRESENT_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "randr.h"
#include "sync.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/x/error.h"
#include "ui/gfx/x/ref_counted_fd.h"
#include "xfixes.h"
#include "xproto.h"

namespace x11 {

class Connection;

template <typename Reply>
struct Response;

template <typename Reply>
class Future;

class COMPONENT_EXPORT(X11) Present {
 public:
  static constexpr unsigned major_version = 1;
  static constexpr unsigned minor_version = 2;

  Present(Connection* connection, const x11::QueryExtensionReply& info);

  uint8_t present() const { return info_.present; }
  uint8_t major_opcode() const { return info_.major_opcode; }
  uint8_t first_event() const { return info_.first_event; }
  uint8_t first_error() const { return info_.first_error; }

  Connection* connection() const { return connection_; }

  enum class Event : uint32_t {
    ConfigureNotify = 0,
    CompleteNotify = 1,
    IdleNotify = 2,
    RedirectNotify = 3,
  };

  enum class EventMask : int {
    NoEvent = 0,
    ConfigureNotify = 1 << 0,
    CompleteNotify = 1 << 1,
    IdleNotify = 1 << 2,
    RedirectNotify = 1 << 3,
  };

  enum class Option : int {
    None = 0,
    Async = 1 << 0,
    Copy = 1 << 1,
    UST = 1 << 2,
    Suboptimal = 1 << 3,
  };

  enum class Capability : int {
    None = 0,
    Async = 1 << 0,
    Fence = 1 << 1,
    UST = 1 << 2,
  };

  enum class CompleteKind : int {
    Pixmap = 0,
    NotifyMSC = 1,
  };

  enum class CompleteMode : int {
    Copy = 0,
    Flip = 1,
    Skip = 2,
    SuboptimalCopy = 3,
  };

  struct Notify {
    bool operator==(const Notify& other) const {
      return window == other.window && serial == other.serial;
    }

    Window window{};
    uint32_t serial{};
  };

  struct GenericEvent {
    static constexpr int type_id = 6;
    static constexpr uint8_t opcode = 0;
    uint8_t extension{};
    uint16_t sequence{};
    uint32_t length{};
    uint16_t evtype{};
    Event event{};

    x11::Window* GetWindow() { return nullptr; }
  };

  struct ConfigureNotifyEvent {
    static constexpr int type_id = 7;
    static constexpr uint8_t opcode = 0;
    uint16_t sequence{};
    Event event{};
    Window window{};
    int16_t x{};
    int16_t y{};
    uint16_t width{};
    uint16_t height{};
    int16_t off_x{};
    int16_t off_y{};
    uint16_t pixmap_width{};
    uint16_t pixmap_height{};
    uint32_t pixmap_flags{};

    x11::Window* GetWindow() { return reinterpret_cast<x11::Window*>(&window); }
  };

  struct CompleteNotifyEvent {
    static constexpr int type_id = 8;
    static constexpr uint8_t opcode = 1;
    uint16_t sequence{};
    CompleteKind kind{};
    CompleteMode mode{};
    Event event{};
    Window window{};
    uint32_t serial{};
    uint64_t ust{};
    uint64_t msc{};

    x11::Window* GetWindow() { return reinterpret_cast<x11::Window*>(&window); }
  };

  struct IdleNotifyEvent {
    static constexpr int type_id = 9;
    static constexpr uint8_t opcode = 2;
    uint16_t sequence{};
    Event event{};
    Window window{};
    uint32_t serial{};
    Pixmap pixmap{};
    Sync::Fence idle_fence{};

    x11::Window* GetWindow() { return reinterpret_cast<x11::Window*>(&window); }
  };

  struct RedirectNotifyEvent {
    static constexpr int type_id = 10;
    static constexpr uint8_t opcode = 3;
    uint16_t sequence{};
    uint8_t update_window{};
    Event event{};
    Window event_window{};
    Window window{};
    Pixmap pixmap{};
    uint32_t serial{};
    XFixes::Region valid_region{};
    XFixes::Region update_region{};
    Rectangle valid_rect{};
    Rectangle update_rect{};
    int16_t x_off{};
    int16_t y_off{};
    RandR::Crtc target_crtc{};
    Sync::Fence wait_fence{};
    Sync::Fence idle_fence{};
    uint32_t options{};
    uint64_t target_msc{};
    uint64_t divisor{};
    uint64_t remainder{};
    std::vector<Notify> notifies{};

    x11::Window* GetWindow() { return reinterpret_cast<x11::Window*>(&window); }
  };

  struct QueryVersionRequest {
    uint32_t major_version{};
    uint32_t minor_version{};
  };

  struct QueryVersionReply {
    uint16_t sequence{};
    uint32_t major_version{};
    uint32_t minor_version{};
  };

  using QueryVersionResponse = Response<QueryVersionReply>;

  Future<QueryVersionReply> QueryVersion(const QueryVersionRequest& request);

  Future<QueryVersionReply> QueryVersion(const uint32_t& major_version = {},
                                         const uint32_t& minor_version = {});

  struct PresentPixmapRequest {
    Window window{};
    Pixmap pixmap{};
    uint32_t serial{};
    XFixes::Region valid{};
    XFixes::Region update{};
    int16_t x_off{};
    int16_t y_off{};
    RandR::Crtc target_crtc{};
    Sync::Fence wait_fence{};
    Sync::Fence idle_fence{};
    uint32_t options{};
    uint64_t target_msc{};
    uint64_t divisor{};
    uint64_t remainder{};
    std::vector<Notify> notifies{};
  };

  using PresentPixmapResponse = Response<void>;

  Future<void> PresentPixmap(const PresentPixmapRequest& request);

  Future<void> PresentPixmap(const Window& window = {},
                             const Pixmap& pixmap = {},
                             const uint32_t& serial = {},
                             const XFixes::Region& valid = {},
                             const XFixes::Region& update = {},
                             const int16_t& x_off = {},
                             const int16_t& y_off = {},
                             const RandR::Crtc& target_crtc = {},
                             const Sync::Fence& wait_fence = {},
                             const Sync::Fence& idle_fence = {},
                             const uint32_t& options = {},
                             const uint64_t& target_msc = {},
                             const uint64_t& divisor = {},
                             const uint64_t& remainder = {},
                             const std::vector<Notify>& notifies = {});

  struct NotifyMSCRequest {
    Window window{};
    uint32_t serial{};
    uint64_t target_msc{};
    uint64_t divisor{};
    uint64_t remainder{};
  };

  using NotifyMSCResponse = Response<void>;

  Future<void> NotifyMSC(const NotifyMSCRequest& request);

  Future<void> NotifyMSC(const Window& window = {},
                         const uint32_t& serial = {},
                         const uint64_t& target_msc = {},
                         const uint64_t& divisor = {},
                         const uint64_t& remainder = {});

  struct SelectInputRequest {
    Event eid{};
    Window window{};
    EventMask event_mask{};
  };

  using SelectInputResponse = Response<void>;

  Future<void> SelectInput(const SelectInputRequest& request);

  Future<void> SelectInput(const Event& eid = {},
                           const Window& window = {},
                           const EventMask& event_mask = {});

  struct QueryCapabilitiesRequest {
    uint32_t target{};
  };

  struct QueryCapabilitiesReply {
    uint16_t sequence{};
    uint32_t capabilities{};
  };

  using QueryCapabilitiesResponse = Response<QueryCapabilitiesReply>;

  Future<QueryCapabilitiesReply> QueryCapabilities(
      const QueryCapabilitiesRequest& request);

  Future<QueryCapabilitiesReply> QueryCapabilities(const uint32_t& target = {});

 private:
  Connection* const connection_;
  x11::QueryExtensionReply info_{};
};

}  // namespace x11

inline constexpr x11::Present::Event operator|(x11::Present::Event l,
                                               x11::Present::Event r) {
  using T = std::underlying_type_t<x11::Present::Event>;
  return static_cast<x11::Present::Event>(static_cast<T>(l) |
                                          static_cast<T>(r));
}

inline constexpr x11::Present::Event operator&(x11::Present::Event l,
                                               x11::Present::Event r) {
  using T = std::underlying_type_t<x11::Present::Event>;
  return static_cast<x11::Present::Event>(static_cast<T>(l) &
                                          static_cast<T>(r));
}

inline constexpr x11::Present::EventMask operator|(x11::Present::EventMask l,
                                                   x11::Present::EventMask r) {
  using T = std::underlying_type_t<x11::Present::EventMask>;
  return static_cast<x11::Present::EventMask>(static_cast<T>(l) |
                                              static_cast<T>(r));
}

inline constexpr x11::Present::EventMask operator&(x11::Present::EventMask l,
                                                   x11::Present::EventMask r) {
  using T = std::underlying_type_t<x11::Present::EventMask>;
  return static_cast<x11::Present::EventMask>(static_cast<T>(l) &
                                              static_cast<T>(r));
}

inline constexpr x11::Present::Option operator|(x11::Present::Option l,
                                                x11::Present::Option r) {
  using T = std::underlying_type_t<x11::Present::Option>;
  return static_cast<x11::Present::Option>(static_cast<T>(l) |
                                           static_cast<T>(r));
}

inline constexpr x11::Present::Option operator&(x11::Present::Option l,
                                                x11::Present::Option r) {
  using T = std::underlying_type_t<x11::Present::Option>;
  return static_cast<x11::Present::Option>(static_cast<T>(l) &
                                           static_cast<T>(r));
}

inline constexpr x11::Present::Capability operator|(
    x11::Present::Capability l,
    x11::Present::Capability r) {
  using T = std::underlying_type_t<x11::Present::Capability>;
  return static_cast<x11::Present::Capability>(static_cast<T>(l) |
                                               static_cast<T>(r));
}

inline constexpr x11::Present::Capability operator&(
    x11::Present::Capability l,
    x11::Present::Capability r) {
  using T = std::underlying_type_t<x11::Present::Capability>;
  return static_cast<x11::Present::Capability>(static_cast<T>(l) &
                                               static_cast<T>(r));
}

inline constexpr x11::Present::CompleteKind operator|(
    x11::Present::CompleteKind l,
    x11::Present::CompleteKind r) {
  using T = std::underlying_type_t<x11::Present::CompleteKind>;
  return static_cast<x11::Present::CompleteKind>(static_cast<T>(l) |
                                                 static_cast<T>(r));
}

inline constexpr x11::Present::CompleteKind operator&(
    x11::Present::CompleteKind l,
    x11::Present::CompleteKind r) {
  using T = std::underlying_type_t<x11::Present::CompleteKind>;
  return static_cast<x11::Present::CompleteKind>(static_cast<T>(l) &
                                                 static_cast<T>(r));
}

inline constexpr x11::Present::CompleteMode operator|(
    x11::Present::CompleteMode l,
    x11::Present::CompleteMode r) {
  using T = std::underlying_type_t<x11::Present::CompleteMode>;
  return static_cast<x11::Present::CompleteMode>(static_cast<T>(l) |
                                                 static_cast<T>(r));
}

inline constexpr x11::Present::CompleteMode operator&(
    x11::Present::CompleteMode l,
    x11::Present::CompleteMode r) {
  using T = std::underlying_type_t<x11::Present::CompleteMode>;
  return static_cast<x11::Present::CompleteMode>(static_cast<T>(l) &
                                                 static_cast<T>(r));
}

#endif  // UI_GFX_X_GENERATED_PROTOS_PRESENT_H_
