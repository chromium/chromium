// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_X_XPROTO_TYPES_H_
#define UI_GFX_X_XPROTO_TYPES_H_

#include <xcb/xcb.h>

#include <cstdint>
#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "base/memory/free_deleter.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"

typedef struct _XDisplay XDisplay;

namespace x11 {

class Connection;

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
  explicit ReadBuffer(scoped_refptr<base::RefCountedMemory> data);

  ReadBuffer(const ReadBuffer&) = delete;
  ReadBuffer(ReadBuffer&&);

  ~ReadBuffer();

  scoped_refptr<base::RefCountedMemory> ReadAndAdvance(size_t length);

  int TakeFd();

  scoped_refptr<base::RefCountedMemory> data;
  size_t offset = 0;
  const int* fds = nullptr;
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

using Error = xcb_generic_error_t;

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
  operator bool() const { return reply.get(); }
  const Reply* operator->() const { return reply.get(); }
  Reply* operator->() { return reply.get(); }

  std::unique_ptr<Reply> reply;
  std::unique_ptr<Error, base::FreeDeleter> error;

 private:
  friend class Future<Reply>;

  Response(std::unique_ptr<Reply> reply,
           std::unique_ptr<Error, base::FreeDeleter> error)
      : reply(std::move(reply)), error(std::move(error)) {}
};

template <>
struct Response<void> {
  std::unique_ptr<Error, base::FreeDeleter> error;

 private:
  friend class Future<void>;

  explicit Response(std::unique_ptr<Error, base::FreeDeleter> error)
      : error(std::move(error)) {}
};

class COMPONENT_EXPORT(X11) FutureBase {
 public:
  using RawReply = scoped_refptr<base::RefCountedMemory>;
  using RawError = std::unique_ptr<xcb_generic_error_t, base::FreeDeleter>;
  using ResponseCallback =
      base::OnceCallback<void(RawReply reply, RawError error)>;

  FutureBase(const FutureBase&) = delete;
  FutureBase& operator=(const FutureBase&) = delete;

 protected:
  FutureBase(Connection* connection, base::Optional<unsigned int> sequence);
  ~FutureBase();

  FutureBase(FutureBase&& future);
  FutureBase& operator=(FutureBase&& future);

  void SyncImpl(Error** raw_error,
                scoped_refptr<base::RefCountedMemory>* raw_reply);
  void SyncImpl(Error** raw_error);

  void OnResponseImpl(ResponseCallback callback);

 private:
  Connection* connection_;
  base::Optional<unsigned int> sequence_;
};

// An x11::Future wraps an asynchronous response from the X11 server.  The
// response may be waited-for with Sync(), or asynchronously handled by
// installing a response handler using OnResponse().
template <typename Reply>
class Future : public FutureBase {
 public:
  using Callback = base::OnceCallback<void(Response<Reply> response)>;

  Future() : FutureBase(nullptr, base::nullopt) {}

  // Blocks until we receive the response from the server. Returns the response.
  Response<Reply> Sync() {
    Error* raw_error = nullptr;
    scoped_refptr<base::RefCountedMemory> raw_reply;
    SyncImpl(&raw_error, &raw_reply);

    std::unique_ptr<Reply> reply;
    if (raw_reply) {
      auto buf = ReadBuffer(raw_reply);
      reply = detail::ReadReply<Reply>(&buf);
    }

    std::unique_ptr<Error, base::FreeDeleter> error;
    if (raw_error)
      error.reset(raw_error);

    return {std::move(reply), std::move(error)};
  }

  // Installs |callback| to be run when the response is received.
  void OnResponse(Callback callback) {
    // This intermediate callback handles the conversion from |raw_reply| to a
    // real Reply object before feeding the result to |callback|.  This means
    // |callback| must be bound as the first argument of the intermediate
    // function.
    auto wrapper = [](Callback callback, RawReply raw_reply, RawError error) {
      ReadBuffer buf(raw_reply);
      std::unique_ptr<Reply> reply =
          raw_reply ? detail::ReadReply<Reply>(&buf) : nullptr;
      std::move(callback).Run({std::move(reply), std::move(error)});
    };
    OnResponseImpl(base::BindOnce(wrapper, std::move(callback)));
  }

  void IgnoreError() {
    OnResponse(base::BindOnce([](Response<Reply>) {}));
  }

 private:
  template <typename R>
  friend Future<R> SendRequest(Connection*, WriteBuffer*, bool);

  Future(Connection* connection, base::Optional<unsigned int> sequence)
      : FutureBase(connection, sequence) {}
};

// Sync() specialization for requests that don't generate replies.  The returned
// response will only contain an error if there was one.
template <>
inline Response<void> Future<void>::Sync() {
  Error* raw_error = nullptr;
  SyncImpl(&raw_error);

  std::unique_ptr<Error, base::FreeDeleter> error;
  if (raw_error)
    error.reset(raw_error);

  return Response<void>{std::move(error)};
}

// OnResponse() specialization for requests that don't generate replies.  The
// response argument to |callback| will only contain an error if there was one.
template <>
inline void Future<void>::OnResponse(Callback callback) {
  // See Future<Reply>::OnResponse() for an explanation of why
  // this wrapper is necessary.
  auto wrapper = [](Callback callback, RawReply reply, RawError error) {
    DCHECK(!reply);
    std::move(callback).Run(Response<void>{std::move(error)});
  };
  OnResponseImpl(base::BindOnce(wrapper, std::move(callback)));
}

template <>
inline void Future<void>::IgnoreError() {
  OnResponse(base::BindOnce([](Response<void>) {}));
}

}  // namespace x11

#endif  //  UI_GFX_X_XPROTO_TYPES_H_
