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

#include "dri2.h"

#include <unistd.h>
#include <xcb/xcb.h>
#include <xcb/xcbext.h>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "ui/gfx/x/xproto_internal.h"

namespace x11 {

Dri2::Dri2(Connection* connection, const x11::QueryExtensionReply& info)
    : connection_(connection), info_(info) {}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<Dri2::BufferSwapCompleteEvent>(
    Dri2::BufferSwapCompleteEvent* event_,
    ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*event_).sequence;
  auto& event_type = (*event_).event_type;
  auto& drawable = (*event_).drawable;
  auto& ust_hi = (*event_).ust_hi;
  auto& ust_lo = (*event_).ust_lo;
  auto& msc_hi = (*event_).msc_hi;
  auto& msc_lo = (*event_).msc_lo;
  auto& sbc = (*event_).sbc;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // event_type
  uint16_t tmp0;
  Read(&tmp0, &buf);
  event_type = static_cast<Dri2::EventType>(tmp0);

  // pad1
  Pad(&buf, 2);

  // drawable
  Read(&drawable, &buf);

  // ust_hi
  Read(&ust_hi, &buf);

  // ust_lo
  Read(&ust_lo, &buf);

  // msc_hi
  Read(&msc_hi, &buf);

  // msc_lo
  Read(&msc_lo, &buf);

  // sbc
  Read(&sbc, &buf);

  DCHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<Dri2::InvalidateBuffersEvent>(
    Dri2::InvalidateBuffersEvent* event_,
    ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*event_).sequence;
  auto& drawable = (*event_).drawable;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // drawable
  Read(&drawable, &buf);

  DCHECK_LE(buf.offset, 32ul);
}

Future<Dri2::QueryVersionReply> Dri2::QueryVersion(
    const Dri2::QueryVersionRequest& request) {
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

  // minor_version
  buf.Write(&minor_version);

  Align(&buf, 4);

  return connection_->SendRequest<Dri2::QueryVersionReply>(
      &buf, "Dri2::QueryVersion", false);
}

Future<Dri2::QueryVersionReply> Dri2::QueryVersion(
    const uint32_t& major_version,
    const uint32_t& minor_version) {
  return Dri2::QueryVersion(
      Dri2::QueryVersionRequest{major_version, minor_version});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Dri2::QueryVersionReply> detail::ReadReply<
    Dri2::QueryVersionReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Dri2::QueryVersionReply>();

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

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Dri2::ConnectReply> Dri2::Connect(const Dri2::ConnectRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& window = request.window;
  auto& driver_type = request.driver_type;

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

  // driver_type
  uint32_t tmp1;
  tmp1 = static_cast<uint32_t>(driver_type);
  buf.Write(&tmp1);

  Align(&buf, 4);

  return connection_->SendRequest<Dri2::ConnectReply>(&buf, "Dri2::Connect",
                                                      false);
}

Future<Dri2::ConnectReply> Dri2::Connect(const Window& window,
                                         const DriverType& driver_type) {
  return Dri2::Connect(Dri2::ConnectRequest{window, driver_type});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Dri2::ConnectReply> detail::ReadReply<Dri2::ConnectReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Dri2::ConnectReply>();

  auto& sequence = (*reply).sequence;
  uint32_t driver_name_length{};
  uint32_t device_name_length{};
  auto& driver_name = (*reply).driver_name;
  size_t driver_name_len = driver_name.size();
  auto& alignment_pad = (*reply).alignment_pad;
  size_t alignment_pad_len = alignment_pad ? alignment_pad->size() : 0;
  auto& device_name = (*reply).device_name;
  size_t device_name_len = device_name.size();

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

  // driver_name_length
  Read(&driver_name_length, &buf);

  // device_name_length
  Read(&device_name_length, &buf);

  // pad1
  Pad(&buf, 16);

  // driver_name
  driver_name.resize(driver_name_length);
  for (auto& driver_name_elem : driver_name) {
    // driver_name_elem
    Read(&driver_name_elem, &buf);
  }

  // alignment_pad
  alignment_pad = buffer->ReadAndAdvance(
      (BitAnd((driver_name_length) + (3), BitNot(3))) - (driver_name_length));

  // device_name
  device_name.resize(device_name_length);
  for (auto& device_name_elem : device_name) {
    // device_name_elem
    Read(&device_name_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Dri2::AuthenticateReply> Dri2::Authenticate(
    const Dri2::AuthenticateRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& window = request.window;
  auto& magic = request.magic;

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

  // magic
  buf.Write(&magic);

  Align(&buf, 4);

  return connection_->SendRequest<Dri2::AuthenticateReply>(
      &buf, "Dri2::Authenticate", false);
}

Future<Dri2::AuthenticateReply> Dri2::Authenticate(const Window& window,
                                                   const uint32_t& magic) {
  return Dri2::Authenticate(Dri2::AuthenticateRequest{window, magic});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Dri2::AuthenticateReply> detail::ReadReply<
    Dri2::AuthenticateReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Dri2::AuthenticateReply>();

  auto& sequence = (*reply).sequence;
  auto& authenticated = (*reply).authenticated;

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

  // authenticated
  Read(&authenticated, &buf);

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Dri2::CreateDrawable(const Dri2::CreateDrawableRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& drawable = request.drawable;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 3;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // drawable
  buf.Write(&drawable);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Dri2::CreateDrawable", false);
}

Future<void> Dri2::CreateDrawable(const Drawable& drawable) {
  return Dri2::CreateDrawable(Dri2::CreateDrawableRequest{drawable});
}

Future<void> Dri2::DestroyDrawable(
    const Dri2::DestroyDrawableRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& drawable = request.drawable;

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

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Dri2::DestroyDrawable", false);
}

Future<void> Dri2::DestroyDrawable(const Drawable& drawable) {
  return Dri2::DestroyDrawable(Dri2::DestroyDrawableRequest{drawable});
}

Future<Dri2::GetBuffersReply> Dri2::GetBuffers(
    const Dri2::GetBuffersRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& drawable = request.drawable;
  auto& count = request.count;
  auto& attachments = request.attachments;
  size_t attachments_len = attachments.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 5;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // drawable
  buf.Write(&drawable);

  // count
  buf.Write(&count);

  // attachments
  DCHECK_EQ(static_cast<size_t>(attachments_len), attachments.size());
  for (auto& attachments_elem : attachments) {
    // attachments_elem
    buf.Write(&attachments_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<Dri2::GetBuffersReply>(
      &buf, "Dri2::GetBuffers", false);
}

Future<Dri2::GetBuffersReply> Dri2::GetBuffers(
    const Drawable& drawable,
    const uint32_t& count,
    const std::vector<uint32_t>& attachments) {
  return Dri2::GetBuffers(
      Dri2::GetBuffersRequest{drawable, count, attachments});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Dri2::GetBuffersReply> detail::ReadReply<Dri2::GetBuffersReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Dri2::GetBuffersReply>();

  auto& sequence = (*reply).sequence;
  auto& width = (*reply).width;
  auto& height = (*reply).height;
  uint32_t count{};
  auto& buffers = (*reply).buffers;
  size_t buffers_len = buffers.size();

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

  // count
  Read(&count, &buf);

  // pad1
  Pad(&buf, 12);

  // buffers
  buffers.resize(count);
  for (auto& buffers_elem : buffers) {
    // buffers_elem
    {
      auto& attachment = buffers_elem.attachment;
      auto& name = buffers_elem.name;
      auto& pitch = buffers_elem.pitch;
      auto& cpp = buffers_elem.cpp;
      auto& flags = buffers_elem.flags;

      // attachment
      uint32_t tmp2;
      Read(&tmp2, &buf);
      attachment = static_cast<Dri2::Attachment>(tmp2);

      // name
      Read(&name, &buf);

      // pitch
      Read(&pitch, &buf);

      // cpp
      Read(&cpp, &buf);

      // flags
      Read(&flags, &buf);
    }
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Dri2::CopyRegionReply> Dri2::CopyRegion(
    const Dri2::CopyRegionRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& drawable = request.drawable;
  auto& region = request.region;
  auto& dest = request.dest;
  auto& src = request.src;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 6;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // drawable
  buf.Write(&drawable);

  // region
  buf.Write(&region);

  // dest
  buf.Write(&dest);

  // src
  buf.Write(&src);

  Align(&buf, 4);

  return connection_->SendRequest<Dri2::CopyRegionReply>(
      &buf, "Dri2::CopyRegion", false);
}

Future<Dri2::CopyRegionReply> Dri2::CopyRegion(const Drawable& drawable,
                                               const uint32_t& region,
                                               const uint32_t& dest,
                                               const uint32_t& src) {
  return Dri2::CopyRegion(Dri2::CopyRegionRequest{drawable, region, dest, src});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Dri2::CopyRegionReply> detail::ReadReply<Dri2::CopyRegionReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Dri2::CopyRegionReply>();

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

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Dri2::GetBuffersWithFormatReply> Dri2::GetBuffersWithFormat(
    const Dri2::GetBuffersWithFormatRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& drawable = request.drawable;
  auto& count = request.count;
  auto& attachments = request.attachments;
  size_t attachments_len = attachments.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 7;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // drawable
  buf.Write(&drawable);

  // count
  buf.Write(&count);

  // attachments
  DCHECK_EQ(static_cast<size_t>(attachments_len), attachments.size());
  for (auto& attachments_elem : attachments) {
    // attachments_elem
    {
      auto& attachment = attachments_elem.attachment;
      auto& format = attachments_elem.format;

      // attachment
      uint32_t tmp3;
      tmp3 = static_cast<uint32_t>(attachment);
      buf.Write(&tmp3);

      // format
      buf.Write(&format);
    }
  }

  Align(&buf, 4);

  return connection_->SendRequest<Dri2::GetBuffersWithFormatReply>(
      &buf, "Dri2::GetBuffersWithFormat", false);
}

Future<Dri2::GetBuffersWithFormatReply> Dri2::GetBuffersWithFormat(
    const Drawable& drawable,
    const uint32_t& count,
    const std::vector<AttachFormat>& attachments) {
  return Dri2::GetBuffersWithFormat(
      Dri2::GetBuffersWithFormatRequest{drawable, count, attachments});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Dri2::GetBuffersWithFormatReply> detail::ReadReply<
    Dri2::GetBuffersWithFormatReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Dri2::GetBuffersWithFormatReply>();

  auto& sequence = (*reply).sequence;
  auto& width = (*reply).width;
  auto& height = (*reply).height;
  uint32_t count{};
  auto& buffers = (*reply).buffers;
  size_t buffers_len = buffers.size();

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

  // count
  Read(&count, &buf);

  // pad1
  Pad(&buf, 12);

  // buffers
  buffers.resize(count);
  for (auto& buffers_elem : buffers) {
    // buffers_elem
    {
      auto& attachment = buffers_elem.attachment;
      auto& name = buffers_elem.name;
      auto& pitch = buffers_elem.pitch;
      auto& cpp = buffers_elem.cpp;
      auto& flags = buffers_elem.flags;

      // attachment
      uint32_t tmp4;
      Read(&tmp4, &buf);
      attachment = static_cast<Dri2::Attachment>(tmp4);

      // name
      Read(&name, &buf);

      // pitch
      Read(&pitch, &buf);

      // cpp
      Read(&cpp, &buf);

      // flags
      Read(&flags, &buf);
    }
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Dri2::SwapBuffersReply> Dri2::SwapBuffers(
    const Dri2::SwapBuffersRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& drawable = request.drawable;
  auto& target_msc_hi = request.target_msc_hi;
  auto& target_msc_lo = request.target_msc_lo;
  auto& divisor_hi = request.divisor_hi;
  auto& divisor_lo = request.divisor_lo;
  auto& remainder_hi = request.remainder_hi;
  auto& remainder_lo = request.remainder_lo;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 8;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // drawable
  buf.Write(&drawable);

  // target_msc_hi
  buf.Write(&target_msc_hi);

  // target_msc_lo
  buf.Write(&target_msc_lo);

  // divisor_hi
  buf.Write(&divisor_hi);

  // divisor_lo
  buf.Write(&divisor_lo);

  // remainder_hi
  buf.Write(&remainder_hi);

  // remainder_lo
  buf.Write(&remainder_lo);

  Align(&buf, 4);

  return connection_->SendRequest<Dri2::SwapBuffersReply>(
      &buf, "Dri2::SwapBuffers", false);
}

Future<Dri2::SwapBuffersReply> Dri2::SwapBuffers(const Drawable& drawable,
                                                 const uint32_t& target_msc_hi,
                                                 const uint32_t& target_msc_lo,
                                                 const uint32_t& divisor_hi,
                                                 const uint32_t& divisor_lo,
                                                 const uint32_t& remainder_hi,
                                                 const uint32_t& remainder_lo) {
  return Dri2::SwapBuffers(Dri2::SwapBuffersRequest{
      drawable, target_msc_hi, target_msc_lo, divisor_hi, divisor_lo,
      remainder_hi, remainder_lo});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Dri2::SwapBuffersReply> detail::ReadReply<
    Dri2::SwapBuffersReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Dri2::SwapBuffersReply>();

  auto& sequence = (*reply).sequence;
  auto& swap_hi = (*reply).swap_hi;
  auto& swap_lo = (*reply).swap_lo;

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

  // swap_hi
  Read(&swap_hi, &buf);

  // swap_lo
  Read(&swap_lo, &buf);

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Dri2::GetMSCReply> Dri2::GetMSC(const Dri2::GetMSCRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& drawable = request.drawable;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 9;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // drawable
  buf.Write(&drawable);

  Align(&buf, 4);

  return connection_->SendRequest<Dri2::GetMSCReply>(&buf, "Dri2::GetMSC",
                                                     false);
}

Future<Dri2::GetMSCReply> Dri2::GetMSC(const Drawable& drawable) {
  return Dri2::GetMSC(Dri2::GetMSCRequest{drawable});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Dri2::GetMSCReply> detail::ReadReply<Dri2::GetMSCReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Dri2::GetMSCReply>();

  auto& sequence = (*reply).sequence;
  auto& ust_hi = (*reply).ust_hi;
  auto& ust_lo = (*reply).ust_lo;
  auto& msc_hi = (*reply).msc_hi;
  auto& msc_lo = (*reply).msc_lo;
  auto& sbc_hi = (*reply).sbc_hi;
  auto& sbc_lo = (*reply).sbc_lo;

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

  // ust_hi
  Read(&ust_hi, &buf);

  // ust_lo
  Read(&ust_lo, &buf);

  // msc_hi
  Read(&msc_hi, &buf);

  // msc_lo
  Read(&msc_lo, &buf);

  // sbc_hi
  Read(&sbc_hi, &buf);

  // sbc_lo
  Read(&sbc_lo, &buf);

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Dri2::WaitMSCReply> Dri2::WaitMSC(const Dri2::WaitMSCRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& drawable = request.drawable;
  auto& target_msc_hi = request.target_msc_hi;
  auto& target_msc_lo = request.target_msc_lo;
  auto& divisor_hi = request.divisor_hi;
  auto& divisor_lo = request.divisor_lo;
  auto& remainder_hi = request.remainder_hi;
  auto& remainder_lo = request.remainder_lo;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 10;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // drawable
  buf.Write(&drawable);

  // target_msc_hi
  buf.Write(&target_msc_hi);

  // target_msc_lo
  buf.Write(&target_msc_lo);

  // divisor_hi
  buf.Write(&divisor_hi);

  // divisor_lo
  buf.Write(&divisor_lo);

  // remainder_hi
  buf.Write(&remainder_hi);

  // remainder_lo
  buf.Write(&remainder_lo);

  Align(&buf, 4);

  return connection_->SendRequest<Dri2::WaitMSCReply>(&buf, "Dri2::WaitMSC",
                                                      false);
}

Future<Dri2::WaitMSCReply> Dri2::WaitMSC(const Drawable& drawable,
                                         const uint32_t& target_msc_hi,
                                         const uint32_t& target_msc_lo,
                                         const uint32_t& divisor_hi,
                                         const uint32_t& divisor_lo,
                                         const uint32_t& remainder_hi,
                                         const uint32_t& remainder_lo) {
  return Dri2::WaitMSC(
      Dri2::WaitMSCRequest{drawable, target_msc_hi, target_msc_lo, divisor_hi,
                           divisor_lo, remainder_hi, remainder_lo});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Dri2::WaitMSCReply> detail::ReadReply<Dri2::WaitMSCReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Dri2::WaitMSCReply>();

  auto& sequence = (*reply).sequence;
  auto& ust_hi = (*reply).ust_hi;
  auto& ust_lo = (*reply).ust_lo;
  auto& msc_hi = (*reply).msc_hi;
  auto& msc_lo = (*reply).msc_lo;
  auto& sbc_hi = (*reply).sbc_hi;
  auto& sbc_lo = (*reply).sbc_lo;

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

  // ust_hi
  Read(&ust_hi, &buf);

  // ust_lo
  Read(&ust_lo, &buf);

  // msc_hi
  Read(&msc_hi, &buf);

  // msc_lo
  Read(&msc_lo, &buf);

  // sbc_hi
  Read(&sbc_hi, &buf);

  // sbc_lo
  Read(&sbc_lo, &buf);

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Dri2::WaitSBCReply> Dri2::WaitSBC(const Dri2::WaitSBCRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& drawable = request.drawable;
  auto& target_sbc_hi = request.target_sbc_hi;
  auto& target_sbc_lo = request.target_sbc_lo;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 11;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // drawable
  buf.Write(&drawable);

  // target_sbc_hi
  buf.Write(&target_sbc_hi);

  // target_sbc_lo
  buf.Write(&target_sbc_lo);

  Align(&buf, 4);

  return connection_->SendRequest<Dri2::WaitSBCReply>(&buf, "Dri2::WaitSBC",
                                                      false);
}

Future<Dri2::WaitSBCReply> Dri2::WaitSBC(const Drawable& drawable,
                                         const uint32_t& target_sbc_hi,
                                         const uint32_t& target_sbc_lo) {
  return Dri2::WaitSBC(
      Dri2::WaitSBCRequest{drawable, target_sbc_hi, target_sbc_lo});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Dri2::WaitSBCReply> detail::ReadReply<Dri2::WaitSBCReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Dri2::WaitSBCReply>();

  auto& sequence = (*reply).sequence;
  auto& ust_hi = (*reply).ust_hi;
  auto& ust_lo = (*reply).ust_lo;
  auto& msc_hi = (*reply).msc_hi;
  auto& msc_lo = (*reply).msc_lo;
  auto& sbc_hi = (*reply).sbc_hi;
  auto& sbc_lo = (*reply).sbc_lo;

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

  // ust_hi
  Read(&ust_hi, &buf);

  // ust_lo
  Read(&ust_lo, &buf);

  // msc_hi
  Read(&msc_hi, &buf);

  // msc_lo
  Read(&msc_lo, &buf);

  // sbc_hi
  Read(&sbc_hi, &buf);

  // sbc_lo
  Read(&sbc_lo, &buf);

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Dri2::SwapInterval(const Dri2::SwapIntervalRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& drawable = request.drawable;
  auto& interval = request.interval;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 12;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // drawable
  buf.Write(&drawable);

  // interval
  buf.Write(&interval);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Dri2::SwapInterval", false);
}

Future<void> Dri2::SwapInterval(const Drawable& drawable,
                                const uint32_t& interval) {
  return Dri2::SwapInterval(Dri2::SwapIntervalRequest{drawable, interval});
}

Future<Dri2::GetParamReply> Dri2::GetParam(
    const Dri2::GetParamRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& drawable = request.drawable;
  auto& param = request.param;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 13;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // drawable
  buf.Write(&drawable);

  // param
  buf.Write(&param);

  Align(&buf, 4);

  return connection_->SendRequest<Dri2::GetParamReply>(&buf, "Dri2::GetParam",
                                                       false);
}

Future<Dri2::GetParamReply> Dri2::GetParam(const Drawable& drawable,
                                           const uint32_t& param) {
  return Dri2::GetParam(Dri2::GetParamRequest{drawable, param});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Dri2::GetParamReply> detail::ReadReply<Dri2::GetParamReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Dri2::GetParamReply>();

  auto& is_param_recognized = (*reply).is_param_recognized;
  auto& sequence = (*reply).sequence;
  auto& value_hi = (*reply).value_hi;
  auto& value_lo = (*reply).value_lo;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // is_param_recognized
  Read(&is_param_recognized, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // value_hi
  Read(&value_hi, &buf);

  // value_lo
  Read(&value_lo, &buf);

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

}  // namespace x11
