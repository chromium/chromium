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

#include "dpms.h"

#include <unistd.h>
#include <xcb/xcb.h>
#include <xcb/xcbext.h>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "ui/gfx/x/xproto_internal.h"

namespace x11 {

Dpms::Dpms(Connection* connection, const x11::QueryExtensionReply& info)
    : connection_(connection), info_(info) {}

Future<Dpms::GetVersionReply> Dpms::GetVersion(
    const Dpms::GetVersionRequest& request) {
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

  return connection_->SendRequest<Dpms::GetVersionReply>(
      &buf, "Dpms::GetVersion", false);
}

Future<Dpms::GetVersionReply> Dpms::GetVersion(
    const uint16_t& client_major_version,
    const uint16_t& client_minor_version) {
  return Dpms::GetVersion(
      Dpms::GetVersionRequest{client_major_version, client_minor_version});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Dpms::GetVersionReply> detail::ReadReply<Dpms::GetVersionReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Dpms::GetVersionReply>();

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

Future<Dpms::CapableReply> Dpms::Capable(const Dpms::CapableRequest& request) {
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

  return connection_->SendRequest<Dpms::CapableReply>(&buf, "Dpms::Capable",
                                                      false);
}

Future<Dpms::CapableReply> Dpms::Capable() {
  return Dpms::Capable(Dpms::CapableRequest{});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Dpms::CapableReply> detail::ReadReply<Dpms::CapableReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Dpms::CapableReply>();

  auto& sequence = (*reply).sequence;
  auto& capable = (*reply).capable;

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

  // capable
  Read(&capable, &buf);

  // pad1
  Pad(&buf, 23);

  Align(&buf, 4);
  DUMP_WILL_BE_CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Dpms::GetTimeoutsReply> Dpms::GetTimeouts(
    const Dpms::GetTimeoutsRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 2;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  Align(&buf, 4);

  return connection_->SendRequest<Dpms::GetTimeoutsReply>(
      &buf, "Dpms::GetTimeouts", false);
}

Future<Dpms::GetTimeoutsReply> Dpms::GetTimeouts() {
  return Dpms::GetTimeouts(Dpms::GetTimeoutsRequest{});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Dpms::GetTimeoutsReply> detail::ReadReply<
    Dpms::GetTimeoutsReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Dpms::GetTimeoutsReply>();

  auto& sequence = (*reply).sequence;
  auto& standby_timeout = (*reply).standby_timeout;
  auto& suspend_timeout = (*reply).suspend_timeout;
  auto& off_timeout = (*reply).off_timeout;

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

  // standby_timeout
  Read(&standby_timeout, &buf);

  // suspend_timeout
  Read(&suspend_timeout, &buf);

  // off_timeout
  Read(&off_timeout, &buf);

  // pad1
  Pad(&buf, 18);

  Align(&buf, 4);
  DUMP_WILL_BE_CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Dpms::SetTimeouts(const Dpms::SetTimeoutsRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& standby_timeout = request.standby_timeout;
  auto& suspend_timeout = request.suspend_timeout;
  auto& off_timeout = request.off_timeout;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 3;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // standby_timeout
  buf.Write(&standby_timeout);

  // suspend_timeout
  buf.Write(&suspend_timeout);

  // off_timeout
  buf.Write(&off_timeout);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Dpms::SetTimeouts", false);
}

Future<void> Dpms::SetTimeouts(const uint16_t& standby_timeout,
                               const uint16_t& suspend_timeout,
                               const uint16_t& off_timeout) {
  return Dpms::SetTimeouts(
      Dpms::SetTimeoutsRequest{standby_timeout, suspend_timeout, off_timeout});
}

Future<void> Dpms::Enable(const Dpms::EnableRequest& request) {
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

  return connection_->SendRequest<void>(&buf, "Dpms::Enable", false);
}

Future<void> Dpms::Enable() {
  return Dpms::Enable(Dpms::EnableRequest{});
}

Future<void> Dpms::Disable(const Dpms::DisableRequest& request) {
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

  return connection_->SendRequest<void>(&buf, "Dpms::Disable", false);
}

Future<void> Dpms::Disable() {
  return Dpms::Disable(Dpms::DisableRequest{});
}

Future<void> Dpms::ForceLevel(const Dpms::ForceLevelRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& power_level = request.power_level;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 6;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // power_level
  uint16_t tmp0;
  tmp0 = static_cast<uint16_t>(power_level);
  buf.Write(&tmp0);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Dpms::ForceLevel", false);
}

Future<void> Dpms::ForceLevel(const DPMSMode& power_level) {
  return Dpms::ForceLevel(Dpms::ForceLevelRequest{power_level});
}

Future<Dpms::InfoReply> Dpms::Info(const Dpms::InfoRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 7;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  Align(&buf, 4);

  return connection_->SendRequest<Dpms::InfoReply>(&buf, "Dpms::Info", false);
}

Future<Dpms::InfoReply> Dpms::Info() {
  return Dpms::Info(Dpms::InfoRequest{});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Dpms::InfoReply> detail::ReadReply<Dpms::InfoReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Dpms::InfoReply>();

  auto& sequence = (*reply).sequence;
  auto& power_level = (*reply).power_level;
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

  // power_level
  uint16_t tmp1;
  Read(&tmp1, &buf);
  power_level = static_cast<Dpms::DPMSMode>(tmp1);

  // state
  Read(&state, &buf);

  // pad1
  Pad(&buf, 21);

  Align(&buf, 4);
  DUMP_WILL_BE_CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

}  // namespace x11
