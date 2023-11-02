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

#ifndef UI_GFX_X_GENERATED_PROTOS_RECORD_H_
#define UI_GFX_X_GENERATED_PROTOS_RECORD_H_

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

class COMPONENT_EXPORT(X11) Record {
 public:
  static constexpr unsigned major_version = 1;
  static constexpr unsigned minor_version = 13;

  Record(Connection* connection, const x11::QueryExtensionReply& info);

  uint8_t present() const { return info_.present; }
  uint8_t major_opcode() const { return info_.major_opcode; }
  uint8_t first_event() const { return info_.first_event; }
  uint8_t first_error() const { return info_.first_error; }

  Connection* connection() const { return connection_; }

  enum class Context : uint32_t {};

  enum class ElementHeader : uint8_t {};

  enum class HType : int {
    FromServerTime = 1 << 0,
    FromClientTime = 1 << 1,
    FromClientSequence = 1 << 2,
  };

  enum class ClientSpec : uint32_t {
    CurrentClients = 1,
    FutureClients = 2,
    AllClients = 3,
  };

  struct Range8 {
    bool operator==(const Range8& other) const {
      return first == other.first && last == other.last;
    }

    uint8_t first{};
    uint8_t last{};
  };

  struct Range16 {
    bool operator==(const Range16& other) const {
      return first == other.first && last == other.last;
    }

    uint16_t first{};
    uint16_t last{};
  };

  struct ExtRange {
    bool operator==(const ExtRange& other) const {
      return major == other.major && minor == other.minor;
    }

    Range8 major{};
    Range16 minor{};
  };

  struct Range {
    bool operator==(const Range& other) const {
      return core_requests == other.core_requests &&
             core_replies == other.core_replies &&
             ext_requests == other.ext_requests &&
             ext_replies == other.ext_replies &&
             delivered_events == other.delivered_events &&
             device_events == other.device_events && errors == other.errors &&
             client_started == other.client_started &&
             client_died == other.client_died;
    }

    Range8 core_requests{};
    Range8 core_replies{};
    ExtRange ext_requests{};
    ExtRange ext_replies{};
    Range8 delivered_events{};
    Range8 device_events{};
    Range8 errors{};
    uint8_t client_started{};
    uint8_t client_died{};
  };

  struct ClientInfo {
    bool operator==(const ClientInfo& other) const {
      return client_resource == other.client_resource && ranges == other.ranges;
    }

    ClientSpec client_resource{};
    std::vector<Range> ranges{};
  };

  struct BadContextError : public x11::Error {
    uint16_t sequence{};
    uint32_t invalid_record{};
    uint16_t minor_opcode{};
    uint8_t major_opcode{};

    std::string ToString() const override;
  };

  struct QueryVersionRequest {
    uint16_t major_version{};
    uint16_t minor_version{};
  };

  struct QueryVersionReply {
    uint16_t sequence{};
    uint16_t major_version{};
    uint16_t minor_version{};
  };

  using QueryVersionResponse = Response<QueryVersionReply>;

  Future<QueryVersionReply> QueryVersion(const QueryVersionRequest& request);

  Future<QueryVersionReply> QueryVersion(const uint16_t& major_version = {},
                                         const uint16_t& minor_version = {});

  struct CreateContextRequest {
    Context context{};
    ElementHeader element_header{};
    std::vector<ClientSpec> client_specs{};
    std::vector<Range> ranges{};
  };

  using CreateContextResponse = Response<void>;

  Future<void> CreateContext(const CreateContextRequest& request);

  Future<void> CreateContext(const Context& context = {},
                             const ElementHeader& element_header = {},
                             const std::vector<ClientSpec>& client_specs = {},
                             const std::vector<Range>& ranges = {});

  struct RegisterClientsRequest {
    Context context{};
    ElementHeader element_header{};
    std::vector<ClientSpec> client_specs{};
    std::vector<Range> ranges{};
  };

  using RegisterClientsResponse = Response<void>;

  Future<void> RegisterClients(const RegisterClientsRequest& request);

  Future<void> RegisterClients(const Context& context = {},
                               const ElementHeader& element_header = {},
                               const std::vector<ClientSpec>& client_specs = {},
                               const std::vector<Range>& ranges = {});

  struct UnregisterClientsRequest {
    Context context{};
    std::vector<ClientSpec> client_specs{};
  };

  using UnregisterClientsResponse = Response<void>;

  Future<void> UnregisterClients(const UnregisterClientsRequest& request);

  Future<void> UnregisterClients(
      const Context& context = {},
      const std::vector<ClientSpec>& client_specs = {});

  struct GetContextRequest {
    Context context{};
  };

  struct GetContextReply {
    uint8_t enabled{};
    uint16_t sequence{};
    ElementHeader element_header{};
    std::vector<ClientInfo> intercepted_clients{};
  };

  using GetContextResponse = Response<GetContextReply>;

  Future<GetContextReply> GetContext(const GetContextRequest& request);

  Future<GetContextReply> GetContext(const Context& context = {});

  struct EnableContextRequest {
    Context context{};
  };

  struct EnableContextReply {
    uint8_t category{};
    uint16_t sequence{};
    ElementHeader element_header{};
    uint8_t client_swapped{};
    uint32_t xid_base{};
    uint32_t server_time{};
    uint32_t rec_sequence_num{};
    std::vector<uint8_t> data{};
  };

  using EnableContextResponse = Response<EnableContextReply>;

  Future<EnableContextReply> EnableContext(const EnableContextRequest& request);

  Future<EnableContextReply> EnableContext(const Context& context = {});

  struct DisableContextRequest {
    Context context{};
  };

  using DisableContextResponse = Response<void>;

  Future<void> DisableContext(const DisableContextRequest& request);

  Future<void> DisableContext(const Context& context = {});

  struct FreeContextRequest {
    Context context{};
  };

  using FreeContextResponse = Response<void>;

  Future<void> FreeContext(const FreeContextRequest& request);

  Future<void> FreeContext(const Context& context = {});

 private:
  Connection* const connection_;
  x11::QueryExtensionReply info_{};
};

}  // namespace x11

inline constexpr x11::Record::HType operator|(x11::Record::HType l,
                                              x11::Record::HType r) {
  using T = std::underlying_type_t<x11::Record::HType>;
  return static_cast<x11::Record::HType>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::Record::HType operator&(x11::Record::HType l,
                                              x11::Record::HType r) {
  using T = std::underlying_type_t<x11::Record::HType>;
  return static_cast<x11::Record::HType>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::Record::ClientSpec operator|(x11::Record::ClientSpec l,
                                                   x11::Record::ClientSpec r) {
  using T = std::underlying_type_t<x11::Record::ClientSpec>;
  return static_cast<x11::Record::ClientSpec>(static_cast<T>(l) |
                                              static_cast<T>(r));
}

inline constexpr x11::Record::ClientSpec operator&(x11::Record::ClientSpec l,
                                                   x11::Record::ClientSpec r) {
  using T = std::underlying_type_t<x11::Record::ClientSpec>;
  return static_cast<x11::Record::ClientSpec>(static_cast<T>(l) &
                                              static_cast<T>(r));
}

#endif  // UI_GFX_X_GENERATED_PROTOS_RECORD_H_
