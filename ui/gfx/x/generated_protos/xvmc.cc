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

#include "xvmc.h"

#include <unistd.h>
#include <xcb/xcb.h>
#include <xcb/xcbext.h>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "ui/gfx/x/xproto_internal.h"

namespace x11 {

XvMC::XvMC(Connection* connection, const x11::QueryExtensionReply& info)
    : connection_(connection), info_(info) {}

Future<XvMC::QueryVersionReply> XvMC::QueryVersion(
    const XvMC::QueryVersionRequest& request) {
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

  return connection_->SendRequest<XvMC::QueryVersionReply>(
      &buf, "XvMC::QueryVersion", false);
}

Future<XvMC::QueryVersionReply> XvMC::QueryVersion() {
  return XvMC::QueryVersion(XvMC::QueryVersionRequest{});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<XvMC::QueryVersionReply> detail::ReadReply<
    XvMC::QueryVersionReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<XvMC::QueryVersionReply>();

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
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<XvMC::ListSurfaceTypesReply> XvMC::ListSurfaceTypes(
    const XvMC::ListSurfaceTypesRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& port_id = request.port_id;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 1;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // port_id
  buf.Write(&port_id);

  Align(&buf, 4);

  return connection_->SendRequest<XvMC::ListSurfaceTypesReply>(
      &buf, "XvMC::ListSurfaceTypes", false);
}

Future<XvMC::ListSurfaceTypesReply> XvMC::ListSurfaceTypes(
    const Xv::Port& port_id) {
  return XvMC::ListSurfaceTypes(XvMC::ListSurfaceTypesRequest{port_id});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<XvMC::ListSurfaceTypesReply> detail::ReadReply<
    XvMC::ListSurfaceTypesReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<XvMC::ListSurfaceTypesReply>();

  auto& sequence = (*reply).sequence;
  uint32_t num{};
  auto& surfaces = (*reply).surfaces;
  size_t surfaces_len = surfaces.size();

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

  // num
  Read(&num, &buf);

  // pad1
  Pad(&buf, 20);

  // surfaces
  surfaces.resize(num);
  for (auto& surfaces_elem : surfaces) {
    // surfaces_elem
    {
      auto& id = surfaces_elem.id;
      auto& chroma_format = surfaces_elem.chroma_format;
      auto& pad0 = surfaces_elem.pad0;
      auto& max_width = surfaces_elem.max_width;
      auto& max_height = surfaces_elem.max_height;
      auto& subpicture_max_width = surfaces_elem.subpicture_max_width;
      auto& subpicture_max_height = surfaces_elem.subpicture_max_height;
      auto& mc_type = surfaces_elem.mc_type;
      auto& flags = surfaces_elem.flags;

      // id
      Read(&id, &buf);

      // chroma_format
      Read(&chroma_format, &buf);

      // pad0
      Read(&pad0, &buf);

      // max_width
      Read(&max_width, &buf);

      // max_height
      Read(&max_height, &buf);

      // subpicture_max_width
      Read(&subpicture_max_width, &buf);

      // subpicture_max_height
      Read(&subpicture_max_height, &buf);

      // mc_type
      Read(&mc_type, &buf);

      // flags
      Read(&flags, &buf);
    }
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<XvMC::CreateContextReply> XvMC::CreateContext(
    const XvMC::CreateContextRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_id = request.context_id;
  auto& port_id = request.port_id;
  auto& surface_id = request.surface_id;
  auto& width = request.width;
  auto& height = request.height;
  auto& flags = request.flags;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 2;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_id
  buf.Write(&context_id);

  // port_id
  buf.Write(&port_id);

  // surface_id
  buf.Write(&surface_id);

  // width
  buf.Write(&width);

  // height
  buf.Write(&height);

  // flags
  buf.Write(&flags);

  Align(&buf, 4);

  return connection_->SendRequest<XvMC::CreateContextReply>(
      &buf, "XvMC::CreateContext", false);
}

Future<XvMC::CreateContextReply> XvMC::CreateContext(const Context& context_id,
                                                     const Xv::Port& port_id,
                                                     const Surface& surface_id,
                                                     const uint16_t& width,
                                                     const uint16_t& height,
                                                     const uint32_t& flags) {
  return XvMC::CreateContext(XvMC::CreateContextRequest{
      context_id, port_id, surface_id, width, height, flags});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<XvMC::CreateContextReply> detail::ReadReply<
    XvMC::CreateContextReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<XvMC::CreateContextReply>();

  auto& sequence = (*reply).sequence;
  auto& width_actual = (*reply).width_actual;
  auto& height_actual = (*reply).height_actual;
  auto& flags_return = (*reply).flags_return;
  auto& priv_data = (*reply).priv_data;
  size_t priv_data_len = priv_data.size();

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

  // width_actual
  Read(&width_actual, &buf);

  // height_actual
  Read(&height_actual, &buf);

  // flags_return
  Read(&flags_return, &buf);

  // pad1
  Pad(&buf, 20);

  // priv_data
  priv_data.resize(length);
  for (auto& priv_data_elem : priv_data) {
    // priv_data_elem
    Read(&priv_data_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> XvMC::DestroyContext(const XvMC::DestroyContextRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_id = request.context_id;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 3;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_id
  buf.Write(&context_id);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "XvMC::DestroyContext", false);
}

Future<void> XvMC::DestroyContext(const Context& context_id) {
  return XvMC::DestroyContext(XvMC::DestroyContextRequest{context_id});
}

Future<XvMC::CreateSurfaceReply> XvMC::CreateSurface(
    const XvMC::CreateSurfaceRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& surface_id = request.surface_id;
  auto& context_id = request.context_id;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 4;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // surface_id
  buf.Write(&surface_id);

  // context_id
  buf.Write(&context_id);

  Align(&buf, 4);

  return connection_->SendRequest<XvMC::CreateSurfaceReply>(
      &buf, "XvMC::CreateSurface", false);
}

Future<XvMC::CreateSurfaceReply> XvMC::CreateSurface(
    const Surface& surface_id,
    const Context& context_id) {
  return XvMC::CreateSurface(
      XvMC::CreateSurfaceRequest{surface_id, context_id});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<XvMC::CreateSurfaceReply> detail::ReadReply<
    XvMC::CreateSurfaceReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<XvMC::CreateSurfaceReply>();

  auto& sequence = (*reply).sequence;
  auto& priv_data = (*reply).priv_data;
  size_t priv_data_len = priv_data.size();

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

  // priv_data
  priv_data.resize(length);
  for (auto& priv_data_elem : priv_data) {
    // priv_data_elem
    Read(&priv_data_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> XvMC::DestroySurface(const XvMC::DestroySurfaceRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& surface_id = request.surface_id;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 5;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // surface_id
  buf.Write(&surface_id);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "XvMC::DestroySurface", false);
}

Future<void> XvMC::DestroySurface(const Surface& surface_id) {
  return XvMC::DestroySurface(XvMC::DestroySurfaceRequest{surface_id});
}

Future<XvMC::CreateSubpictureReply> XvMC::CreateSubpicture(
    const XvMC::CreateSubpictureRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& subpicture_id = request.subpicture_id;
  auto& context = request.context;
  auto& xvimage_id = request.xvimage_id;
  auto& width = request.width;
  auto& height = request.height;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 6;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // subpicture_id
  buf.Write(&subpicture_id);

  // context
  buf.Write(&context);

  // xvimage_id
  buf.Write(&xvimage_id);

  // width
  buf.Write(&width);

  // height
  buf.Write(&height);

  Align(&buf, 4);

  return connection_->SendRequest<XvMC::CreateSubpictureReply>(
      &buf, "XvMC::CreateSubpicture", false);
}

Future<XvMC::CreateSubpictureReply> XvMC::CreateSubpicture(
    const SubPicture& subpicture_id,
    const Context& context,
    const uint32_t& xvimage_id,
    const uint16_t& width,
    const uint16_t& height) {
  return XvMC::CreateSubpicture(XvMC::CreateSubpictureRequest{
      subpicture_id, context, xvimage_id, width, height});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<XvMC::CreateSubpictureReply> detail::ReadReply<
    XvMC::CreateSubpictureReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<XvMC::CreateSubpictureReply>();

  auto& sequence = (*reply).sequence;
  auto& width_actual = (*reply).width_actual;
  auto& height_actual = (*reply).height_actual;
  auto& num_palette_entries = (*reply).num_palette_entries;
  auto& entry_bytes = (*reply).entry_bytes;
  auto& component_order = (*reply).component_order;
  size_t component_order_len = component_order.size();
  auto& priv_data = (*reply).priv_data;
  size_t priv_data_len = priv_data.size();

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

  // width_actual
  Read(&width_actual, &buf);

  // height_actual
  Read(&height_actual, &buf);

  // num_palette_entries
  Read(&num_palette_entries, &buf);

  // entry_bytes
  Read(&entry_bytes, &buf);

  // component_order
  for (auto& component_order_elem : component_order) {
    // component_order_elem
    Read(&component_order_elem, &buf);
  }

  // pad1
  Pad(&buf, 12);

  // priv_data
  priv_data.resize(length);
  for (auto& priv_data_elem : priv_data) {
    // priv_data_elem
    Read(&priv_data_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> XvMC::DestroySubpicture(
    const XvMC::DestroySubpictureRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& subpicture_id = request.subpicture_id;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 7;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // subpicture_id
  buf.Write(&subpicture_id);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "XvMC::DestroySubpicture", false);
}

Future<void> XvMC::DestroySubpicture(const SubPicture& subpicture_id) {
  return XvMC::DestroySubpicture(XvMC::DestroySubpictureRequest{subpicture_id});
}

Future<XvMC::ListSubpictureTypesReply> XvMC::ListSubpictureTypes(
    const XvMC::ListSubpictureTypesRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& port_id = request.port_id;
  auto& surface_id = request.surface_id;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 8;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // port_id
  buf.Write(&port_id);

  // surface_id
  buf.Write(&surface_id);

  Align(&buf, 4);

  return connection_->SendRequest<XvMC::ListSubpictureTypesReply>(
      &buf, "XvMC::ListSubpictureTypes", false);
}

Future<XvMC::ListSubpictureTypesReply> XvMC::ListSubpictureTypes(
    const Xv::Port& port_id,
    const Surface& surface_id) {
  return XvMC::ListSubpictureTypes(
      XvMC::ListSubpictureTypesRequest{port_id, surface_id});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<XvMC::ListSubpictureTypesReply> detail::ReadReply<
    XvMC::ListSubpictureTypesReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<XvMC::ListSubpictureTypesReply>();

  auto& sequence = (*reply).sequence;
  uint32_t num{};
  auto& types = (*reply).types;
  size_t types_len = types.size();

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

  // num
  Read(&num, &buf);

  // pad1
  Pad(&buf, 20);

  // types
  types.resize(num);
  for (auto& types_elem : types) {
    // types_elem
    {
      auto& id = types_elem.id;
      auto& type = types_elem.type;
      auto& byte_order = types_elem.byte_order;
      auto& guid = types_elem.guid;
      size_t guid_len = guid.size();
      auto& bpp = types_elem.bpp;
      auto& num_planes = types_elem.num_planes;
      auto& depth = types_elem.depth;
      auto& red_mask = types_elem.red_mask;
      auto& green_mask = types_elem.green_mask;
      auto& blue_mask = types_elem.blue_mask;
      auto& format = types_elem.format;
      auto& y_sample_bits = types_elem.y_sample_bits;
      auto& u_sample_bits = types_elem.u_sample_bits;
      auto& v_sample_bits = types_elem.v_sample_bits;
      auto& vhorz_y_period = types_elem.vhorz_y_period;
      auto& vhorz_u_period = types_elem.vhorz_u_period;
      auto& vhorz_v_period = types_elem.vhorz_v_period;
      auto& vvert_y_period = types_elem.vvert_y_period;
      auto& vvert_u_period = types_elem.vvert_u_period;
      auto& vvert_v_period = types_elem.vvert_v_period;
      auto& vcomp_order = types_elem.vcomp_order;
      size_t vcomp_order_len = vcomp_order.size();
      auto& vscanline_order = types_elem.vscanline_order;

      // id
      Read(&id, &buf);

      // type
      uint8_t tmp0;
      Read(&tmp0, &buf);
      type = static_cast<Xv::ImageFormatInfoType>(tmp0);

      // byte_order
      uint8_t tmp1;
      Read(&tmp1, &buf);
      byte_order = static_cast<ImageOrder>(tmp1);

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
      uint8_t tmp2;
      Read(&tmp2, &buf);
      format = static_cast<Xv::ImageFormatInfoFormat>(tmp2);

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
      uint8_t tmp3;
      Read(&tmp3, &buf);
      vscanline_order = static_cast<Xv::ScanlineOrder>(tmp3);

      // pad4
      Pad(&buf, 11);
    }
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

}  // namespace x11
