// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/x/xproto_types.h"

#include <xcb/xcbext.h>

#include "base/memory/scoped_refptr.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/xproto_internal.h"

namespace x11 {

namespace {

constexpr uint8_t kResponseTypeReply = 1;

struct ReplyHeader {
  uint8_t response_type;
  uint8_t pad;
  uint16_t sequence;
  uint32_t length;
};

}  // namespace

ReadBuffer::ReadBuffer(scoped_refptr<base::RefCountedMemory> data,
                       bool setup_message)
    : data(data) {
  // X connection setup uses a special reply without the standard header, see:
  // https://www.x.org/releases/X11R7.6/doc/xproto/x11protocol.html#server_response
  // Don't try to parse it like a normal reply.
  if (setup_message)
    return;

  const auto* reply_header = reinterpret_cast<const ReplyHeader*>(data->data());

  // Only replies can have FDs, not events or errors.
  if (reply_header->response_type == kResponseTypeReply) {
    // All replies are at least 32 bytes.  The length field specifies the
    // amount of extra data in 4-byte multiples after the fixed 32 bytes.
    size_t reply_length = 32 + 4 * reply_header->length;

    // libxcb stores the fds after the reply data.
    fds = reinterpret_cast<const int*>(data->data() + reply_length);
  }
}

ReadBuffer::ReadBuffer(ReadBuffer&&) = default;

ReadBuffer::~ReadBuffer() = default;

scoped_refptr<base::RefCountedMemory> ReadBuffer::ReadAndAdvance(
    size_t length) {
  auto buf = base::MakeRefCounted<OffsetRefCountedMemory>(data, offset, length);
  offset += length;
  return buf;
}

int ReadBuffer::TakeFd() {
  return *fds++;
}

WriteBuffer::WriteBuffer() = default;

WriteBuffer::WriteBuffer(WriteBuffer&&) = default;

WriteBuffer::~WriteBuffer() = default;

void WriteBuffer::AppendBuffer(scoped_refptr<base::RefCountedMemory> buffer,
                               size_t size) {
  AppendCurrentBuffer();
  buffers_.push_back(buffer);
  offset_ += size;
}

std::vector<scoped_refptr<base::RefCountedMemory>>& WriteBuffer::GetBuffers() {
  if (!current_buffer_.empty())
    AppendCurrentBuffer();
  return buffers_;
}

void WriteBuffer::AppendCurrentBuffer() {
  buffers_.push_back(base::RefCountedBytes::TakeVector(&current_buffer_));
}

}  // namespace x11
