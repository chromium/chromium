// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/node_link.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <utility>

#include "ipcz/box.h"
#include "ipcz/fragment_ref.h"
#include "ipcz/ipcz.h"
#include "ipcz/link_side.h"
#include "ipcz/link_type.h"
#include "ipcz/message.h"
#include "ipcz/node.h"
#include "ipcz/node_connector.h"
#include "ipcz/node_link_memory.h"
#include "ipcz/node_messages.h"
#include "ipcz/operation_context.h"
#include "ipcz/parcel.h"
#include "ipcz/portal.h"
#include "ipcz/remote_router_link.h"
#include "ipcz/router.h"
#include "ipcz/router_link.h"
#include "ipcz/router_link_state.h"
#include "ipcz/sublink_id.h"
#include "third_party/abseil-cpp/absl/base/macros.h"
#include "util/log.h"
#include "util/ref_counted.h"
#include "util/safe_math.h"

namespace ipcz {

namespace {

template <typename T>
FragmentRef<T> MaybeAdoptFragmentRef(NodeLinkMemory& memory,
                                     const FragmentDescriptor& descriptor) {
  if (descriptor.is_null() || descriptor.size() < sizeof(T) ||
      descriptor.offset() % 8 != 0) {
    return {};
  }

  return memory.AdoptFragmentRef<T>(memory.GetFragment(descriptor));
}

}  // namespace

// static
Ref<NodeLink> NodeLink::CreateActive(Ref<Node> node,
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
                               std::move(memory), kActive));
}

// static
Ref<NodeLink> NodeLink::CreateInactive(Ref<Node> node,
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
                               std::move(memory), kNeverActivated));
}

NodeLink::NodeLink(Ref<Node> node,
                   LinkSide link_side,
                   const NodeName& local_node_name,
                   const NodeName& remote_node_name,
                   Node::Type remote_node_type,
                   uint32_t remote_protocol_version,
                   Ref<DriverTransport> transport,
                   Ref<NodeLinkMemory> memory,
                   ActivationState initial_activation_state)
    : node_(std::move(node)),
      link_side_(link_side),
      local_node_name_(local_node_name),
      remote_node_name_(remote_node_name),
      remote_node_type_(remote_node_type),
      remote_protocol_version_(remote_protocol_version),
      transport_(std::move(transport)),
      memory_(std::move(memory)),
      activation_state_(initial_activation_state) {
  if (initial_activation_state == kActive) {
    transport_->set_listener(WrapRefCounted(this));
    memory_->SetNodeLink(WrapRefCounted(this));
  }
}

NodeLink::~NodeLink() {
  absl::MutexLock lock(&mutex_);
  ABSL_HARDENING_ASSERT(activation_state_ != kActive);
}

void NodeLink::Activate() {
  transport_->set_listener(WrapRefCounted(this));
  memory_->SetNodeLink(WrapRefCounted(this));

  {
    absl::MutexLock lock(&mutex_);
    ABSL_ASSERT(activation_state_ == kNeverActivated);
    activation_state_ = kActive;
  }

  transport_->Activate();
}

Ref<RemoteRouterLink> NodeLink::AddRemoteRouterLink(
    const OperationContext& context,
    SublinkId sublink,
    FragmentRef<RouterLinkState> link_state,
    LinkType type,
    LinkSide side,
    Ref<Router> router) {
  auto link = RemoteRouterLink::Create(context, WrapRefCounted(this), sublink,
                                       std::move(link_state), type, side);

  absl::MutexLock lock(&mutex_);
  if (activation_state_ == kDeactivated) {
    // We don't bind new RemoteRouterLinks once we've been deactivated, lest we
    // incur leaky NodeLink references.
    return nullptr;
  }

  auto [it, added] = sublinks_.try_emplace(
      sublink, Sublink(std::move(link), std::move(router)));
  if (!added) {
    // The SublinkId provided here may have been received from another node and
    // may already be in use if the node is misbehaving.
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

void NodeLink::AddBlockBuffer(BufferId id,
                              uint32_t block_size,
                              DriverMemory memory) {
  msg::AddBlockBuffer add;
  add.params().id = id;
  add.params().block_size = block_size;
  add.params().buffer = add.AppendDriverObject(memory.TakeDriverObject());
  Transmit(add);
}

void NodeLink::RequestIntroduction(const NodeName& name) {
  ABSL_ASSERT(remote_node_type_ == Node::Type::kBroker);

  msg::RequestIntroduction request;
  request.params().name = name;
  Transmit(request);
}

void NodeLink::AcceptIntroduction(const NodeName& name,
                                  LinkSide side,
                                  Node::Type remote_node_type,
                                  uint32_t remote_protocol_version,
                                  Ref<DriverTransport> transport,
                                  DriverMemory memory) {
  ABSL_ASSERT(node_->type() == Node::Type::kBroker);

  msg::AcceptIntroduction accept;
  accept.params().name = name;
  accept.params().link_side = side;
  accept.params().remote_node_type = remote_node_type;
  accept.params().padding = 0;
  accept.params().remote_protocol_version = remote_protocol_version;
  accept.params().transport =
      accept.AppendDriverObject(transport->TakeDriverObject());
  accept.params().memory = accept.AppendDriverObject(memory.TakeDriverObject());
  Transmit(accept);
}

void NodeLink::RejectIntroduction(const NodeName& name) {
  ABSL_ASSERT(node_->type() == Node::Type::kBroker);

  msg::RejectIntroduction reject;
  reject.params().name = name;
  Transmit(reject);
}

void NodeLink::ReferNonBroker(Ref<DriverTransport> transport,
                              uint32_t num_initial_portals,
                              ReferralCallback callback) {
  ABSL_ASSERT(node_->type() == Node::Type::kNormal &&
              remote_node_type_ == Node::Type::kBroker);

  uint64_t referral_id;
  {
    absl::MutexLock lock(&mutex_);
    for (;;) {
      referral_id = next_referral_id_++;
      auto [it, inserted] =
          pending_referrals_.try_emplace(referral_id, std::move(callback));
      if (inserted) {
        break;
      }
    }
  }

  msg::ReferNonBroker refer;
  refer.params().referral_id = referral_id;
  refer.params().num_initial_portals = num_initial_portals;
  refer.params().transport =
      refer.AppendDriverObject(transport->TakeDriverObject());
  Transmit(refer);
}

void NodeLink::AcceptBypassLink(
    const NodeName& current_peer_node,
    SublinkId current_peer_sublink,
    SequenceNumber inbound_sequence_length_from_bypassed_link,
    SublinkId new_sublink,
    FragmentRef<RouterLinkState> link_state) {
  msg::AcceptBypassLink accept;
  accept.params().current_peer_node = current_peer_node;
  accept.params().current_peer_sublink = current_peer_sublink;
  accept.params().inbound_sequence_length_from_bypassed_link =
      inbound_sequence_length_from_bypassed_link;
  accept.params().new_sublink = new_sublink;
  accept.params().new_link_state_fragment = link_state.release().descriptor();
  Transmit(accept);
}

void NodeLink::RequestMemory(size_t size, RequestMemoryCallback callback) {
  const uint32_t size32 = checked_cast<uint32_t>(size);
  {
    absl::MutexLock lock(&mutex_);
    pending_memory_requests_[size32].push_back(std::move(callback));
  }

  msg::RequestMemory request;
  request.params().size = size32;
  request.params().padding = 0;
  Transmit(request);
}

void NodeLink::RelayMessage(const NodeName& to_node, Message& message) {
  ABSL_ASSERT(remote_node_type_ == Node::Type::kBroker);

  msg::RelayMessage relay;
  relay.params().destination = to_node;
  relay.params().data =
      relay.AllocateArray<uint8_t>(message.data_view().size());
  relay.params().padding = 0;
  memcpy(relay.GetArrayData(relay.params().data), message.data_view().data(),
         message.data_view().size());
  relay.params().driver_objects =
      relay.AppendDriverObjects(message.driver_objects());
  Transmit(relay);
}

bool NodeLink::DispatchRelayedMessage(msg::AcceptRelayedMessage& accept) {
  // We only allow a limited subset of messages to be relayed through a broker.
  // Namely, any message which might carry driver objects between two
  // non-brokers needs to be relayable.
  //
  // If an unknown or unsupported message type is relayed it's silently
  // discarded, rather than being rejected as a validation failure. This leaves
  // open the possibility for newer versions of a message to introduce driver
  // objects and support relaying.
  absl::Span<uint8_t> data = accept.GetArrayView<uint8_t>(accept.params().data);
  absl::Span<DriverObject> objects = accept.driver_objects();
  if (data.size() < sizeof(internal::MessageHeaderV0)) {
    return false;
  }
  const uint8_t message_id =
      reinterpret_cast<internal::MessageHeaderV0*>(data.data())->message_id;
  switch (message_id) {
    case msg::AcceptParcelDriverObjects::kId: {
      msg::AcceptParcelDriverObjects accept_parcel;
      return accept_parcel.DeserializeRelayed(data, objects) &&
             OnAcceptParcelDriverObjects(accept_parcel);
    }

    case msg::AddBlockBuffer::kId: {
      msg::AddBlockBuffer add;
      return add.DeserializeRelayed(data, objects) && OnAddBlockBuffer(add);
    }

    default:
      DVLOG(4) << "Ignoring relayed message with ID "
               << static_cast<int>(message_id);
      return true;
  }
}

void NodeLink::Deactivate(const OperationContext& context) {
  {
    absl::MutexLock lock(&mutex_);
    if (activation_state_ != kActive) {
      return;
    }
    activation_state_ = kDeactivated;
  }

  HandleTransportError(context);
  transport_->Deactivate();
  memory_->SetNodeLink(nullptr);
}

void NodeLink::Transmit(Message& message) {
  if (!message.CanTransmitOn(*transport_)) {
    // The driver has indicated that it can't transmit this message through our
    // transport, so the message must instead be relayed through a broker.
    auto broker = node_->GetBrokerLink();
    if (!broker) {
      DLOG(ERROR) << "Cannot relay message without a broker link";
      return;
    }

    broker->RelayMessage(remote_node_name_, message);
    return;
  }

  message.header().sequence_number = GenerateOutgoingSequenceNumber();
  transport_->Transmit(message);
}

SequenceNumber NodeLink::GenerateOutgoingSequenceNumber() {
  return SequenceNumber(next_outgoing_sequence_number_generator_.fetch_add(
      1, std::memory_order_relaxed));
}

bool NodeLink::OnReferNonBroker(msg::ReferNonBroker& refer) {
  if (remote_node_type_ != Node::Type::kNormal ||
      node()->type() != Node::Type::kBroker) {
    return false;
  }

  DriverObject transport = refer.TakeDriverObject(refer.params().transport);
  if (!transport.is_valid()) {
    return false;
  }

  return NodeConnector::HandleNonBrokerReferral(
      node(), refer.params().referral_id, refer.params().num_initial_portals,
      WrapRefCounted(this),
      MakeRefCounted<DriverTransport>(std::move(transport)));
}

bool NodeLink::OnNonBrokerReferralAccepted(
    msg::NonBrokerReferralAccepted& accepted) {
  if (remote_node_type_ != Node::Type::kBroker) {
    return false;
  }

  ReferralCallback callback;
  {
    absl::MutexLock lock(&mutex_);
    auto it = pending_referrals_.find(accepted.params().referral_id);
    if (it == pending_referrals_.end()) {
      return false;
    }
    callback = std::move(it->second);
    pending_referrals_.erase(it);
  }

  const uint32_t protocol_version =
      std::min(msg::kProtocolVersion, accepted.params().protocol_version);
  auto transport = MakeRefCounted<DriverTransport>(
      accepted.TakeDriverObject(accepted.params().transport));
  DriverMemoryMapping mapping =
      DriverMemory(accepted.TakeDriverObject(accepted.params().buffer)).Map();
  if (!transport->driver_object().is_valid() || !mapping.is_valid()) {
    // Not quite a validation failure if the broker simply failed to allocate
    // resources for this link. Treat it like a connection failure.
    callback(/*link=*/nullptr, /*num_initial_portals=*/0);
    return true;
  }

  Ref<NodeLink> link_to_referree = NodeLink::CreateInactive(
      node_, LinkSide::kA, local_node_name_, accepted.params().name,
      Node::Type::kNormal, protocol_version, std::move(transport),
      NodeLinkMemory::Create(node_, std::move(mapping)));
  callback(link_to_referree, accepted.params().num_initial_portals);
  link_to_referree->Activate();
  return true;
}

bool NodeLink::OnNonBrokerReferralRejected(
    msg::NonBrokerReferralRejected& rejected) {
  if (remote_node_type_ != Node::Type::kBroker) {
    return false;
  }

  ReferralCallback callback;
  {
    absl::MutexLock lock(&mutex_);
    auto it = pending_referrals_.find(rejected.params().referral_id);
    if (it == pending_referrals_.end()) {
      return false;
    }
    callback = std::move(it->second);
    pending_referrals_.erase(it);
  }

  callback(/*link=*/nullptr, /*num_initial_portals=*/0);
  return true;
}

bool NodeLink::OnRequestIntroduction(msg::RequestIntroduction& request) {
  if (node()->type() != Node::Type::kBroker) {
    return false;
  }

  node()->HandleIntroductionRequest(*this, request.params().name);
  return true;
}

bool NodeLink::OnAcceptIntroduction(msg::AcceptIntroduction& accept) {
  if (remote_node_type_ != Node::Type::kBroker) {
    return false;
  }

  DriverMemoryMapping mapping =
      DriverMemory(accept.TakeDriverObject(accept.params().memory)).Map();
  if (!mapping.is_valid()) {
    return false;
  }

  auto transport = MakeRefCounted<DriverTransport>(
      accept.TakeDriverObject(accept.params().transport));
  node()->AcceptIntroduction(
      *this, accept.params().name, accept.params().link_side,
      accept.params().remote_node_type, accept.params().remote_protocol_version,
      std::move(transport), NodeLinkMemory::Create(node(), std::move(mapping)));
  return true;
}

bool NodeLink::OnRejectIntroduction(msg::RejectIntroduction& reject) {
  if (remote_node_type_ != Node::Type::kBroker) {
    return false;
  }

  node()->NotifyIntroductionFailed(*this, reject.params().name);
  return true;
}

bool NodeLink::OnRequestIndirectIntroduction(
    msg::RequestIndirectIntroduction& request) {
  // By convention only a broker on side B of a broker-to-broker link will send
  // this message, and so only side-A broker-to-broker links can receive it.
  if (remote_node_type_ != Node::Type::kBroker ||
      node()->type() != Node::Type::kBroker || !link_side_.is_side_a()) {
    return false;
  }

  return node()->HandleIndirectIntroductionRequest(
      *this, request.params().target_node, request.params().source_node);
}

bool NodeLink::OnAddBlockBuffer(msg::AddBlockBuffer& add) {
  DriverMemoryMapping mapping =
      DriverMemory(add.TakeDriverObject(add.params().buffer)).Map();
  if (!mapping.is_valid()) {
    return false;
  }
  return memory().AddBlockBuffer(add.params().id, add.params().block_size,
                                 std::move(mapping));
}

bool NodeLink::OnAcceptParcel(msg::AcceptParcel& accept) {
  absl::Span<uint8_t> parcel_data =
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
  bool is_split_parcel = false;
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

      case HandleType::kBoxedDriverObject: {
        if (driver_objects.empty()) {
          return false;
        }

        objects[i] = MakeRefCounted<Box>(std::move(driver_objects[0]));
        driver_objects.remove_prefix(1);
        break;
      }

      case HandleType::kRelayedBoxedDriverObject: {
        is_split_parcel = true;
        break;
      }

      case HandleType::kBoxedSubparcel:
        // Store a placeholder object for each expected subparcel. These will
        // be filled in by AcceptCompleteParcel() once the last complete
        // subparcel is accepted.
        objects[i] =
            MakeRefCounted<Box>(MakeRefCounted<ParcelWrapper>(Parcel{}));
        break;

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

  const uint32_t num_subparcels = accept.params().num_subparcels;
  const uint32_t subparcel_index = accept.params().subparcel_index;
  if (num_subparcels > Parcel::kMaxSubparcelsPerParcel ||
      subparcel_index >= num_subparcels) {
    return false;
  }

  const SublinkId for_sublink = accept.params().sublink;
  Parcel parcel(accept.params().sequence_number);
  parcel.set_num_subparcels(num_subparcels);
  parcel.set_subparcel_index(subparcel_index);
  parcel.SetObjects(std::move(objects));
  if (!parcel_valid) {
    return false;
  }

  const FragmentDescriptor descriptor = accept.params().parcel_fragment;
  if (!descriptor.is_null()) {
    // The parcel's data resides in a shared memory fragment.
    const Fragment fragment = memory().GetFragment(descriptor);
    if (fragment.is_pending()) {
      // We don't have this buffer yet, but we expect to receive it ASAP. Defer
      // acceptance until then.
      WaitForParcelFragmentToResolve(for_sublink, parcel, descriptor,
                                     is_split_parcel);
      return true;
    }

    if (!parcel.AdoptDataFragment(WrapRefCounted(&memory()), fragment)) {
      return false;
    }
  } else {
    // The parcel's data was inlined within the AcceptParcel message. Adopt the
    // Message contents so our local Parcel doesn't need to copy any data.
    parcel.SetDataFromMessage(std::move(accept).TakeReceivedData(),
                              parcel_data);
  }

  if (is_split_parcel) {
    return AcceptParcelWithoutDriverObjects(for_sublink, parcel);
  }
  return AcceptCompleteParcel(for_sublink, parcel);
}

bool NodeLink::OnAcceptParcelDriverObjects(
    msg::AcceptParcelDriverObjects& accept) {
  Parcel parcel(accept.params().sequence_number);
  std::vector<Ref<APIObject>> objects;
  objects.reserve(accept.driver_objects().size());
  for (auto& object : accept.driver_objects()) {
    objects.push_back(MakeRefCounted<Box>(std::move(object)));
  }
  parcel.SetObjects(std::move(objects));
  return AcceptParcelDriverObjects(accept.params().sublink, parcel);
}

bool NodeLink::OnRouteClosed(msg::RouteClosed& route_closed) {
  absl::optional<Sublink> sublink = GetSublink(route_closed.params().sublink);
  if (!sublink) {
    // The sublink may have already been removed, for example if the application
    // has already closed the associated router. It is therefore not considered
    // an error to receive a RouteClosed message for an unknown sublink.
    return true;
  }

  const OperationContext context{OperationContext::kTransportNotification};
  return sublink->receiver->AcceptRouteClosureFrom(
      context, sublink->router_link->GetType(),
      route_closed.params().sequence_length);
}

bool NodeLink::OnRouteDisconnected(msg::RouteDisconnected& route_closed) {
  absl::optional<Sublink> sublink = GetSublink(route_closed.params().sublink);
  if (!sublink) {
    return true;
  }

  DVLOG(4) << "Accepting RouteDisconnected at "
           << sublink->router_link->Describe();

  const OperationContext context{OperationContext::kTransportNotification};
  return sublink->receiver->AcceptRouteDisconnectedFrom(
      context, sublink->router_link->GetType());
}

bool NodeLink::OnBypassPeer(msg::BypassPeer& bypass) {
  absl::optional<Sublink> sublink = GetSublink(bypass.params().sublink);
  if (!sublink) {
    return true;
  }

  // NOTE: This request is authenticated by the receiving Router, within
  // BypassPeer().
  const OperationContext context{OperationContext::kTransportNotification};
  return sublink->receiver->BypassPeer(context, *sublink->router_link,
                                       bypass.params().bypass_target_node,
                                       bypass.params().bypass_target_sublink);
}

bool NodeLink::OnAcceptBypassLink(msg::AcceptBypassLink& accept) {
  Ref<NodeLink> node_link_to_peer =
      node_->GetLink(accept.params().current_peer_node);
  if (!node_link_to_peer) {
    // If the link to the peer has been severed for whatever reason, the
    // relevant route will be torn down anyway. It's safe to ignore this
    // request in that case.
    return true;
  }

  const Ref<Router> receiver =
      node_link_to_peer->GetRouter(accept.params().current_peer_sublink);
  if (!receiver) {
    // Similar to above, if the targeted Router cannot be resolved from the
    // given sublink, this implies that the route has already been at least
    // partially torn down. It's safe to ignore this request.
    return true;
  }

  auto link_state = MaybeAdoptFragmentRef<RouterLinkState>(
      memory(), accept.params().new_link_state_fragment);
  if (link_state.is_null()) {
    // Bypass links must always come with a valid fragment for their
    // RouterLinkState. If one has not been provided, that's a validation
    // failure.
    return false;
  }

  const OperationContext context{OperationContext::kTransportNotification};
  return receiver->AcceptBypassLink(
      context, *this, accept.params().new_sublink, std::move(link_state),
      accept.params().inbound_sequence_length_from_bypassed_link);
}

bool NodeLink::OnStopProxying(msg::StopProxying& stop) {
  Ref<Router> router = GetRouter(stop.params().sublink);
  if (!router) {
    return true;
  }

  const OperationContext context{OperationContext::kTransportNotification};
  return router->StopProxying(context, stop.params().inbound_sequence_length,
                              stop.params().outbound_sequence_length);
}

bool NodeLink::OnProxyWillStop(msg::ProxyWillStop& will_stop) {
  Ref<Router> router = GetRouter(will_stop.params().sublink);
  if (!router) {
    return true;
  }

  const OperationContext context{OperationContext::kTransportNotification};
  return router->NotifyProxyWillStop(
      context, will_stop.params().inbound_sequence_length);
}

bool NodeLink::OnBypassPeerWithLink(msg::BypassPeerWithLink& bypass) {
  Ref<Router> router = GetRouter(bypass.params().sublink);
  if (!router) {
    return true;
  }

  auto link_state = MaybeAdoptFragmentRef<RouterLinkState>(
      memory(), bypass.params().new_link_state_fragment);
  if (link_state.is_null()) {
    return false;
  }

  const OperationContext context{OperationContext::kTransportNotification};
  return router->AcceptBypassLink(context, *this, bypass.params().new_sublink,
                                  std::move(link_state),
                                  bypass.params().inbound_sequence_length);
}

bool NodeLink::OnStopProxyingToLocalPeer(msg::StopProxyingToLocalPeer& stop) {
  Ref<Router> router = GetRouter(stop.params().sublink);
  if (!router) {
    return true;
  }

  const OperationContext context{OperationContext::kTransportNotification};
  return router->StopProxyingToLocalPeer(
      context, stop.params().outbound_sequence_length);
}

bool NodeLink::OnFlushRouter(msg::FlushRouter& flush) {
  if (Ref<Router> router = GetRouter(flush.params().sublink)) {
    const OperationContext context{OperationContext::kTransportNotification};
    router->Flush(context, Router::kForceProxyBypassAttempt);
  }
  return true;
}

bool NodeLink::OnRequestMemory(msg::RequestMemory& request) {
  DriverMemory memory(node_->driver(), request.params().size);
  msg::ProvideMemory provide;
  provide.params().size = request.params().size;
  provide.params().buffer =
      provide.AppendDriverObject(memory.TakeDriverObject());
  Transmit(provide);
  return true;
}

bool NodeLink::OnProvideMemory(msg::ProvideMemory& provide) {
  DriverMemory memory(provide.TakeDriverObject(provide.params().buffer));
  RequestMemoryCallback callback;
  {
    absl::MutexLock lock(&mutex_);
    auto it = pending_memory_requests_.find(provide.params().size);
    if (it == pending_memory_requests_.end()) {
      return false;
    }

    std::list<RequestMemoryCallback>& callbacks = it->second;
    ABSL_ASSERT(!callbacks.empty());
    callback = std::move(callbacks.front());
    callbacks.pop_front();
    if (callbacks.empty()) {
      pending_memory_requests_.erase(it);
    }
  }

  callback(std::move(memory));
  return true;
}

bool NodeLink::OnRelayMessage(msg::RelayMessage& relay) {
  if (node_->type() != Node::Type::kBroker) {
    return false;
  }

  return node_->RelayMessage(remote_node_name_, relay);
}

bool NodeLink::OnAcceptRelayedMessage(msg::AcceptRelayedMessage& accept) {
  if (remote_node_type_ != Node::Type::kBroker) {
    return false;
  }

  return node_->AcceptRelayedMessage(accept);
}

void NodeLink::OnTransportError() {
  const OperationContext context{OperationContext::kTransportNotification};
  HandleTransportError(context);
}

void NodeLink::HandleTransportError(const OperationContext& context) {
  SublinkMap sublinks;
  {
    absl::MutexLock lock(&mutex_);
    sublinks.swap(sublinks_);
  }

  for (auto& [id, sublink] : sublinks) {
    DVLOG(4) << "NodeLink disconnection dropping "
             << sublink.router_link->Describe() << " which is bound to router "
             << sublink.receiver.get();
    sublink.receiver->NotifyLinkDisconnected(context, *sublink.router_link);
  }

  Ref<NodeLink> self = WrapRefCounted(this);
  node_->DropConnection(context, remote_node_name_);
}

void NodeLink::WaitForParcelFragmentToResolve(
    SublinkId for_sublink,
    Parcel& parcel,
    const FragmentDescriptor& descriptor,
    bool is_split_parcel) {
  // ParcelWrapper wraps a Parcel in a RefCounted object so the reference can
  // be captured by a copyable lambda below.
  struct ParcelWrapper : public RefCounted {
    explicit ParcelWrapper(Parcel parcel) : parcel(std::move(parcel)) {}
    Parcel parcel;
  };

  auto wrapper = MakeRefCounted<ParcelWrapper>(std::move(parcel));
  memory().WaitForBufferAsync(
      descriptor.buffer_id(), [this_link = WrapRefCounted(this), for_sublink,
                               is_split_parcel, wrapper, descriptor]() {
        Ref<NodeLinkMemory> memory = WrapRefCounted(&this_link->memory());
        const Fragment fragment = memory->GetFragment(descriptor);
        Parcel& parcel = wrapper->parcel;
        if (!fragment.is_addressable() ||
            !parcel.AdoptDataFragment(std::move(memory), fragment)) {
          // The fragment is out of bounds or had an invalid header. Either way
          // it doesn't look good for the remote node.
          this_link->OnTransportError();
          return;
        }

        if (is_split_parcel) {
          this_link->AcceptParcelWithoutDriverObjects(for_sublink, parcel);
        } else {
          this_link->AcceptCompleteParcel(for_sublink, parcel);
        }
      });
}

bool NodeLink::AcceptParcelWithoutDriverObjects(SublinkId for_sublink,
                                                Parcel& parcel) {
  const auto key = std::make_tuple(for_sublink, parcel.sequence_number());
  Parcel parcel_with_driver_objects;
  {
    absl::MutexLock lock(&mutex_);
    auto [it, inserted] = partial_parcels_.try_emplace(key, std::move(parcel));
    if (inserted) {
      return true;
    }

    parcel_with_driver_objects = std::move(it->second);
    partial_parcels_.erase(it);
  }

  return AcceptSplitParcel(for_sublink, parcel, parcel_with_driver_objects);
}

bool NodeLink::AcceptParcelDriverObjects(SublinkId for_sublink,
                                         Parcel& parcel) {
  const auto key = std::make_tuple(for_sublink, parcel.sequence_number());
  Parcel parcel_without_driver_objects;
  {
    absl::MutexLock lock(&mutex_);
    auto [it, inserted] = partial_parcels_.try_emplace(key, std::move(parcel));
    if (inserted) {
      return true;
    }

    parcel_without_driver_objects = std::move(it->second);
    partial_parcels_.erase(it);
  }

  return AcceptSplitParcel(for_sublink, parcel_without_driver_objects, parcel);
}

bool NodeLink::AcceptSplitParcel(SublinkId for_sublink,
                                 Parcel& parcel_without_driver_objects,
                                 Parcel& parcel_with_driver_objects) {
  // The parcel with no driver objects should still have an object attachemnt
  // slot reserved for every relayed driver object.
  if (parcel_without_driver_objects.num_objects() <
      parcel_with_driver_objects.num_objects()) {
    return false;
  }

  // Fill in all the object gaps in the data-only parcel with the boxed objects
  // from the driver objects parcel.
  Parcel& complete_parcel = parcel_without_driver_objects;
  auto remaining_driver_objects = parcel_with_driver_objects.objects_view();
  for (auto& object : complete_parcel.objects_view()) {
    if (object) {
      continue;
    }

    if (remaining_driver_objects.empty()) {
      return false;
    }

    object = std::move(remaining_driver_objects[0]);
    remaining_driver_objects.remove_prefix(1);
  }

  // At least one driver object was unclaimed by the data half of the parcel.
  // That's not right.
  if (!remaining_driver_objects.empty()) {
    return false;
  }

  return AcceptCompleteParcel(for_sublink, complete_parcel);
}

bool NodeLink::AcceptCompleteParcel(SublinkId for_sublink, Parcel& parcel) {
  const absl::optional<Sublink> sublink = GetSublink(for_sublink);
  if (!sublink) {
    DVLOG(4) << "Dropping " << parcel.Describe() << " at "
             << local_node_name_.ToString() << ", arriving from "
             << remote_node_name_.ToString() << " via unknown sublink "
             << for_sublink;
    return true;
  }

  // Note that the common case is a standalone complete parcel, where the number
  // of subparcels is 1. In that case no additional tracking is necessary and we
  // immediately accept the parcel below.
  const size_t num_subparcels = parcel.num_subparcels();
  if (num_subparcels > 1) {
    auto key = std::make_tuple(for_sublink, parcel.sequence_number());
    absl::MutexLock lock(&mutex_);
    auto [it, inserted] =
        subparcel_trackers_.try_emplace(key, SubparcelTracker{});
    SubparcelTracker& tracker = it->second;
    if (inserted) {
      tracker.subparcels.resize(num_subparcels);
    }

    // Note that `index` has already been validated against the expected number
    // of subparcels in OnAcceptParcel().
    const size_t index = parcel.subparcel_index();
    ABSL_ASSERT(index < tracker.subparcels.size());
    if (tracker.subparcels[index]) {
      // Multiple subparcels claim the same index for this SequenceNumber. Bad.
      return false;
    }
    tracker.subparcels[index] =
        MakeRefCounted<ParcelWrapper>(std::move(parcel));
    tracker.num_subparcels_received++;
    if (tracker.num_subparcels_received < num_subparcels) {
      // Still waiting for more subparcels.
      return true;
    }

    // We have all subparcels for this SequenceNumber. Join them and proceed.
    // We do this by iterating over the object attachments in subparcel 0,
    // replacing any placeholder ParcelWrapper objects with our own real ones.
    auto subparcels = std::move(tracker.subparcels);
    subparcel_trackers_.erase(it);
    parcel = std::move(subparcels[0]->parcel());
    absl::Span<Ref<ParcelWrapper>> remaining_subparcels =
        absl::MakeSpan(subparcels).subspan(1);
    for (auto& object : parcel.objects_view()) {
      Box* box = Box::FromObject(object.get());
      if (box && box->type() == Box::Type::kSubparcel) {
        if (remaining_subparcels.empty()) {
          // Too many placeholder objects in the main parcel. Bad.
          return false;
        }

        ParcelWrapper* wrapper = box->subparcel().get();
        wrapper->parcel() = std::move(remaining_subparcels.front()->parcel());
        remaining_subparcels.remove_prefix(1);
      }
    }

    if (!remaining_subparcels.empty()) {
      // One or more subparcels unclaimed by the main parcel. Bad.
      return false;
    }
  }

  // At this point we've collected all expected subparcels and can pass the full
  // parcel along to its receiver.
  const OperationContext context{OperationContext::kTransportNotification};
  parcel.set_remote_source(WrapRefCounted(this));
  const LinkType link_type = sublink->router_link->GetType();
  if (link_type.is_outward()) {
    DVLOG(4) << "Accepting inbound " << parcel.Describe() << " at "
             << sublink->router_link->Describe();
    return sublink->receiver->AcceptInboundParcel(context, parcel);
  }

  ABSL_ASSERT(link_type.is_peripheral_inward());
  DVLOG(4) << "Accepting outbound " << parcel.Describe() << " at "
           << sublink->router_link->Describe();
  return sublink->receiver->AcceptOutboundParcel(context, parcel);
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
