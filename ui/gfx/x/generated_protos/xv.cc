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

#include "xv.h"

#include <unistd.h>
#include <xcb/xcb.h>
#include <xcb/xcbext.h>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "ui/gfx/x/xproto_internal.h"

namespace x11 {

Xv::Xv(Connection* connection, const x11::QueryExtensionReply& info)
    : connection_(connection), info_(info) {}

std::string Xv::BadPortError::ToString() const {
  std::stringstream ss_;
  ss_ << "Xv::BadPortError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<Xv::BadPortError>(Xv::BadPortError* error_, ReadBuffer* buffer) {
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
std::string Xv::BadEncodingError::ToString() const {
  std::stringstream ss_;
  ss_ << "Xv::BadEncodingError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<Xv::BadEncodingError>(Xv::BadEncodingError* error_,
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
std::string Xv::BadControlError::ToString() const {
  std::stringstream ss_;
  ss_ << "Xv::BadControlError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<Xv::BadControlError>(Xv::BadControlError* error_,
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
void ReadEvent<Xv::VideoNotifyEvent>(Xv::VideoNotifyEvent* event_,
                                     ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& reason = (*event_).reason;
  auto& sequence = (*event_).sequence;
  auto& time = (*event_).time;
  auto& drawable = (*event_).drawable;
  auto& port = (*event_).port;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // reason
  uint8_t tmp0;
  Read(&tmp0, &buf);
  reason = static_cast<Xv::VideoNotifyReason>(tmp0);

  // sequence
  Read(&sequence, &buf);

  // time
  Read(&time, &buf);

  // drawable
  Read(&drawable, &buf);

  // port
  Read(&port, &buf);

  DUMP_WILL_BE_CHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<Xv::PortNotifyEvent>(Xv::PortNotifyEvent* event_,
                                    ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*event_).sequence;
  auto& time = (*event_).time;
  auto& port = (*event_).port;
  auto& attribute = (*event_).attribute;
  auto& value = (*event_).value;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // time
  Read(&time, &buf);

  // port
  Read(&port, &buf);

  // attribute
  Read(&attribute, &buf);

  // value
  Read(&value, &buf);

  DUMP_WILL_BE_CHECK_LE(buf.offset, 32ul);
}

Future<Xv::QueryExtensionReply> Xv::QueryExtension(
    const Xv::QueryExtensionRequest& request) {
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

  return connection_->SendRequest<Xv::QueryExtensionReply>(
      &buf, "Xv::QueryExtension", false);
}

Future<Xv::QueryExtensionReply> Xv::QueryExtension() {
  return Xv::QueryExtension(Xv::QueryExtensionRequest{});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Xv::QueryExtensionReply> detail::ReadReply<
    Xv::QueryExtensionReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Xv::QueryExtensionReply>();

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

Future<Xv::QueryAdaptorsReply> Xv::QueryAdaptors(
    const Xv::QueryAdaptorsRequest& request) {
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

  return connection_->SendRequest<Xv::QueryAdaptorsReply>(
      &buf, "Xv::QueryAdaptors", false);
}

Future<Xv::QueryAdaptorsReply> Xv::QueryAdaptors(const Window& window) {
  return Xv::QueryAdaptors(Xv::QueryAdaptorsRequest{window});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Xv::QueryAdaptorsReply> detail::ReadReply<
    Xv::QueryAdaptorsReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Xv::QueryAdaptorsReply>();

  auto& sequence = (*reply).sequence;
  uint16_t num_adaptors{};
  auto& info = (*reply).info;
  size_t info_len = info.size();

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

  // num_adaptors
  Read(&num_adaptors, &buf);

  // pad1
  Pad(&buf, 22);

  // info
  info.resize(num_adaptors);
  for (auto& info_elem : info) {
    // info_elem
    {
      auto& base_id = info_elem.base_id;
      uint16_t name_size{};
      auto& num_ports = info_elem.num_ports;
      uint16_t num_formats{};
      auto& type = info_elem.type;
      auto& name = info_elem.name;
      size_t name_len = name.size();
      auto& formats = info_elem.formats;
      size_t formats_len = formats.size();

      // base_id
      Read(&base_id, &buf);

      // name_size
      Read(&name_size, &buf);

      // num_ports
      Read(&num_ports, &buf);

      // num_formats
      Read(&num_formats, &buf);

      // type
      uint8_t tmp1;
      Read(&tmp1, &buf);
      type = static_cast<Xv::Type>(tmp1);

      // pad0
      Pad(&buf, 1);

      // name
      name.resize(name_size);
      for (auto& name_elem : name) {
        // name_elem
        Read(&name_elem, &buf);
      }

      // pad1
      Align(&buf, 4);

      // formats
      formats.resize(num_formats);
      for (auto& formats_elem : formats) {
        // formats_elem
        {
          auto& visual = formats_elem.visual;
          auto& depth = formats_elem.depth;

          // visual
          Read(&visual, &buf);

          // depth
          Read(&depth, &buf);

          // pad0
          Pad(&buf, 3);
        }
      }
    }
  }

  Align(&buf, 4);
  DUMP_WILL_BE_CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Xv::QueryEncodingsReply> Xv::QueryEncodings(
    const Xv::QueryEncodingsRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& port = request.port;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 2;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // port
  buf.Write(&port);

  Align(&buf, 4);

  return connection_->SendRequest<Xv::QueryEncodingsReply>(
      &buf, "Xv::QueryEncodings", false);
}

Future<Xv::QueryEncodingsReply> Xv::QueryEncodings(const Port& port) {
  return Xv::QueryEncodings(Xv::QueryEncodingsRequest{port});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Xv::QueryEncodingsReply> detail::ReadReply<
    Xv::QueryEncodingsReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Xv::QueryEncodingsReply>();

  auto& sequence = (*reply).sequence;
  uint16_t num_encodings{};
  auto& info = (*reply).info;
  size_t info_len = info.size();

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

  // num_encodings
  Read(&num_encodings, &buf);

  // pad1
  Pad(&buf, 22);

  // info
  info.resize(num_encodings);
  for (auto& info_elem : info) {
    // info_elem
    {
      auto& encoding = info_elem.encoding;
      uint16_t name_size{};
      auto& width = info_elem.width;
      auto& height = info_elem.height;
      auto& rate = info_elem.rate;
      auto& name = info_elem.name;
      size_t name_len = name.size();

      // encoding
      Read(&encoding, &buf);

      // name_size
      Read(&name_size, &buf);

      // width
      Read(&width, &buf);

      // height
      Read(&height, &buf);

      // pad0
      Pad(&buf, 2);

      // rate
      {
        auto& numerator = rate.numerator;
        auto& denominator = rate.denominator;

        // numerator
        Read(&numerator, &buf);

        // denominator
        Read(&denominator, &buf);
      }

      // name
      name.resize(name_size);
      for (auto& name_elem : name) {
        // name_elem
        Read(&name_elem, &buf);
      }

      // pad1
      Align(&buf, 4);
    }
  }

  Align(&buf, 4);
  DUMP_WILL_BE_CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Xv::GrabPortReply> Xv::GrabPort(const Xv::GrabPortRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& port = request.port;
  auto& time = request.time;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 3;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // port
  buf.Write(&port);

  // time
  buf.Write(&time);

  Align(&buf, 4);

  return connection_->SendRequest<Xv::GrabPortReply>(&buf, "Xv::GrabPort",
                                                     false);
}

Future<Xv::GrabPortReply> Xv::GrabPort(const Port& port, const Time& time) {
  return Xv::GrabPort(Xv::GrabPortRequest{port, time});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Xv::GrabPortReply> detail::ReadReply<Xv::GrabPortReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Xv::GrabPortReply>();

  auto& result = (*reply).result;
  auto& sequence = (*reply).sequence;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // result
  uint8_t tmp2;
  Read(&tmp2, &buf);
  result = static_cast<Xv::GrabPortStatus>(tmp2);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  Align(&buf, 4);
  DUMP_WILL_BE_CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Xv::UngrabPort(const Xv::UngrabPortRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& port = request.port;
  auto& time = request.time;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 4;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // port
  buf.Write(&port);

  // time
  buf.Write(&time);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Xv::UngrabPort", false);
}

Future<void> Xv::UngrabPort(const Port& port, const Time& time) {
  return Xv::UngrabPort(Xv::UngrabPortRequest{port, time});
}

Future<void> Xv::PutVideo(const Xv::PutVideoRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& port = request.port;
  auto& drawable = request.drawable;
  auto& gc = request.gc;
  auto& vid_x = request.vid_x;
  auto& vid_y = request.vid_y;
  auto& vid_w = request.vid_w;
  auto& vid_h = request.vid_h;
  auto& drw_x = request.drw_x;
  auto& drw_y = request.drw_y;
  auto& drw_w = request.drw_w;
  auto& drw_h = request.drw_h;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 5;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // port
  buf.Write(&port);

  // drawable
  buf.Write(&drawable);

  // gc
  buf.Write(&gc);

  // vid_x
  buf.Write(&vid_x);

  // vid_y
  buf.Write(&vid_y);

  // vid_w
  buf.Write(&vid_w);

  // vid_h
  buf.Write(&vid_h);

  // drw_x
  buf.Write(&drw_x);

  // drw_y
  buf.Write(&drw_y);

  // drw_w
  buf.Write(&drw_w);

  // drw_h
  buf.Write(&drw_h);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Xv::PutVideo", false);
}

Future<void> Xv::PutVideo(const Port& port,
                          const Drawable& drawable,
                          const GraphicsContext& gc,
                          const int16_t& vid_x,
                          const int16_t& vid_y,
                          const uint16_t& vid_w,
                          const uint16_t& vid_h,
                          const int16_t& drw_x,
                          const int16_t& drw_y,
                          const uint16_t& drw_w,
                          const uint16_t& drw_h) {
  return Xv::PutVideo(Xv::PutVideoRequest{port, drawable, gc, vid_x, vid_y,
                                          vid_w, vid_h, drw_x, drw_y, drw_w,
                                          drw_h});
}

Future<void> Xv::PutStill(const Xv::PutStillRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& port = request.port;
  auto& drawable = request.drawable;
  auto& gc = request.gc;
  auto& vid_x = request.vid_x;
  auto& vid_y = request.vid_y;
  auto& vid_w = request.vid_w;
  auto& vid_h = request.vid_h;
  auto& drw_x = request.drw_x;
  auto& drw_y = request.drw_y;
  auto& drw_w = request.drw_w;
  auto& drw_h = request.drw_h;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 6;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // port
  buf.Write(&port);

  // drawable
  buf.Write(&drawable);

  // gc
  buf.Write(&gc);

  // vid_x
  buf.Write(&vid_x);

  // vid_y
  buf.Write(&vid_y);

  // vid_w
  buf.Write(&vid_w);

  // vid_h
  buf.Write(&vid_h);

  // drw_x
  buf.Write(&drw_x);

  // drw_y
  buf.Write(&drw_y);

  // drw_w
  buf.Write(&drw_w);

  // drw_h
  buf.Write(&drw_h);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Xv::PutStill", false);
}

Future<void> Xv::PutStill(const Port& port,
                          const Drawable& drawable,
                          const GraphicsContext& gc,
                          const int16_t& vid_x,
                          const int16_t& vid_y,
                          const uint16_t& vid_w,
                          const uint16_t& vid_h,
                          const int16_t& drw_x,
                          const int16_t& drw_y,
                          const uint16_t& drw_w,
                          const uint16_t& drw_h) {
  return Xv::PutStill(Xv::PutStillRequest{port, drawable, gc, vid_x, vid_y,
                                          vid_w, vid_h, drw_x, drw_y, drw_w,
                                          drw_h});
}

Future<void> Xv::GetVideo(const Xv::GetVideoRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& port = request.port;
  auto& drawable = request.drawable;
  auto& gc = request.gc;
  auto& vid_x = request.vid_x;
  auto& vid_y = request.vid_y;
  auto& vid_w = request.vid_w;
  auto& vid_h = request.vid_h;
  auto& drw_x = request.drw_x;
  auto& drw_y = request.drw_y;
  auto& drw_w = request.drw_w;
  auto& drw_h = request.drw_h;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 7;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // port
  buf.Write(&port);

  // drawable
  buf.Write(&drawable);

  // gc
  buf.Write(&gc);

  // vid_x
  buf.Write(&vid_x);

  // vid_y
  buf.Write(&vid_y);

  // vid_w
  buf.Write(&vid_w);

  // vid_h
  buf.Write(&vid_h);

  // drw_x
  buf.Write(&drw_x);

  // drw_y
  buf.Write(&drw_y);

  // drw_w
  buf.Write(&drw_w);

  // drw_h
  buf.Write(&drw_h);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Xv::GetVideo", false);
}

Future<void> Xv::GetVideo(const Port& port,
                          const Drawable& drawable,
                          const GraphicsContext& gc,
                          const int16_t& vid_x,
                          const int16_t& vid_y,
                          const uint16_t& vid_w,
                          const uint16_t& vid_h,
                          const int16_t& drw_x,
                          const int16_t& drw_y,
                          const uint16_t& drw_w,
                          const uint16_t& drw_h) {
  return Xv::GetVideo(Xv::GetVideoRequest{port, drawable, gc, vid_x, vid_y,
                                          vid_w, vid_h, drw_x, drw_y, drw_w,
                                          drw_h});
}

Future<void> Xv::GetStill(const Xv::GetStillRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& port = request.port;
  auto& drawable = request.drawable;
  auto& gc = request.gc;
  auto& vid_x = request.vid_x;
  auto& vid_y = request.vid_y;
  auto& vid_w = request.vid_w;
  auto& vid_h = request.vid_h;
  auto& drw_x = request.drw_x;
  auto& drw_y = request.drw_y;
  auto& drw_w = request.drw_w;
  auto& drw_h = request.drw_h;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 8;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // port
  buf.Write(&port);

  // drawable
  buf.Write(&drawable);

  // gc
  buf.Write(&gc);

  // vid_x
  buf.Write(&vid_x);

  // vid_y
  buf.Write(&vid_y);

  // vid_w
  buf.Write(&vid_w);

  // vid_h
  buf.Write(&vid_h);

  // drw_x
  buf.Write(&drw_x);

  // drw_y
  buf.Write(&drw_y);

  // drw_w
  buf.Write(&drw_w);

  // drw_h
  buf.Write(&drw_h);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Xv::GetStill", false);
}

Future<void> Xv::GetStill(const Port& port,
                          const Drawable& drawable,
                          const GraphicsContext& gc,
                          const int16_t& vid_x,
                          const int16_t& vid_y,
                          const uint16_t& vid_w,
                          const uint16_t& vid_h,
                          const int16_t& drw_x,
                          const int16_t& drw_y,
                          const uint16_t& drw_w,
                          const uint16_t& drw_h) {
  return Xv::GetStill(Xv::GetStillRequest{port, drawable, gc, vid_x, vid_y,
                                          vid_w, vid_h, drw_x, drw_y, drw_w,
                                          drw_h});
}

Future<void> Xv::StopVideo(const Xv::StopVideoRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& port = request.port;
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

  // port
  buf.Write(&port);

  // drawable
  buf.Write(&drawable);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Xv::StopVideo", false);
}

Future<void> Xv::StopVideo(const Port& port, const Drawable& drawable) {
  return Xv::StopVideo(Xv::StopVideoRequest{port, drawable});
}

Future<void> Xv::SelectVideoNotify(
    const Xv::SelectVideoNotifyRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& drawable = request.drawable;
  auto& onoff = request.onoff;

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

  // onoff
  buf.Write(&onoff);

  // pad0
  Pad(&buf, 3);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Xv::SelectVideoNotify", false);
}

Future<void> Xv::SelectVideoNotify(const Drawable& drawable,
                                   const uint8_t& onoff) {
  return Xv::SelectVideoNotify(Xv::SelectVideoNotifyRequest{drawable, onoff});
}

Future<void> Xv::SelectPortNotify(const Xv::SelectPortNotifyRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& port = request.port;
  auto& onoff = request.onoff;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 11;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // port
  buf.Write(&port);

  // onoff
  buf.Write(&onoff);

  // pad0
  Pad(&buf, 3);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Xv::SelectPortNotify", false);
}

Future<void> Xv::SelectPortNotify(const Port& port, const uint8_t& onoff) {
  return Xv::SelectPortNotify(Xv::SelectPortNotifyRequest{port, onoff});
}

Future<Xv::QueryBestSizeReply> Xv::QueryBestSize(
    const Xv::QueryBestSizeRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& port = request.port;
  auto& vid_w = request.vid_w;
  auto& vid_h = request.vid_h;
  auto& drw_w = request.drw_w;
  auto& drw_h = request.drw_h;
  auto& motion = request.motion;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 12;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // port
  buf.Write(&port);

  // vid_w
  buf.Write(&vid_w);

  // vid_h
  buf.Write(&vid_h);

  // drw_w
  buf.Write(&drw_w);

  // drw_h
  buf.Write(&drw_h);

  // motion
  buf.Write(&motion);

  // pad0
  Pad(&buf, 3);

  Align(&buf, 4);

  return connection_->SendRequest<Xv::QueryBestSizeReply>(
      &buf, "Xv::QueryBestSize", false);
}

Future<Xv::QueryBestSizeReply> Xv::QueryBestSize(const Port& port,
                                                 const uint16_t& vid_w,
                                                 const uint16_t& vid_h,
                                                 const uint16_t& drw_w,
                                                 const uint16_t& drw_h,
                                                 const uint8_t& motion) {
  return Xv::QueryBestSize(
      Xv::QueryBestSizeRequest{port, vid_w, vid_h, drw_w, drw_h, motion});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Xv::QueryBestSizeReply> detail::ReadReply<
    Xv::QueryBestSizeReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Xv::QueryBestSizeReply>();

  auto& sequence = (*reply).sequence;
  auto& actual_width = (*reply).actual_width;
  auto& actual_height = (*reply).actual_height;

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

  // actual_width
  Read(&actual_width, &buf);

  // actual_height
  Read(&actual_height, &buf);

  Align(&buf, 4);
  DUMP_WILL_BE_CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Xv::SetPortAttribute(const Xv::SetPortAttributeRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& port = request.port;
  auto& attribute = request.attribute;
  auto& value = request.value;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 13;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // port
  buf.Write(&port);

  // attribute
  buf.Write(&attribute);

  // value
  buf.Write(&value);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Xv::SetPortAttribute", false);
}

Future<void> Xv::SetPortAttribute(const Port& port,
                                  const Atom& attribute,
                                  const int32_t& value) {
  return Xv::SetPortAttribute(
      Xv::SetPortAttributeRequest{port, attribute, value});
}

Future<Xv::GetPortAttributeReply> Xv::GetPortAttribute(
    const Xv::GetPortAttributeRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& port = request.port;
  auto& attribute = request.attribute;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 14;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // port
  buf.Write(&port);

  // attribute
  buf.Write(&attribute);

  Align(&buf, 4);

  return connection_->SendRequest<Xv::GetPortAttributeReply>(
      &buf, "Xv::GetPortAttribute", false);
}

Future<Xv::GetPortAttributeReply> Xv::GetPortAttribute(const Port& port,
                                                       const Atom& attribute) {
  return Xv::GetPortAttribute(Xv::GetPortAttributeRequest{port, attribute});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Xv::GetPortAttributeReply> detail::ReadReply<
    Xv::GetPortAttributeReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Xv::GetPortAttributeReply>();

  auto& sequence = (*reply).sequence;
  auto& value = (*reply).value;

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

  // value
  Read(&value, &buf);

  Align(&buf, 4);
  DUMP_WILL_BE_CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Xv::QueryPortAttributesReply> Xv::QueryPortAttributes(
    const Xv::QueryPortAttributesRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& port = request.port;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 15;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // port
  buf.Write(&port);

  Align(&buf, 4);

  return connection_->SendRequest<Xv::QueryPortAttributesReply>(
      &buf, "Xv::QueryPortAttributes", false);
}

Future<Xv::QueryPortAttributesReply> Xv::QueryPortAttributes(const Port& port) {
  return Xv::QueryPortAttributes(Xv::QueryPortAttributesRequest{port});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Xv::QueryPortAttributesReply> detail::ReadReply<
    Xv::QueryPortAttributesReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Xv::QueryPortAttributesReply>();

  auto& sequence = (*reply).sequence;
  uint32_t num_attributes{};
  auto& text_size = (*reply).text_size;
  auto& attributes = (*reply).attributes;
  size_t attributes_len = attributes.size();

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

  // num_attributes
  Read(&num_attributes, &buf);

  // text_size
  Read(&text_size, &buf);

  // pad1
  Pad(&buf, 16);

  // attributes
  attributes.resize(num_attributes);
  for (auto& attributes_elem : attributes) {
    // attributes_elem
    {
      auto& flags = attributes_elem.flags;
      auto& min = attributes_elem.min;
      auto& max = attributes_elem.max;
      uint32_t size{};
      auto& name = attributes_elem.name;
      size_t name_len = name.size();

      // flags
      uint32_t tmp3;
      Read(&tmp3, &buf);
      flags = static_cast<Xv::AttributeFlag>(tmp3);

      // min
      Read(&min, &buf);

      // max
      Read(&max, &buf);

      // size
      Read(&size, &buf);

      // name
      name.resize(size);
      for (auto& name_elem : name) {
        // name_elem
        Read(&name_elem, &buf);
      }

      // pad0
      Align(&buf, 4);
    }
  }

  Align(&buf, 4);
  DUMP_WILL_BE_CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Xv::ListImageFormatsReply> Xv::ListImageFormats(
    const Xv::ListImageFormatsRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& port = request.port;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 16;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // port
  buf.Write(&port);

  Align(&buf, 4);

  return connection_->SendRequest<Xv::ListImageFormatsReply>(
      &buf, "Xv::ListImageFormats", false);
}

Future<Xv::ListImageFormatsReply> Xv::ListImageFormats(const Port& port) {
  return Xv::ListImageFormats(Xv::ListImageFormatsRequest{port});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Xv::ListImageFormatsReply> detail::ReadReply<
    Xv::ListImageFormatsReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Xv::ListImageFormatsReply>();

  auto& sequence = (*reply).sequence;
  uint32_t num_formats{};
  auto& format = (*reply).format;
  size_t format_len = format.size();

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

  // num_formats
  Read(&num_formats, &buf);

  // pad1
  Pad(&buf, 20);

  // format
  format.resize(num_formats);
  for (auto& format_elem : format) {
    // format_elem
    {
      auto& id = format_elem.id;
      auto& type = format_elem.type;
      auto& byte_order = format_elem.byte_order;
      auto& guid = format_elem.guid;
      size_t guid_len = guid.size();
      auto& bpp = format_elem.bpp;
      auto& num_planes = format_elem.num_planes;
      auto& depth = format_elem.depth;
      auto& red_mask = format_elem.red_mask;
      auto& green_mask = format_elem.green_mask;
      auto& blue_mask = format_elem.blue_mask;
      auto& format = format_elem.format;
      auto& y_sample_bits = format_elem.y_sample_bits;
      auto& u_sample_bits = format_elem.u_sample_bits;
      auto& v_sample_bits = format_elem.v_sample_bits;
      auto& vhorz_y_period = format_elem.vhorz_y_period;
      auto& vhorz_u_period = format_elem.vhorz_u_period;
      auto& vhorz_v_period = format_elem.vhorz_v_period;
      auto& vvert_y_period = format_elem.vvert_y_period;
      auto& vvert_u_period = format_elem.vvert_u_period;
      auto& vvert_v_period = format_elem.vvert_v_period;
      auto& vcomp_order = format_elem.vcomp_order;
      size_t vcomp_order_len = vcomp_order.size();
      auto& vscanline_order = format_elem.vscanline_order;

      // id
      Read(&id, &buf);

      // type
      uint8_t tmp4;
      Read(&tmp4, &buf);
      type = static_cast<Xv::ImageFormatInfoType>(tmp4);

      // byte_order
      uint8_t tmp5;
      Read(&tmp5, &buf);
      byte_order = static_cast<ImageOrder>(tmp5);

      // pad0
      Pad(&buf, 2);

      // guid
      for (auto& guid_elem : guid) {
        // guid_elem
        Read(&guid_elem, &buf);
      }

      // bpp
      Read(&bpp, &buf);

      // num_planes
      Read(&num_planes, &buf);

      // pad1
      Pad(&buf, 2);

      // depth
      Read(&depth, &buf);

      // pad2
      Pad(&buf, 3);

      // red_mask
      Read(&red_mask, &buf);

      // green_mask
      Read(&green_mask, &buf);

      // blue_mask
      Read(&blue_mask, &buf);

      // format
      uint8_t tmp6;
      Read(&tmp6, &buf);
      format = static_cast<Xv::ImageFormatInfoFormat>(tmp6);

      // pad3
      Pad(&buf, 3);

      // y_sample_bits
      Read(&y_sample_bits, &buf);

      // u_sample_bits
      Read(&u_sample_bits, &buf);

      // v_sample_bits
      Read(&v_sample_bits, &buf);

      // vhorz_y_period
      Read(&vhorz_y_period, &buf);

      // vhorz_u_period
      Read(&vhorz_u_period, &buf);

      // vhorz_v_period
      Read(&vhorz_v_period, &buf);

      // vvert_y_period
      Read(&vvert_y_period, &buf);

      // vvert_u_period
      Read(&vvert_u_period, &buf);

      // vvert_v_period
      Read(&vvert_v_period, &buf);

      // vcomp_order
      for (auto& vcomp_order_elem : vcomp_order) {
        // vcomp_order_elem
        Read(&vcomp_order_elem, &buf);
      }

      // vscanline_order
      uint8_t tmp7;
      Read(&tmp7, &buf);
      vscanline_order = static_cast<Xv::ScanlineOrder>(tmp7);

      // pad4
      Pad(&buf, 11);
    }
  }

  Align(&buf, 4);
  DUMP_WILL_BE_CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Xv::QueryImageAttributesReply> Xv::QueryImageAttributes(
    const Xv::QueryImageAttributesRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& port = request.port;
  auto& id = request.id;
  auto& width = request.width;
  auto& height = request.height;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 17;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // port
  buf.Write(&port);

  // id
  buf.Write(&id);

  // width
  buf.Write(&width);

  // height
  buf.Write(&height);

  Align(&buf, 4);

  return connection_->SendRequest<Xv::QueryImageAttributesReply>(
      &buf, "Xv::QueryImageAttributes", false);
}

Future<Xv::QueryImageAttributesReply> Xv::QueryImageAttributes(
    const Port& port,
    const uint32_t& id,
    const uint16_t& width,
    const uint16_t& height) {
  return Xv::QueryImageAttributes(
      Xv::QueryImageAttributesRequest{port, id, width, height});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Xv::QueryImageAttributesReply> detail::ReadReply<
    Xv::QueryImageAttributesReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Xv::QueryImageAttributesReply>();

  auto& sequence = (*reply).sequence;
  uint32_t num_planes{};
  auto& data_size = (*reply).data_size;
  auto& width = (*reply).width;
  auto& height = (*reply).height;
  auto& pitches = (*reply).pitches;
  size_t pitches_len = pitches.size();
  auto& offsets = (*reply).offsets;
  size_t offsets_len = offsets.size();

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

  // num_planes
  Read(&num_planes, &buf);

  // data_size
  Read(&data_size, &buf);

  // width
  Read(&width, &buf);

  // height
  Read(&height, &buf);

  // pad1
  Pad(&buf, 12);

  // pitches
  pitches.resize(num_planes);
  for (auto& pitches_elem : pitches) {
    // pitches_elem
    Read(&pitches_elem, &buf);
  }

  // offsets
  offsets.resize(num_planes);
  for (auto& offsets_elem : offsets) {
    // offsets_elem
    Read(&offsets_elem, &buf);
  }

  Align(&buf, 4);
  DUMP_WILL_BE_CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Xv::PutImage(const Xv::PutImageRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& port = request.port;
  auto& drawable = request.drawable;
  auto& gc = request.gc;
  auto& id = request.id;
  auto& src_x = request.src_x;
  auto& src_y = request.src_y;
  auto& src_w = request.src_w;
  auto& src_h = request.src_h;
  auto& drw_x = request.drw_x;
  auto& drw_y = request.drw_y;
  auto& drw_w = request.drw_w;
  auto& drw_h = request.drw_h;
  auto& width = request.width;
  auto& height = request.height;
  auto& data = request.data;
  size_t data_len = data.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 18;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // port
  buf.Write(&port);

  // drawable
  buf.Write(&drawable);

  // gc
  buf.Write(&gc);

  // id
  buf.Write(&id);

  // src_x
  buf.Write(&src_x);

  // src_y
  buf.Write(&src_y);

  // src_w
  buf.Write(&src_w);

  // src_h
  buf.Write(&src_h);

  // drw_x
  buf.Write(&drw_x);

  // drw_y
  buf.Write(&drw_y);

  // drw_w
  buf.Write(&drw_w);

  // drw_h
  buf.Write(&drw_h);

  // width
  buf.Write(&width);

  // height
  buf.Write(&height);

  // data
  DUMP_WILL_BE_CHECK_EQ(static_cast<size_t>(data_len), data.size());
  for (auto& data_elem : data) {
    // data_elem
    buf.Write(&data_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Xv::PutImage", false);
}

Future<void> Xv::PutImage(const Port& port,
                          const Drawable& drawable,
                          const GraphicsContext& gc,
                          const uint32_t& id,
                          const int16_t& src_x,
                          const int16_t& src_y,
                          const uint16_t& src_w,
                          const uint16_t& src_h,
                          const int16_t& drw_x,
                          const int16_t& drw_y,
                          const uint16_t& drw_w,
                          const uint16_t& drw_h,
                          const uint16_t& width,
                          const uint16_t& height,
                          const std::vector<uint8_t>& data) {
  return Xv::PutImage(Xv::PutImageRequest{port, drawable, gc, id, src_x, src_y,
                                          src_w, src_h, drw_x, drw_y, drw_w,
                                          drw_h, width, height, data});
}

Future<void> Xv::ShmPutImage(const Xv::ShmPutImageRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& port = request.port;
  auto& drawable = request.drawable;
  auto& gc = request.gc;
  auto& shmseg = request.shmseg;
  auto& id = request.id;
  auto& offset = request.offset;
  auto& src_x = request.src_x;
  auto& src_y = request.src_y;
  auto& src_w = request.src_w;
  auto& src_h = request.src_h;
  auto& drw_x = request.drw_x;
  auto& drw_y = request.drw_y;
  auto& drw_w = request.drw_w;
  auto& drw_h = request.drw_h;
  auto& width = request.width;
  auto& height = request.height;
  auto& send_event = request.send_event;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 19;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // port
  buf.Write(&port);

  // drawable
  buf.Write(&drawable);

  // gc
  buf.Write(&gc);

  // shmseg
  buf.Write(&shmseg);

  // id
  buf.Write(&id);

  // offset
  buf.Write(&offset);

  // src_x
  buf.Write(&src_x);

  // src_y
  buf.Write(&src_y);

  // src_w
  buf.Write(&src_w);

  // src_h
  buf.Write(&src_h);

  // drw_x
  buf.Write(&drw_x);

  // drw_y
  buf.Write(&drw_y);

  // drw_w
  buf.Write(&drw_w);

  // drw_h
  buf.Write(&drw_h);

  // width
  buf.Write(&width);

  // height
  buf.Write(&height);

  // send_event
  buf.Write(&send_event);

  // pad0
  Pad(&buf, 3);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Xv::ShmPutImage", false);
}

Future<void> Xv::ShmPutImage(const Port& port,
                             const Drawable& drawable,
                             const GraphicsContext& gc,
                             const Shm::Seg& shmseg,
                             const uint32_t& id,
                             const uint32_t& offset,
                             const int16_t& src_x,
                             const int16_t& src_y,
                             const uint16_t& src_w,
                             const uint16_t& src_h,
                             const int16_t& drw_x,
                             const int16_t& drw_y,
                             const uint16_t& drw_w,
                             const uint16_t& drw_h,
                             const uint16_t& width,
                             const uint16_t& height,
                             const uint8_t& send_event) {
  return Xv::ShmPutImage(Xv::ShmPutImageRequest{
      port, drawable, gc, shmseg, id, offset, src_x, src_y, src_w, src_h, drw_x,
      drw_y, drw_w, drw_h, width, height, send_event});
}

}  // namespace x11
