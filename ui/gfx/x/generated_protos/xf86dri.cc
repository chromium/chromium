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

#include "xf86dri.h"

#include <unistd.h>
#include <xcb/xcb.h>
#include <xcb/xcbext.h>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "ui/gfx/x/xproto_internal.h"

namespace x11 {

XF86Dri::XF86Dri(Connection* connection, const x11::QueryExtensionReply& info)
    : connection_(connection), info_(info) {}

Future<XF86Dri::QueryVersionReply> XF86Dri::QueryVersion(
    const XF86Dri::QueryVersionRequest& request) {
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

  return connection_->SendRequest<XF86Dri::QueryVersionReply>(
      &buf, "XF86Dri::QueryVersion", false);
}

Future<XF86Dri::QueryVersionReply> XF86Dri::QueryVersion() {
  return XF86Dri::QueryVersion(XF86Dri::QueryVersionRequest{});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<XF86Dri::QueryVersionReply> detail::ReadReply<
    XF86Dri::QueryVersionReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<XF86Dri::QueryVersionReply>();

  auto& sequence = (*reply).sequence;
  auto& dri_major_version = (*reply).dri_major_version;
  auto& dri_minor_version = (*reply).dri_minor_version;
  auto& dri_minor_patch = (*reply).dri_minor_patch;

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

  // dri_major_version
  Read(&dri_major_version, &buf);

  // dri_minor_version
  Read(&dri_minor_version, &buf);

  // dri_minor_patch
  Read(&dri_minor_patch, &buf);

  Align(&buf, 4);
  DUMP_WILL_BE_CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<XF86Dri::QueryDirectRenderingCapableReply>
XF86Dri::QueryDirectRenderingCapable(
    const XF86Dri::QueryDirectRenderingCapableRequest& request) {
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

  return connection_->SendRequest<XF86Dri::QueryDirectRenderingCapableReply>(
      &buf, "XF86Dri::QueryDirectRenderingCapable", false);
}

Future<XF86Dri::QueryDirectRenderingCapableReply>
XF86Dri::QueryDirectRenderingCapable(const uint32_t& screen) {
  return XF86Dri::QueryDirectRenderingCapable(
      XF86Dri::QueryDirectRenderingCapableRequest{screen});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<XF86Dri::QueryDirectRenderingCapableReply> detail::ReadReply<
    XF86Dri::QueryDirectRenderingCapableReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<XF86Dri::QueryDirectRenderingCapableReply>();

  auto& sequence = (*reply).sequence;
  auto& is_capable = (*reply).is_capable;

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

  // is_capable
  Read(&is_capable, &buf);

  Align(&buf, 4);
  DUMP_WILL_BE_CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<XF86Dri::OpenConnectionReply> XF86Dri::OpenConnection(
    const XF86Dri::OpenConnectionRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& screen = request.screen;

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

  Align(&buf, 4);

  return connection_->SendRequest<XF86Dri::OpenConnectionReply>(
      &buf, "XF86Dri::OpenConnection", false);
}

Future<XF86Dri::OpenConnectionReply> XF86Dri::OpenConnection(
    const uint32_t& screen) {
  return XF86Dri::OpenConnection(XF86Dri::OpenConnectionRequest{screen});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<XF86Dri::OpenConnectionReply> detail::ReadReply<
    XF86Dri::OpenConnectionReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<XF86Dri::OpenConnectionReply>();

  auto& sequence = (*reply).sequence;
  auto& sarea_handle_low = (*reply).sarea_handle_low;
  auto& sarea_handle_high = (*reply).sarea_handle_high;
  uint32_t bus_id_len{};
  auto& bus_id = (*reply).bus_id;

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

  // sarea_handle_low
  Read(&sarea_handle_low, &buf);

  // sarea_handle_high
  Read(&sarea_handle_high, &buf);

  // bus_id_len
  Read(&bus_id_len, &buf);

  // pad1
  Pad(&buf, 12);

  // bus_id
  bus_id.resize(bus_id_len);
  for (auto& bus_id_elem : bus_id) {
    // bus_id_elem
    Read(&bus_id_elem, &buf);
  }

  Align(&buf, 4);
  DUMP_WILL_BE_CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> XF86Dri::CloseConnection(
    const XF86Dri::CloseConnectionRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

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

  // screen
  buf.Write(&screen);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "XF86Dri::CloseConnection",
                                        false);
}

Future<void> XF86Dri::CloseConnection(const uint32_t& screen) {
  return XF86Dri::CloseConnection(XF86Dri::CloseConnectionRequest{screen});
}

Future<XF86Dri::GetClientDriverNameReply> XF86Dri::GetClientDriverName(
    const XF86Dri::GetClientDriverNameRequest& request) {
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

  Align(&buf, 4);

  return connection_->SendRequest<XF86Dri::GetClientDriverNameReply>(
      &buf, "XF86Dri::GetClientDriverName", false);
}

Future<XF86Dri::GetClientDriverNameReply> XF86Dri::GetClientDriverName(
    const uint32_t& screen) {
  return XF86Dri::GetClientDriverName(
      XF86Dri::GetClientDriverNameRequest{screen});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<XF86Dri::GetClientDriverNameReply> detail::ReadReply<
    XF86Dri::GetClientDriverNameReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<XF86Dri::GetClientDriverNameReply>();

  auto& sequence = (*reply).sequence;
  auto& client_driver_major_version = (*reply).client_driver_major_version;
  auto& client_driver_minor_version = (*reply).client_driver_minor_version;
  auto& client_driver_patch_version = (*reply).client_driver_patch_version;
  uint32_t client_driver_name_len{};
  auto& client_driver_name = (*reply).client_driver_name;

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

  // client_driver_major_version
  Read(&client_driver_major_version, &buf);

  // client_driver_minor_version
  Read(&client_driver_minor_version, &buf);

  // client_driver_patch_version
  Read(&client_driver_patch_version, &buf);

  // client_driver_name_len
  Read(&client_driver_name_len, &buf);

  // pad1
  Pad(&buf, 8);

  // client_driver_name
  client_driver_name.resize(client_driver_name_len);
  for (auto& client_driver_name_elem : client_driver_name) {
    // client_driver_name_elem
    Read(&client_driver_name_elem, &buf);
  }

  Align(&buf, 4);
  DUMP_WILL_BE_CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<XF86Dri::CreateContextReply> XF86Dri::CreateContext(
    const XF86Dri::CreateContextRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& screen = request.screen;
  auto& visual = request.visual;
  auto& context = request.context;

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

  // visual
  buf.Write(&visual);

  // context
  buf.Write(&context);

  Align(&buf, 4);

  return connection_->SendRequest<XF86Dri::CreateContextReply>(
      &buf, "XF86Dri::CreateContext", false);
}

Future<XF86Dri::CreateContextReply> XF86Dri::CreateContext(
    const uint32_t& screen,
    const uint32_t& visual,
    const uint32_t& context) {
  return XF86Dri::CreateContext(
      XF86Dri::CreateContextRequest{screen, visual, context});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<XF86Dri::CreateContextReply> detail::ReadReply<
    XF86Dri::CreateContextReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<XF86Dri::CreateContextReply>();

  auto& sequence = (*reply).sequence;
  auto& hw_context = (*reply).hw_context;

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

  // hw_context
  Read(&hw_context, &buf);

  Align(&buf, 4);
  DUMP_WILL_BE_CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> XF86Dri::DestroyContext(
    const XF86Dri::DestroyContextRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& screen = request.screen;
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

  // screen
  buf.Write(&screen);

  // context
  buf.Write(&context);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "XF86Dri::DestroyContext", false);
}

Future<void> XF86Dri::DestroyContext(const uint32_t& screen,
                                     const uint32_t& context) {
  return XF86Dri::DestroyContext(
      XF86Dri::DestroyContextRequest{screen, context});
}

Future<XF86Dri::CreateDrawableReply> XF86Dri::CreateDrawable(
    const XF86Dri::CreateDrawableRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& screen = request.screen;
  auto& drawable = request.drawable;

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

  // drawable
  buf.Write(&drawable);

  Align(&buf, 4);

  return connection_->SendRequest<XF86Dri::CreateDrawableReply>(
      &buf, "XF86Dri::CreateDrawable", false);
}

Future<XF86Dri::CreateDrawableReply> XF86Dri::CreateDrawable(
    const uint32_t& screen,
    const uint32_t& drawable) {
  return XF86Dri::CreateDrawable(
      XF86Dri::CreateDrawableRequest{screen, drawable});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<XF86Dri::CreateDrawableReply> detail::ReadReply<
    XF86Dri::CreateDrawableReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<XF86Dri::CreateDrawableReply>();

  auto& sequence = (*reply).sequence;
  auto& hw_drawable_handle = (*reply).hw_drawable_handle;

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

  // hw_drawable_handle
  Read(&hw_drawable_handle, &buf);

  Align(&buf, 4);
  DUMP_WILL_BE_CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> XF86Dri::DestroyDrawable(
    const XF86Dri::DestroyDrawableRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& screen = request.screen;
  auto& drawable = request.drawable;

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

  // drawable
  buf.Write(&drawable);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "XF86Dri::DestroyDrawable",
                                        false);
}

Future<void> XF86Dri::DestroyDrawable(const uint32_t& screen,
                                      const uint32_t& drawable) {
  return XF86Dri::DestroyDrawable(
      XF86Dri::DestroyDrawableRequest{screen, drawable});
}

Future<XF86Dri::GetDrawableInfoReply> XF86Dri::GetDrawableInfo(
    const XF86Dri::GetDrawableInfoRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& screen = request.screen;
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

  // screen
  buf.Write(&screen);

  // drawable
  buf.Write(&drawable);

  Align(&buf, 4);

  return connection_->SendRequest<XF86Dri::GetDrawableInfoReply>(
      &buf, "XF86Dri::GetDrawableInfo", false);
}

Future<XF86Dri::GetDrawableInfoReply> XF86Dri::GetDrawableInfo(
    const uint32_t& screen,
    const uint32_t& drawable) {
  return XF86Dri::GetDrawableInfo(
      XF86Dri::GetDrawableInfoRequest{screen, drawable});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<XF86Dri::GetDrawableInfoReply> detail::ReadReply<
    XF86Dri::GetDrawableInfoReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<XF86Dri::GetDrawableInfoReply>();

  auto& sequence = (*reply).sequence;
  auto& drawable_table_index = (*reply).drawable_table_index;
  auto& drawable_table_stamp = (*reply).drawable_table_stamp;
  auto& drawable_origin_X = (*reply).drawable_origin_X;
  auto& drawable_origin_Y = (*reply).drawable_origin_Y;
  auto& drawable_size_W = (*reply).drawable_size_W;
  auto& drawable_size_H = (*reply).drawable_size_H;
  uint32_t num_clip_rects{};
  auto& back_x = (*reply).back_x;
  auto& back_y = (*reply).back_y;
  uint32_t num_back_clip_rects{};
  auto& clip_rects = (*reply).clip_rects;
  size_t clip_rects_len = clip_rects.size();
  auto& back_clip_rects = (*reply).back_clip_rects;
  size_t back_clip_rects_len = back_clip_rects.size();

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

  // drawable_table_index
  Read(&drawable_table_index, &buf);

  // drawable_table_stamp
  Read(&drawable_table_stamp, &buf);

  // drawable_origin_X
  Read(&drawable_origin_X, &buf);

  // drawable_origin_Y
  Read(&drawable_origin_Y, &buf);

  // drawable_size_W
  Read(&drawable_size_W, &buf);

  // drawable_size_H
  Read(&drawable_size_H, &buf);

  // num_clip_rects
  Read(&num_clip_rects, &buf);

  // back_x
  Read(&back_x, &buf);

  // back_y
  Read(&back_y, &buf);

  // num_back_clip_rects
  Read(&num_back_clip_rects, &buf);

  // clip_rects
  clip_rects.resize(num_clip_rects);
  for (auto& clip_rects_elem : clip_rects) {
    // clip_rects_elem
    {
      auto& x1 = clip_rects_elem.x1;
      auto& y1 = clip_rects_elem.y1;
      auto& x2 = clip_rects_elem.x2;
      auto& x3 = clip_rects_elem.x3;

      // x1
      Read(&x1, &buf);

      // y1
      Read(&y1, &buf);

      // x2
      Read(&x2, &buf);

      // x3
      Read(&x3, &buf);
    }
  }

  // back_clip_rects
  back_clip_rects.resize(num_back_clip_rects);
  for (auto& back_clip_rects_elem : back_clip_rects) {
    // back_clip_rects_elem
    {
      auto& x1 = back_clip_rects_elem.x1;
      auto& y1 = back_clip_rects_elem.y1;
      auto& x2 = back_clip_rects_elem.x2;
      auto& x3 = back_clip_rects_elem.x3;

      // x1
      Read(&x1, &buf);

      // y1
      Read(&y1, &buf);

      // x2
      Read(&x2, &buf);

      // x3
      Read(&x3, &buf);
    }
  }

  Align(&buf, 4);
  DUMP_WILL_BE_CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<XF86Dri::GetDeviceInfoReply> XF86Dri::GetDeviceInfo(
    const XF86Dri::GetDeviceInfoRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& screen = request.screen;

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

  Align(&buf, 4);

  return connection_->SendRequest<XF86Dri::GetDeviceInfoReply>(
      &buf, "XF86Dri::GetDeviceInfo", false);
}

Future<XF86Dri::GetDeviceInfoReply> XF86Dri::GetDeviceInfo(
    const uint32_t& screen) {
  return XF86Dri::GetDeviceInfo(XF86Dri::GetDeviceInfoRequest{screen});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<XF86Dri::GetDeviceInfoReply> detail::ReadReply<
    XF86Dri::GetDeviceInfoReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<XF86Dri::GetDeviceInfoReply>();

  auto& sequence = (*reply).sequence;
  auto& framebuffer_handle_low = (*reply).framebuffer_handle_low;
  auto& framebuffer_handle_high = (*reply).framebuffer_handle_high;
  auto& framebuffer_origin_offset = (*reply).framebuffer_origin_offset;
  auto& framebuffer_size = (*reply).framebuffer_size;
  auto& framebuffer_stride = (*reply).framebuffer_stride;
  uint32_t device_private_size{};
  auto& device_private = (*reply).device_private;
  size_t device_private_len = device_private.size();

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

  // framebuffer_handle_low
  Read(&framebuffer_handle_low, &buf);

  // framebuffer_handle_high
  Read(&framebuffer_handle_high, &buf);

  // framebuffer_origin_offset
  Read(&framebuffer_origin_offset, &buf);

  // framebuffer_size
  Read(&framebuffer_size, &buf);

  // framebuffer_stride
  Read(&framebuffer_stride, &buf);

  // device_private_size
  Read(&device_private_size, &buf);

  // device_private
  device_private.resize(device_private_size);
  for (auto& device_private_elem : device_private) {
    // device_private_elem
    Read(&device_private_elem, &buf);
  }

  Align(&buf, 4);
  DUMP_WILL_BE_CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<XF86Dri::AuthConnectionReply> XF86Dri::AuthConnection(
    const XF86Dri::AuthConnectionRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& screen = request.screen;
  auto& magic = request.magic;

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

  // magic
  buf.Write(&magic);

  Align(&buf, 4);

  return connection_->SendRequest<XF86Dri::AuthConnectionReply>(
      &buf, "XF86Dri::AuthConnection", false);
}

Future<XF86Dri::AuthConnectionReply> XF86Dri::AuthConnection(
    const uint32_t& screen,
    const uint32_t& magic) {
  return XF86Dri::AuthConnection(XF86Dri::AuthConnectionRequest{screen, magic});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<XF86Dri::AuthConnectionReply> detail::ReadReply<
    XF86Dri::AuthConnectionReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<XF86Dri::AuthConnectionReply>();

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
  DUMP_WILL_BE_CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

}  // namespace x11
