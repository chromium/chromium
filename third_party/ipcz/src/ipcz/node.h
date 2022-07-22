// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_IPCZ_NODE_H_
#define IPCZ_SRC_IPCZ_NODE_H_

#include <functional>

#include "ipcz/api_object.h"
#include "ipcz/driver_memory.h"
#include "ipcz/ipcz.h"
#include "ipcz/link_side.h"
#include "ipcz/node_name.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "third_party/abseil-cpp/absl/synchronization/mutex.h"
#include "third_party/abseil-cpp/absl/types/span.h"

namespace ipcz {

class NodeLink;
class NodeLinkMemory;

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

  // Returns a reference to the NodeLink used by this Node to communicate with
  // the remote node identified by `name`; or null if this node has no NodeLink
  // connected to that node.
  Ref<NodeLink> GetLink(const NodeName& name);

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

  // Asynchronously attempts to establish a new NodeLink directly to the named
  // node, invoking `callback` when complete. On success, this node will retain
  // a new NodeLink to the named node, and `callback` will be invoked with a
  // reference to that link. Otherwise `callback` will be invoked with a null
  // reference.
  //
  // If the calling node already has a link to the named node, `callback` may
  // be invoked synchronously with a link to that node before this method
  // returns.
  using EstablishLinkCallback = std::function<void(NodeLink*)>;
  void EstablishLink(const NodeName& name, EstablishLinkCallback callback);

  // Handles an incoming introduction request. Must only be called on a broker
  // node. If this broker has a NodeLink to the node named by `for_node`, it
  // will introduce that node and the remote node on `from_node_link`.
  void HandleIntroductionRequest(NodeLink& from_node_link,
                                 const NodeName& for_node);

  // Accepts an introduction received from the broker. `transport` and `memory`
  // can be used to establish a new NodeLink to the remote node, whose name is
  // `name`. The NodeLink must assume a role as the given `side` of the link.
  void AcceptIntroduction(NodeLink& from_node_link,
                          const NodeName& name,
                          LinkSide side,
                          uint32_t remote_protocol_version,
                          Ref<DriverTransport> transport,
                          Ref<NodeLinkMemory> memory);

  // Handles a rejected introduction from the broker. This is called on a
  // non-broker node that previously requested an introduction to `name` if
  // the broker could not satisfy the request.
  bool CancelIntroduction(const NodeName& name);

  // Drops this node's link to the named node, if one exists.
  void DropLink(const NodeName& name);

 private:
  ~Node() override;

  // Deactivates all NodeLinks and their underlying driver transports in
  // preparation for this node's imminent destruction.
  void ShutDown();

  // Resolves all pending introduction requests with a null link, implying
  // failure.
  void CancelAllIntroductions();

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

  // A map of other nodes to which this node is waiting for an introduction from
  // `broker_link_`. Once such an introduction is received, all callbacks for
  // that NodeName are executed.
  using PendingIntroductionMap =
      absl::flat_hash_map<NodeName, std::vector<EstablishLinkCallback>>;
  PendingIntroductionMap pending_introductions_ ABSL_GUARDED_BY(mutex_);

  // Nodes may race to request introductions to each other from the same broker.
  // This can lead to redundant introductions being sent which the requesting
  // nodes should be able to ignore. However, the following could occur on a
  // broker which is processing a request from node A on Thread 1 while also
  // processing a request from node B on thread 2:
  //
  //    Thread 1                       Thread 2                      Time
  //    ---                            ---                             |
  //    A requests intro to B          B requests intro to A           v
  //    Send B intro X to A
  //                                   Send A intro Y to B
  //    Send A intro X to B
  //                                   Send B intro Y to A
  //
  // Each unique intro shares either end of a transport with its recipients,
  // so both A and B must accept the same introduction (either X or Y). In this
  // scenario however, A will first receive and accept intro X, and will ignore
  // intro Y as redundant. But B will receive intro Y first and ignore intro X
  // as redundant. This is bad.
  //
  // The set of `in_progress_introductions_` allows this (broker) node to guard
  // against such interleaved introductions. Immediately before sending an intro
  // to both recipients, a key identifying them is placed into the set. This key
  // is removed immediately after both introductions are sent. If another thread
  // is asked to introduce the same two nodes while the key is still present, it
  // will ignore the request and send nothing.
  using IntroductionKey = std::pair<NodeName, NodeName>;
  absl::flat_hash_set<IntroductionKey> in_progress_introductions_
      ABSL_GUARDED_BY(mutex_);
};

}  // namespace ipcz

#endif  // IPCZ_SRC_IPCZ_NODE_H_
