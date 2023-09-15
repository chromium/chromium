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

#include "composite.h"

#include <unistd.h>
#include <xcb/xcb.h>
#include <xcb/xcbext.h>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "ui/gfx/x/xproto_internal.h"

namespace x11 {

Composite::Composite(Connection* connection,
                     const x11::QueryExtensionReply& info)
    : connection_(connection), info_(info) {}

Future<Composite::QueryVersionReply> Composite::QueryVersion(
    const Composite::QueryVersionRequest& request) {
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

  return connection_->SendRequest<Composite::QueryVersionReply>(
      &buf, "Composite::QueryVersion", false);
}

Future<Composite::QueryVersionReply> Composite::QueryVersion(
    const uint32_t& client_major_version,
    const uint32_t& client_minor_version) {
  return Composite::QueryVersion(Composite::QueryVersionRequest{
      client_major_version, client_minor_version});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Composite::QueryVersionReply> detail::ReadReply<
    Composite::QueryVersionReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Composite::QueryVersionReply>();

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
  DUMP_WILL_BE_CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Composite::RedirectWindow(
    const Composite::RedirectWindowRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& window = request.window;
  auto& update = request.update;

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

  // update
  uint8_t tmp0;
  tmp0 = static_cast<uint8_t>(update);
  buf.Write(&tmp0);

  // pad0
  Pad(&buf, 3);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Composite::RedirectWindow",
                                        false);
}

Future<void> Composite::RedirectWindow(const Window& window,
                                       const Redirect& update) {
  return Composite::RedirectWindow(
      Composite::RedirectWindowRequest{window, update});
}

Future<void> Composite::RedirectSubwindows(
    const Composite::RedirectSubwindowsRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& window = request.window;
  auto& update = request.update;

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

  // update
  uint8_t tmp1;
  tmp1 = static_cast<uint8_t>(update);
  buf.Write(&tmp1);

  // pad0
  Pad(&buf, 3);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Composite::RedirectSubwindows",
                                        false);
}

Future<void> Composite::RedirectSubwindows(const Window& window,
                                           const Redirect& update) {
  return Composite::RedirectSubwindows(
      Composite::RedirectSubwindowsRequest{window, update});
}

Future<void> Composite::UnredirectWindow(
    const Composite::UnredirectWindowRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& window = request.window;
  auto& update = request.update;

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

  // update
  uint8_t tmp2;
  tmp2 = static_cast<uint8_t>(update);
  buf.Write(&tmp2);

  // pad0
  Pad(&buf, 3);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Composite::UnredirectWindow",
                                        false);
}

Future<void> Composite::UnredirectWindow(const Window& window,
                                         const Redirect& update) {
  return Composite::UnredirectWindow(
      Composite::UnredirectWindowRequest{window, update});
}

Future<void> Composite::UnredirectSubwindows(
    const Composite::UnredirectSubwindowsRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& window = request.window;
  auto& update = request.update;

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

  // update
  uint8_t tmp3;
  tmp3 = static_cast<uint8_t>(update);
  buf.Write(&tmp3);

  // pad0
  Pad(&buf, 3);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Composite::UnredirectSubwindows",
                                        false);
}

Future<void> Composite::UnredirectSubwindows(const Window& window,
                                             const Redirect& update) {
  return Composite::UnredirectSubwindows(
      Composite::UnredirectSubwindowsRequest{window, update});
}

Future<void> Composite::CreateRegionFromBorderClip(
    const Composite::CreateRegionFromBorderClipRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& region = request.region;
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

  // region
  buf.Write(&region);

  // window
  buf.Write(&window);

  Align(&buf, 4);

  return connection_->SendRequest<void>(
      &buf, "Composite::CreateRegionFromBorderClip", false);
}

Future<void> Composite::CreateRegionFromBorderClip(const XFixes::Region& region,
                                                   const Window& window) {
  return Composite::CreateRegionFromBorderClip(
      Composite::CreateRegionFromBorderClipRequest{region, window});
}

Future<void> Composite::NameWindowPixmap(
    const Composite::NameWindowPixmapRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& window = request.window;
  auto& pixmap = request.pixmap;

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

  // pixmap
  buf.Write(&pixmap);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Composite::NameWindowPixmap",
                                        false);
}

Future<void> Composite::NameWindowPixmap(const Window& window,
                                         const Pixmap& pixmap) {
  return Composite::NameWindowPixmap(
      Composite::NameWindowPixmapRequest{window, pixmap});
}

Future<Composite::GetOverlayWindowReply> Composite::GetOverlayWindow(
    const Composite::GetOverlayWindowRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& window = request.window;

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

  Align(&buf, 4);

  return connection_->SendRequest<Composite::GetOverlayWindowReply>(
      &buf, "Composite::GetOverlayWindow", false);
}

Future<Composite::GetOverlayWindowReply> Composite::GetOverlayWindow(
    const Window& window) {
  return Composite::GetOverlayWindow(
      Composite::GetOverlayWindowRequest{window});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Composite::GetOverlayWindowReply> detail::ReadReply<
    Composite::GetOverlayWindowReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Composite::GetOverlayWindowReply>();

  auto& sequence = (*reply).sequence;
  auto& overlay_win = (*reply).overlay_win;

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

  // overlay_win
  Read(&overlay_win, &buf);

  // pad1
  Pad(&buf, 20);

  Align(&buf, 4);
  DUMP_WILL_BE_CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Composite::ReleaseOverlayWindow(
    const Composite::ReleaseOverlayWindowRequest& request) {
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

  return connection_->SendRequest<void>(&buf, "Composite::ReleaseOverlayWindow",
                                        false);
}

Future<void> Composite::ReleaseOverlayWindow(const Window& window) {
  return Composite::ReleaseOverlayWindow(
      Composite::ReleaseOverlayWindowRequest{window});
}

}  // namespace x11
