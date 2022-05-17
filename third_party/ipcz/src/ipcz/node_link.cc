// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/node_link.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <utility>

#include "ipcz/ipcz.h"
#include "ipcz/link_type.h"
#include "ipcz/message.h"
#include "ipcz/node.h"
#include "ipcz/node_messages.h"
#include "ipcz/remote_router_link.h"
#include "ipcz/router.h"
#include "third_party/abseil-cpp/absl/base/macros.h"
#include "util/log.h"
#include "util/ref_counted.h"

namespace ipcz {

// static
Ref<NodeLink> NodeLink::Create(Ref<Node> node,
                               LinkSide link_side,
                               const NodeName& local_node_name,
                               const NodeName& remote_node_name,
                               Node::Type remote_node_type,
                               uint32_t remote_protocol_version,
                               Ref<DriverTransport> transport) {
  return AdoptRef(new NodeLink(std::move(node), link_side, local_node_name,
                               remote_node_name, remote_node_type,
                               remote_protocol_version, std::move(transport)));
}

NodeLink::NodeLink(Ref<Node> node,
                   LinkSide link_side,
                   const NodeName& local_node_name,
                   const NodeName& remote_node_name,
                   Node::Type remote_node_type,
                   uint32_t remote_protocol_version,
                   Ref<DriverTransport> transport)
    : node_(std::move(node)),
      link_side_(link_side),
      local_node_name_(local_node_name),
      remote_node_name_(remote_node_name),
      remote_node_type_(remote_node_type),
      remote_protocol_version_(remote_protocol_version),
      transport_(std::move(transport)) {
  transport_->set_listener(this);
}

NodeLink::~NodeLink() {
  // Ensure this NodeLink is deactivated even if it was never adopted by a Node.
  // If it was already deactivated, this is a no-op.
  Deactivate();
}

Ref<RemoteRouterLink> NodeLink::AddRemoteRouterLink(SublinkId sublink,
                                                    LinkType type,
                                                    LinkSide side,
                                                    Ref<Router> router) {
  auto link =
      RemoteRouterLink::Create(WrapRefCounted(this), sublink, type, side);

  absl::MutexLock lock(&mutex_);
  auto [it, added] = sublinks_.try_emplace(
      sublink, Sublink(std::move(link), std::move(router)));
  if (!added) {
    // The sublink provided here may be received in a message from another node.
    // Failure here serves as a validation signal, as a well-behaved node will
    // not attempt to reuse sublink IDs.
    return nullptr;
  }
  return it->second.router_link;
}

void NodeLink::RemoveRemoteRouterLink(SublinkId sublink) {
  absl::MutexLock lock(&mutex_);
  sublinks_.erase(sublink);
}

absl::optional<NodeLink::Sublink> NodeLink::GetSublink(SublinkId sublink) {
  absl::MutexLock lock(&mutex_);
  auto it = sublinks_.find(sublink);
  if (it == sublinks_.end()) {
    return absl::nullopt;
  }
  return it->second;
}

Ref<Router> NodeLink::GetRouter(SublinkId sublink) {
  absl::MutexLock lock(&mutex_);
  auto it = sublinks_.find(sublink);
  if (it == sublinks_.end()) {
    return nullptr;
  }
  return it->second.receiver;
}

void NodeLink::Deactivate() {
  SublinkMap sublinks;
  {
    absl::MutexLock lock(&mutex_);
    sublinks = std::move(sublinks_);
    if (!active_) {
      return;
    }

    active_ = false;
  }

  sublinks.clear();
  transport_->Deactivate();
}

void NodeLink::Transmit(Message& message) {
  if (!message.CanTransmitOn(*transport_)) {
    // The driver has indicated that it can't transmit this message through our
    // transport, so the message must instead be relayed through a broker.
    //
    // TODO: Broker relaying not yet implemented.
    ABSL_ASSERT(false);
    return;
  }

  message.header().sequence_number = GenerateOutgoingSequenceNumber();
  transport_->Transmit(message);
}

SequenceNumber NodeLink::GenerateOutgoingSequenceNumber() {
  return SequenceNumber(next_outgoing_sequence_number_generator_.fetch_add(
      1, std::memory_order_relaxed));
}

IpczResult NodeLink::OnTransportMessage(
    const DriverTransport::RawMessage& message) {
  return DispatchMessage(message);
}

void NodeLink::OnTransportError() {
  // TODO: Notify all routers attached to sublinks here that this link is dead.
}

bool NodeLink::OnConnectFromBrokerToNonBroker(
    const msg::ConnectFromBrokerToNonBroker&) {
  // This message is never valid to receive once a NodeLink is established.
  return false;
}

bool NodeLink::OnConnectFromNonBrokerToBroker(
    const msg::ConnectFromNonBrokerToBroker&) {
  // This message is never valid to receive once a NodeLink is established.
  return false;
}

bool NodeLink::OnRouteClosed(const msg::RouteClosed& route_closed) {
  absl::optional<Sublink> sublink = GetSublink(route_closed.params().sublink);
  if (!sublink) {
    // The sublink may have already been removed, for example if the application
    // has already closed the associated router. It is therefore not considered
    // an error to receive a RouteClosed message for an unknown sublink.
    return true;
  }

  return sublink->receiver->AcceptRouteClosureFrom(
      sublink->router_link->GetType(), route_closed.params().sequence_length);
}

IpczResult NodeLink::DispatchMessage(
    const DriverTransport::RawMessage& message) {
  if (message.data.size() < sizeof(internal::MessageHeader)) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  const auto& header =
      *reinterpret_cast<const internal::MessageHeader*>(message.data.data());
  switch (header.message_id) {
// clang-format off
#include "ipcz/message_macros/message_dispatch_macros.h"
#include "ipcz/node_messages_generator.h"
#include "ipcz/message_macros/undef_message_macros.h"
      // clang-format on

    default:
      // Future versions may introduce new messages. Silently ignore them.
      DLOG(WARNING) << "Ignoring unknown transport message with ID "
                    << static_cast<int>(header.message_id);
      break;
  }

  return IPCZ_RESULT_OK;
}

NodeLink::Sublink::Sublink(Ref<RemoteRouterLink> router_link,
                           Ref<Router> receiver)
    : router_link(std::move(router_link)), receiver(std::move(receiver)) {}

NodeLink::Sublink::Sublink(Sublink&&) = default;

NodeLink::Sublink::Sublink(const Sublink&) = default;

NodeLink::Sublink& NodeLink::Sublink::operator=(Sublink&&) = default;

NodeLink::Sublink& NodeLink::Sublink::operator=(const Sublink&) = default;

NodeLink::Sublink::~Sublink() = default;

}  // namespace ipcz
