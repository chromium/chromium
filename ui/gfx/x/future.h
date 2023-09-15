// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_X_FUTURE_H_
#define UI_GFX_X_FUTURE_H_

#include "base/component_export.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/xproto_types.h"

namespace x11 {

class Event;

class COMPONENT_EXPORT(X11) FutureBase {
 public:
  FutureBase();
  explicit FutureBase(std::unique_ptr<Connection::FutureImpl> impl);
  FutureBase(FutureBase&&);
  FutureBase& operator=(FutureBase&&);
  ~FutureBase();

  // Block until this request is handled by the server.
  void Wait();

  // Block until this request is handled by the server.  Unlike Sync(), this
  // method doesn't return the response.  Rather, it calls the response
  // handler installed for this request out-of-order.
  void DispatchNow();

  // Returns true iff the response for this request was received after `event`.
  bool AfterEvent(const Event& event) const;

 protected:
  Connection::FutureImpl* impl() { return impl_.get(); }

 private:
  std::unique_ptr<Connection::FutureImpl> impl_;
};

// An Future wraps an asynchronous response from the X11 server.  The
// response may be waited-for with Sync(), or asynchronously handled by
// installing a response handler using OnResponse().
template <typename Reply>
class Future : public FutureBase {
 public:
  using Callback = base::OnceCallback<void(Response<Reply> response)>;

  Future() = default;

  explicit Future(std::unique_ptr<Connection::FutureImpl> impl)
      : FutureBase(std::move(impl)) {
    static_assert(sizeof(Future<Reply>) == sizeof(FutureBase),
                  "Future must not have any members so that it can be sliced "
                  "to FutureBase");
  }

  // Blocks until we receive the response from the server. Returns the response.
  Response<Reply> Sync() {
    if (!impl())
      return {nullptr, nullptr};

    Connection::RawReply raw_reply;
    std::unique_ptr<Error> error;
    impl()->Sync(&raw_reply, &error);

    std::unique_ptr<Reply> reply;
    if (raw_reply) {
      auto buf = ReadBuffer(raw_reply);
      reply = detail::ReadReply<Reply>(&buf);
    }

    return {std::move(reply), std::move(error)};
  }

  // Installs |callback| to be run when the response is received.
  void OnResponse(Callback callback) {
    if (!impl())
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
    impl()->OnResponse(base::BindOnce(wrapper, std::move(callback)));
  }

  void IgnoreError() {
    OnResponse(base::BindOnce([](Response<Reply>) {}));
  }
};

// Sync() specialization for requests that don't generate replies.  The returned
// response will only contain an error if there was one.
template <>
inline Response<void> Future<void>::Sync() {
  if (!impl())
    return Response<void>{nullptr};

  Connection::RawReply raw_reply;
  std::unique_ptr<Error> error;
  impl()->Sync(&raw_reply, &error);
  DUMP_WILL_BE_CHECK(!raw_reply);
  return Response<void>(std::move(error));
}

// OnResponse() specialization for requests that don't generate replies.  The
// response argument to |callback| will only contain an error if there was one.
template <>
inline void Future<void>::OnResponse(Callback callback) {
  if (!impl())
    return;

  // See Future<Reply>::OnResponse() for an explanation of why
  // this wrapper is necessary.
  auto wrapper = [](Callback callback, Connection::RawReply reply,
                    std::unique_ptr<Error> error) {
    DUMP_WILL_BE_CHECK(!reply);
    std::move(callback).Run(Response<void>{std::move(error)});
  };
  impl()->OnResponse(base::BindOnce(wrapper, std::move(callback)));
}

}  // namespace x11

#endif  //  UI_GFX_X_FUTURE_H_
