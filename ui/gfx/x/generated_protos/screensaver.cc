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

#include "screensaver.h"

#include <unistd.h>
#include <xcb/xcb.h>
#include <xcb/xcbext.h>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/xproto_internal.h"

namespace x11 {

ScreenSaver::ScreenSaver(Connection* connection,
                         const x11::QueryExtensionReply& info)
    : connection_(connection), info_(info) {}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<ScreenSaver::NotifyEvent>(ScreenSaver::NotifyEvent* event_,
                                         ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& state = (*event_).state;
  auto& sequence = (*event_).sequence;
  auto& time = (*event_).time;
  auto& root = (*event_).root;
  auto& window = (*event_).window;
  auto& kind = (*event_).kind;
  auto& forced = (*event_).forced;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // state
  uint8_t tmp0;
  Read(&tmp0, &buf);
  state = static_cast<ScreenSaver::State>(tmp0);

  // sequence
  Read(&sequence, &buf);

  // time
  Read(&time, &buf);

  // root
  Read(&root, &buf);

  // window
  Read(&window, &buf);

  // kind
  uint8_t tmp1;
  Read(&tmp1, &buf);
  kind = static_cast<ScreenSaver::Kind>(tmp1);

  // forced
  Read(&forced, &buf);

  // pad0
  Pad(&buf, 14);

  CHECK_LE(buf.offset, 32ul);
}

Future<ScreenSaver::QueryVersionReply> ScreenSaver::QueryVersion(
    const ScreenSaver::QueryVersionRequest& request) {
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

  // pad0
  Pad(&buf, 2);

  Align(&buf, 4);

  return connection_->SendRequest<ScreenSaver::QueryVersionReply>(
      &buf, "ScreenSaver::QueryVersion", false);
}

Future<ScreenSaver::QueryVersionReply> ScreenSaver::QueryVersion(
    const uint8_t& client_major_version,
    const uint8_t& client_minor_version) {
  return ScreenSaver::QueryVersion(ScreenSaver::QueryVersionRequest{
      client_major_version, client_minor_version});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<ScreenSaver::QueryVersionReply> detail::ReadReply<
    ScreenSaver::QueryVersionReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<ScreenSaver::QueryVersionReply>();

  auto& sequence = (*reply).sequence;
  auto& server_major_version = (*reply).server_major_version;
  auto& server_minor_version = (*reply).server_minor_version;

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

  // server_major_version
  Read(&server_major_version, &buf);

  // server_minor_version
  Read(&server_minor_version, &buf);

  // pad1
  Pad(&buf, 20);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<ScreenSaver::QueryInfoReply> ScreenSaver::QueryInfo(
    const ScreenSaver::QueryInfoRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& drawable = request.drawable;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 1;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // drawable
  buf.Write(&drawable);

  Align(&buf, 4);

  return connection_->SendRequest<ScreenSaver::QueryInfoReply>(
      &buf, "ScreenSaver::QueryInfo", false);
}

Future<ScreenSaver::QueryInfoReply> ScreenSaver::QueryInfo(
    const Drawable& drawable) {
  return ScreenSaver::QueryInfo(ScreenSaver::QueryInfoRequest{drawable});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<ScreenSaver::QueryInfoReply> detail::ReadReply<
    ScreenSaver::QueryInfoReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<ScreenSaver::QueryInfoReply>();

  auto& state = (*reply).state;
  auto& sequence = (*reply).sequence;
  auto& saver_window = (*reply).saver_window;
  auto& ms_until_server = (*reply).ms_until_server;
  auto& ms_since_user_input = (*reply).ms_since_user_input;
  auto& event_mask = (*reply).event_mask;
  auto& kind = (*reply).kind;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // state
  Read(&state, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // saver_window
  Read(&saver_window, &buf);

  // ms_until_server
  Read(&ms_until_server, &buf);

  // ms_since_user_input
  Read(&ms_since_user_input, &buf);

  // event_mask
  Read(&event_mask, &buf);

  // kind
  uint8_t tmp2;
  Read(&tmp2, &buf);
  kind = static_cast<ScreenSaver::Kind>(tmp2);

  // pad0
  Pad(&buf, 7);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> ScreenSaver::SelectInput(
    const ScreenSaver::SelectInputRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& drawable = request.drawable;
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

  // drawable
  buf.Write(&drawable);

  // event_mask
  uint32_t tmp3;
  tmp3 = static_cast<uint32_t>(event_mask);
  buf.Write(&tmp3);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "ScreenSaver::SelectInput",
                                        false);
}

Future<void> ScreenSaver::SelectInput(const Drawable& drawable,
                                      const Event& event_mask) {
  return ScreenSaver::SelectInput(
      ScreenSaver::SelectInputRequest{drawable, event_mask});
}

Future<void> ScreenSaver::SetAttributes(
    const ScreenSaver::SetAttributesRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& drawable = request.drawable;
  auto& x = request.x;
  auto& y = request.y;
  auto& width = request.width;
  auto& height = request.height;
  auto& border_width = request.border_width;
  auto& c_class = request.c_class;
  auto& depth = request.depth;
  auto& visual = request.visual;
  CreateWindowAttribute value_mask{};
  auto& value_list = request;

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

  // x
  buf.Write(&x);

  // y
  buf.Write(&y);

  // width
  buf.Write(&width);

  // height
  buf.Write(&height);

  // border_width
  buf.Write(&border_width);

  // c_class
  uint8_t tmp4;
  tmp4 = static_cast<uint8_t>(c_class);
  buf.Write(&tmp4);

  // depth
  buf.Write(&depth);

  // visual
  buf.Write(&visual);

  // value_mask
  SwitchVar(CreateWindowAttribute::BackPixmap,
            value_list.background_pixmap.has_value(), true, &value_mask);
  SwitchVar(CreateWindowAttribute::BackPixel,
            value_list.background_pixel.has_value(), true, &value_mask);
  SwitchVar(CreateWindowAttribute::BorderPixmap,
            value_list.border_pixmap.has_value(), true, &value_mask);
  SwitchVar(CreateWindowAttribute::BorderPixel,
            value_list.border_pixel.has_value(), true, &value_mask);
  SwitchVar(CreateWindowAttribute::BitGravity,
            value_list.bit_gravity.has_value(), true, &value_mask);
  SwitchVar(CreateWindowAttribute::WinGravity,
            value_list.win_gravity.has_value(), true, &value_mask);
  SwitchVar(CreateWindowAttribute::BackingStore,
            value_list.backing_store.has_value(), true, &value_mask);
  SwitchVar(CreateWindowAttribute::BackingPlanes,
            value_list.backing_planes.has_value(), true, &value_mask);
  SwitchVar(CreateWindowAttribute::BackingPixel,
            value_list.backing_pixel.has_value(), true, &value_mask);
  SwitchVar(CreateWindowAttribute::OverrideRedirect,
            value_list.override_redirect.has_value(), true, &value_mask);
  SwitchVar(CreateWindowAttribute::SaveUnder, value_list.save_under.has_value(),
            true, &value_mask);
  SwitchVar(CreateWindowAttribute::EventMask, value_list.event_mask.has_value(),
            true, &value_mask);
  SwitchVar(CreateWindowAttribute::DontPropagate,
            value_list.do_not_propogate_mask.has_value(), true, &value_mask);
  SwitchVar(CreateWindowAttribute::Colormap, value_list.colormap.has_value(),
            true, &value_mask);
  SwitchVar(CreateWindowAttribute::Cursor, value_list.cursor.has_value(), true,
            &value_mask);
  uint32_t tmp5;
  tmp5 = static_cast<uint32_t>(value_mask);
  buf.Write(&tmp5);

  // value_list
  auto value_list_expr = value_mask;
  if (CaseAnd(value_list_expr, CreateWindowAttribute::BackPixmap)) {
    auto& background_pixmap = *value_list.background_pixmap;

    // background_pixmap
    buf.Write(&background_pixmap);
  }
  if (CaseAnd(value_list_expr, CreateWindowAttribute::BackPixel)) {
    auto& background_pixel = *value_list.background_pixel;

    // background_pixel
    buf.Write(&background_pixel);
  }
  if (CaseAnd(value_list_expr, CreateWindowAttribute::BorderPixmap)) {
    auto& border_pixmap = *value_list.border_pixmap;

    // border_pixmap
    buf.Write(&border_pixmap);
  }
  if (CaseAnd(value_list_expr, CreateWindowAttribute::BorderPixel)) {
    auto& border_pixel = *value_list.border_pixel;

    // border_pixel
    buf.Write(&border_pixel);
  }
  if (CaseAnd(value_list_expr, CreateWindowAttribute::BitGravity)) {
    auto& bit_gravity = *value_list.bit_gravity;

    // bit_gravity
    uint32_t tmp6;
    tmp6 = static_cast<uint32_t>(bit_gravity);
    buf.Write(&tmp6);
  }
  if (CaseAnd(value_list_expr, CreateWindowAttribute::WinGravity)) {
    auto& win_gravity = *value_list.win_gravity;

    // win_gravity
    uint32_t tmp7;
    tmp7 = static_cast<uint32_t>(win_gravity);
    buf.Write(&tmp7);
  }
  if (CaseAnd(value_list_expr, CreateWindowAttribute::BackingStore)) {
    auto& backing_store = *value_list.backing_store;

    // backing_store
    uint32_t tmp8;
    tmp8 = static_cast<uint32_t>(backing_store);
    buf.Write(&tmp8);
  }
  if (CaseAnd(value_list_expr, CreateWindowAttribute::BackingPlanes)) {
    auto& backing_planes = *value_list.backing_planes;

    // backing_planes
    buf.Write(&backing_planes);
  }
  if (CaseAnd(value_list_expr, CreateWindowAttribute::BackingPixel)) {
    auto& backing_pixel = *value_list.backing_pixel;

    // backing_pixel
    buf.Write(&backing_pixel);
  }
  if (CaseAnd(value_list_expr, CreateWindowAttribute::OverrideRedirect)) {
    auto& override_redirect = *value_list.override_redirect;

    // override_redirect
    buf.Write(&override_redirect);
  }
  if (CaseAnd(value_list_expr, CreateWindowAttribute::SaveUnder)) {
    auto& save_under = *value_list.save_under;

    // save_under
    buf.Write(&save_under);
  }
  if (CaseAnd(value_list_expr, CreateWindowAttribute::EventMask)) {
    auto& event_mask = *value_list.event_mask;

    // event_mask
    uint32_t tmp9;
    tmp9 = static_cast<uint32_t>(event_mask);
    buf.Write(&tmp9);
  }
  if (CaseAnd(value_list_expr, CreateWindowAttribute::DontPropagate)) {
    auto& do_not_propogate_mask = *value_list.do_not_propogate_mask;

    // do_not_propogate_mask
    uint32_t tmp10;
    tmp10 = static_cast<uint32_t>(do_not_propogate_mask);
    buf.Write(&tmp10);
  }
  if (CaseAnd(value_list_expr, CreateWindowAttribute::Colormap)) {
    auto& colormap = *value_list.colormap;

    // colormap
    buf.Write(&colormap);
  }
  if (CaseAnd(value_list_expr, CreateWindowAttribute::Cursor)) {
    auto& cursor = *value_list.cursor;

    // cursor
    buf.Write(&cursor);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "ScreenSaver::SetAttributes",
                                        false);
}

Future<void> ScreenSaver::SetAttributes(
    const Drawable& drawable,
    const int16_t& x,
    const int16_t& y,
    const uint16_t& width,
    const uint16_t& height,
    const uint16_t& border_width,
    const WindowClass& c_class,
    const uint8_t& depth,
    const VisualId& visual,
    const std::optional<Pixmap>& background_pixmap,
    const std::optional<uint32_t>& background_pixel,
    const std::optional<Pixmap>& border_pixmap,
    const std::optional<uint32_t>& border_pixel,
    const std::optional<Gravity>& bit_gravity,
    const std::optional<Gravity>& win_gravity,
    const std::optional<BackingStore>& backing_store,
    const std::optional<uint32_t>& backing_planes,
    const std::optional<uint32_t>& backing_pixel,
    const std::optional<Bool32>& override_redirect,
    const std::optional<Bool32>& save_under,
    const std::optional<EventMask>& event_mask,
    const std::optional<EventMask>& do_not_propogate_mask,
    const std::optional<ColorMap>& colormap,
    const std::optional<Cursor>& cursor) {
  return ScreenSaver::SetAttributes(
      ScreenSaver::SetAttributesRequest{drawable,
                                        x,
                                        y,
                                        width,
                                        height,
                                        border_width,
                                        c_class,
                                        depth,
                                        visual,
                                        background_pixmap,
                                        background_pixel,
                                        border_pixmap,
                                        border_pixel,
                                        bit_gravity,
                                        win_gravity,
                                        backing_store,
                                        backing_planes,
                                        backing_pixel,
                                        override_redirect,
                                        save_under,
                                        event_mask,
                                        do_not_propogate_mask,
                                        colormap,
                                        cursor});
}

Future<void> ScreenSaver::UnsetAttributes(
    const ScreenSaver::UnsetAttributesRequest& request) {
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

  return connection_->SendRequest<void>(&buf, "ScreenSaver::UnsetAttributes",
                                        false);
}

Future<void> ScreenSaver::UnsetAttributes(const Drawable& drawable) {
  return ScreenSaver::UnsetAttributes(
      ScreenSaver::UnsetAttributesRequest{drawable});
}

Future<void> ScreenSaver::Suspend(const ScreenSaver::SuspendRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& suspend = request.suspend;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 5;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // suspend
  buf.Write(&suspend);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "ScreenSaver::Suspend", false);
}

Future<void> ScreenSaver::Suspend(const uint32_t& suspend) {
  return ScreenSaver::Suspend(ScreenSaver::SuspendRequest{suspend});
}

}  // namespace x11
