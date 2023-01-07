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

#ifndef UI_GFX_X_GENERATED_PROTOS_RES_H_
#define UI_GFX_X_GENERATED_PROTOS_RES_H_

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

class COMPONENT_EXPORT(X11) Res {
 public:
  static constexpr unsigned major_version = 1;
  static constexpr unsigned minor_version = 2;

  Res(Connection* connection, const x11::QueryExtensionReply& info);

  uint8_t present() const { return info_.present; }
  uint8_t major_opcode() const { return info_.major_opcode; }
  uint8_t first_event() const { return info_.first_event; }
  uint8_t first_error() const { return info_.first_error; }

  Connection* connection() const { return connection_; }

  enum class ClientIdMask : int {
    ClientXID = 1 << 0,
    LocalClientPID = 1 << 1,
  };

  struct Client {
    bool operator==(const Client& other) const {
      return resource_base == other.resource_base &&
             resource_mask == other.resource_mask;
    }

    uint32_t resource_base{};
    uint32_t resource_mask{};
  };

  struct Type {
    bool operator==(const Type& other) const {
      return resource_type == other.resource_type && count == other.count;
    }

    Atom resource_type{};
    uint32_t count{};
  };

  struct ClientIdSpec {
    bool operator==(const ClientIdSpec& other) const {
      return client == other.client && mask == other.mask;
    }

    uint32_t client{};
    ClientIdMask mask{};
  };

  struct ClientIdValue {
    bool operator==(const ClientIdValue& other) const {
      return spec == other.spec && length == other.length &&
             value == other.value;
    }

    ClientIdSpec spec{};
    uint32_t length{};
    std::vector<uint32_t> value{};
  };

  struct ResourceIdSpec {
    bool operator==(const ResourceIdSpec& other) const {
      return resource == other.resource && type == other.type;
    }

    uint32_t resource{};
    uint32_t type{};
  };

  struct ResourceSizeSpec {
    bool operator==(const ResourceSizeSpec& other) const {
      return spec == other.spec && bytes == other.bytes &&
             ref_count == other.ref_count && use_count == other.use_count;
    }

    ResourceIdSpec spec{};
    uint32_t bytes{};
    uint32_t ref_count{};
    uint32_t use_count{};
  };

  struct ResourceSizeValue {
    bool operator==(const ResourceSizeValue& other) const {
      return size == other.size && cross_references == other.cross_references;
    }

    ResourceSizeSpec size{};
    std::vector<ResourceSizeSpec> cross_references{};
  };

  struct QueryVersionRequest {
    uint8_t client_major{};
    uint8_t client_minor{};
  };

  struct QueryVersionReply {
    uint16_t sequence{};
    uint16_t server_major{};
    uint16_t server_minor{};
  };

  using QueryVersionResponse = Response<QueryVersionReply>;

  Future<QueryVersionReply> QueryVersion(const QueryVersionRequest& request);

  Future<QueryVersionReply> QueryVersion(const uint8_t& client_major = {},
                                         const uint8_t& client_minor = {});

  struct QueryClientsRequest {};

  struct QueryClientsReply {
    uint16_t sequence{};
    std::vector<Client> clients{};
  };

  using QueryClientsResponse = Response<QueryClientsReply>;

  Future<QueryClientsReply> QueryClients(const QueryClientsRequest& request);

  Future<QueryClientsReply> QueryClients();

  struct QueryClientResourcesRequest {
    uint32_t xid{};
  };

  struct QueryClientResourcesReply {
    uint16_t sequence{};
    std::vector<Type> types{};
  };

  using QueryClientResourcesResponse = Response<QueryClientResourcesReply>;

  Future<QueryClientResourcesReply> QueryClientResources(
      const QueryClientResourcesRequest& request);

  Future<QueryClientResourcesReply> QueryClientResources(
      const uint32_t& xid = {});

  struct QueryClientPixmapBytesRequest {
    uint32_t xid{};
  };

  struct QueryClientPixmapBytesReply {
    uint16_t sequence{};
    uint32_t bytes{};
    uint32_t bytes_overflow{};
  };

  using QueryClientPixmapBytesResponse = Response<QueryClientPixmapBytesReply>;

  Future<QueryClientPixmapBytesReply> QueryClientPixmapBytes(
      const QueryClientPixmapBytesRequest& request);

  Future<QueryClientPixmapBytesReply> QueryClientPixmapBytes(
      const uint32_t& xid = {});

  struct QueryClientIdsRequest {
    std::vector<ClientIdSpec> specs{};
  };

  struct QueryClientIdsReply {
    uint16_t sequence{};
    std::vector<ClientIdValue> ids{};
  };

  using QueryClientIdsResponse = Response<QueryClientIdsReply>;

  Future<QueryClientIdsReply> QueryClientIds(
      const QueryClientIdsRequest& request);

  Future<QueryClientIdsReply> QueryClientIds(
      const std::vector<ClientIdSpec>& specs = {});

  struct QueryResourceBytesRequest {
    uint32_t client{};
    std::vector<ResourceIdSpec> specs{};
  };

  struct QueryResourceBytesReply {
    uint16_t sequence{};
    std::vector<ResourceSizeValue> sizes{};
  };

  using QueryResourceBytesResponse = Response<QueryResourceBytesReply>;

  Future<QueryResourceBytesReply> QueryResourceBytes(
      const QueryResourceBytesRequest& request);

  Future<QueryResourceBytesReply> QueryResourceBytes(
      const uint32_t& client = {},
      const std::vector<ResourceIdSpec>& specs = {});

 private:
  Connection* const connection_;
  x11::QueryExtensionReply info_{};
};

}  // namespace x11

inline constexpr x11::Res::ClientIdMask operator|(x11::Res::ClientIdMask l,
                                                  x11::Res::ClientIdMask r) {
  using T = std::underlying_type_t<x11::Res::ClientIdMask>;
  return static_cast<x11::Res::ClientIdMask>(static_cast<T>(l) |
                                             static_cast<T>(r));
}

inline constexpr x11::Res::ClientIdMask operator&(x11::Res::ClientIdMask l,
                                                  x11::Res::ClientIdMask r) {
  using T = std::underlying_type_t<x11::Res::ClientIdMask>;
  return static_cast<x11::Res::ClientIdMask>(static_cast<T>(l) &
                                             static_cast<T>(r));
}

#endif  // UI_GFX_X_GENERATED_PROTOS_RES_H_
