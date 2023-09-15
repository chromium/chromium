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

#include "xc_misc.h"

#include <unistd.h>
#include <xcb/xcb.h>
#include <xcb/xcbext.h>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "ui/gfx/x/xproto_internal.h"

namespace x11 {

XCMisc::XCMisc(Connection* connection, const x11::QueryExtensionReply& info)
    : connection_(connection), info_(info) {}

Future<XCMisc::GetVersionReply> XCMisc::GetVersion(
    const XCMisc::GetVersionRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& client_major_version = request.client_major_version;
  auto& client_minor_version = request.client_minor_version;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 0;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // client_major_version
  buf.Write(&client_major_version);

  // client_minor_version
  buf.Write(&client_minor_version);

  Align(&buf, 4);

  return connection_->SendRequest<XCMisc::GetVersionReply>(
      &buf, "XCMisc::GetVersion", false);
}

Future<XCMisc::GetVersionReply> XCMisc::GetVersion(
    const uint16_t& client_major_version,
    const uint16_t& client_minor_version) {
  return XCMisc::GetVersion(
      XCMisc::GetVersionRequest{client_major_version, client_minor_version});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<XCMisc::GetVersionReply> detail::ReadReply<
    XCMisc::GetVersionReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<XCMisc::GetVersionReply>();

  auto& sequence = (*reply).sequence;
  auto& server_major_version = (*reply).server_major_version;
  auto& server_minor_version = (*reply).server_minor_version;

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

  // server_major_version
  Read(&server_major_version, &buf);

  // server_minor_version
  Read(&server_minor_version, &buf);

  Align(&buf, 4);
  DUMP_WILL_BE_CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<XCMisc::GetXIDRangeReply> XCMisc::GetXIDRange(
    const XCMisc::GetXIDRangeRequest& request) {
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

  return connection_->SendRequest<XCMisc::GetXIDRangeReply>(
      &buf, "XCMisc::GetXIDRange", false);
}

Future<XCMisc::GetXIDRangeReply> XCMisc::GetXIDRange() {
  return XCMisc::GetXIDRange(XCMisc::GetXIDRangeRequest{});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<XCMisc::GetXIDRangeReply> detail::ReadReply<
    XCMisc::GetXIDRangeReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<XCMisc::GetXIDRangeReply>();

  auto& sequence = (*reply).sequence;
  auto& start_id = (*reply).start_id;
  auto& count = (*reply).count;

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

  // start_id
  Read(&start_id, &buf);

  // count
  Read(&count, &buf);

  Align(&buf, 4);
  DUMP_WILL_BE_CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<XCMisc::GetXIDListReply> XCMisc::GetXIDList(
    const XCMisc::GetXIDListRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& count = request.count;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 2;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // count
  buf.Write(&count);

  Align(&buf, 4);

  return connection_->SendRequest<XCMisc::GetXIDListReply>(
      &buf, "XCMisc::GetXIDList", false);
}

Future<XCMisc::GetXIDListReply> XCMisc::GetXIDList(const uint32_t& count) {
  return XCMisc::GetXIDList(XCMisc::GetXIDListRequest{count});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<XCMisc::GetXIDListReply> detail::ReadReply<
    XCMisc::GetXIDListReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<XCMisc::GetXIDListReply>();

  auto& sequence = (*reply).sequence;
  uint32_t ids_len{};
  auto& ids = (*reply).ids;

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

  // ids_len
  Read(&ids_len, &buf);

  // pad1
  Pad(&buf, 20);

  // ids
  ids.resize(ids_len);
  for (auto& ids_elem : ids) {
    // ids_elem
    Read(&ids_elem, &buf);
  }

  Align(&buf, 4);
  DUMP_WILL_BE_CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

}  // namespace x11
