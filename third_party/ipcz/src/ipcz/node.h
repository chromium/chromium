// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_IPCZ_NODE_H_
#define IPCZ_SRC_IPCZ_NODE_H_

#include "ipcz/api_object.h"
#include "ipcz/ipcz.h"
#include "ipcz/node_name.h"
#include "third_party/abseil-cpp/absl/synchronization/mutex.h"

namespace ipcz {

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

  Node(Type type, const IpczDriver& driver, IpczDriverHandle driver_node);

  Type type() const { return type_; }
  const IpczDriver& driver() const { return driver_; }
  IpczDriverHandle driver_node() const { return driver_node_; }

  // APIObject:
  IpczResult Close() override;

  // Retrieves the name assigned to this node, if any.
  NodeName GetAssignedName();

  // Generates a new random NodeName using this node's driver as a source of
  // randomness.
  NodeName GenerateRandomName() const;

 private:
  ~Node() override;

  const Type type_;
  const IpczDriver driver_;
  const IpczDriverHandle driver_node_;

  absl::Mutex mutex_;

  // The name assigned to this node by the first broker it connected to, or
  // self-assigned if this is a broker node. Once assigned, this name remains
  // constant through the lifetime of the node.
  NodeName assigned_name_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace ipcz

#endif  // IPCZ_SRC_IPCZ_NODE_H_
