// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/driver_transport.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "ipcz/ipcz.h"
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

  return t->Notify(DriverTransport::Message(
      absl::MakeSpan(static_cast<const uint8_t*>(data), num_bytes),
      absl::MakeSpan(driver_handles, num_driver_handles)));
}

}  // namespace

DriverTransport::Message::Message(Data data) : data(data) {}

DriverTransport::Message::Message(Data data,
                                  absl::Span<const IpczDriverHandle> handles)
    : data(data), handles(handles) {}

DriverTransport::Message::Message(const Message&) = default;

DriverTransport::Message& DriverTransport::Message::operator=(const Message&) =
    default;

DriverTransport::Message::~Message() = default;

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

IpczResult DriverTransport::TransmitMessage(const Message& message) {
  return transport_.node()->driver().Transmit(
      transport_.handle(), message.data.data(), message.data.size(),
      message.handles.data(), message.handles.size(), IPCZ_NO_FLAGS, nullptr);
}

IpczResult DriverTransport::Transmit(internal::MessageBase& message) {
  ABSL_ASSERT(message.CanTransmitOn(*this));
  message.Serialize(*this);
  return TransmitMessage(
      Message(message.data_view(), message.transmissible_driver_handles()));
}

IpczResult DriverTransport::Notify(const Message& message) {
  ABSL_ASSERT(listener_);
  return listener_->OnTransportMessage(message);
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
