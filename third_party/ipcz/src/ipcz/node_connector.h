// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_IPCZ_NODE_CONNECTOR_H_
#define IPCZ_SRC_IPCZ_NODE_CONNECTOR_H_

#include <cstdint>
#include <functional>
#include <vector>

#include "ipcz/driver_transport.h"
#include "ipcz/ipcz.h"
#include "ipcz/link_side.h"
#include "ipcz/node_messages.h"
#include "third_party/abseil-cpp/absl/types/span.h"
#include "util/ref_counted.h"

namespace ipcz {

class Node;
class NodeLink;
class Portal;

// A NodeConnector activates and temporarily attaches itself to a
// DriverTransport to listen for and transmit introductory messages between two
// nodes invoking ConnectNode() on opposite ends of the same transport pair.
//
// Once an initial handshake is complete the underlying transport is adopted by
// a new NodeLink and handed off to the local Node to communicate with the
// remote node, and this object is destroyed.
class NodeConnector : public RefCounted, public msg::NodeMessageListener {
 public:
  // Constructs a new NodeConnector to send and receive a handshake on
  // `transport`. The specific type of connector to create is determined by a
  // combination of the Node::Type of `node` and the value of `flags`.
  //
  // If a connection is successfully established, `transport` will eventually
  // be adopted by a NodeLink and passed to `node` for use. Otherwise, all
  // `initial_portals` will observe peer closure.
  //
  // In either case this object invokes `callback` if non-null and then destroys
  // itself once the handshake is complete. If this fails, the NodeLink given
  // to the callback will be null.
  using ConnectCallback = std::function<void(Ref<NodeLink> new_link)>;
  static IpczResult ConnectNode(Ref<Node> node,
                                Ref<DriverTransport> transport,
                                IpczConnectNodeFlags flags,
                                const std::vector<Ref<Portal>>& initial_portals,
                                ConnectCallback callback = nullptr);

  virtual bool Connect() = 0;

 protected:
  NodeConnector(Ref<Node> node,
                Ref<DriverTransport> transport,
                IpczConnectNodeFlags flags,
                std::vector<Ref<Portal>> waiting_portals,
                ConnectCallback callback);
  ~NodeConnector() override;

  size_t num_portals() const { return waiting_portals_.size(); }

  // Invoked once by the implementation when it has completed the handshake.
  // `new_link` has already assumed ownership of the underlying transport and
  // is listening for incoming messages on it. Destroys `this`.
  void AcceptConnection(Ref<NodeLink> new_link,
                        LinkSide link_side,
                        uint32_t num_remote_portals);

  // Invoked if the transport observes an error before receiving the expected
  // handshake message, or if the implementation receives any message other than
  // the one handshake message it expects to see first.
  void RejectConnection();

  const Ref<Node> node_;
  const Ref<DriverTransport> transport_;
  const IpczConnectNodeFlags flags_;
  const std::vector<Ref<Portal>> waiting_portals_;
  Ref<NodeConnector> active_self_;

 private:
  bool ActivateTransportAndConnect();
  void EstablishWaitingPortals(Ref<NodeLink> to_link,
                               LinkSide link_side,
                               size_t max_valid_portals);

  // NodeMessageListener overrides:
  void OnTransportError() override;

  const ConnectCallback callback_;
};

}  // namespace ipcz

#endif  // IPCZ_SRC_IPCZ_NODE_CONNECTOR_H_
