// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/node.h"

#include <optional>
#include <utility>
#include <vector>

#include "ipcz/driver_memory.h"
#include "ipcz/features.h"
#include "ipcz/ipcz.h"
#include "ipcz/link_side.h"
#include "ipcz/node_connector.h"
#include "ipcz/node_link.h"
#include "ipcz/node_link_memory.h"
#include "ipcz/router.h"
#include "third_party/abseil-cpp/absl/base/macros.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/abseil-cpp/absl/container/inlined_vector.h"
#include "third_party/abseil-cpp/absl/synchronization/mutex.h"
#include "third_party/abseil-cpp/absl/types/span.h"
#include "util/log.h"
#include "util/ref_counted.h"

namespace ipcz {

namespace {

// Returns a copy of the structure pointed to by `options` if non-null;
// otherwise returns a default set of options. This function will also adapt
// to newer or older versions of the input options on input, coercing them into
// the current implementation's version if needed.
IpczCreateNodeOptions CopyOrUseDefaultOptions(
    const IpczCreateNodeOptions* options) {
  IpczCreateNodeOptions copied_options = {0};
  if (options) {
    memcpy(&copied_options, options,
           std::min(options->size, sizeof(copied_options)));
  }
  copied_options.size = sizeof(copied_options);
  return copied_options;
}

}  // namespace

// A pending introduction tracks progress of one or more outstanding
// introduction requests for a single node in the system.
class Node::PendingIntroduction {
 public:
  // Constructs a new object to track introduction a specific node. `broker`
  // is the sequence of broker nodes queried for the introduction.
  explicit PendingIntroduction(absl::Span<const Ref<NodeLink>> brokers) {
    pending_replies_.reserve(brokers.size());
    for (const auto& broker : brokers) {
      pending_replies_.insert(broker->remote_node_name());
    }
  }

  // Indicates that all pending responses have come back indicating failure,
  // and that the pending introduction itself has failed.
  bool failed() const { return pending_replies_.empty(); }

  // Marks a specific broker as having responded with rejection.
  void NotifyFailureFrom(NodeLink& rejecting_broker) {
    pending_replies_.erase(rejecting_broker.remote_node_name());
  }

  // Registers a new callback to be invoked once the introduction process has
  // completed, regardless of success or failure.
  void AddCallback(Node::EstablishLinkCallback callback) {
    callbacks_.push_back(std::move(callback));
  }

  // Runs all callbacks associated with this introduction. If the introduction
  // failed, `result` will be null. Otherwise it's a link to the newly
  // introduced remote node.
  void Finish(NodeLink* result) {
    for (auto& callback : callbacks_) {
      callback(result);
    }
  }

 private:
  // Set of brokers from whom we're still expecting a reply for this pending
  // introduction request.
  absl::flat_hash_set<NodeName> pending_replies_;

  // Callbacks to be invoked once the introduction is finished.
  std::vector<EstablishLinkCallback> callbacks_;
};

Node::Node(Type type,
           const IpczDriver& driver,
           const IpczCreateNodeOptions* options)
    : type_(type),
      driver_(driver),
      options_(CopyOrUseDefaultOptions(options)),
      features_(Features::FromNodeOptions(options)) {
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
  std::vector<Ref<Router>> routers(initial_portals.size());
  for (size_t i = 0; i < initial_portals.size(); ++i) {
    auto router = MakeRefCounted<Router>();
    routers[i] = router;
    initial_portals[i] = Router::ReleaseAsHandle(std::move(router));
  }

  auto transport =
      MakeRefCounted<DriverTransport>(DriverObject(driver_, driver_transport));
  IpczResult result = NodeConnector::ConnectNode(WrapRefCounted(this),
                                                 transport, flags, routers);
  if (result != IPCZ_RESULT_OK) {
    // On failure the caller retains ownership of `driver_transport`. Release
    // it here so it doesn't get closed when `transport` is destroyed.
    transport->Release();

    // Wipe out the routers we created, since they are invalid and effectively
    // not returned to the caller on failure.
    for (Ref<Router>& router : routers) {
      Ref<Router> doomed_router = AdoptRef(router.get());
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

bool Node::AddConnection(const NodeName& remote_node_name,
                         Connection connection) {
  std::vector<BrokerLinkCallback> callbacks;
  {
    absl::MutexLock lock(&mutex_);
    for (;;) {
      auto it = connections_.find(remote_node_name);
      if (it == connections_.end()) {
        break;
      }

      // Assume that if we're getting a new connection to an already-known node,
      // it must be because the application has explicitly initiated a new
      // connection to the same node and it expects the previous connection to
      // be replaced.
      const Ref<NodeLink> link = it->second.link;
      mutex_.Unlock();
      DropConnection(*link);
      mutex_.Lock();
    }

    connections_.insert({remote_node_name, connection});
    const bool remote_is_broker =
        connection.link->remote_node_type() == Type::kBroker;
    const bool local_is_broker = type_ == Type::kBroker;
    if (local_is_broker && remote_is_broker) {
      // We're a broker, and this is a link to some other broker. We retain a
      // separate mapping of other brokers so they can be consulted for
      // introductions.
      other_brokers_.insert({remote_node_name, connection.link});
    } else if (remote_is_broker) {
      // The first connection accepted by a non-broker must be a connection to
      // its own broker.
      ABSL_ASSERT(connections_.size() == 1);
      ABSL_ASSERT(!broker_link_);
      broker_link_ = connection.link;
      broker_link_callbacks_.swap(callbacks);
      assigned_name_ = broker_link_->local_node_name();
    }
  }

  for (auto& callback : callbacks) {
    callback(connection.link);
  }
  return true;
}

std::optional<Node::Connection> Node::GetConnection(const NodeName& name) {
  absl::MutexLock lock(&mutex_);
  auto it = connections_.find(name);
  if (it == connections_.end()) {
    return std::nullopt;
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
  absl::InlinedVector<Ref<NodeLink>, 2> brokers_to_query;
  {
    absl::MutexLock lock(&mutex_);
    auto it = connections_.find(name);
    if (it != connections_.end()) {
      existing_link = it->second.link;
    } else {
      if (type_ == Type::kNormal && broker_link_) {
        brokers_to_query.push_back(broker_link_);
      } else if (!other_brokers_.empty()) {
        ABSL_ASSERT(type_ == Type::kBroker);
        brokers_to_query.reserve(other_brokers_.size());
        for (const auto& [broker_name, link] : other_brokers_) {
          brokers_to_query.push_back(link);
        }
      }

      if (!brokers_to_query.empty()) {
        auto [pending_it, inserted] =
            pending_introductions_.insert({name, nullptr});
        auto& intro = pending_it->second;
        if (!intro) {
          intro = std::make_unique<PendingIntroduction>(
              absl::MakeSpan(brokers_to_query));
        }
        intro->AddCallback(std::move(callback));
        if (!inserted) {
          // There was already a pending introduction we can wait for.
          return;
        }
      }
    }
  }

  if (!brokers_to_query.empty()) {
    for (const auto& broker : brokers_to_query) {
      broker->RequestIntroduction(name);
    }
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

  const std::optional<Connection> target_connection = GetConnection(for_node);
  if (!target_connection) {
    // We are not familiar with the requested node. Attempt to establish our own
    // link to it first, then try again.
    EstablishLink(for_node, [self = WrapRefCounted(this),
                             requestor = WrapRefCounted(&from_node_link),
                             name = for_node](NodeLink* link) {
      if (!link) {
        requestor->RejectIntroduction(name);
        return;
      }

      self->HandleIntroductionRequest(*requestor, name);
    });
    return;
  }

  const bool is_target_in_network = !target_connection->broker;
  const bool is_target_broker =
      target_connection->link == target_connection->broker;
  const bool is_requestor_broker =
      from_node_link.remote_node_type() == Type::kBroker;
  if (is_requestor_broker && is_target_broker) {
    DLOG(ERROR) << "Invalid introduction request from broker "
                << requestor.ToString() << " for broker "
                << for_node.ToString();
    return;
  }

  if (is_target_broker || is_requestor_broker || is_target_in_network ||
      target_connection->broker->link_side().is_side_a()) {
    // If one of the two nodes being introduced is a broker, or if the target
    // is in-network (which implies the requestor is too, if it's not a broker)
    // then we are the only node that can introduce these two nodes.
    //
    // Otherwise if this is an introduction between two non-brokers in separate
    // networks, by convention we can only perform the introduction if we're on
    // side A of the link between the two relevant brokers.
    IntroduceRemoteNodes(from_node_link, *target_connection->link);
    return;
  }

  // This is an introduction between two non-brokers in separate networks, and
  // we (one of the networks' brokers) are on side B of the link to the other
  // network's broker. This introduction is therefore the other broker's
  // responsibility.
  msg::RequestIndirectIntroduction request;
  request.v0()->source_node = from_node_link.remote_node_name();
  request.v0()->target_node = target_connection->link->remote_node_name();
  target_connection->broker->Transmit(request);
}

void Node::AcceptIntroduction(NodeLink& from_node_link,
                              const NodeName& name,
                              LinkSide side,
                              Node::Type remote_node_type,
                              uint32_t remote_protocol_version,
                              const Features& remote_features,
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
      remote_protocol_version, remote_features, transport, memory);
  ABSL_ASSERT(new_link);

  std::unique_ptr<PendingIntroduction> pending_introduction;
  {
    absl::MutexLock lock(&mutex_);
    if (type_ == Type::kNormal && !broker_link_) {
      // If we've lost our broker connection, we should ignore any further
      // introductions that arrive.
      return;
    }

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
      pending_introduction = std::move(it->second);
      pending_introductions_.erase(it);
    }
  }

  new_link->Activate();
  if (pending_introduction) {
    pending_introduction->Finish(new_link.get());
  }
}

void Node::NotifyIntroductionFailed(NodeLink& from_broker,
                                    const NodeName& name) {
  std::unique_ptr<PendingIntroduction> failed_introduction;
  {
    absl::MutexLock lock(&mutex_);
    auto it = pending_introductions_.find(name);
    if (it == pending_introductions_.end()) {
      return;
    }

    auto& intro = it->second;
    intro->NotifyFailureFrom(from_broker);
    if (!intro->failed()) {
      // We're still waiting for replies from one or more other brokers.
      return;
    }

    failed_introduction = std::move(intro);
    pending_introductions_.erase(it);
  }

  failed_introduction->Finish(nullptr);
}

bool Node::RelayMessage(const NodeName& from_node, msg::RelayMessage& relay) {
  ABSL_ASSERT(type_ == Type::kBroker);
  auto link = GetLink(relay.v0()->destination);
  if (!link) {
    return true;
  }

  absl::Span<uint8_t> data = relay.GetArrayView<uint8_t>(relay.v0()->data);
  msg::AcceptRelayedMessage accept;
  accept.v0()->source = from_node;
  accept.v0()->data = accept.AllocateArray<uint8_t>(data.size());
  accept.v0()->padding = 0;
  memcpy(accept.GetArrayData(accept.v0()->data), data.data(), data.size());
  accept.v0()->driver_objects =
      accept.AppendDriverObjects(relay.driver_objects());
  link->Transmit(accept);
  return true;
}

bool Node::AcceptRelayedMessage(msg::AcceptRelayedMessage& accept) {
  if (auto link = GetLink(accept.v0()->source)) {
    link->DispatchRelayedMessage(accept);
  }
  return true;
}

void Node::DropConnection(const NodeLink& connection_link) {
  Ref<NodeLink> link;
  std::vector<NodeName> pending_introductions;
  bool lost_broker = false;
  {
    absl::MutexLock lock(&mutex_);
    auto it = connections_.find(connection_link.remote_node_name());
    if (it == connections_.end() || it->second.link != &connection_link) {
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
    } else if (link->remote_node_type() == Type::kBroker) {
      other_brokers_.erase(link->remote_node_name());
    }

    if (link == allocation_delegate_link_) {
      DVLOG(4) << "Node " << local_name.ToString()
               << " lost its allocation delegate";
      allocation_delegate_link_.reset();
    }

    // Accumulate the set of currently pending introductions. If any of them are
    // awaiting a response from the dropped link, their expectations will be
    // updated accordingly by NotifyIntroductionFailed() below.
    pending_introductions.reserve(pending_introductions_.size());
    for (auto& [target, intro] : pending_introductions_) {
      pending_introductions.push_back(target);
    }
  }

  link->Deactivate();

  if (lost_broker) {
    // Break all connections if the broker is lost. In practice we should only
    // need to break connections which were introduced by the lost broker, but
    // there's less risk of weird future inconsistencies if we just say that as
    // a rule, primary broker disconnection serves as a sort of "reset" for a
    // node. The node can be re-connected to a broker and continue operating
    // normally from there.
    ShutDown();
  } else {
    for (auto& target : pending_introductions) {
      NotifyIntroductionFailed(*link, target);
    }
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

bool Node::HandleIndirectIntroductionRequest(NodeLink& from_node_link,
                                             const NodeName& our_node,
                                             const NodeName& their_node) {
  // Enforced by NodeLink before dispatching to this Node.
  ABSL_ASSERT(type_ == Type::kBroker);
  ABSL_ASSERT(from_node_link.remote_node_type() == Type::kBroker);
  ABSL_ASSERT(from_node_link.link_side().is_side_a());

  std::optional<Connection> connection_to_their_node =
      GetConnection(their_node);
  if (!connection_to_their_node) {
    // We need to establish our own direct connection to `their_node` before we
    // can help introduce it to `our_node`.
    EstablishLink(their_node, [self = WrapRefCounted(this),
                               their_broker = WrapRefCounted(&from_node_link),
                               our_node, their_node](NodeLink* link) {
      if (!link) {
        // If we could not get our own link to the identified node, then we
        // can't complete the introduction request. Notify our own node of the
        // failure in case they were awaiting such an introduction.
        std::optional<Connection> connection_to_our_node =
            self->GetConnection(our_node);
        if (connection_to_our_node) {
          connection_to_our_node->link->RejectIntroduction(their_node);
        }
        return;
      }
      self->HandleIndirectIntroductionRequest(*their_broker, our_node,
                                              their_node);
    });
    return true;
  }

  std::optional<Connection> connection_to_our_node = GetConnection(our_node);
  if (!connection_to_our_node) {
    // We may have lost this connection for any number of reasons, but in any
    // case we can't fulfill the request.
    connection_to_their_node->link->RejectIntroduction(our_node);
    return true;
  }

  IntroduceRemoteNodes(*connection_to_our_node->link,
                       *connection_to_their_node->link);
  return true;
}

void Node::ShutDown() {
  ConnectionMap connections;
  {
    absl::MutexLock lock(&mutex_);
    connections_.swap(connections);
    broker_link_.reset();
    allocation_delegate_link_.reset();
    other_brokers_.clear();
    assigned_name_ = {};
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
  for (auto& [name, intro] : introductions) {
    intro->Finish(nullptr);
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

  const absl::Cleanup remove_intro_key = [this, &key] {
    absl::MutexLock lock(&mutex_);
    in_progress_introductions_.erase(key);
  };

  DriverMemoryWithMapping buffer = NodeLinkMemory::AllocateMemory(driver_);
  if (!buffer.memory.is_valid()) {
    return;
  }

  DriverMemory cloned_buffer = buffer.memory.Clone();
  if (!cloned_buffer.is_valid()) {
    return;
  }

  auto [transport_for_first_node, transport_for_second_node] =
      DriverTransport::CreatePair(driver_, first.transport().get(),
                                  second.transport().get());
  first.AcceptIntroduction(
      second_name, LinkSide::kA, second.remote_node_type(),
      second.remote_protocol_version(), second.remote_features(),
      std::move(transport_for_first_node), std::move(cloned_buffer));
  second.AcceptIntroduction(
      first_name, LinkSide::kB, first.remote_node_type(),
      first.remote_protocol_version(), first.remote_features(),
      std::move(transport_for_second_node), std::move(buffer.memory));
}

}  // namespace ipcz
