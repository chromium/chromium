// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/node.h"

#include <utility>
#include <vector>

#include "ipcz/driver_memory.h"
#include "ipcz/ipcz.h"
#include "ipcz/link_side.h"
#include "ipcz/node_connector.h"
#include "ipcz/node_link.h"
#include "ipcz/node_link_memory.h"
#include "ipcz/portal.h"
#include "ipcz/router.h"
#include "third_party/abseil-cpp/absl/base/macros.h"
#include "third_party/abseil-cpp/absl/synchronization/mutex.h"
#include "util/log.h"
#include "util/ref_counted.h"

namespace ipcz {

Node::Node(Type type, const IpczDriver& driver, IpczDriverHandle driver_node)
    : type_(type), driver_(driver), driver_node_(driver_node) {
  if (type_ == Type::kBroker) {
    // Only brokers assign their own names.
    assigned_name_ = GenerateRandomName();
    DVLOG(4) << "Created new broker node " << assigned_name_.ToString();
  } else {
    DVLOG(4) << "Created new non-broker node " << this;
  }
}

Node::~Node() = default;

IpczResult Node::Close() {
  ShutDown();
  return IPCZ_RESULT_OK;
}

IpczResult Node::ConnectNode(IpczDriverHandle driver_transport,
                             IpczConnectNodeFlags flags,
                             absl::Span<IpczHandle> initial_portals) {
  std::vector<Ref<Portal>> portals(initial_portals.size());
  for (size_t i = 0; i < initial_portals.size(); ++i) {
    auto portal =
        MakeRefCounted<Portal>(WrapRefCounted(this), MakeRefCounted<Router>());
    portals[i] = portal;
    initial_portals[i] = Portal::ReleaseAsHandle(std::move(portal));
  }

  auto transport =
      MakeRefCounted<DriverTransport>(DriverObject(driver_, driver_transport));
  IpczResult result = NodeConnector::ConnectNode(WrapRefCounted(this),
                                                 transport, flags, portals);
  if (result != IPCZ_RESULT_OK) {
    // On failure the caller retains ownership of `driver_transport`. Release
    // it here so it doesn't get closed when `transport` is destroyed.
    transport->Release();

    // Wipe out the initial portals we created, since they are invalid and
    // effectively not returned to the caller on failure.
    for (Ref<Portal>& portal : portals) {
      Ref<Portal> doomed_portal = AdoptRef(portal.get());
    }
    return result;
  }
  return IPCZ_RESULT_OK;
}

NodeName Node::GetAssignedName() {
  absl::MutexLock lock(&mutex_);
  return assigned_name_;
}

Ref<NodeLink> Node::GetBrokerLink() {
  absl::MutexLock lock(&mutex_);
  return broker_link_;
}

void Node::SetAssignedName(const NodeName& name) {
  absl::MutexLock lock(&mutex_);
  ABSL_ASSERT(!assigned_name_.is_valid());
  assigned_name_ = name;
}

bool Node::AddConnection(const NodeName& remote_node_name,
                         Connection connection) {
  std::vector<BrokerLinkCallback> callbacks;
  {
    absl::ReleasableMutexLock lock(&mutex_);
    auto [it, inserted] = connections_.insert({remote_node_name, connection});
    if (!inserted) {
      lock.Release();
      connection.link->Deactivate();
      return false;
    }

    if (connection.link->remote_node_type() == Type::kBroker) {
      // The first connection accepted by a non-broker must be a connection to
      // its own broker.
      ABSL_ASSERT(connections_.size() == 1);
      ABSL_ASSERT(!broker_link_);
      broker_link_ = connection.link;
      broker_link_callbacks_.swap(callbacks);
    }
  }

  for (auto& callback : callbacks) {
    callback(connection.link);
  }
  return true;
}

absl::optional<Node::Connection> Node::GetConnection(const NodeName& name) {
  absl::MutexLock lock(&mutex_);
  auto it = connections_.find(name);
  if (it == connections_.end()) {
    return absl::nullopt;
  }
  return it->second;
}

Ref<NodeLink> Node::GetLink(const NodeName& name) {
  absl::MutexLock lock(&mutex_);
  auto it = connections_.find(name);
  if (it == connections_.end()) {
    return nullptr;
  }
  return it->second.link;
}

NodeName Node::GenerateRandomName() const {
  NodeName name;
  IpczResult result =
      driver_.GenerateRandomBytes(sizeof(name), IPCZ_NO_FLAGS, nullptr, &name);
  ABSL_ASSERT(result == IPCZ_RESULT_OK);
  return name;
}

void Node::SetAllocationDelegate(Ref<NodeLink> link) {
  absl::MutexLock lock(&mutex_);
  ABSL_ASSERT(!allocation_delegate_link_);
  allocation_delegate_link_ = std::move(link);
}

void Node::AllocateSharedMemory(size_t size,
                                AllocateSharedMemoryCallback callback) {
  Ref<NodeLink> delegate;
  {
    absl::MutexLock lock(&mutex_);
    delegate = allocation_delegate_link_;
  }

  if (delegate) {
    delegate->RequestMemory(size, std::move(callback));
  } else {
    callback(DriverMemory(driver_, size));
  }
}

void Node::EstablishLink(const NodeName& name, EstablishLinkCallback callback) {
  Ref<NodeLink> existing_link;
  Ref<NodeLink> our_broker;
  {
    absl::MutexLock lock(&mutex_);
    auto it = connections_.find(name);
    if (it != connections_.end()) {
      existing_link = it->second.link;
    } else if (type_ == Type::kNormal && broker_link_) {
      our_broker = broker_link_;

      auto [pending_it, inserted] = pending_introductions_.insert({name, {}});
      pending_it->second.push_back(std::move(callback));
      if (!inserted) {
        // There's already an introduction request out for this node, so there's
        // nothing more we need to do.
        return;
      }
    }
  }

  if (our_broker) {
    our_broker->RequestIntroduction(name);
    return;
  }

  // NOTE: `existing_link` may be null here, implying that we have failed.
  callback(existing_link.get());
}

void Node::HandleIntroductionRequest(NodeLink& from_node_link,
                                     const NodeName& for_node) {
  // NodeLink must never accept these requests on non-broker nodes.
  ABSL_ASSERT(type_ == Type::kBroker);

  const NodeName requestor = from_node_link.remote_node_name();

  DVLOG(4) << "Broker " << from_node_link.local_node_name().ToString()
           << " received introduction request for " << for_node.ToString()
           << " from " << requestor.ToString();

  const absl::optional<Connection> target_connection = GetConnection(for_node);
  if (!target_connection) {
    from_node_link.RejectIntroduction(for_node);
    return;
  }

  IntroduceRemoteNodes(from_node_link, *target_connection->link);
}

void Node::AcceptIntroduction(NodeLink& from_node_link,
                              const NodeName& name,
                              LinkSide side,
                              Node::Type remote_node_type,
                              uint32_t remote_protocol_version,
                              Ref<DriverTransport> transport,
                              Ref<NodeLinkMemory> memory) {
  // NodeLink should never dispatch this method to a node if the introduction
  // didn't come from a broker, so this assertion should always hold.
  ABSL_ASSERT(from_node_link.remote_node_type() == Node::Type::kBroker);

  const NodeName local_name = from_node_link.local_node_name();

  DVLOG(4) << "Node " << local_name.ToString() << " received introduction to "
           << name.ToString() << " from broker "
           << from_node_link.remote_node_name().ToString();

  Ref<NodeLink> new_link = NodeLink::CreateInactive(
      WrapRefCounted(this), side, local_name, name, remote_node_type,
      remote_protocol_version, transport, memory);
  ABSL_ASSERT(new_link);

  std::vector<EstablishLinkCallback> callbacks;
  {
    absl::MutexLock lock(&mutex_);
    auto [connection_it, inserted] =
        connections_.insert({name,
                             {
                                 .link = new_link,
                                 .broker = WrapRefCounted(&from_node_link),
                             }});
    if (!inserted) {
      // If both nodes race to request an introduction to each other, the
      // broker may send redundant introductions. It does however take care to
      // ensure that they're ordered consistently across both nodes, so
      // redundant introductions can be safely ignored by convention.
      return;
    }

    // If this node requested this introduction, we may have callbacks to run.
    // Note that it is not an error to receive an unrequested introduction,
    // since it is only necessary for one of the introduced nodes to have
    // requested it.
    auto it = pending_introductions_.find(name);
    if (it != pending_introductions_.end()) {
      callbacks = std::move(it->second);
      pending_introductions_.erase(it);
    }
  }

  new_link->Activate();
  for (auto& callback : callbacks) {
    callback(new_link.get());
  }
}

bool Node::CancelIntroduction(const NodeName& name) {
  std::vector<EstablishLinkCallback> callbacks;
  {
    absl::MutexLock lock(&mutex_);
    auto it = pending_introductions_.find(name);
    if (it == pending_introductions_.end()) {
      return false;
    }
    callbacks = std::move(it->second);
    pending_introductions_.erase(it);
  }

  for (auto& callback : callbacks) {
    callback(nullptr);
  }

  return true;
}

bool Node::RelayMessage(const NodeName& from_node, msg::RelayMessage& relay) {
  ABSL_ASSERT(type_ == Type::kBroker);
  auto link = GetLink(relay.params().destination);
  if (!link) {
    return true;
  }

  absl::Span<uint8_t> data = relay.GetArrayView<uint8_t>(relay.params().data);
  msg::AcceptRelayedMessage accept;
  accept.params().source = from_node;
  accept.params().data = accept.AllocateArray<uint8_t>(data.size());
  memcpy(accept.GetArrayData(accept.params().data), data.data(), data.size());
  accept.params().driver_objects =
      accept.AppendDriverObjects(relay.driver_objects());
  link->Transmit(accept);
  return true;
}

bool Node::AcceptRelayedMessage(msg::AcceptRelayedMessage& accept) {
  if (auto link = GetLink(accept.params().source)) {
    link->DispatchRelayedMessage(accept);
  }
  return true;
}

void Node::DropConnection(const NodeName& name) {
  Ref<NodeLink> link;
  bool lost_broker = false;
  {
    absl::MutexLock lock(&mutex_);
    auto it = connections_.find(name);
    if (it == connections_.end()) {
      return;
    }
    link = std::move(it->second.link);
    connections_.erase(it);

    const NodeName& local_name = link->local_node_name();
    DVLOG(4) << "Node " << local_name.ToString() << " dropping "
             << "link to " << link->remote_node_name().ToString();
    if (link == broker_link_) {
      DVLOG(4) << "Node " << local_name.ToString() << " lost its broker link";
      broker_link_.reset();
      lost_broker = true;
    }

    if (link == allocation_delegate_link_) {
      DVLOG(4) << "Node " << local_name.ToString()
               << " lost its allocation delegate";
      allocation_delegate_link_.reset();
    }
  }

  link->Deactivate();

  if (lost_broker) {
    CancelAllIntroductions();
  }
}

void Node::WaitForBrokerLinkAsync(BrokerLinkCallback callback) {
  Ref<NodeLink> broker_link;
  {
    absl::MutexLock lock(&mutex_);
    if (!broker_link_) {
      broker_link_callbacks_.push_back(std::move(callback));
      return;
    }

    broker_link = broker_link_;
  }

  callback(std::move(broker_link));
}

void Node::ShutDown() {
  ConnectionMap connections;
  {
    absl::MutexLock lock(&mutex_);
    connections_.swap(connections);
    broker_link_.reset();
    allocation_delegate_link_.reset();
  }

  for (const auto& entry : connections) {
    entry.second.link->Deactivate();
  }

  CancelAllIntroductions();
}

void Node::CancelAllIntroductions() {
  PendingIntroductionMap introductions;
  {
    absl::MutexLock lock(&mutex_);
    introductions.swap(pending_introductions_);
  }

  for (auto& [name, callbacks] : introductions) {
    for (auto& callback : callbacks) {
      callback(nullptr);
    }
  }
}

void Node::IntroduceRemoteNodes(NodeLink& first, NodeLink& second) {
  // Ensure that no other thread does the same introduction concurrently.
  const NodeName& first_name = first.remote_node_name();
  const NodeName& second_name = second.remote_node_name();
  const auto key = (first_name < second_name)
                       ? IntroductionKey(first_name, second_name)
                       : IntroductionKey(second_name, first_name);
  {
    absl::MutexLock lock(&mutex_);
    auto [it, inserted] = in_progress_introductions_.insert(key);
    if (!inserted) {
      return;
    }
  }

  DriverMemoryWithMapping buffer = NodeLinkMemory::AllocateMemory(driver_);
  auto [transport_for_first_node, transport_for_second_node] =
      DriverTransport::CreatePair(driver_, first.transport().get(),
                                  second.transport().get());
  first.AcceptIntroduction(second_name, LinkSide::kA, second.remote_node_type(),
                           second.remote_protocol_version(),
                           std::move(transport_for_first_node),
                           buffer.memory.Clone());
  second.AcceptIntroduction(first_name, LinkSide::kB, first.remote_node_type(),
                            first.remote_protocol_version(),
                            std::move(transport_for_second_node),
                            std::move(buffer.memory));

  absl::MutexLock lock(&mutex_);
  in_progress_introductions_.erase(key);
}

}  // namespace ipcz
