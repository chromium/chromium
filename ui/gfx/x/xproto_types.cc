// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/354829279): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

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

ThrowAwaySizeRefCountedMemory::ThrowAwaySizeRefCountedMemory(
    std::vector<uint8_t> data)
    : data_(std::move(data)) {}

ThrowAwaySizeRefCountedMemory::~ThrowAwaySizeRefCountedMemory() = default;

void* ThrowAwaySizeRefCountedMemory::data() {
  return data_.data();
}

const void* ThrowAwaySizeRefCountedMemory::data() const {
  return data_.data();
}

SizedRefCountedMemory::SizedRefCountedMemory(
    scoped_refptr<UnsizedRefCountedMemory> mem,
    size_t size)
    : mem_(std::move(mem)), size_(size) {}

SizedRefCountedMemory::~SizedRefCountedMemory() = default;

base::span<const uint8_t> SizedRefCountedMemory::AsSpan() const {
  // SAFETY: This relies on the constructor being called with a valid buffer
  // and size pair.
  return UNSAFE_BUFFERS(base::span(mem_->bytes(), size_));
}

ReadBuffer::ReadBuffer(scoped_refptr<UnsizedRefCountedMemory> data,
                       bool setup_message)
    : data(data) {
  // X connection setup uses a special reply without the standard header, see:
  // https://www.x.org/releases/X11R7.6/doc/xproto/x11protocol.html#server_response
  // Don't try to parse it like a normal reply.
  if (setup_message)
    return;

  const ReplyHeader* reply_header = data->cast_to<const ReplyHeader>();

  // Only replies can have FDs, not events or errors.
  if (reply_header->response_type == kResponseTypeReply) {
    // All replies are at least 32 bytes.  The length field specifies the
    // amount of extra data in 4-byte multiples after the fixed 32 bytes.
    size_t reply_length = 32 + 4 * reply_header->length;

    // libxcb stores the fds after the reply data.
    fds = reinterpret_cast<const int*>(data->bytes() + reply_length);
  }
}

ReadBuffer::ReadBuffer(ReadBuffer&&) = default;

ReadBuffer::~ReadBuffer() = default;

scoped_refptr<UnsizedRefCountedMemory> ReadBuffer::ReadAndAdvance(
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

void WriteBuffer::AppendBuffer(scoped_refptr<UnsizedRefCountedMemory> buffer,
                               size_t size) {
  AppendCurrentBuffer();
  sized_buffers_.push_back(
      // SAFETY: This relies on the caller to pass a correct size, as enforced
      // by UNSAFE_BUFFER_USAGE in header.
      UNSAFE_BUFFERS(base::span(buffer->bytes(), size)));
  owned_buffers_.push_back(buffer);
  offset_ += size;
}

void WriteBuffer::AppendSizedBuffer(
    scoped_refptr<base::RefCountedMemory> buffer) {
  AppendCurrentBuffer();
  std::vector<uint8_t> v(buffer->size());
  base::span(v).copy_from(*buffer);
  sized_buffers_.push_back(v);
  owned_buffers_.push_back(ThrowAwaySizeRefCountedMemory::From(std::move(v)));
  offset_ += buffer->size();
}

base::span<base::span<uint8_t>> WriteBuffer::GetBuffers() {
  if (!current_buffer_.empty())
    AppendCurrentBuffer();
  return sized_buffers_;
}

void WriteBuffer::OffsetFirstBuffer(size_t offset) {
  sized_buffers_[0u] = sized_buffers_[0u].subspan(offset);
}

void WriteBuffer::AppendCurrentBuffer() {
  sized_buffers_.push_back(base::span(current_buffer_));
  owned_buffers_.push_back(
      ThrowAwaySizeRefCountedMemory::From(std::move(current_buffer_)));
  current_buffer_.clear();
}

}  // namespace x11
