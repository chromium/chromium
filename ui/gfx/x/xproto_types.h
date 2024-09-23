// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/354829279): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef UI_GFX_X_XPROTO_TYPES_H_
#define UI_GFX_X_XPROTO_TYPES_H_

#include <cstdint>
#include <memory>

#include "base/component_export.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/free_deleter.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"

namespace x11 {

class Error;

// A memory buffer where the size of the memory buffer is unknown because its
// given as `void*` from a C api which expects us to dynamically cast it to
// another type later. Use of this type is not sound as a mistake will cause
// Undefined Behaviour.
class COMPONENT_EXPORT(X11) UnsizedRefCountedMemory
    : public base::RefCountedThreadSafe<UnsizedRefCountedMemory> {
 public:
  uint8_t* bytes() { return cast_to<uint8_t>(); }
  const uint8_t* bytes() const { return cast_to<const uint8_t>(); }

  // Converts the inner pointer to a `T*`. If the type is incorrect, this
  // results in Undefined Behaviour.
  template <class T>
  T* cast_to() {
    return reinterpret_cast<T*>(data());
  }
  template <class T>
    requires(std::is_const_v<T>)
  T* cast_to() const {
    return reinterpret_cast<T*>(data());
  }

 protected:
  friend class base::RefCountedThreadSafe<UnsizedRefCountedMemory>;
  virtual ~UnsizedRefCountedMemory() = default;

  virtual void* data() LIFETIME_BOUND = 0;
  virtual const void* data() const LIFETIME_BOUND = 0;
};

// Convert from a sized memory buffer to an unsized one, in order to use the
// buffer in void* APIs that pass the size separately.
class COMPONENT_EXPORT(X11) ThrowAwaySizeRefCountedMemory final
    : public UnsizedRefCountedMemory {
 public:
  static scoped_refptr<ThrowAwaySizeRefCountedMemory> From(
      std::vector<uint8_t> data) {
    return new ThrowAwaySizeRefCountedMemory(std::move(data));
  }

  ThrowAwaySizeRefCountedMemory(const ThrowAwaySizeRefCountedMemory&) = delete;
  ThrowAwaySizeRefCountedMemory& operator=(
      const ThrowAwaySizeRefCountedMemory&) = delete;

 private:
  explicit ThrowAwaySizeRefCountedMemory(std::vector<uint8_t> data);

  // UnsizedRefCountedMemory:
  void* data() LIFETIME_BOUND override;
  const void* data() const LIFETIME_BOUND override;

  ~ThrowAwaySizeRefCountedMemory() override;

  std::vector<uint8_t> data_;
};

// Convert from an unsized memory buffer to a sized one, by specifying the size.
class COMPONENT_EXPORT(X11) SizedRefCountedMemory final
    : public base::RefCountedMemory {
 public:
  // SAFETY: The caller must ensure that the `mem` buffer points to at least
  // `size` many bytes or Undefined Behaviour can result.
  UNSAFE_BUFFER_USAGE static scoped_refptr<SizedRefCountedMemory> From(
      scoped_refptr<UnsizedRefCountedMemory> mem,
      size_t size) {
    return new SizedRefCountedMemory(std::move(mem), size);
  }

  SizedRefCountedMemory(const SizedRefCountedMemory&) = delete;
  SizedRefCountedMemory& operator=(const SizedRefCountedMemory&) = delete;

 private:
  SizedRefCountedMemory(scoped_refptr<UnsizedRefCountedMemory> mem,
                        size_t size);

  // RefCountedMemory:
  base::span<const uint8_t> AsSpan() const LIFETIME_BOUND override;

  ~SizedRefCountedMemory() override;

  scoped_refptr<UnsizedRefCountedMemory> mem_;
  size_t size_;
};

using RawReply = scoped_refptr<UnsizedRefCountedMemory>;
using RawError = scoped_refptr<UnsizedRefCountedMemory>;
using ResponseCallback =
    base::OnceCallback<void(RawReply reply, std::unique_ptr<Error> error)>;

// xcb returns unsigned int when making requests.  This may be updated to
// uint16_t if/when we stop using xcb for socket IO.
using SequenceType = unsigned int;

constexpr uint8_t kSendEventMask = 0x80;

// Constants from the X11 protocol documentation:
// https://www.x.org/releases/X11R7.5/doc/x11proto/proto.html
inline constexpr size_t kMinimumErrorSize = 32;
inline constexpr size_t kMinimumEventSize = 32;

namespace detail {

template <typename T>
void VerifyAlignment(T* t, size_t offset) {
  // On the wire, X11 types are always aligned to their size.  This is a sanity
  // check to ensure padding etc are working properly.
  if (sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8) {
    CHECK_EQ(offset % sizeof(*t), 0UL);
  }
}

}  // namespace detail

// Wraps data read from the connection.
struct COMPONENT_EXPORT(X11) ReadBuffer {
  explicit ReadBuffer(scoped_refptr<UnsizedRefCountedMemory> data,
                      bool setup_message = false);

  ReadBuffer(const ReadBuffer&) = delete;
  ReadBuffer(ReadBuffer&&);

  ~ReadBuffer();

  scoped_refptr<UnsizedRefCountedMemory> ReadAndAdvance(size_t length);

  int TakeFd();

  scoped_refptr<UnsizedRefCountedMemory> data;
  size_t offset = 0;
  raw_ptr<const int, AllowPtrArithmetic> fds = nullptr;
};

// Wraps data to write to the connection.
class COMPONENT_EXPORT(X11) WriteBuffer {
 public:
  WriteBuffer();

  WriteBuffer(const WriteBuffer&) = delete;
  WriteBuffer(WriteBuffer&&);

  ~WriteBuffer();

  // Safety: The `buffer` must point to at least `size` many bytes.
  UNSAFE_BUFFER_USAGE void AppendBuffer(
      scoped_refptr<UnsizedRefCountedMemory> buffer,
      size_t size);

  void AppendSizedBuffer(scoped_refptr<base::RefCountedMemory> buffer);

  base::span<base::span<uint8_t>> GetBuffers();

  // Advance the pointer in the first buffer by `offset`.
  void OffsetFirstBuffer(size_t offset);

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

  std::vector<scoped_refptr<UnsizedRefCountedMemory>> owned_buffers_;
  std::vector<base::span<uint8_t>> sized_buffers_;
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

#endif  // UI_GFX_X_XPROTO_TYPES_H_
