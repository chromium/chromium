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

#include "shape.h"

#include <unistd.h>
#include <xcb/xcb.h>
#include <xcb/xcbext.h>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/xproto_internal.h"

namespace x11 {

Shape::Shape(Connection* connection, const x11::QueryExtensionReply& info)
    : connection_(connection), info_(info) {}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<Shape::NotifyEvent>(Shape::NotifyEvent* event_,
                                   ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& shape_kind = (*event_).shape_kind;
  auto& sequence = (*event_).sequence;
  auto& affected_window = (*event_).affected_window;
  auto& extents_x = (*event_).extents_x;
  auto& extents_y = (*event_).extents_y;
  auto& extents_width = (*event_).extents_width;
  auto& extents_height = (*event_).extents_height;
  auto& server_time = (*event_).server_time;
  auto& shaped = (*event_).shaped;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // shape_kind
  uint8_t tmp0;
  Read(&tmp0, &buf);
  shape_kind = static_cast<Shape::Sk>(tmp0);

  // sequence
  Read(&sequence, &buf);

  // affected_window
  Read(&affected_window, &buf);

  // extents_x
  Read(&extents_x, &buf);

  // extents_y
  Read(&extents_y, &buf);

  // extents_width
  Read(&extents_width, &buf);

  // extents_height
  Read(&extents_height, &buf);

  // server_time
  Read(&server_time, &buf);

  // shaped
  Read(&shaped, &buf);

  // pad0
  Pad(&buf, 11);

  CHECK_LE(buf.offset, 32ul);
}

Future<Shape::QueryVersionReply> Shape::QueryVersion(
    const Shape::QueryVersionRequest& request) {
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

  return connection_->SendRequest<Shape::QueryVersionReply>(
      &buf, "Shape::QueryVersion", false);
}

Future<Shape::QueryVersionReply> Shape::QueryVersion() {
  return Shape::QueryVersion(Shape::QueryVersionRequest{});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Shape::QueryVersionReply> detail::ReadReply<
    Shape::QueryVersionReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Shape::QueryVersionReply>();

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
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Shape::Rectangles(const Shape::RectanglesRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& operation = request.operation;
  auto& destination_kind = request.destination_kind;
  auto& ordering = request.ordering;
  auto& destination_window = request.destination_window;
  auto& x_offset = request.x_offset;
  auto& y_offset = request.y_offset;
  auto& rectangles = request.rectangles;
  size_t rectangles_len = rectangles.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 1;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // operation
  uint8_t tmp1;
  tmp1 = static_cast<uint8_t>(operation);
  buf.Write(&tmp1);

  // destination_kind
  uint8_t tmp2;
  tmp2 = static_cast<uint8_t>(destination_kind);
  buf.Write(&tmp2);

  // ordering
  uint8_t tmp3;
  tmp3 = static_cast<uint8_t>(ordering);
  buf.Write(&tmp3);

  // pad0
  Pad(&buf, 1);

  // destination_window
  buf.Write(&destination_window);

  // x_offset
  buf.Write(&x_offset);

  // y_offset
  buf.Write(&y_offset);

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

  return connection_->SendRequest<void>(&buf, "Shape::Rectangles", false);
}

Future<void> Shape::Rectangles(const So& operation,
                               const Sk& destination_kind,
                               const ClipOrdering& ordering,
                               const Window& destination_window,
                               const int16_t& x_offset,
                               const int16_t& y_offset,
                               const std::vector<Rectangle>& rectangles) {
  return Shape::Rectangles(Shape::RectanglesRequest{
      operation, destination_kind, ordering, destination_window, x_offset,
      y_offset, rectangles});
}

Future<void> Shape::Mask(const Shape::MaskRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& operation = request.operation;
  auto& destination_kind = request.destination_kind;
  auto& destination_window = request.destination_window;
  auto& x_offset = request.x_offset;
  auto& y_offset = request.y_offset;
  auto& source_bitmap = request.source_bitmap;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 2;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // operation
  uint8_t tmp4;
  tmp4 = static_cast<uint8_t>(operation);
  buf.Write(&tmp4);

  // destination_kind
  uint8_t tmp5;
  tmp5 = static_cast<uint8_t>(destination_kind);
  buf.Write(&tmp5);

  // pad0
  Pad(&buf, 2);

  // destination_window
  buf.Write(&destination_window);

  // x_offset
  buf.Write(&x_offset);

  // y_offset
  buf.Write(&y_offset);

  // source_bitmap
  buf.Write(&source_bitmap);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Shape::Mask", false);
}

Future<void> Shape::Mask(const So& operation,
                         const Sk& destination_kind,
                         const Window& destination_window,
                         const int16_t& x_offset,
                         const int16_t& y_offset,
                         const Pixmap& source_bitmap) {
  return Shape::Mask(Shape::MaskRequest{operation, destination_kind,
                                        destination_window, x_offset, y_offset,
                                        source_bitmap});
}

Future<void> Shape::Combine(const Shape::CombineRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& operation = request.operation;
  auto& destination_kind = request.destination_kind;
  auto& source_kind = request.source_kind;
  auto& destination_window = request.destination_window;
  auto& x_offset = request.x_offset;
  auto& y_offset = request.y_offset;
  auto& source_window = request.source_window;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 3;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // operation
  uint8_t tmp6;
  tmp6 = static_cast<uint8_t>(operation);
  buf.Write(&tmp6);

  // destination_kind
  uint8_t tmp7;
  tmp7 = static_cast<uint8_t>(destination_kind);
  buf.Write(&tmp7);

  // source_kind
  uint8_t tmp8;
  tmp8 = static_cast<uint8_t>(source_kind);
  buf.Write(&tmp8);

  // pad0
  Pad(&buf, 1);

  // destination_window
  buf.Write(&destination_window);

  // x_offset
  buf.Write(&x_offset);

  // y_offset
  buf.Write(&y_offset);

  // source_window
  buf.Write(&source_window);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Shape::Combine", false);
}

Future<void> Shape::Combine(const So& operation,
                            const Sk& destination_kind,
                            const Sk& source_kind,
                            const Window& destination_window,
                            const int16_t& x_offset,
                            const int16_t& y_offset,
                            const Window& source_window) {
  return Shape::Combine(Shape::CombineRequest{
      operation, destination_kind, source_kind, destination_window, x_offset,
      y_offset, source_window});
}

Future<void> Shape::Offset(const Shape::OffsetRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& destination_kind = request.destination_kind;
  auto& destination_window = request.destination_window;
  auto& x_offset = request.x_offset;
  auto& y_offset = request.y_offset;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 4;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // destination_kind
  uint8_t tmp9;
  tmp9 = static_cast<uint8_t>(destination_kind);
  buf.Write(&tmp9);

  // pad0
  Pad(&buf, 3);

  // destination_window
  buf.Write(&destination_window);

  // x_offset
  buf.Write(&x_offset);

  // y_offset
  buf.Write(&y_offset);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Shape::Offset", false);
}

Future<void> Shape::Offset(const Sk& destination_kind,
                           const Window& destination_window,
                           const int16_t& x_offset,
                           const int16_t& y_offset) {
  return Shape::Offset(Shape::OffsetRequest{
      destination_kind, destination_window, x_offset, y_offset});
}

Future<Shape::QueryExtentsReply> Shape::QueryExtents(
    const Shape::QueryExtentsRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& destination_window = request.destination_window;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 5;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // destination_window
  buf.Write(&destination_window);

  Align(&buf, 4);

  return connection_->SendRequest<Shape::QueryExtentsReply>(
      &buf, "Shape::QueryExtents", false);
}

Future<Shape::QueryExtentsReply> Shape::QueryExtents(
    const Window& destination_window) {
  return Shape::QueryExtents(Shape::QueryExtentsRequest{destination_window});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Shape::QueryExtentsReply> detail::ReadReply<
    Shape::QueryExtentsReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Shape::QueryExtentsReply>();

  auto& sequence = (*reply).sequence;
  auto& bounding_shaped = (*reply).bounding_shaped;
  auto& clip_shaped = (*reply).clip_shaped;
  auto& bounding_shape_extents_x = (*reply).bounding_shape_extents_x;
  auto& bounding_shape_extents_y = (*reply).bounding_shape_extents_y;
  auto& bounding_shape_extents_width = (*reply).bounding_shape_extents_width;
  auto& bounding_shape_extents_height = (*reply).bounding_shape_extents_height;
  auto& clip_shape_extents_x = (*reply).clip_shape_extents_x;
  auto& clip_shape_extents_y = (*reply).clip_shape_extents_y;
  auto& clip_shape_extents_width = (*reply).clip_shape_extents_width;
  auto& clip_shape_extents_height = (*reply).clip_shape_extents_height;

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

  // bounding_shaped
  Read(&bounding_shaped, &buf);

  // clip_shaped
  Read(&clip_shaped, &buf);

  // pad1
  Pad(&buf, 2);

  // bounding_shape_extents_x
  Read(&bounding_shape_extents_x, &buf);

  // bounding_shape_extents_y
  Read(&bounding_shape_extents_y, &buf);

  // bounding_shape_extents_width
  Read(&bounding_shape_extents_width, &buf);

  // bounding_shape_extents_height
  Read(&bounding_shape_extents_height, &buf);

  // clip_shape_extents_x
  Read(&clip_shape_extents_x, &buf);

  // clip_shape_extents_y
  Read(&clip_shape_extents_y, &buf);

  // clip_shape_extents_width
  Read(&clip_shape_extents_width, &buf);

  // clip_shape_extents_height
  Read(&clip_shape_extents_height, &buf);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Shape::SelectInput(const Shape::SelectInputRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& destination_window = request.destination_window;
  auto& enable = request.enable;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 6;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // destination_window
  buf.Write(&destination_window);

  // enable
  buf.Write(&enable);

  // pad0
  Pad(&buf, 3);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Shape::SelectInput", false);
}

Future<void> Shape::SelectInput(const Window& destination_window,
                                const uint8_t& enable) {
  return Shape::SelectInput(
      Shape::SelectInputRequest{destination_window, enable});
}

Future<Shape::InputSelectedReply> Shape::InputSelected(
    const Shape::InputSelectedRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& destination_window = request.destination_window;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 7;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // destination_window
  buf.Write(&destination_window);

  Align(&buf, 4);

  return connection_->SendRequest<Shape::InputSelectedReply>(
      &buf, "Shape::InputSelected", false);
}

Future<Shape::InputSelectedReply> Shape::InputSelected(
    const Window& destination_window) {
  return Shape::InputSelected(Shape::InputSelectedRequest{destination_window});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Shape::InputSelectedReply> detail::ReadReply<
    Shape::InputSelectedReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Shape::InputSelectedReply>();

  auto& enabled = (*reply).enabled;
  auto& sequence = (*reply).sequence;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // enabled
  Read(&enabled, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Shape::GetRectanglesReply> Shape::GetRectangles(
    const Shape::GetRectanglesRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& window = request.window;
  auto& source_kind = request.source_kind;

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

  // source_kind
  uint8_t tmp10;
  tmp10 = static_cast<uint8_t>(source_kind);
  buf.Write(&tmp10);

  // pad0
  Pad(&buf, 3);

  Align(&buf, 4);

  return connection_->SendRequest<Shape::GetRectanglesReply>(
      &buf, "Shape::GetRectangles", false);
}

Future<Shape::GetRectanglesReply> Shape::GetRectangles(const Window& window,
                                                       const Sk& source_kind) {
  return Shape::GetRectangles(Shape::GetRectanglesRequest{window, source_kind});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Shape::GetRectanglesReply> detail::ReadReply<
    Shape::GetRectanglesReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Shape::GetRectanglesReply>();

  auto& ordering = (*reply).ordering;
  auto& sequence = (*reply).sequence;
  uint32_t rectangles_len{};
  auto& rectangles = (*reply).rectangles;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // ordering
  uint8_t tmp11;
  Read(&tmp11, &buf);
  ordering = static_cast<ClipOrdering>(tmp11);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // rectangles_len
  Read(&rectangles_len, &buf);

  // pad0
  Pad(&buf, 20);

  // rectangles
  rectangles.resize(rectangles_len);
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

}  // namespace x11
