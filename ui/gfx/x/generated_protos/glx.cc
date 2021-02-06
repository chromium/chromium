// Copyright 2021 The Chromium Authors. All rights reserved.
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

#include "glx.h"

#include <xcb/xcb.h>
#include <xcb/xcbext.h>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "ui/gfx/x/xproto_internal.h"

namespace x11 {

Glx::Glx(Connection* connection, const x11::QueryExtensionReply& info)
    : connection_(connection), info_(info) {}

std::string Glx::GenericError::ToString() const {
  std::stringstream ss_;
  ss_ << "Glx::GenericError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<Glx::GenericError>(Glx::GenericError* error_,
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

  // pad0
  Pad(&buf, 21);

  DCHECK_LE(buf.offset, 32ul);
}
std::string Glx::BadContextError::ToString() const {
  std::stringstream ss_;
  ss_ << "Glx::BadContextError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<Glx::BadContextError>(Glx::BadContextError* error_,
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

  // pad0
  Pad(&buf, 21);

  DCHECK_LE(buf.offset, 32ul);
}
std::string Glx::BadContextStateError::ToString() const {
  std::stringstream ss_;
  ss_ << "Glx::BadContextStateError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<Glx::BadContextStateError>(Glx::BadContextStateError* error_,
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

  // pad0
  Pad(&buf, 21);

  DCHECK_LE(buf.offset, 32ul);
}
std::string Glx::BadDrawableError::ToString() const {
  std::stringstream ss_;
  ss_ << "Glx::BadDrawableError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<Glx::BadDrawableError>(Glx::BadDrawableError* error_,
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

  // pad0
  Pad(&buf, 21);

  DCHECK_LE(buf.offset, 32ul);
}
std::string Glx::BadPixmapError::ToString() const {
  std::stringstream ss_;
  ss_ << "Glx::BadPixmapError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<Glx::BadPixmapError>(Glx::BadPixmapError* error_,
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

  // pad0
  Pad(&buf, 21);

  DCHECK_LE(buf.offset, 32ul);
}
std::string Glx::BadContextTagError::ToString() const {
  std::stringstream ss_;
  ss_ << "Glx::BadContextTagError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<Glx::BadContextTagError>(Glx::BadContextTagError* error_,
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

  // pad0
  Pad(&buf, 21);

  DCHECK_LE(buf.offset, 32ul);
}
std::string Glx::BadCurrentWindowError::ToString() const {
  std::stringstream ss_;
  ss_ << "Glx::BadCurrentWindowError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<Glx::BadCurrentWindowError>(Glx::BadCurrentWindowError* error_,
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

  // pad0
  Pad(&buf, 21);

  DCHECK_LE(buf.offset, 32ul);
}
std::string Glx::BadRenderRequestError::ToString() const {
  std::stringstream ss_;
  ss_ << "Glx::BadRenderRequestError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<Glx::BadRenderRequestError>(Glx::BadRenderRequestError* error_,
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

  // pad0
  Pad(&buf, 21);

  DCHECK_LE(buf.offset, 32ul);
}
std::string Glx::BadLargeRequestError::ToString() const {
  std::stringstream ss_;
  ss_ << "Glx::BadLargeRequestError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<Glx::BadLargeRequestError>(Glx::BadLargeRequestError* error_,
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

  // pad0
  Pad(&buf, 21);

  DCHECK_LE(buf.offset, 32ul);
}
std::string Glx::UnsupportedPrivateRequestError::ToString() const {
  std::stringstream ss_;
  ss_ << "Glx::UnsupportedPrivateRequestError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<Glx::UnsupportedPrivateRequestError>(
    Glx::UnsupportedPrivateRequestError* error_,
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

  // pad0
  Pad(&buf, 21);

  DCHECK_LE(buf.offset, 32ul);
}
std::string Glx::BadFBConfigError::ToString() const {
  std::stringstream ss_;
  ss_ << "Glx::BadFBConfigError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<Glx::BadFBConfigError>(Glx::BadFBConfigError* error_,
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

  // pad0
  Pad(&buf, 21);

  DCHECK_LE(buf.offset, 32ul);
}
std::string Glx::BadPbufferError::ToString() const {
  std::stringstream ss_;
  ss_ << "Glx::BadPbufferError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<Glx::BadPbufferError>(Glx::BadPbufferError* error_,
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

  // pad0
  Pad(&buf, 21);

  DCHECK_LE(buf.offset, 32ul);
}
std::string Glx::BadCurrentDrawableError::ToString() const {
  std::stringstream ss_;
  ss_ << "Glx::BadCurrentDrawableError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<Glx::BadCurrentDrawableError>(
    Glx::BadCurrentDrawableError* error_,
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

  // pad0
  Pad(&buf, 21);

  DCHECK_LE(buf.offset, 32ul);
}
std::string Glx::BadWindowError::ToString() const {
  std::stringstream ss_;
  ss_ << "Glx::BadWindowError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<Glx::BadWindowError>(Glx::BadWindowError* error_,
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

  // pad0
  Pad(&buf, 21);

  DCHECK_LE(buf.offset, 32ul);
}
std::string Glx::GLXBadProfileARBError::ToString() const {
  std::stringstream ss_;
  ss_ << "Glx::GLXBadProfileARBError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<Glx::GLXBadProfileARBError>(Glx::GLXBadProfileARBError* error_,
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

  // pad0
  Pad(&buf, 21);

  DCHECK_LE(buf.offset, 32ul);
}
template <>
COMPONENT_EXPORT(X11)
void ReadEvent<Glx::PbufferClobberEvent>(Glx::PbufferClobberEvent* event_,
                                         ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*event_).sequence;
  auto& event_type = (*event_).event_type;
  auto& draw_type = (*event_).draw_type;
  auto& drawable = (*event_).drawable;
  auto& b_mask = (*event_).b_mask;
  auto& aux_buffer = (*event_).aux_buffer;
  auto& x = (*event_).x;
  auto& y = (*event_).y;
  auto& width = (*event_).width;
  auto& height = (*event_).height;
  auto& count = (*event_).count;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // event_type
  Read(&event_type, &buf);

  // draw_type
  Read(&draw_type, &buf);

  // drawable
  Read(&drawable, &buf);

  // b_mask
  Read(&b_mask, &buf);

  // aux_buffer
  Read(&aux_buffer, &buf);

  // x
  Read(&x, &buf);

  // y
  Read(&y, &buf);

  // width
  Read(&width, &buf);

  // height
  Read(&height, &buf);

  // count
  Read(&count, &buf);

  // pad1
  Pad(&buf, 4);

  DCHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<Glx::BufferSwapCompleteEvent>(
    Glx::BufferSwapCompleteEvent* event_,
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
  Read(&event_type, &buf);

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

Future<void> Glx::Render(const Glx::RenderRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& data = request.data;
  size_t data_len = data.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 1;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // data
  DCHECK_EQ(static_cast<size_t>(data_len), data.size());
  for (auto& data_elem : data) {
    // data_elem
    buf.Write(&data_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Glx::Render", false);
}

Future<void> Glx::RenderLarge(const Glx::RenderLargeRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& request_num = request.request_num;
  auto& request_total = request.request_total;
  uint32_t data_len{};
  auto& data = request.data;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 2;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // request_num
  buf.Write(&request_num);

  // request_total
  buf.Write(&request_total);

  // data_len
  data_len = data.size();
  buf.Write(&data_len);

  // data
  DCHECK_EQ(static_cast<size_t>(data_len), data.size());
  for (auto& data_elem : data) {
    // data_elem
    buf.Write(&data_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Glx::RenderLarge", false);
}

Future<void> Glx::CreateContext(const Glx::CreateContextRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context = request.context;
  auto& visual = request.visual;
  auto& screen = request.screen;
  auto& share_list = request.share_list;
  auto& is_direct = request.is_direct;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 3;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context
  buf.Write(&context);

  // visual
  buf.Write(&visual);

  // screen
  buf.Write(&screen);

  // share_list
  buf.Write(&share_list);

  // is_direct
  buf.Write(&is_direct);

  // pad0
  Pad(&buf, 3);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Glx::CreateContext", false);
}

Future<void> Glx::DestroyContext(const Glx::DestroyContextRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context = request.context;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 4;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context
  buf.Write(&context);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Glx::DestroyContext", false);
}

Future<Glx::MakeCurrentReply> Glx::MakeCurrent(
    const Glx::MakeCurrentRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& drawable = request.drawable;
  auto& context = request.context;
  auto& old_context_tag = request.old_context_tag;

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

  // context
  buf.Write(&context);

  // old_context_tag
  buf.Write(&old_context_tag);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::MakeCurrentReply>(
      &buf, "Glx::MakeCurrent", false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::MakeCurrentReply> detail::ReadReply<Glx::MakeCurrentReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::MakeCurrentReply>();

  auto& sequence = (*reply).sequence;
  auto& context_tag = (*reply).context_tag;

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

  // context_tag
  Read(&context_tag, &buf);

  // pad1
  Pad(&buf, 20);

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Glx::IsDirectReply> Glx::IsDirect(const Glx::IsDirectRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context = request.context;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 6;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context
  buf.Write(&context);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::IsDirectReply>(&buf, "Glx::IsDirect",
                                                      false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::IsDirectReply> detail::ReadReply<Glx::IsDirectReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::IsDirectReply>();

  auto& sequence = (*reply).sequence;
  auto& is_direct = (*reply).is_direct;

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

  // is_direct
  Read(&is_direct, &buf);

  // pad1
  Pad(&buf, 23);

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Glx::QueryVersionReply> Glx::QueryVersion(
    const Glx::QueryVersionRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& major_version = request.major_version;
  auto& minor_version = request.minor_version;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 7;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // major_version
  buf.Write(&major_version);

  // minor_version
  buf.Write(&minor_version);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::QueryVersionReply>(
      &buf, "Glx::QueryVersion", false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::QueryVersionReply> detail::ReadReply<
    Glx::QueryVersionReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::QueryVersionReply>();

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
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Glx::WaitGL(const Glx::WaitGLRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 8;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Glx::WaitGL", false);
}

Future<void> Glx::WaitX(const Glx::WaitXRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 9;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Glx::WaitX", false);
}

Future<void> Glx::CopyContext(const Glx::CopyContextRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& src = request.src;
  auto& dest = request.dest;
  auto& mask = request.mask;
  auto& src_context_tag = request.src_context_tag;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 10;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // src
  buf.Write(&src);

  // dest
  buf.Write(&dest);

  // mask
  buf.Write(&mask);

  // src_context_tag
  buf.Write(&src_context_tag);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Glx::CopyContext", false);
}

Future<void> Glx::SwapBuffers(const Glx::SwapBuffersRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& drawable = request.drawable;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 11;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // drawable
  buf.Write(&drawable);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Glx::SwapBuffers", false);
}

Future<void> Glx::UseXFont(const Glx::UseXFontRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& font = request.font;
  auto& first = request.first;
  auto& count = request.count;
  auto& list_base = request.list_base;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 12;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // font
  buf.Write(&font);

  // first
  buf.Write(&first);

  // count
  buf.Write(&count);

  // list_base
  buf.Write(&list_base);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Glx::UseXFont", false);
}

Future<void> Glx::CreateGLXPixmap(const Glx::CreateGLXPixmapRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& screen = request.screen;
  auto& visual = request.visual;
  auto& pixmap = request.pixmap;
  auto& glx_pixmap = request.glx_pixmap;

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

  // visual
  buf.Write(&visual);

  // pixmap
  buf.Write(&pixmap);

  // glx_pixmap
  buf.Write(&glx_pixmap);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Glx::CreateGLXPixmap", false);
}

Future<Glx::GetVisualConfigsReply> Glx::GetVisualConfigs(
    const Glx::GetVisualConfigsRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& screen = request.screen;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 14;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // screen
  buf.Write(&screen);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::GetVisualConfigsReply>(
      &buf, "Glx::GetVisualConfigs", false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::GetVisualConfigsReply> detail::ReadReply<
    Glx::GetVisualConfigsReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::GetVisualConfigsReply>();

  auto& sequence = (*reply).sequence;
  auto& num_visuals = (*reply).num_visuals;
  auto& num_properties = (*reply).num_properties;
  auto& property_list = (*reply).property_list;
  size_t property_list_len = property_list.size();

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

  // num_visuals
  Read(&num_visuals, &buf);

  // num_properties
  Read(&num_properties, &buf);

  // pad1
  Pad(&buf, 16);

  // property_list
  property_list.resize(length);
  for (auto& property_list_elem : property_list) {
    // property_list_elem
    Read(&property_list_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Glx::DestroyGLXPixmap(
    const Glx::DestroyGLXPixmapRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& glx_pixmap = request.glx_pixmap;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 15;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // glx_pixmap
  buf.Write(&glx_pixmap);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Glx::DestroyGLXPixmap", false);
}

Future<void> Glx::VendorPrivate(const Glx::VendorPrivateRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& vendor_code = request.vendor_code;
  auto& context_tag = request.context_tag;
  auto& data = request.data;
  size_t data_len = data.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 16;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // vendor_code
  buf.Write(&vendor_code);

  // context_tag
  buf.Write(&context_tag);

  // data
  DCHECK_EQ(static_cast<size_t>(data_len), data.size());
  for (auto& data_elem : data) {
    // data_elem
    buf.Write(&data_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Glx::VendorPrivate", false);
}

Future<Glx::VendorPrivateWithReplyReply> Glx::VendorPrivateWithReply(
    const Glx::VendorPrivateWithReplyRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& vendor_code = request.vendor_code;
  auto& context_tag = request.context_tag;
  auto& data = request.data;
  size_t data_len = data.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 17;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // vendor_code
  buf.Write(&vendor_code);

  // context_tag
  buf.Write(&context_tag);

  // data
  DCHECK_EQ(static_cast<size_t>(data_len), data.size());
  for (auto& data_elem : data) {
    // data_elem
    buf.Write(&data_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<Glx::VendorPrivateWithReplyReply>(
      &buf, "Glx::VendorPrivateWithReply", false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::VendorPrivateWithReplyReply> detail::ReadReply<
    Glx::VendorPrivateWithReplyReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::VendorPrivateWithReplyReply>();

  auto& sequence = (*reply).sequence;
  auto& retval = (*reply).retval;
  auto& data1 = (*reply).data1;
  size_t data1_len = data1.size();
  auto& data2 = (*reply).data2;
  size_t data2_len = data2.size();

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

  // retval
  Read(&retval, &buf);

  // data1
  for (auto& data1_elem : data1) {
    // data1_elem
    Read(&data1_elem, &buf);
  }

  // data2
  data2.resize((length) * (4));
  for (auto& data2_elem : data2) {
    // data2_elem
    Read(&data2_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Glx::QueryExtensionsStringReply> Glx::QueryExtensionsString(
    const Glx::QueryExtensionsStringRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& screen = request.screen;

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

  Align(&buf, 4);

  return connection_->SendRequest<Glx::QueryExtensionsStringReply>(
      &buf, "Glx::QueryExtensionsString", false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::QueryExtensionsStringReply> detail::ReadReply<
    Glx::QueryExtensionsStringReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::QueryExtensionsStringReply>();

  auto& sequence = (*reply).sequence;
  auto& n = (*reply).n;

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
  Pad(&buf, 4);

  // n
  Read(&n, &buf);

  // pad2
  Pad(&buf, 16);

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Glx::QueryServerStringReply> Glx::QueryServerString(
    const Glx::QueryServerStringRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& screen = request.screen;
  auto& name = request.name;

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

  // name
  buf.Write(&name);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::QueryServerStringReply>(
      &buf, "Glx::QueryServerString", false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::QueryServerStringReply> detail::ReadReply<
    Glx::QueryServerStringReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::QueryServerStringReply>();

  auto& sequence = (*reply).sequence;
  uint32_t str_len{};
  auto& string = (*reply).string;
  size_t string_len = string.size();

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
  Pad(&buf, 4);

  // str_len
  Read(&str_len, &buf);

  // pad2
  Pad(&buf, 16);

  // string
  string.resize(str_len);
  for (auto& string_elem : string) {
    // string_elem
    Read(&string_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Glx::ClientInfo(const Glx::ClientInfoRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& major_version = request.major_version;
  auto& minor_version = request.minor_version;
  uint32_t str_len{};
  auto& string = request.string;
  size_t string_len = string.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 20;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // major_version
  buf.Write(&major_version);

  // minor_version
  buf.Write(&minor_version);

  // str_len
  str_len = string.size();
  buf.Write(&str_len);

  // string
  DCHECK_EQ(static_cast<size_t>(str_len), string.size());
  for (auto& string_elem : string) {
    // string_elem
    buf.Write(&string_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Glx::ClientInfo", false);
}

Future<Glx::GetFBConfigsReply> Glx::GetFBConfigs(
    const Glx::GetFBConfigsRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& screen = request.screen;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 21;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // screen
  buf.Write(&screen);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::GetFBConfigsReply>(
      &buf, "Glx::GetFBConfigs", false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::GetFBConfigsReply> detail::ReadReply<
    Glx::GetFBConfigsReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::GetFBConfigsReply>();

  auto& sequence = (*reply).sequence;
  auto& num_FB_configs = (*reply).num_FB_configs;
  auto& num_properties = (*reply).num_properties;
  auto& property_list = (*reply).property_list;
  size_t property_list_len = property_list.size();

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

  // num_FB_configs
  Read(&num_FB_configs, &buf);

  // num_properties
  Read(&num_properties, &buf);

  // pad1
  Pad(&buf, 16);

  // property_list
  property_list.resize(length);
  for (auto& property_list_elem : property_list) {
    // property_list_elem
    Read(&property_list_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Glx::CreatePixmap(const Glx::CreatePixmapRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& screen = request.screen;
  auto& fbconfig = request.fbconfig;
  auto& pixmap = request.pixmap;
  auto& glx_pixmap = request.glx_pixmap;
  auto& num_attribs = request.num_attribs;
  auto& attribs = request.attribs;
  size_t attribs_len = attribs.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 22;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // screen
  buf.Write(&screen);

  // fbconfig
  buf.Write(&fbconfig);

  // pixmap
  buf.Write(&pixmap);

  // glx_pixmap
  buf.Write(&glx_pixmap);

  // num_attribs
  buf.Write(&num_attribs);

  // attribs
  DCHECK_EQ(static_cast<size_t>((num_attribs) * (2)), attribs.size());
  for (auto& attribs_elem : attribs) {
    // attribs_elem
    buf.Write(&attribs_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Glx::CreatePixmap", false);
}

Future<void> Glx::DestroyPixmap(const Glx::DestroyPixmapRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& glx_pixmap = request.glx_pixmap;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 23;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // glx_pixmap
  buf.Write(&glx_pixmap);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Glx::DestroyPixmap", false);
}

Future<void> Glx::CreateNewContext(
    const Glx::CreateNewContextRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context = request.context;
  auto& fbconfig = request.fbconfig;
  auto& screen = request.screen;
  auto& render_type = request.render_type;
  auto& share_list = request.share_list;
  auto& is_direct = request.is_direct;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 24;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context
  buf.Write(&context);

  // fbconfig
  buf.Write(&fbconfig);

  // screen
  buf.Write(&screen);

  // render_type
  buf.Write(&render_type);

  // share_list
  buf.Write(&share_list);

  // is_direct
  buf.Write(&is_direct);

  // pad0
  Pad(&buf, 3);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Glx::CreateNewContext", false);
}

Future<Glx::QueryContextReply> Glx::QueryContext(
    const Glx::QueryContextRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context = request.context;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 25;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context
  buf.Write(&context);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::QueryContextReply>(
      &buf, "Glx::QueryContext", false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::QueryContextReply> detail::ReadReply<
    Glx::QueryContextReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::QueryContextReply>();

  auto& sequence = (*reply).sequence;
  auto& num_attribs = (*reply).num_attribs;
  auto& attribs = (*reply).attribs;
  size_t attribs_len = attribs.size();

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

  // num_attribs
  Read(&num_attribs, &buf);

  // pad1
  Pad(&buf, 20);

  // attribs
  attribs.resize((num_attribs) * (2));
  for (auto& attribs_elem : attribs) {
    // attribs_elem
    Read(&attribs_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Glx::MakeContextCurrentReply> Glx::MakeContextCurrent(
    const Glx::MakeContextCurrentRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& old_context_tag = request.old_context_tag;
  auto& drawable = request.drawable;
  auto& read_drawable = request.read_drawable;
  auto& context = request.context;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 26;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // old_context_tag
  buf.Write(&old_context_tag);

  // drawable
  buf.Write(&drawable);

  // read_drawable
  buf.Write(&read_drawable);

  // context
  buf.Write(&context);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::MakeContextCurrentReply>(
      &buf, "Glx::MakeContextCurrent", false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::MakeContextCurrentReply> detail::ReadReply<
    Glx::MakeContextCurrentReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::MakeContextCurrentReply>();

  auto& sequence = (*reply).sequence;
  auto& context_tag = (*reply).context_tag;

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

  // context_tag
  Read(&context_tag, &buf);

  // pad1
  Pad(&buf, 20);

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Glx::CreatePbuffer(const Glx::CreatePbufferRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& screen = request.screen;
  auto& fbconfig = request.fbconfig;
  auto& pbuffer = request.pbuffer;
  auto& num_attribs = request.num_attribs;
  auto& attribs = request.attribs;
  size_t attribs_len = attribs.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 27;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // screen
  buf.Write(&screen);

  // fbconfig
  buf.Write(&fbconfig);

  // pbuffer
  buf.Write(&pbuffer);

  // num_attribs
  buf.Write(&num_attribs);

  // attribs
  DCHECK_EQ(static_cast<size_t>((num_attribs) * (2)), attribs.size());
  for (auto& attribs_elem : attribs) {
    // attribs_elem
    buf.Write(&attribs_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Glx::CreatePbuffer", false);
}

Future<void> Glx::DestroyPbuffer(const Glx::DestroyPbufferRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& pbuffer = request.pbuffer;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 28;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // pbuffer
  buf.Write(&pbuffer);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Glx::DestroyPbuffer", false);
}

Future<Glx::GetDrawableAttributesReply> Glx::GetDrawableAttributes(
    const Glx::GetDrawableAttributesRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& drawable = request.drawable;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 29;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // drawable
  buf.Write(&drawable);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::GetDrawableAttributesReply>(
      &buf, "Glx::GetDrawableAttributes", false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::GetDrawableAttributesReply> detail::ReadReply<
    Glx::GetDrawableAttributesReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::GetDrawableAttributesReply>();

  auto& sequence = (*reply).sequence;
  auto& num_attribs = (*reply).num_attribs;
  auto& attribs = (*reply).attribs;
  size_t attribs_len = attribs.size();

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

  // num_attribs
  Read(&num_attribs, &buf);

  // pad1
  Pad(&buf, 20);

  // attribs
  attribs.resize((num_attribs) * (2));
  for (auto& attribs_elem : attribs) {
    // attribs_elem
    Read(&attribs_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Glx::ChangeDrawableAttributes(
    const Glx::ChangeDrawableAttributesRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& drawable = request.drawable;
  auto& num_attribs = request.num_attribs;
  auto& attribs = request.attribs;
  size_t attribs_len = attribs.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 30;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // drawable
  buf.Write(&drawable);

  // num_attribs
  buf.Write(&num_attribs);

  // attribs
  DCHECK_EQ(static_cast<size_t>((num_attribs) * (2)), attribs.size());
  for (auto& attribs_elem : attribs) {
    // attribs_elem
    buf.Write(&attribs_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Glx::ChangeDrawableAttributes",
                                        false);
}

Future<void> Glx::CreateWindow(const Glx::CreateWindowRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& screen = request.screen;
  auto& fbconfig = request.fbconfig;
  auto& window = request.window;
  auto& glx_window = request.glx_window;
  auto& num_attribs = request.num_attribs;
  auto& attribs = request.attribs;
  size_t attribs_len = attribs.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 31;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // screen
  buf.Write(&screen);

  // fbconfig
  buf.Write(&fbconfig);

  // window
  buf.Write(&window);

  // glx_window
  buf.Write(&glx_window);

  // num_attribs
  buf.Write(&num_attribs);

  // attribs
  DCHECK_EQ(static_cast<size_t>((num_attribs) * (2)), attribs.size());
  for (auto& attribs_elem : attribs) {
    // attribs_elem
    buf.Write(&attribs_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Glx::CreateWindow", false);
}

Future<void> Glx::DeleteWindow(const Glx::DeleteWindowRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& glxwindow = request.glxwindow;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 32;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // glxwindow
  buf.Write(&glxwindow);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Glx::DeleteWindow", false);
}

Future<void> Glx::SetClientInfoARB(
    const Glx::SetClientInfoARBRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& major_version = request.major_version;
  auto& minor_version = request.minor_version;
  auto& num_versions = request.num_versions;
  uint32_t gl_str_len{};
  uint32_t glx_str_len{};
  auto& gl_versions = request.gl_versions;
  size_t gl_versions_len = gl_versions.size();
  auto& gl_extension_string = request.gl_extension_string;
  size_t gl_extension_string_len = gl_extension_string.size();
  auto& glx_extension_string = request.glx_extension_string;
  size_t glx_extension_string_len = glx_extension_string.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 33;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // major_version
  buf.Write(&major_version);

  // minor_version
  buf.Write(&minor_version);

  // num_versions
  buf.Write(&num_versions);

  // gl_str_len
  gl_str_len = gl_extension_string.size();
  buf.Write(&gl_str_len);

  // glx_str_len
  glx_str_len = glx_extension_string.size();
  buf.Write(&glx_str_len);

  // gl_versions
  DCHECK_EQ(static_cast<size_t>((num_versions) * (2)), gl_versions.size());
  for (auto& gl_versions_elem : gl_versions) {
    // gl_versions_elem
    buf.Write(&gl_versions_elem);
  }

  // gl_extension_string
  DCHECK_EQ(static_cast<size_t>(gl_str_len), gl_extension_string.size());
  for (auto& gl_extension_string_elem : gl_extension_string) {
    // gl_extension_string_elem
    buf.Write(&gl_extension_string_elem);
  }

  // glx_extension_string
  DCHECK_EQ(static_cast<size_t>(glx_str_len), glx_extension_string.size());
  for (auto& glx_extension_string_elem : glx_extension_string) {
    // glx_extension_string_elem
    buf.Write(&glx_extension_string_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Glx::SetClientInfoARB", false);
}

Future<void> Glx::CreateContextAttribsARB(
    const Glx::CreateContextAttribsARBRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context = request.context;
  auto& fbconfig = request.fbconfig;
  auto& screen = request.screen;
  auto& share_list = request.share_list;
  auto& is_direct = request.is_direct;
  auto& num_attribs = request.num_attribs;
  auto& attribs = request.attribs;
  size_t attribs_len = attribs.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 34;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context
  buf.Write(&context);

  // fbconfig
  buf.Write(&fbconfig);

  // screen
  buf.Write(&screen);

  // share_list
  buf.Write(&share_list);

  // is_direct
  buf.Write(&is_direct);

  // pad0
  Pad(&buf, 3);

  // num_attribs
  buf.Write(&num_attribs);

  // attribs
  DCHECK_EQ(static_cast<size_t>((num_attribs) * (2)), attribs.size());
  for (auto& attribs_elem : attribs) {
    // attribs_elem
    buf.Write(&attribs_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Glx::CreateContextAttribsARB",
                                        false);
}

Future<void> Glx::SetClientInfo2ARB(
    const Glx::SetClientInfo2ARBRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& major_version = request.major_version;
  auto& minor_version = request.minor_version;
  auto& num_versions = request.num_versions;
  uint32_t gl_str_len{};
  uint32_t glx_str_len{};
  auto& gl_versions = request.gl_versions;
  size_t gl_versions_len = gl_versions.size();
  auto& gl_extension_string = request.gl_extension_string;
  size_t gl_extension_string_len = gl_extension_string.size();
  auto& glx_extension_string = request.glx_extension_string;
  size_t glx_extension_string_len = glx_extension_string.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 35;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // major_version
  buf.Write(&major_version);

  // minor_version
  buf.Write(&minor_version);

  // num_versions
  buf.Write(&num_versions);

  // gl_str_len
  gl_str_len = gl_extension_string.size();
  buf.Write(&gl_str_len);

  // glx_str_len
  glx_str_len = glx_extension_string.size();
  buf.Write(&glx_str_len);

  // gl_versions
  DCHECK_EQ(static_cast<size_t>((num_versions) * (3)), gl_versions.size());
  for (auto& gl_versions_elem : gl_versions) {
    // gl_versions_elem
    buf.Write(&gl_versions_elem);
  }

  // gl_extension_string
  DCHECK_EQ(static_cast<size_t>(gl_str_len), gl_extension_string.size());
  for (auto& gl_extension_string_elem : gl_extension_string) {
    // gl_extension_string_elem
    buf.Write(&gl_extension_string_elem);
  }

  // glx_extension_string
  DCHECK_EQ(static_cast<size_t>(glx_str_len), glx_extension_string.size());
  for (auto& glx_extension_string_elem : glx_extension_string) {
    // glx_extension_string_elem
    buf.Write(&glx_extension_string_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Glx::SetClientInfo2ARB", false);
}

Future<void> Glx::NewList(const Glx::NewListRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& list = request.list;
  auto& mode = request.mode;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 101;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // list
  buf.Write(&list);

  // mode
  buf.Write(&mode);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Glx::NewList", false);
}

Future<void> Glx::EndList(const Glx::EndListRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 102;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Glx::EndList", false);
}

Future<void> Glx::DeleteLists(const Glx::DeleteListsRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& list = request.list;
  auto& range = request.range;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 103;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // list
  buf.Write(&list);

  // range
  buf.Write(&range);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Glx::DeleteLists", false);
}

Future<Glx::GenListsReply> Glx::GenLists(const Glx::GenListsRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& range = request.range;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 104;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // range
  buf.Write(&range);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::GenListsReply>(&buf, "Glx::GenLists",
                                                      false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::GenListsReply> detail::ReadReply<Glx::GenListsReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::GenListsReply>();

  auto& sequence = (*reply).sequence;
  auto& ret_val = (*reply).ret_val;

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

  // ret_val
  Read(&ret_val, &buf);

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Glx::FeedbackBuffer(const Glx::FeedbackBufferRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& size = request.size;
  auto& type = request.type;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 105;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // size
  buf.Write(&size);

  // type
  buf.Write(&type);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Glx::FeedbackBuffer", false);
}

Future<void> Glx::SelectBuffer(const Glx::SelectBufferRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& size = request.size;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 106;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // size
  buf.Write(&size);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Glx::SelectBuffer", false);
}

Future<Glx::RenderModeReply> Glx::RenderMode(
    const Glx::RenderModeRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& mode = request.mode;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 107;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // mode
  buf.Write(&mode);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::RenderModeReply>(&buf, "Glx::RenderMode",
                                                        false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::RenderModeReply> detail::ReadReply<Glx::RenderModeReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::RenderModeReply>();

  auto& sequence = (*reply).sequence;
  auto& ret_val = (*reply).ret_val;
  uint32_t n{};
  auto& new_mode = (*reply).new_mode;
  auto& data = (*reply).data;
  size_t data_len = data.size();

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

  // ret_val
  Read(&ret_val, &buf);

  // n
  Read(&n, &buf);

  // new_mode
  Read(&new_mode, &buf);

  // pad1
  Pad(&buf, 12);

  // data
  data.resize(n);
  for (auto& data_elem : data) {
    // data_elem
    Read(&data_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Glx::FinishReply> Glx::Finish(const Glx::FinishRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 108;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::FinishReply>(&buf, "Glx::Finish", false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::FinishReply> detail::ReadReply<Glx::FinishReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::FinishReply>();

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

Future<void> Glx::PixelStoref(const Glx::PixelStorefRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& pname = request.pname;
  auto& datum = request.datum;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 109;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // pname
  buf.Write(&pname);

  // datum
  buf.Write(&datum);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Glx::PixelStoref", false);
}

Future<void> Glx::PixelStorei(const Glx::PixelStoreiRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& pname = request.pname;
  auto& datum = request.datum;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 110;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // pname
  buf.Write(&pname);

  // datum
  buf.Write(&datum);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Glx::PixelStorei", false);
}

Future<Glx::ReadPixelsReply> Glx::ReadPixels(
    const Glx::ReadPixelsRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& x = request.x;
  auto& y = request.y;
  auto& width = request.width;
  auto& height = request.height;
  auto& format = request.format;
  auto& type = request.type;
  auto& swap_bytes = request.swap_bytes;
  auto& lsb_first = request.lsb_first;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 111;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // x
  buf.Write(&x);

  // y
  buf.Write(&y);

  // width
  buf.Write(&width);

  // height
  buf.Write(&height);

  // format
  buf.Write(&format);

  // type
  buf.Write(&type);

  // swap_bytes
  buf.Write(&swap_bytes);

  // lsb_first
  buf.Write(&lsb_first);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::ReadPixelsReply>(&buf, "Glx::ReadPixels",
                                                        false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::ReadPixelsReply> detail::ReadReply<Glx::ReadPixelsReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::ReadPixelsReply>();

  auto& sequence = (*reply).sequence;
  auto& data = (*reply).data;
  size_t data_len = data.size();

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

  // data
  data.resize((length) * (4));
  for (auto& data_elem : data) {
    // data_elem
    Read(&data_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Glx::GetBooleanvReply> Glx::GetBooleanv(
    const Glx::GetBooleanvRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& pname = request.pname;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 112;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // pname
  buf.Write(&pname);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::GetBooleanvReply>(
      &buf, "Glx::GetBooleanv", false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::GetBooleanvReply> detail::ReadReply<Glx::GetBooleanvReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::GetBooleanvReply>();

  auto& sequence = (*reply).sequence;
  uint32_t n{};
  auto& datum = (*reply).datum;
  auto& data = (*reply).data;
  size_t data_len = data.size();

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
  Pad(&buf, 4);

  // n
  Read(&n, &buf);

  // datum
  Read(&datum, &buf);

  // pad2
  Pad(&buf, 15);

  // data
  data.resize(n);
  for (auto& data_elem : data) {
    // data_elem
    Read(&data_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Glx::GetClipPlaneReply> Glx::GetClipPlane(
    const Glx::GetClipPlaneRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& plane = request.plane;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 113;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // plane
  buf.Write(&plane);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::GetClipPlaneReply>(
      &buf, "Glx::GetClipPlane", false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::GetClipPlaneReply> detail::ReadReply<
    Glx::GetClipPlaneReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::GetClipPlaneReply>();

  auto& sequence = (*reply).sequence;
  auto& data = (*reply).data;
  size_t data_len = data.size();

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

  // data
  data.resize((length) / (2));
  for (auto& data_elem : data) {
    // data_elem
    Read(&data_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Glx::GetDoublevReply> Glx::GetDoublev(
    const Glx::GetDoublevRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& pname = request.pname;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 114;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // pname
  buf.Write(&pname);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::GetDoublevReply>(&buf, "Glx::GetDoublev",
                                                        false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::GetDoublevReply> detail::ReadReply<Glx::GetDoublevReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::GetDoublevReply>();

  auto& sequence = (*reply).sequence;
  uint32_t n{};
  auto& datum = (*reply).datum;
  auto& data = (*reply).data;
  size_t data_len = data.size();

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
  Pad(&buf, 4);

  // n
  Read(&n, &buf);

  // datum
  Read(&datum, &buf);

  // pad2
  Pad(&buf, 8);

  // data
  data.resize(n);
  for (auto& data_elem : data) {
    // data_elem
    Read(&data_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Glx::GetErrorReply> Glx::GetError(const Glx::GetErrorRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 115;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::GetErrorReply>(&buf, "Glx::GetError",
                                                      false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::GetErrorReply> detail::ReadReply<Glx::GetErrorReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::GetErrorReply>();

  auto& sequence = (*reply).sequence;
  auto& error = (*reply).error;

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

  // error
  Read(&error, &buf);

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Glx::GetFloatvReply> Glx::GetFloatv(
    const Glx::GetFloatvRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& pname = request.pname;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 116;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // pname
  buf.Write(&pname);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::GetFloatvReply>(&buf, "Glx::GetFloatv",
                                                       false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::GetFloatvReply> detail::ReadReply<Glx::GetFloatvReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::GetFloatvReply>();

  auto& sequence = (*reply).sequence;
  uint32_t n{};
  auto& datum = (*reply).datum;
  auto& data = (*reply).data;
  size_t data_len = data.size();

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
  Pad(&buf, 4);

  // n
  Read(&n, &buf);

  // datum
  Read(&datum, &buf);

  // pad2
  Pad(&buf, 12);

  // data
  data.resize(n);
  for (auto& data_elem : data) {
    // data_elem
    Read(&data_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Glx::GetIntegervReply> Glx::GetIntegerv(
    const Glx::GetIntegervRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& pname = request.pname;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 117;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // pname
  buf.Write(&pname);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::GetIntegervReply>(
      &buf, "Glx::GetIntegerv", false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::GetIntegervReply> detail::ReadReply<Glx::GetIntegervReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::GetIntegervReply>();

  auto& sequence = (*reply).sequence;
  uint32_t n{};
  auto& datum = (*reply).datum;
  auto& data = (*reply).data;
  size_t data_len = data.size();

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
  Pad(&buf, 4);

  // n
  Read(&n, &buf);

  // datum
  Read(&datum, &buf);

  // pad2
  Pad(&buf, 12);

  // data
  data.resize(n);
  for (auto& data_elem : data) {
    // data_elem
    Read(&data_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Glx::GetLightfvReply> Glx::GetLightfv(
    const Glx::GetLightfvRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& light = request.light;
  auto& pname = request.pname;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 118;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // light
  buf.Write(&light);

  // pname
  buf.Write(&pname);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::GetLightfvReply>(&buf, "Glx::GetLightfv",
                                                        false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::GetLightfvReply> detail::ReadReply<Glx::GetLightfvReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::GetLightfvReply>();

  auto& sequence = (*reply).sequence;
  uint32_t n{};
  auto& datum = (*reply).datum;
  auto& data = (*reply).data;
  size_t data_len = data.size();

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
  Pad(&buf, 4);

  // n
  Read(&n, &buf);

  // datum
  Read(&datum, &buf);

  // pad2
  Pad(&buf, 12);

  // data
  data.resize(n);
  for (auto& data_elem : data) {
    // data_elem
    Read(&data_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Glx::GetLightivReply> Glx::GetLightiv(
    const Glx::GetLightivRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& light = request.light;
  auto& pname = request.pname;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 119;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // light
  buf.Write(&light);

  // pname
  buf.Write(&pname);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::GetLightivReply>(&buf, "Glx::GetLightiv",
                                                        false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::GetLightivReply> detail::ReadReply<Glx::GetLightivReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::GetLightivReply>();

  auto& sequence = (*reply).sequence;
  uint32_t n{};
  auto& datum = (*reply).datum;
  auto& data = (*reply).data;
  size_t data_len = data.size();

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
  Pad(&buf, 4);

  // n
  Read(&n, &buf);

  // datum
  Read(&datum, &buf);

  // pad2
  Pad(&buf, 12);

  // data
  data.resize(n);
  for (auto& data_elem : data) {
    // data_elem
    Read(&data_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Glx::GetMapdvReply> Glx::GetMapdv(const Glx::GetMapdvRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& target = request.target;
  auto& query = request.query;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 120;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // target
  buf.Write(&target);

  // query
  buf.Write(&query);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::GetMapdvReply>(&buf, "Glx::GetMapdv",
                                                      false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::GetMapdvReply> detail::ReadReply<Glx::GetMapdvReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::GetMapdvReply>();

  auto& sequence = (*reply).sequence;
  uint32_t n{};
  auto& datum = (*reply).datum;
  auto& data = (*reply).data;
  size_t data_len = data.size();

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
  Pad(&buf, 4);

  // n
  Read(&n, &buf);

  // datum
  Read(&datum, &buf);

  // pad2
  Pad(&buf, 8);

  // data
  data.resize(n);
  for (auto& data_elem : data) {
    // data_elem
    Read(&data_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Glx::GetMapfvReply> Glx::GetMapfv(const Glx::GetMapfvRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& target = request.target;
  auto& query = request.query;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 121;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // target
  buf.Write(&target);

  // query
  buf.Write(&query);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::GetMapfvReply>(&buf, "Glx::GetMapfv",
                                                      false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::GetMapfvReply> detail::ReadReply<Glx::GetMapfvReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::GetMapfvReply>();

  auto& sequence = (*reply).sequence;
  uint32_t n{};
  auto& datum = (*reply).datum;
  auto& data = (*reply).data;
  size_t data_len = data.size();

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
  Pad(&buf, 4);

  // n
  Read(&n, &buf);

  // datum
  Read(&datum, &buf);

  // pad2
  Pad(&buf, 12);

  // data
  data.resize(n);
  for (auto& data_elem : data) {
    // data_elem
    Read(&data_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Glx::GetMapivReply> Glx::GetMapiv(const Glx::GetMapivRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& target = request.target;
  auto& query = request.query;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 122;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // target
  buf.Write(&target);

  // query
  buf.Write(&query);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::GetMapivReply>(&buf, "Glx::GetMapiv",
                                                      false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::GetMapivReply> detail::ReadReply<Glx::GetMapivReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::GetMapivReply>();

  auto& sequence = (*reply).sequence;
  uint32_t n{};
  auto& datum = (*reply).datum;
  auto& data = (*reply).data;
  size_t data_len = data.size();

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
  Pad(&buf, 4);

  // n
  Read(&n, &buf);

  // datum
  Read(&datum, &buf);

  // pad2
  Pad(&buf, 12);

  // data
  data.resize(n);
  for (auto& data_elem : data) {
    // data_elem
    Read(&data_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Glx::GetMaterialfvReply> Glx::GetMaterialfv(
    const Glx::GetMaterialfvRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& face = request.face;
  auto& pname = request.pname;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 123;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // face
  buf.Write(&face);

  // pname
  buf.Write(&pname);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::GetMaterialfvReply>(
      &buf, "Glx::GetMaterialfv", false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::GetMaterialfvReply> detail::ReadReply<
    Glx::GetMaterialfvReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::GetMaterialfvReply>();

  auto& sequence = (*reply).sequence;
  uint32_t n{};
  auto& datum = (*reply).datum;
  auto& data = (*reply).data;
  size_t data_len = data.size();

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
  Pad(&buf, 4);

  // n
  Read(&n, &buf);

  // datum
  Read(&datum, &buf);

  // pad2
  Pad(&buf, 12);

  // data
  data.resize(n);
  for (auto& data_elem : data) {
    // data_elem
    Read(&data_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Glx::GetMaterialivReply> Glx::GetMaterialiv(
    const Glx::GetMaterialivRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& face = request.face;
  auto& pname = request.pname;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 124;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // face
  buf.Write(&face);

  // pname
  buf.Write(&pname);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::GetMaterialivReply>(
      &buf, "Glx::GetMaterialiv", false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::GetMaterialivReply> detail::ReadReply<
    Glx::GetMaterialivReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::GetMaterialivReply>();

  auto& sequence = (*reply).sequence;
  uint32_t n{};
  auto& datum = (*reply).datum;
  auto& data = (*reply).data;
  size_t data_len = data.size();

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
  Pad(&buf, 4);

  // n
  Read(&n, &buf);

  // datum
  Read(&datum, &buf);

  // pad2
  Pad(&buf, 12);

  // data
  data.resize(n);
  for (auto& data_elem : data) {
    // data_elem
    Read(&data_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Glx::GetPixelMapfvReply> Glx::GetPixelMapfv(
    const Glx::GetPixelMapfvRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& map = request.map;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 125;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // map
  buf.Write(&map);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::GetPixelMapfvReply>(
      &buf, "Glx::GetPixelMapfv", false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::GetPixelMapfvReply> detail::ReadReply<
    Glx::GetPixelMapfvReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::GetPixelMapfvReply>();

  auto& sequence = (*reply).sequence;
  uint32_t n{};
  auto& datum = (*reply).datum;
  auto& data = (*reply).data;
  size_t data_len = data.size();

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
  Pad(&buf, 4);

  // n
  Read(&n, &buf);

  // datum
  Read(&datum, &buf);

  // pad2
  Pad(&buf, 12);

  // data
  data.resize(n);
  for (auto& data_elem : data) {
    // data_elem
    Read(&data_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Glx::GetPixelMapuivReply> Glx::GetPixelMapuiv(
    const Glx::GetPixelMapuivRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& map = request.map;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 126;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // map
  buf.Write(&map);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::GetPixelMapuivReply>(
      &buf, "Glx::GetPixelMapuiv", false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::GetPixelMapuivReply> detail::ReadReply<
    Glx::GetPixelMapuivReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::GetPixelMapuivReply>();

  auto& sequence = (*reply).sequence;
  uint32_t n{};
  auto& datum = (*reply).datum;
  auto& data = (*reply).data;
  size_t data_len = data.size();

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
  Pad(&buf, 4);

  // n
  Read(&n, &buf);

  // datum
  Read(&datum, &buf);

  // pad2
  Pad(&buf, 12);

  // data
  data.resize(n);
  for (auto& data_elem : data) {
    // data_elem
    Read(&data_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Glx::GetPixelMapusvReply> Glx::GetPixelMapusv(
    const Glx::GetPixelMapusvRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& map = request.map;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 127;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // map
  buf.Write(&map);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::GetPixelMapusvReply>(
      &buf, "Glx::GetPixelMapusv", false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::GetPixelMapusvReply> detail::ReadReply<
    Glx::GetPixelMapusvReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::GetPixelMapusvReply>();

  auto& sequence = (*reply).sequence;
  uint32_t n{};
  auto& datum = (*reply).datum;
  auto& data = (*reply).data;
  size_t data_len = data.size();

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
  Pad(&buf, 4);

  // n
  Read(&n, &buf);

  // datum
  Read(&datum, &buf);

  // pad2
  Pad(&buf, 16);

  // data
  data.resize(n);
  for (auto& data_elem : data) {
    // data_elem
    Read(&data_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Glx::GetPolygonStippleReply> Glx::GetPolygonStipple(
    const Glx::GetPolygonStippleRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& lsb_first = request.lsb_first;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 128;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // lsb_first
  buf.Write(&lsb_first);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::GetPolygonStippleReply>(
      &buf, "Glx::GetPolygonStipple", false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::GetPolygonStippleReply> detail::ReadReply<
    Glx::GetPolygonStippleReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::GetPolygonStippleReply>();

  auto& sequence = (*reply).sequence;
  auto& data = (*reply).data;
  size_t data_len = data.size();

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

  // data
  data.resize((length) * (4));
  for (auto& data_elem : data) {
    // data_elem
    Read(&data_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Glx::GetStringReply> Glx::GetString(
    const Glx::GetStringRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& name = request.name;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 129;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // name
  buf.Write(&name);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::GetStringReply>(&buf, "Glx::GetString",
                                                       false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::GetStringReply> detail::ReadReply<Glx::GetStringReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::GetStringReply>();

  auto& sequence = (*reply).sequence;
  uint32_t n{};
  auto& string = (*reply).string;
  size_t string_len = string.size();

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
  Pad(&buf, 4);

  // n
  Read(&n, &buf);

  // pad2
  Pad(&buf, 16);

  // string
  string.resize(n);
  for (auto& string_elem : string) {
    // string_elem
    Read(&string_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Glx::GetTexEnvfvReply> Glx::GetTexEnvfv(
    const Glx::GetTexEnvfvRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& target = request.target;
  auto& pname = request.pname;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 130;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // target
  buf.Write(&target);

  // pname
  buf.Write(&pname);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::GetTexEnvfvReply>(
      &buf, "Glx::GetTexEnvfv", false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::GetTexEnvfvReply> detail::ReadReply<Glx::GetTexEnvfvReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::GetTexEnvfvReply>();

  auto& sequence = (*reply).sequence;
  uint32_t n{};
  auto& datum = (*reply).datum;
  auto& data = (*reply).data;
  size_t data_len = data.size();

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
  Pad(&buf, 4);

  // n
  Read(&n, &buf);

  // datum
  Read(&datum, &buf);

  // pad2
  Pad(&buf, 12);

  // data
  data.resize(n);
  for (auto& data_elem : data) {
    // data_elem
    Read(&data_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Glx::GetTexEnvivReply> Glx::GetTexEnviv(
    const Glx::GetTexEnvivRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& target = request.target;
  auto& pname = request.pname;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 131;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // target
  buf.Write(&target);

  // pname
  buf.Write(&pname);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::GetTexEnvivReply>(
      &buf, "Glx::GetTexEnviv", false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::GetTexEnvivReply> detail::ReadReply<Glx::GetTexEnvivReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::GetTexEnvivReply>();

  auto& sequence = (*reply).sequence;
  uint32_t n{};
  auto& datum = (*reply).datum;
  auto& data = (*reply).data;
  size_t data_len = data.size();

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
  Pad(&buf, 4);

  // n
  Read(&n, &buf);

  // datum
  Read(&datum, &buf);

  // pad2
  Pad(&buf, 12);

  // data
  data.resize(n);
  for (auto& data_elem : data) {
    // data_elem
    Read(&data_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Glx::GetTexGendvReply> Glx::GetTexGendv(
    const Glx::GetTexGendvRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& coord = request.coord;
  auto& pname = request.pname;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 132;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // coord
  buf.Write(&coord);

  // pname
  buf.Write(&pname);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::GetTexGendvReply>(
      &buf, "Glx::GetTexGendv", false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::GetTexGendvReply> detail::ReadReply<Glx::GetTexGendvReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::GetTexGendvReply>();

  auto& sequence = (*reply).sequence;
  uint32_t n{};
  auto& datum = (*reply).datum;
  auto& data = (*reply).data;
  size_t data_len = data.size();

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
  Pad(&buf, 4);

  // n
  Read(&n, &buf);

  // datum
  Read(&datum, &buf);

  // pad2
  Pad(&buf, 8);

  // data
  data.resize(n);
  for (auto& data_elem : data) {
    // data_elem
    Read(&data_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Glx::GetTexGenfvReply> Glx::GetTexGenfv(
    const Glx::GetTexGenfvRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& coord = request.coord;
  auto& pname = request.pname;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 133;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // coord
  buf.Write(&coord);

  // pname
  buf.Write(&pname);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::GetTexGenfvReply>(
      &buf, "Glx::GetTexGenfv", false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::GetTexGenfvReply> detail::ReadReply<Glx::GetTexGenfvReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::GetTexGenfvReply>();

  auto& sequence = (*reply).sequence;
  uint32_t n{};
  auto& datum = (*reply).datum;
  auto& data = (*reply).data;
  size_t data_len = data.size();

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
  Pad(&buf, 4);

  // n
  Read(&n, &buf);

  // datum
  Read(&datum, &buf);

  // pad2
  Pad(&buf, 12);

  // data
  data.resize(n);
  for (auto& data_elem : data) {
    // data_elem
    Read(&data_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Glx::GetTexGenivReply> Glx::GetTexGeniv(
    const Glx::GetTexGenivRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& coord = request.coord;
  auto& pname = request.pname;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 134;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // coord
  buf.Write(&coord);

  // pname
  buf.Write(&pname);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::GetTexGenivReply>(
      &buf, "Glx::GetTexGeniv", false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::GetTexGenivReply> detail::ReadReply<Glx::GetTexGenivReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::GetTexGenivReply>();

  auto& sequence = (*reply).sequence;
  uint32_t n{};
  auto& datum = (*reply).datum;
  auto& data = (*reply).data;
  size_t data_len = data.size();

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
  Pad(&buf, 4);

  // n
  Read(&n, &buf);

  // datum
  Read(&datum, &buf);

  // pad2
  Pad(&buf, 12);

  // data
  data.resize(n);
  for (auto& data_elem : data) {
    // data_elem
    Read(&data_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Glx::GetTexImageReply> Glx::GetTexImage(
    const Glx::GetTexImageRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& target = request.target;
  auto& level = request.level;
  auto& format = request.format;
  auto& type = request.type;
  auto& swap_bytes = request.swap_bytes;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 135;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // target
  buf.Write(&target);

  // level
  buf.Write(&level);

  // format
  buf.Write(&format);

  // type
  buf.Write(&type);

  // swap_bytes
  buf.Write(&swap_bytes);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::GetTexImageReply>(
      &buf, "Glx::GetTexImage", false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::GetTexImageReply> detail::ReadReply<Glx::GetTexImageReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::GetTexImageReply>();

  auto& sequence = (*reply).sequence;
  auto& width = (*reply).width;
  auto& height = (*reply).height;
  auto& depth = (*reply).depth;
  auto& data = (*reply).data;
  size_t data_len = data.size();

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
  Pad(&buf, 8);

  // width
  Read(&width, &buf);

  // height
  Read(&height, &buf);

  // depth
  Read(&depth, &buf);

  // pad2
  Pad(&buf, 4);

  // data
  data.resize((length) * (4));
  for (auto& data_elem : data) {
    // data_elem
    Read(&data_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Glx::GetTexParameterfvReply> Glx::GetTexParameterfv(
    const Glx::GetTexParameterfvRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& target = request.target;
  auto& pname = request.pname;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 136;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // target
  buf.Write(&target);

  // pname
  buf.Write(&pname);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::GetTexParameterfvReply>(
      &buf, "Glx::GetTexParameterfv", false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::GetTexParameterfvReply> detail::ReadReply<
    Glx::GetTexParameterfvReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::GetTexParameterfvReply>();

  auto& sequence = (*reply).sequence;
  uint32_t n{};
  auto& datum = (*reply).datum;
  auto& data = (*reply).data;
  size_t data_len = data.size();

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
  Pad(&buf, 4);

  // n
  Read(&n, &buf);

  // datum
  Read(&datum, &buf);

  // pad2
  Pad(&buf, 12);

  // data
  data.resize(n);
  for (auto& data_elem : data) {
    // data_elem
    Read(&data_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Glx::GetTexParameterivReply> Glx::GetTexParameteriv(
    const Glx::GetTexParameterivRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& target = request.target;
  auto& pname = request.pname;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 137;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // target
  buf.Write(&target);

  // pname
  buf.Write(&pname);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::GetTexParameterivReply>(
      &buf, "Glx::GetTexParameteriv", false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::GetTexParameterivReply> detail::ReadReply<
    Glx::GetTexParameterivReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::GetTexParameterivReply>();

  auto& sequence = (*reply).sequence;
  uint32_t n{};
  auto& datum = (*reply).datum;
  auto& data = (*reply).data;
  size_t data_len = data.size();

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
  Pad(&buf, 4);

  // n
  Read(&n, &buf);

  // datum
  Read(&datum, &buf);

  // pad2
  Pad(&buf, 12);

  // data
  data.resize(n);
  for (auto& data_elem : data) {
    // data_elem
    Read(&data_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Glx::GetTexLevelParameterfvReply> Glx::GetTexLevelParameterfv(
    const Glx::GetTexLevelParameterfvRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& target = request.target;
  auto& level = request.level;
  auto& pname = request.pname;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 138;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // target
  buf.Write(&target);

  // level
  buf.Write(&level);

  // pname
  buf.Write(&pname);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::GetTexLevelParameterfvReply>(
      &buf, "Glx::GetTexLevelParameterfv", false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::GetTexLevelParameterfvReply> detail::ReadReply<
    Glx::GetTexLevelParameterfvReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::GetTexLevelParameterfvReply>();

  auto& sequence = (*reply).sequence;
  uint32_t n{};
  auto& datum = (*reply).datum;
  auto& data = (*reply).data;
  size_t data_len = data.size();

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
  Pad(&buf, 4);

  // n
  Read(&n, &buf);

  // datum
  Read(&datum, &buf);

  // pad2
  Pad(&buf, 12);

  // data
  data.resize(n);
  for (auto& data_elem : data) {
    // data_elem
    Read(&data_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Glx::GetTexLevelParameterivReply> Glx::GetTexLevelParameteriv(
    const Glx::GetTexLevelParameterivRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& target = request.target;
  auto& level = request.level;
  auto& pname = request.pname;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 139;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // target
  buf.Write(&target);

  // level
  buf.Write(&level);

  // pname
  buf.Write(&pname);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::GetTexLevelParameterivReply>(
      &buf, "Glx::GetTexLevelParameteriv", false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::GetTexLevelParameterivReply> detail::ReadReply<
    Glx::GetTexLevelParameterivReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::GetTexLevelParameterivReply>();

  auto& sequence = (*reply).sequence;
  uint32_t n{};
  auto& datum = (*reply).datum;
  auto& data = (*reply).data;
  size_t data_len = data.size();

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
  Pad(&buf, 4);

  // n
  Read(&n, &buf);

  // datum
  Read(&datum, &buf);

  // pad2
  Pad(&buf, 12);

  // data
  data.resize(n);
  for (auto& data_elem : data) {
    // data_elem
    Read(&data_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Glx::IsEnabledReply> Glx::IsEnabled(
    const Glx::IsEnabledRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& capability = request.capability;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 140;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // capability
  buf.Write(&capability);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::IsEnabledReply>(&buf, "Glx::IsEnabled",
                                                       false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::IsEnabledReply> detail::ReadReply<Glx::IsEnabledReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::IsEnabledReply>();

  auto& sequence = (*reply).sequence;
  auto& ret_val = (*reply).ret_val;

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

  // ret_val
  Read(&ret_val, &buf);

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Glx::IsListReply> Glx::IsList(const Glx::IsListRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& list = request.list;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 141;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // list
  buf.Write(&list);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::IsListReply>(&buf, "Glx::IsList", false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::IsListReply> detail::ReadReply<Glx::IsListReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::IsListReply>();

  auto& sequence = (*reply).sequence;
  auto& ret_val = (*reply).ret_val;

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

  // ret_val
  Read(&ret_val, &buf);

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Glx::Flush(const Glx::FlushRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 142;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Glx::Flush", false);
}

Future<Glx::AreTexturesResidentReply> Glx::AreTexturesResident(
    const Glx::AreTexturesResidentRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  int32_t n{};
  auto& textures = request.textures;
  size_t textures_len = textures.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 143;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // n
  n = textures.size();
  buf.Write(&n);

  // textures
  DCHECK_EQ(static_cast<size_t>(n), textures.size());
  for (auto& textures_elem : textures) {
    // textures_elem
    buf.Write(&textures_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<Glx::AreTexturesResidentReply>(
      &buf, "Glx::AreTexturesResident", false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::AreTexturesResidentReply> detail::ReadReply<
    Glx::AreTexturesResidentReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::AreTexturesResidentReply>();

  auto& sequence = (*reply).sequence;
  auto& ret_val = (*reply).ret_val;
  auto& data = (*reply).data;
  size_t data_len = data.size();

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

  // ret_val
  Read(&ret_val, &buf);

  // pad1
  Pad(&buf, 20);

  // data
  data.resize((length) * (4));
  for (auto& data_elem : data) {
    // data_elem
    Read(&data_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Glx::DeleteTextures(const Glx::DeleteTexturesRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  int32_t n{};
  auto& textures = request.textures;
  size_t textures_len = textures.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 144;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // n
  n = textures.size();
  buf.Write(&n);

  // textures
  DCHECK_EQ(static_cast<size_t>(n), textures.size());
  for (auto& textures_elem : textures) {
    // textures_elem
    buf.Write(&textures_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Glx::DeleteTextures", false);
}

Future<Glx::GenTexturesReply> Glx::GenTextures(
    const Glx::GenTexturesRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& n = request.n;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 145;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // n
  buf.Write(&n);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::GenTexturesReply>(
      &buf, "Glx::GenTextures", false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::GenTexturesReply> detail::ReadReply<Glx::GenTexturesReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::GenTexturesReply>();

  auto& sequence = (*reply).sequence;
  auto& data = (*reply).data;
  size_t data_len = data.size();

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

  // data
  data.resize(length);
  for (auto& data_elem : data) {
    // data_elem
    Read(&data_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Glx::IsTextureReply> Glx::IsTexture(
    const Glx::IsTextureRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& texture = request.texture;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 146;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // texture
  buf.Write(&texture);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::IsTextureReply>(&buf, "Glx::IsTexture",
                                                       false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::IsTextureReply> detail::ReadReply<Glx::IsTextureReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::IsTextureReply>();

  auto& sequence = (*reply).sequence;
  auto& ret_val = (*reply).ret_val;

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

  // ret_val
  Read(&ret_val, &buf);

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Glx::GetColorTableReply> Glx::GetColorTable(
    const Glx::GetColorTableRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& target = request.target;
  auto& format = request.format;
  auto& type = request.type;
  auto& swap_bytes = request.swap_bytes;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 147;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // target
  buf.Write(&target);

  // format
  buf.Write(&format);

  // type
  buf.Write(&type);

  // swap_bytes
  buf.Write(&swap_bytes);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::GetColorTableReply>(
      &buf, "Glx::GetColorTable", false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::GetColorTableReply> detail::ReadReply<
    Glx::GetColorTableReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::GetColorTableReply>();

  auto& sequence = (*reply).sequence;
  auto& width = (*reply).width;
  auto& data = (*reply).data;
  size_t data_len = data.size();

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
  Pad(&buf, 8);

  // width
  Read(&width, &buf);

  // pad2
  Pad(&buf, 12);

  // data
  data.resize((length) * (4));
  for (auto& data_elem : data) {
    // data_elem
    Read(&data_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Glx::GetColorTableParameterfvReply> Glx::GetColorTableParameterfv(
    const Glx::GetColorTableParameterfvRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& target = request.target;
  auto& pname = request.pname;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 148;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // target
  buf.Write(&target);

  // pname
  buf.Write(&pname);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::GetColorTableParameterfvReply>(
      &buf, "Glx::GetColorTableParameterfv", false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::GetColorTableParameterfvReply> detail::ReadReply<
    Glx::GetColorTableParameterfvReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::GetColorTableParameterfvReply>();

  auto& sequence = (*reply).sequence;
  uint32_t n{};
  auto& datum = (*reply).datum;
  auto& data = (*reply).data;
  size_t data_len = data.size();

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
  Pad(&buf, 4);

  // n
  Read(&n, &buf);

  // datum
  Read(&datum, &buf);

  // pad2
  Pad(&buf, 12);

  // data
  data.resize(n);
  for (auto& data_elem : data) {
    // data_elem
    Read(&data_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Glx::GetColorTableParameterivReply> Glx::GetColorTableParameteriv(
    const Glx::GetColorTableParameterivRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& target = request.target;
  auto& pname = request.pname;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 149;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // target
  buf.Write(&target);

  // pname
  buf.Write(&pname);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::GetColorTableParameterivReply>(
      &buf, "Glx::GetColorTableParameteriv", false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::GetColorTableParameterivReply> detail::ReadReply<
    Glx::GetColorTableParameterivReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::GetColorTableParameterivReply>();

  auto& sequence = (*reply).sequence;
  uint32_t n{};
  auto& datum = (*reply).datum;
  auto& data = (*reply).data;
  size_t data_len = data.size();

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
  Pad(&buf, 4);

  // n
  Read(&n, &buf);

  // datum
  Read(&datum, &buf);

  // pad2
  Pad(&buf, 12);

  // data
  data.resize(n);
  for (auto& data_elem : data) {
    // data_elem
    Read(&data_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Glx::GetConvolutionFilterReply> Glx::GetConvolutionFilter(
    const Glx::GetConvolutionFilterRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& target = request.target;
  auto& format = request.format;
  auto& type = request.type;
  auto& swap_bytes = request.swap_bytes;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 150;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // target
  buf.Write(&target);

  // format
  buf.Write(&format);

  // type
  buf.Write(&type);

  // swap_bytes
  buf.Write(&swap_bytes);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::GetConvolutionFilterReply>(
      &buf, "Glx::GetConvolutionFilter", false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::GetConvolutionFilterReply> detail::ReadReply<
    Glx::GetConvolutionFilterReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::GetConvolutionFilterReply>();

  auto& sequence = (*reply).sequence;
  auto& width = (*reply).width;
  auto& height = (*reply).height;
  auto& data = (*reply).data;
  size_t data_len = data.size();

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
  Pad(&buf, 8);

  // width
  Read(&width, &buf);

  // height
  Read(&height, &buf);

  // pad2
  Pad(&buf, 8);

  // data
  data.resize((length) * (4));
  for (auto& data_elem : data) {
    // data_elem
    Read(&data_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Glx::GetConvolutionParameterfvReply> Glx::GetConvolutionParameterfv(
    const Glx::GetConvolutionParameterfvRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& target = request.target;
  auto& pname = request.pname;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 151;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // target
  buf.Write(&target);

  // pname
  buf.Write(&pname);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::GetConvolutionParameterfvReply>(
      &buf, "Glx::GetConvolutionParameterfv", false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::GetConvolutionParameterfvReply> detail::ReadReply<
    Glx::GetConvolutionParameterfvReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::GetConvolutionParameterfvReply>();

  auto& sequence = (*reply).sequence;
  uint32_t n{};
  auto& datum = (*reply).datum;
  auto& data = (*reply).data;
  size_t data_len = data.size();

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
  Pad(&buf, 4);

  // n
  Read(&n, &buf);

  // datum
  Read(&datum, &buf);

  // pad2
  Pad(&buf, 12);

  // data
  data.resize(n);
  for (auto& data_elem : data) {
    // data_elem
    Read(&data_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Glx::GetConvolutionParameterivReply> Glx::GetConvolutionParameteriv(
    const Glx::GetConvolutionParameterivRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& target = request.target;
  auto& pname = request.pname;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 152;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // target
  buf.Write(&target);

  // pname
  buf.Write(&pname);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::GetConvolutionParameterivReply>(
      &buf, "Glx::GetConvolutionParameteriv", false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::GetConvolutionParameterivReply> detail::ReadReply<
    Glx::GetConvolutionParameterivReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::GetConvolutionParameterivReply>();

  auto& sequence = (*reply).sequence;
  uint32_t n{};
  auto& datum = (*reply).datum;
  auto& data = (*reply).data;
  size_t data_len = data.size();

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
  Pad(&buf, 4);

  // n
  Read(&n, &buf);

  // datum
  Read(&datum, &buf);

  // pad2
  Pad(&buf, 12);

  // data
  data.resize(n);
  for (auto& data_elem : data) {
    // data_elem
    Read(&data_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Glx::GetSeparableFilterReply> Glx::GetSeparableFilter(
    const Glx::GetSeparableFilterRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& target = request.target;
  auto& format = request.format;
  auto& type = request.type;
  auto& swap_bytes = request.swap_bytes;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 153;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // target
  buf.Write(&target);

  // format
  buf.Write(&format);

  // type
  buf.Write(&type);

  // swap_bytes
  buf.Write(&swap_bytes);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::GetSeparableFilterReply>(
      &buf, "Glx::GetSeparableFilter", false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::GetSeparableFilterReply> detail::ReadReply<
    Glx::GetSeparableFilterReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::GetSeparableFilterReply>();

  auto& sequence = (*reply).sequence;
  auto& row_w = (*reply).row_w;
  auto& col_h = (*reply).col_h;
  auto& rows_and_cols = (*reply).rows_and_cols;
  size_t rows_and_cols_len = rows_and_cols.size();

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
  Pad(&buf, 8);

  // row_w
  Read(&row_w, &buf);

  // col_h
  Read(&col_h, &buf);

  // pad2
  Pad(&buf, 8);

  // rows_and_cols
  rows_and_cols.resize((length) * (4));
  for (auto& rows_and_cols_elem : rows_and_cols) {
    // rows_and_cols_elem
    Read(&rows_and_cols_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Glx::GetHistogramReply> Glx::GetHistogram(
    const Glx::GetHistogramRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& target = request.target;
  auto& format = request.format;
  auto& type = request.type;
  auto& swap_bytes = request.swap_bytes;
  auto& reset = request.reset;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 154;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // target
  buf.Write(&target);

  // format
  buf.Write(&format);

  // type
  buf.Write(&type);

  // swap_bytes
  buf.Write(&swap_bytes);

  // reset
  buf.Write(&reset);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::GetHistogramReply>(
      &buf, "Glx::GetHistogram", false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::GetHistogramReply> detail::ReadReply<
    Glx::GetHistogramReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::GetHistogramReply>();

  auto& sequence = (*reply).sequence;
  auto& width = (*reply).width;
  auto& data = (*reply).data;
  size_t data_len = data.size();

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
  Pad(&buf, 8);

  // width
  Read(&width, &buf);

  // pad2
  Pad(&buf, 12);

  // data
  data.resize((length) * (4));
  for (auto& data_elem : data) {
    // data_elem
    Read(&data_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Glx::GetHistogramParameterfvReply> Glx::GetHistogramParameterfv(
    const Glx::GetHistogramParameterfvRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& target = request.target;
  auto& pname = request.pname;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 155;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // target
  buf.Write(&target);

  // pname
  buf.Write(&pname);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::GetHistogramParameterfvReply>(
      &buf, "Glx::GetHistogramParameterfv", false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::GetHistogramParameterfvReply> detail::ReadReply<
    Glx::GetHistogramParameterfvReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::GetHistogramParameterfvReply>();

  auto& sequence = (*reply).sequence;
  uint32_t n{};
  auto& datum = (*reply).datum;
  auto& data = (*reply).data;
  size_t data_len = data.size();

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
  Pad(&buf, 4);

  // n
  Read(&n, &buf);

  // datum
  Read(&datum, &buf);

  // pad2
  Pad(&buf, 12);

  // data
  data.resize(n);
  for (auto& data_elem : data) {
    // data_elem
    Read(&data_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Glx::GetHistogramParameterivReply> Glx::GetHistogramParameteriv(
    const Glx::GetHistogramParameterivRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& target = request.target;
  auto& pname = request.pname;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 156;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // target
  buf.Write(&target);

  // pname
  buf.Write(&pname);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::GetHistogramParameterivReply>(
      &buf, "Glx::GetHistogramParameteriv", false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::GetHistogramParameterivReply> detail::ReadReply<
    Glx::GetHistogramParameterivReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::GetHistogramParameterivReply>();

  auto& sequence = (*reply).sequence;
  uint32_t n{};
  auto& datum = (*reply).datum;
  auto& data = (*reply).data;
  size_t data_len = data.size();

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
  Pad(&buf, 4);

  // n
  Read(&n, &buf);

  // datum
  Read(&datum, &buf);

  // pad2
  Pad(&buf, 12);

  // data
  data.resize(n);
  for (auto& data_elem : data) {
    // data_elem
    Read(&data_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Glx::GetMinmaxReply> Glx::GetMinmax(
    const Glx::GetMinmaxRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& target = request.target;
  auto& format = request.format;
  auto& type = request.type;
  auto& swap_bytes = request.swap_bytes;
  auto& reset = request.reset;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 157;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // target
  buf.Write(&target);

  // format
  buf.Write(&format);

  // type
  buf.Write(&type);

  // swap_bytes
  buf.Write(&swap_bytes);

  // reset
  buf.Write(&reset);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::GetMinmaxReply>(&buf, "Glx::GetMinmax",
                                                       false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::GetMinmaxReply> detail::ReadReply<Glx::GetMinmaxReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::GetMinmaxReply>();

  auto& sequence = (*reply).sequence;
  auto& data = (*reply).data;
  size_t data_len = data.size();

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

  // data
  data.resize((length) * (4));
  for (auto& data_elem : data) {
    // data_elem
    Read(&data_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Glx::GetMinmaxParameterfvReply> Glx::GetMinmaxParameterfv(
    const Glx::GetMinmaxParameterfvRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& target = request.target;
  auto& pname = request.pname;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 158;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // target
  buf.Write(&target);

  // pname
  buf.Write(&pname);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::GetMinmaxParameterfvReply>(
      &buf, "Glx::GetMinmaxParameterfv", false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::GetMinmaxParameterfvReply> detail::ReadReply<
    Glx::GetMinmaxParameterfvReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::GetMinmaxParameterfvReply>();

  auto& sequence = (*reply).sequence;
  uint32_t n{};
  auto& datum = (*reply).datum;
  auto& data = (*reply).data;
  size_t data_len = data.size();

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
  Pad(&buf, 4);

  // n
  Read(&n, &buf);

  // datum
  Read(&datum, &buf);

  // pad2
  Pad(&buf, 12);

  // data
  data.resize(n);
  for (auto& data_elem : data) {
    // data_elem
    Read(&data_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Glx::GetMinmaxParameterivReply> Glx::GetMinmaxParameteriv(
    const Glx::GetMinmaxParameterivRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& target = request.target;
  auto& pname = request.pname;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 159;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // target
  buf.Write(&target);

  // pname
  buf.Write(&pname);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::GetMinmaxParameterivReply>(
      &buf, "Glx::GetMinmaxParameteriv", false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::GetMinmaxParameterivReply> detail::ReadReply<
    Glx::GetMinmaxParameterivReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::GetMinmaxParameterivReply>();

  auto& sequence = (*reply).sequence;
  uint32_t n{};
  auto& datum = (*reply).datum;
  auto& data = (*reply).data;
  size_t data_len = data.size();

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
  Pad(&buf, 4);

  // n
  Read(&n, &buf);

  // datum
  Read(&datum, &buf);

  // pad2
  Pad(&buf, 12);

  // data
  data.resize(n);
  for (auto& data_elem : data) {
    // data_elem
    Read(&data_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Glx::GetCompressedTexImageARBReply> Glx::GetCompressedTexImageARB(
    const Glx::GetCompressedTexImageARBRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& target = request.target;
  auto& level = request.level;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 160;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // target
  buf.Write(&target);

  // level
  buf.Write(&level);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::GetCompressedTexImageARBReply>(
      &buf, "Glx::GetCompressedTexImageARB", false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::GetCompressedTexImageARBReply> detail::ReadReply<
    Glx::GetCompressedTexImageARBReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::GetCompressedTexImageARBReply>();

  auto& sequence = (*reply).sequence;
  auto& size = (*reply).size;
  auto& data = (*reply).data;
  size_t data_len = data.size();

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
  Pad(&buf, 8);

  // size
  Read(&size, &buf);

  // pad2
  Pad(&buf, 12);

  // data
  data.resize((length) * (4));
  for (auto& data_elem : data) {
    // data_elem
    Read(&data_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Glx::DeleteQueriesARB(
    const Glx::DeleteQueriesARBRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  int32_t n{};
  auto& ids = request.ids;
  size_t ids_len = ids.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 161;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // n
  n = ids.size();
  buf.Write(&n);

  // ids
  DCHECK_EQ(static_cast<size_t>(n), ids.size());
  for (auto& ids_elem : ids) {
    // ids_elem
    buf.Write(&ids_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Glx::DeleteQueriesARB", false);
}

Future<Glx::GenQueriesARBReply> Glx::GenQueriesARB(
    const Glx::GenQueriesARBRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& n = request.n;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 162;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // n
  buf.Write(&n);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::GenQueriesARBReply>(
      &buf, "Glx::GenQueriesARB", false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::GenQueriesARBReply> detail::ReadReply<
    Glx::GenQueriesARBReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::GenQueriesARBReply>();

  auto& sequence = (*reply).sequence;
  auto& data = (*reply).data;
  size_t data_len = data.size();

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

  // data
  data.resize(length);
  for (auto& data_elem : data) {
    // data_elem
    Read(&data_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Glx::IsQueryARBReply> Glx::IsQueryARB(
    const Glx::IsQueryARBRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& id = request.id;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 163;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // id
  buf.Write(&id);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::IsQueryARBReply>(&buf, "Glx::IsQueryARB",
                                                        false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::IsQueryARBReply> detail::ReadReply<Glx::IsQueryARBReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::IsQueryARBReply>();

  auto& sequence = (*reply).sequence;
  auto& ret_val = (*reply).ret_val;

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

  // ret_val
  Read(&ret_val, &buf);

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Glx::GetQueryivARBReply> Glx::GetQueryivARB(
    const Glx::GetQueryivARBRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& target = request.target;
  auto& pname = request.pname;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 164;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // target
  buf.Write(&target);

  // pname
  buf.Write(&pname);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::GetQueryivARBReply>(
      &buf, "Glx::GetQueryivARB", false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::GetQueryivARBReply> detail::ReadReply<
    Glx::GetQueryivARBReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::GetQueryivARBReply>();

  auto& sequence = (*reply).sequence;
  uint32_t n{};
  auto& datum = (*reply).datum;
  auto& data = (*reply).data;
  size_t data_len = data.size();

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
  Pad(&buf, 4);

  // n
  Read(&n, &buf);

  // datum
  Read(&datum, &buf);

  // pad2
  Pad(&buf, 12);

  // data
  data.resize(n);
  for (auto& data_elem : data) {
    // data_elem
    Read(&data_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Glx::GetQueryObjectivARBReply> Glx::GetQueryObjectivARB(
    const Glx::GetQueryObjectivARBRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& id = request.id;
  auto& pname = request.pname;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 165;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // id
  buf.Write(&id);

  // pname
  buf.Write(&pname);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::GetQueryObjectivARBReply>(
      &buf, "Glx::GetQueryObjectivARB", false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::GetQueryObjectivARBReply> detail::ReadReply<
    Glx::GetQueryObjectivARBReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::GetQueryObjectivARBReply>();

  auto& sequence = (*reply).sequence;
  uint32_t n{};
  auto& datum = (*reply).datum;
  auto& data = (*reply).data;
  size_t data_len = data.size();

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
  Pad(&buf, 4);

  // n
  Read(&n, &buf);

  // datum
  Read(&datum, &buf);

  // pad2
  Pad(&buf, 12);

  // data
  data.resize(n);
  for (auto& data_elem : data) {
    // data_elem
    Read(&data_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Glx::GetQueryObjectuivARBReply> Glx::GetQueryObjectuivARB(
    const Glx::GetQueryObjectuivARBRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_tag = request.context_tag;
  auto& id = request.id;
  auto& pname = request.pname;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 166;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_tag
  buf.Write(&context_tag);

  // id
  buf.Write(&id);

  // pname
  buf.Write(&pname);

  Align(&buf, 4);

  return connection_->SendRequest<Glx::GetQueryObjectuivARBReply>(
      &buf, "Glx::GetQueryObjectuivARB", false);
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Glx::GetQueryObjectuivARBReply> detail::ReadReply<
    Glx::GetQueryObjectuivARBReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Glx::GetQueryObjectuivARBReply>();

  auto& sequence = (*reply).sequence;
  uint32_t n{};
  auto& datum = (*reply).datum;
  auto& data = (*reply).data;
  size_t data_len = data.size();

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
  Pad(&buf, 4);

  // n
  Read(&n, &buf);

  // datum
  Read(&datum, &buf);

  // pad2
  Pad(&buf, 12);

  // data
  data.resize(n);
  for (auto& data_elem : data) {
    // data_elem
    Read(&data_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

}  // namespace x11
