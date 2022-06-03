// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_X_FUTURE_H_
#define UI_GFX_X_FUTURE_H_

#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/xproto_types.h"

namespace x11 {

// An Future wraps an asynchronous response from the X11 server.  The
// response may be waited-for with Sync(), or asynchronously handled by
// installing a response handler using OnResponse().
template <typename Reply>
class Future {
 public:
  using Callback = base::OnceCallback<void(Response<Reply> response)>;

  Future() = default;

  explicit Future(std::unique_ptr<Connection::FutureImpl> impl)
      : impl_(std::move(impl)) {}

  // Blocks until we receive the response from the server. Returns the response.
  Response<Reply> Sync() {
    if (!impl_)
      return {nullptr, nullptr};

    Connection::RawReply raw_reply;
    std::unique_ptr<Error> error;
    impl_->Sync(&raw_reply, &error);

    std::unique_ptr<Reply> reply;
    if (raw_reply) {
      auto buf = ReadBuffer(raw_reply);
      reply = detail::ReadReply<Reply>(&buf);
    }

    return {std::move(reply), std::move(error)};
  }

  // Block until this request is handled by the server.  Unlike Sync(), this
  // method doesn't return the response.  Rather, it calls the response
  // handler installed for this request out-of-order.
  void Wait() {
    if (impl_)
      impl_->Wait();
  }

  // Installs |callback| to be run when the response is received.
  void OnResponse(Callback callback) {
    if (!impl_)
      return;

    // This intermediate callback handles the conversion from |raw_reply| to a
    // real Reply object before feeding the result to |callback|.  This means
    // |callback| must be bound as the first argument of the intermediate
    // function.
    auto wrapper = [](Callback callback, Connection::RawReply raw_reply,
                      std::unique_ptr<Error> error) {
      std::unique_ptr<Reply> reply;
      if (raw_reply) {
        ReadBuffer buf(raw_reply);
        reply = detail::ReadReply<Reply>(&buf);
      }
      std::move(callback).Run({std::move(reply), std::move(error)});
    };
    impl_->OnResponse(base::BindOnce(wrapper, std::move(callback)));
  }

  void IgnoreError() {
    OnResponse(base::BindOnce([](Response<Reply>) {}));
  }

 private:
  std::unique_ptr<Connection::FutureImpl> impl_;
};

// Sync() specialization for requests that don't generate replies.  The returned
// response will only contain an error if there was one.
template <>
inline Response<void> Future<void>::Sync() {
  if (!impl_)
    return Response<void>{nullptr};

  Connection::RawReply raw_reply;
  std::unique_ptr<Error> error;
  impl_->Sync(&raw_reply, &error);
  DCHECK(!raw_reply);
  return Response<void>(std::move(error));
}

// OnResponse() specialization for requests that don't generate replies.  The
// response argument to |callback| will only contain an error if there was one.
template <>
inline void Future<void>::OnResponse(Callback callback) {
  if (!impl_)
    return;

  // See Future<Reply>::OnResponse() for an explanation of why
  // this wrapper is necessary.
  auto wrapper = [](Callback callback, Connection::RawReply reply,
                    std::unique_ptr<Error> error) {
    DCHECK(!reply);
    std::move(callback).Run(Response<void>{std::move(error)});
  };
  impl_->OnResponse(base::BindOnce(wrapper, std::move(callback)));
}

}  // namespace x11

#endif  //  UI_GFX_X_FUTURE_H_
