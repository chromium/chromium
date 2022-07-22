// Copyright 2022 The Chromium Authors. All rights reserved.
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

void Node::SetBrokerLink(Ref<NodeLink> link) {
  absl::MutexLock lock(&mutex_);
  ABSL_ASSERT(!broker_link_);
  broker_link_ = std::move(link);
}

void Node::SetAssignedName(const NodeName& name) {
  absl::MutexLock lock(&mutex_);
  ABSL_ASSERT(!assigned_name_.is_valid());
  assigned_name_ = name;
}

bool Node::AddLink(const NodeName& remote_node_name, Ref<NodeLink> link) {
  {
    absl::MutexLock lock(&mutex_);
    auto [it, inserted] = node_links_.insert({remote_node_name, link});
    if (inserted) {
      return true;
    }
  }

  link->Deactivate();
  return false;
}

Ref<NodeLink> Node::GetLink(const NodeName& name) {
  absl::MutexLock lock(&mutex_);
  auto it = node_links_.find(name);
  if (it == node_links_.end()) {
    return nullptr;
  }
  return it->second;
}

NodeName Node::GenerateRandomName() const {
  NodeName name;
  IpczResult result =
      driver_.GenerateRandomBytes(sizeof(name), IPCZ_NO_FLAGS, nullptr, &name);
  ABSL_ASSERT(result == IPCZ_RESULT_OK);
  return name;
}

void Node::AllocateSharedMemory(size_t size,
                                AllocateSharedMemoryCallback callback) {
  // TODO: Implement delegated allocation when this Node is connected to another
  // with the IPCZ_CONNECT_NODE_TO_ALLOCATION_DELEGATE flag set. For now we
  // assume all nodes can perform direct allocation.
  callback(DriverMemory(driver_, size));
}

void Node::EstablishLink(const NodeName& name, EstablishLinkCallback callback) {
  Ref<NodeLink> broker;
  Ref<NodeLink> link;
  {
    absl::MutexLock lock(&mutex_);
    broker = broker_link_;
    auto it = node_links_.find(name);
    if (it != node_links_.end()) {
      link = it->second;
    } else if (type_ == Type::kNormal && broker) {
      auto [pending_it, inserted] = pending_introductions_.insert({name, {}});
      pending_it->second.push_back(std::move(callback));
      if (!inserted) {
        // There's already an introduction request out for this node, so there's
        // nothing more we need to do.
        return;
      }
    }
  }

  if (broker && !link) {
    broker->RequestIntroduction(name);
  } else {
    callback(link.get());
  }
}

void Node::HandleIntroductionRequest(NodeLink& from_node_link,
                                     const NodeName& for_node) {
  // NodeLink must never accept these requests on non-broker nodes.
  ABSL_ASSERT(type_ == Type::kBroker);

  const NodeName requestor = from_node_link.remote_node_name();

  DVLOG(4) << "Broker " << from_node_link.local_node_name().ToString()
           << " received introduction request for " << for_node.ToString()
           << " from " << requestor.ToString();

  // A key which uniquely identifies the pair of nodes being introduced
  // regardless of who requested the introduction.
  const auto key = (requestor < for_node)
                       ? IntroductionKey(requestor, for_node)
                       : IntroductionKey(for_node, requestor);

  Ref<NodeLink> target_link;
  {
    absl::MutexLock lock(&mutex_);
    auto it = node_links_.find(for_node);
    if (it != node_links_.end()) {
      target_link = it->second;

      auto [intro_it, inserted] = in_progress_introductions_.insert(key);
      if (!inserted) {
        // We're already introducing the same two nodes, so drop this request.
        return;
      }
    }
  }

  if (!target_link) {
    from_node_link.RejectIntroduction(for_node);
    return;
  }

  DriverMemoryWithMapping buffer = NodeLinkMemory::AllocateMemory(driver_);
  auto [transport_for_target, transport_for_requestor] =
      DriverTransport::CreatePair(driver_, target_link->transport().get(),
                                  from_node_link.transport().get());
  target_link->AcceptIntroduction(
      requestor, LinkSide::kA, from_node_link.remote_protocol_version(),
      std::move(transport_for_target), buffer.memory.Clone());
  from_node_link.AcceptIntroduction(
      for_node, LinkSide::kB, target_link->remote_protocol_version(),
      std::move(transport_for_requestor), std::move(buffer.memory));

  absl::MutexLock lock(&mutex_);
  in_progress_introductions_.erase(key);
}

void Node::AcceptIntroduction(NodeLink& from_node_link,
                              const NodeName& name,
                              LinkSide side,
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

  Ref<NodeLink> new_link = NodeLink::Create(
      WrapRefCounted(this), side, local_name, name, Type::kNormal,
      remote_protocol_version, transport, std::move(memory));
  ABSL_ASSERT(new_link);

  std::vector<EstablishLinkCallback> callbacks;
  {
    absl::MutexLock lock(&mutex_);
    auto [link_it, inserted] = node_links_.insert({name, new_link});
    if (!inserted) {
      // If both nodes race to request an introduction to each other, the
      // broker may send redundant introductions. It does however take care to
      // ensure that they're ordered consistently across both nodes, so
      // redundant introductions can be safely ignored by convention.
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

  if (transport) {
    transport->Activate();
  }

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

void Node::DropLink(const NodeName& name) {
  Ref<NodeLink> link;
  bool lost_broker = false;
  {
    absl::MutexLock lock(&mutex_);
    auto it = node_links_.find(name);
    if (it == node_links_.end()) {
      return;
    }
    link = std::move(it->second);
    node_links_.erase(it);

    DVLOG(4) << "Node " << link->local_node_name().ToString() << " dropping "
             << " link to " << link->remote_node_name().ToString();
    if (link == broker_link_) {
      DVLOG(4) << "Node " << link->local_node_name().ToString()
               << " has lost its broker link";
      broker_link_.reset();
      lost_broker = true;
    }
  }

  link->Deactivate();

  if (lost_broker) {
    CancelAllIntroductions();
  }
}

void Node::ShutDown() {
  NodeLinkMap node_links;
  {
    absl::MutexLock lock(&mutex_);
    std::swap(node_links_, node_links);
    broker_link_.reset();
  }

  for (const auto& entry : node_links) {
    entry.second->Deactivate();
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

}  // namespace ipcz
