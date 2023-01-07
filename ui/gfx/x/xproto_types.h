// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_X_XPROTO_TYPES_H_
#define UI_GFX_X_XPROTO_TYPES_H_

#include <cstdint>
#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/component_export.h"
#include "base/memory/free_deleter.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"

namespace x11 {

class Error;

constexpr uint8_t kSendEventMask = 0x80;

namespace detail {

template <typename T>
void VerifyAlignment(T* t, size_t offset) {
  // On the wire, X11 types are always aligned to their size.  This is a sanity
  // check to ensure padding etc are working properly.
  if (sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8)
    DCHECK_EQ(offset % sizeof(*t), 0UL);
}

}  // namespace detail

// Wraps data read from the connection.
struct COMPONENT_EXPORT(X11) ReadBuffer {
  explicit ReadBuffer(scoped_refptr<base::RefCountedMemory> data,
                      bool setup_message = false);

  ReadBuffer(const ReadBuffer&) = delete;
  ReadBuffer(ReadBuffer&&);

  ~ReadBuffer();

  scoped_refptr<base::RefCountedMemory> ReadAndAdvance(size_t length);

  int TakeFd();

  scoped_refptr<base::RefCountedMemory> data;
  size_t offset = 0;
  raw_ptr<const int> fds = nullptr;
};

// Wraps data to write to the connection.
class COMPONENT_EXPORT(X11) WriteBuffer {
 public:
  WriteBuffer();

  WriteBuffer(const WriteBuffer&) = delete;
  WriteBuffer(WriteBuffer&&);

  ~WriteBuffer();

  void AppendBuffer(scoped_refptr<base::RefCountedMemory> buffer, size_t size);

  std::vector<scoped_refptr<base::RefCountedMemory>>& GetBuffers();

  size_t offset() const { return offset_; }

  std::vector<int>& fds() { return fds_; }

  template <typename T>
  void Write(const T* t) {
    static_assert(std::is_trivially_copyable<T>::value, "");
    detail::VerifyAlignment(t, offset_);
    const uint8_t* start = reinterpret_cast<const uint8_t*>(t);
    std::copy(start, start + sizeof(*t), std::back_inserter(current_buffer_));
    offset_ += sizeof(*t);
  }

 private:
  void AppendCurrentBuffer();

  std::vector<scoped_refptr<base::RefCountedMemory>> buffers_;
  std::vector<uint8_t> current_buffer_;
  size_t offset_ = 0;
  std::vector<int> fds_;
};

namespace detail {

template <typename Reply>
std::unique_ptr<Reply> ReadReply(ReadBuffer* buffer);

}  // namespace detail

template <class Reply>
class Future;

template <typename T>
T Read(ReadBuffer* buf);

template <typename T>
WriteBuffer Write(const T& t);

template <typename T>
void ReadEvent(T* event, ReadBuffer* buf);

template <typename Reply>
struct Response {
  Response(std::unique_ptr<Reply> reply, std::unique_ptr<Error> error)
      : reply(std::move(reply)), error(std::move(error)) {}

  operator bool() const { return reply.get(); }
  const Reply* operator->() const { return reply.get(); }
  Reply* operator->() { return reply.get(); }

  std::unique_ptr<Reply> reply;
  std::unique_ptr<Error> error;
};

template <>
struct Response<void> {
  std::unique_ptr<Error> error;

 private:
  friend class Future<void>;

  explicit Response(std::unique_ptr<Error> error) : error(std::move(error)) {}
};

}  // namespace x11

#endif  //  UI_GFX_X_XPROTO_TYPES_H_
