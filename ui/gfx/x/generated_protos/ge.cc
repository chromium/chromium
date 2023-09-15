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

#include "ge.h"

#include <unistd.h>
#include <xcb/xcb.h>
#include <xcb/xcbext.h>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "ui/gfx/x/xproto_internal.h"

namespace x11 {

GenericEvent::GenericEvent(Connection* connection,
                           const x11::QueryExtensionReply& info)
    : connection_(connection), info_(info) {}

Future<GenericEvent::QueryVersionReply> GenericEvent::QueryVersion(
    const GenericEvent::QueryVersionRequest& request) {
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

  return connection_->SendRequest<GenericEvent::QueryVersionReply>(
      &buf, "GenericEvent::QueryVersion", false);
}

Future<GenericEvent::QueryVersionReply> GenericEvent::QueryVersion(
    const uint16_t& client_major_version,
    const uint16_t& client_minor_version) {
  return GenericEvent::QueryVersion(GenericEvent::QueryVersionRequest{
      client_major_version, client_minor_version});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<GenericEvent::QueryVersionReply> detail::ReadReply<
    GenericEvent::QueryVersionReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<GenericEvent::QueryVersionReply>();

  auto& sequence = (*reply).sequence;
  auto& major_version = (*reply).major_version;
  auto& minor_version = (*reply).minor_version;

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

  // major_version
  Read(&major_version, &buf);

  // minor_version
  Read(&minor_version, &buf);

  // pad1
  Pad(&buf, 20);

  Align(&buf, 4);
  DUMP_WILL_BE_CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

}  // namespace x11
