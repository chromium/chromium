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

#include "shm.h"

#include <unistd.h>
#include <xcb/xcb.h>
#include <xcb/xcbext.h>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/xproto_internal.h"

namespace x11 {

Shm::Shm(Connection* connection, const x11::QueryExtensionReply& info)
    : connection_(connection), info_(info) {}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<Shm::CompletionEvent>(Shm::CompletionEvent* event_,
                                     ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*event_).sequence;
  auto& drawable = (*event_).drawable;
  auto& minor_event = (*event_).minor_event;
  auto& major_event = (*event_).major_event;
  auto& shmseg = (*event_).shmseg;
  auto& offset = (*event_).offset;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // drawable
  Read(&drawable, &buf);

  // minor_event
  Read(&minor_event, &buf);

  // major_event
  Read(&major_event, &buf);

  // pad1
  Pad(&buf, 1);

  // shmseg
  Read(&shmseg, &buf);

  // offset
  Read(&offset, &buf);

  CHECK_LE(buf.offset, 32ul);
}

std::string Shm::BadSegError::ToString() const {
  std::stringstream ss_;
  ss_ << "Shm::BadSegError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<Shm::BadSegError>(Shm::BadSegError* error_, ReadBuffer* buffer) {
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
  Pad(&buf, 1);

  CHECK_LE(buf.offset, 32ul);
}

Future<Shm::QueryVersionReply> Shm::QueryVersion(
    const Shm::QueryVersionRequest& request) {
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

  return connection_->SendRequest<Shm::QueryVersionReply>(
      &buf, "Shm::QueryVersion", false);
}

Future<Shm::QueryVersionReply> Shm::QueryVersion() {
  return Shm::QueryVersion(Shm::QueryVersionRequest{});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Shm::QueryVersionReply> detail::ReadReply<
    Shm::QueryVersionReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Shm::QueryVersionReply>();

  auto& shared_pixmaps = (*reply).shared_pixmaps;
  auto& sequence = (*reply).sequence;
  auto& major_version = (*reply).major_version;
  auto& minor_version = (*reply).minor_version;
  auto& uid = (*reply).uid;
  auto& gid = (*reply).gid;
  auto& pixmap_format = (*reply).pixmap_format;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // shared_pixmaps
  Read(&shared_pixmaps, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // major_version
  Read(&major_version, &buf);

  // minor_version
  Read(&minor_version, &buf);

  // uid
  Read(&uid, &buf);

  // gid
  Read(&gid, &buf);

  // pixmap_format
  Read(&pixmap_format, &buf);

  // pad0
  Pad(&buf, 15);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Shm::Attach(const Shm::AttachRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& shmseg = request.shmseg;
  auto& shmid = request.shmid;
  auto& read_only = request.read_only;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 1;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // shmseg
  buf.Write(&shmseg);

  // shmid
  buf.Write(&shmid);

  // read_only
  buf.Write(&read_only);

  // pad0
  Pad(&buf, 3);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Shm::Attach", false);
}

Future<void> Shm::Attach(const Seg& shmseg,
                         const uint32_t& shmid,
                         const uint8_t& read_only) {
  return Shm::Attach(Shm::AttachRequest{shmseg, shmid, read_only});
}

Future<void> Shm::Detach(const Shm::DetachRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& shmseg = request.shmseg;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 2;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // shmseg
  buf.Write(&shmseg);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Shm::Detach", false);
}

Future<void> Shm::Detach(const Seg& shmseg) {
  return Shm::Detach(Shm::DetachRequest{shmseg});
}

Future<void> Shm::PutImage(const Shm::PutImageRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& drawable = request.drawable;
  auto& gc = request.gc;
  auto& total_width = request.total_width;
  auto& total_height = request.total_height;
  auto& src_x = request.src_x;
  auto& src_y = request.src_y;
  auto& src_width = request.src_width;
  auto& src_height = request.src_height;
  auto& dst_x = request.dst_x;
  auto& dst_y = request.dst_y;
  auto& depth = request.depth;
  auto& format = request.format;
  auto& send_event = request.send_event;
  auto& shmseg = request.shmseg;
  auto& offset = request.offset;

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

  // gc
  buf.Write(&gc);

  // total_width
  buf.Write(&total_width);

  // total_height
  buf.Write(&total_height);

  // src_x
  buf.Write(&src_x);

  // src_y
  buf.Write(&src_y);

  // src_width
  buf.Write(&src_width);

  // src_height
  buf.Write(&src_height);

  // dst_x
  buf.Write(&dst_x);

  // dst_y
  buf.Write(&dst_y);

  // depth
  buf.Write(&depth);

  // format
  uint8_t tmp0;
  tmp0 = static_cast<uint8_t>(format);
  buf.Write(&tmp0);

  // send_event
  buf.Write(&send_event);

  // pad0
  Pad(&buf, 1);

  // shmseg
  buf.Write(&shmseg);

  // offset
  buf.Write(&offset);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Shm::PutImage", false);
}

Future<void> Shm::PutImage(const Drawable& drawable,
                           const GraphicsContext& gc,
                           const uint16_t& total_width,
                           const uint16_t& total_height,
                           const uint16_t& src_x,
                           const uint16_t& src_y,
                           const uint16_t& src_width,
                           const uint16_t& src_height,
                           const int16_t& dst_x,
                           const int16_t& dst_y,
                           const uint8_t& depth,
                           const ImageFormat& format,
                           const uint8_t& send_event,
                           const Seg& shmseg,
                           const uint32_t& offset) {
  return Shm::PutImage(Shm::PutImageRequest{
      drawable, gc, total_width, total_height, src_x, src_y, src_width,
      src_height, dst_x, dst_y, depth, format, send_event, shmseg, offset});
}

Future<Shm::GetImageReply> Shm::GetImage(const Shm::GetImageRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& drawable = request.drawable;
  auto& x = request.x;
  auto& y = request.y;
  auto& width = request.width;
  auto& height = request.height;
  auto& plane_mask = request.plane_mask;
  auto& format = request.format;
  auto& shmseg = request.shmseg;
  auto& offset = request.offset;

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

  // x
  buf.Write(&x);

  // y
  buf.Write(&y);

  // width
  buf.Write(&width);

  // height
  buf.Write(&height);

  // plane_mask
  buf.Write(&plane_mask);

  // format
  buf.Write(&format);

  // pad0
  Pad(&buf, 3);

  // shmseg
  buf.Write(&shmseg);

  // offset
  buf.Write(&offset);

  Align(&buf, 4);

  return connection_->SendRequest<Shm::GetImageReply>(&buf, "Shm::GetImage",
                                                      false);
}

Future<Shm::GetImageReply> Shm::GetImage(const Drawable& drawable,
                                         const int16_t& x,
                                         const int16_t& y,
                                         const uint16_t& width,
                                         const uint16_t& height,
                                         const uint32_t& plane_mask,
                                         const uint8_t& format,
                                         const Seg& shmseg,
                                         const uint32_t& offset) {
  return Shm::GetImage(Shm::GetImageRequest{
      drawable, x, y, width, height, plane_mask, format, shmseg, offset});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Shm::GetImageReply> detail::ReadReply<Shm::GetImageReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Shm::GetImageReply>();

  auto& depth = (*reply).depth;
  auto& sequence = (*reply).sequence;
  auto& visual = (*reply).visual;
  auto& size = (*reply).size;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // depth
  Read(&depth, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // visual
  Read(&visual, &buf);

  // size
  Read(&size, &buf);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Shm::CreatePixmap(const Shm::CreatePixmapRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& pid = request.pid;
  auto& drawable = request.drawable;
  auto& width = request.width;
  auto& height = request.height;
  auto& depth = request.depth;
  auto& shmseg = request.shmseg;
  auto& offset = request.offset;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 5;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // pid
  buf.Write(&pid);

  // drawable
  buf.Write(&drawable);

  // width
  buf.Write(&width);

  // height
  buf.Write(&height);

  // depth
  buf.Write(&depth);

  // pad0
  Pad(&buf, 3);

  // shmseg
  buf.Write(&shmseg);

  // offset
  buf.Write(&offset);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Shm::CreatePixmap", false);
}

Future<void> Shm::CreatePixmap(const Pixmap& pid,
                               const Drawable& drawable,
                               const uint16_t& width,
                               const uint16_t& height,
                               const uint8_t& depth,
                               const Seg& shmseg,
                               const uint32_t& offset) {
  return Shm::CreatePixmap(Shm::CreatePixmapRequest{
      pid, drawable, width, height, depth, shmseg, offset});
}

Future<void> Shm::AttachFd(const Shm::AttachFdRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& shmseg = request.shmseg;
  auto& shm_fd = request.shm_fd;
  auto& read_only = request.read_only;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 6;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // shmseg
  buf.Write(&shmseg);

  // shm_fd
  buf.fds().push_back(HANDLE_EINTR(dup(shm_fd.get())));

  // read_only
  buf.Write(&read_only);

  // pad0
  Pad(&buf, 3);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Shm::AttachFd", false);
}

Future<void> Shm::AttachFd(const Seg& shmseg,
                           const RefCountedFD& shm_fd,
                           const uint8_t& read_only) {
  return Shm::AttachFd(Shm::AttachFdRequest{shmseg, shm_fd, read_only});
}

Future<Shm::CreateSegmentReply> Shm::CreateSegment(
    const Shm::CreateSegmentRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& shmseg = request.shmseg;
  auto& size = request.size;
  auto& read_only = request.read_only;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 7;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // shmseg
  buf.Write(&shmseg);

  // size
  buf.Write(&size);

  // read_only
  buf.Write(&read_only);

  // pad0
  Pad(&buf, 3);

  Align(&buf, 4);

  return connection_->SendRequest<Shm::CreateSegmentReply>(
      &buf, "Shm::CreateSegment", true);
}

Future<Shm::CreateSegmentReply> Shm::CreateSegment(const Seg& shmseg,
                                                   const uint32_t& size,
                                                   const uint8_t& read_only) {
  return Shm::CreateSegment(Shm::CreateSegmentRequest{shmseg, size, read_only});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Shm::CreateSegmentReply> detail::ReadReply<
    Shm::CreateSegmentReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Shm::CreateSegmentReply>();

  auto& nfd = (*reply).nfd;
  auto& sequence = (*reply).sequence;
  auto& shm_fd = (*reply).shm_fd;

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

  // shm_fd
  shm_fd = RefCountedFD(buf.TakeFd());

  // pad0
  Pad(&buf, 24);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

}  // namespace x11
