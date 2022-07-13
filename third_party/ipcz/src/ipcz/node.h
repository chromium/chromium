// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_IPCZ_NODE_H_
#define IPCZ_SRC_IPCZ_NODE_H_

#include <functional>

#include "ipcz/api_object.h"
#include "ipcz/driver_memory.h"
#include "ipcz/ipcz.h"
#include "ipcz/node_name.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/synchronization/mutex.h"
#include "third_party/abseil-cpp/absl/types/span.h"

namespace ipcz {

class NodeLink;

// A Node controls creation and interconnection of a collection of routers which
// can establish links to and from other routers in other nodes. Every node is
// assigned a globally unique name by a trusted broker node, and nodes may be
// introduced to each other exclusively through such brokers.
class Node : public APIObjectImpl<Node, APIObject::kNode> {
 public:
  enum class Type {
    // A broker node assigns its own name and is able to assign names to other
    // nodes upon connection. Brokers are trusted to introduce nodes to each
    // other upon request, and brokers may connect to other brokers in order to
    // share information and effectively bridge two node networks together.
    kBroker,

    // A "normal" (i.e. non-broker) node is assigned a permanent name by the
    // first broker node it connects to, and it can only make contact with other
    // nodes by requesting an introduction from that broker.
    kNormal,
  };

  // Constructs a new node of the given `type`, using `driver` to support IPC.
  // Note that `driver` must outlive the Node. `driver_node` is an arbitrary
  // driver-specific handle that may be used for additional context when
  // interfacing with the driver regarding this node.
  Node(Type type, const IpczDriver& driver, IpczDriverHandle driver_node);

  Type type() const { return type_; }
  const IpczDriver& driver() const { return driver_; }
  IpczDriverHandle driver_node() const { return driver_node_; }

  // APIObject:
  IpczResult Close() override;

  // Connects to another node using `driver_transport` for I/O to and from the
  // other node. `initial_portals` is a collection of new portals who may
  // immediately begin to route parcels over a link to the new node, assuming
  // the link is established successfully.
  IpczResult ConnectNode(IpczDriverHandle driver_transport,
                         IpczConnectNodeFlags flags,
                         absl::Span<IpczHandle> initial_portals);

  // Retrieves the name assigned to this node, if any.
  NodeName GetAssignedName();

  // Gets a reference to the node's broker link, if it has one.
  Ref<NodeLink> GetBrokerLink();

  // Sets this node's broker link, which is used e.g. to make introduction
  // requests.
  //
  // This is called by a NodeConnector implementation after accepting a valid
  // handshake message from a broker node, and `link` will be used as this
  // node's permanent broker.
  //
  // Note that like any other NodeLink used by this Node, the same `link` must
  // also be registered via AddLink() to associate it with its remote Node's
  // name. This is also done by NodeConnector.
  void SetBrokerLink(Ref<NodeLink> link);

  // Sets this node's assigned name as given by a broker. NodeConnector is
  // responsible for calling on non-broker Nodes this after receiving the
  // expected handshake from a broker. Must not be called on broker nodes, as
  // they assign their own name at construction time.
  void SetAssignedName(const NodeName& name);

  // Registers a new NodeLink for the given `remote_node_name`.
  bool AddLink(const NodeName& remote_node_name, Ref<NodeLink> link);

  // Generates a new random NodeName using this node's driver as a source of
  // randomness.
  NodeName GenerateRandomName() const;

  // Requests allocation of a new shared memory object of the given size.
  // `callback` is invoked with the new object when allocation is complete.
  // This operation is asynchronous if allocation is delegated to another node,
  // but if this node can allocate directly through the driver, `callback` is
  // invoked with the result before this method returns.
  using AllocateSharedMemoryCallback = std::function<void(DriverMemory)>;
  void AllocateSharedMemory(size_t size, AllocateSharedMemoryCallback callback);

 private:
  ~Node() override;

  // Deactivates all NodeLinks and their underlying driver transports in
  // preparation for this node's imminent destruction.
  void ShutDown();

  const Type type_;
  const IpczDriver& driver_;
  const IpczDriverHandle driver_node_;

  absl::Mutex mutex_;

  // The name assigned to this node by the first broker it connected to, or
  // self-assigned if this is a broker node. Once assigned, this name remains
  // constant through the lifetime of the node.
  NodeName assigned_name_ ABSL_GUARDED_BY(mutex_);

  // A link to the first broker this node connected to. If this link is broken,
  // the node will lose all its other links too.
  Ref<NodeLink> broker_link_ ABSL_GUARDED_BY(mutex_);

  // Lookup table of broker-assigned node names and links to those nodes. All of
  // these links and their associated names are received by the `broker_link_`
  // if this is a non-broker node. If this is a broker node, these links are
  // either assigned by this node itself, or received from other brokers in the
  // system.
  using NodeLinkMap = absl::flat_hash_map<NodeName, Ref<NodeLink>>;
  NodeLinkMap node_links_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace ipcz

#endif  // IPCZ_SRC_IPCZ_NODE_H_
