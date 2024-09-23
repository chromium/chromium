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

#include "randr.h"

#include <unistd.h>
#include <xcb/xcb.h>
#include <xcb/xcbext.h>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/xproto_internal.h"

namespace x11 {

RandR::RandR(Connection* connection, const x11::QueryExtensionReply& info)
    : connection_(connection), info_(info) {}

std::string RandR::BadOutputError::ToString() const {
  std::stringstream ss_;
  ss_ << "RandR::BadOutputError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<RandR::BadOutputError>(RandR::BadOutputError* error_,
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

  CHECK_LE(buf.offset, 32ul);
}

std::string RandR::BadCrtcError::ToString() const {
  std::stringstream ss_;
  ss_ << "RandR::BadCrtcError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<RandR::BadCrtcError>(RandR::BadCrtcError* error_,
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

  CHECK_LE(buf.offset, 32ul);
}

std::string RandR::BadModeError::ToString() const {
  std::stringstream ss_;
  ss_ << "RandR::BadModeError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<RandR::BadModeError>(RandR::BadModeError* error_,
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

  CHECK_LE(buf.offset, 32ul);
}

std::string RandR::BadProviderError::ToString() const {
  std::stringstream ss_;
  ss_ << "RandR::BadProviderError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<RandR::BadProviderError>(RandR::BadProviderError* error_,
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

  CHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<RandR::ScreenChangeNotifyEvent>(
    RandR::ScreenChangeNotifyEvent* event_,
    ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& rotation = (*event_).rotation;
  auto& sequence = (*event_).sequence;
  auto& timestamp = (*event_).timestamp;
  auto& config_timestamp = (*event_).config_timestamp;
  auto& root = (*event_).root;
  auto& request_window = (*event_).request_window;
  auto& sizeID = (*event_).sizeID;
  auto& subpixel_order = (*event_).subpixel_order;
  auto& width = (*event_).width;
  auto& height = (*event_).height;
  auto& mwidth = (*event_).mwidth;
  auto& mheight = (*event_).mheight;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // rotation
  uint8_t tmp0;
  Read(&tmp0, &buf);
  rotation = static_cast<RandR::Rotation>(tmp0);

  // sequence
  Read(&sequence, &buf);

  // timestamp
  Read(&timestamp, &buf);

  // config_timestamp
  Read(&config_timestamp, &buf);

  // root
  Read(&root, &buf);

  // request_window
  Read(&request_window, &buf);

  // sizeID
  Read(&sizeID, &buf);

  // subpixel_order
  uint16_t tmp1;
  Read(&tmp1, &buf);
  subpixel_order = static_cast<Render::SubPixel>(tmp1);

  // width
  Read(&width, &buf);

  // height
  Read(&height, &buf);

  // mwidth
  Read(&mwidth, &buf);

  // mheight
  Read(&mheight, &buf);

  CHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<RandR::NotifyEvent>(RandR::NotifyEvent* event_,
                                   ReadBuffer* buffer) {
  auto& buf = *buffer;

  RandR::Notify subCode{};
  auto& sequence = (*event_).sequence;
  auto& data = (*event_);

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // subCode
  uint8_t tmp2;
  Read(&tmp2, &buf);
  subCode = static_cast<RandR::Notify>(tmp2);

  // sequence
  Read(&sequence, &buf);

  // data
  auto data_expr = subCode;
  if (CaseEq(data_expr, RandR::Notify::CrtcChange)) {
    data.cc.emplace(decltype(data.cc)::value_type());
    auto& timestamp = (*data.cc).timestamp;
    auto& window = (*data.cc).window;
    auto& crtc = (*data.cc).crtc;
    auto& mode = (*data.cc).mode;
    auto& rotation = (*data.cc).rotation;
    auto& x = (*data.cc).x;
    auto& y = (*data.cc).y;
    auto& width = (*data.cc).width;
    auto& height = (*data.cc).height;

    // timestamp
    Read(&timestamp, &buf);

    // window
    Read(&window, &buf);

    // crtc
    Read(&crtc, &buf);

    // mode
    Read(&mode, &buf);

    // rotation
    uint16_t tmp3;
    Read(&tmp3, &buf);
    rotation = static_cast<RandR::Rotation>(tmp3);

    // pad0
    Pad(&buf, 2);

    // x
    Read(&x, &buf);

    // y
    Read(&y, &buf);

    // width
    Read(&width, &buf);

    // height
    Read(&height, &buf);
  }
  if (CaseEq(data_expr, RandR::Notify::OutputChange)) {
    data.oc.emplace(decltype(data.oc)::value_type());
    auto& timestamp = (*data.oc).timestamp;
    auto& config_timestamp = (*data.oc).config_timestamp;
    auto& window = (*data.oc).window;
    auto& output = (*data.oc).output;
    auto& crtc = (*data.oc).crtc;
    auto& mode = (*data.oc).mode;
    auto& rotation = (*data.oc).rotation;
    auto& connection = (*data.oc).connection;
    auto& subpixel_order = (*data.oc).subpixel_order;

    // timestamp
    Read(&timestamp, &buf);

    // config_timestamp
    Read(&config_timestamp, &buf);

    // window
    Read(&window, &buf);

    // output
    Read(&output, &buf);

    // crtc
    Read(&crtc, &buf);

    // mode
    Read(&mode, &buf);

    // rotation
    uint16_t tmp4;
    Read(&tmp4, &buf);
    rotation = static_cast<RandR::Rotation>(tmp4);

    // connection
    uint8_t tmp5;
    Read(&tmp5, &buf);
    connection = static_cast<RandR::RandRConnection>(tmp5);

    // subpixel_order
    uint8_t tmp6;
    Read(&tmp6, &buf);
    subpixel_order = static_cast<Render::SubPixel>(tmp6);
  }
  if (CaseEq(data_expr, RandR::Notify::OutputProperty)) {
    data.op.emplace(decltype(data.op)::value_type());
    auto& window = (*data.op).window;
    auto& output = (*data.op).output;
    auto& atom = (*data.op).atom;
    auto& timestamp = (*data.op).timestamp;
    auto& status = (*data.op).status;

    // window
    Read(&window, &buf);

    // output
    Read(&output, &buf);

    // atom
    Read(&atom, &buf);

    // timestamp
    Read(&timestamp, &buf);

    // status
    uint8_t tmp7;
    Read(&tmp7, &buf);
    status = static_cast<Property>(tmp7);

    // pad1
    Pad(&buf, 11);
  }
  if (CaseEq(data_expr, RandR::Notify::ProviderChange)) {
    data.pc.emplace(decltype(data.pc)::value_type());
    auto& timestamp = (*data.pc).timestamp;
    auto& window = (*data.pc).window;
    auto& provider = (*data.pc).provider;

    // timestamp
    Read(&timestamp, &buf);

    // window
    Read(&window, &buf);

    // provider
    Read(&provider, &buf);

    // pad2
    Pad(&buf, 16);
  }
  if (CaseEq(data_expr, RandR::Notify::ProviderProperty)) {
    data.pp.emplace(decltype(data.pp)::value_type());
    auto& window = (*data.pp).window;
    auto& provider = (*data.pp).provider;
    auto& atom = (*data.pp).atom;
    auto& timestamp = (*data.pp).timestamp;
    auto& state = (*data.pp).state;

    // window
    Read(&window, &buf);

    // provider
    Read(&provider, &buf);

    // atom
    Read(&atom, &buf);

    // timestamp
    Read(&timestamp, &buf);

    // state
    Read(&state, &buf);

    // pad3
    Pad(&buf, 11);
  }
  if (CaseEq(data_expr, RandR::Notify::ResourceChange)) {
    data.rc.emplace(decltype(data.rc)::value_type());
    auto& timestamp = (*data.rc).timestamp;
    auto& window = (*data.rc).window;

    // timestamp
    Read(&timestamp, &buf);

    // window
    Read(&window, &buf);

    // pad4
    Pad(&buf, 20);
  }
  if (CaseEq(data_expr, RandR::Notify::Lease)) {
    data.lc.emplace(decltype(data.lc)::value_type());
    auto& timestamp = (*data.lc).timestamp;
    auto& window = (*data.lc).window;
    auto& lease = (*data.lc).lease;
    auto& created = (*data.lc).created;

    // timestamp
    Read(&timestamp, &buf);

    // window
    Read(&window, &buf);

    // lease
    Read(&lease, &buf);

    // created
    Read(&created, &buf);

    // pad5
    Pad(&buf, 15);
  }

  CHECK_LE(buf.offset, 32ul);
}

Future<RandR::QueryVersionReply> RandR::QueryVersion(
    const RandR::QueryVersionRequest& request) {
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

  return connection_->SendRequest<RandR::QueryVersionReply>(
      &buf, "RandR::QueryVersion", false);
}

Future<RandR::QueryVersionReply> RandR::QueryVersion(
    const uint32_t& major_version,
    const uint32_t& minor_version) {
  return RandR::QueryVersion(
      RandR::QueryVersionRequest{major_version, minor_version});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<RandR::QueryVersionReply> detail::ReadReply<
    RandR::QueryVersionReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<RandR::QueryVersionReply>();

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
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<RandR::SetScreenConfigReply> RandR::SetScreenConfig(
    const RandR::SetScreenConfigRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& window = request.window;
  auto& timestamp = request.timestamp;
  auto& config_timestamp = request.config_timestamp;
  auto& sizeID = request.sizeID;
  auto& rotation = request.rotation;
  auto& rate = request.rate;

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

  // timestamp
  buf.Write(&timestamp);

  // config_timestamp
  buf.Write(&config_timestamp);

  // sizeID
  buf.Write(&sizeID);

  // rotation
  uint16_t tmp8;
  tmp8 = static_cast<uint16_t>(rotation);
  buf.Write(&tmp8);

  // rate
  buf.Write(&rate);

  // pad0
  Pad(&buf, 2);

  Align(&buf, 4);

  return connection_->SendRequest<RandR::SetScreenConfigReply>(
      &buf, "RandR::SetScreenConfig", false);
}

Future<RandR::SetScreenConfigReply> RandR::SetScreenConfig(
    const Window& window,
    const Time& timestamp,
    const Time& config_timestamp,
    const uint16_t& sizeID,
    const Rotation& rotation,
    const uint16_t& rate) {
  return RandR::SetScreenConfig(RandR::SetScreenConfigRequest{
      window, timestamp, config_timestamp, sizeID, rotation, rate});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<RandR::SetScreenConfigReply> detail::ReadReply<
    RandR::SetScreenConfigReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<RandR::SetScreenConfigReply>();

  auto& status = (*reply).status;
  auto& sequence = (*reply).sequence;
  auto& new_timestamp = (*reply).new_timestamp;
  auto& config_timestamp = (*reply).config_timestamp;
  auto& root = (*reply).root;
  auto& subpixel_order = (*reply).subpixel_order;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // status
  uint8_t tmp9;
  Read(&tmp9, &buf);
  status = static_cast<RandR::SetConfig>(tmp9);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // new_timestamp
  Read(&new_timestamp, &buf);

  // config_timestamp
  Read(&config_timestamp, &buf);

  // root
  Read(&root, &buf);

  // subpixel_order
  uint16_t tmp10;
  Read(&tmp10, &buf);
  subpixel_order = static_cast<Render::SubPixel>(tmp10);

  // pad0
  Pad(&buf, 10);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> RandR::SelectInput(const RandR::SelectInputRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& window = request.window;
  auto& enable = request.enable;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 4;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // window
  buf.Write(&window);

  // enable
  uint16_t tmp11;
  tmp11 = static_cast<uint16_t>(enable);
  buf.Write(&tmp11);

  // pad0
  Pad(&buf, 2);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "RandR::SelectInput", false);
}

Future<void> RandR::SelectInput(const Window& window,
                                const NotifyMask& enable) {
  return RandR::SelectInput(RandR::SelectInputRequest{window, enable});
}

Future<RandR::GetScreenInfoReply> RandR::GetScreenInfo(
    const RandR::GetScreenInfoRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& window = request.window;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 5;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // window
  buf.Write(&window);

  Align(&buf, 4);

  return connection_->SendRequest<RandR::GetScreenInfoReply>(
      &buf, "RandR::GetScreenInfo", false);
}

Future<RandR::GetScreenInfoReply> RandR::GetScreenInfo(const Window& window) {
  return RandR::GetScreenInfo(RandR::GetScreenInfoRequest{window});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<RandR::GetScreenInfoReply> detail::ReadReply<
    RandR::GetScreenInfoReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<RandR::GetScreenInfoReply>();

  auto& rotations = (*reply).rotations;
  auto& sequence = (*reply).sequence;
  auto& root = (*reply).root;
  auto& timestamp = (*reply).timestamp;
  auto& config_timestamp = (*reply).config_timestamp;
  uint16_t nSizes{};
  auto& sizeID = (*reply).sizeID;
  auto& rotation = (*reply).rotation;
  auto& rate = (*reply).rate;
  auto& nInfo = (*reply).nInfo;
  auto& sizes = (*reply).sizes;
  auto& rates = (*reply).rates;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // rotations
  uint8_t tmp12;
  Read(&tmp12, &buf);
  rotations = static_cast<RandR::Rotation>(tmp12);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // root
  Read(&root, &buf);

  // timestamp
  Read(&timestamp, &buf);

  // config_timestamp
  Read(&config_timestamp, &buf);

  // nSizes
  Read(&nSizes, &buf);

  // sizeID
  Read(&sizeID, &buf);

  // rotation
  uint16_t tmp13;
  Read(&tmp13, &buf);
  rotation = static_cast<RandR::Rotation>(tmp13);

  // rate
  Read(&rate, &buf);

  // nInfo
  Read(&nInfo, &buf);

  // pad0
  Pad(&buf, 2);

  // sizes
  sizes.resize(nSizes);
  for (auto& sizes_elem : sizes) {
    // sizes_elem
    {
      auto& width = sizes_elem.width;
      auto& height = sizes_elem.height;
      auto& mwidth = sizes_elem.mwidth;
      auto& mheight = sizes_elem.mheight;

      // width
      Read(&width, &buf);

      // height
      Read(&height, &buf);

      // mwidth
      Read(&mwidth, &buf);

      // mheight
      Read(&mheight, &buf);
    }
  }

  // rates
  rates.resize((nInfo) - (nSizes));
  for (auto& rates_elem : rates) {
    // rates_elem
    {
      uint16_t nRates{};
      auto& rates = rates_elem.rates;

      // nRates
      Read(&nRates, &buf);

      // rates
      rates.resize(nRates);
      for (auto& rates_elem : rates) {
        // rates_elem
        Read(&rates_elem, &buf);
      }
    }
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<RandR::GetScreenSizeRangeReply> RandR::GetScreenSizeRange(
    const RandR::GetScreenSizeRangeRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& window = request.window;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 6;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // window
  buf.Write(&window);

  Align(&buf, 4);

  return connection_->SendRequest<RandR::GetScreenSizeRangeReply>(
      &buf, "RandR::GetScreenSizeRange", false);
}

Future<RandR::GetScreenSizeRangeReply> RandR::GetScreenSizeRange(
    const Window& window) {
  return RandR::GetScreenSizeRange(RandR::GetScreenSizeRangeRequest{window});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<RandR::GetScreenSizeRangeReply> detail::ReadReply<
    RandR::GetScreenSizeRangeReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<RandR::GetScreenSizeRangeReply>();

  auto& sequence = (*reply).sequence;
  auto& min_width = (*reply).min_width;
  auto& min_height = (*reply).min_height;
  auto& max_width = (*reply).max_width;
  auto& max_height = (*reply).max_height;

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

  // min_width
  Read(&min_width, &buf);

  // min_height
  Read(&min_height, &buf);

  // max_width
  Read(&max_width, &buf);

  // max_height
  Read(&max_height, &buf);

  // pad1
  Pad(&buf, 16);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> RandR::SetScreenSize(const RandR::SetScreenSizeRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& window = request.window;
  auto& width = request.width;
  auto& height = request.height;
  auto& mm_width = request.mm_width;
  auto& mm_height = request.mm_height;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 7;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // window
  buf.Write(&window);

  // width
  buf.Write(&width);

  // height
  buf.Write(&height);

  // mm_width
  buf.Write(&mm_width);

  // mm_height
  buf.Write(&mm_height);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "RandR::SetScreenSize", false);
}

Future<void> RandR::SetScreenSize(const Window& window,
                                  const uint16_t& width,
                                  const uint16_t& height,
                                  const uint32_t& mm_width,
                                  const uint32_t& mm_height) {
  return RandR::SetScreenSize(
      RandR::SetScreenSizeRequest{window, width, height, mm_width, mm_height});
}

Future<RandR::GetScreenResourcesReply> RandR::GetScreenResources(
    const RandR::GetScreenResourcesRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& window = request.window;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 8;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // window
  buf.Write(&window);

  Align(&buf, 4);

  return connection_->SendRequest<RandR::GetScreenResourcesReply>(
      &buf, "RandR::GetScreenResources", false);
}

Future<RandR::GetScreenResourcesReply> RandR::GetScreenResources(
    const Window& window) {
  return RandR::GetScreenResources(RandR::GetScreenResourcesRequest{window});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<RandR::GetScreenResourcesReply> detail::ReadReply<
    RandR::GetScreenResourcesReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<RandR::GetScreenResourcesReply>();

  auto& sequence = (*reply).sequence;
  auto& timestamp = (*reply).timestamp;
  auto& config_timestamp = (*reply).config_timestamp;
  uint16_t num_crtcs{};
  uint16_t num_outputs{};
  uint16_t num_modes{};
  uint16_t names_len{};
  auto& crtcs = (*reply).crtcs;
  auto& outputs = (*reply).outputs;
  auto& modes = (*reply).modes;
  auto& names = (*reply).names;

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

  // timestamp
  Read(&timestamp, &buf);

  // config_timestamp
  Read(&config_timestamp, &buf);

  // num_crtcs
  Read(&num_crtcs, &buf);

  // num_outputs
  Read(&num_outputs, &buf);

  // num_modes
  Read(&num_modes, &buf);

  // names_len
  Read(&names_len, &buf);

  // pad1
  Pad(&buf, 8);

  // crtcs
  crtcs.resize(num_crtcs);
  for (auto& crtcs_elem : crtcs) {
    // crtcs_elem
    Read(&crtcs_elem, &buf);
  }

  // outputs
  outputs.resize(num_outputs);
  for (auto& outputs_elem : outputs) {
    // outputs_elem
    Read(&outputs_elem, &buf);
  }

  // modes
  modes.resize(num_modes);
  for (auto& modes_elem : modes) {
    // modes_elem
    {
      auto& id = modes_elem.id;
      auto& width = modes_elem.width;
      auto& height = modes_elem.height;
      auto& dot_clock = modes_elem.dot_clock;
      auto& hsync_start = modes_elem.hsync_start;
      auto& hsync_end = modes_elem.hsync_end;
      auto& htotal = modes_elem.htotal;
      auto& hskew = modes_elem.hskew;
      auto& vsync_start = modes_elem.vsync_start;
      auto& vsync_end = modes_elem.vsync_end;
      auto& vtotal = modes_elem.vtotal;
      auto& name_len = modes_elem.name_len;
      auto& mode_flags = modes_elem.mode_flags;

      // id
      Read(&id, &buf);

      // width
      Read(&width, &buf);

      // height
      Read(&height, &buf);

      // dot_clock
      Read(&dot_clock, &buf);

      // hsync_start
      Read(&hsync_start, &buf);

      // hsync_end
      Read(&hsync_end, &buf);

      // htotal
      Read(&htotal, &buf);

      // hskew
      Read(&hskew, &buf);

      // vsync_start
      Read(&vsync_start, &buf);

      // vsync_end
      Read(&vsync_end, &buf);

      // vtotal
      Read(&vtotal, &buf);

      // name_len
      Read(&name_len, &buf);

      // mode_flags
      uint32_t tmp14;
      Read(&tmp14, &buf);
      mode_flags = static_cast<RandR::ModeFlag>(tmp14);
    }
  }

  // names
  names.resize(names_len);
  for (auto& names_elem : names) {
    // names_elem
    Read(&names_elem, &buf);
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<RandR::GetOutputInfoReply> RandR::GetOutputInfo(
    const RandR::GetOutputInfoRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& output = request.output;
  auto& config_timestamp = request.config_timestamp;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 9;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // output
  buf.Write(&output);

  // config_timestamp
  buf.Write(&config_timestamp);

  Align(&buf, 4);

  return connection_->SendRequest<RandR::GetOutputInfoReply>(
      &buf, "RandR::GetOutputInfo", false);
}

Future<RandR::GetOutputInfoReply> RandR::GetOutputInfo(
    const Output& output,
    const Time& config_timestamp) {
  return RandR::GetOutputInfo(
      RandR::GetOutputInfoRequest{output, config_timestamp});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<RandR::GetOutputInfoReply> detail::ReadReply<
    RandR::GetOutputInfoReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<RandR::GetOutputInfoReply>();

  auto& status = (*reply).status;
  auto& sequence = (*reply).sequence;
  auto& timestamp = (*reply).timestamp;
  auto& crtc = (*reply).crtc;
  auto& mm_width = (*reply).mm_width;
  auto& mm_height = (*reply).mm_height;
  auto& connection = (*reply).connection;
  auto& subpixel_order = (*reply).subpixel_order;
  uint16_t num_crtcs{};
  uint16_t num_modes{};
  auto& num_preferred = (*reply).num_preferred;
  uint16_t num_clones{};
  uint16_t name_len{};
  auto& crtcs = (*reply).crtcs;
  auto& modes = (*reply).modes;
  auto& clones = (*reply).clones;
  auto& name = (*reply).name;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // status
  uint8_t tmp15;
  Read(&tmp15, &buf);
  status = static_cast<RandR::SetConfig>(tmp15);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // timestamp
  Read(&timestamp, &buf);

  // crtc
  Read(&crtc, &buf);

  // mm_width
  Read(&mm_width, &buf);

  // mm_height
  Read(&mm_height, &buf);

  // connection
  uint8_t tmp16;
  Read(&tmp16, &buf);
  connection = static_cast<RandR::RandRConnection>(tmp16);

  // subpixel_order
  uint8_t tmp17;
  Read(&tmp17, &buf);
  subpixel_order = static_cast<Render::SubPixel>(tmp17);

  // num_crtcs
  Read(&num_crtcs, &buf);

  // num_modes
  Read(&num_modes, &buf);

  // num_preferred
  Read(&num_preferred, &buf);

  // num_clones
  Read(&num_clones, &buf);

  // name_len
  Read(&name_len, &buf);

  // crtcs
  crtcs.resize(num_crtcs);
  for (auto& crtcs_elem : crtcs) {
    // crtcs_elem
    Read(&crtcs_elem, &buf);
  }

  // modes
  modes.resize(num_modes);
  for (auto& modes_elem : modes) {
    // modes_elem
    Read(&modes_elem, &buf);
  }

  // clones
  clones.resize(num_clones);
  for (auto& clones_elem : clones) {
    // clones_elem
    Read(&clones_elem, &buf);
  }

  // name
  name.resize(name_len);
  for (auto& name_elem : name) {
    // name_elem
    Read(&name_elem, &buf);
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<RandR::ListOutputPropertiesReply> RandR::ListOutputProperties(
    const RandR::ListOutputPropertiesRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& output = request.output;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 10;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // output
  buf.Write(&output);

  Align(&buf, 4);

  return connection_->SendRequest<RandR::ListOutputPropertiesReply>(
      &buf, "RandR::ListOutputProperties", false);
}

Future<RandR::ListOutputPropertiesReply> RandR::ListOutputProperties(
    const Output& output) {
  return RandR::ListOutputProperties(
      RandR::ListOutputPropertiesRequest{output});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<RandR::ListOutputPropertiesReply> detail::ReadReply<
    RandR::ListOutputPropertiesReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<RandR::ListOutputPropertiesReply>();

  auto& sequence = (*reply).sequence;
  uint16_t num_atoms{};
  auto& atoms = (*reply).atoms;

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

  // num_atoms
  Read(&num_atoms, &buf);

  // pad1
  Pad(&buf, 22);

  // atoms
  atoms.resize(num_atoms);
  for (auto& atoms_elem : atoms) {
    // atoms_elem
    Read(&atoms_elem, &buf);
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<RandR::QueryOutputPropertyReply> RandR::QueryOutputProperty(
    const RandR::QueryOutputPropertyRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& output = request.output;
  auto& property = request.property;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 11;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // output
  buf.Write(&output);

  // property
  buf.Write(&property);

  Align(&buf, 4);

  return connection_->SendRequest<RandR::QueryOutputPropertyReply>(
      &buf, "RandR::QueryOutputProperty", false);
}

Future<RandR::QueryOutputPropertyReply> RandR::QueryOutputProperty(
    const Output& output,
    const Atom& property) {
  return RandR::QueryOutputProperty(
      RandR::QueryOutputPropertyRequest{output, property});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<RandR::QueryOutputPropertyReply> detail::ReadReply<
    RandR::QueryOutputPropertyReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<RandR::QueryOutputPropertyReply>();

  auto& sequence = (*reply).sequence;
  auto& pending = (*reply).pending;
  auto& range = (*reply).range;
  auto& immutable = (*reply).immutable;
  auto& validValues = (*reply).validValues;

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

  // pending
  Read(&pending, &buf);

  // range
  Read(&range, &buf);

  // immutable
  Read(&immutable, &buf);

  // pad1
  Pad(&buf, 21);

  // validValues
  validValues.resize(length);
  for (auto& validValues_elem : validValues) {
    // validValues_elem
    Read(&validValues_elem, &buf);
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> RandR::ConfigureOutputProperty(
    const RandR::ConfigureOutputPropertyRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& output = request.output;
  auto& property = request.property;
  auto& pending = request.pending;
  auto& range = request.range;
  auto& values = request.values;
  size_t values_len = values.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 12;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // output
  buf.Write(&output);

  // property
  buf.Write(&property);

  // pending
  buf.Write(&pending);

  // range
  buf.Write(&range);

  // pad0
  Pad(&buf, 2);

  // values
  CHECK_EQ(static_cast<size_t>(values_len), values.size());
  for (auto& values_elem : values) {
    // values_elem
    buf.Write(&values_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "RandR::ConfigureOutputProperty",
                                        false);
}

Future<void> RandR::ConfigureOutputProperty(
    const Output& output,
    const Atom& property,
    const uint8_t& pending,
    const uint8_t& range,
    const std::vector<int32_t>& values) {
  return RandR::ConfigureOutputProperty(RandR::ConfigureOutputPropertyRequest{
      output, property, pending, range, values});
}

Future<void> RandR::ChangeOutputProperty(
    const RandR::ChangeOutputPropertyRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& output = request.output;
  auto& property = request.property;
  auto& type = request.type;
  auto& format = request.format;
  auto& mode = request.mode;
  auto& num_units = request.num_units;
  auto& data = request.data;
  size_t data_len = data ? data->size() : 0;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 13;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // output
  buf.Write(&output);

  // property
  buf.Write(&property);

  // type
  buf.Write(&type);

  // format
  buf.Write(&format);

  // mode
  uint8_t tmp18;
  tmp18 = static_cast<uint8_t>(mode);
  buf.Write(&tmp18);

  // pad0
  Pad(&buf, 2);

  // num_units
  buf.Write(&num_units);

  // data
  buf.AppendSizedBuffer(data);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "RandR::ChangeOutputProperty",
                                        false);
}

Future<void> RandR::ChangeOutputProperty(
    const Output& output,
    const Atom& property,
    const Atom& type,
    const uint8_t& format,
    const PropMode& mode,
    const uint32_t& num_units,
    const scoped_refptr<base::RefCountedMemory>& data) {
  return RandR::ChangeOutputProperty(RandR::ChangeOutputPropertyRequest{
      output, property, type, format, mode, num_units, data});
}

Future<void> RandR::DeleteOutputProperty(
    const RandR::DeleteOutputPropertyRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& output = request.output;
  auto& property = request.property;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 14;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // output
  buf.Write(&output);

  // property
  buf.Write(&property);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "RandR::DeleteOutputProperty",
                                        false);
}

Future<void> RandR::DeleteOutputProperty(const Output& output,
                                         const Atom& property) {
  return RandR::DeleteOutputProperty(
      RandR::DeleteOutputPropertyRequest{output, property});
}

Future<RandR::GetOutputPropertyReply> RandR::GetOutputProperty(
    const RandR::GetOutputPropertyRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& output = request.output;
  auto& property = request.property;
  auto& type = request.type;
  auto& long_offset = request.long_offset;
  auto& long_length = request.long_length;
  auto& c_delete = request.c_delete;
  auto& pending = request.pending;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 15;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // output
  buf.Write(&output);

  // property
  buf.Write(&property);

  // type
  buf.Write(&type);

  // long_offset
  buf.Write(&long_offset);

  // long_length
  buf.Write(&long_length);

  // c_delete
  buf.Write(&c_delete);

  // pending
  buf.Write(&pending);

  // pad0
  Pad(&buf, 2);

  Align(&buf, 4);

  return connection_->SendRequest<RandR::GetOutputPropertyReply>(
      &buf, "RandR::GetOutputProperty", false);
}

Future<RandR::GetOutputPropertyReply> RandR::GetOutputProperty(
    const Output& output,
    const Atom& property,
    const Atom& type,
    const uint32_t& long_offset,
    const uint32_t& long_length,
    const uint8_t& c_delete,
    const uint8_t& pending) {
  return RandR::GetOutputProperty(RandR::GetOutputPropertyRequest{
      output, property, type, long_offset, long_length, c_delete, pending});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<RandR::GetOutputPropertyReply> detail::ReadReply<
    RandR::GetOutputPropertyReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<RandR::GetOutputPropertyReply>();

  auto& format = (*reply).format;
  auto& sequence = (*reply).sequence;
  auto& type = (*reply).type;
  auto& bytes_after = (*reply).bytes_after;
  auto& num_items = (*reply).num_items;
  auto& data = (*reply).data;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // format
  Read(&format, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // type
  Read(&type, &buf);

  // bytes_after
  Read(&bytes_after, &buf);

  // num_items
  Read(&num_items, &buf);

  // pad0
  Pad(&buf, 12);

  // data
  data.resize((num_items) * ((format) / (8)));
  for (auto& data_elem : data) {
    // data_elem
    Read(&data_elem, &buf);
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<RandR::CreateModeReply> RandR::CreateMode(
    const RandR::CreateModeRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& window = request.window;
  auto& mode_info = request.mode_info;
  auto& name = request.name;
  size_t name_len = name.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 16;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // window
  buf.Write(&window);

  // mode_info
  {
    auto& id = mode_info.id;
    auto& width = mode_info.width;
    auto& height = mode_info.height;
    auto& dot_clock = mode_info.dot_clock;
    auto& hsync_start = mode_info.hsync_start;
    auto& hsync_end = mode_info.hsync_end;
    auto& htotal = mode_info.htotal;
    auto& hskew = mode_info.hskew;
    auto& vsync_start = mode_info.vsync_start;
    auto& vsync_end = mode_info.vsync_end;
    auto& vtotal = mode_info.vtotal;
    auto& name_len = mode_info.name_len;
    auto& mode_flags = mode_info.mode_flags;

    // id
    buf.Write(&id);

    // width
    buf.Write(&width);

    // height
    buf.Write(&height);

    // dot_clock
    buf.Write(&dot_clock);

    // hsync_start
    buf.Write(&hsync_start);

    // hsync_end
    buf.Write(&hsync_end);

    // htotal
    buf.Write(&htotal);

    // hskew
    buf.Write(&hskew);

    // vsync_start
    buf.Write(&vsync_start);

    // vsync_end
    buf.Write(&vsync_end);

    // vtotal
    buf.Write(&vtotal);

    // name_len
    buf.Write(&name_len);

    // mode_flags
    uint32_t tmp19;
    tmp19 = static_cast<uint32_t>(mode_flags);
    buf.Write(&tmp19);
  }

  // name
  CHECK_EQ(static_cast<size_t>(name_len), name.size());
  for (auto& name_elem : name) {
    // name_elem
    buf.Write(&name_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<RandR::CreateModeReply>(
      &buf, "RandR::CreateMode", false);
}

Future<RandR::CreateModeReply> RandR::CreateMode(const Window& window,
                                                 const ModeInfo& mode_info,
                                                 const std::string& name) {
  return RandR::CreateMode(RandR::CreateModeRequest{window, mode_info, name});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<RandR::CreateModeReply> detail::ReadReply<
    RandR::CreateModeReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<RandR::CreateModeReply>();

  auto& sequence = (*reply).sequence;
  auto& mode = (*reply).mode;

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

  // mode
  Read(&mode, &buf);

  // pad1
  Pad(&buf, 20);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> RandR::DestroyMode(const RandR::DestroyModeRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& mode = request.mode;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 17;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // mode
  buf.Write(&mode);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "RandR::DestroyMode", false);
}

Future<void> RandR::DestroyMode(const Mode& mode) {
  return RandR::DestroyMode(RandR::DestroyModeRequest{mode});
}

Future<void> RandR::AddOutputMode(const RandR::AddOutputModeRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& output = request.output;
  auto& mode = request.mode;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 18;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // output
  buf.Write(&output);

  // mode
  buf.Write(&mode);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "RandR::AddOutputMode", false);
}

Future<void> RandR::AddOutputMode(const Output& output, const Mode& mode) {
  return RandR::AddOutputMode(RandR::AddOutputModeRequest{output, mode});
}

Future<void> RandR::DeleteOutputMode(
    const RandR::DeleteOutputModeRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& output = request.output;
  auto& mode = request.mode;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 19;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // output
  buf.Write(&output);

  // mode
  buf.Write(&mode);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "RandR::DeleteOutputMode", false);
}

Future<void> RandR::DeleteOutputMode(const Output& output, const Mode& mode) {
  return RandR::DeleteOutputMode(RandR::DeleteOutputModeRequest{output, mode});
}

Future<RandR::GetCrtcInfoReply> RandR::GetCrtcInfo(
    const RandR::GetCrtcInfoRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& crtc = request.crtc;
  auto& config_timestamp = request.config_timestamp;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 20;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // crtc
  buf.Write(&crtc);

  // config_timestamp
  buf.Write(&config_timestamp);

  Align(&buf, 4);

  return connection_->SendRequest<RandR::GetCrtcInfoReply>(
      &buf, "RandR::GetCrtcInfo", false);
}

Future<RandR::GetCrtcInfoReply> RandR::GetCrtcInfo(
    const Crtc& crtc,
    const Time& config_timestamp) {
  return RandR::GetCrtcInfo(RandR::GetCrtcInfoRequest{crtc, config_timestamp});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<RandR::GetCrtcInfoReply> detail::ReadReply<
    RandR::GetCrtcInfoReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<RandR::GetCrtcInfoReply>();

  auto& status = (*reply).status;
  auto& sequence = (*reply).sequence;
  auto& timestamp = (*reply).timestamp;
  auto& x = (*reply).x;
  auto& y = (*reply).y;
  auto& width = (*reply).width;
  auto& height = (*reply).height;
  auto& mode = (*reply).mode;
  auto& rotation = (*reply).rotation;
  auto& rotations = (*reply).rotations;
  uint16_t num_outputs{};
  uint16_t num_possible_outputs{};
  auto& outputs = (*reply).outputs;
  auto& possible = (*reply).possible;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // status
  uint8_t tmp20;
  Read(&tmp20, &buf);
  status = static_cast<RandR::SetConfig>(tmp20);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // timestamp
  Read(&timestamp, &buf);

  // x
  Read(&x, &buf);

  // y
  Read(&y, &buf);

  // width
  Read(&width, &buf);

  // height
  Read(&height, &buf);

  // mode
  Read(&mode, &buf);

  // rotation
  uint16_t tmp21;
  Read(&tmp21, &buf);
  rotation = static_cast<RandR::Rotation>(tmp21);

  // rotations
  uint16_t tmp22;
  Read(&tmp22, &buf);
  rotations = static_cast<RandR::Rotation>(tmp22);

  // num_outputs
  Read(&num_outputs, &buf);

  // num_possible_outputs
  Read(&num_possible_outputs, &buf);

  // outputs
  outputs.resize(num_outputs);
  for (auto& outputs_elem : outputs) {
    // outputs_elem
    Read(&outputs_elem, &buf);
  }

  // possible
  possible.resize(num_possible_outputs);
  for (auto& possible_elem : possible) {
    // possible_elem
    Read(&possible_elem, &buf);
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<RandR::SetCrtcConfigReply> RandR::SetCrtcConfig(
    const RandR::SetCrtcConfigRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& crtc = request.crtc;
  auto& timestamp = request.timestamp;
  auto& config_timestamp = request.config_timestamp;
  auto& x = request.x;
  auto& y = request.y;
  auto& mode = request.mode;
  auto& rotation = request.rotation;
  auto& outputs = request.outputs;
  size_t outputs_len = outputs.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 21;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // crtc
  buf.Write(&crtc);

  // timestamp
  buf.Write(&timestamp);

  // config_timestamp
  buf.Write(&config_timestamp);

  // x
  buf.Write(&x);

  // y
  buf.Write(&y);

  // mode
  buf.Write(&mode);

  // rotation
  uint16_t tmp23;
  tmp23 = static_cast<uint16_t>(rotation);
  buf.Write(&tmp23);

  // pad0
  Pad(&buf, 2);

  // outputs
  CHECK_EQ(static_cast<size_t>(outputs_len), outputs.size());
  for (auto& outputs_elem : outputs) {
    // outputs_elem
    buf.Write(&outputs_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<RandR::SetCrtcConfigReply>(
      &buf, "RandR::SetCrtcConfig", false);
}

Future<RandR::SetCrtcConfigReply> RandR::SetCrtcConfig(
    const Crtc& crtc,
    const Time& timestamp,
    const Time& config_timestamp,
    const int16_t& x,
    const int16_t& y,
    const Mode& mode,
    const Rotation& rotation,
    const std::vector<Output>& outputs) {
  return RandR::SetCrtcConfig(RandR::SetCrtcConfigRequest{
      crtc, timestamp, config_timestamp, x, y, mode, rotation, outputs});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<RandR::SetCrtcConfigReply> detail::ReadReply<
    RandR::SetCrtcConfigReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<RandR::SetCrtcConfigReply>();

  auto& status = (*reply).status;
  auto& sequence = (*reply).sequence;
  auto& timestamp = (*reply).timestamp;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // status
  uint8_t tmp24;
  Read(&tmp24, &buf);
  status = static_cast<RandR::SetConfig>(tmp24);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // timestamp
  Read(&timestamp, &buf);

  // pad0
  Pad(&buf, 20);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<RandR::GetCrtcGammaSizeReply> RandR::GetCrtcGammaSize(
    const RandR::GetCrtcGammaSizeRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& crtc = request.crtc;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 22;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // crtc
  buf.Write(&crtc);

  Align(&buf, 4);

  return connection_->SendRequest<RandR::GetCrtcGammaSizeReply>(
      &buf, "RandR::GetCrtcGammaSize", false);
}

Future<RandR::GetCrtcGammaSizeReply> RandR::GetCrtcGammaSize(const Crtc& crtc) {
  return RandR::GetCrtcGammaSize(RandR::GetCrtcGammaSizeRequest{crtc});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<RandR::GetCrtcGammaSizeReply> detail::ReadReply<
    RandR::GetCrtcGammaSizeReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<RandR::GetCrtcGammaSizeReply>();

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
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<RandR::GetCrtcGammaReply> RandR::GetCrtcGamma(
    const RandR::GetCrtcGammaRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& crtc = request.crtc;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 23;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // crtc
  buf.Write(&crtc);

  Align(&buf, 4);

  return connection_->SendRequest<RandR::GetCrtcGammaReply>(
      &buf, "RandR::GetCrtcGamma", false);
}

Future<RandR::GetCrtcGammaReply> RandR::GetCrtcGamma(const Crtc& crtc) {
  return RandR::GetCrtcGamma(RandR::GetCrtcGammaRequest{crtc});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<RandR::GetCrtcGammaReply> detail::ReadReply<
    RandR::GetCrtcGammaReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<RandR::GetCrtcGammaReply>();

  auto& sequence = (*reply).sequence;
  uint16_t size{};
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

  // size
  Read(&size, &buf);

  // pad1
  Pad(&buf, 22);

  // red
  red.resize(size);
  for (auto& red_elem : red) {
    // red_elem
    Read(&red_elem, &buf);
  }

  // green
  green.resize(size);
  for (auto& green_elem : green) {
    // green_elem
    Read(&green_elem, &buf);
  }

  // blue
  blue.resize(size);
  for (auto& blue_elem : blue) {
    // blue_elem
    Read(&blue_elem, &buf);
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> RandR::SetCrtcGamma(const RandR::SetCrtcGammaRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& crtc = request.crtc;
  uint16_t size{};
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
  uint8_t minor_opcode = 24;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // crtc
  buf.Write(&crtc);

  // size
  size = blue.size();
  buf.Write(&size);

  // pad0
  Pad(&buf, 2);

  // red
  CHECK_EQ(static_cast<size_t>(size), red.size());
  for (auto& red_elem : red) {
    // red_elem
    buf.Write(&red_elem);
  }

  // green
  CHECK_EQ(static_cast<size_t>(size), green.size());
  for (auto& green_elem : green) {
    // green_elem
    buf.Write(&green_elem);
  }

  // blue
  CHECK_EQ(static_cast<size_t>(size), blue.size());
  for (auto& blue_elem : blue) {
    // blue_elem
    buf.Write(&blue_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "RandR::SetCrtcGamma", false);
}

Future<void> RandR::SetCrtcGamma(const Crtc& crtc,
                                 const std::vector<uint16_t>& red,
                                 const std::vector<uint16_t>& green,
                                 const std::vector<uint16_t>& blue) {
  return RandR::SetCrtcGamma(
      RandR::SetCrtcGammaRequest{crtc, red, green, blue});
}

Future<RandR::GetScreenResourcesCurrentReply> RandR::GetScreenResourcesCurrent(
    const RandR::GetScreenResourcesCurrentRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& window = request.window;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 25;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // window
  buf.Write(&window);

  Align(&buf, 4);

  return connection_->SendRequest<RandR::GetScreenResourcesCurrentReply>(
      &buf, "RandR::GetScreenResourcesCurrent", false);
}

Future<RandR::GetScreenResourcesCurrentReply> RandR::GetScreenResourcesCurrent(
    const Window& window) {
  return RandR::GetScreenResourcesCurrent(
      RandR::GetScreenResourcesCurrentRequest{window});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<RandR::GetScreenResourcesCurrentReply> detail::ReadReply<
    RandR::GetScreenResourcesCurrentReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<RandR::GetScreenResourcesCurrentReply>();

  auto& sequence = (*reply).sequence;
  auto& timestamp = (*reply).timestamp;
  auto& config_timestamp = (*reply).config_timestamp;
  uint16_t num_crtcs{};
  uint16_t num_outputs{};
  uint16_t num_modes{};
  uint16_t names_len{};
  auto& crtcs = (*reply).crtcs;
  auto& outputs = (*reply).outputs;
  auto& modes = (*reply).modes;
  auto& names = (*reply).names;

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

  // timestamp
  Read(&timestamp, &buf);

  // config_timestamp
  Read(&config_timestamp, &buf);

  // num_crtcs
  Read(&num_crtcs, &buf);

  // num_outputs
  Read(&num_outputs, &buf);

  // num_modes
  Read(&num_modes, &buf);

  // names_len
  Read(&names_len, &buf);

  // pad1
  Pad(&buf, 8);

  // crtcs
  crtcs.resize(num_crtcs);
  for (auto& crtcs_elem : crtcs) {
    // crtcs_elem
    Read(&crtcs_elem, &buf);
  }

  // outputs
  outputs.resize(num_outputs);
  for (auto& outputs_elem : outputs) {
    // outputs_elem
    Read(&outputs_elem, &buf);
  }

  // modes
  modes.resize(num_modes);
  for (auto& modes_elem : modes) {
    // modes_elem
    {
      auto& id = modes_elem.id;
      auto& width = modes_elem.width;
      auto& height = modes_elem.height;
      auto& dot_clock = modes_elem.dot_clock;
      auto& hsync_start = modes_elem.hsync_start;
      auto& hsync_end = modes_elem.hsync_end;
      auto& htotal = modes_elem.htotal;
      auto& hskew = modes_elem.hskew;
      auto& vsync_start = modes_elem.vsync_start;
      auto& vsync_end = modes_elem.vsync_end;
      auto& vtotal = modes_elem.vtotal;
      auto& name_len = modes_elem.name_len;
      auto& mode_flags = modes_elem.mode_flags;

      // id
      Read(&id, &buf);

      // width
      Read(&width, &buf);

      // height
      Read(&height, &buf);

      // dot_clock
      Read(&dot_clock, &buf);

      // hsync_start
      Read(&hsync_start, &buf);

      // hsync_end
      Read(&hsync_end, &buf);

      // htotal
      Read(&htotal, &buf);

      // hskew
      Read(&hskew, &buf);

      // vsync_start
      Read(&vsync_start, &buf);

      // vsync_end
      Read(&vsync_end, &buf);

      // vtotal
      Read(&vtotal, &buf);

      // name_len
      Read(&name_len, &buf);

      // mode_flags
      uint32_t tmp25;
      Read(&tmp25, &buf);
      mode_flags = static_cast<RandR::ModeFlag>(tmp25);
    }
  }

  // names
  names.resize(names_len);
  for (auto& names_elem : names) {
    // names_elem
    Read(&names_elem, &buf);
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> RandR::SetCrtcTransform(
    const RandR::SetCrtcTransformRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& crtc = request.crtc;
  auto& transform = request.transform;
  uint16_t filter_len{};
  auto& filter_name = request.filter_name;
  size_t filter_name_len = filter_name.size();
  auto& filter_params = request.filter_params;
  size_t filter_params_len = filter_params.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 26;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // crtc
  buf.Write(&crtc);

  // transform
  {
    auto& matrix11 = transform.matrix11;
    auto& matrix12 = transform.matrix12;
    auto& matrix13 = transform.matrix13;
    auto& matrix21 = transform.matrix21;
    auto& matrix22 = transform.matrix22;
    auto& matrix23 = transform.matrix23;
    auto& matrix31 = transform.matrix31;
    auto& matrix32 = transform.matrix32;
    auto& matrix33 = transform.matrix33;

    // matrix11
    buf.Write(&matrix11);

    // matrix12
    buf.Write(&matrix12);

    // matrix13
    buf.Write(&matrix13);

    // matrix21
    buf.Write(&matrix21);

    // matrix22
    buf.Write(&matrix22);

    // matrix23
    buf.Write(&matrix23);

    // matrix31
    buf.Write(&matrix31);

    // matrix32
    buf.Write(&matrix32);

    // matrix33
    buf.Write(&matrix33);
  }

  // filter_len
  filter_len = filter_name.size();
  buf.Write(&filter_len);

  // pad0
  Pad(&buf, 2);

  // filter_name
  CHECK_EQ(static_cast<size_t>(filter_len), filter_name.size());
  for (auto& filter_name_elem : filter_name) {
    // filter_name_elem
    buf.Write(&filter_name_elem);
  }

  // pad1
  Align(&buf, 4);

  // filter_params
  CHECK_EQ(static_cast<size_t>(filter_params_len), filter_params.size());
  for (auto& filter_params_elem : filter_params) {
    // filter_params_elem
    buf.Write(&filter_params_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "RandR::SetCrtcTransform", false);
}

Future<void> RandR::SetCrtcTransform(
    const Crtc& crtc,
    const Render::Transform& transform,
    const std::string& filter_name,
    const std::vector<Render::Fixed>& filter_params) {
  return RandR::SetCrtcTransform(RandR::SetCrtcTransformRequest{
      crtc, transform, filter_name, filter_params});
}

Future<RandR::GetCrtcTransformReply> RandR::GetCrtcTransform(
    const RandR::GetCrtcTransformRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& crtc = request.crtc;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 27;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // crtc
  buf.Write(&crtc);

  Align(&buf, 4);

  return connection_->SendRequest<RandR::GetCrtcTransformReply>(
      &buf, "RandR::GetCrtcTransform", false);
}

Future<RandR::GetCrtcTransformReply> RandR::GetCrtcTransform(const Crtc& crtc) {
  return RandR::GetCrtcTransform(RandR::GetCrtcTransformRequest{crtc});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<RandR::GetCrtcTransformReply> detail::ReadReply<
    RandR::GetCrtcTransformReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<RandR::GetCrtcTransformReply>();

  auto& sequence = (*reply).sequence;
  auto& pending_transform = (*reply).pending_transform;
  auto& has_transforms = (*reply).has_transforms;
  auto& current_transform = (*reply).current_transform;
  uint16_t pending_len{};
  uint16_t pending_nparams{};
  uint16_t current_len{};
  uint16_t current_nparams{};
  auto& pending_filter_name = (*reply).pending_filter_name;
  auto& pending_params = (*reply).pending_params;
  auto& current_filter_name = (*reply).current_filter_name;
  auto& current_params = (*reply).current_params;

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

  // pending_transform
  {
    auto& matrix11 = pending_transform.matrix11;
    auto& matrix12 = pending_transform.matrix12;
    auto& matrix13 = pending_transform.matrix13;
    auto& matrix21 = pending_transform.matrix21;
    auto& matrix22 = pending_transform.matrix22;
    auto& matrix23 = pending_transform.matrix23;
    auto& matrix31 = pending_transform.matrix31;
    auto& matrix32 = pending_transform.matrix32;
    auto& matrix33 = pending_transform.matrix33;

    // matrix11
    Read(&matrix11, &buf);

    // matrix12
    Read(&matrix12, &buf);

    // matrix13
    Read(&matrix13, &buf);

    // matrix21
    Read(&matrix21, &buf);

    // matrix22
    Read(&matrix22, &buf);

    // matrix23
    Read(&matrix23, &buf);

    // matrix31
    Read(&matrix31, &buf);

    // matrix32
    Read(&matrix32, &buf);

    // matrix33
    Read(&matrix33, &buf);
  }

  // has_transforms
  Read(&has_transforms, &buf);

  // pad1
  Pad(&buf, 3);

  // current_transform
  {
    auto& matrix11 = current_transform.matrix11;
    auto& matrix12 = current_transform.matrix12;
    auto& matrix13 = current_transform.matrix13;
    auto& matrix21 = current_transform.matrix21;
    auto& matrix22 = current_transform.matrix22;
    auto& matrix23 = current_transform.matrix23;
    auto& matrix31 = current_transform.matrix31;
    auto& matrix32 = current_transform.matrix32;
    auto& matrix33 = current_transform.matrix33;

    // matrix11
    Read(&matrix11, &buf);

    // matrix12
    Read(&matrix12, &buf);

    // matrix13
    Read(&matrix13, &buf);

    // matrix21
    Read(&matrix21, &buf);

    // matrix22
    Read(&matrix22, &buf);

    // matrix23
    Read(&matrix23, &buf);

    // matrix31
    Read(&matrix31, &buf);

    // matrix32
    Read(&matrix32, &buf);

    // matrix33
    Read(&matrix33, &buf);
  }

  // pad2
  Pad(&buf, 4);

  // pending_len
  Read(&pending_len, &buf);

  // pending_nparams
  Read(&pending_nparams, &buf);

  // current_len
  Read(&current_len, &buf);

  // current_nparams
  Read(&current_nparams, &buf);

  // pending_filter_name
  pending_filter_name.resize(pending_len);
  for (auto& pending_filter_name_elem : pending_filter_name) {
    // pending_filter_name_elem
    Read(&pending_filter_name_elem, &buf);
  }

  // pad3
  Align(&buf, 4);

  // pending_params
  pending_params.resize(pending_nparams);
  for (auto& pending_params_elem : pending_params) {
    // pending_params_elem
    Read(&pending_params_elem, &buf);
  }

  // current_filter_name
  current_filter_name.resize(current_len);
  for (auto& current_filter_name_elem : current_filter_name) {
    // current_filter_name_elem
    Read(&current_filter_name_elem, &buf);
  }

  // pad4
  Align(&buf, 4);

  // current_params
  current_params.resize(current_nparams);
  for (auto& current_params_elem : current_params) {
    // current_params_elem
    Read(&current_params_elem, &buf);
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<RandR::GetPanningReply> RandR::GetPanning(
    const RandR::GetPanningRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& crtc = request.crtc;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 28;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // crtc
  buf.Write(&crtc);

  Align(&buf, 4);

  return connection_->SendRequest<RandR::GetPanningReply>(
      &buf, "RandR::GetPanning", false);
}

Future<RandR::GetPanningReply> RandR::GetPanning(const Crtc& crtc) {
  return RandR::GetPanning(RandR::GetPanningRequest{crtc});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<RandR::GetPanningReply> detail::ReadReply<
    RandR::GetPanningReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<RandR::GetPanningReply>();

  auto& status = (*reply).status;
  auto& sequence = (*reply).sequence;
  auto& timestamp = (*reply).timestamp;
  auto& left = (*reply).left;
  auto& top = (*reply).top;
  auto& width = (*reply).width;
  auto& height = (*reply).height;
  auto& track_left = (*reply).track_left;
  auto& track_top = (*reply).track_top;
  auto& track_width = (*reply).track_width;
  auto& track_height = (*reply).track_height;
  auto& border_left = (*reply).border_left;
  auto& border_top = (*reply).border_top;
  auto& border_right = (*reply).border_right;
  auto& border_bottom = (*reply).border_bottom;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // status
  uint8_t tmp26;
  Read(&tmp26, &buf);
  status = static_cast<RandR::SetConfig>(tmp26);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // timestamp
  Read(&timestamp, &buf);

  // left
  Read(&left, &buf);

  // top
  Read(&top, &buf);

  // width
  Read(&width, &buf);

  // height
  Read(&height, &buf);

  // track_left
  Read(&track_left, &buf);

  // track_top
  Read(&track_top, &buf);

  // track_width
  Read(&track_width, &buf);

  // track_height
  Read(&track_height, &buf);

  // border_left
  Read(&border_left, &buf);

  // border_top
  Read(&border_top, &buf);

  // border_right
  Read(&border_right, &buf);

  // border_bottom
  Read(&border_bottom, &buf);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<RandR::SetPanningReply> RandR::SetPanning(
    const RandR::SetPanningRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& crtc = request.crtc;
  auto& timestamp = request.timestamp;
  auto& left = request.left;
  auto& top = request.top;
  auto& width = request.width;
  auto& height = request.height;
  auto& track_left = request.track_left;
  auto& track_top = request.track_top;
  auto& track_width = request.track_width;
  auto& track_height = request.track_height;
  auto& border_left = request.border_left;
  auto& border_top = request.border_top;
  auto& border_right = request.border_right;
  auto& border_bottom = request.border_bottom;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 29;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // crtc
  buf.Write(&crtc);

  // timestamp
  buf.Write(&timestamp);

  // left
  buf.Write(&left);

  // top
  buf.Write(&top);

  // width
  buf.Write(&width);

  // height
  buf.Write(&height);

  // track_left
  buf.Write(&track_left);

  // track_top
  buf.Write(&track_top);

  // track_width
  buf.Write(&track_width);

  // track_height
  buf.Write(&track_height);

  // border_left
  buf.Write(&border_left);

  // border_top
  buf.Write(&border_top);

  // border_right
  buf.Write(&border_right);

  // border_bottom
  buf.Write(&border_bottom);

  Align(&buf, 4);

  return connection_->SendRequest<RandR::SetPanningReply>(
      &buf, "RandR::SetPanning", false);
}

Future<RandR::SetPanningReply> RandR::SetPanning(const Crtc& crtc,
                                                 const Time& timestamp,
                                                 const uint16_t& left,
                                                 const uint16_t& top,
                                                 const uint16_t& width,
                                                 const uint16_t& height,
                                                 const uint16_t& track_left,
                                                 const uint16_t& track_top,
                                                 const uint16_t& track_width,
                                                 const uint16_t& track_height,
                                                 const int16_t& border_left,
                                                 const int16_t& border_top,
                                                 const int16_t& border_right,
                                                 const int16_t& border_bottom) {
  return RandR::SetPanning(RandR::SetPanningRequest{
      crtc, timestamp, left, top, width, height, track_left, track_top,
      track_width, track_height, border_left, border_top, border_right,
      border_bottom});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<RandR::SetPanningReply> detail::ReadReply<
    RandR::SetPanningReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<RandR::SetPanningReply>();

  auto& status = (*reply).status;
  auto& sequence = (*reply).sequence;
  auto& timestamp = (*reply).timestamp;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // status
  uint8_t tmp27;
  Read(&tmp27, &buf);
  status = static_cast<RandR::SetConfig>(tmp27);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // timestamp
  Read(&timestamp, &buf);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> RandR::SetOutputPrimary(
    const RandR::SetOutputPrimaryRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& window = request.window;
  auto& output = request.output;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 30;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // window
  buf.Write(&window);

  // output
  buf.Write(&output);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "RandR::SetOutputPrimary", false);
}

Future<void> RandR::SetOutputPrimary(const Window& window,
                                     const Output& output) {
  return RandR::SetOutputPrimary(
      RandR::SetOutputPrimaryRequest{window, output});
}

Future<RandR::GetOutputPrimaryReply> RandR::GetOutputPrimary(
    const RandR::GetOutputPrimaryRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& window = request.window;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 31;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // window
  buf.Write(&window);

  Align(&buf, 4);

  return connection_->SendRequest<RandR::GetOutputPrimaryReply>(
      &buf, "RandR::GetOutputPrimary", false);
}

Future<RandR::GetOutputPrimaryReply> RandR::GetOutputPrimary(
    const Window& window) {
  return RandR::GetOutputPrimary(RandR::GetOutputPrimaryRequest{window});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<RandR::GetOutputPrimaryReply> detail::ReadReply<
    RandR::GetOutputPrimaryReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<RandR::GetOutputPrimaryReply>();

  auto& sequence = (*reply).sequence;
  auto& output = (*reply).output;

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

  // output
  Read(&output, &buf);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<RandR::GetProvidersReply> RandR::GetProviders(
    const RandR::GetProvidersRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& window = request.window;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 32;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // window
  buf.Write(&window);

  Align(&buf, 4);

  return connection_->SendRequest<RandR::GetProvidersReply>(
      &buf, "RandR::GetProviders", false);
}

Future<RandR::GetProvidersReply> RandR::GetProviders(const Window& window) {
  return RandR::GetProviders(RandR::GetProvidersRequest{window});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<RandR::GetProvidersReply> detail::ReadReply<
    RandR::GetProvidersReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<RandR::GetProvidersReply>();

  auto& sequence = (*reply).sequence;
  auto& timestamp = (*reply).timestamp;
  uint16_t num_providers{};
  auto& providers = (*reply).providers;

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

  // timestamp
  Read(&timestamp, &buf);

  // num_providers
  Read(&num_providers, &buf);

  // pad1
  Pad(&buf, 18);

  // providers
  providers.resize(num_providers);
  for (auto& providers_elem : providers) {
    // providers_elem
    Read(&providers_elem, &buf);
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<RandR::GetProviderInfoReply> RandR::GetProviderInfo(
    const RandR::GetProviderInfoRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& provider = request.provider;
  auto& config_timestamp = request.config_timestamp;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 33;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // provider
  buf.Write(&provider);

  // config_timestamp
  buf.Write(&config_timestamp);

  Align(&buf, 4);

  return connection_->SendRequest<RandR::GetProviderInfoReply>(
      &buf, "RandR::GetProviderInfo", false);
}

Future<RandR::GetProviderInfoReply> RandR::GetProviderInfo(
    const Provider& provider,
    const Time& config_timestamp) {
  return RandR::GetProviderInfo(
      RandR::GetProviderInfoRequest{provider, config_timestamp});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<RandR::GetProviderInfoReply> detail::ReadReply<
    RandR::GetProviderInfoReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<RandR::GetProviderInfoReply>();

  auto& status = (*reply).status;
  auto& sequence = (*reply).sequence;
  auto& timestamp = (*reply).timestamp;
  auto& capabilities = (*reply).capabilities;
  uint16_t num_crtcs{};
  uint16_t num_outputs{};
  uint16_t num_associated_providers{};
  uint16_t name_len{};
  auto& crtcs = (*reply).crtcs;
  auto& outputs = (*reply).outputs;
  auto& associated_providers = (*reply).associated_providers;
  auto& associated_capability = (*reply).associated_capability;
  auto& name = (*reply).name;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // status
  Read(&status, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // timestamp
  Read(&timestamp, &buf);

  // capabilities
  uint32_t tmp28;
  Read(&tmp28, &buf);
  capabilities = static_cast<RandR::ProviderCapability>(tmp28);

  // num_crtcs
  Read(&num_crtcs, &buf);

  // num_outputs
  Read(&num_outputs, &buf);

  // num_associated_providers
  Read(&num_associated_providers, &buf);

  // name_len
  Read(&name_len, &buf);

  // pad0
  Pad(&buf, 8);

  // crtcs
  crtcs.resize(num_crtcs);
  for (auto& crtcs_elem : crtcs) {
    // crtcs_elem
    Read(&crtcs_elem, &buf);
  }

  // outputs
  outputs.resize(num_outputs);
  for (auto& outputs_elem : outputs) {
    // outputs_elem
    Read(&outputs_elem, &buf);
  }

  // associated_providers
  associated_providers.resize(num_associated_providers);
  for (auto& associated_providers_elem : associated_providers) {
    // associated_providers_elem
    Read(&associated_providers_elem, &buf);
  }

  // associated_capability
  associated_capability.resize(num_associated_providers);
  for (auto& associated_capability_elem : associated_capability) {
    // associated_capability_elem
    Read(&associated_capability_elem, &buf);
  }

  // name
  name.resize(name_len);
  for (auto& name_elem : name) {
    // name_elem
    Read(&name_elem, &buf);
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> RandR::SetProviderOffloadSink(
    const RandR::SetProviderOffloadSinkRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& provider = request.provider;
  auto& sink_provider = request.sink_provider;
  auto& config_timestamp = request.config_timestamp;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 34;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // provider
  buf.Write(&provider);

  // sink_provider
  buf.Write(&sink_provider);

  // config_timestamp
  buf.Write(&config_timestamp);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "RandR::SetProviderOffloadSink",
                                        false);
}

Future<void> RandR::SetProviderOffloadSink(const Provider& provider,
                                           const Provider& sink_provider,
                                           const Time& config_timestamp) {
  return RandR::SetProviderOffloadSink(RandR::SetProviderOffloadSinkRequest{
      provider, sink_provider, config_timestamp});
}

Future<void> RandR::SetProviderOutputSource(
    const RandR::SetProviderOutputSourceRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& provider = request.provider;
  auto& source_provider = request.source_provider;
  auto& config_timestamp = request.config_timestamp;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 35;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // provider
  buf.Write(&provider);

  // source_provider
  buf.Write(&source_provider);

  // config_timestamp
  buf.Write(&config_timestamp);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "RandR::SetProviderOutputSource",
                                        false);
}

Future<void> RandR::SetProviderOutputSource(const Provider& provider,
                                            const Provider& source_provider,
                                            const Time& config_timestamp) {
  return RandR::SetProviderOutputSource(RandR::SetProviderOutputSourceRequest{
      provider, source_provider, config_timestamp});
}

Future<RandR::ListProviderPropertiesReply> RandR::ListProviderProperties(
    const RandR::ListProviderPropertiesRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& provider = request.provider;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 36;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // provider
  buf.Write(&provider);

  Align(&buf, 4);

  return connection_->SendRequest<RandR::ListProviderPropertiesReply>(
      &buf, "RandR::ListProviderProperties", false);
}

Future<RandR::ListProviderPropertiesReply> RandR::ListProviderProperties(
    const Provider& provider) {
  return RandR::ListProviderProperties(
      RandR::ListProviderPropertiesRequest{provider});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<RandR::ListProviderPropertiesReply> detail::ReadReply<
    RandR::ListProviderPropertiesReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<RandR::ListProviderPropertiesReply>();

  auto& sequence = (*reply).sequence;
  uint16_t num_atoms{};
  auto& atoms = (*reply).atoms;

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

  // num_atoms
  Read(&num_atoms, &buf);

  // pad1
  Pad(&buf, 22);

  // atoms
  atoms.resize(num_atoms);
  for (auto& atoms_elem : atoms) {
    // atoms_elem
    Read(&atoms_elem, &buf);
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<RandR::QueryProviderPropertyReply> RandR::QueryProviderProperty(
    const RandR::QueryProviderPropertyRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& provider = request.provider;
  auto& property = request.property;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 37;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // provider
  buf.Write(&provider);

  // property
  buf.Write(&property);

  Align(&buf, 4);

  return connection_->SendRequest<RandR::QueryProviderPropertyReply>(
      &buf, "RandR::QueryProviderProperty", false);
}

Future<RandR::QueryProviderPropertyReply> RandR::QueryProviderProperty(
    const Provider& provider,
    const Atom& property) {
  return RandR::QueryProviderProperty(
      RandR::QueryProviderPropertyRequest{provider, property});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<RandR::QueryProviderPropertyReply> detail::ReadReply<
    RandR::QueryProviderPropertyReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<RandR::QueryProviderPropertyReply>();

  auto& sequence = (*reply).sequence;
  auto& pending = (*reply).pending;
  auto& range = (*reply).range;
  auto& immutable = (*reply).immutable;
  auto& valid_values = (*reply).valid_values;

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

  // pending
  Read(&pending, &buf);

  // range
  Read(&range, &buf);

  // immutable
  Read(&immutable, &buf);

  // pad1
  Pad(&buf, 21);

  // valid_values
  valid_values.resize(length);
  for (auto& valid_values_elem : valid_values) {
    // valid_values_elem
    Read(&valid_values_elem, &buf);
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> RandR::ConfigureProviderProperty(
    const RandR::ConfigureProviderPropertyRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& provider = request.provider;
  auto& property = request.property;
  auto& pending = request.pending;
  auto& range = request.range;
  auto& values = request.values;
  size_t values_len = values.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 38;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // provider
  buf.Write(&provider);

  // property
  buf.Write(&property);

  // pending
  buf.Write(&pending);

  // range
  buf.Write(&range);

  // pad0
  Pad(&buf, 2);

  // values
  CHECK_EQ(static_cast<size_t>(values_len), values.size());
  for (auto& values_elem : values) {
    // values_elem
    buf.Write(&values_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(
      &buf, "RandR::ConfigureProviderProperty", false);
}

Future<void> RandR::ConfigureProviderProperty(
    const Provider& provider,
    const Atom& property,
    const uint8_t& pending,
    const uint8_t& range,
    const std::vector<int32_t>& values) {
  return RandR::ConfigureProviderProperty(
      RandR::ConfigureProviderPropertyRequest{provider, property, pending,
                                              range, values});
}

Future<void> RandR::ChangeProviderProperty(
    const RandR::ChangeProviderPropertyRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& provider = request.provider;
  auto& property = request.property;
  auto& type = request.type;
  auto& format = request.format;
  auto& mode = request.mode;
  auto& num_items = request.num_items;
  auto& data = request.data;
  size_t data_len = data ? data->size() : 0;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 39;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // provider
  buf.Write(&provider);

  // property
  buf.Write(&property);

  // type
  buf.Write(&type);

  // format
  buf.Write(&format);

  // mode
  buf.Write(&mode);

  // pad0
  Pad(&buf, 2);

  // num_items
  buf.Write(&num_items);

  // data
  buf.AppendSizedBuffer(data);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "RandR::ChangeProviderProperty",
                                        false);
}

Future<void> RandR::ChangeProviderProperty(
    const Provider& provider,
    const Atom& property,
    const Atom& type,
    const uint8_t& format,
    const uint8_t& mode,
    const uint32_t& num_items,
    const scoped_refptr<base::RefCountedMemory>& data) {
  return RandR::ChangeProviderProperty(RandR::ChangeProviderPropertyRequest{
      provider, property, type, format, mode, num_items, data});
}

Future<void> RandR::DeleteProviderProperty(
    const RandR::DeleteProviderPropertyRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& provider = request.provider;
  auto& property = request.property;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 40;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // provider
  buf.Write(&provider);

  // property
  buf.Write(&property);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "RandR::DeleteProviderProperty",
                                        false);
}

Future<void> RandR::DeleteProviderProperty(const Provider& provider,
                                           const Atom& property) {
  return RandR::DeleteProviderProperty(
      RandR::DeleteProviderPropertyRequest{provider, property});
}

Future<RandR::GetProviderPropertyReply> RandR::GetProviderProperty(
    const RandR::GetProviderPropertyRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& provider = request.provider;
  auto& property = request.property;
  auto& type = request.type;
  auto& long_offset = request.long_offset;
  auto& long_length = request.long_length;
  auto& c_delete = request.c_delete;
  auto& pending = request.pending;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 41;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // provider
  buf.Write(&provider);

  // property
  buf.Write(&property);

  // type
  buf.Write(&type);

  // long_offset
  buf.Write(&long_offset);

  // long_length
  buf.Write(&long_length);

  // c_delete
  buf.Write(&c_delete);

  // pending
  buf.Write(&pending);

  // pad0
  Pad(&buf, 2);

  Align(&buf, 4);

  return connection_->SendRequest<RandR::GetProviderPropertyReply>(
      &buf, "RandR::GetProviderProperty", false);
}

Future<RandR::GetProviderPropertyReply> RandR::GetProviderProperty(
    const Provider& provider,
    const Atom& property,
    const Atom& type,
    const uint32_t& long_offset,
    const uint32_t& long_length,
    const uint8_t& c_delete,
    const uint8_t& pending) {
  return RandR::GetProviderProperty(RandR::GetProviderPropertyRequest{
      provider, property, type, long_offset, long_length, c_delete, pending});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<RandR::GetProviderPropertyReply> detail::ReadReply<
    RandR::GetProviderPropertyReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<RandR::GetProviderPropertyReply>();

  auto& format = (*reply).format;
  auto& sequence = (*reply).sequence;
  auto& type = (*reply).type;
  auto& bytes_after = (*reply).bytes_after;
  auto& num_items = (*reply).num_items;
  auto& data = (*reply).data;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // format
  Read(&format, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // type
  Read(&type, &buf);

  // bytes_after
  Read(&bytes_after, &buf);

  // num_items
  Read(&num_items, &buf);

  // pad0
  Pad(&buf, 12);

  // data
  data = buffer->ReadAndAdvance((num_items) * ((format) / (8)));

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<RandR::GetMonitorsReply> RandR::GetMonitors(
    const RandR::GetMonitorsRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& window = request.window;
  auto& get_active = request.get_active;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 42;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // window
  buf.Write(&window);

  // get_active
  buf.Write(&get_active);

  Align(&buf, 4);

  return connection_->SendRequest<RandR::GetMonitorsReply>(
      &buf, "RandR::GetMonitors", false);
}

Future<RandR::GetMonitorsReply> RandR::GetMonitors(const Window& window,
                                                   const uint8_t& get_active) {
  return RandR::GetMonitors(RandR::GetMonitorsRequest{window, get_active});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<RandR::GetMonitorsReply> detail::ReadReply<
    RandR::GetMonitorsReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<RandR::GetMonitorsReply>();

  auto& sequence = (*reply).sequence;
  auto& timestamp = (*reply).timestamp;
  uint32_t nMonitors{};
  auto& nOutputs = (*reply).nOutputs;
  auto& monitors = (*reply).monitors;

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

  // timestamp
  Read(&timestamp, &buf);

  // nMonitors
  Read(&nMonitors, &buf);

  // nOutputs
  Read(&nOutputs, &buf);

  // pad1
  Pad(&buf, 12);

  // monitors
  monitors.resize(nMonitors);
  for (auto& monitors_elem : monitors) {
    // monitors_elem
    {
      auto& name = monitors_elem.name;
      auto& primary = monitors_elem.primary;
      auto& automatic = monitors_elem.automatic;
      uint16_t nOutput{};
      auto& x = monitors_elem.x;
      auto& y = monitors_elem.y;
      auto& width = monitors_elem.width;
      auto& height = monitors_elem.height;
      auto& width_in_millimeters = monitors_elem.width_in_millimeters;
      auto& height_in_millimeters = monitors_elem.height_in_millimeters;
      auto& outputs = monitors_elem.outputs;

      // name
      Read(&name, &buf);

      // primary
      Read(&primary, &buf);

      // automatic
      Read(&automatic, &buf);

      // nOutput
      Read(&nOutput, &buf);

      // x
      Read(&x, &buf);

      // y
      Read(&y, &buf);

      // width
      Read(&width, &buf);

      // height
      Read(&height, &buf);

      // width_in_millimeters
      Read(&width_in_millimeters, &buf);

      // height_in_millimeters
      Read(&height_in_millimeters, &buf);

      // outputs
      outputs.resize(nOutput);
      for (auto& outputs_elem : outputs) {
        // outputs_elem
        Read(&outputs_elem, &buf);
      }
    }
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> RandR::SetMonitor(const RandR::SetMonitorRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& window = request.window;
  auto& monitorinfo = request.monitorinfo;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 43;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // window
  buf.Write(&window);

  // monitorinfo
  {
    auto& name = monitorinfo.name;
    auto& primary = monitorinfo.primary;
    auto& automatic = monitorinfo.automatic;
    uint16_t nOutput{};
    auto& x = monitorinfo.x;
    auto& y = monitorinfo.y;
    auto& width = monitorinfo.width;
    auto& height = monitorinfo.height;
    auto& width_in_millimeters = monitorinfo.width_in_millimeters;
    auto& height_in_millimeters = monitorinfo.height_in_millimeters;
    auto& outputs = monitorinfo.outputs;

    // name
    buf.Write(&name);

    // primary
    buf.Write(&primary);

    // automatic
    buf.Write(&automatic);

    // nOutput
    nOutput = outputs.size();
    buf.Write(&nOutput);

    // x
    buf.Write(&x);

    // y
    buf.Write(&y);

    // width
    buf.Write(&width);

    // height
    buf.Write(&height);

    // width_in_millimeters
    buf.Write(&width_in_millimeters);

    // height_in_millimeters
    buf.Write(&height_in_millimeters);

    // outputs
    CHECK_EQ(static_cast<size_t>(nOutput), outputs.size());
    for (auto& outputs_elem : outputs) {
      // outputs_elem
      buf.Write(&outputs_elem);
    }
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "RandR::SetMonitor", false);
}

Future<void> RandR::SetMonitor(const Window& window,
                               const MonitorInfo& monitorinfo) {
  return RandR::SetMonitor(RandR::SetMonitorRequest{window, monitorinfo});
}

Future<void> RandR::DeleteMonitor(const RandR::DeleteMonitorRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& window = request.window;
  auto& name = request.name;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 44;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // window
  buf.Write(&window);

  // name
  buf.Write(&name);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "RandR::DeleteMonitor", false);
}

Future<void> RandR::DeleteMonitor(const Window& window, const Atom& name) {
  return RandR::DeleteMonitor(RandR::DeleteMonitorRequest{window, name});
}

Future<RandR::CreateLeaseReply> RandR::CreateLease(
    const RandR::CreateLeaseRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& window = request.window;
  auto& lid = request.lid;
  uint16_t num_crtcs{};
  uint16_t num_outputs{};
  auto& crtcs = request.crtcs;
  size_t crtcs_len = crtcs.size();
  auto& outputs = request.outputs;
  size_t outputs_len = outputs.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 45;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // window
  buf.Write(&window);

  // lid
  buf.Write(&lid);

  // num_crtcs
  num_crtcs = crtcs.size();
  buf.Write(&num_crtcs);

  // num_outputs
  num_outputs = outputs.size();
  buf.Write(&num_outputs);

  // crtcs
  CHECK_EQ(static_cast<size_t>(num_crtcs), crtcs.size());
  for (auto& crtcs_elem : crtcs) {
    // crtcs_elem
    buf.Write(&crtcs_elem);
  }

  // outputs
  CHECK_EQ(static_cast<size_t>(num_outputs), outputs.size());
  for (auto& outputs_elem : outputs) {
    // outputs_elem
    buf.Write(&outputs_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<RandR::CreateLeaseReply>(
      &buf, "RandR::CreateLease", true);
}

Future<RandR::CreateLeaseReply> RandR::CreateLease(
    const Window& window,
    const Lease& lid,
    const std::vector<Crtc>& crtcs,
    const std::vector<Output>& outputs) {
  return RandR::CreateLease(
      RandR::CreateLeaseRequest{window, lid, crtcs, outputs});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<RandR::CreateLeaseReply> detail::ReadReply<
    RandR::CreateLeaseReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<RandR::CreateLeaseReply>();

  auto& nfd = (*reply).nfd;
  auto& sequence = (*reply).sequence;
  auto& master_fd = (*reply).master_fd;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // nfd
  Read(&nfd, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // master_fd
  master_fd = RefCountedFD(buf.TakeFd());

  // pad0
  Pad(&buf, 24);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> RandR::FreeLease(const RandR::FreeLeaseRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& lid = request.lid;
  auto& terminate = request.terminate;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 46;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // lid
  buf.Write(&lid);

  // terminate
  buf.Write(&terminate);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "RandR::FreeLease", false);
}

Future<void> RandR::FreeLease(const Lease& lid, const uint8_t& terminate) {
  return RandR::FreeLease(RandR::FreeLeaseRequest{lid, terminate});
}

}  // namespace x11
