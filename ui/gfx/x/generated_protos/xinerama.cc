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

#include "xinerama.h"

#include <unistd.h>
#include <xcb/xcb.h>
#include <xcb/xcbext.h>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "ui/gfx/x/xproto_internal.h"

namespace x11 {

Xinerama::Xinerama(Connection* connection, const x11::QueryExtensionReply& info)
    : connection_(connection), info_(info) {}

Future<Xinerama::QueryVersionReply> Xinerama::QueryVersion(
    const Xinerama::QueryVersionRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& major = request.major;
  auto& minor = request.minor;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 0;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // major
  buf.Write(&major);

  // minor
  buf.Write(&minor);

  Align(&buf, 4);

  return connection_->SendRequest<Xinerama::QueryVersionReply>(
      &buf, "Xinerama::QueryVersion", false);
}

Future<Xinerama::QueryVersionReply> Xinerama::QueryVersion(
    const uint8_t& major,
    const uint8_t& minor) {
  return Xinerama::QueryVersion(Xinerama::QueryVersionRequest{major, minor});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Xinerama::QueryVersionReply> detail::ReadReply<
    Xinerama::QueryVersionReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Xinerama::QueryVersionReply>();

  auto& sequence = (*reply).sequence;
  auto& major = (*reply).major;
  auto& minor = (*reply).minor;

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

  // major
  Read(&major, &buf);

  // minor
  Read(&minor, &buf);

  Align(&buf, 4);
  DUMP_WILL_BE_CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Xinerama::GetStateReply> Xinerama::GetState(
    const Xinerama::GetStateRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& window = request.window;

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

  Align(&buf, 4);

  return connection_->SendRequest<Xinerama::GetStateReply>(
      &buf, "Xinerama::GetState", false);
}

Future<Xinerama::GetStateReply> Xinerama::GetState(const Window& window) {
  return Xinerama::GetState(Xinerama::GetStateRequest{window});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Xinerama::GetStateReply> detail::ReadReply<
    Xinerama::GetStateReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Xinerama::GetStateReply>();

  auto& state = (*reply).state;
  auto& sequence = (*reply).sequence;
  auto& window = (*reply).window;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // state
  Read(&state, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // window
  Read(&window, &buf);

  Align(&buf, 4);
  DUMP_WILL_BE_CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Xinerama::GetScreenCountReply> Xinerama::GetScreenCount(
    const Xinerama::GetScreenCountRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& window = request.window;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 2;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // window
  buf.Write(&window);

  Align(&buf, 4);

  return connection_->SendRequest<Xinerama::GetScreenCountReply>(
      &buf, "Xinerama::GetScreenCount", false);
}

Future<Xinerama::GetScreenCountReply> Xinerama::GetScreenCount(
    const Window& window) {
  return Xinerama::GetScreenCount(Xinerama::GetScreenCountRequest{window});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Xinerama::GetScreenCountReply> detail::ReadReply<
    Xinerama::GetScreenCountReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Xinerama::GetScreenCountReply>();

  auto& screen_count = (*reply).screen_count;
  auto& sequence = (*reply).sequence;
  auto& window = (*reply).window;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // screen_count
  Read(&screen_count, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // window
  Read(&window, &buf);

  Align(&buf, 4);
  DUMP_WILL_BE_CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Xinerama::GetScreenSizeReply> Xinerama::GetScreenSize(
    const Xinerama::GetScreenSizeRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& window = request.window;
  auto& screen = request.screen;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 3;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // window
  buf.Write(&window);

  // screen
  buf.Write(&screen);

  Align(&buf, 4);

  return connection_->SendRequest<Xinerama::GetScreenSizeReply>(
      &buf, "Xinerama::GetScreenSize", false);
}

Future<Xinerama::GetScreenSizeReply> Xinerama::GetScreenSize(
    const Window& window,
    const uint32_t& screen) {
  return Xinerama::GetScreenSize(
      Xinerama::GetScreenSizeRequest{window, screen});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Xinerama::GetScreenSizeReply> detail::ReadReply<
    Xinerama::GetScreenSizeReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Xinerama::GetScreenSizeReply>();

  auto& sequence = (*reply).sequence;
  auto& width = (*reply).width;
  auto& height = (*reply).height;
  auto& window = (*reply).window;
  auto& screen = (*reply).screen;

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

  // width
  Read(&width, &buf);

  // height
  Read(&height, &buf);

  // window
  Read(&window, &buf);

  // screen
  Read(&screen, &buf);

  Align(&buf, 4);
  DUMP_WILL_BE_CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Xinerama::IsActiveReply> Xinerama::IsActive(
    const Xinerama::IsActiveRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 4;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  Align(&buf, 4);

  return connection_->SendRequest<Xinerama::IsActiveReply>(
      &buf, "Xinerama::IsActive", false);
}

Future<Xinerama::IsActiveReply> Xinerama::IsActive() {
  return Xinerama::IsActive(Xinerama::IsActiveRequest{});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Xinerama::IsActiveReply> detail::ReadReply<
    Xinerama::IsActiveReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Xinerama::IsActiveReply>();

  auto& sequence = (*reply).sequence;
  auto& state = (*reply).state;

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

  // state
  Read(&state, &buf);

  Align(&buf, 4);
  DUMP_WILL_BE_CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Xinerama::QueryScreensReply> Xinerama::QueryScreens(
    const Xinerama::QueryScreensRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 5;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  Align(&buf, 4);

  return connection_->SendRequest<Xinerama::QueryScreensReply>(
      &buf, "Xinerama::QueryScreens", false);
}

Future<Xinerama::QueryScreensReply> Xinerama::QueryScreens() {
  return Xinerama::QueryScreens(Xinerama::QueryScreensRequest{});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Xinerama::QueryScreensReply> detail::ReadReply<
    Xinerama::QueryScreensReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Xinerama::QueryScreensReply>();

  auto& sequence = (*reply).sequence;
  uint32_t number{};
  auto& screen_info = (*reply).screen_info;
  size_t screen_info_len = screen_info.size();

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

  // number
  Read(&number, &buf);

  // pad1
  Pad(&buf, 20);

  // screen_info
  screen_info.resize(number);
  for (auto& screen_info_elem : screen_info) {
    // screen_info_elem
    {
      auto& x_org = screen_info_elem.x_org;
      auto& y_org = screen_info_elem.y_org;
      auto& width = screen_info_elem.width;
      auto& height = screen_info_elem.height;

      // x_org
      Read(&x_org, &buf);

      // y_org
      Read(&y_org, &buf);

      // width
      Read(&width, &buf);

      // height
      Read(&height, &buf);
    }
  }

  Align(&buf, 4);
  DUMP_WILL_BE_CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

}  // namespace x11
