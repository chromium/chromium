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

#ifndef UI_GFX_X_GENERATED_PROTOS_SHM_H_
#define UI_GFX_X_GENERATED_PROTOS_SHM_H_

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

class COMPONENT_EXPORT(X11) Shm {
 public:
  static constexpr unsigned major_version = 1;
  static constexpr unsigned minor_version = 2;

  Shm(Connection* connection, const x11::QueryExtensionReply& info);

  uint8_t present() const { return info_.present; }
  uint8_t major_opcode() const { return info_.major_opcode; }
  uint8_t first_event() const { return info_.first_event; }
  uint8_t first_error() const { return info_.first_error; }

  Connection* connection() const { return connection_; }

  enum class Seg : uint32_t {};

  struct CompletionEvent {
    static constexpr uint8_t type_id = 7;
    static constexpr uint8_t opcode = 0;
    uint16_t sequence{};
    Drawable drawable{};
    uint16_t minor_event{};
    uint8_t major_event{};
    Seg shmseg{};
    uint32_t offset{};
  };

  struct BadSegError : public x11::Error {
    uint16_t sequence{};
    uint32_t bad_value{};
    uint16_t minor_opcode{};
    uint8_t major_opcode{};

    std::string ToString() const override;
  };

  struct QueryVersionRequest {};

  struct QueryVersionReply {
    uint8_t shared_pixmaps{};
    uint16_t sequence{};
    uint16_t major_version{};
    uint16_t minor_version{};
    uint16_t uid{};
    uint16_t gid{};
    uint8_t pixmap_format{};
  };

  using QueryVersionResponse = Response<QueryVersionReply>;

  Future<QueryVersionReply> QueryVersion(const QueryVersionRequest& request);

  Future<QueryVersionReply> QueryVersion();

  struct AttachRequest {
    Seg shmseg{};
    uint32_t shmid{};
    uint8_t read_only{};
  };

  using AttachResponse = Response<void>;

  Future<void> Attach(const AttachRequest& request);

  Future<void> Attach(const Seg& shmseg = {},
                      const uint32_t& shmid = {},
                      const uint8_t& read_only = {});

  struct DetachRequest {
    Seg shmseg{};
  };

  using DetachResponse = Response<void>;

  Future<void> Detach(const DetachRequest& request);

  Future<void> Detach(const Seg& shmseg = {});

  struct PutImageRequest {
    Drawable drawable{};
    GraphicsContext gc{};
    uint16_t total_width{};
    uint16_t total_height{};
    uint16_t src_x{};
    uint16_t src_y{};
    uint16_t src_width{};
    uint16_t src_height{};
    int16_t dst_x{};
    int16_t dst_y{};
    uint8_t depth{};
    ImageFormat format{};
    uint8_t send_event{};
    Seg shmseg{};
    uint32_t offset{};
  };

  using PutImageResponse = Response<void>;

  Future<void> PutImage(const PutImageRequest& request);

  Future<void> PutImage(const Drawable& drawable = {},
                        const GraphicsContext& gc = {},
                        const uint16_t& total_width = {},
                        const uint16_t& total_height = {},
                        const uint16_t& src_x = {},
                        const uint16_t& src_y = {},
                        const uint16_t& src_width = {},
                        const uint16_t& src_height = {},
                        const int16_t& dst_x = {},
                        const int16_t& dst_y = {},
                        const uint8_t& depth = {},
                        const ImageFormat& format = {},
                        const uint8_t& send_event = {},
                        const Seg& shmseg = {},
                        const uint32_t& offset = {});

  struct GetImageRequest {
    Drawable drawable{};
    int16_t x{};
    int16_t y{};
    uint16_t width{};
    uint16_t height{};
    uint32_t plane_mask{};
    uint8_t format{};
    Seg shmseg{};
    uint32_t offset{};
  };

  struct GetImageReply {
    uint8_t depth{};
    uint16_t sequence{};
    VisualId visual{};
    uint32_t size{};
  };

  using GetImageResponse = Response<GetImageReply>;

  Future<GetImageReply> GetImage(const GetImageRequest& request);

  Future<GetImageReply> GetImage(const Drawable& drawable = {},
                                 const int16_t& x = {},
                                 const int16_t& y = {},
                                 const uint16_t& width = {},
                                 const uint16_t& height = {},
                                 const uint32_t& plane_mask = {},
                                 const uint8_t& format = {},
                                 const Seg& shmseg = {},
                                 const uint32_t& offset = {});

  struct CreatePixmapRequest {
    Pixmap pid{};
    Drawable drawable{};
    uint16_t width{};
    uint16_t height{};
    uint8_t depth{};
    Seg shmseg{};
    uint32_t offset{};
  };

  using CreatePixmapResponse = Response<void>;

  Future<void> CreatePixmap(const CreatePixmapRequest& request);

  Future<void> CreatePixmap(const Pixmap& pid = {},
                            const Drawable& drawable = {},
                            const uint16_t& width = {},
                            const uint16_t& height = {},
                            const uint8_t& depth = {},
                            const Seg& shmseg = {},
                            const uint32_t& offset = {});

  struct AttachFdRequest {
    Seg shmseg{};
    RefCountedFD shm_fd{};
    uint8_t read_only{};
  };

  using AttachFdResponse = Response<void>;

  Future<void> AttachFd(const AttachFdRequest& request);

  Future<void> AttachFd(const Seg& shmseg = {},
                        const RefCountedFD& shm_fd = {},
                        const uint8_t& read_only = {});

  struct CreateSegmentRequest {
    Seg shmseg{};
    uint32_t size{};
    uint8_t read_only{};
  };

  struct CreateSegmentReply {
    uint8_t nfd{};
    uint16_t sequence{};
    RefCountedFD shm_fd{};
  };

  using CreateSegmentResponse = Response<CreateSegmentReply>;

  Future<CreateSegmentReply> CreateSegment(const CreateSegmentRequest& request);

  Future<CreateSegmentReply> CreateSegment(const Seg& shmseg = {},
                                           const uint32_t& size = {},
                                           const uint8_t& read_only = {});

 private:
  Connection* const connection_;
  x11::QueryExtensionReply info_{};
};

}  // namespace x11

#endif  // UI_GFX_X_GENERATED_PROTOS_SHM_H_
