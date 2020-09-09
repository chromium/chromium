// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/x/xproto_internal.h"

#include <stdint.h>
#include <xcb/xcb.h>
#include <xcb/xcbext.h>

#include "ui/gfx/x/x11.h"

namespace x11 {

MallocedRefCountedMemory::MallocedRefCountedMemory(void* data)
    : data_(reinterpret_cast<uint8_t*>(data)) {}

const uint8_t* MallocedRefCountedMemory::front() const {
  return data_;
}

size_t MallocedRefCountedMemory::size() const {
  // There's no easy way to tell how large malloc'ed data is.
  NOTREACHED();
  return 0;
}

MallocedRefCountedMemory::~MallocedRefCountedMemory() {
  free(data_);
}

OffsetRefCountedMemory::OffsetRefCountedMemory(
    scoped_refptr<base::RefCountedMemory> memory,
    size_t offset,
    size_t size)
    : memory_(memory), offset_(offset), size_(size) {}

const uint8_t* OffsetRefCountedMemory::front() const {
  return memory_->front() + offset_;
}

size_t OffsetRefCountedMemory::size() const {
  return size_;
}

OffsetRefCountedMemory::~OffsetRefCountedMemory() = default;

UnretainedRefCountedMemory::UnretainedRefCountedMemory(const void* data)
    : data_(reinterpret_cast<const uint8_t*>(data)) {}

const uint8_t* UnretainedRefCountedMemory::front() const {
  return data_;
}

size_t UnretainedRefCountedMemory::size() const {
  // There's no easy way to tell how large malloc'ed data is.
  NOTREACHED();
  return 0;
}

UnretainedRefCountedMemory::~UnretainedRefCountedMemory() = default;

base::Optional<unsigned int> SendRequestImpl(x11::Connection* connection,
                                             WriteBuffer* buf,
                                             bool is_void,
                                             bool reply_has_fds) {
  xcb_protocol_request_t xpr{
      .ext = nullptr,
      .isvoid = is_void,
  };

  struct RequestHeader {
    uint8_t major_opcode;
    uint8_t minor_opcode;
    uint16_t length;
  };

  struct ExtendedRequestHeader {
    RequestHeader header;
    uint32_t long_length;
  };
  static_assert(sizeof(ExtendedRequestHeader) == 8, "");

  auto& first_buffer = buf->GetBuffers()[0];
  DCHECK_GE(first_buffer->size(), sizeof(RequestHeader));
  auto* old_header = reinterpret_cast<RequestHeader*>(
      const_cast<uint8_t*>(first_buffer->data()));
  ExtendedRequestHeader new_header{*old_header, 0};

  // Requests are always a multiple of 4 bytes on the wire.  Because of this,
  // the length field represents the size in chunks of 4 bytes.
  DCHECK_EQ(buf->offset() % 4, 0UL);
  size_t size32 = buf->offset() / 4;

  // XCB requires 2 iovecs for its own internal usage.
  std::vector<struct iovec> io{{nullptr, 0}, {nullptr, 0}};
  if (size32 < connection->setup().maximum_request_length) {
    // Regular request
    old_header->length = size32;
  } else if (size32 < connection->extended_max_request_length()) {
    // BigRequests extension request
    DCHECK_EQ(new_header.header.length, 0U);
    new_header.long_length = size32 + 1;

    io.push_back({&new_header, sizeof(ExtendedRequestHeader)});
    first_buffer = base::MakeRefCounted<OffsetRefCountedMemory>(
        first_buffer, sizeof(RequestHeader),
        first_buffer->size() - sizeof(RequestHeader));
  } else {
    LOG(ERROR) << "Cannot send request of length " << buf->offset();
    return base::nullopt;
  }

  for (auto& buffer : buf->GetBuffers())
    io.push_back({const_cast<uint8_t*>(buffer->data()), buffer->size()});
  xpr.count = io.size() - 2;

  xcb_connection_t* conn = connection->XcbConnection();
  auto flags = XCB_REQUEST_CHECKED | XCB_REQUEST_RAW;
  if (reply_has_fds)
    flags |= XCB_REQUEST_REPLY_FDS;

  for (int fd : buf->fds())
    xcb_send_fd(conn, fd);
  unsigned int sequence = xcb_send_request(conn, flags, &io[2], &xpr);

  if (xcb_connection_has_error(conn))
    return base::nullopt;
  if (connection->synchronous())
    connection->Sync();
  return sequence;
}

}  // namespace x11
