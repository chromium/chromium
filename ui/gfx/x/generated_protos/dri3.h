// Copyright 2021 The Chromium Authors. All rights reserved.
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

#ifndef UI_GFX_X_GENERATED_PROTOS_DRI3_H_
#define UI_GFX_X_GENERATED_PROTOS_DRI3_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "ui/gfx/x/error.h"
#include "xproto.h"

namespace x11 {

class Connection;

template <typename Reply>
struct Response;

template <typename Reply>
class Future;

class COMPONENT_EXPORT(X11) Dri3 {
 public:
  static constexpr unsigned major_version = 1;
  static constexpr unsigned minor_version = 2;

  Dri3(Connection* connection, const x11::QueryExtensionReply& info);

  uint8_t present() const { return info_.present; }
  uint8_t major_opcode() const { return info_.major_opcode; }
  uint8_t first_event() const { return info_.first_event; }
  uint8_t first_error() const { return info_.first_error; }

  Connection* connection() const { return connection_; }

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

  struct OpenRequest {
    Drawable drawable{};
    uint32_t provider{};
  };

  struct OpenReply {
    uint8_t nfd{};
    uint16_t sequence{};
    base::ScopedFD device_fd{};
  };

  using OpenResponse = Response<OpenReply>;

  Future<OpenReply> Open(const OpenRequest& request);

  struct PixmapFromBufferRequest {
    Pixmap pixmap{};
    Drawable drawable{};
    uint32_t size{};
    uint16_t width{};
    uint16_t height{};
    uint16_t stride{};
    uint8_t depth{};
    uint8_t bpp{};
    base::ScopedFD pixmap_fd{};
  };

  using PixmapFromBufferResponse = Response<void>;

  Future<void> PixmapFromBuffer(const PixmapFromBufferRequest& request);

  struct BufferFromPixmapRequest {
    Pixmap pixmap{};
  };

  struct BufferFromPixmapReply {
    uint8_t nfd{};
    uint16_t sequence{};
    uint32_t size{};
    uint16_t width{};
    uint16_t height{};
    uint16_t stride{};
    uint8_t depth{};
    uint8_t bpp{};
    base::ScopedFD pixmap_fd{};
  };

  using BufferFromPixmapResponse = Response<BufferFromPixmapReply>;

  Future<BufferFromPixmapReply> BufferFromPixmap(
      const BufferFromPixmapRequest& request);

  struct FenceFromFDRequest {
    Drawable drawable{};
    uint32_t fence{};
    uint8_t initially_triggered{};
    base::ScopedFD fence_fd{};
  };

  using FenceFromFDResponse = Response<void>;

  Future<void> FenceFromFD(const FenceFromFDRequest& request);

  struct FDFromFenceRequest {
    Drawable drawable{};
    uint32_t fence{};
  };

  struct FDFromFenceReply {
    uint8_t nfd{};
    uint16_t sequence{};
    base::ScopedFD fence_fd{};
  };

  using FDFromFenceResponse = Response<FDFromFenceReply>;

  Future<FDFromFenceReply> FDFromFence(const FDFromFenceRequest& request);

  struct GetSupportedModifiersRequest {
    uint32_t window{};
    uint8_t depth{};
    uint8_t bpp{};
  };

  struct GetSupportedModifiersReply {
    uint16_t sequence{};
    std::vector<uint64_t> window_modifiers{};
    std::vector<uint64_t> screen_modifiers{};
  };

  using GetSupportedModifiersResponse = Response<GetSupportedModifiersReply>;

  Future<GetSupportedModifiersReply> GetSupportedModifiers(
      const GetSupportedModifiersRequest& request);

  struct PixmapFromBuffersRequest {
    Pixmap pixmap{};
    Window window{};
    uint16_t width{};
    uint16_t height{};
    uint32_t stride0{};
    uint32_t offset0{};
    uint32_t stride1{};
    uint32_t offset1{};
    uint32_t stride2{};
    uint32_t offset2{};
    uint32_t stride3{};
    uint32_t offset3{};
    uint8_t depth{};
    uint8_t bpp{};
    uint64_t modifier{};
    std::vector<base::ScopedFD> buffers{};
  };

  using PixmapFromBuffersResponse = Response<void>;

  Future<void> PixmapFromBuffers(const PixmapFromBuffersRequest& request);

  struct BuffersFromPixmapRequest {
    Pixmap pixmap{};
  };

  struct BuffersFromPixmapReply {
    uint16_t sequence{};
    uint16_t width{};
    uint16_t height{};
    uint64_t modifier{};
    uint8_t depth{};
    uint8_t bpp{};
    std::vector<uint32_t> strides{};
    std::vector<uint32_t> offsets{};
    std::vector<base::ScopedFD> buffers{};
  };

  using BuffersFromPixmapResponse = Response<BuffersFromPixmapReply>;

  Future<BuffersFromPixmapReply> BuffersFromPixmap(
      const BuffersFromPixmapRequest& request);

 private:
  Connection* const connection_;
  x11::QueryExtensionReply info_{};
};

}  // namespace x11

#endif  // UI_GFX_X_GENERATED_PROTOS_DRI3_H_
