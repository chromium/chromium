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

#include "present.h"

#include <unistd.h>
#include <xcb/xcb.h>
#include <xcb/xcbext.h>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "ui/gfx/x/xproto_internal.h"

namespace x11 {

Present::Present(Connection* connection, const x11::QueryExtensionReply& info)
    : connection_(connection), info_(info) {}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<Present::GenericEvent>(Present::GenericEvent* event_,
                                      ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& extension = (*event_).extension;
  auto& sequence = (*event_).sequence;
  auto& length = (*event_).length;
  auto& evtype = (*event_).evtype;
  auto& event = (*event_).event;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // extension
  Read(&extension, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  Read(&length, &buf);

  // evtype
  Read(&evtype, &buf);

  // pad0
  Pad(&buf, 2);

  // event
  Read(&event, &buf);

  DCHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<Present::ConfigureNotifyEvent>(
    Present::ConfigureNotifyEvent* event_,
    ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*event_).sequence;
  auto& event = (*event_).event;
  auto& window = (*event_).window;
  auto& x = (*event_).x;
  auto& y = (*event_).y;
  auto& width = (*event_).width;
  auto& height = (*event_).height;
  auto& off_x = (*event_).off_x;
  auto& off_y = (*event_).off_y;
  auto& pixmap_width = (*event_).pixmap_width;
  auto& pixmap_height = (*event_).pixmap_height;
  auto& pixmap_flags = (*event_).pixmap_flags;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // extension
  uint8_t extension;
  Read(&extension, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // event_type
  uint16_t event_type;
  Read(&event_type, &buf);

  // pad0
  Pad(&buf, 2);

  // event
  Read(&event, &buf);

  // window
  Read(&window, &buf);

  // x
  Read(&x, &buf);

  // y
  Read(&y, &buf);

  // width
  Read(&width, &buf);

  // height
  Read(&height, &buf);

  // off_x
  Read(&off_x, &buf);

  // off_y
  Read(&off_y, &buf);

  // pixmap_width
  Read(&pixmap_width, &buf);

  // pixmap_height
  Read(&pixmap_height, &buf);

  // pixmap_flags
  Read(&pixmap_flags, &buf);

  Align(&buf, 4);
  DCHECK_EQ(buf.offset, 32 + 4 * length);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<Present::CompleteNotifyEvent>(
    Present::CompleteNotifyEvent* event_,
    ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*event_).sequence;
  auto& kind = (*event_).kind;
  auto& mode = (*event_).mode;
  auto& event = (*event_).event;
  auto& window = (*event_).window;
  auto& serial = (*event_).serial;
  auto& ust = (*event_).ust;
  auto& msc = (*event_).msc;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // extension
  uint8_t extension;
  Read(&extension, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // event_type
  uint16_t event_type;
  Read(&event_type, &buf);

  // kind
  uint8_t tmp0;
  Read(&tmp0, &buf);
  kind = static_cast<Present::CompleteKind>(tmp0);

  // mode
  uint8_t tmp1;
  Read(&tmp1, &buf);
  mode = static_cast<Present::CompleteMode>(tmp1);

  // event
  Read(&event, &buf);

  // window
  Read(&window, &buf);

  // serial
  Read(&serial, &buf);

  // ust
  Read(&ust, &buf);

  // msc
  Read(&msc, &buf);

  Align(&buf, 4);
  DCHECK_EQ(buf.offset, 32 + 4 * length);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<Present::IdleNotifyEvent>(Present::IdleNotifyEvent* event_,
                                         ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*event_).sequence;
  auto& event = (*event_).event;
  auto& window = (*event_).window;
  auto& serial = (*event_).serial;
  auto& pixmap = (*event_).pixmap;
  auto& idle_fence = (*event_).idle_fence;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // extension
  uint8_t extension;
  Read(&extension, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // event_type
  uint16_t event_type;
  Read(&event_type, &buf);

  // pad0
  Pad(&buf, 2);

  // event
  Read(&event, &buf);

  // window
  Read(&window, &buf);

  // serial
  Read(&serial, &buf);

  // pixmap
  Read(&pixmap, &buf);

  // idle_fence
  Read(&idle_fence, &buf);

  Align(&buf, 4);
  DCHECK_EQ(buf.offset, 32 + 4 * length);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<Present::RedirectNotifyEvent>(
    Present::RedirectNotifyEvent* event_,
    ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*event_).sequence;
  auto& update_window = (*event_).update_window;
  auto& event = (*event_).event;
  auto& event_window = (*event_).event_window;
  auto& window = (*event_).window;
  auto& pixmap = (*event_).pixmap;
  auto& serial = (*event_).serial;
  auto& valid_region = (*event_).valid_region;
  auto& update_region = (*event_).update_region;
  auto& valid_rect = (*event_).valid_rect;
  auto& update_rect = (*event_).update_rect;
  auto& x_off = (*event_).x_off;
  auto& y_off = (*event_).y_off;
  auto& target_crtc = (*event_).target_crtc;
  auto& wait_fence = (*event_).wait_fence;
  auto& idle_fence = (*event_).idle_fence;
  auto& options = (*event_).options;
  auto& target_msc = (*event_).target_msc;
  auto& divisor = (*event_).divisor;
  auto& remainder = (*event_).remainder;
  auto& notifies = (*event_).notifies;
  size_t notifies_len = notifies.size();

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // extension
  uint8_t extension;
  Read(&extension, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // event_type
  uint16_t event_type;
  Read(&event_type, &buf);

  // update_window
  Read(&update_window, &buf);

  // pad0
  Pad(&buf, 1);

  // event
  Read(&event, &buf);

  // event_window
  Read(&event_window, &buf);

  // window
  Read(&window, &buf);

  // pixmap
  Read(&pixmap, &buf);

  // serial
  Read(&serial, &buf);

  // valid_region
  Read(&valid_region, &buf);

  // update_region
  Read(&update_region, &buf);

  // valid_rect
  {
    auto& x = valid_rect.x;
    auto& y = valid_rect.y;
    auto& width = valid_rect.width;
    auto& height = valid_rect.height;

    // x
    Read(&x, &buf);

    // y
    Read(&y, &buf);

    // width
    Read(&width, &buf);

    // height
    Read(&height, &buf);
  }

  // update_rect
  {
    auto& x = update_rect.x;
    auto& y = update_rect.y;
    auto& width = update_rect.width;
    auto& height = update_rect.height;

    // x
    Read(&x, &buf);

    // y
    Read(&y, &buf);

    // width
    Read(&width, &buf);

    // height
    Read(&height, &buf);
  }

  // x_off
  Read(&x_off, &buf);

  // y_off
  Read(&y_off, &buf);

  // target_crtc
  Read(&target_crtc, &buf);

  // wait_fence
  Read(&wait_fence, &buf);

  // idle_fence
  Read(&idle_fence, &buf);

  // options
  Read(&options, &buf);

  // pad1
  Pad(&buf, 4);

  // target_msc
  Read(&target_msc, &buf);

  // divisor
  Read(&divisor, &buf);

  // remainder
  Read(&remainder, &buf);

  // notifies
  notifies.resize(notifies_len);
  for (auto& notifies_elem : notifies) {
    // notifies_elem
    {
      auto& window = notifies_elem.window;
      auto& serial = notifies_elem.serial;

      // window
      Read(&window, &buf);

      // serial
      Read(&serial, &buf);
    }
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset, 32 + 4 * length);
}

Future<Present::QueryVersionReply> Present::QueryVersion(
    const Present::QueryVersionRequest& request) {
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

  return connection_->SendRequest<Present::QueryVersionReply>(
      &buf, "Present::QueryVersion", false);
}

Future<Present::QueryVersionReply> Present::QueryVersion(
    const uint32_t& major_version,
    const uint32_t& minor_version) {
  return Present::QueryVersion(
      Present::QueryVersionRequest{major_version, minor_version});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Present::QueryVersionReply> detail::ReadReply<
    Present::QueryVersionReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Present::QueryVersionReply>();

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
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Present::PresentPixmap(
    const Present::PresentPixmapRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& window = request.window;
  auto& pixmap = request.pixmap;
  auto& serial = request.serial;
  auto& valid = request.valid;
  auto& update = request.update;
  auto& x_off = request.x_off;
  auto& y_off = request.y_off;
  auto& target_crtc = request.target_crtc;
  auto& wait_fence = request.wait_fence;
  auto& idle_fence = request.idle_fence;
  auto& options = request.options;
  auto& target_msc = request.target_msc;
  auto& divisor = request.divisor;
  auto& remainder = request.remainder;
  auto& notifies = request.notifies;
  size_t notifies_len = notifies.size();

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

  // pixmap
  buf.Write(&pixmap);

  // serial
  buf.Write(&serial);

  // valid
  buf.Write(&valid);

  // update
  buf.Write(&update);

  // x_off
  buf.Write(&x_off);

  // y_off
  buf.Write(&y_off);

  // target_crtc
  buf.Write(&target_crtc);

  // wait_fence
  buf.Write(&wait_fence);

  // idle_fence
  buf.Write(&idle_fence);

  // options
  buf.Write(&options);

  // pad0
  Pad(&buf, 4);

  // target_msc
  buf.Write(&target_msc);

  // divisor
  buf.Write(&divisor);

  // remainder
  buf.Write(&remainder);

  // notifies
  DCHECK_EQ(static_cast<size_t>(notifies_len), notifies.size());
  for (auto& notifies_elem : notifies) {
    // notifies_elem
    {
      auto& window = notifies_elem.window;
      auto& serial = notifies_elem.serial;

      // window
      buf.Write(&window);

      // serial
      buf.Write(&serial);
    }
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Present::PresentPixmap", false);
}

Future<void> Present::PresentPixmap(const Window& window,
                                    const Pixmap& pixmap,
                                    const uint32_t& serial,
                                    const XFixes::Region& valid,
                                    const XFixes::Region& update,
                                    const int16_t& x_off,
                                    const int16_t& y_off,
                                    const RandR::Crtc& target_crtc,
                                    const Sync::Fence& wait_fence,
                                    const Sync::Fence& idle_fence,
                                    const uint32_t& options,
                                    const uint64_t& target_msc,
                                    const uint64_t& divisor,
                                    const uint64_t& remainder,
                                    const std::vector<Notify>& notifies) {
  return Present::PresentPixmap(Present::PresentPixmapRequest{
      window, pixmap, serial, valid, update, x_off, y_off, target_crtc,
      wait_fence, idle_fence, options, target_msc, divisor, remainder,
      notifies});
}

Future<void> Present::NotifyMSC(const Present::NotifyMSCRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& window = request.window;
  auto& serial = request.serial;
  auto& target_msc = request.target_msc;
  auto& divisor = request.divisor;
  auto& remainder = request.remainder;

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

  // serial
  buf.Write(&serial);

  // pad0
  Pad(&buf, 4);

  // target_msc
  buf.Write(&target_msc);

  // divisor
  buf.Write(&divisor);

  // remainder
  buf.Write(&remainder);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Present::NotifyMSC", false);
}

Future<void> Present::NotifyMSC(const Window& window,
                                const uint32_t& serial,
                                const uint64_t& target_msc,
                                const uint64_t& divisor,
                                const uint64_t& remainder) {
  return Present::NotifyMSC(Present::NotifyMSCRequest{
      window, serial, target_msc, divisor, remainder});
}

Future<void> Present::SelectInput(const Present::SelectInputRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& eid = request.eid;
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

  // eid
  buf.Write(&eid);

  // window
  buf.Write(&window);

  // event_mask
  uint32_t tmp2;
  tmp2 = static_cast<uint32_t>(event_mask);
  buf.Write(&tmp2);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Present::SelectInput", false);
}

Future<void> Present::SelectInput(const Event& eid,
                                  const Window& window,
                                  const EventMask& event_mask) {
  return Present::SelectInput(
      Present::SelectInputRequest{eid, window, event_mask});
}

Future<Present::QueryCapabilitiesReply> Present::QueryCapabilities(
    const Present::QueryCapabilitiesRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& target = request.target;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 4;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // target
  buf.Write(&target);

  Align(&buf, 4);

  return connection_->SendRequest<Present::QueryCapabilitiesReply>(
      &buf, "Present::QueryCapabilities", false);
}

Future<Present::QueryCapabilitiesReply> Present::QueryCapabilities(
    const uint32_t& target) {
  return Present::QueryCapabilities(Present::QueryCapabilitiesRequest{target});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Present::QueryCapabilitiesReply> detail::ReadReply<
    Present::QueryCapabilitiesReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Present::QueryCapabilitiesReply>();

  auto& sequence = (*reply).sequence;
  auto& capabilities = (*reply).capabilities;

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

  // capabilities
  Read(&capabilities, &buf);

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

}  // namespace x11
