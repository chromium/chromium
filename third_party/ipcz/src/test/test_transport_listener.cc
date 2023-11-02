// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/test_transport_listener.h"

#include <utility>

#include "ipcz/driver_object.h"
#include "ipcz/driver_transport.h"
#include "ipcz/message.h"
#include "ipcz/node.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/base/macros.h"
#include "third_party/abseil-cpp/absl/synchronization/notification.h"
#include "util/ref_counted.h"

namespace ipcz::test {

class TestTransportListener::ListenerImpl : public DriverTransport::Listener {
 public:
  ListenerImpl() = default;

  void set_message_handler(
      TestTransportListener::GenericMessageHandler handler) {
    ABSL_ASSERT(!message_handler_);
    message_handler_ = std::move(handler);
  }

  void set_error_handler(TestTransportListener::ErrorHandler handler) {
    ABSL_ASSERT(!error_handler_);
    error_handler_ = std::move(handler);
  }

  void WaitForDeactivation() { deactivation_.WaitForNotification(); }

  // DriverTransport::Listener:
  bool OnTransportMessage(const DriverTransport::RawMessage& message,
                          const DriverTransport& transport) override {
    ABSL_ASSERT(message_handler_);
    return message_handler_(message);
  }

  void OnTransportError() override {
    if (error_handler_) {
      error_handler_();
    }
  }

  void OnTransportDeactivated() override { deactivation_.Notify(); }

 private:
  ~ListenerImpl() override = default;

  TestTransportListener::GenericMessageHandler message_handler_ = nullptr;
  TestTransportListener::ErrorHandler error_handler_ = nullptr;
  absl::Notification deactivation_;
};

TestTransportListener::TestTransportListener(IpczHandle node,
                                             IpczDriverHandle handle)
    : TestTransportListener(MakeRefCounted<DriverTransport>(
          DriverObject(reinterpret_cast<Node*>(node)->driver(), handle))) {}

TestTransportListener::TestTransportListener(Ref<DriverTransport> transport)
    : transport_(std::move(transport)),
      listener_(MakeRefCounted<ListenerImpl>()) {
  transport_->set_listener(listener_);
}

TestTransportListener::~TestTransportListener() {
  StopListening();
}

void TestTransportListener::StopListening() {
  if (!activated_) {
    return;
  }

  transport_->Deactivate();
  activated_ = false;
  listener_->WaitForDeactivation();
}

void TestTransportListener::OnRawMessage(GenericMessageHandler handler) {
  listener_->set_message_handler(std::move(handler));
  ActivateTransportIfNecessary();
}

void TestTransportListener::OnStringMessage(
    std::function<void(std::string_view)> handler) {
  OnRawMessage([handler](const DriverTransport::RawMessage& message) {
    EXPECT_TRUE(message.handles.empty());
    handler(std::string_view(reinterpret_cast<const char*>(message.data.data()),
                             message.data.size()));
    return true;
  });
}

void TestTransportListener::OnError(ErrorHandler handler) {
  listener_->set_error_handler(std::move(handler));

  // Since the caller only cares about handling errors, ensure all valid
  // messages are cleanly discarded. This also activates the transport.
  OnRawMessage([this](const DriverTransport::RawMessage& message) {
    Message m;
    return m.DeserializeUnknownType(message, *transport_);
  });
}

void TestTransportListener::ActivateTransportIfNecessary() {
  if (activated_) {
    return;
  }

  activated_ = true;
  transport_->Activate();
}

}  // namespace ipcz::test
