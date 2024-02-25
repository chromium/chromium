// Copyright 2022 The Chromium Authors
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

IpczResult IPCZ_API NotifyTransport(IpczHandle listener,
                                    const void* data,
                                    size_t num_bytes,
                                    const IpczDriverHandle* driver_handles,
                                    size_t num_driver_handles,
                                    IpczTransportActivityFlags flags,
                                    const void* options) {
  DriverTransport* t = DriverTransport::FromHandle(listener);
  if (!t) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  if (flags & IPCZ_TRANSPORT_ACTIVITY_DEACTIVATED) {
    const Ref<DriverTransport> doomed_transport =
        DriverTransport::TakeFromHandle(listener);
    doomed_transport->NotifyDeactivated();
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

// static
DriverTransport::Pair DriverTransport::CreatePair(
    const IpczDriver& driver,
    const DriverTransport* transport0,
    const DriverTransport* transport1) {
  IpczDriverHandle new_transport0;
  IpczDriverHandle new_transport1;
  IpczDriverHandle target_transport0 = IPCZ_INVALID_DRIVER_HANDLE;
  IpczDriverHandle target_transport1 = IPCZ_INVALID_DRIVER_HANDLE;
  if (transport0) {
    ABSL_HARDENING_ASSERT(transport1);
    target_transport0 = transport0->driver_object().handle();
    target_transport1 = transport1->driver_object().handle();
  }
  IpczResult result = driver.CreateTransports(
      target_transport0, target_transport1, IPCZ_NO_FLAGS, nullptr,
      &new_transport0, &new_transport1);
  ABSL_HARDENING_ASSERT(result == IPCZ_RESULT_OK);
  auto first =
      MakeRefCounted<DriverTransport>(DriverObject(driver, new_transport0));
  auto second =
      MakeRefCounted<DriverTransport>(DriverObject(driver, new_transport1));
  return {std::move(first), std::move(second)};
}

IpczDriverHandle DriverTransport::Release() {
  return transport_.release();
}

IpczResult DriverTransport::Activate() {
  // Acquire a self-reference, balanced in NotifyTransport() when the driver
  // invokes its activity handler with IPCZ_TRANSPORT_ACTIVITY_DEACTIVATED.
  IpczHandle handle = ReleaseAsHandle(WrapRefCounted(this));
  return transport_.driver()->ActivateTransport(
      transport_.handle(), handle, NotifyTransport, IPCZ_NO_FLAGS, nullptr);
}

IpczResult DriverTransport::Deactivate() {
  if (!transport_.is_valid()) {
    // The transport is already deactivated. Avoids a null dereference.
    return IPCZ_RESULT_FAILED_PRECONDITION;
  }
  return transport_.driver()->DeactivateTransport(transport_.handle(),
                                                  IPCZ_NO_FLAGS, nullptr);
}

IpczResult DriverTransport::Transmit(Message& message) {
  ABSL_ASSERT(message.CanTransmitOn(*this));
  if (!message.Serialize(*this)) {
    // If serialization fails despite the object appearing to be serializable,
    // we have to assume the transport is in a dysfunctional state and will be
    // torn down by the driver soon. Discard the transmission.
    return IPCZ_RESULT_FAILED_PRECONDITION;
  }

  const absl::Span<const uint8_t> data = message.data_view();
  const absl::Span<const IpczDriverHandle> handles =
      message.transmissible_driver_handles();
  return transport_.driver()->Transmit(transport_.handle(), data.data(),
                                       data.size(), handles.data(),
                                       handles.size(), IPCZ_NO_FLAGS, nullptr);
}

bool DriverTransport::Notify(const RawMessage& message) {
  ABSL_ASSERT(listener_);
  // Listener methods may set a new Listener on this DriverTransport, and that
  // may drop their own last reference. Keep a reference here to ensure this
  // Listener remains alive through the extent of its notification.
  Ref<Listener> listener = listener_;
  return listener->OnTransportMessage(message, *this);
}

void DriverTransport::NotifyError() {
  ABSL_ASSERT(listener_);
  // Listener methods may set a new Listener on this DriverTransport, and that
  // may drop their own last reference. Keep a reference here to ensure this
  // Listener remains alive through the extent of its notification.
  Ref<Listener> listener = listener_;
  return listener->OnTransportError();
}

void DriverTransport::NotifyDeactivated() {
  ABSL_ASSERT(listener_);
  Ref<Listener> listener = std::move(listener_);
  listener->OnTransportDeactivated();
}

IpczResult DriverTransport::Close() {
  // Applications should not close transport handles provided to the driver
  // by ActivateTransport(). These handles are automatically closed on
  // deactivation by ipcz, or when the driver signals an unrecoverable error via
  // IPCZ_TRANSPORT_ACTIVITY_ERROR.
  return IPCZ_RESULT_INVALID_ARGUMENT;
}

}  // namespace ipcz
