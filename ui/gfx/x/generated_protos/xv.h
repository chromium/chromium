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

#ifndef UI_GFX_X_GENERATED_PROTOS_XV_H_
#define UI_GFX_X_GENERATED_PROTOS_XV_H_

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
#include "shm.h"
#include "ui/gfx/x/error.h"
#include "ui/gfx/x/ref_counted_fd.h"
#include "xproto.h"

namespace x11 {

class Connection;

template <typename Reply>
struct Response;

template <typename Reply>
class Future;

class COMPONENT_EXPORT(X11) Xv {
 public:
  static constexpr unsigned major_version = 2;
  static constexpr unsigned minor_version = 2;

  Xv(Connection* connection, const x11::QueryExtensionReply& info);

  uint8_t present() const { return info_.present; }
  uint8_t major_opcode() const { return info_.major_opcode; }
  uint8_t first_event() const { return info_.first_event; }
  uint8_t first_error() const { return info_.first_error; }

  Connection* connection() const { return connection_; }

  enum class Port : uint32_t {};

  enum class Encoding : uint32_t {};

  enum class Type : int {
    InputMask = 1 << 0,
    OutputMask = 1 << 1,
    VideoMask = 1 << 2,
    StillMask = 1 << 3,
    ImageMask = 1 << 4,
  };

  enum class ImageFormatInfoType : int {
    RGB = 0,
    YUV = 1,
  };

  enum class ImageFormatInfoFormat : int {
    Packed = 0,
    Planar = 1,
  };

  enum class AttributeFlag : int {
    Gettable = 1 << 0,
    Settable = 1 << 1,
  };

  enum class VideoNotifyReason : int {
    Started = 0,
    Stopped = 1,
    Busy = 2,
    Preempted = 3,
    HardError = 4,
  };

  enum class ScanlineOrder : int {
    TopToBottom = 0,
    BottomToTop = 1,
  };

  enum class GrabPortStatus : int {
    Success = 0,
    BadExtension = 1,
    AlreadyGrabbed = 2,
    InvalidTime = 3,
    BadReply = 4,
    BadAlloc = 5,
  };

  struct Rational {
    int32_t numerator{};
    int32_t denominator{};
  };

  struct Format {
    VisualId visual{};
    uint8_t depth{};
  };

  struct AdaptorInfo {
    Port base_id{};
    uint16_t num_ports{};
    Type type{};
    std::string name{};
    std::vector<Format> formats{};
  };

  struct EncodingInfo {
    Encoding encoding{};
    uint16_t width{};
    uint16_t height{};
    Rational rate{};
    std::string name{};
  };

  struct Image {
    uint32_t id{};
    uint16_t width{};
    uint16_t height{};
    std::vector<uint32_t> pitches{};
    std::vector<uint32_t> offsets{};
    std::vector<uint8_t> data{};
  };

  struct AttributeInfo {
    AttributeFlag flags{};
    int32_t min{};
    int32_t max{};
    std::string name{};
  };

  struct ImageFormatInfo {
    uint32_t id{};
    ImageFormatInfoType type{};
    ImageOrder byte_order{};
    std::array<uint8_t, 16> guid{};
    uint8_t bpp{};
    uint8_t num_planes{};
    uint8_t depth{};
    uint32_t red_mask{};
    uint32_t green_mask{};
    uint32_t blue_mask{};
    ImageFormatInfoFormat format{};
    uint32_t y_sample_bits{};
    uint32_t u_sample_bits{};
    uint32_t v_sample_bits{};
    uint32_t vhorz_y_period{};
    uint32_t vhorz_u_period{};
    uint32_t vhorz_v_period{};
    uint32_t vvert_y_period{};
    uint32_t vvert_u_period{};
    uint32_t vvert_v_period{};
    std::array<uint8_t, 32> vcomp_order{};
    ScanlineOrder vscanline_order{};
  };

  struct BadPortError : public x11::Error {
    uint16_t sequence{};

    std::string ToString() const override;
  };

  struct BadEncodingError : public x11::Error {
    uint16_t sequence{};

    std::string ToString() const override;
  };

  struct BadControlError : public x11::Error {
    uint16_t sequence{};

    std::string ToString() const override;
  };

  struct VideoNotifyEvent {
    static constexpr int type_id = 81;
    static constexpr uint8_t opcode = 0;
    bool send_event{};
    VideoNotifyReason reason{};
    uint16_t sequence{};
    Time time{};
    Drawable drawable{};
    Port port{};

    x11::Window* GetWindow() {
      return reinterpret_cast<x11::Window*>(&drawable);
    }
  };

  struct PortNotifyEvent {
    static constexpr int type_id = 82;
    static constexpr uint8_t opcode = 1;
    bool send_event{};
    uint16_t sequence{};
    Time time{};
    Port port{};
    Atom attribute{};
    int32_t value{};

    x11::Window* GetWindow() { return nullptr; }
  };

  struct QueryExtensionRequest {};

  struct QueryExtensionReply {
    uint16_t sequence{};
    uint16_t major{};
    uint16_t minor{};
  };

  using QueryExtensionResponse = Response<QueryExtensionReply>;

  Future<QueryExtensionReply> QueryExtension(
      const QueryExtensionRequest& request);

  Future<QueryExtensionReply> QueryExtension();

  struct QueryAdaptorsRequest {
    Window window{};
  };

  struct QueryAdaptorsReply {
    uint16_t sequence{};
    std::vector<AdaptorInfo> info{};
  };

  using QueryAdaptorsResponse = Response<QueryAdaptorsReply>;

  Future<QueryAdaptorsReply> QueryAdaptors(const QueryAdaptorsRequest& request);

  Future<QueryAdaptorsReply> QueryAdaptors(const Window& window = {});

  struct QueryEncodingsRequest {
    Port port{};
  };

  struct QueryEncodingsReply {
    uint16_t sequence{};
    std::vector<EncodingInfo> info{};
  };

  using QueryEncodingsResponse = Response<QueryEncodingsReply>;

  Future<QueryEncodingsReply> QueryEncodings(
      const QueryEncodingsRequest& request);

  Future<QueryEncodingsReply> QueryEncodings(const Port& port = {});

  struct GrabPortRequest {
    Port port{};
    Time time{};
  };

  struct GrabPortReply {
    GrabPortStatus result{};
    uint16_t sequence{};
  };

  using GrabPortResponse = Response<GrabPortReply>;

  Future<GrabPortReply> GrabPort(const GrabPortRequest& request);

  Future<GrabPortReply> GrabPort(const Port& port = {}, const Time& time = {});

  struct UngrabPortRequest {
    Port port{};
    Time time{};
  };

  using UngrabPortResponse = Response<void>;

  Future<void> UngrabPort(const UngrabPortRequest& request);

  Future<void> UngrabPort(const Port& port = {}, const Time& time = {});

  struct PutVideoRequest {
    Port port{};
    Drawable drawable{};
    GraphicsContext gc{};
    int16_t vid_x{};
    int16_t vid_y{};
    uint16_t vid_w{};
    uint16_t vid_h{};
    int16_t drw_x{};
    int16_t drw_y{};
    uint16_t drw_w{};
    uint16_t drw_h{};
  };

  using PutVideoResponse = Response<void>;

  Future<void> PutVideo(const PutVideoRequest& request);

  Future<void> PutVideo(const Port& port = {},
                        const Drawable& drawable = {},
                        const GraphicsContext& gc = {},
                        const int16_t& vid_x = {},
                        const int16_t& vid_y = {},
                        const uint16_t& vid_w = {},
                        const uint16_t& vid_h = {},
                        const int16_t& drw_x = {},
                        const int16_t& drw_y = {},
                        const uint16_t& drw_w = {},
                        const uint16_t& drw_h = {});

  struct PutStillRequest {
    Port port{};
    Drawable drawable{};
    GraphicsContext gc{};
    int16_t vid_x{};
    int16_t vid_y{};
    uint16_t vid_w{};
    uint16_t vid_h{};
    int16_t drw_x{};
    int16_t drw_y{};
    uint16_t drw_w{};
    uint16_t drw_h{};
  };

  using PutStillResponse = Response<void>;

  Future<void> PutStill(const PutStillRequest& request);

  Future<void> PutStill(const Port& port = {},
                        const Drawable& drawable = {},
                        const GraphicsContext& gc = {},
                        const int16_t& vid_x = {},
                        const int16_t& vid_y = {},
                        const uint16_t& vid_w = {},
                        const uint16_t& vid_h = {},
                        const int16_t& drw_x = {},
                        const int16_t& drw_y = {},
                        const uint16_t& drw_w = {},
                        const uint16_t& drw_h = {});

  struct GetVideoRequest {
    Port port{};
    Drawable drawable{};
    GraphicsContext gc{};
    int16_t vid_x{};
    int16_t vid_y{};
    uint16_t vid_w{};
    uint16_t vid_h{};
    int16_t drw_x{};
    int16_t drw_y{};
    uint16_t drw_w{};
    uint16_t drw_h{};
  };

  using GetVideoResponse = Response<void>;

  Future<void> GetVideo(const GetVideoRequest& request);

  Future<void> GetVideo(const Port& port = {},
                        const Drawable& drawable = {},
                        const GraphicsContext& gc = {},
                        const int16_t& vid_x = {},
                        const int16_t& vid_y = {},
                        const uint16_t& vid_w = {},
                        const uint16_t& vid_h = {},
                        const int16_t& drw_x = {},
                        const int16_t& drw_y = {},
                        const uint16_t& drw_w = {},
                        const uint16_t& drw_h = {});

  struct GetStillRequest {
    Port port{};
    Drawable drawable{};
    GraphicsContext gc{};
    int16_t vid_x{};
    int16_t vid_y{};
    uint16_t vid_w{};
    uint16_t vid_h{};
    int16_t drw_x{};
    int16_t drw_y{};
    uint16_t drw_w{};
    uint16_t drw_h{};
  };

  using GetStillResponse = Response<void>;

  Future<void> GetStill(const GetStillRequest& request);

  Future<void> GetStill(const Port& port = {},
                        const Drawable& drawable = {},
                        const GraphicsContext& gc = {},
                        const int16_t& vid_x = {},
                        const int16_t& vid_y = {},
                        const uint16_t& vid_w = {},
                        const uint16_t& vid_h = {},
                        const int16_t& drw_x = {},
                        const int16_t& drw_y = {},
                        const uint16_t& drw_w = {},
                        const uint16_t& drw_h = {});

  struct StopVideoRequest {
    Port port{};
    Drawable drawable{};
  };

  using StopVideoResponse = Response<void>;

  Future<void> StopVideo(const StopVideoRequest& request);

  Future<void> StopVideo(const Port& port = {}, const Drawable& drawable = {});

  struct SelectVideoNotifyRequest {
    Drawable drawable{};
    uint8_t onoff{};
  };

  using SelectVideoNotifyResponse = Response<void>;

  Future<void> SelectVideoNotify(const SelectVideoNotifyRequest& request);

  Future<void> SelectVideoNotify(const Drawable& drawable = {},
                                 const uint8_t& onoff = {});

  struct SelectPortNotifyRequest {
    Port port{};
    uint8_t onoff{};
  };

  using SelectPortNotifyResponse = Response<void>;

  Future<void> SelectPortNotify(const SelectPortNotifyRequest& request);

  Future<void> SelectPortNotify(const Port& port = {},
                                const uint8_t& onoff = {});

  struct QueryBestSizeRequest {
    Port port{};
    uint16_t vid_w{};
    uint16_t vid_h{};
    uint16_t drw_w{};
    uint16_t drw_h{};
    uint8_t motion{};
  };

  struct QueryBestSizeReply {
    uint16_t sequence{};
    uint16_t actual_width{};
    uint16_t actual_height{};
  };

  using QueryBestSizeResponse = Response<QueryBestSizeReply>;

  Future<QueryBestSizeReply> QueryBestSize(const QueryBestSizeRequest& request);

  Future<QueryBestSizeReply> QueryBestSize(const Port& port = {},
                                           const uint16_t& vid_w = {},
                                           const uint16_t& vid_h = {},
                                           const uint16_t& drw_w = {},
                                           const uint16_t& drw_h = {},
                                           const uint8_t& motion = {});

  struct SetPortAttributeRequest {
    Port port{};
    Atom attribute{};
    int32_t value{};
  };

  using SetPortAttributeResponse = Response<void>;

  Future<void> SetPortAttribute(const SetPortAttributeRequest& request);

  Future<void> SetPortAttribute(const Port& port = {},
                                const Atom& attribute = {},
                                const int32_t& value = {});

  struct GetPortAttributeRequest {
    Port port{};
    Atom attribute{};
  };

  struct GetPortAttributeReply {
    uint16_t sequence{};
    int32_t value{};
  };

  using GetPortAttributeResponse = Response<GetPortAttributeReply>;

  Future<GetPortAttributeReply> GetPortAttribute(
      const GetPortAttributeRequest& request);

  Future<GetPortAttributeReply> GetPortAttribute(const Port& port = {},
                                                 const Atom& attribute = {});

  struct QueryPortAttributesRequest {
    Port port{};
  };

  struct QueryPortAttributesReply {
    uint16_t sequence{};
    uint32_t text_size{};
    std::vector<AttributeInfo> attributes{};
  };

  using QueryPortAttributesResponse = Response<QueryPortAttributesReply>;

  Future<QueryPortAttributesReply> QueryPortAttributes(
      const QueryPortAttributesRequest& request);

  Future<QueryPortAttributesReply> QueryPortAttributes(const Port& port = {});

  struct ListImageFormatsRequest {
    Port port{};
  };

  struct ListImageFormatsReply {
    uint16_t sequence{};
    std::vector<ImageFormatInfo> format{};
  };

  using ListImageFormatsResponse = Response<ListImageFormatsReply>;

  Future<ListImageFormatsReply> ListImageFormats(
      const ListImageFormatsRequest& request);

  Future<ListImageFormatsReply> ListImageFormats(const Port& port = {});

  struct QueryImageAttributesRequest {
    Port port{};
    uint32_t id{};
    uint16_t width{};
    uint16_t height{};
  };

  struct QueryImageAttributesReply {
    uint16_t sequence{};
    uint32_t data_size{};
    uint16_t width{};
    uint16_t height{};
    std::vector<uint32_t> pitches{};
    std::vector<uint32_t> offsets{};
  };

  using QueryImageAttributesResponse = Response<QueryImageAttributesReply>;

  Future<QueryImageAttributesReply> QueryImageAttributes(
      const QueryImageAttributesRequest& request);

  Future<QueryImageAttributesReply> QueryImageAttributes(
      const Port& port = {},
      const uint32_t& id = {},
      const uint16_t& width = {},
      const uint16_t& height = {});

  struct PutImageRequest {
    Port port{};
    Drawable drawable{};
    GraphicsContext gc{};
    uint32_t id{};
    int16_t src_x{};
    int16_t src_y{};
    uint16_t src_w{};
    uint16_t src_h{};
    int16_t drw_x{};
    int16_t drw_y{};
    uint16_t drw_w{};
    uint16_t drw_h{};
    uint16_t width{};
    uint16_t height{};
    std::vector<uint8_t> data{};
  };

  using PutImageResponse = Response<void>;

  Future<void> PutImage(const PutImageRequest& request);

  Future<void> PutImage(const Port& port = {},
                        const Drawable& drawable = {},
                        const GraphicsContext& gc = {},
                        const uint32_t& id = {},
                        const int16_t& src_x = {},
                        const int16_t& src_y = {},
                        const uint16_t& src_w = {},
                        const uint16_t& src_h = {},
                        const int16_t& drw_x = {},
                        const int16_t& drw_y = {},
                        const uint16_t& drw_w = {},
                        const uint16_t& drw_h = {},
                        const uint16_t& width = {},
                        const uint16_t& height = {},
                        const std::vector<uint8_t>& data = {});

  struct ShmPutImageRequest {
    Port port{};
    Drawable drawable{};
    GraphicsContext gc{};
    Shm::Seg shmseg{};
    uint32_t id{};
    uint32_t offset{};
    int16_t src_x{};
    int16_t src_y{};
    uint16_t src_w{};
    uint16_t src_h{};
    int16_t drw_x{};
    int16_t drw_y{};
    uint16_t drw_w{};
    uint16_t drw_h{};
    uint16_t width{};
    uint16_t height{};
    uint8_t send_event{};
  };

  using ShmPutImageResponse = Response<void>;

  Future<void> ShmPutImage(const ShmPutImageRequest& request);

  Future<void> ShmPutImage(const Port& port = {},
                           const Drawable& drawable = {},
                           const GraphicsContext& gc = {},
                           const Shm::Seg& shmseg = {},
                           const uint32_t& id = {},
                           const uint32_t& offset = {},
                           const int16_t& src_x = {},
                           const int16_t& src_y = {},
                           const uint16_t& src_w = {},
                           const uint16_t& src_h = {},
                           const int16_t& drw_x = {},
                           const int16_t& drw_y = {},
                           const uint16_t& drw_w = {},
                           const uint16_t& drw_h = {},
                           const uint16_t& width = {},
                           const uint16_t& height = {},
                           const uint8_t& send_event = {});

 private:
  Connection* const connection_;
  x11::QueryExtensionReply info_{};
};

}  // namespace x11

inline constexpr x11::Xv::Type operator|(x11::Xv::Type l, x11::Xv::Type r) {
  using T = std::underlying_type_t<x11::Xv::Type>;
  return static_cast<x11::Xv::Type>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::Xv::Type operator&(x11::Xv::Type l, x11::Xv::Type r) {
  using T = std::underlying_type_t<x11::Xv::Type>;
  return static_cast<x11::Xv::Type>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::Xv::ImageFormatInfoType operator|(
    x11::Xv::ImageFormatInfoType l,
    x11::Xv::ImageFormatInfoType r) {
  using T = std::underlying_type_t<x11::Xv::ImageFormatInfoType>;
  return static_cast<x11::Xv::ImageFormatInfoType>(static_cast<T>(l) |
                                                   static_cast<T>(r));
}

inline constexpr x11::Xv::ImageFormatInfoType operator&(
    x11::Xv::ImageFormatInfoType l,
    x11::Xv::ImageFormatInfoType r) {
  using T = std::underlying_type_t<x11::Xv::ImageFormatInfoType>;
  return static_cast<x11::Xv::ImageFormatInfoType>(static_cast<T>(l) &
                                                   static_cast<T>(r));
}

inline constexpr x11::Xv::ImageFormatInfoFormat operator|(
    x11::Xv::ImageFormatInfoFormat l,
    x11::Xv::ImageFormatInfoFormat r) {
  using T = std::underlying_type_t<x11::Xv::ImageFormatInfoFormat>;
  return static_cast<x11::Xv::ImageFormatInfoFormat>(static_cast<T>(l) |
                                                     static_cast<T>(r));
}

inline constexpr x11::Xv::ImageFormatInfoFormat operator&(
    x11::Xv::ImageFormatInfoFormat l,
    x11::Xv::ImageFormatInfoFormat r) {
  using T = std::underlying_type_t<x11::Xv::ImageFormatInfoFormat>;
  return static_cast<x11::Xv::ImageFormatInfoFormat>(static_cast<T>(l) &
                                                     static_cast<T>(r));
}

inline constexpr x11::Xv::AttributeFlag operator|(x11::Xv::AttributeFlag l,
                                                  x11::Xv::AttributeFlag r) {
  using T = std::underlying_type_t<x11::Xv::AttributeFlag>;
  return static_cast<x11::Xv::AttributeFlag>(static_cast<T>(l) |
                                             static_cast<T>(r));
}

inline constexpr x11::Xv::AttributeFlag operator&(x11::Xv::AttributeFlag l,
                                                  x11::Xv::AttributeFlag r) {
  using T = std::underlying_type_t<x11::Xv::AttributeFlag>;
  return static_cast<x11::Xv::AttributeFlag>(static_cast<T>(l) &
                                             static_cast<T>(r));
}

inline constexpr x11::Xv::VideoNotifyReason operator|(
    x11::Xv::VideoNotifyReason l,
    x11::Xv::VideoNotifyReason r) {
  using T = std::underlying_type_t<x11::Xv::VideoNotifyReason>;
  return static_cast<x11::Xv::VideoNotifyReason>(static_cast<T>(l) |
                                                 static_cast<T>(r));
}

inline constexpr x11::Xv::VideoNotifyReason operator&(
    x11::Xv::VideoNotifyReason l,
    x11::Xv::VideoNotifyReason r) {
  using T = std::underlying_type_t<x11::Xv::VideoNotifyReason>;
  return static_cast<x11::Xv::VideoNotifyReason>(static_cast<T>(l) &
                                                 static_cast<T>(r));
}

inline constexpr x11::Xv::ScanlineOrder operator|(x11::Xv::ScanlineOrder l,
                                                  x11::Xv::ScanlineOrder r) {
  using T = std::underlying_type_t<x11::Xv::ScanlineOrder>;
  return static_cast<x11::Xv::ScanlineOrder>(static_cast<T>(l) |
                                             static_cast<T>(r));
}

inline constexpr x11::Xv::ScanlineOrder operator&(x11::Xv::ScanlineOrder l,
                                                  x11::Xv::ScanlineOrder r) {
  using T = std::underlying_type_t<x11::Xv::ScanlineOrder>;
  return static_cast<x11::Xv::ScanlineOrder>(static_cast<T>(l) &
                                             static_cast<T>(r));
}

inline constexpr x11::Xv::GrabPortStatus operator|(x11::Xv::GrabPortStatus l,
                                                   x11::Xv::GrabPortStatus r) {
  using T = std::underlying_type_t<x11::Xv::GrabPortStatus>;
  return static_cast<x11::Xv::GrabPortStatus>(static_cast<T>(l) |
                                              static_cast<T>(r));
}

inline constexpr x11::Xv::GrabPortStatus operator&(x11::Xv::GrabPortStatus l,
                                                   x11::Xv::GrabPortStatus r) {
  using T = std::underlying_type_t<x11::Xv::GrabPortStatus>;
  return static_cast<x11::Xv::GrabPortStatus>(static_cast<T>(l) &
                                              static_cast<T>(r));
}

#endif  // UI_GFX_X_GENERATED_PROTOS_XV_H_
