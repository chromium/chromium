// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file was automatically generated with:
// ../../ui/gfx/x/gen_xproto.py \
//    ../../third_party/xcbproto/src \
//    gen/ui/gfx/x \
//    bigreq \
//    dri3 \
//    glx \
//    randr \
//    render \
//    screensaver \
//    shape \
//    shm \
//    sync \
//    xfixes \
//    xinput \
//    xkb \
//    xproto \
//    xtest

#ifndef UI_GFX_X_GENERATED_PROTOS_SCREENSAVER_H_
#define UI_GFX_X_GENERATED_PROTOS_SCREENSAVER_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <vector>

#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "base/memory/scoped_refptr.h"
#include "ui/gfx/x/error.h"
#include "ui/gfx/x/ref_counted_fd.h"
#include "ui/gfx/x/xproto_types.h"
#include "xproto.h"

namespace x11 {

class Connection;

template <typename Reply>
struct Response;

template <typename Reply>
class Future;

class COMPONENT_EXPORT(X11) ScreenSaver {
 public:
  static constexpr unsigned major_version = 1;
  static constexpr unsigned minor_version = 1;

  ScreenSaver(Connection* connection, const x11::QueryExtensionReply& info);

  uint8_t present() const { return info_.present; }
  uint8_t major_opcode() const { return info_.major_opcode; }
  uint8_t first_event() const { return info_.first_event; }
  uint8_t first_error() const { return info_.first_error; }

  Connection* connection() const { return connection_; }

  enum class Kind : int {
    Blanked = 0,
    Internal = 1,
    External = 2,
  };

  enum class Event : int {
    NotifyMask = 1 << 0,
    CycleMask = 1 << 1,
  };

  enum class State : int {
    Off = 0,
    On = 1,
    Cycle = 2,
    Disabled = 3,
  };

  struct NotifyEvent {
    static constexpr uint8_t type_id = 5;
    static constexpr uint8_t opcode = 0;
    State state{};
    uint16_t sequence{};
    Time time{};
    Window root{};
    Window window{};
    Kind kind{};
    uint8_t forced{};
  };

  struct QueryVersionRequest {
    uint8_t client_major_version{};
    uint8_t client_minor_version{};
  };

  struct QueryVersionReply {
    uint16_t sequence{};
    uint16_t server_major_version{};
    uint16_t server_minor_version{};
  };

  using QueryVersionResponse = Response<QueryVersionReply>;

  Future<QueryVersionReply> QueryVersion(const QueryVersionRequest& request);

  Future<QueryVersionReply> QueryVersion(
      const uint8_t& client_major_version = {},
      const uint8_t& client_minor_version = {});

  struct QueryInfoRequest {
    Drawable drawable{};
  };

  struct QueryInfoReply {
    uint8_t state{};
    uint16_t sequence{};
    Window saver_window{};
    uint32_t ms_until_server{};
    uint32_t ms_since_user_input{};
    uint32_t event_mask{};
    Kind kind{};
  };

  using QueryInfoResponse = Response<QueryInfoReply>;

  Future<QueryInfoReply> QueryInfo(const QueryInfoRequest& request);

  Future<QueryInfoReply> QueryInfo(const Drawable& drawable = {});

  struct SelectInputRequest {
    Drawable drawable{};
    Event event_mask{};
  };

  using SelectInputResponse = Response<void>;

  Future<void> SelectInput(const SelectInputRequest& request);

  Future<void> SelectInput(const Drawable& drawable = {},
                           const Event& event_mask = {});

  struct SetAttributesRequest {
    Drawable drawable{};
    int16_t x{};
    int16_t y{};
    uint16_t width{};
    uint16_t height{};
    uint16_t border_width{};
    WindowClass c_class{};
    uint8_t depth{};
    VisualId visual{};
    std::optional<Pixmap> background_pixmap{};
    std::optional<uint32_t> background_pixel{};
    std::optional<Pixmap> border_pixmap{};
    std::optional<uint32_t> border_pixel{};
    std::optional<Gravity> bit_gravity{};
    std::optional<Gravity> win_gravity{};
    std::optional<BackingStore> backing_store{};
    std::optional<uint32_t> backing_planes{};
    std::optional<uint32_t> backing_pixel{};
    std::optional<Bool32> override_redirect{};
    std::optional<Bool32> save_under{};
    std::optional<EventMask> event_mask{};
    std::optional<EventMask> do_not_propogate_mask{};
    std::optional<ColorMap> colormap{};
    std::optional<Cursor> cursor{};
  };

  using SetAttributesResponse = Response<void>;

  Future<void> SetAttributes(const SetAttributesRequest& request);

  Future<void> SetAttributes(
      const Drawable& drawable = {},
      const int16_t& x = {},
      const int16_t& y = {},
      const uint16_t& width = {},
      const uint16_t& height = {},
      const uint16_t& border_width = {},
      const WindowClass& c_class = {},
      const uint8_t& depth = {},
      const VisualId& visual = {},
      const std::optional<Pixmap>& background_pixmap = std::nullopt,
      const std::optional<uint32_t>& background_pixel = std::nullopt,
      const std::optional<Pixmap>& border_pixmap = std::nullopt,
      const std::optional<uint32_t>& border_pixel = std::nullopt,
      const std::optional<Gravity>& bit_gravity = std::nullopt,
      const std::optional<Gravity>& win_gravity = std::nullopt,
      const std::optional<BackingStore>& backing_store = std::nullopt,
      const std::optional<uint32_t>& backing_planes = std::nullopt,
      const std::optional<uint32_t>& backing_pixel = std::nullopt,
      const std::optional<Bool32>& override_redirect = std::nullopt,
      const std::optional<Bool32>& save_under = std::nullopt,
      const std::optional<EventMask>& event_mask = std::nullopt,
      const std::optional<EventMask>& do_not_propogate_mask = std::nullopt,
      const std::optional<ColorMap>& colormap = std::nullopt,
      const std::optional<Cursor>& cursor = std::nullopt);

  struct UnsetAttributesRequest {
    Drawable drawable{};
  };

  using UnsetAttributesResponse = Response<void>;

  Future<void> UnsetAttributes(const UnsetAttributesRequest& request);

  Future<void> UnsetAttributes(const Drawable& drawable = {});

  struct SuspendRequest {
    uint32_t suspend{};
  };

  using SuspendResponse = Response<void>;

  Future<void> Suspend(const SuspendRequest& request);

  Future<void> Suspend(const uint32_t& suspend = {});

 private:
  Connection* const connection_;
  x11::QueryExtensionReply info_{};
};

}  // namespace x11

inline constexpr x11::ScreenSaver::Kind operator|(x11::ScreenSaver::Kind l,
                                                  x11::ScreenSaver::Kind r) {
  using T = std::underlying_type_t<x11::ScreenSaver::Kind>;
  return static_cast<x11::ScreenSaver::Kind>(static_cast<T>(l) |
                                             static_cast<T>(r));
}

inline constexpr x11::ScreenSaver::Kind operator&(x11::ScreenSaver::Kind l,
                                                  x11::ScreenSaver::Kind r) {
  using T = std::underlying_type_t<x11::ScreenSaver::Kind>;
  return static_cast<x11::ScreenSaver::Kind>(static_cast<T>(l) &
                                             static_cast<T>(r));
}

inline constexpr x11::ScreenSaver::Event operator|(x11::ScreenSaver::Event l,
                                                   x11::ScreenSaver::Event r) {
  using T = std::underlying_type_t<x11::ScreenSaver::Event>;
  return static_cast<x11::ScreenSaver::Event>(static_cast<T>(l) |
                                              static_cast<T>(r));
}

inline constexpr x11::ScreenSaver::Event operator&(x11::ScreenSaver::Event l,
                                                   x11::ScreenSaver::Event r) {
  using T = std::underlying_type_t<x11::ScreenSaver::Event>;
  return static_cast<x11::ScreenSaver::Event>(static_cast<T>(l) &
                                              static_cast<T>(r));
}

inline constexpr x11::ScreenSaver::State operator|(x11::ScreenSaver::State l,
                                                   x11::ScreenSaver::State r) {
  using T = std::underlying_type_t<x11::ScreenSaver::State>;
  return static_cast<x11::ScreenSaver::State>(static_cast<T>(l) |
                                              static_cast<T>(r));
}

inline constexpr x11::ScreenSaver::State operator&(x11::ScreenSaver::State l,
                                                   x11::ScreenSaver::State r) {
  using T = std::underlying_type_t<x11::ScreenSaver::State>;
  return static_cast<x11::ScreenSaver::State>(static_cast<T>(l) &
                                              static_cast<T>(r));
}

#endif  // UI_GFX_X_GENERATED_PROTOS_SCREENSAVER_H_
