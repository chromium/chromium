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

class Node;

// Encapsulates shared ownership of a transport endpoint created by an ipcz
// driver. The driver calls into this object to notify ipcz of incoming messages
// on the transport, and ipcz calls into this object to submit outgoing messages
// for transmission by the driver.
class DriverTransport
    : public APIObjectImpl<DriverTransport, APIObject::kTransport> {
 public:
  using Pair = std::pair<Ref<DriverTransport>, Ref<DriverTransport>>;
  using Data = absl::Span<const uint8_t>;

  // A view into a transport message. Does not own the underlying data or
  // handles.
  struct Message {
    explicit Message(Data data);
    Message(Data data, absl::Span<const IpczDriverHandle> handles);
    Message(const Message&);
    Message& operator=(const Message&);
    ~Message();

    Data data;
    absl::Span<const IpczDriverHandle> handles;
  };

  // A Listener to receive message and error events from the driver.
  class Listener {
   public:
    virtual ~Listener() = default;

    // Accepts a raw message from the transport. Note that this is called
    // without *any* validation of the size or content of `message`.
    virtual IpczResult OnTransportMessage(const Message& message) = 0;

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

  // Asks the driver to submit the data and driver objects in `message` for
  // transmission from this transport endpoint to the opposite endpoint.
  IpczResult TransmitMessage(const Message& message);

  // Templated helper for transmitting macro-generated ipcz messages. This
  // performs any necessary in-place serialization of driver objects before
  // transmitting.
  template <typename T>
  IpczResult Transmit(T& message) {
    if (!message.Serialize(*this)) {
      return IPCZ_RESULT_INVALID_ARGUMENT;
    }
    return TransmitMessage(
        Message(message.data_view(), message.transmissible_driver_handles()));
  }

  // Invoked by the driver any time this transport receives data and driver
  // handles to be passed back into ipcz.
  IpczResult Notify(const Message& message);
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
