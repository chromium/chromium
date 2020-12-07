// Copyright 2020 The Chromium Authors. All rights reserved.
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

ReadBuffer::ReadBuffer(scoped_refptr<base::RefCountedMemory> data)
    : data(data) {
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

FutureBase::FutureBase(Connection* connection,
                       base::Optional<unsigned int> sequence,
                       const char* request_name,
                       bool generates_reply)
    : connection_(connection),
      sequence_valid_(sequence.has_value()),
      sequence_(sequence_valid_ ? sequence.value() : 0) {
  if (!sequence_valid_)
    return;
  // Install a default response-handler that throws away the reply and prints
  // the error if there is one.  This handler may be overridden by clients.
  connection_->AddRequest(
      sequence_,
      base::BindOnce(
          [](Connection* connection, const char* request_name,
             Connection::ErrorHandler error_handler, RawReply raw_reply,
             RawError raw_error) {
            if (!raw_error)
              return;

            auto error = connection->ParseError(raw_error);
            error_handler.Run(error.get(), request_name);
          },
          connection_, request_name, connection_->error_handler_),
      request_name, generates_reply);
}

void FutureBase::Wait() {
  if (!sequence_valid_)
    return;
  connection_->WaitForResponse(sequence_);
  connection_->ProcessResponse(sequence_);
}

void FutureBase::SyncImpl(RawError* raw_reply, RawReply* raw_error) {
  if (!sequence_valid_)
    return;
  connection_->WaitForResponse(sequence_);
  connection_->TakeResponse(sequence_, raw_reply, raw_error);
}

void FutureBase::OnResponseImpl(ResponseCallback callback) {
  if (!sequence_valid_)
    return;
  connection_->UpdateRequestHandler(sequence_, std::move(callback));
}

// static
std::unique_ptr<Error> FutureBase::ParseErrorImpl(x11::Connection* connection,
                                                  RawError raw_error) {
  if (!raw_error)
    return nullptr;
  return connection->ParseError(raw_error);
}

}  // namespace x11
