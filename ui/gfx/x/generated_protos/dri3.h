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

#ifndef UI_GFX_X_GENERATED_PROTOS_DRI3_H_
#define UI_GFX_X_GENERATED_PROTOS_DRI3_H_

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

class COMPONENT_EXPORT(X11) Dri3 {
 public:
  static constexpr unsigned major_version = 1;
  static constexpr unsigned minor_version = 4;

  Dri3(Connection* connection, const x11::QueryExtensionReply& info);

  uint8_t present() const { return info_.present; }
  uint8_t major_opcode() const { return info_.major_opcode; }
  uint8_t first_event() const { return info_.first_event; }
  uint8_t first_error() const { return info_.first_error; }

  Connection* connection() const { return connection_; }

  enum class Syncobj : uint32_t {};

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

  struct OpenRequest {
    Drawable drawable{};
    uint32_t provider{};
  };

  struct OpenReply {
    uint8_t nfd{};
    uint16_t sequence{};
    RefCountedFD device_fd{};
  };

  using OpenResponse = Response<OpenReply>;

  Future<OpenReply> Open(const OpenRequest& request);

  Future<OpenReply> Open(const Drawable& drawable = {},
                         const uint32_t& provider = {});

  struct PixmapFromBufferRequest {
    Pixmap pixmap{};
    Drawable drawable{};
    uint32_t size{};
    uint16_t width{};
    uint16_t height{};
    uint16_t stride{};
    uint8_t depth{};
    uint8_t bpp{};
    RefCountedFD pixmap_fd{};
  };

  using PixmapFromBufferResponse = Response<void>;

  Future<void> PixmapFromBuffer(const PixmapFromBufferRequest& request);

  Future<void> PixmapFromBuffer(const Pixmap& pixmap = {},
                                const Drawable& drawable = {},
                                const uint32_t& size = {},
                                const uint16_t& width = {},
                                const uint16_t& height = {},
                                const uint16_t& stride = {},
                                const uint8_t& depth = {},
                                const uint8_t& bpp = {},
                                const RefCountedFD& pixmap_fd = {});

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
    RefCountedFD pixmap_fd{};
  };

  using BufferFromPixmapResponse = Response<BufferFromPixmapReply>;

  Future<BufferFromPixmapReply> BufferFromPixmap(
      const BufferFromPixmapRequest& request);

  Future<BufferFromPixmapReply> BufferFromPixmap(const Pixmap& pixmap = {});

  struct FenceFromFDRequest {
    Drawable drawable{};
    uint32_t fence{};
    uint8_t initially_triggered{};
    RefCountedFD fence_fd{};
  };

  using FenceFromFDResponse = Response<void>;

  Future<void> FenceFromFD(const FenceFromFDRequest& request);

  Future<void> FenceFromFD(const Drawable& drawable = {},
                           const uint32_t& fence = {},
                           const uint8_t& initially_triggered = {},
                           const RefCountedFD& fence_fd = {});

  struct FDFromFenceRequest {
    Drawable drawable{};
    uint32_t fence{};
  };

  struct FDFromFenceReply {
    uint8_t nfd{};
    uint16_t sequence{};
    RefCountedFD fence_fd{};
  };

  using FDFromFenceResponse = Response<FDFromFenceReply>;

  Future<FDFromFenceReply> FDFromFence(const FDFromFenceRequest& request);

  Future<FDFromFenceReply> FDFromFence(const Drawable& drawable = {},
                                       const uint32_t& fence = {});

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

  Future<GetSupportedModifiersReply> GetSupportedModifiers(
      const uint32_t& window = {},
      const uint8_t& depth = {},
      const uint8_t& bpp = {});

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
    std::vector<RefCountedFD> buffers{};
  };

  using PixmapFromBuffersResponse = Response<void>;

  Future<void> PixmapFromBuffers(const PixmapFromBuffersRequest& request);

  Future<void> PixmapFromBuffers(const Pixmap& pixmap = {},
                                 const Window& window = {},
                                 const uint16_t& width = {},
                                 const uint16_t& height = {},
                                 const uint32_t& stride0 = {},
                                 const uint32_t& offset0 = {},
                                 const uint32_t& stride1 = {},
                                 const uint32_t& offset1 = {},
                                 const uint32_t& stride2 = {},
                                 const uint32_t& offset2 = {},
                                 const uint32_t& stride3 = {},
                                 const uint32_t& offset3 = {},
                                 const uint8_t& depth = {},
                                 const uint8_t& bpp = {},
                                 const uint64_t& modifier = {},
                                 const std::vector<RefCountedFD>& buffers = {});

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
    std::vector<RefCountedFD> buffers{};
  };

  using BuffersFromPixmapResponse = Response<BuffersFromPixmapReply>;

  Future<BuffersFromPixmapReply> BuffersFromPixmap(
      const BuffersFromPixmapRequest& request);

  Future<BuffersFromPixmapReply> BuffersFromPixmap(const Pixmap& pixmap = {});

  struct SetDRMDeviceInUseRequest {
    Window window{};
    uint32_t drmMajor{};
    uint32_t drmMinor{};
  };

  using SetDRMDeviceInUseResponse = Response<void>;

  Future<void> SetDRMDeviceInUse(const SetDRMDeviceInUseRequest& request);

  Future<void> SetDRMDeviceInUse(const Window& window = {},
                                 const uint32_t& drmMajor = {},
                                 const uint32_t& drmMinor = {});

  struct ImportSyncobjRequest {
    Syncobj syncobj{};
    Drawable drawable{};
    RefCountedFD syncobj_fd{};
  };

  using ImportSyncobjResponse = Response<void>;

  Future<void> ImportSyncobj(const ImportSyncobjRequest& request);

  Future<void> ImportSyncobj(const Syncobj& syncobj = {},
                             const Drawable& drawable = {},
                             const RefCountedFD& syncobj_fd = {});

  struct FreeSyncobjRequest {
    Syncobj syncobj{};
  };

  using FreeSyncobjResponse = Response<void>;

  Future<void> FreeSyncobj(const FreeSyncobjRequest& request);

  Future<void> FreeSyncobj(const Syncobj& syncobj = {});

 private:
  Connection* const connection_;
  x11::QueryExtensionReply info_{};
};

}  // namespace x11

#endif  // UI_GFX_X_GENERATED_PROTOS_DRI3_H_
