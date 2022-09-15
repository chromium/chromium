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

#ifndef UI_GFX_X_GENERATED_PROTOS_DPMS_H_
#define UI_GFX_X_GENERATED_PROTOS_DPMS_H_

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

class COMPONENT_EXPORT(X11) Dpms {
 public:
  static constexpr unsigned major_version = 0;
  static constexpr unsigned minor_version = 0;

  Dpms(Connection* connection, const x11::QueryExtensionReply& info);

  uint8_t present() const { return info_.present; }
  uint8_t major_opcode() const { return info_.major_opcode; }
  uint8_t first_event() const { return info_.first_event; }
  uint8_t first_error() const { return info_.first_error; }

  Connection* connection() const { return connection_; }

  enum class DPMSMode : int {
    On = 0,
    Standby = 1,
    Suspend = 2,
    Off = 3,
  };

  struct GetVersionRequest {
    uint16_t client_major_version{};
    uint16_t client_minor_version{};
  };

  struct GetVersionReply {
    uint16_t sequence{};
    uint16_t server_major_version{};
    uint16_t server_minor_version{};
  };

  using GetVersionResponse = Response<GetVersionReply>;

  Future<GetVersionReply> GetVersion(const GetVersionRequest& request);

  Future<GetVersionReply> GetVersion(const uint16_t& client_major_version = {},
                                     const uint16_t& client_minor_version = {});

  struct CapableRequest {};

  struct CapableReply {
    uint16_t sequence{};
    uint8_t capable{};
  };

  using CapableResponse = Response<CapableReply>;

  Future<CapableReply> Capable(const CapableRequest& request);

  Future<CapableReply> Capable();

  struct GetTimeoutsRequest {};

  struct GetTimeoutsReply {
    uint16_t sequence{};
    uint16_t standby_timeout{};
    uint16_t suspend_timeout{};
    uint16_t off_timeout{};
  };

  using GetTimeoutsResponse = Response<GetTimeoutsReply>;

  Future<GetTimeoutsReply> GetTimeouts(const GetTimeoutsRequest& request);

  Future<GetTimeoutsReply> GetTimeouts();

  struct SetTimeoutsRequest {
    uint16_t standby_timeout{};
    uint16_t suspend_timeout{};
    uint16_t off_timeout{};
  };

  using SetTimeoutsResponse = Response<void>;

  Future<void> SetTimeouts(const SetTimeoutsRequest& request);

  Future<void> SetTimeouts(const uint16_t& standby_timeout = {},
                           const uint16_t& suspend_timeout = {},
                           const uint16_t& off_timeout = {});

  struct EnableRequest {};

  using EnableResponse = Response<void>;

  Future<void> Enable(const EnableRequest& request);

  Future<void> Enable();

  struct DisableRequest {};

  using DisableResponse = Response<void>;

  Future<void> Disable(const DisableRequest& request);

  Future<void> Disable();

  struct ForceLevelRequest {
    DPMSMode power_level{};
  };

  using ForceLevelResponse = Response<void>;

  Future<void> ForceLevel(const ForceLevelRequest& request);

  Future<void> ForceLevel(const DPMSMode& power_level = {});

  struct InfoRequest {};

  struct InfoReply {
    uint16_t sequence{};
    DPMSMode power_level{};
    uint8_t state{};
  };

  using InfoResponse = Response<InfoReply>;

  Future<InfoReply> Info(const InfoRequest& request);

  Future<InfoReply> Info();

 private:
  Connection* const connection_;
  x11::QueryExtensionReply info_{};
};

}  // namespace x11

inline constexpr x11::Dpms::DPMSMode operator|(x11::Dpms::DPMSMode l,
                                               x11::Dpms::DPMSMode r) {
  using T = std::underlying_type_t<x11::Dpms::DPMSMode>;
  return static_cast<x11::Dpms::DPMSMode>(static_cast<T>(l) |
                                          static_cast<T>(r));
}

inline constexpr x11::Dpms::DPMSMode operator&(x11::Dpms::DPMSMode l,
                                               x11::Dpms::DPMSMode r) {
  using T = std::underlying_type_t<x11::Dpms::DPMSMode>;
  return static_cast<x11::Dpms::DPMSMode>(static_cast<T>(l) &
                                          static_cast<T>(r));
}

#endif  // UI_GFX_X_GENERATED_PROTOS_DPMS_H_
