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

#include "bigreq.h"

#include <unistd.h>
#include <xcb/xcb.h>
#include <xcb/xcbext.h>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/xproto_internal.h"

namespace x11 {

BigRequests::BigRequests(Connection* connection,
                         const x11::QueryExtensionReply& info)
    : connection_(connection), info_(info) {}

Future<BigRequests::EnableReply> BigRequests::Enable(
    const BigRequests::EnableRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 0;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  Align(&buf, 4);

  return connection_->SendRequest<BigRequests::EnableReply>(
      &buf, "BigRequests::Enable", false);
}

Future<BigRequests::EnableReply> BigRequests::Enable() {
  return BigRequests::Enable(BigRequests::EnableRequest{});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<BigRequests::EnableReply> detail::ReadReply<
    BigRequests::EnableReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<BigRequests::EnableReply>();

  auto& sequence = (*reply).sequence;
  auto& maximum_request_length = (*reply).maximum_request_length;

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

  // maximum_request_length
  Read(&maximum_request_length, &buf);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

}  // namespace x11
