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

#ifndef UI_GFX_X_GENERATED_PROTOS_XF86DRI_H_
#define UI_GFX_X_GENERATED_PROTOS_XF86DRI_H_

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

class COMPONENT_EXPORT(X11) XF86Dri {
 public:
  static constexpr unsigned major_version = 4;
  static constexpr unsigned minor_version = 1;

  XF86Dri(Connection* connection, const x11::QueryExtensionReply& info);

  uint8_t present() const { return info_.present; }
  uint8_t major_opcode() const { return info_.major_opcode; }
  uint8_t first_event() const { return info_.first_event; }
  uint8_t first_error() const { return info_.first_error; }

  Connection* connection() const { return connection_; }

  struct DrmClipRect {
    bool operator==(const DrmClipRect& other) const {
      return x1 == other.x1 && y1 == other.y1 && x2 == other.x2 &&
             x3 == other.x3;
    }

    int16_t x1{};
    int16_t y1{};
    int16_t x2{};
    int16_t x3{};
  };

  struct QueryVersionRequest {};

  struct QueryVersionReply {
    uint16_t sequence{};
    uint16_t dri_major_version{};
    uint16_t dri_minor_version{};
    uint32_t dri_minor_patch{};
  };

  using QueryVersionResponse = Response<QueryVersionReply>;

  Future<QueryVersionReply> QueryVersion(const QueryVersionRequest& request);

  Future<QueryVersionReply> QueryVersion();

  struct QueryDirectRenderingCapableRequest {
    uint32_t screen{};
  };

  struct QueryDirectRenderingCapableReply {
    uint16_t sequence{};
    uint8_t is_capable{};
  };

  using QueryDirectRenderingCapableResponse =
      Response<QueryDirectRenderingCapableReply>;

  Future<QueryDirectRenderingCapableReply> QueryDirectRenderingCapable(
      const QueryDirectRenderingCapableRequest& request);

  Future<QueryDirectRenderingCapableReply> QueryDirectRenderingCapable(
      const uint32_t& screen = {});

  struct OpenConnectionRequest {
    uint32_t screen{};
  };

  struct OpenConnectionReply {
    uint16_t sequence{};
    uint32_t sarea_handle_low{};
    uint32_t sarea_handle_high{};
    std::string bus_id{};
  };

  using OpenConnectionResponse = Response<OpenConnectionReply>;

  Future<OpenConnectionReply> OpenConnection(
      const OpenConnectionRequest& request);

  Future<OpenConnectionReply> OpenConnection(const uint32_t& screen = {});

  struct CloseConnectionRequest {
    uint32_t screen{};
  };

  using CloseConnectionResponse = Response<void>;

  Future<void> CloseConnection(const CloseConnectionRequest& request);

  Future<void> CloseConnection(const uint32_t& screen = {});

  struct GetClientDriverNameRequest {
    uint32_t screen{};
  };

  struct GetClientDriverNameReply {
    uint16_t sequence{};
    uint32_t client_driver_major_version{};
    uint32_t client_driver_minor_version{};
    uint32_t client_driver_patch_version{};
    std::string client_driver_name{};
  };

  using GetClientDriverNameResponse = Response<GetClientDriverNameReply>;

  Future<GetClientDriverNameReply> GetClientDriverName(
      const GetClientDriverNameRequest& request);

  Future<GetClientDriverNameReply> GetClientDriverName(
      const uint32_t& screen = {});

  struct CreateContextRequest {
    uint32_t screen{};
    uint32_t visual{};
    uint32_t context{};
  };

  struct CreateContextReply {
    uint16_t sequence{};
    uint32_t hw_context{};
  };

  using CreateContextResponse = Response<CreateContextReply>;

  Future<CreateContextReply> CreateContext(const CreateContextRequest& request);

  Future<CreateContextReply> CreateContext(const uint32_t& screen = {},
                                           const uint32_t& visual = {},
                                           const uint32_t& context = {});

  struct DestroyContextRequest {
    uint32_t screen{};
    uint32_t context{};
  };

  using DestroyContextResponse = Response<void>;

  Future<void> DestroyContext(const DestroyContextRequest& request);

  Future<void> DestroyContext(const uint32_t& screen = {},
                              const uint32_t& context = {});

  struct CreateDrawableRequest {
    uint32_t screen{};
    uint32_t drawable{};
  };

  struct CreateDrawableReply {
    uint16_t sequence{};
    uint32_t hw_drawable_handle{};
  };

  using CreateDrawableResponse = Response<CreateDrawableReply>;

  Future<CreateDrawableReply> CreateDrawable(
      const CreateDrawableRequest& request);

  Future<CreateDrawableReply> CreateDrawable(const uint32_t& screen = {},
                                             const uint32_t& drawable = {});

  struct DestroyDrawableRequest {
    uint32_t screen{};
    uint32_t drawable{};
  };

  using DestroyDrawableResponse = Response<void>;

  Future<void> DestroyDrawable(const DestroyDrawableRequest& request);

  Future<void> DestroyDrawable(const uint32_t& screen = {},
                               const uint32_t& drawable = {});

  struct GetDrawableInfoRequest {
    uint32_t screen{};
    uint32_t drawable{};
  };

  struct GetDrawableInfoReply {
    uint16_t sequence{};
    uint32_t drawable_table_index{};
    uint32_t drawable_table_stamp{};
    int16_t drawable_origin_X{};
    int16_t drawable_origin_Y{};
    int16_t drawable_size_W{};
    int16_t drawable_size_H{};
    int16_t back_x{};
    int16_t back_y{};
    std::vector<DrmClipRect> clip_rects{};
    std::vector<DrmClipRect> back_clip_rects{};
  };

  using GetDrawableInfoResponse = Response<GetDrawableInfoReply>;

  Future<GetDrawableInfoReply> GetDrawableInfo(
      const GetDrawableInfoRequest& request);

  Future<GetDrawableInfoReply> GetDrawableInfo(const uint32_t& screen = {},
                                               const uint32_t& drawable = {});

  struct GetDeviceInfoRequest {
    uint32_t screen{};
  };

  struct GetDeviceInfoReply {
    uint16_t sequence{};
    uint32_t framebuffer_handle_low{};
    uint32_t framebuffer_handle_high{};
    uint32_t framebuffer_origin_offset{};
    uint32_t framebuffer_size{};
    uint32_t framebuffer_stride{};
    std::vector<uint32_t> device_private{};
  };

  using GetDeviceInfoResponse = Response<GetDeviceInfoReply>;

  Future<GetDeviceInfoReply> GetDeviceInfo(const GetDeviceInfoRequest& request);

  Future<GetDeviceInfoReply> GetDeviceInfo(const uint32_t& screen = {});

  struct AuthConnectionRequest {
    uint32_t screen{};
    uint32_t magic{};
  };

  struct AuthConnectionReply {
    uint16_t sequence{};
    uint32_t authenticated{};
  };

  using AuthConnectionResponse = Response<AuthConnectionReply>;

  Future<AuthConnectionReply> AuthConnection(
      const AuthConnectionRequest& request);

  Future<AuthConnectionReply> AuthConnection(const uint32_t& screen = {},
                                             const uint32_t& magic = {});

 private:
  Connection* const connection_;
  x11::QueryExtensionReply info_{};
};

}  // namespace x11

#endif  // UI_GFX_X_GENERATED_PROTOS_XF86DRI_H_
