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

#ifndef UI_GFX_X_GENERATED_PROTOS_COMPOSITE_H_
#define UI_GFX_X_GENERATED_PROTOS_COMPOSITE_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
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

class COMPONENT_EXPORT(X11) Composite {
 public:
  static constexpr unsigned major_version = 0;
  static constexpr unsigned minor_version = 4;

  Composite(Connection* connection, const x11::QueryExtensionReply& info);

  uint8_t present() const { return info_.present; }
  uint8_t major_opcode() const { return info_.major_opcode; }
  uint8_t first_event() const { return info_.first_event; }
  uint8_t first_error() const { return info_.first_error; }

  Connection* connection() const { return connection_; }

  enum class Redirect : int {
    Automatic = 0,
    Manual = 1,
  };

  struct QueryVersionRequest {
    uint32_t client_major_version{};
    uint32_t client_minor_version{};
  };

  struct QueryVersionReply {
    uint16_t sequence{};
    uint32_t major_version{};
    uint32_t minor_version{};
  };

  using QueryVersionResponse = Response<QueryVersionReply>;

  Future<QueryVersionReply> QueryVersion(const QueryVersionRequest& request);

  Future<QueryVersionReply> QueryVersion(
      const uint32_t& client_major_version = {},
      const uint32_t& client_minor_version = {});

  struct RedirectWindowRequest {
    Window window{};
    Redirect update{};
  };

  using RedirectWindowResponse = Response<void>;

  Future<void> RedirectWindow(const RedirectWindowRequest& request);

  Future<void> RedirectWindow(const Window& window = {},
                              const Redirect& update = {});

  struct RedirectSubwindowsRequest {
    Window window{};
    Redirect update{};
  };

  using RedirectSubwindowsResponse = Response<void>;

  Future<void> RedirectSubwindows(const RedirectSubwindowsRequest& request);

  Future<void> RedirectSubwindows(const Window& window = {},
                                  const Redirect& update = {});

  struct UnredirectWindowRequest {
    Window window{};
    Redirect update{};
  };

  using UnredirectWindowResponse = Response<void>;

  Future<void> UnredirectWindow(const UnredirectWindowRequest& request);

  Future<void> UnredirectWindow(const Window& window = {},
                                const Redirect& update = {});

  struct UnredirectSubwindowsRequest {
    Window window{};
    Redirect update{};
  };

  using UnredirectSubwindowsResponse = Response<void>;

  Future<void> UnredirectSubwindows(const UnredirectSubwindowsRequest& request);

  Future<void> UnredirectSubwindows(const Window& window = {},
                                    const Redirect& update = {});

  struct CreateRegionFromBorderClipRequest {
    XFixes::Region region{};
    Window window{};
  };

  using CreateRegionFromBorderClipResponse = Response<void>;

  Future<void> CreateRegionFromBorderClip(
      const CreateRegionFromBorderClipRequest& request);

  Future<void> CreateRegionFromBorderClip(const XFixes::Region& region = {},
                                          const Window& window = {});

  struct NameWindowPixmapRequest {
    Window window{};
    Pixmap pixmap{};
  };

  using NameWindowPixmapResponse = Response<void>;

  Future<void> NameWindowPixmap(const NameWindowPixmapRequest& request);

  Future<void> NameWindowPixmap(const Window& window = {},
                                const Pixmap& pixmap = {});

  struct GetOverlayWindowRequest {
    Window window{};
  };

  struct GetOverlayWindowReply {
    uint16_t sequence{};
    Window overlay_win{};
  };

  using GetOverlayWindowResponse = Response<GetOverlayWindowReply>;

  Future<GetOverlayWindowReply> GetOverlayWindow(
      const GetOverlayWindowRequest& request);

  Future<GetOverlayWindowReply> GetOverlayWindow(const Window& window = {});

  struct ReleaseOverlayWindowRequest {
    Window window{};
  };

  using ReleaseOverlayWindowResponse = Response<void>;

  Future<void> ReleaseOverlayWindow(const ReleaseOverlayWindowRequest& request);

  Future<void> ReleaseOverlayWindow(const Window& window = {});

 private:
  Connection* const connection_;
  x11::QueryExtensionReply info_{};
};

}  // namespace x11

inline constexpr x11::Composite::Redirect operator|(
    x11::Composite::Redirect l,
    x11::Composite::Redirect r) {
  using T = std::underlying_type_t<x11::Composite::Redirect>;
  return static_cast<x11::Composite::Redirect>(static_cast<T>(l) |
                                               static_cast<T>(r));
}

inline constexpr x11::Composite::Redirect operator&(
    x11::Composite::Redirect l,
    x11::Composite::Redirect r) {
  using T = std::underlying_type_t<x11::Composite::Redirect>;
  return static_cast<x11::Composite::Redirect>(static_cast<T>(l) &
                                               static_cast<T>(r));
}

#endif  // UI_GFX_X_GENERATED_PROTOS_COMPOSITE_H_
