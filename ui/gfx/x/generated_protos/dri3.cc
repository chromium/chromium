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

#include "dri3.h"

#include <unistd.h>
#include <xcb/xcb.h>
#include <xcb/xcbext.h>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/xproto_internal.h"

namespace x11 {

Dri3::Dri3(Connection* connection, const x11::QueryExtensionReply& info)
    : connection_(connection), info_(info) {}

Future<Dri3::QueryVersionReply> Dri3::QueryVersion(
    const Dri3::QueryVersionRequest& request) {
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

  return connection_->SendRequest<Dri3::QueryVersionReply>(
      &buf, "Dri3::QueryVersion", false);
}

Future<Dri3::QueryVersionReply> Dri3::QueryVersion(
    const uint32_t& major_version,
    const uint32_t& minor_version) {
  return Dri3::QueryVersion(
      Dri3::QueryVersionRequest{major_version, minor_version});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Dri3::QueryVersionReply> detail::ReadReply<
    Dri3::QueryVersionReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Dri3::QueryVersionReply>();

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

Future<Dri3::OpenReply> Dri3::Open(const Dri3::OpenRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& drawable = request.drawable;
  auto& provider = request.provider;

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

  // provider
  buf.Write(&provider);

  Align(&buf, 4);

  return connection_->SendRequest<Dri3::OpenReply>(&buf, "Dri3::Open", true);
}

Future<Dri3::OpenReply> Dri3::Open(const Drawable& drawable,
                                   const uint32_t& provider) {
  return Dri3::Open(Dri3::OpenRequest{drawable, provider});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Dri3::OpenReply> detail::ReadReply<Dri3::OpenReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Dri3::OpenReply>();

  auto& nfd = (*reply).nfd;
  auto& sequence = (*reply).sequence;
  auto& device_fd = (*reply).device_fd;

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

  // device_fd
  device_fd = RefCountedFD(buf.TakeFd());

  // pad0
  Pad(&buf, 24);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Dri3::PixmapFromBuffer(
    const Dri3::PixmapFromBufferRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& pixmap = request.pixmap;
  auto& drawable = request.drawable;
  auto& size = request.size;
  auto& width = request.width;
  auto& height = request.height;
  auto& stride = request.stride;
  auto& depth = request.depth;
  auto& bpp = request.bpp;
  auto& pixmap_fd = request.pixmap_fd;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 2;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // pixmap
  buf.Write(&pixmap);

  // drawable
  buf.Write(&drawable);

  // size
  buf.Write(&size);

  // width
  buf.Write(&width);

  // height
  buf.Write(&height);

  // stride
  buf.Write(&stride);

  // depth
  buf.Write(&depth);

  // bpp
  buf.Write(&bpp);

  // pixmap_fd
  buf.fds().push_back(HANDLE_EINTR(dup(pixmap_fd.get())));

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Dri3::PixmapFromBuffer", false);
}

Future<void> Dri3::PixmapFromBuffer(const Pixmap& pixmap,
                                    const Drawable& drawable,
                                    const uint32_t& size,
                                    const uint16_t& width,
                                    const uint16_t& height,
                                    const uint16_t& stride,
                                    const uint8_t& depth,
                                    const uint8_t& bpp,
                                    const RefCountedFD& pixmap_fd) {
  return Dri3::PixmapFromBuffer(Dri3::PixmapFromBufferRequest{
      pixmap, drawable, size, width, height, stride, depth, bpp, pixmap_fd});
}

Future<Dri3::BufferFromPixmapReply> Dri3::BufferFromPixmap(
    const Dri3::BufferFromPixmapRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& pixmap = request.pixmap;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 3;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // pixmap
  buf.Write(&pixmap);

  Align(&buf, 4);

  return connection_->SendRequest<Dri3::BufferFromPixmapReply>(
      &buf, "Dri3::BufferFromPixmap", true);
}

Future<Dri3::BufferFromPixmapReply> Dri3::BufferFromPixmap(
    const Pixmap& pixmap) {
  return Dri3::BufferFromPixmap(Dri3::BufferFromPixmapRequest{pixmap});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Dri3::BufferFromPixmapReply> detail::ReadReply<
    Dri3::BufferFromPixmapReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Dri3::BufferFromPixmapReply>();

  auto& nfd = (*reply).nfd;
  auto& sequence = (*reply).sequence;
  auto& size = (*reply).size;
  auto& width = (*reply).width;
  auto& height = (*reply).height;
  auto& stride = (*reply).stride;
  auto& depth = (*reply).depth;
  auto& bpp = (*reply).bpp;
  auto& pixmap_fd = (*reply).pixmap_fd;

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

  // size
  Read(&size, &buf);

  // width
  Read(&width, &buf);

  // height
  Read(&height, &buf);

  // stride
  Read(&stride, &buf);

  // depth
  Read(&depth, &buf);

  // bpp
  Read(&bpp, &buf);

  // pixmap_fd
  pixmap_fd = RefCountedFD(buf.TakeFd());

  // pad0
  Pad(&buf, 12);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Dri3::FenceFromFD(const Dri3::FenceFromFDRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& drawable = request.drawable;
  auto& fence = request.fence;
  auto& initially_triggered = request.initially_triggered;
  auto& fence_fd = request.fence_fd;

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

  // fence
  buf.Write(&fence);

  // initially_triggered
  buf.Write(&initially_triggered);

  // pad0
  Pad(&buf, 3);

  // fence_fd
  buf.fds().push_back(HANDLE_EINTR(dup(fence_fd.get())));

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Dri3::FenceFromFD", false);
}

Future<void> Dri3::FenceFromFD(const Drawable& drawable,
                               const uint32_t& fence,
                               const uint8_t& initially_triggered,
                               const RefCountedFD& fence_fd) {
  return Dri3::FenceFromFD(
      Dri3::FenceFromFDRequest{drawable, fence, initially_triggered, fence_fd});
}

Future<Dri3::FDFromFenceReply> Dri3::FDFromFence(
    const Dri3::FDFromFenceRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& drawable = request.drawable;
  auto& fence = request.fence;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 5;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // drawable
  buf.Write(&drawable);

  // fence
  buf.Write(&fence);

  Align(&buf, 4);

  return connection_->SendRequest<Dri3::FDFromFenceReply>(
      &buf, "Dri3::FDFromFence", true);
}

Future<Dri3::FDFromFenceReply> Dri3::FDFromFence(const Drawable& drawable,
                                                 const uint32_t& fence) {
  return Dri3::FDFromFence(Dri3::FDFromFenceRequest{drawable, fence});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Dri3::FDFromFenceReply> detail::ReadReply<
    Dri3::FDFromFenceReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Dri3::FDFromFenceReply>();

  auto& nfd = (*reply).nfd;
  auto& sequence = (*reply).sequence;
  auto& fence_fd = (*reply).fence_fd;

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

  // fence_fd
  fence_fd = RefCountedFD(buf.TakeFd());

  // pad0
  Pad(&buf, 24);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Dri3::GetSupportedModifiersReply> Dri3::GetSupportedModifiers(
    const Dri3::GetSupportedModifiersRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& window = request.window;
  auto& depth = request.depth;
  auto& bpp = request.bpp;

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

  // depth
  buf.Write(&depth);

  // bpp
  buf.Write(&bpp);

  // pad0
  Pad(&buf, 2);

  Align(&buf, 4);

  return connection_->SendRequest<Dri3::GetSupportedModifiersReply>(
      &buf, "Dri3::GetSupportedModifiers", false);
}

Future<Dri3::GetSupportedModifiersReply> Dri3::GetSupportedModifiers(
    const uint32_t& window,
    const uint8_t& depth,
    const uint8_t& bpp) {
  return Dri3::GetSupportedModifiers(
      Dri3::GetSupportedModifiersRequest{window, depth, bpp});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Dri3::GetSupportedModifiersReply> detail::ReadReply<
    Dri3::GetSupportedModifiersReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Dri3::GetSupportedModifiersReply>();

  auto& sequence = (*reply).sequence;
  uint32_t num_window_modifiers{};
  uint32_t num_screen_modifiers{};
  auto& window_modifiers = (*reply).window_modifiers;
  auto& screen_modifiers = (*reply).screen_modifiers;

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

  // num_window_modifiers
  Read(&num_window_modifiers, &buf);

  // num_screen_modifiers
  Read(&num_screen_modifiers, &buf);

  // pad1
  Pad(&buf, 16);

  // window_modifiers
  window_modifiers.resize(num_window_modifiers);
  for (auto& window_modifiers_elem : window_modifiers) {
    // window_modifiers_elem
    Read(&window_modifiers_elem, &buf);
  }

  // screen_modifiers
  screen_modifiers.resize(num_screen_modifiers);
  for (auto& screen_modifiers_elem : screen_modifiers) {
    // screen_modifiers_elem
    Read(&screen_modifiers_elem, &buf);
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Dri3::PixmapFromBuffers(
    const Dri3::PixmapFromBuffersRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& pixmap = request.pixmap;
  auto& window = request.window;
  uint8_t num_buffers{};
  auto& width = request.width;
  auto& height = request.height;
  auto& stride0 = request.stride0;
  auto& offset0 = request.offset0;
  auto& stride1 = request.stride1;
  auto& offset1 = request.offset1;
  auto& stride2 = request.stride2;
  auto& offset2 = request.offset2;
  auto& stride3 = request.stride3;
  auto& offset3 = request.offset3;
  auto& depth = request.depth;
  auto& bpp = request.bpp;
  auto& modifier = request.modifier;
  auto& buffers = request.buffers;
  size_t buffers_len = buffers.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 7;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // pixmap
  buf.Write(&pixmap);

  // window
  buf.Write(&window);

  // num_buffers
  num_buffers = buffers.size();
  buf.Write(&num_buffers);

  // pad0
  Pad(&buf, 3);

  // width
  buf.Write(&width);

  // height
  buf.Write(&height);

  // stride0
  buf.Write(&stride0);

  // offset0
  buf.Write(&offset0);

  // stride1
  buf.Write(&stride1);

  // offset1
  buf.Write(&offset1);

  // stride2
  buf.Write(&stride2);

  // offset2
  buf.Write(&offset2);

  // stride3
  buf.Write(&stride3);

  // offset3
  buf.Write(&offset3);

  // depth
  buf.Write(&depth);

  // bpp
  buf.Write(&bpp);

  // pad1
  Pad(&buf, 2);

  // modifier
  buf.Write(&modifier);

  // buffers
  CHECK_EQ(static_cast<size_t>(num_buffers), buffers.size());
  for (auto& buffers_elem : buffers) {
    // buffers_elem
    buf.fds().push_back(HANDLE_EINTR(dup(buffers_elem.get())));
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Dri3::PixmapFromBuffers", false);
}

Future<void> Dri3::PixmapFromBuffers(const Pixmap& pixmap,
                                     const Window& window,
                                     const uint16_t& width,
                                     const uint16_t& height,
                                     const uint32_t& stride0,
                                     const uint32_t& offset0,
                                     const uint32_t& stride1,
                                     const uint32_t& offset1,
                                     const uint32_t& stride2,
                                     const uint32_t& offset2,
                                     const uint32_t& stride3,
                                     const uint32_t& offset3,
                                     const uint8_t& depth,
                                     const uint8_t& bpp,
                                     const uint64_t& modifier,
                                     const std::vector<RefCountedFD>& buffers) {
  return Dri3::PixmapFromBuffers(Dri3::PixmapFromBuffersRequest{
      pixmap, window, width, height, stride0, offset0, stride1, offset1,
      stride2, offset2, stride3, offset3, depth, bpp, modifier, buffers});
}

Future<Dri3::BuffersFromPixmapReply> Dri3::BuffersFromPixmap(
    const Dri3::BuffersFromPixmapRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& pixmap = request.pixmap;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 8;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // pixmap
  buf.Write(&pixmap);

  Align(&buf, 4);

  return connection_->SendRequest<Dri3::BuffersFromPixmapReply>(
      &buf, "Dri3::BuffersFromPixmap", true);
}

Future<Dri3::BuffersFromPixmapReply> Dri3::BuffersFromPixmap(
    const Pixmap& pixmap) {
  return Dri3::BuffersFromPixmap(Dri3::BuffersFromPixmapRequest{pixmap});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Dri3::BuffersFromPixmapReply> detail::ReadReply<
    Dri3::BuffersFromPixmapReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Dri3::BuffersFromPixmapReply>();

  uint8_t nfd{};
  auto& sequence = (*reply).sequence;
  auto& width = (*reply).width;
  auto& height = (*reply).height;
  auto& modifier = (*reply).modifier;
  auto& depth = (*reply).depth;
  auto& bpp = (*reply).bpp;
  auto& strides = (*reply).strides;
  auto& offsets = (*reply).offsets;
  auto& buffers = (*reply).buffers;

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

  // width
  Read(&width, &buf);

  // height
  Read(&height, &buf);

  // pad0
  Pad(&buf, 4);

  // modifier
  Read(&modifier, &buf);

  // depth
  Read(&depth, &buf);

  // bpp
  Read(&bpp, &buf);

  // pad1
  Pad(&buf, 6);

  // strides
  strides.resize(nfd);
  for (auto& strides_elem : strides) {
    // strides_elem
    Read(&strides_elem, &buf);
  }

  // offsets
  offsets.resize(nfd);
  for (auto& offsets_elem : offsets) {
    // offsets_elem
    Read(&offsets_elem, &buf);
  }

  // buffers
  buffers.resize(nfd);
  for (auto& buffers_elem : buffers) {
    // buffers_elem
    buffers_elem = RefCountedFD(buf.TakeFd());
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Dri3::SetDRMDeviceInUse(
    const Dri3::SetDRMDeviceInUseRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& window = request.window;
  auto& drmMajor = request.drmMajor;
  auto& drmMinor = request.drmMinor;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 9;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // window
  buf.Write(&window);

  // drmMajor
  buf.Write(&drmMajor);

  // drmMinor
  buf.Write(&drmMinor);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Dri3::SetDRMDeviceInUse", false);
}

Future<void> Dri3::SetDRMDeviceInUse(const Window& window,
                                     const uint32_t& drmMajor,
                                     const uint32_t& drmMinor) {
  return Dri3::SetDRMDeviceInUse(
      Dri3::SetDRMDeviceInUseRequest{window, drmMajor, drmMinor});
}

Future<void> Dri3::ImportSyncobj(const Dri3::ImportSyncobjRequest& request) {
  if (!connection_->Ready() || !present()) {
    return {};
  }

  WriteBuffer buf;

  auto& syncobj = request.syncobj;
  auto& drawable = request.drawable;
  auto& syncobj_fd = request.syncobj_fd;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 10;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // syncobj
  buf.Write(&syncobj);

  // drawable
  buf.Write(&drawable);

  // syncobj_fd
  buf.fds().push_back(HANDLE_EINTR(dup(syncobj_fd.get())));

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Dri3::ImportSyncobj", false);
}

Future<void> Dri3::ImportSyncobj(const Syncobj& syncobj,
                                 const Drawable& drawable,
                                 const RefCountedFD& syncobj_fd) {
  return Dri3::ImportSyncobj(
      Dri3::ImportSyncobjRequest{syncobj, drawable, syncobj_fd});
}

Future<void> Dri3::FreeSyncobj(const Dri3::FreeSyncobjRequest& request) {
  if (!connection_->Ready() || !present()) {
    return {};
  }

  WriteBuffer buf;

  auto& syncobj = request.syncobj;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 11;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // syncobj
  buf.Write(&syncobj);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Dri3::FreeSyncobj", false);
}

Future<void> Dri3::FreeSyncobj(const Syncobj& syncobj) {
  return Dri3::FreeSyncobj(Dri3::FreeSyncobjRequest{syncobj});
}

}  // namespace x11
