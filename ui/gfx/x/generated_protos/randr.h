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

#ifndef UI_GFX_X_GENERATED_PROTOS_RANDR_H_
#define UI_GFX_X_GENERATED_PROTOS_RANDR_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <vector>

#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "base/memory/scoped_refptr.h"
#include "render.h"
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

class COMPONENT_EXPORT(X11) RandR {
 public:
  static constexpr unsigned major_version = 1;
  static constexpr unsigned minor_version = 6;

  RandR(Connection* connection, const x11::QueryExtensionReply& info);

  uint8_t present() const { return info_.present; }
  uint8_t major_opcode() const { return info_.major_opcode; }
  uint8_t first_event() const { return info_.first_event; }
  uint8_t first_error() const { return info_.first_error; }

  Connection* connection() const { return connection_; }

  enum class Mode : uint32_t {};

  enum class Crtc : uint32_t {};

  enum class Output : uint32_t {};

  enum class Provider : uint32_t {};

  enum class Lease : uint32_t {};

  enum class Rotation : int {
    Rotate_0 = 1 << 0,
    Rotate_90 = 1 << 1,
    Rotate_180 = 1 << 2,
    Rotate_270 = 1 << 3,
    Reflect_X = 1 << 4,
    Reflect_Y = 1 << 5,
  };

  enum class SetConfig : int {
    Success = 0,
    InvalidConfigTime = 1,
    InvalidTime = 2,
    Failed = 3,
  };

  enum class NotifyMask : int {
    ScreenChange = 1 << 0,
    CrtcChange = 1 << 1,
    OutputChange = 1 << 2,
    OutputProperty = 1 << 3,
    ProviderChange = 1 << 4,
    ProviderProperty = 1 << 5,
    ResourceChange = 1 << 6,
    Lease = 1 << 7,
  };

  enum class ModeFlag : int {
    HsyncPositive = 1 << 0,
    HsyncNegative = 1 << 1,
    VsyncPositive = 1 << 2,
    VsyncNegative = 1 << 3,
    Interlace = 1 << 4,
    DoubleScan = 1 << 5,
    Csync = 1 << 6,
    CsyncPositive = 1 << 7,
    CsyncNegative = 1 << 8,
    HskewPresent = 1 << 9,
    Bcast = 1 << 10,
    PixelMultiplex = 1 << 11,
    DoubleClock = 1 << 12,
    HalveClock = 1 << 13,
  };

  enum class RandRConnection : int {
    Connected = 0,
    Disconnected = 1,
    Unknown = 2,
  };

  enum class Transform : int {
    Unit = 1 << 0,
    ScaleUp = 1 << 1,
    ScaleDown = 1 << 2,
    Projective = 1 << 3,
  };

  enum class ProviderCapability : int {
    SourceOutput = 1 << 0,
    SinkOutput = 1 << 1,
    SourceOffload = 1 << 2,
    SinkOffload = 1 << 3,
  };

  enum class Notify : int {
    CrtcChange = 0,
    OutputChange = 1,
    OutputProperty = 2,
    ProviderChange = 3,
    ProviderProperty = 4,
    ResourceChange = 5,
    Lease = 6,
  };

  struct BadOutputError : public x11::Error {
    uint16_t sequence{};
    uint32_t bad_value{};
    uint16_t minor_opcode{};
    uint8_t major_opcode{};

    std::string ToString() const override;
  };

  struct BadCrtcError : public x11::Error {
    uint16_t sequence{};
    uint32_t bad_value{};
    uint16_t minor_opcode{};
    uint8_t major_opcode{};

    std::string ToString() const override;
  };

  struct BadModeError : public x11::Error {
    uint16_t sequence{};
    uint32_t bad_value{};
    uint16_t minor_opcode{};
    uint8_t major_opcode{};

    std::string ToString() const override;
  };

  struct BadProviderError : public x11::Error {
    uint16_t sequence{};
    uint32_t bad_value{};
    uint16_t minor_opcode{};
    uint8_t major_opcode{};

    std::string ToString() const override;
  };

  struct ScreenSize {
    bool operator==(const ScreenSize& other) const {
      return width == other.width && height == other.height &&
             mwidth == other.mwidth && mheight == other.mheight;
    }

    uint16_t width{};
    uint16_t height{};
    uint16_t mwidth{};
    uint16_t mheight{};
  };

  struct RefreshRates {
    bool operator==(const RefreshRates& other) const {
      return rates == other.rates;
    }

    std::vector<uint16_t> rates{};
  };

  struct ModeInfo {
    bool operator==(const ModeInfo& other) const {
      return id == other.id && width == other.width && height == other.height &&
             dot_clock == other.dot_clock && hsync_start == other.hsync_start &&
             hsync_end == other.hsync_end && htotal == other.htotal &&
             hskew == other.hskew && vsync_start == other.vsync_start &&
             vsync_end == other.vsync_end && vtotal == other.vtotal &&
             name_len == other.name_len && mode_flags == other.mode_flags;
    }

    uint32_t id{};
    uint16_t width{};
    uint16_t height{};
    uint32_t dot_clock{};
    uint16_t hsync_start{};
    uint16_t hsync_end{};
    uint16_t htotal{};
    uint16_t hskew{};
    uint16_t vsync_start{};
    uint16_t vsync_end{};
    uint16_t vtotal{};
    uint16_t name_len{};
    ModeFlag mode_flags{};
  };

  struct ScreenChangeNotifyEvent {
    static constexpr uint8_t type_id = 3;
    static constexpr uint8_t opcode = 0;
    Rotation rotation{};
    uint16_t sequence{};
    Time timestamp{};
    Time config_timestamp{};
    Window root{};
    Window request_window{};
    uint16_t sizeID{};
    Render::SubPixel subpixel_order{};
    uint16_t width{};
    uint16_t height{};
    uint16_t mwidth{};
    uint16_t mheight{};
  };

  struct MonitorInfo {
    bool operator==(const MonitorInfo& other) const {
      return name == other.name && primary == other.primary &&
             automatic == other.automatic && x == other.x && y == other.y &&
             width == other.width && height == other.height &&
             width_in_millimeters == other.width_in_millimeters &&
             height_in_millimeters == other.height_in_millimeters &&
             outputs == other.outputs;
    }

    Atom name{};
    uint8_t primary{};
    uint8_t automatic{};
    int16_t x{};
    int16_t y{};
    uint16_t width{};
    uint16_t height{};
    uint32_t width_in_millimeters{};
    uint32_t height_in_millimeters{};
    std::vector<Output> outputs{};
  };

  struct NotifyEvent {
    static constexpr uint8_t type_id = 4;
    static constexpr uint8_t opcode = 1;
    uint16_t sequence{};
    struct Cc {
      Time timestamp{};
      Window window{};
      Crtc crtc{};
      Mode mode{};
      Rotation rotation{};
      int16_t x{};
      int16_t y{};
      uint16_t width{};
      uint16_t height{};
    };
    struct Oc {
      Time timestamp{};
      Time config_timestamp{};
      Window window{};
      Output output{};
      Crtc crtc{};
      Mode mode{};
      Rotation rotation{};
      RandRConnection connection{};
      Render::SubPixel subpixel_order{};
    };
    struct Op {
      Window window{};
      Output output{};
      Atom atom{};
      Time timestamp{};
      Property status{};
    };
    struct Pc {
      Time timestamp{};
      Window window{};
      Provider provider{};
    };
    struct Pp {
      Window window{};
      Provider provider{};
      Atom atom{};
      Time timestamp{};
      uint8_t state{};
    };
    struct Rc {
      Time timestamp{};
      Window window{};
    };
    struct Lc {
      Time timestamp{};
      Window window{};
      Lease lease{};
      uint8_t created{};
    };
    std::optional<Cc> cc{};
    std::optional<Oc> oc{};
    std::optional<Op> op{};
    std::optional<Pc> pc{};
    std::optional<Pp> pp{};
    std::optional<Rc> rc{};
    std::optional<Lc> lc{};
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

  struct SetScreenConfigRequest {
    Window window{};
    Time timestamp{};
    Time config_timestamp{};
    uint16_t sizeID{};
    Rotation rotation{};
    uint16_t rate{};
  };

  struct SetScreenConfigReply {
    SetConfig status{};
    uint16_t sequence{};
    Time new_timestamp{};
    Time config_timestamp{};
    Window root{};
    Render::SubPixel subpixel_order{};
  };

  using SetScreenConfigResponse = Response<SetScreenConfigReply>;

  Future<SetScreenConfigReply> SetScreenConfig(
      const SetScreenConfigRequest& request);

  Future<SetScreenConfigReply> SetScreenConfig(
      const Window& window = {},
      const Time& timestamp = {},
      const Time& config_timestamp = {},
      const uint16_t& sizeID = {},
      const Rotation& rotation = {},
      const uint16_t& rate = {});

  struct SelectInputRequest {
    Window window{};
    NotifyMask enable{};
  };

  using SelectInputResponse = Response<void>;

  Future<void> SelectInput(const SelectInputRequest& request);

  Future<void> SelectInput(const Window& window = {},
                           const NotifyMask& enable = {});

  struct GetScreenInfoRequest {
    Window window{};
  };

  struct GetScreenInfoReply {
    Rotation rotations{};
    uint16_t sequence{};
    Window root{};
    Time timestamp{};
    Time config_timestamp{};
    uint16_t sizeID{};
    Rotation rotation{};
    uint16_t rate{};
    uint16_t nInfo{};
    std::vector<ScreenSize> sizes{};
    std::vector<RefreshRates> rates{};
  };

  using GetScreenInfoResponse = Response<GetScreenInfoReply>;

  Future<GetScreenInfoReply> GetScreenInfo(const GetScreenInfoRequest& request);

  Future<GetScreenInfoReply> GetScreenInfo(const Window& window = {});

  struct GetScreenSizeRangeRequest {
    Window window{};
  };

  struct GetScreenSizeRangeReply {
    uint16_t sequence{};
    uint16_t min_width{};
    uint16_t min_height{};
    uint16_t max_width{};
    uint16_t max_height{};
  };

  using GetScreenSizeRangeResponse = Response<GetScreenSizeRangeReply>;

  Future<GetScreenSizeRangeReply> GetScreenSizeRange(
      const GetScreenSizeRangeRequest& request);

  Future<GetScreenSizeRangeReply> GetScreenSizeRange(const Window& window = {});

  struct SetScreenSizeRequest {
    Window window{};
    uint16_t width{};
    uint16_t height{};
    uint32_t mm_width{};
    uint32_t mm_height{};
  };

  using SetScreenSizeResponse = Response<void>;

  Future<void> SetScreenSize(const SetScreenSizeRequest& request);

  Future<void> SetScreenSize(const Window& window = {},
                             const uint16_t& width = {},
                             const uint16_t& height = {},
                             const uint32_t& mm_width = {},
                             const uint32_t& mm_height = {});

  struct GetScreenResourcesRequest {
    Window window{};
  };

  struct GetScreenResourcesReply {
    uint16_t sequence{};
    Time timestamp{};
    Time config_timestamp{};
    std::vector<Crtc> crtcs{};
    std::vector<Output> outputs{};
    std::vector<ModeInfo> modes{};
    std::vector<uint8_t> names{};
  };

  using GetScreenResourcesResponse = Response<GetScreenResourcesReply>;

  Future<GetScreenResourcesReply> GetScreenResources(
      const GetScreenResourcesRequest& request);

  Future<GetScreenResourcesReply> GetScreenResources(const Window& window = {});

  struct GetOutputInfoRequest {
    Output output{};
    Time config_timestamp{};
  };

  struct GetOutputInfoReply {
    SetConfig status{};
    uint16_t sequence{};
    Time timestamp{};
    Crtc crtc{};
    uint32_t mm_width{};
    uint32_t mm_height{};
    RandRConnection connection{};
    Render::SubPixel subpixel_order{};
    uint16_t num_preferred{};
    std::vector<Crtc> crtcs{};
    std::vector<Mode> modes{};
    std::vector<Output> clones{};
    std::vector<uint8_t> name{};
  };

  using GetOutputInfoResponse = Response<GetOutputInfoReply>;

  Future<GetOutputInfoReply> GetOutputInfo(const GetOutputInfoRequest& request);

  Future<GetOutputInfoReply> GetOutputInfo(const Output& output = {},
                                           const Time& config_timestamp = {});

  struct ListOutputPropertiesRequest {
    Output output{};
  };

  struct ListOutputPropertiesReply {
    uint16_t sequence{};
    std::vector<Atom> atoms{};
  };

  using ListOutputPropertiesResponse = Response<ListOutputPropertiesReply>;

  Future<ListOutputPropertiesReply> ListOutputProperties(
      const ListOutputPropertiesRequest& request);

  Future<ListOutputPropertiesReply> ListOutputProperties(
      const Output& output = {});

  struct QueryOutputPropertyRequest {
    Output output{};
    Atom property{};
  };

  struct QueryOutputPropertyReply {
    uint16_t sequence{};
    uint8_t pending{};
    uint8_t range{};
    uint8_t immutable{};
    std::vector<int32_t> validValues{};
  };

  using QueryOutputPropertyResponse = Response<QueryOutputPropertyReply>;

  Future<QueryOutputPropertyReply> QueryOutputProperty(
      const QueryOutputPropertyRequest& request);

  Future<QueryOutputPropertyReply> QueryOutputProperty(
      const Output& output = {},
      const Atom& property = {});

  struct ConfigureOutputPropertyRequest {
    Output output{};
    Atom property{};
    uint8_t pending{};
    uint8_t range{};
    std::vector<int32_t> values{};
  };

  using ConfigureOutputPropertyResponse = Response<void>;

  Future<void> ConfigureOutputProperty(
      const ConfigureOutputPropertyRequest& request);

  Future<void> ConfigureOutputProperty(const Output& output = {},
                                       const Atom& property = {},
                                       const uint8_t& pending = {},
                                       const uint8_t& range = {},
                                       const std::vector<int32_t>& values = {});

  struct ChangeOutputPropertyRequest {
    Output output{};
    Atom property{};
    Atom type{};
    uint8_t format{};
    PropMode mode{};
    uint32_t num_units{};
    scoped_refptr<base::RefCountedMemory> data{};
  };

  using ChangeOutputPropertyResponse = Response<void>;

  Future<void> ChangeOutputProperty(const ChangeOutputPropertyRequest& request);

  Future<void> ChangeOutputProperty(
      const Output& output = {},
      const Atom& property = {},
      const Atom& type = {},
      const uint8_t& format = {},
      const PropMode& mode = {},
      const uint32_t& num_units = {},
      const scoped_refptr<base::RefCountedMemory>& data = {});

  struct DeleteOutputPropertyRequest {
    Output output{};
    Atom property{};
  };

  using DeleteOutputPropertyResponse = Response<void>;

  Future<void> DeleteOutputProperty(const DeleteOutputPropertyRequest& request);

  Future<void> DeleteOutputProperty(const Output& output = {},
                                    const Atom& property = {});

  struct GetOutputPropertyRequest {
    Output output{};
    Atom property{};
    Atom type{};
    uint32_t long_offset{};
    uint32_t long_length{};
    uint8_t c_delete{};
    uint8_t pending{};
  };

  struct GetOutputPropertyReply {
    uint8_t format{};
    uint16_t sequence{};
    Atom type{};
    uint32_t bytes_after{};
    uint32_t num_items{};
    std::vector<uint8_t> data{};
  };

  using GetOutputPropertyResponse = Response<GetOutputPropertyReply>;

  Future<GetOutputPropertyReply> GetOutputProperty(
      const GetOutputPropertyRequest& request);

  Future<GetOutputPropertyReply> GetOutputProperty(
      const Output& output = {},
      const Atom& property = {},
      const Atom& type = {},
      const uint32_t& long_offset = {},
      const uint32_t& long_length = {},
      const uint8_t& c_delete = {},
      const uint8_t& pending = {});

  struct CreateModeRequest {
    Window window{};
    ModeInfo mode_info{};
    std::string name{};
  };

  struct CreateModeReply {
    uint16_t sequence{};
    Mode mode{};
  };

  using CreateModeResponse = Response<CreateModeReply>;

  Future<CreateModeReply> CreateMode(const CreateModeRequest& request);

  Future<CreateModeReply> CreateMode(
      const Window& window = {},
      const ModeInfo& mode_info =
          {{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}},
      const std::string& name = {});

  struct DestroyModeRequest {
    Mode mode{};
  };

  using DestroyModeResponse = Response<void>;

  Future<void> DestroyMode(const DestroyModeRequest& request);

  Future<void> DestroyMode(const Mode& mode = {});

  struct AddOutputModeRequest {
    Output output{};
    Mode mode{};
  };

  using AddOutputModeResponse = Response<void>;

  Future<void> AddOutputMode(const AddOutputModeRequest& request);

  Future<void> AddOutputMode(const Output& output = {}, const Mode& mode = {});

  struct DeleteOutputModeRequest {
    Output output{};
    Mode mode{};
  };

  using DeleteOutputModeResponse = Response<void>;

  Future<void> DeleteOutputMode(const DeleteOutputModeRequest& request);

  Future<void> DeleteOutputMode(const Output& output = {},
                                const Mode& mode = {});

  struct GetCrtcInfoRequest {
    Crtc crtc{};
    Time config_timestamp{};
  };

  struct GetCrtcInfoReply {
    SetConfig status{};
    uint16_t sequence{};
    Time timestamp{};
    int16_t x{};
    int16_t y{};
    uint16_t width{};
    uint16_t height{};
    Mode mode{};
    Rotation rotation{};
    Rotation rotations{};
    std::vector<Output> outputs{};
    std::vector<Output> possible{};
  };

  using GetCrtcInfoResponse = Response<GetCrtcInfoReply>;

  Future<GetCrtcInfoReply> GetCrtcInfo(const GetCrtcInfoRequest& request);

  Future<GetCrtcInfoReply> GetCrtcInfo(const Crtc& crtc = {},
                                       const Time& config_timestamp = {});

  struct SetCrtcConfigRequest {
    Crtc crtc{};
    Time timestamp{};
    Time config_timestamp{};
    int16_t x{};
    int16_t y{};
    Mode mode{};
    Rotation rotation{};
    std::vector<Output> outputs{};
  };

  struct SetCrtcConfigReply {
    SetConfig status{};
    uint16_t sequence{};
    Time timestamp{};
  };

  using SetCrtcConfigResponse = Response<SetCrtcConfigReply>;

  Future<SetCrtcConfigReply> SetCrtcConfig(const SetCrtcConfigRequest& request);

  Future<SetCrtcConfigReply> SetCrtcConfig(
      const Crtc& crtc = {},
      const Time& timestamp = {},
      const Time& config_timestamp = {},
      const int16_t& x = {},
      const int16_t& y = {},
      const Mode& mode = {},
      const Rotation& rotation = {},
      const std::vector<Output>& outputs = {});

  struct GetCrtcGammaSizeRequest {
    Crtc crtc{};
  };

  struct GetCrtcGammaSizeReply {
    uint16_t sequence{};
    uint16_t size{};
  };

  using GetCrtcGammaSizeResponse = Response<GetCrtcGammaSizeReply>;

  Future<GetCrtcGammaSizeReply> GetCrtcGammaSize(
      const GetCrtcGammaSizeRequest& request);

  Future<GetCrtcGammaSizeReply> GetCrtcGammaSize(const Crtc& crtc = {});

  struct GetCrtcGammaRequest {
    Crtc crtc{};
  };

  struct GetCrtcGammaReply {
    uint16_t sequence{};
    std::vector<uint16_t> red{};
    std::vector<uint16_t> green{};
    std::vector<uint16_t> blue{};
  };

  using GetCrtcGammaResponse = Response<GetCrtcGammaReply>;

  Future<GetCrtcGammaReply> GetCrtcGamma(const GetCrtcGammaRequest& request);

  Future<GetCrtcGammaReply> GetCrtcGamma(const Crtc& crtc = {});

  struct SetCrtcGammaRequest {
    Crtc crtc{};
    std::vector<uint16_t> red{};
    std::vector<uint16_t> green{};
    std::vector<uint16_t> blue{};
  };

  using SetCrtcGammaResponse = Response<void>;

  Future<void> SetCrtcGamma(const SetCrtcGammaRequest& request);

  Future<void> SetCrtcGamma(const Crtc& crtc = {},
                            const std::vector<uint16_t>& red = {},
                            const std::vector<uint16_t>& green = {},
                            const std::vector<uint16_t>& blue = {});

  struct GetScreenResourcesCurrentRequest {
    Window window{};
  };

  struct GetScreenResourcesCurrentReply {
    uint16_t sequence{};
    Time timestamp{};
    Time config_timestamp{};
    std::vector<Crtc> crtcs{};
    std::vector<Output> outputs{};
    std::vector<ModeInfo> modes{};
    std::vector<uint8_t> names{};
  };

  using GetScreenResourcesCurrentResponse =
      Response<GetScreenResourcesCurrentReply>;

  Future<GetScreenResourcesCurrentReply> GetScreenResourcesCurrent(
      const GetScreenResourcesCurrentRequest& request);

  Future<GetScreenResourcesCurrentReply> GetScreenResourcesCurrent(
      const Window& window = {});

  struct SetCrtcTransformRequest {
    Crtc crtc{};
    Render::Transform transform{};
    std::string filter_name{};
    std::vector<Render::Fixed> filter_params{};
  };

  using SetCrtcTransformResponse = Response<void>;

  Future<void> SetCrtcTransform(const SetCrtcTransformRequest& request);

  Future<void> SetCrtcTransform(
      const Crtc& crtc = {},
      const Render::Transform& transform = {{}, {}, {}, {}, {}, {}, {}, {}, {}},
      const std::string& filter_name = {},
      const std::vector<Render::Fixed>& filter_params = {});

  struct GetCrtcTransformRequest {
    Crtc crtc{};
  };

  struct GetCrtcTransformReply {
    uint16_t sequence{};
    Render::Transform pending_transform{};
    uint8_t has_transforms{};
    Render::Transform current_transform{};
    std::string pending_filter_name{};
    std::vector<Render::Fixed> pending_params{};
    std::string current_filter_name{};
    std::vector<Render::Fixed> current_params{};
  };

  using GetCrtcTransformResponse = Response<GetCrtcTransformReply>;

  Future<GetCrtcTransformReply> GetCrtcTransform(
      const GetCrtcTransformRequest& request);

  Future<GetCrtcTransformReply> GetCrtcTransform(const Crtc& crtc = {});

  struct GetPanningRequest {
    Crtc crtc{};
  };

  struct GetPanningReply {
    SetConfig status{};
    uint16_t sequence{};
    Time timestamp{};
    uint16_t left{};
    uint16_t top{};
    uint16_t width{};
    uint16_t height{};
    uint16_t track_left{};
    uint16_t track_top{};
    uint16_t track_width{};
    uint16_t track_height{};
    int16_t border_left{};
    int16_t border_top{};
    int16_t border_right{};
    int16_t border_bottom{};
  };

  using GetPanningResponse = Response<GetPanningReply>;

  Future<GetPanningReply> GetPanning(const GetPanningRequest& request);

  Future<GetPanningReply> GetPanning(const Crtc& crtc = {});

  struct SetPanningRequest {
    Crtc crtc{};
    Time timestamp{};
    uint16_t left{};
    uint16_t top{};
    uint16_t width{};
    uint16_t height{};
    uint16_t track_left{};
    uint16_t track_top{};
    uint16_t track_width{};
    uint16_t track_height{};
    int16_t border_left{};
    int16_t border_top{};
    int16_t border_right{};
    int16_t border_bottom{};
  };

  struct SetPanningReply {
    SetConfig status{};
    uint16_t sequence{};
    Time timestamp{};
  };

  using SetPanningResponse = Response<SetPanningReply>;

  Future<SetPanningReply> SetPanning(const SetPanningRequest& request);

  Future<SetPanningReply> SetPanning(const Crtc& crtc = {},
                                     const Time& timestamp = {},
                                     const uint16_t& left = {},
                                     const uint16_t& top = {},
                                     const uint16_t& width = {},
                                     const uint16_t& height = {},
                                     const uint16_t& track_left = {},
                                     const uint16_t& track_top = {},
                                     const uint16_t& track_width = {},
                                     const uint16_t& track_height = {},
                                     const int16_t& border_left = {},
                                     const int16_t& border_top = {},
                                     const int16_t& border_right = {},
                                     const int16_t& border_bottom = {});

  struct SetOutputPrimaryRequest {
    Window window{};
    Output output{};
  };

  using SetOutputPrimaryResponse = Response<void>;

  Future<void> SetOutputPrimary(const SetOutputPrimaryRequest& request);

  Future<void> SetOutputPrimary(const Window& window = {},
                                const Output& output = {});

  struct GetOutputPrimaryRequest {
    Window window{};
  };

  struct GetOutputPrimaryReply {
    uint16_t sequence{};
    Output output{};
  };

  using GetOutputPrimaryResponse = Response<GetOutputPrimaryReply>;

  Future<GetOutputPrimaryReply> GetOutputPrimary(
      const GetOutputPrimaryRequest& request);

  Future<GetOutputPrimaryReply> GetOutputPrimary(const Window& window = {});

  struct GetProvidersRequest {
    Window window{};
  };

  struct GetProvidersReply {
    uint16_t sequence{};
    Time timestamp{};
    std::vector<Provider> providers{};
  };

  using GetProvidersResponse = Response<GetProvidersReply>;

  Future<GetProvidersReply> GetProviders(const GetProvidersRequest& request);

  Future<GetProvidersReply> GetProviders(const Window& window = {});

  struct GetProviderInfoRequest {
    Provider provider{};
    Time config_timestamp{};
  };

  struct GetProviderInfoReply {
    uint8_t status{};
    uint16_t sequence{};
    Time timestamp{};
    ProviderCapability capabilities{};
    std::vector<Crtc> crtcs{};
    std::vector<Output> outputs{};
    std::vector<Provider> associated_providers{};
    std::vector<uint32_t> associated_capability{};
    std::string name{};
  };

  using GetProviderInfoResponse = Response<GetProviderInfoReply>;

  Future<GetProviderInfoReply> GetProviderInfo(
      const GetProviderInfoRequest& request);

  Future<GetProviderInfoReply> GetProviderInfo(
      const Provider& provider = {},
      const Time& config_timestamp = {});

  struct SetProviderOffloadSinkRequest {
    Provider provider{};
    Provider sink_provider{};
    Time config_timestamp{};
  };

  using SetProviderOffloadSinkResponse = Response<void>;

  Future<void> SetProviderOffloadSink(
      const SetProviderOffloadSinkRequest& request);

  Future<void> SetProviderOffloadSink(const Provider& provider = {},
                                      const Provider& sink_provider = {},
                                      const Time& config_timestamp = {});

  struct SetProviderOutputSourceRequest {
    Provider provider{};
    Provider source_provider{};
    Time config_timestamp{};
  };

  using SetProviderOutputSourceResponse = Response<void>;

  Future<void> SetProviderOutputSource(
      const SetProviderOutputSourceRequest& request);

  Future<void> SetProviderOutputSource(const Provider& provider = {},
                                       const Provider& source_provider = {},
                                       const Time& config_timestamp = {});

  struct ListProviderPropertiesRequest {
    Provider provider{};
  };

  struct ListProviderPropertiesReply {
    uint16_t sequence{};
    std::vector<Atom> atoms{};
  };

  using ListProviderPropertiesResponse = Response<ListProviderPropertiesReply>;

  Future<ListProviderPropertiesReply> ListProviderProperties(
      const ListProviderPropertiesRequest& request);

  Future<ListProviderPropertiesReply> ListProviderProperties(
      const Provider& provider = {});

  struct QueryProviderPropertyRequest {
    Provider provider{};
    Atom property{};
  };

  struct QueryProviderPropertyReply {
    uint16_t sequence{};
    uint8_t pending{};
    uint8_t range{};
    uint8_t immutable{};
    std::vector<int32_t> valid_values{};
  };

  using QueryProviderPropertyResponse = Response<QueryProviderPropertyReply>;

  Future<QueryProviderPropertyReply> QueryProviderProperty(
      const QueryProviderPropertyRequest& request);

  Future<QueryProviderPropertyReply> QueryProviderProperty(
      const Provider& provider = {},
      const Atom& property = {});

  struct ConfigureProviderPropertyRequest {
    Provider provider{};
    Atom property{};
    uint8_t pending{};
    uint8_t range{};
    std::vector<int32_t> values{};
  };

  using ConfigureProviderPropertyResponse = Response<void>;

  Future<void> ConfigureProviderProperty(
      const ConfigureProviderPropertyRequest& request);

  Future<void> ConfigureProviderProperty(
      const Provider& provider = {},
      const Atom& property = {},
      const uint8_t& pending = {},
      const uint8_t& range = {},
      const std::vector<int32_t>& values = {});

  struct ChangeProviderPropertyRequest {
    Provider provider{};
    Atom property{};
    Atom type{};
    uint8_t format{};
    uint8_t mode{};
    uint32_t num_items{};
    scoped_refptr<base::RefCountedMemory> data{};
  };

  using ChangeProviderPropertyResponse = Response<void>;

  Future<void> ChangeProviderProperty(
      const ChangeProviderPropertyRequest& request);

  Future<void> ChangeProviderProperty(
      const Provider& provider = {},
      const Atom& property = {},
      const Atom& type = {},
      const uint8_t& format = {},
      const uint8_t& mode = {},
      const uint32_t& num_items = {},
      const scoped_refptr<base::RefCountedMemory>& data = {});

  struct DeleteProviderPropertyRequest {
    Provider provider{};
    Atom property{};
  };

  using DeleteProviderPropertyResponse = Response<void>;

  Future<void> DeleteProviderProperty(
      const DeleteProviderPropertyRequest& request);

  Future<void> DeleteProviderProperty(const Provider& provider = {},
                                      const Atom& property = {});

  struct GetProviderPropertyRequest {
    Provider provider{};
    Atom property{};
    Atom type{};
    uint32_t long_offset{};
    uint32_t long_length{};
    uint8_t c_delete{};
    uint8_t pending{};
  };

  struct GetProviderPropertyReply {
    uint8_t format{};
    uint16_t sequence{};
    Atom type{};
    uint32_t bytes_after{};
    uint32_t num_items{};
    scoped_refptr<UnsizedRefCountedMemory> data{};
  };

  using GetProviderPropertyResponse = Response<GetProviderPropertyReply>;

  Future<GetProviderPropertyReply> GetProviderProperty(
      const GetProviderPropertyRequest& request);

  Future<GetProviderPropertyReply> GetProviderProperty(
      const Provider& provider = {},
      const Atom& property = {},
      const Atom& type = {},
      const uint32_t& long_offset = {},
      const uint32_t& long_length = {},
      const uint8_t& c_delete = {},
      const uint8_t& pending = {});

  struct GetMonitorsRequest {
    Window window{};
    uint8_t get_active{};
  };

  struct GetMonitorsReply {
    uint16_t sequence{};
    Time timestamp{};
    uint32_t nOutputs{};
    std::vector<MonitorInfo> monitors{};
  };

  using GetMonitorsResponse = Response<GetMonitorsReply>;

  Future<GetMonitorsReply> GetMonitors(const GetMonitorsRequest& request);

  Future<GetMonitorsReply> GetMonitors(const Window& window = {},
                                       const uint8_t& get_active = {});

  struct SetMonitorRequest {
    Window window{};
    MonitorInfo monitorinfo{};
  };

  using SetMonitorResponse = Response<void>;

  Future<void> SetMonitor(const SetMonitorRequest& request);

  Future<void> SetMonitor(const Window& window = {},
                          const MonitorInfo& monitorinfo =
                              {{}, {}, {}, {}, {}, {}, {}, {}, {}, {}});

  struct DeleteMonitorRequest {
    Window window{};
    Atom name{};
  };

  using DeleteMonitorResponse = Response<void>;

  Future<void> DeleteMonitor(const DeleteMonitorRequest& request);

  Future<void> DeleteMonitor(const Window& window = {}, const Atom& name = {});

  struct CreateLeaseRequest {
    Window window{};
    Lease lid{};
    std::vector<Crtc> crtcs{};
    std::vector<Output> outputs{};
  };

  struct CreateLeaseReply {
    uint8_t nfd{};
    uint16_t sequence{};
    RefCountedFD master_fd{};
  };

  using CreateLeaseResponse = Response<CreateLeaseReply>;

  Future<CreateLeaseReply> CreateLease(const CreateLeaseRequest& request);

  Future<CreateLeaseReply> CreateLease(const Window& window = {},
                                       const Lease& lid = {},
                                       const std::vector<Crtc>& crtcs = {},
                                       const std::vector<Output>& outputs = {});

  struct FreeLeaseRequest {
    Lease lid{};
    uint8_t terminate{};
  };

  using FreeLeaseResponse = Response<void>;

  Future<void> FreeLease(const FreeLeaseRequest& request);

  Future<void> FreeLease(const Lease& lid = {}, const uint8_t& terminate = {});

 private:
  Connection* const connection_;
  x11::QueryExtensionReply info_{};
};

}  // namespace x11

inline constexpr x11::RandR::Rotation operator|(x11::RandR::Rotation l,
                                                x11::RandR::Rotation r) {
  using T = std::underlying_type_t<x11::RandR::Rotation>;
  return static_cast<x11::RandR::Rotation>(static_cast<T>(l) |
                                           static_cast<T>(r));
}

inline constexpr x11::RandR::Rotation operator&(x11::RandR::Rotation l,
                                                x11::RandR::Rotation r) {
  using T = std::underlying_type_t<x11::RandR::Rotation>;
  return static_cast<x11::RandR::Rotation>(static_cast<T>(l) &
                                           static_cast<T>(r));
}

inline constexpr x11::RandR::SetConfig operator|(x11::RandR::SetConfig l,
                                                 x11::RandR::SetConfig r) {
  using T = std::underlying_type_t<x11::RandR::SetConfig>;
  return static_cast<x11::RandR::SetConfig>(static_cast<T>(l) |
                                            static_cast<T>(r));
}

inline constexpr x11::RandR::SetConfig operator&(x11::RandR::SetConfig l,
                                                 x11::RandR::SetConfig r) {
  using T = std::underlying_type_t<x11::RandR::SetConfig>;
  return static_cast<x11::RandR::SetConfig>(static_cast<T>(l) &
                                            static_cast<T>(r));
}

inline constexpr x11::RandR::NotifyMask operator|(x11::RandR::NotifyMask l,
                                                  x11::RandR::NotifyMask r) {
  using T = std::underlying_type_t<x11::RandR::NotifyMask>;
  return static_cast<x11::RandR::NotifyMask>(static_cast<T>(l) |
                                             static_cast<T>(r));
}

inline constexpr x11::RandR::NotifyMask operator&(x11::RandR::NotifyMask l,
                                                  x11::RandR::NotifyMask r) {
  using T = std::underlying_type_t<x11::RandR::NotifyMask>;
  return static_cast<x11::RandR::NotifyMask>(static_cast<T>(l) &
                                             static_cast<T>(r));
}

inline constexpr x11::RandR::ModeFlag operator|(x11::RandR::ModeFlag l,
                                                x11::RandR::ModeFlag r) {
  using T = std::underlying_type_t<x11::RandR::ModeFlag>;
  return static_cast<x11::RandR::ModeFlag>(static_cast<T>(l) |
                                           static_cast<T>(r));
}

inline constexpr x11::RandR::ModeFlag operator&(x11::RandR::ModeFlag l,
                                                x11::RandR::ModeFlag r) {
  using T = std::underlying_type_t<x11::RandR::ModeFlag>;
  return static_cast<x11::RandR::ModeFlag>(static_cast<T>(l) &
                                           static_cast<T>(r));
}

inline constexpr x11::RandR::RandRConnection operator|(
    x11::RandR::RandRConnection l,
    x11::RandR::RandRConnection r) {
  using T = std::underlying_type_t<x11::RandR::RandRConnection>;
  return static_cast<x11::RandR::RandRConnection>(static_cast<T>(l) |
                                                  static_cast<T>(r));
}

inline constexpr x11::RandR::RandRConnection operator&(
    x11::RandR::RandRConnection l,
    x11::RandR::RandRConnection r) {
  using T = std::underlying_type_t<x11::RandR::RandRConnection>;
  return static_cast<x11::RandR::RandRConnection>(static_cast<T>(l) &
                                                  static_cast<T>(r));
}

inline constexpr x11::RandR::Transform operator|(x11::RandR::Transform l,
                                                 x11::RandR::Transform r) {
  using T = std::underlying_type_t<x11::RandR::Transform>;
  return static_cast<x11::RandR::Transform>(static_cast<T>(l) |
                                            static_cast<T>(r));
}

inline constexpr x11::RandR::Transform operator&(x11::RandR::Transform l,
                                                 x11::RandR::Transform r) {
  using T = std::underlying_type_t<x11::RandR::Transform>;
  return static_cast<x11::RandR::Transform>(static_cast<T>(l) &
                                            static_cast<T>(r));
}

inline constexpr x11::RandR::ProviderCapability operator|(
    x11::RandR::ProviderCapability l,
    x11::RandR::ProviderCapability r) {
  using T = std::underlying_type_t<x11::RandR::ProviderCapability>;
  return static_cast<x11::RandR::ProviderCapability>(static_cast<T>(l) |
                                                     static_cast<T>(r));
}

inline constexpr x11::RandR::ProviderCapability operator&(
    x11::RandR::ProviderCapability l,
    x11::RandR::ProviderCapability r) {
  using T = std::underlying_type_t<x11::RandR::ProviderCapability>;
  return static_cast<x11::RandR::ProviderCapability>(static_cast<T>(l) &
                                                     static_cast<T>(r));
}

inline constexpr x11::RandR::Notify operator|(x11::RandR::Notify l,
                                              x11::RandR::Notify r) {
  using T = std::underlying_type_t<x11::RandR::Notify>;
  return static_cast<x11::RandR::Notify>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::RandR::Notify operator&(x11::RandR::Notify l,
                                              x11::RandR::Notify r) {
  using T = std::underlying_type_t<x11::RandR::Notify>;
  return static_cast<x11::RandR::Notify>(static_cast<T>(l) & static_cast<T>(r));
}

#endif  // UI_GFX_X_GENERATED_PROTOS_RANDR_H_
