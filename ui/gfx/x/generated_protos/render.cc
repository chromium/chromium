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

#include "render.h"

#include <unistd.h>
#include <xcb/xcb.h>
#include <xcb/xcbext.h>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/xproto_internal.h"

namespace x11 {

Render::Render(Connection* connection, const x11::QueryExtensionReply& info)
    : connection_(connection), info_(info) {}

std::string Render::PictFormatError::ToString() const {
  std::stringstream ss_;
  ss_ << "Render::PictFormatError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<Render::PictFormatError>(Render::PictFormatError* error_,
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

std::string Render::PictureError::ToString() const {
  std::stringstream ss_;
  ss_ << "Render::PictureError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<Render::PictureError>(Render::PictureError* error_,
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

std::string Render::PictOpError::ToString() const {
  std::stringstream ss_;
  ss_ << "Render::PictOpError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<Render::PictOpError>(Render::PictOpError* error_,
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

std::string Render::GlyphSetError::ToString() const {
  std::stringstream ss_;
  ss_ << "Render::GlyphSetError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<Render::GlyphSetError>(Render::GlyphSetError* error_,
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

std::string Render::GlyphError::ToString() const {
  std::stringstream ss_;
  ss_ << "Render::GlyphError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<Render::GlyphError>(Render::GlyphError* error_,
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

Future<Render::QueryVersionReply> Render::QueryVersion(
    const Render::QueryVersionRequest& request) {
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

  return connection_->SendRequest<Render::QueryVersionReply>(
      &buf, "Render::QueryVersion", false);
}

Future<Render::QueryVersionReply> Render::QueryVersion(
    const uint32_t& client_major_version,
    const uint32_t& client_minor_version) {
  return Render::QueryVersion(
      Render::QueryVersionRequest{client_major_version, client_minor_version});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Render::QueryVersionReply> detail::ReadReply<
    Render::QueryVersionReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Render::QueryVersionReply>();

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

Future<Render::QueryPictFormatsReply> Render::QueryPictFormats(
    const Render::QueryPictFormatsRequest& request) {
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

  return connection_->SendRequest<Render::QueryPictFormatsReply>(
      &buf, "Render::QueryPictFormats", false);
}

Future<Render::QueryPictFormatsReply> Render::QueryPictFormats() {
  return Render::QueryPictFormats(Render::QueryPictFormatsRequest{});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Render::QueryPictFormatsReply> detail::ReadReply<
    Render::QueryPictFormatsReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Render::QueryPictFormatsReply>();

  auto& sequence = (*reply).sequence;
  uint32_t num_formats{};
  uint32_t num_screens{};
  auto& num_depths = (*reply).num_depths;
  auto& num_visuals = (*reply).num_visuals;
  uint32_t num_subpixel{};
  auto& formats = (*reply).formats;
  auto& screens = (*reply).screens;
  auto& subpixels = (*reply).subpixels;

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

  // num_screens
  Read(&num_screens, &buf);

  // num_depths
  Read(&num_depths, &buf);

  // num_visuals
  Read(&num_visuals, &buf);

  // num_subpixel
  Read(&num_subpixel, &buf);

  // pad1
  Pad(&buf, 4);

  // formats
  formats.resize(num_formats);
  for (auto& formats_elem : formats) {
    // formats_elem
    {
      auto& id = formats_elem.id;
      auto& type = formats_elem.type;
      auto& depth = formats_elem.depth;
      auto& direct = formats_elem.direct;
      auto& colormap = formats_elem.colormap;

      // id
      Read(&id, &buf);

      // type
      uint8_t tmp0;
      Read(&tmp0, &buf);
      type = static_cast<Render::PictType>(tmp0);

      // depth
      Read(&depth, &buf);

      // pad0
      Pad(&buf, 2);

      // direct
      {
        auto& red_shift = direct.red_shift;
        auto& red_mask = direct.red_mask;
        auto& green_shift = direct.green_shift;
        auto& green_mask = direct.green_mask;
        auto& blue_shift = direct.blue_shift;
        auto& blue_mask = direct.blue_mask;
        auto& alpha_shift = direct.alpha_shift;
        auto& alpha_mask = direct.alpha_mask;

        // red_shift
        Read(&red_shift, &buf);

        // red_mask
        Read(&red_mask, &buf);

        // green_shift
        Read(&green_shift, &buf);

        // green_mask
        Read(&green_mask, &buf);

        // blue_shift
        Read(&blue_shift, &buf);

        // blue_mask
        Read(&blue_mask, &buf);

        // alpha_shift
        Read(&alpha_shift, &buf);

        // alpha_mask
        Read(&alpha_mask, &buf);
      }

      // colormap
      Read(&colormap, &buf);
    }
  }

  // screens
  screens.resize(num_screens);
  for (auto& screens_elem : screens) {
    // screens_elem
    {
      uint32_t num_depths{};
      auto& fallback = screens_elem.fallback;
      auto& depths = screens_elem.depths;

      // num_depths
      Read(&num_depths, &buf);

      // fallback
      Read(&fallback, &buf);

      // depths
      depths.resize(num_depths);
      for (auto& depths_elem : depths) {
        // depths_elem
        {
          auto& depth = depths_elem.depth;
          uint16_t num_visuals{};
          auto& visuals = depths_elem.visuals;

          // depth
          Read(&depth, &buf);

          // pad0
          Pad(&buf, 1);

          // num_visuals
          Read(&num_visuals, &buf);

          // pad1
          Pad(&buf, 4);

          // visuals
          visuals.resize(num_visuals);
          for (auto& visuals_elem : visuals) {
            // visuals_elem
            {
              auto& visual = visuals_elem.visual;
              auto& format = visuals_elem.format;

              // visual
              Read(&visual, &buf);

              // format
              Read(&format, &buf);
            }
          }
        }
      }
    }
  }

  // subpixels
  subpixels.resize(num_subpixel);
  for (auto& subpixels_elem : subpixels) {
    // subpixels_elem
    uint32_t tmp1;
    Read(&tmp1, &buf);
    subpixels_elem = static_cast<Render::SubPixel>(tmp1);
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Render::QueryPictIndexValuesReply> Render::QueryPictIndexValues(
    const Render::QueryPictIndexValuesRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& format = request.format;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 2;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // format
  buf.Write(&format);

  Align(&buf, 4);

  return connection_->SendRequest<Render::QueryPictIndexValuesReply>(
      &buf, "Render::QueryPictIndexValues", false);
}

Future<Render::QueryPictIndexValuesReply> Render::QueryPictIndexValues(
    const PictFormat& format) {
  return Render::QueryPictIndexValues(
      Render::QueryPictIndexValuesRequest{format});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Render::QueryPictIndexValuesReply> detail::ReadReply<
    Render::QueryPictIndexValuesReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Render::QueryPictIndexValuesReply>();

  auto& sequence = (*reply).sequence;
  uint32_t num_values{};
  auto& values = (*reply).values;

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

  // num_values
  Read(&num_values, &buf);

  // pad1
  Pad(&buf, 20);

  // values
  values.resize(num_values);
  for (auto& values_elem : values) {
    // values_elem
    {
      auto& pixel = values_elem.pixel;
      auto& red = values_elem.red;
      auto& green = values_elem.green;
      auto& blue = values_elem.blue;
      auto& alpha = values_elem.alpha;

      // pixel
      Read(&pixel, &buf);

      // red
      Read(&red, &buf);

      // green
      Read(&green, &buf);

      // blue
      Read(&blue, &buf);

      // alpha
      Read(&alpha, &buf);
    }
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Render::CreatePicture(
    const Render::CreatePictureRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& pid = request.pid;
  auto& drawable = request.drawable;
  auto& format = request.format;
  CreatePictureAttribute value_mask{};
  auto& value_list = request;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 4;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // pid
  buf.Write(&pid);

  // drawable
  buf.Write(&drawable);

  // format
  buf.Write(&format);

  // value_mask
  SwitchVar(CreatePictureAttribute::Repeat, value_list.repeat.has_value(), true,
            &value_mask);
  SwitchVar(CreatePictureAttribute::AlphaMap, value_list.alphamap.has_value(),
            true, &value_mask);
  SwitchVar(CreatePictureAttribute::AlphaXOrigin,
            value_list.alphaxorigin.has_value(), true, &value_mask);
  SwitchVar(CreatePictureAttribute::AlphaYOrigin,
            value_list.alphayorigin.has_value(), true, &value_mask);
  SwitchVar(CreatePictureAttribute::ClipXOrigin,
            value_list.clipxorigin.has_value(), true, &value_mask);
  SwitchVar(CreatePictureAttribute::ClipYOrigin,
            value_list.clipyorigin.has_value(), true, &value_mask);
  SwitchVar(CreatePictureAttribute::ClipMask, value_list.clipmask.has_value(),
            true, &value_mask);
  SwitchVar(CreatePictureAttribute::GraphicsExposure,
            value_list.graphicsexposure.has_value(), true, &value_mask);
  SwitchVar(CreatePictureAttribute::SubwindowMode,
            value_list.subwindowmode.has_value(), true, &value_mask);
  SwitchVar(CreatePictureAttribute::PolyEdge, value_list.polyedge.has_value(),
            true, &value_mask);
  SwitchVar(CreatePictureAttribute::PolyMode, value_list.polymode.has_value(),
            true, &value_mask);
  SwitchVar(CreatePictureAttribute::Dither, value_list.dither.has_value(), true,
            &value_mask);
  SwitchVar(CreatePictureAttribute::ComponentAlpha,
            value_list.componentalpha.has_value(), true, &value_mask);
  uint32_t tmp2;
  tmp2 = static_cast<uint32_t>(value_mask);
  buf.Write(&tmp2);

  // value_list
  auto value_list_expr = value_mask;
  if (CaseAnd(value_list_expr, CreatePictureAttribute::Repeat)) {
    auto& repeat = *value_list.repeat;

    // repeat
    uint32_t tmp3;
    tmp3 = static_cast<uint32_t>(repeat);
    buf.Write(&tmp3);
  }
  if (CaseAnd(value_list_expr, CreatePictureAttribute::AlphaMap)) {
    auto& alphamap = *value_list.alphamap;

    // alphamap
    buf.Write(&alphamap);
  }
  if (CaseAnd(value_list_expr, CreatePictureAttribute::AlphaXOrigin)) {
    auto& alphaxorigin = *value_list.alphaxorigin;

    // alphaxorigin
    buf.Write(&alphaxorigin);
  }
  if (CaseAnd(value_list_expr, CreatePictureAttribute::AlphaYOrigin)) {
    auto& alphayorigin = *value_list.alphayorigin;

    // alphayorigin
    buf.Write(&alphayorigin);
  }
  if (CaseAnd(value_list_expr, CreatePictureAttribute::ClipXOrigin)) {
    auto& clipxorigin = *value_list.clipxorigin;

    // clipxorigin
    buf.Write(&clipxorigin);
  }
  if (CaseAnd(value_list_expr, CreatePictureAttribute::ClipYOrigin)) {
    auto& clipyorigin = *value_list.clipyorigin;

    // clipyorigin
    buf.Write(&clipyorigin);
  }
  if (CaseAnd(value_list_expr, CreatePictureAttribute::ClipMask)) {
    auto& clipmask = *value_list.clipmask;

    // clipmask
    buf.Write(&clipmask);
  }
  if (CaseAnd(value_list_expr, CreatePictureAttribute::GraphicsExposure)) {
    auto& graphicsexposure = *value_list.graphicsexposure;

    // graphicsexposure
    buf.Write(&graphicsexposure);
  }
  if (CaseAnd(value_list_expr, CreatePictureAttribute::SubwindowMode)) {
    auto& subwindowmode = *value_list.subwindowmode;

    // subwindowmode
    uint32_t tmp4;
    tmp4 = static_cast<uint32_t>(subwindowmode);
    buf.Write(&tmp4);
  }
  if (CaseAnd(value_list_expr, CreatePictureAttribute::PolyEdge)) {
    auto& polyedge = *value_list.polyedge;

    // polyedge
    uint32_t tmp5;
    tmp5 = static_cast<uint32_t>(polyedge);
    buf.Write(&tmp5);
  }
  if (CaseAnd(value_list_expr, CreatePictureAttribute::PolyMode)) {
    auto& polymode = *value_list.polymode;

    // polymode
    uint32_t tmp6;
    tmp6 = static_cast<uint32_t>(polymode);
    buf.Write(&tmp6);
  }
  if (CaseAnd(value_list_expr, CreatePictureAttribute::Dither)) {
    auto& dither = *value_list.dither;

    // dither
    buf.Write(&dither);
  }
  if (CaseAnd(value_list_expr, CreatePictureAttribute::ComponentAlpha)) {
    auto& componentalpha = *value_list.componentalpha;

    // componentalpha
    buf.Write(&componentalpha);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Render::CreatePicture", false);
}

Future<void> Render::CreatePicture(
    const Picture& pid,
    const Drawable& drawable,
    const PictFormat& format,
    const std::optional<Repeat>& repeat,
    const std::optional<Picture>& alphamap,
    const std::optional<int32_t>& alphaxorigin,
    const std::optional<int32_t>& alphayorigin,
    const std::optional<int32_t>& clipxorigin,
    const std::optional<int32_t>& clipyorigin,
    const std::optional<Pixmap>& clipmask,
    const std::optional<uint32_t>& graphicsexposure,
    const std::optional<SubwindowMode>& subwindowmode,
    const std::optional<PolyEdge>& polyedge,
    const std::optional<PolyMode>& polymode,
    const std::optional<Atom>& dither,
    const std::optional<uint32_t>& componentalpha) {
  return Render::CreatePicture(Render::CreatePictureRequest{
      pid, drawable, format, repeat, alphamap, alphaxorigin, alphayorigin,
      clipxorigin, clipyorigin, clipmask, graphicsexposure, subwindowmode,
      polyedge, polymode, dither, componentalpha});
}

Future<void> Render::ChangePicture(
    const Render::ChangePictureRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& picture = request.picture;
  CreatePictureAttribute value_mask{};
  auto& value_list = request;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 5;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // picture
  buf.Write(&picture);

  // value_mask
  SwitchVar(CreatePictureAttribute::Repeat, value_list.repeat.has_value(), true,
            &value_mask);
  SwitchVar(CreatePictureAttribute::AlphaMap, value_list.alphamap.has_value(),
            true, &value_mask);
  SwitchVar(CreatePictureAttribute::AlphaXOrigin,
            value_list.alphaxorigin.has_value(), true, &value_mask);
  SwitchVar(CreatePictureAttribute::AlphaYOrigin,
            value_list.alphayorigin.has_value(), true, &value_mask);
  SwitchVar(CreatePictureAttribute::ClipXOrigin,
            value_list.clipxorigin.has_value(), true, &value_mask);
  SwitchVar(CreatePictureAttribute::ClipYOrigin,
            value_list.clipyorigin.has_value(), true, &value_mask);
  SwitchVar(CreatePictureAttribute::ClipMask, value_list.clipmask.has_value(),
            true, &value_mask);
  SwitchVar(CreatePictureAttribute::GraphicsExposure,
            value_list.graphicsexposure.has_value(), true, &value_mask);
  SwitchVar(CreatePictureAttribute::SubwindowMode,
            value_list.subwindowmode.has_value(), true, &value_mask);
  SwitchVar(CreatePictureAttribute::PolyEdge, value_list.polyedge.has_value(),
            true, &value_mask);
  SwitchVar(CreatePictureAttribute::PolyMode, value_list.polymode.has_value(),
            true, &value_mask);
  SwitchVar(CreatePictureAttribute::Dither, value_list.dither.has_value(), true,
            &value_mask);
  SwitchVar(CreatePictureAttribute::ComponentAlpha,
            value_list.componentalpha.has_value(), true, &value_mask);
  uint32_t tmp7;
  tmp7 = static_cast<uint32_t>(value_mask);
  buf.Write(&tmp7);

  // value_list
  auto value_list_expr = value_mask;
  if (CaseAnd(value_list_expr, CreatePictureAttribute::Repeat)) {
    auto& repeat = *value_list.repeat;

    // repeat
    uint32_t tmp8;
    tmp8 = static_cast<uint32_t>(repeat);
    buf.Write(&tmp8);
  }
  if (CaseAnd(value_list_expr, CreatePictureAttribute::AlphaMap)) {
    auto& alphamap = *value_list.alphamap;

    // alphamap
    buf.Write(&alphamap);
  }
  if (CaseAnd(value_list_expr, CreatePictureAttribute::AlphaXOrigin)) {
    auto& alphaxorigin = *value_list.alphaxorigin;

    // alphaxorigin
    buf.Write(&alphaxorigin);
  }
  if (CaseAnd(value_list_expr, CreatePictureAttribute::AlphaYOrigin)) {
    auto& alphayorigin = *value_list.alphayorigin;

    // alphayorigin
    buf.Write(&alphayorigin);
  }
  if (CaseAnd(value_list_expr, CreatePictureAttribute::ClipXOrigin)) {
    auto& clipxorigin = *value_list.clipxorigin;

    // clipxorigin
    buf.Write(&clipxorigin);
  }
  if (CaseAnd(value_list_expr, CreatePictureAttribute::ClipYOrigin)) {
    auto& clipyorigin = *value_list.clipyorigin;

    // clipyorigin
    buf.Write(&clipyorigin);
  }
  if (CaseAnd(value_list_expr, CreatePictureAttribute::ClipMask)) {
    auto& clipmask = *value_list.clipmask;

    // clipmask
    buf.Write(&clipmask);
  }
  if (CaseAnd(value_list_expr, CreatePictureAttribute::GraphicsExposure)) {
    auto& graphicsexposure = *value_list.graphicsexposure;

    // graphicsexposure
    buf.Write(&graphicsexposure);
  }
  if (CaseAnd(value_list_expr, CreatePictureAttribute::SubwindowMode)) {
    auto& subwindowmode = *value_list.subwindowmode;

    // subwindowmode
    uint32_t tmp9;
    tmp9 = static_cast<uint32_t>(subwindowmode);
    buf.Write(&tmp9);
  }
  if (CaseAnd(value_list_expr, CreatePictureAttribute::PolyEdge)) {
    auto& polyedge = *value_list.polyedge;

    // polyedge
    uint32_t tmp10;
    tmp10 = static_cast<uint32_t>(polyedge);
    buf.Write(&tmp10);
  }
  if (CaseAnd(value_list_expr, CreatePictureAttribute::PolyMode)) {
    auto& polymode = *value_list.polymode;

    // polymode
    uint32_t tmp11;
    tmp11 = static_cast<uint32_t>(polymode);
    buf.Write(&tmp11);
  }
  if (CaseAnd(value_list_expr, CreatePictureAttribute::Dither)) {
    auto& dither = *value_list.dither;

    // dither
    buf.Write(&dither);
  }
  if (CaseAnd(value_list_expr, CreatePictureAttribute::ComponentAlpha)) {
    auto& componentalpha = *value_list.componentalpha;

    // componentalpha
    buf.Write(&componentalpha);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Render::ChangePicture", false);
}

Future<void> Render::ChangePicture(
    const Picture& picture,
    const std::optional<Repeat>& repeat,
    const std::optional<Picture>& alphamap,
    const std::optional<int32_t>& alphaxorigin,
    const std::optional<int32_t>& alphayorigin,
    const std::optional<int32_t>& clipxorigin,
    const std::optional<int32_t>& clipyorigin,
    const std::optional<Pixmap>& clipmask,
    const std::optional<uint32_t>& graphicsexposure,
    const std::optional<SubwindowMode>& subwindowmode,
    const std::optional<PolyEdge>& polyedge,
    const std::optional<PolyMode>& polymode,
    const std::optional<Atom>& dither,
    const std::optional<uint32_t>& componentalpha) {
  return Render::ChangePicture(Render::ChangePictureRequest{
      picture, repeat, alphamap, alphaxorigin, alphayorigin, clipxorigin,
      clipyorigin, clipmask, graphicsexposure, subwindowmode, polyedge,
      polymode, dither, componentalpha});
}

Future<void> Render::SetPictureClipRectangles(
    const Render::SetPictureClipRectanglesRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& picture = request.picture;
  auto& clip_x_origin = request.clip_x_origin;
  auto& clip_y_origin = request.clip_y_origin;
  auto& rectangles = request.rectangles;
  size_t rectangles_len = rectangles.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 6;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // picture
  buf.Write(&picture);

  // clip_x_origin
  buf.Write(&clip_x_origin);

  // clip_y_origin
  buf.Write(&clip_y_origin);

  // rectangles
  CHECK_EQ(static_cast<size_t>(rectangles_len), rectangles.size());
  for (auto& rectangles_elem : rectangles) {
    // rectangles_elem
    {
      auto& x = rectangles_elem.x;
      auto& y = rectangles_elem.y;
      auto& width = rectangles_elem.width;
      auto& height = rectangles_elem.height;

      // x
      buf.Write(&x);

      // y
      buf.Write(&y);

      // width
      buf.Write(&width);

      // height
      buf.Write(&height);
    }
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(
      &buf, "Render::SetPictureClipRectangles", false);
}

Future<void> Render::SetPictureClipRectangles(
    const Picture& picture,
    const int16_t& clip_x_origin,
    const int16_t& clip_y_origin,
    const std::vector<Rectangle>& rectangles) {
  return Render::SetPictureClipRectangles(
      Render::SetPictureClipRectanglesRequest{picture, clip_x_origin,
                                              clip_y_origin, rectangles});
}

Future<void> Render::FreePicture(const Render::FreePictureRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& picture = request.picture;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 7;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // picture
  buf.Write(&picture);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Render::FreePicture", false);
}

Future<void> Render::FreePicture(const Picture& picture) {
  return Render::FreePicture(Render::FreePictureRequest{picture});
}

Future<void> Render::Composite(const Render::CompositeRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& op = request.op;
  auto& src = request.src;
  auto& mask = request.mask;
  auto& dst = request.dst;
  auto& src_x = request.src_x;
  auto& src_y = request.src_y;
  auto& mask_x = request.mask_x;
  auto& mask_y = request.mask_y;
  auto& dst_x = request.dst_x;
  auto& dst_y = request.dst_y;
  auto& width = request.width;
  auto& height = request.height;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 8;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // op
  uint8_t tmp12;
  tmp12 = static_cast<uint8_t>(op);
  buf.Write(&tmp12);

  // pad0
  Pad(&buf, 3);

  // src
  buf.Write(&src);

  // mask
  buf.Write(&mask);

  // dst
  buf.Write(&dst);

  // src_x
  buf.Write(&src_x);

  // src_y
  buf.Write(&src_y);

  // mask_x
  buf.Write(&mask_x);

  // mask_y
  buf.Write(&mask_y);

  // dst_x
  buf.Write(&dst_x);

  // dst_y
  buf.Write(&dst_y);

  // width
  buf.Write(&width);

  // height
  buf.Write(&height);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Render::Composite", false);
}

Future<void> Render::Composite(const PictOp& op,
                               const Picture& src,
                               const Picture& mask,
                               const Picture& dst,
                               const int16_t& src_x,
                               const int16_t& src_y,
                               const int16_t& mask_x,
                               const int16_t& mask_y,
                               const int16_t& dst_x,
                               const int16_t& dst_y,
                               const uint16_t& width,
                               const uint16_t& height) {
  return Render::Composite(
      Render::CompositeRequest{op, src, mask, dst, src_x, src_y, mask_x, mask_y,
                               dst_x, dst_y, width, height});
}

Future<void> Render::Trapezoids(const Render::TrapezoidsRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& op = request.op;
  auto& src = request.src;
  auto& dst = request.dst;
  auto& mask_format = request.mask_format;
  auto& src_x = request.src_x;
  auto& src_y = request.src_y;
  auto& traps = request.traps;
  size_t traps_len = traps.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 10;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // op
  uint8_t tmp13;
  tmp13 = static_cast<uint8_t>(op);
  buf.Write(&tmp13);

  // pad0
  Pad(&buf, 3);

  // src
  buf.Write(&src);

  // dst
  buf.Write(&dst);

  // mask_format
  buf.Write(&mask_format);

  // src_x
  buf.Write(&src_x);

  // src_y
  buf.Write(&src_y);

  // traps
  CHECK_EQ(static_cast<size_t>(traps_len), traps.size());
  for (auto& traps_elem : traps) {
    // traps_elem
    {
      auto& top = traps_elem.top;
      auto& bottom = traps_elem.bottom;
      auto& left = traps_elem.left;
      auto& right = traps_elem.right;

      // top
      buf.Write(&top);

      // bottom
      buf.Write(&bottom);

      // left
      {
        auto& p1 = left.p1;
        auto& p2 = left.p2;

        // p1
        {
          auto& x = p1.x;
          auto& y = p1.y;

          // x
          buf.Write(&x);

          // y
          buf.Write(&y);
        }

        // p2
        {
          auto& x = p2.x;
          auto& y = p2.y;

          // x
          buf.Write(&x);

          // y
          buf.Write(&y);
        }
      }

      // right
      {
        auto& p1 = right.p1;
        auto& p2 = right.p2;

        // p1
        {
          auto& x = p1.x;
          auto& y = p1.y;

          // x
          buf.Write(&x);

          // y
          buf.Write(&y);
        }

        // p2
        {
          auto& x = p2.x;
          auto& y = p2.y;

          // x
          buf.Write(&x);

          // y
          buf.Write(&y);
        }
      }
    }
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Render::Trapezoids", false);
}

Future<void> Render::Trapezoids(const PictOp& op,
                                const Picture& src,
                                const Picture& dst,
                                const PictFormat& mask_format,
                                const int16_t& src_x,
                                const int16_t& src_y,
                                const std::vector<Trapezoid>& traps) {
  return Render::Trapezoids(Render::TrapezoidsRequest{op, src, dst, mask_format,
                                                      src_x, src_y, traps});
}

Future<void> Render::Triangles(const Render::TrianglesRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& op = request.op;
  auto& src = request.src;
  auto& dst = request.dst;
  auto& mask_format = request.mask_format;
  auto& src_x = request.src_x;
  auto& src_y = request.src_y;
  auto& triangles = request.triangles;
  size_t triangles_len = triangles.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 11;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // op
  uint8_t tmp14;
  tmp14 = static_cast<uint8_t>(op);
  buf.Write(&tmp14);

  // pad0
  Pad(&buf, 3);

  // src
  buf.Write(&src);

  // dst
  buf.Write(&dst);

  // mask_format
  buf.Write(&mask_format);

  // src_x
  buf.Write(&src_x);

  // src_y
  buf.Write(&src_y);

  // triangles
  CHECK_EQ(static_cast<size_t>(triangles_len), triangles.size());
  for (auto& triangles_elem : triangles) {
    // triangles_elem
    {
      auto& p1 = triangles_elem.p1;
      auto& p2 = triangles_elem.p2;
      auto& p3 = triangles_elem.p3;

      // p1
      {
        auto& x = p1.x;
        auto& y = p1.y;

        // x
        buf.Write(&x);

        // y
        buf.Write(&y);
      }

      // p2
      {
        auto& x = p2.x;
        auto& y = p2.y;

        // x
        buf.Write(&x);

        // y
        buf.Write(&y);
      }

      // p3
      {
        auto& x = p3.x;
        auto& y = p3.y;

        // x
        buf.Write(&x);

        // y
        buf.Write(&y);
      }
    }
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Render::Triangles", false);
}

Future<void> Render::Triangles(const PictOp& op,
                               const Picture& src,
                               const Picture& dst,
                               const PictFormat& mask_format,
                               const int16_t& src_x,
                               const int16_t& src_y,
                               const std::vector<Triangle>& triangles) {
  return Render::Triangles(Render::TrianglesRequest{op, src, dst, mask_format,
                                                    src_x, src_y, triangles});
}

Future<void> Render::TriStrip(const Render::TriStripRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& op = request.op;
  auto& src = request.src;
  auto& dst = request.dst;
  auto& mask_format = request.mask_format;
  auto& src_x = request.src_x;
  auto& src_y = request.src_y;
  auto& points = request.points;
  size_t points_len = points.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 12;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // op
  uint8_t tmp15;
  tmp15 = static_cast<uint8_t>(op);
  buf.Write(&tmp15);

  // pad0
  Pad(&buf, 3);

  // src
  buf.Write(&src);

  // dst
  buf.Write(&dst);

  // mask_format
  buf.Write(&mask_format);

  // src_x
  buf.Write(&src_x);

  // src_y
  buf.Write(&src_y);

  // points
  CHECK_EQ(static_cast<size_t>(points_len), points.size());
  for (auto& points_elem : points) {
    // points_elem
    {
      auto& x = points_elem.x;
      auto& y = points_elem.y;

      // x
      buf.Write(&x);

      // y
      buf.Write(&y);
    }
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Render::TriStrip", false);
}

Future<void> Render::TriStrip(const PictOp& op,
                              const Picture& src,
                              const Picture& dst,
                              const PictFormat& mask_format,
                              const int16_t& src_x,
                              const int16_t& src_y,
                              const std::vector<PointFix>& points) {
  return Render::TriStrip(
      Render::TriStripRequest{op, src, dst, mask_format, src_x, src_y, points});
}

Future<void> Render::TriFan(const Render::TriFanRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& op = request.op;
  auto& src = request.src;
  auto& dst = request.dst;
  auto& mask_format = request.mask_format;
  auto& src_x = request.src_x;
  auto& src_y = request.src_y;
  auto& points = request.points;
  size_t points_len = points.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 13;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // op
  uint8_t tmp16;
  tmp16 = static_cast<uint8_t>(op);
  buf.Write(&tmp16);

  // pad0
  Pad(&buf, 3);

  // src
  buf.Write(&src);

  // dst
  buf.Write(&dst);

  // mask_format
  buf.Write(&mask_format);

  // src_x
  buf.Write(&src_x);

  // src_y
  buf.Write(&src_y);

  // points
  CHECK_EQ(static_cast<size_t>(points_len), points.size());
  for (auto& points_elem : points) {
    // points_elem
    {
      auto& x = points_elem.x;
      auto& y = points_elem.y;

      // x
      buf.Write(&x);

      // y
      buf.Write(&y);
    }
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Render::TriFan", false);
}

Future<void> Render::TriFan(const PictOp& op,
                            const Picture& src,
                            const Picture& dst,
                            const PictFormat& mask_format,
                            const int16_t& src_x,
                            const int16_t& src_y,
                            const std::vector<PointFix>& points) {
  return Render::TriFan(
      Render::TriFanRequest{op, src, dst, mask_format, src_x, src_y, points});
}

Future<void> Render::CreateGlyphSet(
    const Render::CreateGlyphSetRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& gsid = request.gsid;
  auto& format = request.format;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 17;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // gsid
  buf.Write(&gsid);

  // format
  buf.Write(&format);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Render::CreateGlyphSet", false);
}

Future<void> Render::CreateGlyphSet(const GlyphSet& gsid,
                                    const PictFormat& format) {
  return Render::CreateGlyphSet(Render::CreateGlyphSetRequest{gsid, format});
}

Future<void> Render::ReferenceGlyphSet(
    const Render::ReferenceGlyphSetRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& gsid = request.gsid;
  auto& existing = request.existing;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 18;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // gsid
  buf.Write(&gsid);

  // existing
  buf.Write(&existing);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Render::ReferenceGlyphSet",
                                        false);
}

Future<void> Render::ReferenceGlyphSet(const GlyphSet& gsid,
                                       const GlyphSet& existing) {
  return Render::ReferenceGlyphSet(
      Render::ReferenceGlyphSetRequest{gsid, existing});
}

Future<void> Render::FreeGlyphSet(const Render::FreeGlyphSetRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& glyphset = request.glyphset;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 19;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // glyphset
  buf.Write(&glyphset);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Render::FreeGlyphSet", false);
}

Future<void> Render::FreeGlyphSet(const GlyphSet& glyphset) {
  return Render::FreeGlyphSet(Render::FreeGlyphSetRequest{glyphset});
}

Future<void> Render::AddGlyphs(const Render::AddGlyphsRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& glyphset = request.glyphset;
  uint32_t glyphs_len{};
  auto& glyphids = request.glyphids;
  size_t glyphids_len = glyphids.size();
  auto& glyphs = request.glyphs;
  auto& data = request.data;
  size_t data_len = data.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 20;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // glyphset
  buf.Write(&glyphset);

  // glyphs_len
  glyphs_len = glyphs.size();
  buf.Write(&glyphs_len);

  // glyphids
  CHECK_EQ(static_cast<size_t>(glyphs_len), glyphids.size());
  for (auto& glyphids_elem : glyphids) {
    // glyphids_elem
    buf.Write(&glyphids_elem);
  }

  // glyphs
  CHECK_EQ(static_cast<size_t>(glyphs_len), glyphs.size());
  for (auto& glyphs_elem : glyphs) {
    // glyphs_elem
    {
      auto& width = glyphs_elem.width;
      auto& height = glyphs_elem.height;
      auto& x = glyphs_elem.x;
      auto& y = glyphs_elem.y;
      auto& x_off = glyphs_elem.x_off;
      auto& y_off = glyphs_elem.y_off;

      // width
      buf.Write(&width);

      // height
      buf.Write(&height);

      // x
      buf.Write(&x);

      // y
      buf.Write(&y);

      // x_off
      buf.Write(&x_off);

      // y_off
      buf.Write(&y_off);
    }
  }

  // data
  CHECK_EQ(static_cast<size_t>(data_len), data.size());
  for (auto& data_elem : data) {
    // data_elem
    buf.Write(&data_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Render::AddGlyphs", false);
}

Future<void> Render::AddGlyphs(const GlyphSet& glyphset,
                               const std::vector<uint32_t>& glyphids,
                               const std::vector<GlyphInfo>& glyphs,
                               const std::vector<uint8_t>& data) {
  return Render::AddGlyphs(
      Render::AddGlyphsRequest{glyphset, glyphids, glyphs, data});
}

Future<void> Render::FreeGlyphs(const Render::FreeGlyphsRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& glyphset = request.glyphset;
  auto& glyphs = request.glyphs;
  size_t glyphs_len = glyphs.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 22;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // glyphset
  buf.Write(&glyphset);

  // glyphs
  CHECK_EQ(static_cast<size_t>(glyphs_len), glyphs.size());
  for (auto& glyphs_elem : glyphs) {
    // glyphs_elem
    buf.Write(&glyphs_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Render::FreeGlyphs", false);
}

Future<void> Render::FreeGlyphs(const GlyphSet& glyphset,
                                const std::vector<Glyph>& glyphs) {
  return Render::FreeGlyphs(Render::FreeGlyphsRequest{glyphset, glyphs});
}

Future<void> Render::CompositeGlyphs8(
    const Render::CompositeGlyphs8Request& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& op = request.op;
  auto& src = request.src;
  auto& dst = request.dst;
  auto& mask_format = request.mask_format;
  auto& glyphset = request.glyphset;
  auto& src_x = request.src_x;
  auto& src_y = request.src_y;
  auto& glyphcmds = request.glyphcmds;
  size_t glyphcmds_len = glyphcmds.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 23;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // op
  uint8_t tmp17;
  tmp17 = static_cast<uint8_t>(op);
  buf.Write(&tmp17);

  // pad0
  Pad(&buf, 3);

  // src
  buf.Write(&src);

  // dst
  buf.Write(&dst);

  // mask_format
  buf.Write(&mask_format);

  // glyphset
  buf.Write(&glyphset);

  // src_x
  buf.Write(&src_x);

  // src_y
  buf.Write(&src_y);

  // glyphcmds
  CHECK_EQ(static_cast<size_t>(glyphcmds_len), glyphcmds.size());
  for (auto& glyphcmds_elem : glyphcmds) {
    // glyphcmds_elem
    buf.Write(&glyphcmds_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Render::CompositeGlyphs8",
                                        false);
}

Future<void> Render::CompositeGlyphs8(const PictOp& op,
                                      const Picture& src,
                                      const Picture& dst,
                                      const PictFormat& mask_format,
                                      const GlyphSet& glyphset,
                                      const int16_t& src_x,
                                      const int16_t& src_y,
                                      const std::vector<uint8_t>& glyphcmds) {
  return Render::CompositeGlyphs8(Render::CompositeGlyphs8Request{
      op, src, dst, mask_format, glyphset, src_x, src_y, glyphcmds});
}

Future<void> Render::CompositeGlyphs16(
    const Render::CompositeGlyphs16Request& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& op = request.op;
  auto& src = request.src;
  auto& dst = request.dst;
  auto& mask_format = request.mask_format;
  auto& glyphset = request.glyphset;
  auto& src_x = request.src_x;
  auto& src_y = request.src_y;
  auto& glyphcmds = request.glyphcmds;
  size_t glyphcmds_len = glyphcmds.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 24;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // op
  uint8_t tmp18;
  tmp18 = static_cast<uint8_t>(op);
  buf.Write(&tmp18);

  // pad0
  Pad(&buf, 3);

  // src
  buf.Write(&src);

  // dst
  buf.Write(&dst);

  // mask_format
  buf.Write(&mask_format);

  // glyphset
  buf.Write(&glyphset);

  // src_x
  buf.Write(&src_x);

  // src_y
  buf.Write(&src_y);

  // glyphcmds
  CHECK_EQ(static_cast<size_t>(glyphcmds_len), glyphcmds.size());
  for (auto& glyphcmds_elem : glyphcmds) {
    // glyphcmds_elem
    buf.Write(&glyphcmds_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Render::CompositeGlyphs16",
                                        false);
}

Future<void> Render::CompositeGlyphs16(const PictOp& op,
                                       const Picture& src,
                                       const Picture& dst,
                                       const PictFormat& mask_format,
                                       const GlyphSet& glyphset,
                                       const int16_t& src_x,
                                       const int16_t& src_y,
                                       const std::vector<uint8_t>& glyphcmds) {
  return Render::CompositeGlyphs16(Render::CompositeGlyphs16Request{
      op, src, dst, mask_format, glyphset, src_x, src_y, glyphcmds});
}

Future<void> Render::CompositeGlyphs32(
    const Render::CompositeGlyphs32Request& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& op = request.op;
  auto& src = request.src;
  auto& dst = request.dst;
  auto& mask_format = request.mask_format;
  auto& glyphset = request.glyphset;
  auto& src_x = request.src_x;
  auto& src_y = request.src_y;
  auto& glyphcmds = request.glyphcmds;
  size_t glyphcmds_len = glyphcmds.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 25;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // op
  uint8_t tmp19;
  tmp19 = static_cast<uint8_t>(op);
  buf.Write(&tmp19);

  // pad0
  Pad(&buf, 3);

  // src
  buf.Write(&src);

  // dst
  buf.Write(&dst);

  // mask_format
  buf.Write(&mask_format);

  // glyphset
  buf.Write(&glyphset);

  // src_x
  buf.Write(&src_x);

  // src_y
  buf.Write(&src_y);

  // glyphcmds
  CHECK_EQ(static_cast<size_t>(glyphcmds_len), glyphcmds.size());
  for (auto& glyphcmds_elem : glyphcmds) {
    // glyphcmds_elem
    buf.Write(&glyphcmds_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Render::CompositeGlyphs32",
                                        false);
}

Future<void> Render::CompositeGlyphs32(const PictOp& op,
                                       const Picture& src,
                                       const Picture& dst,
                                       const PictFormat& mask_format,
                                       const GlyphSet& glyphset,
                                       const int16_t& src_x,
                                       const int16_t& src_y,
                                       const std::vector<uint8_t>& glyphcmds) {
  return Render::CompositeGlyphs32(Render::CompositeGlyphs32Request{
      op, src, dst, mask_format, glyphset, src_x, src_y, glyphcmds});
}

Future<void> Render::FillRectangles(
    const Render::FillRectanglesRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& op = request.op;
  auto& dst = request.dst;
  auto& color = request.color;
  auto& rects = request.rects;
  size_t rects_len = rects.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 26;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // op
  uint8_t tmp20;
  tmp20 = static_cast<uint8_t>(op);
  buf.Write(&tmp20);

  // pad0
  Pad(&buf, 3);

  // dst
  buf.Write(&dst);

  // color
  {
    auto& red = color.red;
    auto& green = color.green;
    auto& blue = color.blue;
    auto& alpha = color.alpha;

    // red
    buf.Write(&red);

    // green
    buf.Write(&green);

    // blue
    buf.Write(&blue);

    // alpha
    buf.Write(&alpha);
  }

  // rects
  CHECK_EQ(static_cast<size_t>(rects_len), rects.size());
  for (auto& rects_elem : rects) {
    // rects_elem
    {
      auto& x = rects_elem.x;
      auto& y = rects_elem.y;
      auto& width = rects_elem.width;
      auto& height = rects_elem.height;

      // x
      buf.Write(&x);

      // y
      buf.Write(&y);

      // width
      buf.Write(&width);

      // height
      buf.Write(&height);
    }
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Render::FillRectangles", false);
}

Future<void> Render::FillRectangles(const PictOp& op,
                                    const Picture& dst,
                                    const Color& color,
                                    const std::vector<Rectangle>& rects) {
  return Render::FillRectangles(
      Render::FillRectanglesRequest{op, dst, color, rects});
}

Future<void> Render::CreateCursor(const Render::CreateCursorRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& cid = request.cid;
  auto& source = request.source;
  auto& x = request.x;
  auto& y = request.y;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 27;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // cid
  buf.Write(&cid);

  // source
  buf.Write(&source);

  // x
  buf.Write(&x);

  // y
  buf.Write(&y);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Render::CreateCursor", false);
}

Future<void> Render::CreateCursor(const Cursor& cid,
                                  const Picture& source,
                                  const uint16_t& x,
                                  const uint16_t& y) {
  return Render::CreateCursor(Render::CreateCursorRequest{cid, source, x, y});
}

Future<void> Render::SetPictureTransform(
    const Render::SetPictureTransformRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& picture = request.picture;
  auto& transform = request.transform;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 28;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // picture
  buf.Write(&picture);

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

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Render::SetPictureTransform",
                                        false);
}

Future<void> Render::SetPictureTransform(const Picture& picture,
                                         const Transform& transform) {
  return Render::SetPictureTransform(
      Render::SetPictureTransformRequest{picture, transform});
}

Future<Render::QueryFiltersReply> Render::QueryFilters(
    const Render::QueryFiltersRequest& request) {
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

  return connection_->SendRequest<Render::QueryFiltersReply>(
      &buf, "Render::QueryFilters", false);
}

Future<Render::QueryFiltersReply> Render::QueryFilters(
    const Drawable& drawable) {
  return Render::QueryFilters(Render::QueryFiltersRequest{drawable});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Render::QueryFiltersReply> detail::ReadReply<
    Render::QueryFiltersReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Render::QueryFiltersReply>();

  auto& sequence = (*reply).sequence;
  uint32_t num_aliases{};
  uint32_t num_filters{};
  auto& aliases = (*reply).aliases;
  auto& filters = (*reply).filters;

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

  // num_aliases
  Read(&num_aliases, &buf);

  // num_filters
  Read(&num_filters, &buf);

  // pad1
  Pad(&buf, 16);

  // aliases
  aliases.resize(num_aliases);
  for (auto& aliases_elem : aliases) {
    // aliases_elem
    Read(&aliases_elem, &buf);
  }

  // filters
  filters.resize(num_filters);
  for (auto& filters_elem : filters) {
    // filters_elem
    {
      uint8_t name_len{};
      auto& name = filters_elem.name;

      // name_len
      Read(&name_len, &buf);

      // name
      name.resize(name_len);
      for (auto& name_elem : name) {
        // name_elem
        Read(&name_elem, &buf);
      }
    }
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Render::SetPictureFilter(
    const Render::SetPictureFilterRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& picture = request.picture;
  uint16_t filter_len{};
  auto& filter = request.filter;
  auto& values = request.values;
  size_t values_len = values.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 30;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // picture
  buf.Write(&picture);

  // filter_len
  filter_len = filter.size();
  buf.Write(&filter_len);

  // pad0
  Pad(&buf, 2);

  // filter
  CHECK_EQ(static_cast<size_t>(filter_len), filter.size());
  for (auto& filter_elem : filter) {
    // filter_elem
    buf.Write(&filter_elem);
  }

  // pad1
  Align(&buf, 4);

  // values
  CHECK_EQ(static_cast<size_t>(values_len), values.size());
  for (auto& values_elem : values) {
    // values_elem
    buf.Write(&values_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Render::SetPictureFilter",
                                        false);
}

Future<void> Render::SetPictureFilter(const Picture& picture,
                                      const std::string& filter,
                                      const std::vector<Fixed>& values) {
  return Render::SetPictureFilter(
      Render::SetPictureFilterRequest{picture, filter, values});
}

Future<void> Render::CreateAnimCursor(
    const Render::CreateAnimCursorRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& cid = request.cid;
  auto& cursors = request.cursors;
  size_t cursors_len = cursors.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 31;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // cid
  buf.Write(&cid);

  // cursors
  CHECK_EQ(static_cast<size_t>(cursors_len), cursors.size());
  for (auto& cursors_elem : cursors) {
    // cursors_elem
    {
      auto& cursor = cursors_elem.cursor;
      auto& delay = cursors_elem.delay;

      // cursor
      buf.Write(&cursor);

      // delay
      buf.Write(&delay);
    }
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Render::CreateAnimCursor",
                                        false);
}

Future<void> Render::CreateAnimCursor(
    const Cursor& cid,
    const std::vector<AnimationCursorElement>& cursors) {
  return Render::CreateAnimCursor(
      Render::CreateAnimCursorRequest{cid, cursors});
}

Future<void> Render::AddTraps(const Render::AddTrapsRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& picture = request.picture;
  auto& x_off = request.x_off;
  auto& y_off = request.y_off;
  auto& traps = request.traps;
  size_t traps_len = traps.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 32;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // picture
  buf.Write(&picture);

  // x_off
  buf.Write(&x_off);

  // y_off
  buf.Write(&y_off);

  // traps
  CHECK_EQ(static_cast<size_t>(traps_len), traps.size());
  for (auto& traps_elem : traps) {
    // traps_elem
    {
      auto& top = traps_elem.top;
      auto& bot = traps_elem.bot;

      // top
      {
        auto& l = top.l;
        auto& r = top.r;
        auto& y = top.y;

        // l
        buf.Write(&l);

        // r
        buf.Write(&r);

        // y
        buf.Write(&y);
      }

      // bot
      {
        auto& l = bot.l;
        auto& r = bot.r;
        auto& y = bot.y;

        // l
        buf.Write(&l);

        // r
        buf.Write(&r);

        // y
        buf.Write(&y);
      }
    }
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Render::AddTraps", false);
}

Future<void> Render::AddTraps(const Picture& picture,
                              const int16_t& x_off,
                              const int16_t& y_off,
                              const std::vector<Trap>& traps) {
  return Render::AddTraps(
      Render::AddTrapsRequest{picture, x_off, y_off, traps});
}

Future<void> Render::CreateSolidFill(
    const Render::CreateSolidFillRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& picture = request.picture;
  auto& color = request.color;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 33;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // picture
  buf.Write(&picture);

  // color
  {
    auto& red = color.red;
    auto& green = color.green;
    auto& blue = color.blue;
    auto& alpha = color.alpha;

    // red
    buf.Write(&red);

    // green
    buf.Write(&green);

    // blue
    buf.Write(&blue);

    // alpha
    buf.Write(&alpha);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Render::CreateSolidFill", false);
}

Future<void> Render::CreateSolidFill(const Picture& picture,
                                     const Color& color) {
  return Render::CreateSolidFill(
      Render::CreateSolidFillRequest{picture, color});
}

Future<void> Render::CreateLinearGradient(
    const Render::CreateLinearGradientRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& picture = request.picture;
  auto& p1 = request.p1;
  auto& p2 = request.p2;
  uint32_t num_stops{};
  auto& stops = request.stops;
  size_t stops_len = stops.size();
  auto& colors = request.colors;
  size_t colors_len = colors.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 34;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // picture
  buf.Write(&picture);

  // p1
  {
    auto& x = p1.x;
    auto& y = p1.y;

    // x
    buf.Write(&x);

    // y
    buf.Write(&y);
  }

  // p2
  {
    auto& x = p2.x;
    auto& y = p2.y;

    // x
    buf.Write(&x);

    // y
    buf.Write(&y);
  }

  // num_stops
  num_stops = colors.size();
  buf.Write(&num_stops);

  // stops
  CHECK_EQ(static_cast<size_t>(num_stops), stops.size());
  for (auto& stops_elem : stops) {
    // stops_elem
    buf.Write(&stops_elem);
  }

  // colors
  CHECK_EQ(static_cast<size_t>(num_stops), colors.size());
  for (auto& colors_elem : colors) {
    // colors_elem
    {
      auto& red = colors_elem.red;
      auto& green = colors_elem.green;
      auto& blue = colors_elem.blue;
      auto& alpha = colors_elem.alpha;

      // red
      buf.Write(&red);

      // green
      buf.Write(&green);

      // blue
      buf.Write(&blue);

      // alpha
      buf.Write(&alpha);
    }
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Render::CreateLinearGradient",
                                        false);
}

Future<void> Render::CreateLinearGradient(const Picture& picture,
                                          const PointFix& p1,
                                          const PointFix& p2,
                                          const std::vector<Fixed>& stops,
                                          const std::vector<Color>& colors) {
  return Render::CreateLinearGradient(
      Render::CreateLinearGradientRequest{picture, p1, p2, stops, colors});
}

Future<void> Render::CreateRadialGradient(
    const Render::CreateRadialGradientRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& picture = request.picture;
  auto& inner = request.inner;
  auto& outer = request.outer;
  auto& inner_radius = request.inner_radius;
  auto& outer_radius = request.outer_radius;
  uint32_t num_stops{};
  auto& stops = request.stops;
  size_t stops_len = stops.size();
  auto& colors = request.colors;
  size_t colors_len = colors.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 35;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // picture
  buf.Write(&picture);

  // inner
  {
    auto& x = inner.x;
    auto& y = inner.y;

    // x
    buf.Write(&x);

    // y
    buf.Write(&y);
  }

  // outer
  {
    auto& x = outer.x;
    auto& y = outer.y;

    // x
    buf.Write(&x);

    // y
    buf.Write(&y);
  }

  // inner_radius
  buf.Write(&inner_radius);

  // outer_radius
  buf.Write(&outer_radius);

  // num_stops
  num_stops = colors.size();
  buf.Write(&num_stops);

  // stops
  CHECK_EQ(static_cast<size_t>(num_stops), stops.size());
  for (auto& stops_elem : stops) {
    // stops_elem
    buf.Write(&stops_elem);
  }

  // colors
  CHECK_EQ(static_cast<size_t>(num_stops), colors.size());
  for (auto& colors_elem : colors) {
    // colors_elem
    {
      auto& red = colors_elem.red;
      auto& green = colors_elem.green;
      auto& blue = colors_elem.blue;
      auto& alpha = colors_elem.alpha;

      // red
      buf.Write(&red);

      // green
      buf.Write(&green);

      // blue
      buf.Write(&blue);

      // alpha
      buf.Write(&alpha);
    }
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Render::CreateRadialGradient",
                                        false);
}

Future<void> Render::CreateRadialGradient(const Picture& picture,
                                          const PointFix& inner,
                                          const PointFix& outer,
                                          const Fixed& inner_radius,
                                          const Fixed& outer_radius,
                                          const std::vector<Fixed>& stops,
                                          const std::vector<Color>& colors) {
  return Render::CreateRadialGradient(Render::CreateRadialGradientRequest{
      picture, inner, outer, inner_radius, outer_radius, stops, colors});
}

Future<void> Render::CreateConicalGradient(
    const Render::CreateConicalGradientRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& picture = request.picture;
  auto& center = request.center;
  auto& angle = request.angle;
  uint32_t num_stops{};
  auto& stops = request.stops;
  size_t stops_len = stops.size();
  auto& colors = request.colors;
  size_t colors_len = colors.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 36;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // picture
  buf.Write(&picture);

  // center
  {
    auto& x = center.x;
    auto& y = center.y;

    // x
    buf.Write(&x);

    // y
    buf.Write(&y);
  }

  // angle
  buf.Write(&angle);

  // num_stops
  num_stops = colors.size();
  buf.Write(&num_stops);

  // stops
  CHECK_EQ(static_cast<size_t>(num_stops), stops.size());
  for (auto& stops_elem : stops) {
    // stops_elem
    buf.Write(&stops_elem);
  }

  // colors
  CHECK_EQ(static_cast<size_t>(num_stops), colors.size());
  for (auto& colors_elem : colors) {
    // colors_elem
    {
      auto& red = colors_elem.red;
      auto& green = colors_elem.green;
      auto& blue = colors_elem.blue;
      auto& alpha = colors_elem.alpha;

      // red
      buf.Write(&red);

      // green
      buf.Write(&green);

      // blue
      buf.Write(&blue);

      // alpha
      buf.Write(&alpha);
    }
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Render::CreateConicalGradient",
                                        false);
}

Future<void> Render::CreateConicalGradient(const Picture& picture,
                                           const PointFix& center,
                                           const Fixed& angle,
                                           const std::vector<Fixed>& stops,
                                           const std::vector<Color>& colors) {
  return Render::CreateConicalGradient(Render::CreateConicalGradientRequest{
      picture, center, angle, stops, colors});
}

}  // namespace x11
