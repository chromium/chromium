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

#ifndef UI_GFX_X_GENERATED_PROTOS_XINERAMA_H_
#define UI_GFX_X_GENERATED_PROTOS_XINERAMA_H_

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
#include "xproto.h"

namespace x11 {

class Connection;

template <typename Reply>
struct Response;

template <typename Reply>
class Future;

class COMPONENT_EXPORT(X11) Xinerama {
 public:
  static constexpr unsigned major_version = 1;
  static constexpr unsigned minor_version = 1;

  Xinerama(Connection* connection, const x11::QueryExtensionReply& info);

  uint8_t present() const { return info_.present; }
  uint8_t major_opcode() const { return info_.major_opcode; }
  uint8_t first_event() const { return info_.first_event; }
  uint8_t first_error() const { return info_.first_error; }

  Connection* connection() const { return connection_; }

  struct ScreenInfo {
    bool operator==(const ScreenInfo& other) const {
      return x_org == other.x_org && y_org == other.y_org &&
             width == other.width && height == other.height;
    }

    int16_t x_org{};
    int16_t y_org{};
    uint16_t width{};
    uint16_t height{};
  };

  struct QueryVersionRequest {
    uint8_t major{};
    uint8_t minor{};
  };

  struct QueryVersionReply {
    uint16_t sequence{};
    uint16_t major{};
    uint16_t minor{};
  };

  using QueryVersionResponse = Response<QueryVersionReply>;

  Future<QueryVersionReply> QueryVersion(const QueryVersionRequest& request);

  Future<QueryVersionReply> QueryVersion(const uint8_t& major = {},
                                         const uint8_t& minor = {});

  struct GetStateRequest {
    Window window{};
  };

  struct GetStateReply {
    uint8_t state{};
    uint16_t sequence{};
    Window window{};
  };

  using GetStateResponse = Response<GetStateReply>;

  Future<GetStateReply> GetState(const GetStateRequest& request);

  Future<GetStateReply> GetState(const Window& window = {});

  struct GetScreenCountRequest {
    Window window{};
  };

  struct GetScreenCountReply {
    uint8_t screen_count{};
    uint16_t sequence{};
    Window window{};
  };

  using GetScreenCountResponse = Response<GetScreenCountReply>;

  Future<GetScreenCountReply> GetScreenCount(
      const GetScreenCountRequest& request);

  Future<GetScreenCountReply> GetScreenCount(const Window& window = {});

  struct GetScreenSizeRequest {
    Window window{};
    uint32_t screen{};
  };

  struct GetScreenSizeReply {
    uint16_t sequence{};
    uint32_t width{};
    uint32_t height{};
    Window window{};
    uint32_t screen{};
  };

  using GetScreenSizeResponse = Response<GetScreenSizeReply>;

  Future<GetScreenSizeReply> GetScreenSize(const GetScreenSizeRequest& request);

  Future<GetScreenSizeReply> GetScreenSize(const Window& window = {},
                                           const uint32_t& screen = {});

  struct IsActiveRequest {};

  struct IsActiveReply {
    uint16_t sequence{};
    uint32_t state{};
  };

  using IsActiveResponse = Response<IsActiveReply>;

  Future<IsActiveReply> IsActive(const IsActiveRequest& request);

  Future<IsActiveReply> IsActive();

  struct QueryScreensRequest {};

  struct QueryScreensReply {
    uint16_t sequence{};
    std::vector<ScreenInfo> screen_info{};
  };

  using QueryScreensResponse = Response<QueryScreensReply>;

  Future<QueryScreensReply> QueryScreens(const QueryScreensRequest& request);

  Future<QueryScreensReply> QueryScreens();

 private:
  Connection* const connection_;
  x11::QueryExtensionReply info_{};
};

}  // namespace x11

#endif  // UI_GFX_X_GENERATED_PROTOS_XINERAMA_H_
