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

#include "xevie.h"

#include <unistd.h>
#include <xcb/xcb.h>
#include <xcb/xcbext.h>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "ui/gfx/x/xproto_internal.h"

namespace x11 {

Xevie::Xevie(Connection* connection, const x11::QueryExtensionReply& info)
    : connection_(connection), info_(info) {}

Future<Xevie::QueryVersionReply> Xevie::QueryVersion(
    const Xevie::QueryVersionRequest& request) {
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

  return connection_->SendRequest<Xevie::QueryVersionReply>(
      &buf, "Xevie::QueryVersion", false);
}

Future<Xevie::QueryVersionReply> Xevie::QueryVersion(
    const uint16_t& client_major_version,
    const uint16_t& client_minor_version) {
  return Xevie::QueryVersion(
      Xevie::QueryVersionRequest{client_major_version, client_minor_version});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Xevie::QueryVersionReply> detail::ReadReply<
    Xevie::QueryVersionReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Xevie::QueryVersionReply>();

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

  // pad1
  Pad(&buf, 20);

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Xevie::StartReply> Xevie::Start(const Xevie::StartRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& screen = request.screen;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 1;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // screen
  buf.Write(&screen);

  Align(&buf, 4);

  return connection_->SendRequest<Xevie::StartReply>(&buf, "Xevie::Start",
                                                     false);
}

Future<Xevie::StartReply> Xevie::Start(const uint32_t& screen) {
  return Xevie::Start(Xevie::StartRequest{screen});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Xevie::StartReply> detail::ReadReply<Xevie::StartReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Xevie::StartReply>();

  auto& sequence = (*reply).sequence;

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

  // pad1
  Pad(&buf, 24);

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Xevie::EndReply> Xevie::End(const Xevie::EndRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& cmap = request.cmap;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 2;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // cmap
  buf.Write(&cmap);

  Align(&buf, 4);

  return connection_->SendRequest<Xevie::EndReply>(&buf, "Xevie::End", false);
}

Future<Xevie::EndReply> Xevie::End(const uint32_t& cmap) {
  return Xevie::End(Xevie::EndRequest{cmap});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Xevie::EndReply> detail::ReadReply<Xevie::EndReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Xevie::EndReply>();

  auto& sequence = (*reply).sequence;

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

  // pad1
  Pad(&buf, 24);

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Xevie::SendReply> Xevie::Send(const Xevie::SendRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& event = request.event;
  auto& data_type = request.data_type;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 3;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // event
  {
    // pad0
    Pad(&buf, 32);
  }

  // data_type
  buf.Write(&data_type);

  // pad0
  Pad(&buf, 64);

  Align(&buf, 4);

  return connection_->SendRequest<Xevie::SendReply>(&buf, "Xevie::Send", false);
}

Future<Xevie::SendReply> Xevie::Send(const Event& event,
                                     const uint32_t& data_type) {
  return Xevie::Send(Xevie::SendRequest{event, data_type});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Xevie::SendReply> detail::ReadReply<Xevie::SendReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Xevie::SendReply>();

  auto& sequence = (*reply).sequence;

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

  // pad1
  Pad(&buf, 24);

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Xevie::SelectInputReply> Xevie::SelectInput(
    const Xevie::SelectInputRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& event_mask = request.event_mask;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 4;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // event_mask
  buf.Write(&event_mask);

  Align(&buf, 4);

  return connection_->SendRequest<Xevie::SelectInputReply>(
      &buf, "Xevie::SelectInput", false);
}

Future<Xevie::SelectInputReply> Xevie::SelectInput(const uint32_t& event_mask) {
  return Xevie::SelectInput(Xevie::SelectInputRequest{event_mask});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Xevie::SelectInputReply> detail::ReadReply<
    Xevie::SelectInputReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Xevie::SelectInputReply>();

  auto& sequence = (*reply).sequence;

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

  // pad1
  Pad(&buf, 24);

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

}  // namespace x11
