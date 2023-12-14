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

#include "xtest.h"

#include <unistd.h>
#include <xcb/xcb.h>
#include <xcb/xcbext.h>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/xproto_internal.h"

namespace x11 {

Test::Test(Connection* connection, const x11::QueryExtensionReply& info)
    : connection_(connection), info_(info) {}

Future<Test::GetVersionReply> Test::GetVersion(
    const Test::GetVersionRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& major_version = request.major_version;
  auto& minor_version = request.minor_version;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 0;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // major_version
  buf.Write(&major_version);

  // pad0
  Pad(&buf, 1);

  // minor_version
  buf.Write(&minor_version);

  Align(&buf, 4);

  return connection_->SendRequest<Test::GetVersionReply>(
      &buf, "Test::GetVersion", false);
}

Future<Test::GetVersionReply> Test::GetVersion(const uint8_t& major_version,
                                               const uint16_t& minor_version) {
  return Test::GetVersion(
      Test::GetVersionRequest{major_version, minor_version});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Test::GetVersionReply> detail::ReadReply<Test::GetVersionReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Test::GetVersionReply>();

  auto& major_version = (*reply).major_version;
  auto& sequence = (*reply).sequence;
  auto& minor_version = (*reply).minor_version;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // major_version
  Read(&major_version, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // minor_version
  Read(&minor_version, &buf);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Test::CompareCursorReply> Test::CompareCursor(
    const Test::CompareCursorRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& window = request.window;
  auto& cursor = request.cursor;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 1;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // window
  buf.Write(&window);

  // cursor
  buf.Write(&cursor);

  Align(&buf, 4);

  return connection_->SendRequest<Test::CompareCursorReply>(
      &buf, "Test::CompareCursor", false);
}

Future<Test::CompareCursorReply> Test::CompareCursor(
    const Window& window,
    const x11::Cursor& cursor) {
  return Test::CompareCursor(Test::CompareCursorRequest{window, cursor});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Test::CompareCursorReply> detail::ReadReply<
    Test::CompareCursorReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Test::CompareCursorReply>();

  auto& same = (*reply).same;
  auto& sequence = (*reply).sequence;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // same
  Read(&same, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Test::FakeInput(const Test::FakeInputRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& type = request.type;
  auto& detail = request.detail;
  auto& time = request.time;
  auto& root = request.root;
  auto& rootX = request.rootX;
  auto& rootY = request.rootY;
  auto& deviceid = request.deviceid;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 2;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // type
  buf.Write(&type);

  // detail
  buf.Write(&detail);

  // pad0
  Pad(&buf, 2);

  // time
  buf.Write(&time);

  // root
  buf.Write(&root);

  // pad1
  Pad(&buf, 8);

  // rootX
  buf.Write(&rootX);

  // rootY
  buf.Write(&rootY);

  // pad2
  Pad(&buf, 7);

  // deviceid
  buf.Write(&deviceid);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Test::FakeInput", false);
}

Future<void> Test::FakeInput(const uint8_t& type,
                             const uint8_t& detail,
                             const uint32_t& time,
                             const Window& root,
                             const int16_t& rootX,
                             const int16_t& rootY,
                             const uint8_t& deviceid) {
  return Test::FakeInput(
      Test::FakeInputRequest{type, detail, time, root, rootX, rootY, deviceid});
}

Future<void> Test::GrabControl(const Test::GrabControlRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& impervious = request.impervious;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 3;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // impervious
  buf.Write(&impervious);

  // pad0
  Pad(&buf, 3);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Test::GrabControl", false);
}

Future<void> Test::GrabControl(const uint8_t& impervious) {
  return Test::GrabControl(Test::GrabControlRequest{impervious});
}

}  // namespace x11
