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

#ifndef UI_GFX_X_GENERATED_PROTOS_XF86VIDMODE_H_
#define UI_GFX_X_GENERATED_PROTOS_XF86VIDMODE_H_

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

class COMPONENT_EXPORT(X11) XF86VidMode {
 public:
  static constexpr unsigned major_version = 2;
  static constexpr unsigned minor_version = 2;

  XF86VidMode(Connection* connection, const x11::QueryExtensionReply& info);

  uint8_t present() const { return info_.present; }
  uint8_t major_opcode() const { return info_.major_opcode; }
  uint8_t first_event() const { return info_.first_event; }
  uint8_t first_error() const { return info_.first_error; }

  Connection* connection() const { return connection_; }

  enum class Syncrange : uint32_t {};

  enum class DotClock : uint32_t {};

  enum class ModeFlag : int {
    Positive_HSync = 1 << 0,
    Negative_HSync = 1 << 1,
    Positive_VSync = 1 << 2,
    Negative_VSync = 1 << 3,
    Interlace = 1 << 4,
    Composite_Sync = 1 << 5,
    Positive_CSync = 1 << 6,
    Negative_CSync = 1 << 7,
    HSkew = 1 << 8,
    Broadcast = 1 << 9,
    Pixmux = 1 << 10,
    Double_Clock = 1 << 11,
    Half_Clock = 1 << 12,
  };

  enum class ClockFlag : int {
    Programable = 1 << 0,
  };

  enum class Permission : int {
    Read = 1 << 0,
    Write = 1 << 1,
  };

  struct ModeInfo {
    bool operator==(const ModeInfo& other) const {
      return dotclock == other.dotclock && hdisplay == other.hdisplay &&
             hsyncstart == other.hsyncstart && hsyncend == other.hsyncend &&
             htotal == other.htotal && hskew == other.hskew &&
             vdisplay == other.vdisplay && vsyncstart == other.vsyncstart &&
             vsyncend == other.vsyncend && vtotal == other.vtotal &&
             flags == other.flags && privsize == other.privsize;
    }

    DotClock dotclock{};
    uint16_t hdisplay{};
    uint16_t hsyncstart{};
    uint16_t hsyncend{};
    uint16_t htotal{};
    uint32_t hskew{};
    uint16_t vdisplay{};
    uint16_t vsyncstart{};
    uint16_t vsyncend{};
    uint16_t vtotal{};
    ModeFlag flags{};
    uint32_t privsize{};
  };

  struct BadClockError : public x11::Error {
    uint16_t sequence{};
    uint32_t bad_value{};
    uint16_t minor_opcode{};
    uint8_t major_opcode{};

    std::string ToString() const override;
  };

  struct BadHTimingsError : public x11::Error {
    uint16_t sequence{};
    uint32_t bad_value{};
    uint16_t minor_opcode{};
    uint8_t major_opcode{};

    std::string ToString() const override;
  };

  struct BadVTimingsError : public x11::Error {
    uint16_t sequence{};
    uint32_t bad_value{};
    uint16_t minor_opcode{};
    uint8_t major_opcode{};

    std::string ToString() const override;
  };

  struct ModeUnsuitableError : public x11::Error {
    uint16_t sequence{};
    uint32_t bad_value{};
    uint16_t minor_opcode{};
    uint8_t major_opcode{};

    std::string ToString() const override;
  };

  struct ExtensionDisabledError : public x11::Error {
    uint16_t sequence{};
    uint32_t bad_value{};
    uint16_t minor_opcode{};
    uint8_t major_opcode{};

    std::string ToString() const override;
  };

  struct ClientNotLocalError : public x11::Error {
    uint16_t sequence{};
    uint32_t bad_value{};
    uint16_t minor_opcode{};
    uint8_t major_opcode{};

    std::string ToString() const override;
  };

  struct ZoomLockedError : public x11::Error {
    uint16_t sequence{};
    uint32_t bad_value{};
    uint16_t minor_opcode{};
    uint8_t major_opcode{};

    std::string ToString() const override;
  };

  struct QueryVersionRequest {};

  struct QueryVersionReply {
    uint16_t sequence{};
    uint16_t major_version{};
    uint16_t minor_version{};
  };

  using QueryVersionResponse = Response<QueryVersionReply>;

  Future<QueryVersionReply> QueryVersion(const QueryVersionRequest& request);

  Future<QueryVersionReply> QueryVersion();

  struct GetModeLineRequest {
    uint16_t screen{};
  };

  struct GetModeLineReply {
    uint16_t sequence{};
    DotClock dotclock{};
    uint16_t hdisplay{};
    uint16_t hsyncstart{};
    uint16_t hsyncend{};
    uint16_t htotal{};
    uint16_t hskew{};
    uint16_t vdisplay{};
    uint16_t vsyncstart{};
    uint16_t vsyncend{};
    uint16_t vtotal{};
    ModeFlag flags{};
    std::vector<uint8_t> c_private{};
  };

  using GetModeLineResponse = Response<GetModeLineReply>;

  Future<GetModeLineReply> GetModeLine(const GetModeLineRequest& request);

  Future<GetModeLineReply> GetModeLine(const uint16_t& screen = {});

  struct ModModeLineRequest {
    uint32_t screen{};
    uint16_t hdisplay{};
    uint16_t hsyncstart{};
    uint16_t hsyncend{};
    uint16_t htotal{};
    uint16_t hskew{};
    uint16_t vdisplay{};
    uint16_t vsyncstart{};
    uint16_t vsyncend{};
    uint16_t vtotal{};
    ModeFlag flags{};
    std::vector<uint8_t> c_private{};
  };

  using ModModeLineResponse = Response<void>;

  Future<void> ModModeLine(const ModModeLineRequest& request);

  Future<void> ModModeLine(const uint32_t& screen = {},
                           const uint16_t& hdisplay = {},
                           const uint16_t& hsyncstart = {},
                           const uint16_t& hsyncend = {},
                           const uint16_t& htotal = {},
                           const uint16_t& hskew = {},
                           const uint16_t& vdisplay = {},
                           const uint16_t& vsyncstart = {},
                           const uint16_t& vsyncend = {},
                           const uint16_t& vtotal = {},
                           const ModeFlag& flags = {},
                           const std::vector<uint8_t>& c_private = {});

  struct SwitchModeRequest {
    uint16_t screen{};
    uint16_t zoom{};
  };

  using SwitchModeResponse = Response<void>;

  Future<void> SwitchMode(const SwitchModeRequest& request);

  Future<void> SwitchMode(const uint16_t& screen = {},
                          const uint16_t& zoom = {});

  struct GetMonitorRequest {
    uint16_t screen{};
  };

  struct GetMonitorReply {
    uint16_t sequence{};
    std::vector<Syncrange> hsync{};
    std::vector<Syncrange> vsync{};
    std::string vendor{};
    scoped_refptr<base::RefCountedMemory> alignment_pad{};
    std::string model{};
  };

  using GetMonitorResponse = Response<GetMonitorReply>;

  Future<GetMonitorReply> GetMonitor(const GetMonitorRequest& request);

  Future<GetMonitorReply> GetMonitor(const uint16_t& screen = {});

  struct LockModeSwitchRequest {
    uint16_t screen{};
    uint16_t lock{};
  };

  using LockModeSwitchResponse = Response<void>;

  Future<void> LockModeSwitch(const LockModeSwitchRequest& request);

  Future<void> LockModeSwitch(const uint16_t& screen = {},
                              const uint16_t& lock = {});

  struct GetAllModeLinesRequest {
    uint16_t screen{};
  };

  struct GetAllModeLinesReply {
    uint16_t sequence{};
    std::vector<ModeInfo> modeinfo{};
  };

  using GetAllModeLinesResponse = Response<GetAllModeLinesReply>;

  Future<GetAllModeLinesReply> GetAllModeLines(
      const GetAllModeLinesRequest& request);

  Future<GetAllModeLinesReply> GetAllModeLines(const uint16_t& screen = {});

  struct AddModeLineRequest {
    uint32_t screen{};
    DotClock dotclock{};
    uint16_t hdisplay{};
    uint16_t hsyncstart{};
    uint16_t hsyncend{};
    uint16_t htotal{};
    uint16_t hskew{};
    uint16_t vdisplay{};
    uint16_t vsyncstart{};
    uint16_t vsyncend{};
    uint16_t vtotal{};
    ModeFlag flags{};
    DotClock after_dotclock{};
    uint16_t after_hdisplay{};
    uint16_t after_hsyncstart{};
    uint16_t after_hsyncend{};
    uint16_t after_htotal{};
    uint16_t after_hskew{};
    uint16_t after_vdisplay{};
    uint16_t after_vsyncstart{};
    uint16_t after_vsyncend{};
    uint16_t after_vtotal{};
    ModeFlag after_flags{};
    std::vector<uint8_t> c_private{};
  };

  using AddModeLineResponse = Response<void>;

  Future<void> AddModeLine(const AddModeLineRequest& request);

  Future<void> AddModeLine(const uint32_t& screen = {},
                           const DotClock& dotclock = {},
                           const uint16_t& hdisplay = {},
                           const uint16_t& hsyncstart = {},
                           const uint16_t& hsyncend = {},
                           const uint16_t& htotal = {},
                           const uint16_t& hskew = {},
                           const uint16_t& vdisplay = {},
                           const uint16_t& vsyncstart = {},
                           const uint16_t& vsyncend = {},
                           const uint16_t& vtotal = {},
                           const ModeFlag& flags = {},
                           const DotClock& after_dotclock = {},
                           const uint16_t& after_hdisplay = {},
                           const uint16_t& after_hsyncstart = {},
                           const uint16_t& after_hsyncend = {},
                           const uint16_t& after_htotal = {},
                           const uint16_t& after_hskew = {},
                           const uint16_t& after_vdisplay = {},
                           const uint16_t& after_vsyncstart = {},
                           const uint16_t& after_vsyncend = {},
                           const uint16_t& after_vtotal = {},
                           const ModeFlag& after_flags = {},
                           const std::vector<uint8_t>& c_private = {});

  struct DeleteModeLineRequest {
    uint32_t screen{};
    DotClock dotclock{};
    uint16_t hdisplay{};
    uint16_t hsyncstart{};
    uint16_t hsyncend{};
    uint16_t htotal{};
    uint16_t hskew{};
    uint16_t vdisplay{};
    uint16_t vsyncstart{};
    uint16_t vsyncend{};
    uint16_t vtotal{};
    ModeFlag flags{};
    std::vector<uint8_t> c_private{};
  };

  using DeleteModeLineResponse = Response<void>;

  Future<void> DeleteModeLine(const DeleteModeLineRequest& request);

  Future<void> DeleteModeLine(const uint32_t& screen = {},
                              const DotClock& dotclock = {},
                              const uint16_t& hdisplay = {},
                              const uint16_t& hsyncstart = {},
                              const uint16_t& hsyncend = {},
                              const uint16_t& htotal = {},
                              const uint16_t& hskew = {},
                              const uint16_t& vdisplay = {},
                              const uint16_t& vsyncstart = {},
                              const uint16_t& vsyncend = {},
                              const uint16_t& vtotal = {},
                              const ModeFlag& flags = {},
                              const std::vector<uint8_t>& c_private = {});

  struct ValidateModeLineRequest {
    uint32_t screen{};
    DotClock dotclock{};
    uint16_t hdisplay{};
    uint16_t hsyncstart{};
    uint16_t hsyncend{};
    uint16_t htotal{};
    uint16_t hskew{};
    uint16_t vdisplay{};
    uint16_t vsyncstart{};
    uint16_t vsyncend{};
    uint16_t vtotal{};
    ModeFlag flags{};
    std::vector<uint8_t> c_private{};
  };

  struct ValidateModeLineReply {
    uint16_t sequence{};
    uint32_t status{};
  };

  using ValidateModeLineResponse = Response<ValidateModeLineReply>;

  Future<ValidateModeLineReply> ValidateModeLine(
      const ValidateModeLineRequest& request);

  Future<ValidateModeLineReply> ValidateModeLine(
      const uint32_t& screen = {},
      const DotClock& dotclock = {},
      const uint16_t& hdisplay = {},
      const uint16_t& hsyncstart = {},
      const uint16_t& hsyncend = {},
      const uint16_t& htotal = {},
      const uint16_t& hskew = {},
      const uint16_t& vdisplay = {},
      const uint16_t& vsyncstart = {},
      const uint16_t& vsyncend = {},
      const uint16_t& vtotal = {},
      const ModeFlag& flags = {},
      const std::vector<uint8_t>& c_private = {});

  struct SwitchToModeRequest {
    uint32_t screen{};
    DotClock dotclock{};
    uint16_t hdisplay{};
    uint16_t hsyncstart{};
    uint16_t hsyncend{};
    uint16_t htotal{};
    uint16_t hskew{};
    uint16_t vdisplay{};
    uint16_t vsyncstart{};
    uint16_t vsyncend{};
    uint16_t vtotal{};
    ModeFlag flags{};
    std::vector<uint8_t> c_private{};
  };

  using SwitchToModeResponse = Response<void>;

  Future<void> SwitchToMode(const SwitchToModeRequest& request);

  Future<void> SwitchToMode(const uint32_t& screen = {},
                            const DotClock& dotclock = {},
                            const uint16_t& hdisplay = {},
                            const uint16_t& hsyncstart = {},
                            const uint16_t& hsyncend = {},
                            const uint16_t& htotal = {},
                            const uint16_t& hskew = {},
                            const uint16_t& vdisplay = {},
                            const uint16_t& vsyncstart = {},
                            const uint16_t& vsyncend = {},
                            const uint16_t& vtotal = {},
                            const ModeFlag& flags = {},
                            const std::vector<uint8_t>& c_private = {});

  struct GetViewPortRequest {
    uint16_t screen{};
  };

  struct GetViewPortReply {
    uint16_t sequence{};
    uint32_t x{};
    uint32_t y{};
  };

  using GetViewPortResponse = Response<GetViewPortReply>;

  Future<GetViewPortReply> GetViewPort(const GetViewPortRequest& request);

  Future<GetViewPortReply> GetViewPort(const uint16_t& screen = {});

  struct SetViewPortRequest {
    uint16_t screen{};
    uint32_t x{};
    uint32_t y{};
  };

  using SetViewPortResponse = Response<void>;

  Future<void> SetViewPort(const SetViewPortRequest& request);

  Future<void> SetViewPort(const uint16_t& screen = {},
                           const uint32_t& x = {},
                           const uint32_t& y = {});

  struct GetDotClocksRequest {
    uint16_t screen{};
  };

  struct GetDotClocksReply {
    uint16_t sequence{};
    ClockFlag flags{};
    uint32_t clocks{};
    uint32_t maxclocks{};
    std::vector<uint32_t> clock{};
  };

  using GetDotClocksResponse = Response<GetDotClocksReply>;

  Future<GetDotClocksReply> GetDotClocks(const GetDotClocksRequest& request);

  Future<GetDotClocksReply> GetDotClocks(const uint16_t& screen = {});

  struct SetClientVersionRequest {
    uint16_t major{};
    uint16_t minor{};
  };

  using SetClientVersionResponse = Response<void>;

  Future<void> SetClientVersion(const SetClientVersionRequest& request);

  Future<void> SetClientVersion(const uint16_t& major = {},
                                const uint16_t& minor = {});

  struct SetGammaRequest {
    uint16_t screen{};
    uint32_t red{};
    uint32_t green{};
    uint32_t blue{};
  };

  using SetGammaResponse = Response<void>;

  Future<void> SetGamma(const SetGammaRequest& request);

  Future<void> SetGamma(const uint16_t& screen = {},
                        const uint32_t& red = {},
                        const uint32_t& green = {},
                        const uint32_t& blue = {});

  struct GetGammaRequest {
    uint16_t screen{};
  };

  struct GetGammaReply {
    uint16_t sequence{};
    uint32_t red{};
    uint32_t green{};
    uint32_t blue{};
  };

  using GetGammaResponse = Response<GetGammaReply>;

  Future<GetGammaReply> GetGamma(const GetGammaRequest& request);

  Future<GetGammaReply> GetGamma(const uint16_t& screen = {});

  struct GetGammaRampRequest {
    uint16_t screen{};
    uint16_t size{};
  };

  struct GetGammaRampReply {
    uint16_t sequence{};
    uint16_t size{};
    std::vector<uint16_t> red{};
    std::vector<uint16_t> green{};
    std::vector<uint16_t> blue{};
  };

  using GetGammaRampResponse = Response<GetGammaRampReply>;

  Future<GetGammaRampReply> GetGammaRamp(const GetGammaRampRequest& request);

  Future<GetGammaRampReply> GetGammaRamp(const uint16_t& screen = {},
                                         const uint16_t& size = {});

  struct SetGammaRampRequest {
    uint16_t screen{};
    uint16_t size{};
    std::vector<uint16_t> red{};
    std::vector<uint16_t> green{};
    std::vector<uint16_t> blue{};
  };

  using SetGammaRampResponse = Response<void>;

  Future<void> SetGammaRamp(const SetGammaRampRequest& request);

  Future<void> SetGammaRamp(const uint16_t& screen = {},
                            const uint16_t& size = {},
                            const std::vector<uint16_t>& red = {},
                            const std::vector<uint16_t>& green = {},
                            const std::vector<uint16_t>& blue = {});

  struct GetGammaRampSizeRequest {
    uint16_t screen{};
  };

  struct GetGammaRampSizeReply {
    uint16_t sequence{};
    uint16_t size{};
  };

  using GetGammaRampSizeResponse = Response<GetGammaRampSizeReply>;

  Future<GetGammaRampSizeReply> GetGammaRampSize(
      const GetGammaRampSizeRequest& request);

  Future<GetGammaRampSizeReply> GetGammaRampSize(const uint16_t& screen = {});

  struct GetPermissionsRequest {
    uint16_t screen{};
  };

  struct GetPermissionsReply {
    uint16_t sequence{};
    Permission permissions{};
  };

  using GetPermissionsResponse = Response<GetPermissionsReply>;

  Future<GetPermissionsReply> GetPermissions(
      const GetPermissionsRequest& request);

  Future<GetPermissionsReply> GetPermissions(const uint16_t& screen = {});

 private:
  Connection* const connection_;
  x11::QueryExtensionReply info_{};
};

}  // namespace x11

inline constexpr x11::XF86VidMode::ModeFlag operator|(
    x11::XF86VidMode::ModeFlag l,
    x11::XF86VidMode::ModeFlag r) {
  using T = std::underlying_type_t<x11::XF86VidMode::ModeFlag>;
  return static_cast<x11::XF86VidMode::ModeFlag>(static_cast<T>(l) |
                                                 static_cast<T>(r));
}

inline constexpr x11::XF86VidMode::ModeFlag operator&(
    x11::XF86VidMode::ModeFlag l,
    x11::XF86VidMode::ModeFlag r) {
  using T = std::underlying_type_t<x11::XF86VidMode::ModeFlag>;
  return static_cast<x11::XF86VidMode::ModeFlag>(static_cast<T>(l) &
                                                 static_cast<T>(r));
}

inline constexpr x11::XF86VidMode::ClockFlag operator|(
    x11::XF86VidMode::ClockFlag l,
    x11::XF86VidMode::ClockFlag r) {
  using T = std::underlying_type_t<x11::XF86VidMode::ClockFlag>;
  return static_cast<x11::XF86VidMode::ClockFlag>(static_cast<T>(l) |
                                                  static_cast<T>(r));
}

inline constexpr x11::XF86VidMode::ClockFlag operator&(
    x11::XF86VidMode::ClockFlag l,
    x11::XF86VidMode::ClockFlag r) {
  using T = std::underlying_type_t<x11::XF86VidMode::ClockFlag>;
  return static_cast<x11::XF86VidMode::ClockFlag>(static_cast<T>(l) &
                                                  static_cast<T>(r));
}

inline constexpr x11::XF86VidMode::Permission operator|(
    x11::XF86VidMode::Permission l,
    x11::XF86VidMode::Permission r) {
  using T = std::underlying_type_t<x11::XF86VidMode::Permission>;
  return static_cast<x11::XF86VidMode::Permission>(static_cast<T>(l) |
                                                   static_cast<T>(r));
}

inline constexpr x11::XF86VidMode::Permission operator&(
    x11::XF86VidMode::Permission l,
    x11::XF86VidMode::Permission r) {
  using T = std::underlying_type_t<x11::XF86VidMode::Permission>;
  return static_cast<x11::XF86VidMode::Permission>(static_cast<T>(l) &
                                                   static_cast<T>(r));
}

#endif  // UI_GFX_X_GENERATED_PROTOS_XF86VIDMODE_H_
