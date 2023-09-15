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

#include "damage.h"

#include <unistd.h>
#include <xcb/xcb.h>
#include <xcb/xcbext.h>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "ui/gfx/x/xproto_internal.h"

namespace x11 {

Damage::Damage(Connection* connection, const x11::QueryExtensionReply& info)
    : connection_(connection), info_(info) {}

std::string Damage::BadDamageError::ToString() const {
  std::stringstream ss_;
  ss_ << "Damage::BadDamageError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<Damage::BadDamageError>(Damage::BadDamageError* error_,
                                       ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*error_).sequence;
  auto& bad_value = (*error_).bad_value;
  auto& minor_opcode = (*error_).minor_opcode;
  auto& major_opcode = (*error_).major_opcode;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // error_code
  uint8_t error_code;
  Read(&error_code, &buf);

  // sequence
  Read(&sequence, &buf);

  // bad_value
  Read(&bad_value, &buf);

  // minor_opcode
  Read(&minor_opcode, &buf);

  // major_opcode
  Read(&major_opcode, &buf);

  DUMP_WILL_BE_CHECK_LE(buf.offset, 32ul);
}
template <>
COMPONENT_EXPORT(X11)
void ReadEvent<Damage::NotifyEvent>(Damage::NotifyEvent* event_,
                                    ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& level = (*event_).level;
  auto& sequence = (*event_).sequence;
  auto& drawable = (*event_).drawable;
  auto& damage = (*event_).damage;
  auto& timestamp = (*event_).timestamp;
  auto& area = (*event_).area;
  auto& geometry = (*event_).geometry;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // level
  uint8_t tmp0;
  Read(&tmp0, &buf);
  level = static_cast<Damage::ReportLevel>(tmp0);

  // sequence
  Read(&sequence, &buf);

  // drawable
  Read(&drawable, &buf);

  // damage
  Read(&damage, &buf);

  // timestamp
  Read(&timestamp, &buf);

  // area
  {
    auto& x = area.x;
    auto& y = area.y;
    auto& width = area.width;
    auto& height = area.height;

    // x
    Read(&x, &buf);

    // y
    Read(&y, &buf);

    // width
    Read(&width, &buf);

    // height
    Read(&height, &buf);
  }

  // geometry
  {
    auto& x = geometry.x;
    auto& y = geometry.y;
    auto& width = geometry.width;
    auto& height = geometry.height;

    // x
    Read(&x, &buf);

    // y
    Read(&y, &buf);

    // width
    Read(&width, &buf);

    // height
    Read(&height, &buf);
  }

  DUMP_WILL_BE_CHECK_LE(buf.offset, 32ul);
}

Future<Damage::QueryVersionReply> Damage::QueryVersion(
    const Damage::QueryVersionRequest& request) {
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

  return connection_->SendRequest<Damage::QueryVersionReply>(
      &buf, "Damage::QueryVersion", false);
}

Future<Damage::QueryVersionReply> Damage::QueryVersion(
    const uint32_t& client_major_version,
    const uint32_t& client_minor_version) {
  return Damage::QueryVersion(
      Damage::QueryVersionRequest{client_major_version, client_minor_version});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Damage::QueryVersionReply> detail::ReadReply<
    Damage::QueryVersionReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Damage::QueryVersionReply>();

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
  Pad(&buf, 16);

  Align(&buf, 4);
  DUMP_WILL_BE_CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Damage::Create(const Damage::CreateRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& damage = request.damage;
  auto& drawable = request.drawable;
  auto& level = request.level;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 1;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // damage
  buf.Write(&damage);

  // drawable
  buf.Write(&drawable);

  // level
  uint8_t tmp1;
  tmp1 = static_cast<uint8_t>(level);
  buf.Write(&tmp1);

  // pad0
  Pad(&buf, 3);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Damage::Create", false);
}

Future<void> Damage::Create(const DamageId& damage,
                            const Drawable& drawable,
                            const ReportLevel& level) {
  return Damage::Create(Damage::CreateRequest{damage, drawable, level});
}

Future<void> Damage::Destroy(const Damage::DestroyRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& damage = request.damage;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 2;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // damage
  buf.Write(&damage);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Damage::Destroy", false);
}

Future<void> Damage::Destroy(const DamageId& damage) {
  return Damage::Destroy(Damage::DestroyRequest{damage});
}

Future<void> Damage::Subtract(const Damage::SubtractRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& damage = request.damage;
  auto& repair = request.repair;
  auto& parts = request.parts;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 3;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // damage
  buf.Write(&damage);

  // repair
  buf.Write(&repair);

  // parts
  buf.Write(&parts);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Damage::Subtract", false);
}

Future<void> Damage::Subtract(const DamageId& damage,
                              const XFixes::Region& repair,
                              const XFixes::Region& parts) {
  return Damage::Subtract(Damage::SubtractRequest{damage, repair, parts});
}

Future<void> Damage::Add(const Damage::AddRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& drawable = request.drawable;
  auto& region = request.region;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 4;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // drawable
  buf.Write(&drawable);

  // region
  buf.Write(&region);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Damage::Add", false);
}

Future<void> Damage::Add(const Drawable& drawable,
                         const XFixes::Region& region) {
  return Damage::Add(Damage::AddRequest{drawable, region});
}

}  // namespace x11
