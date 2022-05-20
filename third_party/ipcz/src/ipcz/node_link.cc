// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/node_link.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <utility>

#include "ipcz/box.h"
#include "ipcz/ipcz.h"
#include "ipcz/link_type.h"
#include "ipcz/message.h"
#include "ipcz/node.h"
#include "ipcz/node_link_memory.h"
#include "ipcz/node_messages.h"
#include "ipcz/portal.h"
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
                               Ref<DriverTransport> transport,
                               Ref<NodeLinkMemory> memory) {
  return AdoptRef(new NodeLink(std::move(node), link_side, local_node_name,
                               remote_node_name, remote_node_type,
                               remote_protocol_version, std::move(transport),
                               std::move(memory)));
}

NodeLink::NodeLink(Ref<Node> node,
                   LinkSide link_side,
                   const NodeName& local_node_name,
                   const NodeName& remote_node_name,
                   Node::Type remote_node_type,
                   uint32_t remote_protocol_version,
                   Ref<DriverTransport> transport,
                   Ref<NodeLinkMemory> memory)
    : node_(std::move(node)),
      link_side_(link_side),
      local_node_name_(local_node_name),
      remote_node_name_(remote_node_name),
      remote_node_type_(remote_node_type),
      remote_protocol_version_(remote_protocol_version),
      transport_(std::move(transport)),
      memory_(std::move(memory)) {
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

bool NodeLink::OnAcceptParcel(msg::AcceptParcel& accept) {
  absl::Span<const uint8_t> parcel_data =
      accept.GetArrayView<uint8_t>(accept.params().parcel_data);
  absl::Span<const HandleType> handle_types =
      accept.GetArrayView<HandleType>(accept.params().handle_types);
  absl::Span<const RouterDescriptor> new_routers =
      accept.GetArrayView<RouterDescriptor>(accept.params().new_routers);
  auto driver_objects = accept.driver_objects();

  // Note that on any validation failure below, we defer rejection at least
  // until any deserialized objects are stored in a new Parcel object. This
  // ensures that they're properly cleaned up before we return.
  bool parcel_valid = true;
  std::vector<Ref<APIObject>> objects(handle_types.size());
  for (size_t i = 0; i < handle_types.size(); ++i) {
    switch (handle_types[i]) {
      case HandleType::kPortal: {
        if (new_routers.empty()) {
          parcel_valid = false;
          continue;
        }

        Ref<Router> new_router = Router::Deserialize(new_routers[0], *this);
        if (!new_router) {
          parcel_valid = false;
          continue;
        }

        objects[i] = MakeRefCounted<Portal>(node_, std::move(new_router));
        new_routers.remove_prefix(1);
        break;
      }

      case HandleType::kBox: {
        if (driver_objects.empty()) {
          return false;
        }

        objects[i] = MakeRefCounted<Box>(std::move(driver_objects[0]));
        driver_objects.remove_prefix(1);
        break;
      }

      default:
        parcel_valid = false;
        break;
    }
  }

  if (!new_routers.empty() || !driver_objects.empty()) {
    // There should be no unclaimed routers. If there are, it's a validation
    // failure.
    parcel_valid = false;
  }

  const SublinkId for_sublink = accept.params().sublink;
  Parcel parcel(accept.params().sequence_number);
  parcel.SetObjects(std::move(objects));
  if (!parcel_valid) {
    return false;
  }

  parcel.SetInlinedData(
      std::vector<uint8_t>(parcel_data.begin(), parcel_data.end()));

  const absl::optional<Sublink> sublink = GetSublink(for_sublink);
  if (!sublink) {
    DVLOG(4) << "Dropping " << parcel.Describe() << " at "
             << local_node_name_.ToString() << ", arriving from "
             << remote_node_name_.ToString() << " via unknown sublink "
             << for_sublink;
    return true;
  }
  const LinkType link_type = sublink->router_link->GetType();
  if (link_type.is_outward()) {
    DVLOG(4) << "Accepting inbound " << parcel.Describe() << " at "
             << sublink->router_link->Describe();
    return sublink->receiver->AcceptInboundParcel(parcel);
  }

  ABSL_ASSERT(link_type.is_peripheral_inward());
  DVLOG(4) << "Accepting outbound " << parcel.Describe() << " at "
           << sublink->router_link->Describe();
  return sublink->receiver->AcceptOutboundParcel(parcel);
}

bool NodeLink::OnRouteClosed(msg::RouteClosed& route_closed) {
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

void NodeLink::OnTransportError() {
  // TODO: Notify all routers attached to sublinks here that this link is dead.
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
