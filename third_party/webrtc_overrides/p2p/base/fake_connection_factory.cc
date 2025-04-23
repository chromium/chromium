// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/webrtc_overrides/p2p/base/fake_connection_factory.h"

#include <cstdint>
#include <memory>
#include <string_view>
#include <utility>

#include "base/synchronization/waitable_event.h"
#include "third_party/webrtc/api/candidate.h"
#include "third_party/webrtc/api/task_queue/task_queue_base.h"
#include "third_party/webrtc/p2p/base/connection.h"
#include "third_party/webrtc/p2p/base/p2p_constants.h"
#include "third_party/webrtc/p2p/base/port_allocator.h"
#include "third_party/webrtc/p2p/base/port_interface.h"
#include "third_party/webrtc/p2p/test/fake_port_allocator.h"
#include "third_party/webrtc/rtc_base/net_helper.h"
#include "third_party/webrtc/rtc_base/socket_address.h"
#include "third_party/webrtc_overrides/environment.h"
#include "third_party/webrtc_overrides/rtc_base/fake_socket_factory.h"

namespace blink {

FakeConnectionFactory::FakeConnectionFactory(webrtc::TaskQueueBase* thread,
                                             base::WaitableEvent* readyEvent)
    : readyEvent_(readyEvent),
      sf_(std::make_unique<blink::FakeSocketFactory>()),
      allocator_(
          std::make_unique<webrtc::FakePortAllocator>(WebRtcEnvironment(),
                                                      sf_.get(),
                                                      thread)) {}

void FakeConnectionFactory::Prepare(uint32_t allocator_flags) {
  if (sessions_.size() > 0) {
    return;
  }
  allocator_->set_flags(allocator_flags);
  auto session = allocator_->CreateSession("test", /*component=*/1, "ice_ufrag",
                                           "ice_password");
  session->set_generation(0);
  session->SignalPortReady.connect(this, &FakeConnectionFactory::OnPortReady);
  session->StartGettingPorts();
  sessions_.push_back(std::move(session));
}

webrtc::Connection* FakeConnectionFactory::CreateConnection(
    webrtc::IceCandidateType type,
    std::string_view remote_ip,
    int remote_port,
    int priority) {
  if (ports_.size() == 0) {
    return nullptr;
  }
  webrtc::Candidate remote =
      CreateUdpCandidate(type, remote_ip, remote_port, priority);
  webrtc::Connection* conn = nullptr;
  for (auto port : ports_) {
    if (port->SupportsProtocol(remote.protocol())) {
      conn = port->GetConnection(remote.address());
      if (!conn) {
        conn = port->CreateConnection(remote,
                                      webrtc::PortInterface::ORIGIN_MESSAGE);
      }
    }
  }
  return conn;
}

void FakeConnectionFactory::OnPortReady(webrtc::PortAllocatorSession* session,
                                        webrtc::PortInterface* port) {
  ports_.push_back(port);
  if (!readyEvent_->IsSignaled()) {
    readyEvent_->Signal();
  }
}

webrtc::Candidate FakeConnectionFactory::CreateUdpCandidate(
    webrtc::IceCandidateType type,
    std::string_view ip,
    int port,
    int priority,
    std::string_view ufrag) {
  webrtc::Candidate c;
  c.set_address(webrtc::SocketAddress(ip.data(), port));
  c.set_component(::webrtc::ICE_CANDIDATE_COMPONENT_DEFAULT);
  c.set_protocol(::webrtc::UDP_PROTOCOL_NAME);
  c.set_priority(priority);
  c.set_username(ufrag.data());
  c.set_type(type);
  return c;
}

}  // namespace blink
