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

#ifndef UI_GFX_X_GENERATED_PROTOS_XEVIE_H_
#define UI_GFX_X_GENERATED_PROTOS_XEVIE_H_

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

class COMPONENT_EXPORT(X11) Xevie {
 public:
  static constexpr unsigned major_version = 1;
  static constexpr unsigned minor_version = 0;

  Xevie(Connection* connection, const x11::QueryExtensionReply& info);

  uint8_t present() const { return info_.present; }
  uint8_t major_opcode() const { return info_.major_opcode; }
  uint8_t first_event() const { return info_.first_event; }
  uint8_t first_error() const { return info_.first_error; }

  Connection* connection() const { return connection_; }

  enum class Datatype : int {
    Unmodified = 0,
    Modified = 1,
  };

  struct Event {
    bool operator==(const Event& other) const { return true; }
  };

  struct QueryVersionRequest {
    uint16_t client_major_version{};
    uint16_t client_minor_version{};
  };

  struct QueryVersionReply {
    uint16_t sequence{};
    uint16_t server_major_version{};
    uint16_t server_minor_version{};
  };

  using QueryVersionResponse = Response<QueryVersionReply>;

  Future<QueryVersionReply> QueryVersion(const QueryVersionRequest& request);

  Future<QueryVersionReply> QueryVersion(
      const uint16_t& client_major_version = {},
      const uint16_t& client_minor_version = {});

  struct StartRequest {
    uint32_t screen{};
  };

  struct StartReply {
    uint16_t sequence{};
  };

  using StartResponse = Response<StartReply>;

  Future<StartReply> Start(const StartRequest& request);

  Future<StartReply> Start(const uint32_t& screen = {});

  struct EndRequest {
    uint32_t cmap{};
  };

  struct EndReply {
    uint16_t sequence{};
  };

  using EndResponse = Response<EndReply>;

  Future<EndReply> End(const EndRequest& request);

  Future<EndReply> End(const uint32_t& cmap = {});

  struct SendRequest {
    Event event{};
    uint32_t data_type{};
  };

  struct SendReply {
    uint16_t sequence{};
  };

  using SendResponse = Response<SendReply>;

  Future<SendReply> Send(const SendRequest& request);

  Future<SendReply> Send(const Event& event = {},
                         const uint32_t& data_type = {});

  struct SelectInputRequest {
    uint32_t event_mask{};
  };

  struct SelectInputReply {
    uint16_t sequence{};
  };

  using SelectInputResponse = Response<SelectInputReply>;

  Future<SelectInputReply> SelectInput(const SelectInputRequest& request);

  Future<SelectInputReply> SelectInput(const uint32_t& event_mask = {});

 private:
  Connection* const connection_;
  x11::QueryExtensionReply info_{};
};

}  // namespace x11

inline constexpr x11::Xevie::Datatype operator|(x11::Xevie::Datatype l,
                                                x11::Xevie::Datatype r) {
  using T = std::underlying_type_t<x11::Xevie::Datatype>;
  return static_cast<x11::Xevie::Datatype>(static_cast<T>(l) |
                                           static_cast<T>(r));
}

inline constexpr x11::Xevie::Datatype operator&(x11::Xevie::Datatype l,
                                                x11::Xevie::Datatype r) {
  using T = std::underlying_type_t<x11::Xevie::Datatype>;
  return static_cast<x11::Xevie::Datatype>(static_cast<T>(l) &
                                           static_cast<T>(r));
}

#endif  // UI_GFX_X_GENERATED_PROTOS_XEVIE_H_
