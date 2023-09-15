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

#include "xf86vidmode.h"

#include <unistd.h>
#include <xcb/xcb.h>
#include <xcb/xcbext.h>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "ui/gfx/x/xproto_internal.h"

namespace x11 {

XF86VidMode::XF86VidMode(Connection* connection,
                         const x11::QueryExtensionReply& info)
    : connection_(connection), info_(info) {}

std::string XF86VidMode::BadClockError::ToString() const {
  std::stringstream ss_;
  ss_ << "XF86VidMode::BadClockError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<XF86VidMode::BadClockError>(XF86VidMode::BadClockError* error_,
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
std::string XF86VidMode::BadHTimingsError::ToString() const {
  std::stringstream ss_;
  ss_ << "XF86VidMode::BadHTimingsError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<XF86VidMode::BadHTimingsError>(
    XF86VidMode::BadHTimingsError* error_,
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
std::string XF86VidMode::BadVTimingsError::ToString() const {
  std::stringstream ss_;
  ss_ << "XF86VidMode::BadVTimingsError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<XF86VidMode::BadVTimingsError>(
    XF86VidMode::BadVTimingsError* error_,
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
std::string XF86VidMode::ModeUnsuitableError::ToString() const {
  std::stringstream ss_;
  ss_ << "XF86VidMode::ModeUnsuitableError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<XF86VidMode::ModeUnsuitableError>(
    XF86VidMode::ModeUnsuitableError* error_,
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
std::string XF86VidMode::ExtensionDisabledError::ToString() const {
  std::stringstream ss_;
  ss_ << "XF86VidMode::ExtensionDisabledError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<XF86VidMode::ExtensionDisabledError>(
    XF86VidMode::ExtensionDisabledError* error_,
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
std::string XF86VidMode::ClientNotLocalError::ToString() const {
  std::stringstream ss_;
  ss_ << "XF86VidMode::ClientNotLocalError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<XF86VidMode::ClientNotLocalError>(
    XF86VidMode::ClientNotLocalError* error_,
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
std::string XF86VidMode::ZoomLockedError::ToString() const {
  std::stringstream ss_;
  ss_ << "XF86VidMode::ZoomLockedError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<XF86VidMode::ZoomLockedError>(
    XF86VidMode::ZoomLockedError* error_,
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
Future<XF86VidMode::QueryVersionReply> XF86VidMode::QueryVersion(
    const XF86VidMode::QueryVersionRequest& request) {
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

  return connection_->SendRequest<XF86VidMode::QueryVersionReply>(
      &buf, "XF86VidMode::QueryVersion", false);
}

Future<XF86VidMode::QueryVersionReply> XF86VidMode::QueryVersion() {
  return XF86VidMode::QueryVersion(XF86VidMode::QueryVersionRequest{});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<XF86VidMode::QueryVersionReply> detail::ReadReply<
    XF86VidMode::QueryVersionReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<XF86VidMode::QueryVersionReply>();

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
  DUMP_WILL_BE_CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<XF86VidMode::GetModeLineReply> XF86VidMode::GetModeLine(
    const XF86VidMode::GetModeLineRequest& request) {
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

  // pad0
  Pad(&buf, 2);

  Align(&buf, 4);

  return connection_->SendRequest<XF86VidMode::GetModeLineReply>(
      &buf, "XF86VidMode::GetModeLine", false);
}

Future<XF86VidMode::GetModeLineReply> XF86VidMode::GetModeLine(
    const uint16_t& screen) {
  return XF86VidMode::GetModeLine(XF86VidMode::GetModeLineRequest{screen});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<XF86VidMode::GetModeLineReply> detail::ReadReply<
    XF86VidMode::GetModeLineReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<XF86VidMode::GetModeLineReply>();

  auto& sequence = (*reply).sequence;
  auto& dotclock = (*reply).dotclock;
  auto& hdisplay = (*reply).hdisplay;
  auto& hsyncstart = (*reply).hsyncstart;
  auto& hsyncend = (*reply).hsyncend;
  auto& htotal = (*reply).htotal;
  auto& hskew = (*reply).hskew;
  auto& vdisplay = (*reply).vdisplay;
  auto& vsyncstart = (*reply).vsyncstart;
  auto& vsyncend = (*reply).vsyncend;
  auto& vtotal = (*reply).vtotal;
  auto& flags = (*reply).flags;
  uint32_t privsize{};
  auto& c_private = (*reply).c_private;
  size_t c_private_len = c_private.size();

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

  // dotclock
  Read(&dotclock, &buf);

  // hdisplay
  Read(&hdisplay, &buf);

  // hsyncstart
  Read(&hsyncstart, &buf);

  // hsyncend
  Read(&hsyncend, &buf);

  // htotal
  Read(&htotal, &buf);

  // hskew
  Read(&hskew, &buf);

  // vdisplay
  Read(&vdisplay, &buf);

  // vsyncstart
  Read(&vsyncstart, &buf);

  // vsyncend
  Read(&vsyncend, &buf);

  // vtotal
  Read(&vtotal, &buf);

  // pad1
  Pad(&buf, 2);

  // flags
  uint32_t tmp0;
  Read(&tmp0, &buf);
  flags = static_cast<XF86VidMode::ModeFlag>(tmp0);

  // pad2
  Pad(&buf, 12);

  // privsize
  Read(&privsize, &buf);

  // c_private
  c_private.resize(privsize);
  for (auto& c_private_elem : c_private) {
    // c_private_elem
    Read(&c_private_elem, &buf);
  }

  Align(&buf, 4);
  DUMP_WILL_BE_CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> XF86VidMode::ModModeLine(
    const XF86VidMode::ModModeLineRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& screen = request.screen;
  auto& hdisplay = request.hdisplay;
  auto& hsyncstart = request.hsyncstart;
  auto& hsyncend = request.hsyncend;
  auto& htotal = request.htotal;
  auto& hskew = request.hskew;
  auto& vdisplay = request.vdisplay;
  auto& vsyncstart = request.vsyncstart;
  auto& vsyncend = request.vsyncend;
  auto& vtotal = request.vtotal;
  auto& flags = request.flags;
  uint32_t privsize{};
  auto& c_private = request.c_private;
  size_t c_private_len = c_private.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 2;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // screen
  buf.Write(&screen);

  // hdisplay
  buf.Write(&hdisplay);

  // hsyncstart
  buf.Write(&hsyncstart);

  // hsyncend
  buf.Write(&hsyncend);

  // htotal
  buf.Write(&htotal);

  // hskew
  buf.Write(&hskew);

  // vdisplay
  buf.Write(&vdisplay);

  // vsyncstart
  buf.Write(&vsyncstart);

  // vsyncend
  buf.Write(&vsyncend);

  // vtotal
  buf.Write(&vtotal);

  // pad0
  Pad(&buf, 2);

  // flags
  uint32_t tmp1;
  tmp1 = static_cast<uint32_t>(flags);
  buf.Write(&tmp1);

  // pad1
  Pad(&buf, 12);

  // privsize
  privsize = c_private.size();
  buf.Write(&privsize);

  // c_private
  DUMP_WILL_BE_CHECK_EQ(static_cast<size_t>(privsize), c_private.size());
  for (auto& c_private_elem : c_private) {
    // c_private_elem
    buf.Write(&c_private_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "XF86VidMode::ModModeLine",
                                        false);
}

Future<void> XF86VidMode::ModModeLine(const uint32_t& screen,
                                      const uint16_t& hdisplay,
                                      const uint16_t& hsyncstart,
                                      const uint16_t& hsyncend,
                                      const uint16_t& htotal,
                                      const uint16_t& hskew,
                                      const uint16_t& vdisplay,
                                      const uint16_t& vsyncstart,
                                      const uint16_t& vsyncend,
                                      const uint16_t& vtotal,
                                      const ModeFlag& flags,
                                      const std::vector<uint8_t>& c_private) {
  return XF86VidMode::ModModeLine(XF86VidMode::ModModeLineRequest{
      screen, hdisplay, hsyncstart, hsyncend, htotal, hskew, vdisplay,
      vsyncstart, vsyncend, vtotal, flags, c_private});
}

Future<void> XF86VidMode::SwitchMode(
    const XF86VidMode::SwitchModeRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& screen = request.screen;
  auto& zoom = request.zoom;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 3;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // screen
  buf.Write(&screen);

  // zoom
  buf.Write(&zoom);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "XF86VidMode::SwitchMode", false);
}

Future<void> XF86VidMode::SwitchMode(const uint16_t& screen,
                                     const uint16_t& zoom) {
  return XF86VidMode::SwitchMode(XF86VidMode::SwitchModeRequest{screen, zoom});
}

Future<XF86VidMode::GetMonitorReply> XF86VidMode::GetMonitor(
    const XF86VidMode::GetMonitorRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& screen = request.screen;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 4;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // screen
  buf.Write(&screen);

  // pad0
  Pad(&buf, 2);

  Align(&buf, 4);

  return connection_->SendRequest<XF86VidMode::GetMonitorReply>(
      &buf, "XF86VidMode::GetMonitor", false);
}

Future<XF86VidMode::GetMonitorReply> XF86VidMode::GetMonitor(
    const uint16_t& screen) {
  return XF86VidMode::GetMonitor(XF86VidMode::GetMonitorRequest{screen});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<XF86VidMode::GetMonitorReply> detail::ReadReply<
    XF86VidMode::GetMonitorReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<XF86VidMode::GetMonitorReply>();

  auto& sequence = (*reply).sequence;
  uint8_t vendor_length{};
  uint8_t model_length{};
  uint8_t num_hsync{};
  uint8_t num_vsync{};
  auto& hsync = (*reply).hsync;
  size_t hsync_len = hsync.size();
  auto& vsync = (*reply).vsync;
  size_t vsync_len = vsync.size();
  auto& vendor = (*reply).vendor;
  size_t vendor_len = vendor.size();
  auto& alignment_pad = (*reply).alignment_pad;
  size_t alignment_pad_len = alignment_pad ? alignment_pad->size() : 0;
  auto& model = (*reply).model;
  size_t model_len = model.size();

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

  // vendor_length
  Read(&vendor_length, &buf);

  // model_length
  Read(&model_length, &buf);

  // num_hsync
  Read(&num_hsync, &buf);

  // num_vsync
  Read(&num_vsync, &buf);

  // pad1
  Pad(&buf, 20);

  // hsync
  hsync.resize(num_hsync);
  for (auto& hsync_elem : hsync) {
    // hsync_elem
    Read(&hsync_elem, &buf);
  }

  // vsync
  vsync.resize(num_vsync);
  for (auto& vsync_elem : vsync) {
    // vsync_elem
    Read(&vsync_elem, &buf);
  }

  // vendor
  vendor.resize(vendor_length);
  for (auto& vendor_elem : vendor) {
    // vendor_elem
    Read(&vendor_elem, &buf);
  }

  // alignment_pad
  alignment_pad = buffer->ReadAndAdvance(
      (BitAnd((vendor_length) + (3), BitNot(3))) - (vendor_length));

  // model
  model.resize(model_length);
  for (auto& model_elem : model) {
    // model_elem
    Read(&model_elem, &buf);
  }

  Align(&buf, 4);
  DUMP_WILL_BE_CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> XF86VidMode::LockModeSwitch(
    const XF86VidMode::LockModeSwitchRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& screen = request.screen;
  auto& lock = request.lock;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 5;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // screen
  buf.Write(&screen);

  // lock
  buf.Write(&lock);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "XF86VidMode::LockModeSwitch",
                                        false);
}

Future<void> XF86VidMode::LockModeSwitch(const uint16_t& screen,
                                         const uint16_t& lock) {
  return XF86VidMode::LockModeSwitch(
      XF86VidMode::LockModeSwitchRequest{screen, lock});
}

Future<XF86VidMode::GetAllModeLinesReply> XF86VidMode::GetAllModeLines(
    const XF86VidMode::GetAllModeLinesRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& screen = request.screen;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 6;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // screen
  buf.Write(&screen);

  // pad0
  Pad(&buf, 2);

  Align(&buf, 4);

  return connection_->SendRequest<XF86VidMode::GetAllModeLinesReply>(
      &buf, "XF86VidMode::GetAllModeLines", false);
}

Future<XF86VidMode::GetAllModeLinesReply> XF86VidMode::GetAllModeLines(
    const uint16_t& screen) {
  return XF86VidMode::GetAllModeLines(
      XF86VidMode::GetAllModeLinesRequest{screen});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<XF86VidMode::GetAllModeLinesReply> detail::ReadReply<
    XF86VidMode::GetAllModeLinesReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<XF86VidMode::GetAllModeLinesReply>();

  auto& sequence = (*reply).sequence;
  uint32_t modecount{};
  auto& modeinfo = (*reply).modeinfo;
  size_t modeinfo_len = modeinfo.size();

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

  // modecount
  Read(&modecount, &buf);

  // pad1
  Pad(&buf, 20);

  // modeinfo
  modeinfo.resize(modecount);
  for (auto& modeinfo_elem : modeinfo) {
    // modeinfo_elem
    {
      auto& dotclock = modeinfo_elem.dotclock;
      auto& hdisplay = modeinfo_elem.hdisplay;
      auto& hsyncstart = modeinfo_elem.hsyncstart;
      auto& hsyncend = modeinfo_elem.hsyncend;
      auto& htotal = modeinfo_elem.htotal;
      auto& hskew = modeinfo_elem.hskew;
      auto& vdisplay = modeinfo_elem.vdisplay;
      auto& vsyncstart = modeinfo_elem.vsyncstart;
      auto& vsyncend = modeinfo_elem.vsyncend;
      auto& vtotal = modeinfo_elem.vtotal;
      auto& flags = modeinfo_elem.flags;
      auto& privsize = modeinfo_elem.privsize;

      // dotclock
      Read(&dotclock, &buf);

      // hdisplay
      Read(&hdisplay, &buf);

      // hsyncstart
      Read(&hsyncstart, &buf);

      // hsyncend
      Read(&hsyncend, &buf);

      // htotal
      Read(&htotal, &buf);

      // hskew
      Read(&hskew, &buf);

      // vdisplay
      Read(&vdisplay, &buf);

      // vsyncstart
      Read(&vsyncstart, &buf);

      // vsyncend
      Read(&vsyncend, &buf);

      // vtotal
      Read(&vtotal, &buf);

      // pad0
      Pad(&buf, 4);

      // flags
      uint32_t tmp2;
      Read(&tmp2, &buf);
      flags = static_cast<XF86VidMode::ModeFlag>(tmp2);

      // pad1
      Pad(&buf, 12);

      // privsize
      Read(&privsize, &buf);
    }
  }

  Align(&buf, 4);
  DUMP_WILL_BE_CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> XF86VidMode::AddModeLine(
    const XF86VidMode::AddModeLineRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& screen = request.screen;
  auto& dotclock = request.dotclock;
  auto& hdisplay = request.hdisplay;
  auto& hsyncstart = request.hsyncstart;
  auto& hsyncend = request.hsyncend;
  auto& htotal = request.htotal;
  auto& hskew = request.hskew;
  auto& vdisplay = request.vdisplay;
  auto& vsyncstart = request.vsyncstart;
  auto& vsyncend = request.vsyncend;
  auto& vtotal = request.vtotal;
  auto& flags = request.flags;
  uint32_t privsize{};
  auto& after_dotclock = request.after_dotclock;
  auto& after_hdisplay = request.after_hdisplay;
  auto& after_hsyncstart = request.after_hsyncstart;
  auto& after_hsyncend = request.after_hsyncend;
  auto& after_htotal = request.after_htotal;
  auto& after_hskew = request.after_hskew;
  auto& after_vdisplay = request.after_vdisplay;
  auto& after_vsyncstart = request.after_vsyncstart;
  auto& after_vsyncend = request.after_vsyncend;
  auto& after_vtotal = request.after_vtotal;
  auto& after_flags = request.after_flags;
  auto& c_private = request.c_private;
  size_t c_private_len = c_private.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 7;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // screen
  buf.Write(&screen);

  // dotclock
  buf.Write(&dotclock);

  // hdisplay
  buf.Write(&hdisplay);

  // hsyncstart
  buf.Write(&hsyncstart);

  // hsyncend
  buf.Write(&hsyncend);

  // htotal
  buf.Write(&htotal);

  // hskew
  buf.Write(&hskew);

  // vdisplay
  buf.Write(&vdisplay);

  // vsyncstart
  buf.Write(&vsyncstart);

  // vsyncend
  buf.Write(&vsyncend);

  // vtotal
  buf.Write(&vtotal);

  // pad0
  Pad(&buf, 2);

  // flags
  uint32_t tmp3;
  tmp3 = static_cast<uint32_t>(flags);
  buf.Write(&tmp3);

  // pad1
  Pad(&buf, 12);

  // privsize
  privsize = c_private.size();
  buf.Write(&privsize);

  // after_dotclock
  buf.Write(&after_dotclock);

  // after_hdisplay
  buf.Write(&after_hdisplay);

  // after_hsyncstart
  buf.Write(&after_hsyncstart);

  // after_hsyncend
  buf.Write(&after_hsyncend);

  // after_htotal
  buf.Write(&after_htotal);

  // after_hskew
  buf.Write(&after_hskew);

  // after_vdisplay
  buf.Write(&after_vdisplay);

  // after_vsyncstart
  buf.Write(&after_vsyncstart);

  // after_vsyncend
  buf.Write(&after_vsyncend);

  // after_vtotal
  buf.Write(&after_vtotal);

  // pad2
  Pad(&buf, 2);

  // after_flags
  uint32_t tmp4;
  tmp4 = static_cast<uint32_t>(after_flags);
  buf.Write(&tmp4);

  // pad3
  Pad(&buf, 12);

  // c_private
  DUMP_WILL_BE_CHECK_EQ(static_cast<size_t>(privsize), c_private.size());
  for (auto& c_private_elem : c_private) {
    // c_private_elem
    buf.Write(&c_private_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "XF86VidMode::AddModeLine",
                                        false);
}

Future<void> XF86VidMode::AddModeLine(const uint32_t& screen,
                                      const DotClock& dotclock,
                                      const uint16_t& hdisplay,
                                      const uint16_t& hsyncstart,
                                      const uint16_t& hsyncend,
                                      const uint16_t& htotal,
                                      const uint16_t& hskew,
                                      const uint16_t& vdisplay,
                                      const uint16_t& vsyncstart,
                                      const uint16_t& vsyncend,
                                      const uint16_t& vtotal,
                                      const ModeFlag& flags,
                                      const DotClock& after_dotclock,
                                      const uint16_t& after_hdisplay,
                                      const uint16_t& after_hsyncstart,
                                      const uint16_t& after_hsyncend,
                                      const uint16_t& after_htotal,
                                      const uint16_t& after_hskew,
                                      const uint16_t& after_vdisplay,
                                      const uint16_t& after_vsyncstart,
                                      const uint16_t& after_vsyncend,
                                      const uint16_t& after_vtotal,
                                      const ModeFlag& after_flags,
                                      const std::vector<uint8_t>& c_private) {
  return XF86VidMode::AddModeLine(XF86VidMode::AddModeLineRequest{
      screen,         dotclock,         hdisplay,
      hsyncstart,     hsyncend,         htotal,
      hskew,          vdisplay,         vsyncstart,
      vsyncend,       vtotal,           flags,
      after_dotclock, after_hdisplay,   after_hsyncstart,
      after_hsyncend, after_htotal,     after_hskew,
      after_vdisplay, after_vsyncstart, after_vsyncend,
      after_vtotal,   after_flags,      c_private});
}

Future<void> XF86VidMode::DeleteModeLine(
    const XF86VidMode::DeleteModeLineRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& screen = request.screen;
  auto& dotclock = request.dotclock;
  auto& hdisplay = request.hdisplay;
  auto& hsyncstart = request.hsyncstart;
  auto& hsyncend = request.hsyncend;
  auto& htotal = request.htotal;
  auto& hskew = request.hskew;
  auto& vdisplay = request.vdisplay;
  auto& vsyncstart = request.vsyncstart;
  auto& vsyncend = request.vsyncend;
  auto& vtotal = request.vtotal;
  auto& flags = request.flags;
  uint32_t privsize{};
  auto& c_private = request.c_private;
  size_t c_private_len = c_private.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 8;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // screen
  buf.Write(&screen);

  // dotclock
  buf.Write(&dotclock);

  // hdisplay
  buf.Write(&hdisplay);

  // hsyncstart
  buf.Write(&hsyncstart);

  // hsyncend
  buf.Write(&hsyncend);

  // htotal
  buf.Write(&htotal);

  // hskew
  buf.Write(&hskew);

  // vdisplay
  buf.Write(&vdisplay);

  // vsyncstart
  buf.Write(&vsyncstart);

  // vsyncend
  buf.Write(&vsyncend);

  // vtotal
  buf.Write(&vtotal);

  // pad0
  Pad(&buf, 2);

  // flags
  uint32_t tmp5;
  tmp5 = static_cast<uint32_t>(flags);
  buf.Write(&tmp5);

  // pad1
  Pad(&buf, 12);

  // privsize
  privsize = c_private.size();
  buf.Write(&privsize);

  // c_private
  DUMP_WILL_BE_CHECK_EQ(static_cast<size_t>(privsize), c_private.size());
  for (auto& c_private_elem : c_private) {
    // c_private_elem
    buf.Write(&c_private_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "XF86VidMode::DeleteModeLine",
                                        false);
}

Future<void> XF86VidMode::DeleteModeLine(
    const uint32_t& screen,
    const DotClock& dotclock,
    const uint16_t& hdisplay,
    const uint16_t& hsyncstart,
    const uint16_t& hsyncend,
    const uint16_t& htotal,
    const uint16_t& hskew,
    const uint16_t& vdisplay,
    const uint16_t& vsyncstart,
    const uint16_t& vsyncend,
    const uint16_t& vtotal,
    const ModeFlag& flags,
    const std::vector<uint8_t>& c_private) {
  return XF86VidMode::DeleteModeLine(XF86VidMode::DeleteModeLineRequest{
      screen, dotclock, hdisplay, hsyncstart, hsyncend, htotal, hskew, vdisplay,
      vsyncstart, vsyncend, vtotal, flags, c_private});
}

Future<XF86VidMode::ValidateModeLineReply> XF86VidMode::ValidateModeLine(
    const XF86VidMode::ValidateModeLineRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& screen = request.screen;
  auto& dotclock = request.dotclock;
  auto& hdisplay = request.hdisplay;
  auto& hsyncstart = request.hsyncstart;
  auto& hsyncend = request.hsyncend;
  auto& htotal = request.htotal;
  auto& hskew = request.hskew;
  auto& vdisplay = request.vdisplay;
  auto& vsyncstart = request.vsyncstart;
  auto& vsyncend = request.vsyncend;
  auto& vtotal = request.vtotal;
  auto& flags = request.flags;
  uint32_t privsize{};
  auto& c_private = request.c_private;
  size_t c_private_len = c_private.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 9;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // screen
  buf.Write(&screen);

  // dotclock
  buf.Write(&dotclock);

  // hdisplay
  buf.Write(&hdisplay);

  // hsyncstart
  buf.Write(&hsyncstart);

  // hsyncend
  buf.Write(&hsyncend);

  // htotal
  buf.Write(&htotal);

  // hskew
  buf.Write(&hskew);

  // vdisplay
  buf.Write(&vdisplay);

  // vsyncstart
  buf.Write(&vsyncstart);

  // vsyncend
  buf.Write(&vsyncend);

  // vtotal
  buf.Write(&vtotal);

  // pad0
  Pad(&buf, 2);

  // flags
  uint32_t tmp6;
  tmp6 = static_cast<uint32_t>(flags);
  buf.Write(&tmp6);

  // pad1
  Pad(&buf, 12);

  // privsize
  privsize = c_private.size();
  buf.Write(&privsize);

  // c_private
  DUMP_WILL_BE_CHECK_EQ(static_cast<size_t>(privsize), c_private.size());
  for (auto& c_private_elem : c_private) {
    // c_private_elem
    buf.Write(&c_private_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<XF86VidMode::ValidateModeLineReply>(
      &buf, "XF86VidMode::ValidateModeLine", false);
}

Future<XF86VidMode::ValidateModeLineReply> XF86VidMode::ValidateModeLine(
    const uint32_t& screen,
    const DotClock& dotclock,
    const uint16_t& hdisplay,
    const uint16_t& hsyncstart,
    const uint16_t& hsyncend,
    const uint16_t& htotal,
    const uint16_t& hskew,
    const uint16_t& vdisplay,
    const uint16_t& vsyncstart,
    const uint16_t& vsyncend,
    const uint16_t& vtotal,
    const ModeFlag& flags,
    const std::vector<uint8_t>& c_private) {
  return XF86VidMode::ValidateModeLine(XF86VidMode::ValidateModeLineRequest{
      screen, dotclock, hdisplay, hsyncstart, hsyncend, htotal, hskew, vdisplay,
      vsyncstart, vsyncend, vtotal, flags, c_private});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<XF86VidMode::ValidateModeLineReply> detail::ReadReply<
    XF86VidMode::ValidateModeLineReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<XF86VidMode::ValidateModeLineReply>();

  auto& sequence = (*reply).sequence;
  auto& status = (*reply).status;

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

  // status
  Read(&status, &buf);

  // pad1
  Pad(&buf, 20);

  Align(&buf, 4);
  DUMP_WILL_BE_CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> XF86VidMode::SwitchToMode(
    const XF86VidMode::SwitchToModeRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& screen = request.screen;
  auto& dotclock = request.dotclock;
  auto& hdisplay = request.hdisplay;
  auto& hsyncstart = request.hsyncstart;
  auto& hsyncend = request.hsyncend;
  auto& htotal = request.htotal;
  auto& hskew = request.hskew;
  auto& vdisplay = request.vdisplay;
  auto& vsyncstart = request.vsyncstart;
  auto& vsyncend = request.vsyncend;
  auto& vtotal = request.vtotal;
  auto& flags = request.flags;
  uint32_t privsize{};
  auto& c_private = request.c_private;
  size_t c_private_len = c_private.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 10;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // screen
  buf.Write(&screen);

  // dotclock
  buf.Write(&dotclock);

  // hdisplay
  buf.Write(&hdisplay);

  // hsyncstart
  buf.Write(&hsyncstart);

  // hsyncend
  buf.Write(&hsyncend);

  // htotal
  buf.Write(&htotal);

  // hskew
  buf.Write(&hskew);

  // vdisplay
  buf.Write(&vdisplay);

  // vsyncstart
  buf.Write(&vsyncstart);

  // vsyncend
  buf.Write(&vsyncend);

  // vtotal
  buf.Write(&vtotal);

  // pad0
  Pad(&buf, 2);

  // flags
  uint32_t tmp7;
  tmp7 = static_cast<uint32_t>(flags);
  buf.Write(&tmp7);

  // pad1
  Pad(&buf, 12);

  // privsize
  privsize = c_private.size();
  buf.Write(&privsize);

  // c_private
  DUMP_WILL_BE_CHECK_EQ(static_cast<size_t>(privsize), c_private.size());
  for (auto& c_private_elem : c_private) {
    // c_private_elem
    buf.Write(&c_private_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "XF86VidMode::SwitchToMode",
                                        false);
}

Future<void> XF86VidMode::SwitchToMode(const uint32_t& screen,
                                       const DotClock& dotclock,
                                       const uint16_t& hdisplay,
                                       const uint16_t& hsyncstart,
                                       const uint16_t& hsyncend,
                                       const uint16_t& htotal,
                                       const uint16_t& hskew,
                                       const uint16_t& vdisplay,
                                       const uint16_t& vsyncstart,
                                       const uint16_t& vsyncend,
                                       const uint16_t& vtotal,
                                       const ModeFlag& flags,
                                       const std::vector<uint8_t>& c_private) {
  return XF86VidMode::SwitchToMode(XF86VidMode::SwitchToModeRequest{
      screen, dotclock, hdisplay, hsyncstart, hsyncend, htotal, hskew, vdisplay,
      vsyncstart, vsyncend, vtotal, flags, c_private});
}

Future<XF86VidMode::GetViewPortReply> XF86VidMode::GetViewPort(
    const XF86VidMode::GetViewPortRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& screen = request.screen;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 11;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // screen
  buf.Write(&screen);

  // pad0
  Pad(&buf, 2);

  Align(&buf, 4);

  return connection_->SendRequest<XF86VidMode::GetViewPortReply>(
      &buf, "XF86VidMode::GetViewPort", false);
}

Future<XF86VidMode::GetViewPortReply> XF86VidMode::GetViewPort(
    const uint16_t& screen) {
  return XF86VidMode::GetViewPort(XF86VidMode::GetViewPortRequest{screen});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<XF86VidMode::GetViewPortReply> detail::ReadReply<
    XF86VidMode::GetViewPortReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<XF86VidMode::GetViewPortReply>();

  auto& sequence = (*reply).sequence;
  auto& x = (*reply).x;
  auto& y = (*reply).y;

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

  // x
  Read(&x, &buf);

  // y
  Read(&y, &buf);

  // pad1
  Pad(&buf, 16);

  Align(&buf, 4);
  DUMP_WILL_BE_CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> XF86VidMode::SetViewPort(
    const XF86VidMode::SetViewPortRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& screen = request.screen;
  auto& x = request.x;
  auto& y = request.y;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 12;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // screen
  buf.Write(&screen);

  // pad0
  Pad(&buf, 2);

  // x
  buf.Write(&x);

  // y
  buf.Write(&y);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "XF86VidMode::SetViewPort",
                                        false);
}

Future<void> XF86VidMode::SetViewPort(const uint16_t& screen,
                                      const uint32_t& x,
                                      const uint32_t& y) {
  return XF86VidMode::SetViewPort(
      XF86VidMode::SetViewPortRequest{screen, x, y});
}

Future<XF86VidMode::GetDotClocksReply> XF86VidMode::GetDotClocks(
    const XF86VidMode::GetDotClocksRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& screen = request.screen;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 13;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // screen
  buf.Write(&screen);

  // pad0
  Pad(&buf, 2);

  Align(&buf, 4);

  return connection_->SendRequest<XF86VidMode::GetDotClocksReply>(
      &buf, "XF86VidMode::GetDotClocks", false);
}

Future<XF86VidMode::GetDotClocksReply> XF86VidMode::GetDotClocks(
    const uint16_t& screen) {
  return XF86VidMode::GetDotClocks(XF86VidMode::GetDotClocksRequest{screen});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<XF86VidMode::GetDotClocksReply> detail::ReadReply<
    XF86VidMode::GetDotClocksReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<XF86VidMode::GetDotClocksReply>();

  auto& sequence = (*reply).sequence;
  auto& flags = (*reply).flags;
  auto& clocks = (*reply).clocks;
  auto& maxclocks = (*reply).maxclocks;
  auto& clock = (*reply).clock;
  size_t clock_len = clock.size();

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

  // flags
  uint32_t tmp8;
  Read(&tmp8, &buf);
  flags = static_cast<XF86VidMode::ClockFlag>(tmp8);

  // clocks
  Read(&clocks, &buf);

  // maxclocks
  Read(&maxclocks, &buf);

  // pad1
  Pad(&buf, 12);

  // clock
  clock.resize(((1) - (BitAnd(flags, 1))) * (clocks));
  for (auto& clock_elem : clock) {
    // clock_elem
    Read(&clock_elem, &buf);
  }

  Align(&buf, 4);
  DUMP_WILL_BE_CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> XF86VidMode::SetClientVersion(
    const XF86VidMode::SetClientVersionRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& major = request.major;
  auto& minor = request.minor;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 14;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // major
  buf.Write(&major);

  // minor
  buf.Write(&minor);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "XF86VidMode::SetClientVersion",
                                        false);
}

Future<void> XF86VidMode::SetClientVersion(const uint16_t& major,
                                           const uint16_t& minor) {
  return XF86VidMode::SetClientVersion(
      XF86VidMode::SetClientVersionRequest{major, minor});
}

Future<void> XF86VidMode::SetGamma(
    const XF86VidMode::SetGammaRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& screen = request.screen;
  auto& red = request.red;
  auto& green = request.green;
  auto& blue = request.blue;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 15;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // screen
  buf.Write(&screen);

  // pad0
  Pad(&buf, 2);

  // red
  buf.Write(&red);

  // green
  buf.Write(&green);

  // blue
  buf.Write(&blue);

  // pad1
  Pad(&buf, 12);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "XF86VidMode::SetGamma", false);
}

Future<void> XF86VidMode::SetGamma(const uint16_t& screen,
                                   const uint32_t& red,
                                   const uint32_t& green,
                                   const uint32_t& blue) {
  return XF86VidMode::SetGamma(
      XF86VidMode::SetGammaRequest{screen, red, green, blue});
}

Future<XF86VidMode::GetGammaReply> XF86VidMode::GetGamma(
    const XF86VidMode::GetGammaRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& screen = request.screen;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 16;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // screen
  buf.Write(&screen);

  // pad0
  Pad(&buf, 26);

  Align(&buf, 4);

  return connection_->SendRequest<XF86VidMode::GetGammaReply>(
      &buf, "XF86VidMode::GetGamma", false);
}

Future<XF86VidMode::GetGammaReply> XF86VidMode::GetGamma(
    const uint16_t& screen) {
  return XF86VidMode::GetGamma(XF86VidMode::GetGammaRequest{screen});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<XF86VidMode::GetGammaReply> detail::ReadReply<
    XF86VidMode::GetGammaReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<XF86VidMode::GetGammaReply>();

  auto& sequence = (*reply).sequence;
  auto& red = (*reply).red;
  auto& green = (*reply).green;
  auto& blue = (*reply).blue;

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

  // red
  Read(&red, &buf);

  // green
  Read(&green, &buf);

  // blue
  Read(&blue, &buf);

  // pad1
  Pad(&buf, 12);

  Align(&buf, 4);
  DUMP_WILL_BE_CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<XF86VidMode::GetGammaRampReply> XF86VidMode::GetGammaRamp(
    const XF86VidMode::GetGammaRampRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& screen = request.screen;
  auto& size = request.size;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 17;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // screen
  buf.Write(&screen);

  // size
  buf.Write(&size);

  Align(&buf, 4);

  return connection_->SendRequest<XF86VidMode::GetGammaRampReply>(
      &buf, "XF86VidMode::GetGammaRamp", false);
}

Future<XF86VidMode::GetGammaRampReply> XF86VidMode::GetGammaRamp(
    const uint16_t& screen,
    const uint16_t& size) {
  return XF86VidMode::GetGammaRamp(
      XF86VidMode::GetGammaRampRequest{screen, size});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<XF86VidMode::GetGammaRampReply> detail::ReadReply<
    XF86VidMode::GetGammaRampReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<XF86VidMode::GetGammaRampReply>();

  auto& sequence = (*reply).sequence;
  auto& size = (*reply).size;
  auto& red = (*reply).red;
  size_t red_len = red.size();
  auto& green = (*reply).green;
  size_t green_len = green.size();
  auto& blue = (*reply).blue;
  size_t blue_len = blue.size();

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

  // size
  Read(&size, &buf);

  // pad1
  Pad(&buf, 22);

  // red
  red.resize(BitAnd((size) + (1), BitNot(1)));
  for (auto& red_elem : red) {
    // red_elem
    Read(&red_elem, &buf);
  }

  // green
  green.resize(BitAnd((size) + (1), BitNot(1)));
  for (auto& green_elem : green) {
    // green_elem
    Read(&green_elem, &buf);
  }

  // blue
  blue.resize(BitAnd((size) + (1), BitNot(1)));
  for (auto& blue_elem : blue) {
    // blue_elem
    Read(&blue_elem, &buf);
  }

  Align(&buf, 4);
  DUMP_WILL_BE_CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> XF86VidMode::SetGammaRamp(
    const XF86VidMode::SetGammaRampRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& screen = request.screen;
  auto& size = request.size;
  auto& red = request.red;
  size_t red_len = red.size();
  auto& green = request.green;
  size_t green_len = green.size();
  auto& blue = request.blue;
  size_t blue_len = blue.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 18;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // screen
  buf.Write(&screen);

  // size
  buf.Write(&size);

  // red
  DUMP_WILL_BE_CHECK_EQ(static_cast<size_t>(BitAnd((size) + (1), BitNot(1))),
                        red.size());
  for (auto& red_elem : red) {
    // red_elem
    buf.Write(&red_elem);
  }

  // green
  DUMP_WILL_BE_CHECK_EQ(static_cast<size_t>(BitAnd((size) + (1), BitNot(1))),
                        green.size());
  for (auto& green_elem : green) {
    // green_elem
    buf.Write(&green_elem);
  }

  // blue
  DUMP_WILL_BE_CHECK_EQ(static_cast<size_t>(BitAnd((size) + (1), BitNot(1))),
                        blue.size());
  for (auto& blue_elem : blue) {
    // blue_elem
    buf.Write(&blue_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "XF86VidMode::SetGammaRamp",
                                        false);
}

Future<void> XF86VidMode::SetGammaRamp(const uint16_t& screen,
                                       const uint16_t& size,
                                       const std::vector<uint16_t>& red,
                                       const std::vector<uint16_t>& green,
                                       const std::vector<uint16_t>& blue) {
  return XF86VidMode::SetGammaRamp(
      XF86VidMode::SetGammaRampRequest{screen, size, red, green, blue});
}

Future<XF86VidMode::GetGammaRampSizeReply> XF86VidMode::GetGammaRampSize(
    const XF86VidMode::GetGammaRampSizeRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& screen = request.screen;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 19;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // screen
  buf.Write(&screen);

  // pad0
  Pad(&buf, 2);

  Align(&buf, 4);

  return connection_->SendRequest<XF86VidMode::GetGammaRampSizeReply>(
      &buf, "XF86VidMode::GetGammaRampSize", false);
}

Future<XF86VidMode::GetGammaRampSizeReply> XF86VidMode::GetGammaRampSize(
    const uint16_t& screen) {
  return XF86VidMode::GetGammaRampSize(
      XF86VidMode::GetGammaRampSizeRequest{screen});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<XF86VidMode::GetGammaRampSizeReply> detail::ReadReply<
    XF86VidMode::GetGammaRampSizeReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<XF86VidMode::GetGammaRampSizeReply>();

  auto& sequence = (*reply).sequence;
  auto& size = (*reply).size;

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

  // size
  Read(&size, &buf);

  // pad1
  Pad(&buf, 22);

  Align(&buf, 4);
  DUMP_WILL_BE_CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<XF86VidMode::GetPermissionsReply> XF86VidMode::GetPermissions(
    const XF86VidMode::GetPermissionsRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& screen = request.screen;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 20;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // screen
  buf.Write(&screen);

  // pad0
  Pad(&buf, 2);

  Align(&buf, 4);

  return connection_->SendRequest<XF86VidMode::GetPermissionsReply>(
      &buf, "XF86VidMode::GetPermissions", false);
}

Future<XF86VidMode::GetPermissionsReply> XF86VidMode::GetPermissions(
    const uint16_t& screen) {
  return XF86VidMode::GetPermissions(
      XF86VidMode::GetPermissionsRequest{screen});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<XF86VidMode::GetPermissionsReply> detail::ReadReply<
    XF86VidMode::GetPermissionsReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<XF86VidMode::GetPermissionsReply>();

  auto& sequence = (*reply).sequence;
  auto& permissions = (*reply).permissions;

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

  // permissions
  uint32_t tmp9;
  Read(&tmp9, &buf);
  permissions = static_cast<XF86VidMode::Permission>(tmp9);

  // pad1
  Pad(&buf, 20);

  Align(&buf, 4);
  DUMP_WILL_BE_CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

}  // namespace x11
