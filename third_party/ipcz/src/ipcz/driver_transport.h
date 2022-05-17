// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_IPCZ_DRIVER_TRANSPORT_H_
#define IPCZ_SRC_IPCZ_DRIVER_TRANSPORT_H_

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "ipcz/api_object.h"
#include "ipcz/driver_object.h"
#include "ipcz/ipcz.h"
#include "third_party/abseil-cpp/absl/base/macros.h"
#include "third_party/abseil-cpp/absl/types/span.h"
#include "util/ref_counted.h"

namespace ipcz {

class Message;
class Node;

// Encapsulates shared ownership of a transport endpoint created by an ipcz
// driver. The driver calls into this object to notify ipcz of incoming messages
// on the transport, and ipcz calls into this object to submit outgoing messages
// for transmission by the driver.
class DriverTransport
    : public APIObjectImpl<DriverTransport, APIObject::kTransport> {
 public:
  using Pair = std::pair<Ref<DriverTransport>, Ref<DriverTransport>>;

  // A view into the unowned raw contents of an incoming transport message.
  struct RawMessage {
    absl::Span<const uint8_t> data;
    absl::Span<const IpczDriverHandle> handles;
  };

  // A Listener to receive message and error events from the driver.
  class Listener {
   public:
    virtual ~Listener() = default;

    // Accepts a raw message from the transport. Note that this is called
    // without *any* validation of the size or content of `message`.
    virtual bool OnTransportMessage(const RawMessage& message,
                                    const DriverTransport& transport) = 0;

    // Indicates that some unrecoverable error has occurred with the transport.
    virtual void OnTransportError() = 0;
  };

  // Constructs a new DriverTransport object over the driver-created transport
  // handle in `transport`.
  explicit DriverTransport(DriverObject transport);

  // Set the object handling any incoming message or error notifications. This
  // is only safe to set before Activate() is called, or from within one of the
  // Listener methods when invoked by this DriverTransport (because invocations
  // are mutually exclusive). `listener` must outlive this DriverTransport.
  void set_listener(Listener* listener) { listener_ = listener; }

  // Exposes the underlying driver handle for this transport.
  const DriverObject& driver_object() const { return transport_; }

  // Releases ownership of the underlying driver transport, returning it to the
  // caller. After this call, the DriverTransport object is reset and
  // `driver_object()` will return an invalid object.
  DriverObject TakeDriverObject() {
    ABSL_ASSERT(!listener_);
    return std::move(transport_);
  }

  // Releases the driver handle so that it's no longer controlled by this
  // DriverTranport.
  IpczDriverHandle Release();

  // Begins listening on the transport for incoming data and driver objects.
  // Once this is called, the transport's Listener may be invoked by the driver
  // at any time from arbitrary threads, as determined by the driver
  // implementation itself. The driver will continue listening on this transport
  // until Deactivate() is called or an unrecoverable error is encountered.
  IpczResult Activate();

  // Requests that the driver cease listening for incoming data and driver
  // objects on this transport. Once a transport is deactivated, it can never be
  // reactivated.
  IpczResult Deactivate();

  // Helper for transmitting macro-generated ipcz messages. This performs any
  // necessary in-place serialization of driver objects before transmitting,
  // hence it takes a mutable reference to `message`.
  IpczResult Transmit(Message& message);

  // Invoked by the driver any time this transport receives data and driver
  // handles to be passed back into ipcz.
  bool Notify(const RawMessage& message);
  void NotifyError();

  // APIObject:
  IpczResult Close() override;

 private:
  ~DriverTransport() override;

  DriverObject transport_;

  Listener* listener_ = nullptr;
};

}  // namespace ipcz

#endif  // IPCZ_SRC_IPCZ_DRIVER_TRANSPORT_H_
