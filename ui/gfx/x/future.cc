// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/x/future.h"

#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/event.h"

namespace x11 {

FutureImpl::FutureImpl(Connection* connection,
                       SequenceType sequence,
                       bool generates_reply,
                       const char* request_name_for_tracing)
    : connection_(connection),
      sequence_(sequence),
      generates_reply_(generates_reply),
      request_name_for_tracing_(request_name_for_tracing) {}

void FutureImpl::Wait() {
  connection_->WaitForResponse(this);
}

void FutureImpl::DispatchNow() {
  Wait();
  ProcessResponse();
}

bool FutureImpl::AfterEvent(const Event& event) const {
  return CompareSequenceIds(event.sequence(), sequence_) > 0;
}

void FutureImpl::Sync(RawReply* raw_reply, std::unique_ptr<Error>* error) {
  Wait();
  TakeResponse(raw_reply, error);
}

void FutureImpl::OnResponse(ResponseCallback callback) {
  UpdateRequestHandler(std::move(callback));
}

void FutureImpl::UpdateRequestHandler(ResponseCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(connection_->sequence_checker_);
  CHECK(callback);

  auto* request = connection_->GetRequestForFuture(this);
  // Make sure we haven't processed this request yet.
  CHECK(request->callback);

  request->callback = std::move(callback);
}

void FutureImpl::ProcessResponse() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(connection_->sequence_checker_);

  auto* request = connection_->GetRequestForFuture(this);
  CHECK(request->callback);
  CHECK(request->have_response);

  std::move(request->callback)
      .Run(std::move(request->reply), std::move(request->error));
}

void FutureImpl::TakeResponse(RawReply* raw_reply,
                              std::unique_ptr<Error>* error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(connection_->sequence_checker_);

  auto* request = connection_->GetRequestForFuture(this);
  CHECK(request->callback);
  CHECK(request->have_response);

  *raw_reply = std::move(request->reply);
  *error = std::move(request->error);
  request->callback.Reset();
}

FutureBase::FutureBase() = default;

FutureBase::FutureBase(std::unique_ptr<FutureImpl> impl)
    : impl_(std::move(impl)) {}

FutureBase::FutureBase(FutureBase&&) = default;

FutureBase& FutureBase::operator=(FutureBase&&) = default;

FutureBase::~FutureBase() = default;

void FutureBase::Wait() {
  if (impl_) {
    impl_->Wait();
  }
}

void FutureBase::DispatchNow() {
  CHECK(impl_);
  impl_->DispatchNow();
}

bool FutureBase::AfterEvent(const Event& event) const {
  return impl_ ? impl_->AfterEvent(event) : false;
}

}  // namespace x11
