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

#ifndef UI_GFX_X_GENERATED_PROTOS_DRI2_H_
#define UI_GFX_X_GENERATED_PROTOS_DRI2_H_

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

class COMPONENT_EXPORT(X11) Dri2 {
 public:
  static constexpr unsigned major_version = 1;
  static constexpr unsigned minor_version = 4;

  Dri2(Connection* connection, const x11::QueryExtensionReply& info);

  uint8_t present() const { return info_.present; }
  uint8_t major_opcode() const { return info_.major_opcode; }
  uint8_t first_event() const { return info_.first_event; }
  uint8_t first_error() const { return info_.first_error; }

  Connection* connection() const { return connection_; }

  enum class Attachment : int {
    BufferFrontLeft = 0,
    BufferBackLeft = 1,
    BufferFrontRight = 2,
    BufferBackRight = 3,
    BufferDepth = 4,
    BufferStencil = 5,
    BufferAccum = 6,
    BufferFakeFrontLeft = 7,
    BufferFakeFrontRight = 8,
    BufferDepthStencil = 9,
    BufferHiz = 10,
  };

  enum class DriverType : int {
    DRI = 0,
    VDPAU = 1,
  };

  enum class EventType : int {
    ExchangeComplete = 1,
    BlitComplete = 2,
    FlipComplete = 3,
  };

  struct DRI2Buffer {
    Attachment attachment{};
    uint32_t name{};
    uint32_t pitch{};
    uint32_t cpp{};
    uint32_t flags{};
  };

  struct AttachFormat {
    Attachment attachment{};
    uint32_t format{};
  };

  struct BufferSwapCompleteEvent {
    static constexpr int type_id = 2;
    static constexpr uint8_t opcode = 0;
    bool send_event{};
    uint16_t sequence{};
    EventType event_type{};
    Drawable drawable{};
    uint32_t ust_hi{};
    uint32_t ust_lo{};
    uint32_t msc_hi{};
    uint32_t msc_lo{};
    uint32_t sbc{};

    x11::Window* GetWindow() {
      return reinterpret_cast<x11::Window*>(&drawable);
    }
  };

  struct InvalidateBuffersEvent {
    static constexpr int type_id = 3;
    static constexpr uint8_t opcode = 1;
    bool send_event{};
    uint16_t sequence{};
    Drawable drawable{};

    x11::Window* GetWindow() {
      return reinterpret_cast<x11::Window*>(&drawable);
    }
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

  struct ConnectRequest {
    Window window{};
    DriverType driver_type{};
  };

  struct ConnectReply {
    uint16_t sequence{};
    std::string driver_name{};
    scoped_refptr<base::RefCountedMemory> alignment_pad{};
    std::string device_name{};
  };

  using ConnectResponse = Response<ConnectReply>;

  Future<ConnectReply> Connect(const ConnectRequest& request);

  struct AuthenticateRequest {
    Window window{};
    uint32_t magic{};
  };

  struct AuthenticateReply {
    uint16_t sequence{};
    uint32_t authenticated{};
  };

  using AuthenticateResponse = Response<AuthenticateReply>;

  Future<AuthenticateReply> Authenticate(const AuthenticateRequest& request);

  struct CreateDrawableRequest {
    Drawable drawable{};
  };

  using CreateDrawableResponse = Response<void>;

  Future<void> CreateDrawable(const CreateDrawableRequest& request);

  struct DestroyDrawableRequest {
    Drawable drawable{};
  };

  using DestroyDrawableResponse = Response<void>;

  Future<void> DestroyDrawable(const DestroyDrawableRequest& request);

  struct GetBuffersRequest {
    Drawable drawable{};
    uint32_t count{};
    std::vector<uint32_t> attachments{};
  };

  struct GetBuffersReply {
    uint16_t sequence{};
    uint32_t width{};
    uint32_t height{};
    std::vector<DRI2Buffer> buffers{};
  };

  using GetBuffersResponse = Response<GetBuffersReply>;

  Future<GetBuffersReply> GetBuffers(const GetBuffersRequest& request);

  struct CopyRegionRequest {
    Drawable drawable{};
    uint32_t region{};
    uint32_t dest{};
    uint32_t src{};
  };

  struct CopyRegionReply {
    uint16_t sequence{};
  };

  using CopyRegionResponse = Response<CopyRegionReply>;

  Future<CopyRegionReply> CopyRegion(const CopyRegionRequest& request);

  struct GetBuffersWithFormatRequest {
    Drawable drawable{};
    uint32_t count{};
    std::vector<AttachFormat> attachments{};
  };

  struct GetBuffersWithFormatReply {
    uint16_t sequence{};
    uint32_t width{};
    uint32_t height{};
    std::vector<DRI2Buffer> buffers{};
  };

  using GetBuffersWithFormatResponse = Response<GetBuffersWithFormatReply>;

  Future<GetBuffersWithFormatReply> GetBuffersWithFormat(
      const GetBuffersWithFormatRequest& request);

  struct SwapBuffersRequest {
    Drawable drawable{};
    uint32_t target_msc_hi{};
    uint32_t target_msc_lo{};
    uint32_t divisor_hi{};
    uint32_t divisor_lo{};
    uint32_t remainder_hi{};
    uint32_t remainder_lo{};
  };

  struct SwapBuffersReply {
    uint16_t sequence{};
    uint32_t swap_hi{};
    uint32_t swap_lo{};
  };

  using SwapBuffersResponse = Response<SwapBuffersReply>;

  Future<SwapBuffersReply> SwapBuffers(const SwapBuffersRequest& request);

  struct GetMSCRequest {
    Drawable drawable{};
  };

  struct GetMSCReply {
    uint16_t sequence{};
    uint32_t ust_hi{};
    uint32_t ust_lo{};
    uint32_t msc_hi{};
    uint32_t msc_lo{};
    uint32_t sbc_hi{};
    uint32_t sbc_lo{};
  };

  using GetMSCResponse = Response<GetMSCReply>;

  Future<GetMSCReply> GetMSC(const GetMSCRequest& request);

  struct WaitMSCRequest {
    Drawable drawable{};
    uint32_t target_msc_hi{};
    uint32_t target_msc_lo{};
    uint32_t divisor_hi{};
    uint32_t divisor_lo{};
    uint32_t remainder_hi{};
    uint32_t remainder_lo{};
  };

  struct WaitMSCReply {
    uint16_t sequence{};
    uint32_t ust_hi{};
    uint32_t ust_lo{};
    uint32_t msc_hi{};
    uint32_t msc_lo{};
    uint32_t sbc_hi{};
    uint32_t sbc_lo{};
  };

  using WaitMSCResponse = Response<WaitMSCReply>;

  Future<WaitMSCReply> WaitMSC(const WaitMSCRequest& request);

  struct WaitSBCRequest {
    Drawable drawable{};
    uint32_t target_sbc_hi{};
    uint32_t target_sbc_lo{};
  };

  struct WaitSBCReply {
    uint16_t sequence{};
    uint32_t ust_hi{};
    uint32_t ust_lo{};
    uint32_t msc_hi{};
    uint32_t msc_lo{};
    uint32_t sbc_hi{};
    uint32_t sbc_lo{};
  };

  using WaitSBCResponse = Response<WaitSBCReply>;

  Future<WaitSBCReply> WaitSBC(const WaitSBCRequest& request);

  struct SwapIntervalRequest {
    Drawable drawable{};
    uint32_t interval{};
  };

  using SwapIntervalResponse = Response<void>;

  Future<void> SwapInterval(const SwapIntervalRequest& request);

  struct GetParamRequest {
    Drawable drawable{};
    uint32_t param{};
  };

  struct GetParamReply {
    uint8_t is_param_recognized{};
    uint16_t sequence{};
    uint32_t value_hi{};
    uint32_t value_lo{};
  };

  using GetParamResponse = Response<GetParamReply>;

  Future<GetParamReply> GetParam(const GetParamRequest& request);

 private:
  Connection* const connection_;
  x11::QueryExtensionReply info_{};
};

}  // namespace x11

inline constexpr x11::Dri2::Attachment operator|(x11::Dri2::Attachment l,
                                                 x11::Dri2::Attachment r) {
  using T = std::underlying_type_t<x11::Dri2::Attachment>;
  return static_cast<x11::Dri2::Attachment>(static_cast<T>(l) |
                                            static_cast<T>(r));
}

inline constexpr x11::Dri2::Attachment operator&(x11::Dri2::Attachment l,
                                                 x11::Dri2::Attachment r) {
  using T = std::underlying_type_t<x11::Dri2::Attachment>;
  return static_cast<x11::Dri2::Attachment>(static_cast<T>(l) &
                                            static_cast<T>(r));
}

inline constexpr x11::Dri2::DriverType operator|(x11::Dri2::DriverType l,
                                                 x11::Dri2::DriverType r) {
  using T = std::underlying_type_t<x11::Dri2::DriverType>;
  return static_cast<x11::Dri2::DriverType>(static_cast<T>(l) |
                                            static_cast<T>(r));
}

inline constexpr x11::Dri2::DriverType operator&(x11::Dri2::DriverType l,
                                                 x11::Dri2::DriverType r) {
  using T = std::underlying_type_t<x11::Dri2::DriverType>;
  return static_cast<x11::Dri2::DriverType>(static_cast<T>(l) &
                                            static_cast<T>(r));
}

inline constexpr x11::Dri2::EventType operator|(x11::Dri2::EventType l,
                                                x11::Dri2::EventType r) {
  using T = std::underlying_type_t<x11::Dri2::EventType>;
  return static_cast<x11::Dri2::EventType>(static_cast<T>(l) |
                                           static_cast<T>(r));
}

inline constexpr x11::Dri2::EventType operator&(x11::Dri2::EventType l,
                                                x11::Dri2::EventType r) {
  using T = std::underlying_type_t<x11::Dri2::EventType>;
  return static_cast<x11::Dri2::EventType>(static_cast<T>(l) &
                                           static_cast<T>(r));
}

#endif  // UI_GFX_X_GENERATED_PROTOS_DRI2_H_
