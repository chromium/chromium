// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/driver_transport.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "ipcz/ipcz.h"
#include "ipcz/message.h"
#include "ipcz/node.h"
#include "third_party/abseil-cpp/absl/base/macros.h"
#include "third_party/abseil-cpp/absl/types/span.h"
#include "util/ref_counted.h"

namespace ipcz {

namespace {

IpczResult IPCZ_API NotifyTransport(IpczHandle transport,
                                    const void* data,
                                    size_t num_bytes,
                                    const IpczDriverHandle* driver_handles,
                                    size_t num_driver_handles,
                                    IpczTransportActivityFlags flags,
                                    const void* options) {
  DriverTransport* t = DriverTransport::FromHandle(transport);
  if (!t) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  if (flags & IPCZ_TRANSPORT_ACTIVITY_DEACTIVATED) {
    const Ref<DriverTransport> doomed_transport =
        DriverTransport::TakeFromHandle(transport);
    return IPCZ_RESULT_OK;
  }

  if (flags & IPCZ_TRANSPORT_ACTIVITY_ERROR) {
    t->NotifyError();
    return IPCZ_RESULT_OK;
  }

  if (!t->Notify({absl::MakeSpan(static_cast<const uint8_t*>(data), num_bytes),
                  absl::MakeSpan(driver_handles, num_driver_handles)})) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  return IPCZ_RESULT_OK;
}

}  // namespace

DriverTransport::DriverTransport(DriverObject transport)
    : transport_(std::move(transport)) {}

DriverTransport::~DriverTransport() = default;

IpczDriverHandle DriverTransport::Release() {
  return transport_.release();
}

IpczResult DriverTransport::Activate() {
  // Acquire a self-reference, balanced in NotifyTransport() when the driver
  // invokes its activity handler with IPCZ_TRANSPORT_ACTIVITY_DEACTIVATED.
  IpczHandle handle = ReleaseAsHandle(WrapRefCounted(this));
  return transport_.node()->driver().ActivateTransport(
      transport_.handle(), handle, NotifyTransport, IPCZ_NO_FLAGS, nullptr);
}

IpczResult DriverTransport::Deactivate() {
  return transport_.node()->driver().DeactivateTransport(
      transport_.handle(), IPCZ_NO_FLAGS, nullptr);
}

IpczResult DriverTransport::Transmit(Message& message) {
  ABSL_ASSERT(message.CanTransmitOn(*this));
  message.Serialize(*this);

  const absl::Span<const uint8_t> data = message.data_view();
  const absl::Span<const IpczDriverHandle> handles =
      message.transmissible_driver_handles();
  return transport_.node()->driver().Transmit(
      transport_.handle(), data.data(), data.size(), handles.data(),
      handles.size(), IPCZ_NO_FLAGS, nullptr);
}

bool DriverTransport::Notify(const RawMessage& message) {
  ABSL_ASSERT(listener_);
  return listener_->OnTransportMessage(message, *this);
}

void DriverTransport::NotifyError() {
  ABSL_ASSERT(listener_);
  listener_->OnTransportError();
}

IpczResult DriverTransport::Close() {
  // Applications should not close transport handles provided to the driver
  // by ActivateTransport(). These handles are automatically closed on
  // deactivation by ipcz, or when the driver signals an unrecoverable error via
  // IPCZ_TRANSPORT_ACTIVITY_ERROR.
  return IPCZ_RESULT_INVALID_ARGUMENT;
}

}  // namespace ipcz
