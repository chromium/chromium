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

#include "res.h"

#include <unistd.h>
#include <xcb/xcb.h>
#include <xcb/xcbext.h>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "ui/gfx/x/xproto_internal.h"

namespace x11 {

Res::Res(Connection* connection, const x11::QueryExtensionReply& info)
    : connection_(connection), info_(info) {}

Future<Res::QueryVersionReply> Res::QueryVersion(
    const Res::QueryVersionRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& client_major = request.client_major;
  auto& client_minor = request.client_minor;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 0;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // client_major
  buf.Write(&client_major);

  // client_minor
  buf.Write(&client_minor);

  Align(&buf, 4);

  return connection_->SendRequest<Res::QueryVersionReply>(
      &buf, "Res::QueryVersion", false);
}

Future<Res::QueryVersionReply> Res::QueryVersion(const uint8_t& client_major,
                                                 const uint8_t& client_minor) {
  return Res::QueryVersion(
      Res::QueryVersionRequest{client_major, client_minor});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Res::QueryVersionReply> detail::ReadReply<
    Res::QueryVersionReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Res::QueryVersionReply>();

  auto& sequence = (*reply).sequence;
  auto& server_major = (*reply).server_major;
  auto& server_minor = (*reply).server_minor;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // server_major
  Read(&server_major, &buf);

  // server_minor
  Read(&server_minor, &buf);

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Res::QueryClientsReply> Res::QueryClients(
    const Res::QueryClientsRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 1;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  Align(&buf, 4);

  return connection_->SendRequest<Res::QueryClientsReply>(
      &buf, "Res::QueryClients", false);
}

Future<Res::QueryClientsReply> Res::QueryClients() {
  return Res::QueryClients(Res::QueryClientsRequest{});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Res::QueryClientsReply> detail::ReadReply<
    Res::QueryClientsReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Res::QueryClientsReply>();

  auto& sequence = (*reply).sequence;
  uint32_t num_clients{};
  auto& clients = (*reply).clients;
  size_t clients_len = clients.size();

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // num_clients
  Read(&num_clients, &buf);

  // pad1
  Pad(&buf, 20);

  // clients
  clients.resize(num_clients);
  for (auto& clients_elem : clients) {
    // clients_elem
    {
      auto& resource_base = clients_elem.resource_base;
      auto& resource_mask = clients_elem.resource_mask;

      // resource_base
      Read(&resource_base, &buf);

      // resource_mask
      Read(&resource_mask, &buf);
    }
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Res::QueryClientResourcesReply> Res::QueryClientResources(
    const Res::QueryClientResourcesRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& xid = request.xid;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 2;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // xid
  buf.Write(&xid);

  Align(&buf, 4);

  return connection_->SendRequest<Res::QueryClientResourcesReply>(
      &buf, "Res::QueryClientResources", false);
}

Future<Res::QueryClientResourcesReply> Res::QueryClientResources(
    const uint32_t& xid) {
  return Res::QueryClientResources(Res::QueryClientResourcesRequest{xid});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Res::QueryClientResourcesReply> detail::ReadReply<
    Res::QueryClientResourcesReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Res::QueryClientResourcesReply>();

  auto& sequence = (*reply).sequence;
  uint32_t num_types{};
  auto& types = (*reply).types;
  size_t types_len = types.size();

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // num_types
  Read(&num_types, &buf);

  // pad1
  Pad(&buf, 20);

  // types
  types.resize(num_types);
  for (auto& types_elem : types) {
    // types_elem
    {
      auto& resource_type = types_elem.resource_type;
      auto& count = types_elem.count;

      // resource_type
      Read(&resource_type, &buf);

      // count
      Read(&count, &buf);
    }
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Res::QueryClientPixmapBytesReply> Res::QueryClientPixmapBytes(
    const Res::QueryClientPixmapBytesRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& xid = request.xid;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 3;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // xid
  buf.Write(&xid);

  Align(&buf, 4);

  return connection_->SendRequest<Res::QueryClientPixmapBytesReply>(
      &buf, "Res::QueryClientPixmapBytes", false);
}

Future<Res::QueryClientPixmapBytesReply> Res::QueryClientPixmapBytes(
    const uint32_t& xid) {
  return Res::QueryClientPixmapBytes(Res::QueryClientPixmapBytesRequest{xid});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Res::QueryClientPixmapBytesReply> detail::ReadReply<
    Res::QueryClientPixmapBytesReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Res::QueryClientPixmapBytesReply>();

  auto& sequence = (*reply).sequence;
  auto& bytes = (*reply).bytes;
  auto& bytes_overflow = (*reply).bytes_overflow;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // bytes
  Read(&bytes, &buf);

  // bytes_overflow
  Read(&bytes_overflow, &buf);

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Res::QueryClientIdsReply> Res::QueryClientIds(
    const Res::QueryClientIdsRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  uint32_t num_specs{};
  auto& specs = request.specs;
  size_t specs_len = specs.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 4;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // num_specs
  num_specs = specs.size();
  buf.Write(&num_specs);

  // specs
  DCHECK_EQ(static_cast<size_t>(num_specs), specs.size());
  for (auto& specs_elem : specs) {
    // specs_elem
    {
      auto& client = specs_elem.client;
      auto& mask = specs_elem.mask;

      // client
      buf.Write(&client);

      // mask
      uint32_t tmp0;
      tmp0 = static_cast<uint32_t>(mask);
      buf.Write(&tmp0);
    }
  }

  Align(&buf, 4);

  return connection_->SendRequest<Res::QueryClientIdsReply>(
      &buf, "Res::QueryClientIds", false);
}

Future<Res::QueryClientIdsReply> Res::QueryClientIds(
    const std::vector<ClientIdSpec>& specs) {
  return Res::QueryClientIds(Res::QueryClientIdsRequest{specs});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Res::QueryClientIdsReply> detail::ReadReply<
    Res::QueryClientIdsReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Res::QueryClientIdsReply>();

  auto& sequence = (*reply).sequence;
  uint32_t num_ids{};
  auto& ids = (*reply).ids;
  size_t ids_len = ids.size();

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // num_ids
  Read(&num_ids, &buf);

  // pad1
  Pad(&buf, 20);

  // ids
  ids.resize(num_ids);
  for (auto& ids_elem : ids) {
    // ids_elem
    {
      auto& spec = ids_elem.spec;
      auto& length = ids_elem.length;
      auto& value = ids_elem.value;
      size_t value_len = value.size();

      // spec
      {
        auto& client = spec.client;
        auto& mask = spec.mask;

        // client
        Read(&client, &buf);

        // mask
        uint32_t tmp1;
        Read(&tmp1, &buf);
        mask = static_cast<Res::ClientIdMask>(tmp1);
      }

      // length
      Read(&length, &buf);

      // value
      value.resize((length) / (4));
      for (auto& value_elem : value) {
        // value_elem
        Read(&value_elem, &buf);
      }
    }
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Res::QueryResourceBytesReply> Res::QueryResourceBytes(
    const Res::QueryResourceBytesRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& client = request.client;
  uint32_t num_specs{};
  auto& specs = request.specs;
  size_t specs_len = specs.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 5;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // client
  buf.Write(&client);

  // num_specs
  num_specs = specs.size();
  buf.Write(&num_specs);

  // specs
  DCHECK_EQ(static_cast<size_t>(num_specs), specs.size());
  for (auto& specs_elem : specs) {
    // specs_elem
    {
      auto& resource = specs_elem.resource;
      auto& type = specs_elem.type;

      // resource
      buf.Write(&resource);

      // type
      buf.Write(&type);
    }
  }

  Align(&buf, 4);

  return connection_->SendRequest<Res::QueryResourceBytesReply>(
      &buf, "Res::QueryResourceBytes", false);
}

Future<Res::QueryResourceBytesReply> Res::QueryResourceBytes(
    const uint32_t& client,
    const std::vector<ResourceIdSpec>& specs) {
  return Res::QueryResourceBytes(Res::QueryResourceBytesRequest{client, specs});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Res::QueryResourceBytesReply> detail::ReadReply<
    Res::QueryResourceBytesReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Res::QueryResourceBytesReply>();

  auto& sequence = (*reply).sequence;
  uint32_t num_sizes{};
  auto& sizes = (*reply).sizes;
  size_t sizes_len = sizes.size();

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // num_sizes
  Read(&num_sizes, &buf);

  // pad1
  Pad(&buf, 20);

  // sizes
  sizes.resize(num_sizes);
  for (auto& sizes_elem : sizes) {
    // sizes_elem
    {
      auto& size = sizes_elem.size;
      uint32_t num_cross_references{};
      auto& cross_references = sizes_elem.cross_references;
      size_t cross_references_len = cross_references.size();

      // size
      {
        auto& spec = size.spec;
        auto& bytes = size.bytes;
        auto& ref_count = size.ref_count;
        auto& use_count = size.use_count;

        // spec
        {
          auto& resource = spec.resource;
          auto& type = spec.type;

          // resource
          Read(&resource, &buf);

          // type
          Read(&type, &buf);
        }

        // bytes
        Read(&bytes, &buf);

        // ref_count
        Read(&ref_count, &buf);

        // use_count
        Read(&use_count, &buf);
      }

      // num_cross_references
      Read(&num_cross_references, &buf);

      // cross_references
      cross_references.resize(num_cross_references);
      for (auto& cross_references_elem : cross_references) {
        // cross_references_elem
        {
          auto& spec = cross_references_elem.spec;
          auto& bytes = cross_references_elem.bytes;
          auto& ref_count = cross_references_elem.ref_count;
          auto& use_count = cross_references_elem.use_count;

          // spec
          {
            auto& resource = spec.resource;
            auto& type = spec.type;

            // resource
            Read(&resource, &buf);

            // type
            Read(&type, &buf);
          }

          // bytes
          Read(&bytes, &buf);

          // ref_count
          Read(&ref_count, &buf);

          // use_count
          Read(&use_count, &buf);
        }
      }
    }
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

}  // namespace x11
