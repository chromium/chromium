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

#include "xfixes.h"

#include <unistd.h>
#include <xcb/xcb.h>
#include <xcb/xcbext.h>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/xproto_internal.h"

namespace x11 {

XFixes::XFixes(Connection* connection, const x11::QueryExtensionReply& info)
    : connection_(connection), info_(info) {}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<XFixes::SelectionNotifyEvent>(
    XFixes::SelectionNotifyEvent* event_,
    ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& subtype = (*event_).subtype;
  auto& sequence = (*event_).sequence;
  auto& window = (*event_).window;
  auto& owner = (*event_).owner;
  auto& selection = (*event_).selection;
  auto& timestamp = (*event_).timestamp;
  auto& selection_timestamp = (*event_).selection_timestamp;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // subtype
  uint8_t tmp0;
  Read(&tmp0, &buf);
  subtype = static_cast<XFixes::SelectionEvent>(tmp0);

  // sequence
  Read(&sequence, &buf);

  // window
  Read(&window, &buf);

  // owner
  Read(&owner, &buf);

  // selection
  Read(&selection, &buf);

  // timestamp
  Read(&timestamp, &buf);

  // selection_timestamp
  Read(&selection_timestamp, &buf);

  // pad0
  Pad(&buf, 8);

  CHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<XFixes::CursorNotifyEvent>(XFixes::CursorNotifyEvent* event_,
                                          ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& subtype = (*event_).subtype;
  auto& sequence = (*event_).sequence;
  auto& window = (*event_).window;
  auto& cursor_serial = (*event_).cursor_serial;
  auto& timestamp = (*event_).timestamp;
  auto& name = (*event_).name;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // subtype
  uint8_t tmp1;
  Read(&tmp1, &buf);
  subtype = static_cast<XFixes::CursorNotify>(tmp1);

  // sequence
  Read(&sequence, &buf);

  // window
  Read(&window, &buf);

  // cursor_serial
  Read(&cursor_serial, &buf);

  // timestamp
  Read(&timestamp, &buf);

  // name
  Read(&name, &buf);

  // pad0
  Pad(&buf, 12);

  CHECK_LE(buf.offset, 32ul);
}

std::string XFixes::BadRegionError::ToString() const {
  std::stringstream ss_;
  ss_ << "XFixes::BadRegionError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<XFixes::BadRegionError>(XFixes::BadRegionError* error_,
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

Future<XFixes::QueryVersionReply> XFixes::QueryVersion(
    const XFixes::QueryVersionRequest& request) {
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

  return connection_->SendRequest<XFixes::QueryVersionReply>(
      &buf, "XFixes::QueryVersion", false);
}

Future<XFixes::QueryVersionReply> XFixes::QueryVersion(
    const uint32_t& client_major_version,
    const uint32_t& client_minor_version) {
  return XFixes::QueryVersion(
      XFixes::QueryVersionRequest{client_major_version, client_minor_version});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<XFixes::QueryVersionReply> detail::ReadReply<
    XFixes::QueryVersionReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<XFixes::QueryVersionReply>();

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

Future<void> XFixes::ChangeSaveSet(
    const XFixes::ChangeSaveSetRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& mode = request.mode;
  auto& target = request.target;
  auto& map = request.map;
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

  // mode
  uint8_t tmp2;
  tmp2 = static_cast<uint8_t>(mode);
  buf.Write(&tmp2);

  // target
  uint8_t tmp3;
  tmp3 = static_cast<uint8_t>(target);
  buf.Write(&tmp3);

  // map
  uint8_t tmp4;
  tmp4 = static_cast<uint8_t>(map);
  buf.Write(&tmp4);

  // pad0
  Pad(&buf, 1);

  // window
  buf.Write(&window);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "XFixes::ChangeSaveSet", false);
}

Future<void> XFixes::ChangeSaveSet(const SaveSetMode& mode,
                                   const SaveSetTarget& target,
                                   const SaveSetMapping& map,
                                   const Window& window) {
  return XFixes::ChangeSaveSet(
      XFixes::ChangeSaveSetRequest{mode, target, map, window});
}

Future<void> XFixes::SelectSelectionInput(
    const XFixes::SelectSelectionInputRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& window = request.window;
  auto& selection = request.selection;
  auto& event_mask = request.event_mask;

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

  // selection
  buf.Write(&selection);

  // event_mask
  uint32_t tmp5;
  tmp5 = static_cast<uint32_t>(event_mask);
  buf.Write(&tmp5);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "XFixes::SelectSelectionInput",
                                        false);
}

Future<void> XFixes::SelectSelectionInput(
    const Window& window,
    const Atom& selection,
    const SelectionEventMask& event_mask) {
  return XFixes::SelectSelectionInput(
      XFixes::SelectSelectionInputRequest{window, selection, event_mask});
}

Future<void> XFixes::SelectCursorInput(
    const XFixes::SelectCursorInputRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& window = request.window;
  auto& event_mask = request.event_mask;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 3;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // window
  buf.Write(&window);

  // event_mask
  uint32_t tmp6;
  tmp6 = static_cast<uint32_t>(event_mask);
  buf.Write(&tmp6);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "XFixes::SelectCursorInput",
                                        false);
}

Future<void> XFixes::SelectCursorInput(const Window& window,
                                       const CursorNotifyMask& event_mask) {
  return XFixes::SelectCursorInput(
      XFixes::SelectCursorInputRequest{window, event_mask});
}

Future<XFixes::GetCursorImageReply> XFixes::GetCursorImage(
    const XFixes::GetCursorImageRequest& request) {
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

  return connection_->SendRequest<XFixes::GetCursorImageReply>(
      &buf, "XFixes::GetCursorImage", false);
}

Future<XFixes::GetCursorImageReply> XFixes::GetCursorImage() {
  return XFixes::GetCursorImage(XFixes::GetCursorImageRequest{});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<XFixes::GetCursorImageReply> detail::ReadReply<
    XFixes::GetCursorImageReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<XFixes::GetCursorImageReply>();

  auto& sequence = (*reply).sequence;
  auto& x = (*reply).x;
  auto& y = (*reply).y;
  auto& width = (*reply).width;
  auto& height = (*reply).height;
  auto& xhot = (*reply).xhot;
  auto& yhot = (*reply).yhot;
  auto& cursor_serial = (*reply).cursor_serial;
  auto& cursor_image = (*reply).cursor_image;

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

  // width
  Read(&width, &buf);

  // height
  Read(&height, &buf);

  // xhot
  Read(&xhot, &buf);

  // yhot
  Read(&yhot, &buf);

  // cursor_serial
  Read(&cursor_serial, &buf);

  // pad1
  Pad(&buf, 8);

  // cursor_image
  cursor_image.resize((width) * (height));
  for (auto& cursor_image_elem : cursor_image) {
    // cursor_image_elem
    Read(&cursor_image_elem, &buf);
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> XFixes::CreateRegion(const XFixes::CreateRegionRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& region = request.region;
  auto& rectangles = request.rectangles;
  size_t rectangles_len = rectangles.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 5;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // region
  buf.Write(&region);

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

  return connection_->SendRequest<void>(&buf, "XFixes::CreateRegion", false);
}

Future<void> XFixes::CreateRegion(const Region& region,
                                  const std::vector<Rectangle>& rectangles) {
  return XFixes::CreateRegion(XFixes::CreateRegionRequest{region, rectangles});
}

Future<void> XFixes::CreateRegionFromBitmap(
    const XFixes::CreateRegionFromBitmapRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& region = request.region;
  auto& bitmap = request.bitmap;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 6;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // region
  buf.Write(&region);

  // bitmap
  buf.Write(&bitmap);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "XFixes::CreateRegionFromBitmap",
                                        false);
}

Future<void> XFixes::CreateRegionFromBitmap(const Region& region,
                                            const Pixmap& bitmap) {
  return XFixes::CreateRegionFromBitmap(
      XFixes::CreateRegionFromBitmapRequest{region, bitmap});
}

Future<void> XFixes::CreateRegionFromWindow(
    const XFixes::CreateRegionFromWindowRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& region = request.region;
  auto& window = request.window;
  auto& kind = request.kind;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 7;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // region
  buf.Write(&region);

  // window
  buf.Write(&window);

  // kind
  uint8_t tmp7;
  tmp7 = static_cast<uint8_t>(kind);
  buf.Write(&tmp7);

  // pad0
  Pad(&buf, 3);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "XFixes::CreateRegionFromWindow",
                                        false);
}

Future<void> XFixes::CreateRegionFromWindow(const Region& region,
                                            const Window& window,
                                            const Shape::Sk& kind) {
  return XFixes::CreateRegionFromWindow(
      XFixes::CreateRegionFromWindowRequest{region, window, kind});
}

Future<void> XFixes::CreateRegionFromGC(
    const XFixes::CreateRegionFromGCRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& region = request.region;
  auto& gc = request.gc;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 8;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // region
  buf.Write(&region);

  // gc
  buf.Write(&gc);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "XFixes::CreateRegionFromGC",
                                        false);
}

Future<void> XFixes::CreateRegionFromGC(const Region& region,
                                        const GraphicsContext& gc) {
  return XFixes::CreateRegionFromGC(
      XFixes::CreateRegionFromGCRequest{region, gc});
}

Future<void> XFixes::CreateRegionFromPicture(
    const XFixes::CreateRegionFromPictureRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& region = request.region;
  auto& picture = request.picture;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 9;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // region
  buf.Write(&region);

  // picture
  buf.Write(&picture);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "XFixes::CreateRegionFromPicture",
                                        false);
}

Future<void> XFixes::CreateRegionFromPicture(const Region& region,
                                             const Render::Picture& picture) {
  return XFixes::CreateRegionFromPicture(
      XFixes::CreateRegionFromPictureRequest{region, picture});
}

Future<void> XFixes::DestroyRegion(
    const XFixes::DestroyRegionRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& region = request.region;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 10;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // region
  buf.Write(&region);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "XFixes::DestroyRegion", false);
}

Future<void> XFixes::DestroyRegion(const Region& region) {
  return XFixes::DestroyRegion(XFixes::DestroyRegionRequest{region});
}

Future<void> XFixes::SetRegion(const XFixes::SetRegionRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& region = request.region;
  auto& rectangles = request.rectangles;
  size_t rectangles_len = rectangles.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 11;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // region
  buf.Write(&region);

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

  return connection_->SendRequest<void>(&buf, "XFixes::SetRegion", false);
}

Future<void> XFixes::SetRegion(const Region& region,
                               const std::vector<Rectangle>& rectangles) {
  return XFixes::SetRegion(XFixes::SetRegionRequest{region, rectangles});
}

Future<void> XFixes::CopyRegion(const XFixes::CopyRegionRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& source = request.source;
  auto& destination = request.destination;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 12;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // source
  buf.Write(&source);

  // destination
  buf.Write(&destination);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "XFixes::CopyRegion", false);
}

Future<void> XFixes::CopyRegion(const Region& source,
                                const Region& destination) {
  return XFixes::CopyRegion(XFixes::CopyRegionRequest{source, destination});
}

Future<void> XFixes::UnionRegion(const XFixes::UnionRegionRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& source1 = request.source1;
  auto& source2 = request.source2;
  auto& destination = request.destination;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 13;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // source1
  buf.Write(&source1);

  // source2
  buf.Write(&source2);

  // destination
  buf.Write(&destination);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "XFixes::UnionRegion", false);
}

Future<void> XFixes::UnionRegion(const Region& source1,
                                 const Region& source2,
                                 const Region& destination) {
  return XFixes::UnionRegion(
      XFixes::UnionRegionRequest{source1, source2, destination});
}

Future<void> XFixes::IntersectRegion(
    const XFixes::IntersectRegionRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& source1 = request.source1;
  auto& source2 = request.source2;
  auto& destination = request.destination;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 14;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // source1
  buf.Write(&source1);

  // source2
  buf.Write(&source2);

  // destination
  buf.Write(&destination);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "XFixes::IntersectRegion", false);
}

Future<void> XFixes::IntersectRegion(const Region& source1,
                                     const Region& source2,
                                     const Region& destination) {
  return XFixes::IntersectRegion(
      XFixes::IntersectRegionRequest{source1, source2, destination});
}

Future<void> XFixes::SubtractRegion(
    const XFixes::SubtractRegionRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& source1 = request.source1;
  auto& source2 = request.source2;
  auto& destination = request.destination;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 15;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // source1
  buf.Write(&source1);

  // source2
  buf.Write(&source2);

  // destination
  buf.Write(&destination);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "XFixes::SubtractRegion", false);
}

Future<void> XFixes::SubtractRegion(const Region& source1,
                                    const Region& source2,
                                    const Region& destination) {
  return XFixes::SubtractRegion(
      XFixes::SubtractRegionRequest{source1, source2, destination});
}

Future<void> XFixes::InvertRegion(const XFixes::InvertRegionRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& source = request.source;
  auto& bounds = request.bounds;
  auto& destination = request.destination;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 16;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // source
  buf.Write(&source);

  // bounds
  {
    auto& x = bounds.x;
    auto& y = bounds.y;
    auto& width = bounds.width;
    auto& height = bounds.height;

    // x
    buf.Write(&x);

    // y
    buf.Write(&y);

    // width
    buf.Write(&width);

    // height
    buf.Write(&height);
  }

  // destination
  buf.Write(&destination);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "XFixes::InvertRegion", false);
}

Future<void> XFixes::InvertRegion(const Region& source,
                                  const Rectangle& bounds,
                                  const Region& destination) {
  return XFixes::InvertRegion(
      XFixes::InvertRegionRequest{source, bounds, destination});
}

Future<void> XFixes::TranslateRegion(
    const XFixes::TranslateRegionRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& region = request.region;
  auto& dx = request.dx;
  auto& dy = request.dy;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 17;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // region
  buf.Write(&region);

  // dx
  buf.Write(&dx);

  // dy
  buf.Write(&dy);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "XFixes::TranslateRegion", false);
}

Future<void> XFixes::TranslateRegion(const Region& region,
                                     const int16_t& dx,
                                     const int16_t& dy) {
  return XFixes::TranslateRegion(
      XFixes::TranslateRegionRequest{region, dx, dy});
}

Future<void> XFixes::RegionExtents(
    const XFixes::RegionExtentsRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& source = request.source;
  auto& destination = request.destination;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 18;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // source
  buf.Write(&source);

  // destination
  buf.Write(&destination);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "XFixes::RegionExtents", false);
}

Future<void> XFixes::RegionExtents(const Region& source,
                                   const Region& destination) {
  return XFixes::RegionExtents(
      XFixes::RegionExtentsRequest{source, destination});
}

Future<XFixes::FetchRegionReply> XFixes::FetchRegion(
    const XFixes::FetchRegionRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& region = request.region;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 19;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // region
  buf.Write(&region);

  Align(&buf, 4);

  return connection_->SendRequest<XFixes::FetchRegionReply>(
      &buf, "XFixes::FetchRegion", false);
}

Future<XFixes::FetchRegionReply> XFixes::FetchRegion(const Region& region) {
  return XFixes::FetchRegion(XFixes::FetchRegionRequest{region});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<XFixes::FetchRegionReply> detail::ReadReply<
    XFixes::FetchRegionReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<XFixes::FetchRegionReply>();

  auto& sequence = (*reply).sequence;
  auto& extents = (*reply).extents;
  auto& rectangles = (*reply).rectangles;

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

  // extents
  {
    auto& x = extents.x;
    auto& y = extents.y;
    auto& width = extents.width;
    auto& height = extents.height;

    // x
    Read(&x, &buf);

    // y
    Read(&y, &buf);

    // width
    Read(&width, &buf);

    // height
    Read(&height, &buf);
  }

  // pad1
  Pad(&buf, 16);

  // rectangles
  rectangles.resize((length) / (2));
  for (auto& rectangles_elem : rectangles) {
    // rectangles_elem
    {
      auto& x = rectangles_elem.x;
      auto& y = rectangles_elem.y;
      auto& width = rectangles_elem.width;
      auto& height = rectangles_elem.height;

      // x
      Read(&x, &buf);

      // y
      Read(&y, &buf);

      // width
      Read(&width, &buf);

      // height
      Read(&height, &buf);
    }
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> XFixes::SetGCClipRegion(
    const XFixes::SetGCClipRegionRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& gc = request.gc;
  auto& region = request.region;
  auto& x_origin = request.x_origin;
  auto& y_origin = request.y_origin;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 20;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // gc
  buf.Write(&gc);

  // region
  buf.Write(&region);

  // x_origin
  buf.Write(&x_origin);

  // y_origin
  buf.Write(&y_origin);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "XFixes::SetGCClipRegion", false);
}

Future<void> XFixes::SetGCClipRegion(const GraphicsContext& gc,
                                     const Region& region,
                                     const int16_t& x_origin,
                                     const int16_t& y_origin) {
  return XFixes::SetGCClipRegion(
      XFixes::SetGCClipRegionRequest{gc, region, x_origin, y_origin});
}

Future<void> XFixes::SetWindowShapeRegion(
    const XFixes::SetWindowShapeRegionRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& dest = request.dest;
  auto& dest_kind = request.dest_kind;
  auto& x_offset = request.x_offset;
  auto& y_offset = request.y_offset;
  auto& region = request.region;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 21;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // dest
  buf.Write(&dest);

  // dest_kind
  uint8_t tmp8;
  tmp8 = static_cast<uint8_t>(dest_kind);
  buf.Write(&tmp8);

  // pad0
  Pad(&buf, 3);

  // x_offset
  buf.Write(&x_offset);

  // y_offset
  buf.Write(&y_offset);

  // region
  buf.Write(&region);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "XFixes::SetWindowShapeRegion",
                                        false);
}

Future<void> XFixes::SetWindowShapeRegion(const Window& dest,
                                          const Shape::Sk& dest_kind,
                                          const int16_t& x_offset,
                                          const int16_t& y_offset,
                                          const Region& region) {
  return XFixes::SetWindowShapeRegion(XFixes::SetWindowShapeRegionRequest{
      dest, dest_kind, x_offset, y_offset, region});
}

Future<void> XFixes::SetPictureClipRegion(
    const XFixes::SetPictureClipRegionRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& picture = request.picture;
  auto& region = request.region;
  auto& x_origin = request.x_origin;
  auto& y_origin = request.y_origin;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 22;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // picture
  buf.Write(&picture);

  // region
  buf.Write(&region);

  // x_origin
  buf.Write(&x_origin);

  // y_origin
  buf.Write(&y_origin);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "XFixes::SetPictureClipRegion",
                                        false);
}

Future<void> XFixes::SetPictureClipRegion(const Render::Picture& picture,
                                          const Region& region,
                                          const int16_t& x_origin,
                                          const int16_t& y_origin) {
  return XFixes::SetPictureClipRegion(
      XFixes::SetPictureClipRegionRequest{picture, region, x_origin, y_origin});
}

Future<void> XFixes::SetCursorName(
    const XFixes::SetCursorNameRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& cursor = request.cursor;
  uint16_t nbytes{};
  auto& name = request.name;
  size_t name_len = name.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 23;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // cursor
  buf.Write(&cursor);

  // nbytes
  nbytes = name.size();
  buf.Write(&nbytes);

  // pad0
  Pad(&buf, 2);

  // name
  CHECK_EQ(static_cast<size_t>(nbytes), name.size());
  for (auto& name_elem : name) {
    // name_elem
    buf.Write(&name_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "XFixes::SetCursorName", false);
}

Future<void> XFixes::SetCursorName(const Cursor& cursor,
                                   const std::string& name) {
  return XFixes::SetCursorName(XFixes::SetCursorNameRequest{cursor, name});
}

Future<XFixes::GetCursorNameReply> XFixes::GetCursorName(
    const XFixes::GetCursorNameRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& cursor = request.cursor;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 24;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // cursor
  buf.Write(&cursor);

  Align(&buf, 4);

  return connection_->SendRequest<XFixes::GetCursorNameReply>(
      &buf, "XFixes::GetCursorName", false);
}

Future<XFixes::GetCursorNameReply> XFixes::GetCursorName(const Cursor& cursor) {
  return XFixes::GetCursorName(XFixes::GetCursorNameRequest{cursor});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<XFixes::GetCursorNameReply> detail::ReadReply<
    XFixes::GetCursorNameReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<XFixes::GetCursorNameReply>();

  auto& sequence = (*reply).sequence;
  auto& atom = (*reply).atom;
  uint16_t nbytes{};
  auto& name = (*reply).name;

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

  // atom
  Read(&atom, &buf);

  // nbytes
  Read(&nbytes, &buf);

  // pad1
  Pad(&buf, 18);

  // name
  name.resize(nbytes);
  for (auto& name_elem : name) {
    // name_elem
    Read(&name_elem, &buf);
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<XFixes::GetCursorImageAndNameReply> XFixes::GetCursorImageAndName(
    const XFixes::GetCursorImageAndNameRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 25;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  Align(&buf, 4);

  return connection_->SendRequest<XFixes::GetCursorImageAndNameReply>(
      &buf, "XFixes::GetCursorImageAndName", false);
}

Future<XFixes::GetCursorImageAndNameReply> XFixes::GetCursorImageAndName() {
  return XFixes::GetCursorImageAndName(XFixes::GetCursorImageAndNameRequest{});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<XFixes::GetCursorImageAndNameReply> detail::ReadReply<
    XFixes::GetCursorImageAndNameReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<XFixes::GetCursorImageAndNameReply>();

  auto& sequence = (*reply).sequence;
  auto& x = (*reply).x;
  auto& y = (*reply).y;
  auto& width = (*reply).width;
  auto& height = (*reply).height;
  auto& xhot = (*reply).xhot;
  auto& yhot = (*reply).yhot;
  auto& cursor_serial = (*reply).cursor_serial;
  auto& cursor_atom = (*reply).cursor_atom;
  uint16_t nbytes{};
  auto& cursor_image = (*reply).cursor_image;
  auto& name = (*reply).name;

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

  // width
  Read(&width, &buf);

  // height
  Read(&height, &buf);

  // xhot
  Read(&xhot, &buf);

  // yhot
  Read(&yhot, &buf);

  // cursor_serial
  Read(&cursor_serial, &buf);

  // cursor_atom
  Read(&cursor_atom, &buf);

  // nbytes
  Read(&nbytes, &buf);

  // pad1
  Pad(&buf, 2);

  // cursor_image
  cursor_image.resize((width) * (height));
  for (auto& cursor_image_elem : cursor_image) {
    // cursor_image_elem
    Read(&cursor_image_elem, &buf);
  }

  // name
  name.resize(nbytes);
  for (auto& name_elem : name) {
    // name_elem
    Read(&name_elem, &buf);
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> XFixes::ChangeCursor(const XFixes::ChangeCursorRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& source = request.source;
  auto& destination = request.destination;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 26;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // source
  buf.Write(&source);

  // destination
  buf.Write(&destination);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "XFixes::ChangeCursor", false);
}

Future<void> XFixes::ChangeCursor(const Cursor& source,
                                  const Cursor& destination) {
  return XFixes::ChangeCursor(XFixes::ChangeCursorRequest{source, destination});
}

Future<void> XFixes::ChangeCursorByName(
    const XFixes::ChangeCursorByNameRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& src = request.src;
  uint16_t nbytes{};
  auto& name = request.name;
  size_t name_len = name.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 27;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // src
  buf.Write(&src);

  // nbytes
  nbytes = name.size();
  buf.Write(&nbytes);

  // pad0
  Pad(&buf, 2);

  // name
  CHECK_EQ(static_cast<size_t>(nbytes), name.size());
  for (auto& name_elem : name) {
    // name_elem
    buf.Write(&name_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "XFixes::ChangeCursorByName",
                                        false);
}

Future<void> XFixes::ChangeCursorByName(const Cursor& src,
                                        const std::string& name) {
  return XFixes::ChangeCursorByName(
      XFixes::ChangeCursorByNameRequest{src, name});
}

Future<void> XFixes::ExpandRegion(const XFixes::ExpandRegionRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& source = request.source;
  auto& destination = request.destination;
  auto& left = request.left;
  auto& right = request.right;
  auto& top = request.top;
  auto& bottom = request.bottom;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 28;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // source
  buf.Write(&source);

  // destination
  buf.Write(&destination);

  // left
  buf.Write(&left);

  // right
  buf.Write(&right);

  // top
  buf.Write(&top);

  // bottom
  buf.Write(&bottom);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "XFixes::ExpandRegion", false);
}

Future<void> XFixes::ExpandRegion(const Region& source,
                                  const Region& destination,
                                  const uint16_t& left,
                                  const uint16_t& right,
                                  const uint16_t& top,
                                  const uint16_t& bottom) {
  return XFixes::ExpandRegion(XFixes::ExpandRegionRequest{
      source, destination, left, right, top, bottom});
}

Future<void> XFixes::HideCursor(const XFixes::HideCursorRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& window = request.window;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 29;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // window
  buf.Write(&window);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "XFixes::HideCursor", false);
}

Future<void> XFixes::HideCursor(const Window& window) {
  return XFixes::HideCursor(XFixes::HideCursorRequest{window});
}

Future<void> XFixes::ShowCursor(const XFixes::ShowCursorRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& window = request.window;

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

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "XFixes::ShowCursor", false);
}

Future<void> XFixes::ShowCursor(const Window& window) {
  return XFixes::ShowCursor(XFixes::ShowCursorRequest{window});
}

Future<void> XFixes::CreatePointerBarrier(
    const XFixes::CreatePointerBarrierRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& barrier = request.barrier;
  auto& window = request.window;
  auto& x1 = request.x1;
  auto& y1 = request.y1;
  auto& x2 = request.x2;
  auto& y2 = request.y2;
  auto& directions = request.directions;
  uint16_t num_devices{};
  auto& devices = request.devices;
  size_t devices_len = devices.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 31;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // barrier
  buf.Write(&barrier);

  // window
  buf.Write(&window);

  // x1
  buf.Write(&x1);

  // y1
  buf.Write(&y1);

  // x2
  buf.Write(&x2);

  // y2
  buf.Write(&y2);

  // directions
  uint32_t tmp9;
  tmp9 = static_cast<uint32_t>(directions);
  buf.Write(&tmp9);

  // pad0
  Pad(&buf, 2);

  // num_devices
  num_devices = devices.size();
  buf.Write(&num_devices);

  // devices
  CHECK_EQ(static_cast<size_t>(num_devices), devices.size());
  for (auto& devices_elem : devices) {
    // devices_elem
    buf.Write(&devices_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "XFixes::CreatePointerBarrier",
                                        false);
}

Future<void> XFixes::CreatePointerBarrier(
    const Barrier& barrier,
    const Window& window,
    const uint16_t& x1,
    const uint16_t& y1,
    const uint16_t& x2,
    const uint16_t& y2,
    const BarrierDirections& directions,
    const std::vector<uint16_t>& devices) {
  return XFixes::CreatePointerBarrier(XFixes::CreatePointerBarrierRequest{
      barrier, window, x1, y1, x2, y2, directions, devices});
}

Future<void> XFixes::DeletePointerBarrier(
    const XFixes::DeletePointerBarrierRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& barrier = request.barrier;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 32;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // barrier
  buf.Write(&barrier);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "XFixes::DeletePointerBarrier",
                                        false);
}

Future<void> XFixes::DeletePointerBarrier(const Barrier& barrier) {
  return XFixes::DeletePointerBarrier(
      XFixes::DeletePointerBarrierRequest{barrier});
}

Future<void> XFixes::SetClientDisconnectMode(
    const XFixes::SetClientDisconnectModeRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& disconnect_mode = request.disconnect_mode;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 33;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // disconnect_mode
  uint32_t tmp10;
  tmp10 = static_cast<uint32_t>(disconnect_mode);
  buf.Write(&tmp10);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "XFixes::SetClientDisconnectMode",
                                        false);
}

Future<void> XFixes::SetClientDisconnectMode(
    const ClientDisconnectFlags& disconnect_mode) {
  return XFixes::SetClientDisconnectMode(
      XFixes::SetClientDisconnectModeRequest{disconnect_mode});
}

Future<XFixes::GetClientDisconnectModeReply> XFixes::GetClientDisconnectMode(
    const XFixes::GetClientDisconnectModeRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 34;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  Align(&buf, 4);

  return connection_->SendRequest<XFixes::GetClientDisconnectModeReply>(
      &buf, "XFixes::GetClientDisconnectMode", false);
}

Future<XFixes::GetClientDisconnectModeReply> XFixes::GetClientDisconnectMode() {
  return XFixes::GetClientDisconnectMode(
      XFixes::GetClientDisconnectModeRequest{});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<XFixes::GetClientDisconnectModeReply> detail::ReadReply<
    XFixes::GetClientDisconnectModeReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<XFixes::GetClientDisconnectModeReply>();

  auto& sequence = (*reply).sequence;
  auto& disconnect_mode = (*reply).disconnect_mode;

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

  // disconnect_mode
  uint32_t tmp11;
  Read(&tmp11, &buf);
  disconnect_mode = static_cast<XFixes::ClientDisconnectFlags>(tmp11);

  // pad1
  Pad(&buf, 20);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

}  // namespace x11
