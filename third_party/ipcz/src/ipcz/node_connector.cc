// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/node_connector.h"

#include <algorithm>
#include <utility>

#include "ipcz/driver_memory.h"
#include "ipcz/driver_transport.h"
#include "ipcz/ipcz.h"
#include "ipcz/link_side.h"
#include "ipcz/node_link.h"
#include "ipcz/portal.h"
#include "ipcz/remote_router_link.h"
#include "ipcz/router.h"
#include "ipcz/sublink_id.h"
#include "third_party/abseil-cpp/absl/base/macros.h"
#include "util/log.h"
#include "util/ref_counted.h"

namespace ipcz {

namespace {

class NodeConnectorForBrokerToNonBroker : public NodeConnector {
 public:
  NodeConnectorForBrokerToNonBroker(Ref<Node> node,
                                    Ref<DriverTransport> transport,
                                    IpczConnectNodeFlags flags,
                                    std::vector<Ref<Portal>> waiting_portals,
                                    ConnectCallback callback)
      : NodeConnector(std::move(node),
                      std::move(transport),
                      flags,
                      std::move(waiting_portals),
                      std::move(callback)) {}

  ~NodeConnectorForBrokerToNonBroker() override = default;

  // NodeConnector:
  bool Connect() override {
    DVLOG(4) << "Sending direct ConnectFromBrokerToNonBroker from broker "
             << broker_name_.ToString() << " to new node "
             << new_remote_node_name_.ToString() << " with " << num_portals()
             << " initial portals";

    ABSL_ASSERT(node_->type() == Node::Type::kBroker);
    msg::ConnectFromBrokerToNonBroker connect;
    connect.params().broker_name = broker_name_;
    connect.params().receiver_name = new_remote_node_name_;
    connect.params().protocol_version = msg::kProtocolVersion;
    connect.params().num_initial_portals =
        checked_cast<uint32_t>(num_portals());
    return IPCZ_RESULT_OK == transport_->Transmit(connect);
  }

  // NodeMessageListener overrides:
  bool OnConnectFromNonBrokerToBroker(
      msg::ConnectFromNonBrokerToBroker& connect) override {
    DVLOG(4) << "Accepting ConnectFromNonBrokerToBroker on broker "
             << broker_name_.ToString() << " from new node "
             << new_remote_node_name_.ToString();
    AcceptConnection(
        NodeLink::Create(node_, LinkSide::kA, broker_name_,
                         new_remote_node_name_, Node::Type::kNormal,
                         connect.params().protocol_version, transport_),
        LinkSide::kA, connect.params().num_initial_portals);
    return true;
  }

 private:
  const NodeName broker_name_{node_->GetAssignedName()};
  const NodeName new_remote_node_name_{node_->GenerateRandomName()};
};

class NodeConnectorForNonBrokerToBroker : public NodeConnector {
 public:
  NodeConnectorForNonBrokerToBroker(Ref<Node> node,
                                    Ref<DriverTransport> transport,
                                    IpczConnectNodeFlags flags,
                                    std::vector<Ref<Portal>> waiting_portals,
                                    ConnectCallback callback)
      : NodeConnector(std::move(node),
                      std::move(transport),
                      flags,
                      std::move(waiting_portals),
                      std::move(callback)) {}

  ~NodeConnectorForNonBrokerToBroker() override = default;

  // NodeConnector:
  bool Connect() override {
    ABSL_ASSERT(node_->type() == Node::Type::kNormal);
    msg::ConnectFromNonBrokerToBroker connect;
    connect.params().protocol_version = msg::kProtocolVersion;
    connect.params().num_initial_portals =
        checked_cast<uint32_t>(num_portals());
    return IPCZ_RESULT_OK == transport_->Transmit(connect);
  }

  // NodeMessageListener overrides:
  bool OnConnectFromBrokerToNonBroker(
      msg::ConnectFromBrokerToNonBroker& connect) override {
    DVLOG(4) << "New node accepting ConnectFromBrokerToNonBroker with assigned "
             << "name " << connect.params().receiver_name.ToString()
             << " from broker " << connect.params().broker_name.ToString();

    auto new_link =
        NodeLink::Create(node_, LinkSide::kB, connect.params().receiver_name,
                         connect.params().broker_name, Node::Type::kBroker,
                         connect.params().protocol_version, transport_);
    node_->SetAssignedName(connect.params().receiver_name);
    node_->SetBrokerLink(new_link);

    // TODO: Support delegated allocation of shared memory.
    ABSL_ASSERT((flags_ & IPCZ_CONNECT_NODE_TO_ALLOCATION_DELEGATE) == 0);

    AcceptConnection(std::move(new_link), LinkSide::kB,
                     connect.params().num_initial_portals);
    return true;
  }
};

std::pair<Ref<NodeConnector>, IpczResult> CreateConnector(
    Ref<Node> node,
    Ref<DriverTransport> transport,
    IpczConnectNodeFlags flags,
    const std::vector<Ref<Portal>>& initial_portals,
    NodeConnector::ConnectCallback callback) {
  const bool from_broker = node->type() == Node::Type::kBroker;
  const bool to_broker = (flags & IPCZ_CONNECT_NODE_TO_BROKER) != 0;
  const bool inherit_broker = (flags & IPCZ_CONNECT_NODE_INHERIT_BROKER) != 0;
  if (from_broker) {
    if (to_broker) {
      // TODO: Implement broker-to-broker connections.
      ABSL_ASSERT(false);
      return {nullptr, IPCZ_RESULT_INVALID_ARGUMENT};
    }

    return {MakeRefCounted<NodeConnectorForBrokerToNonBroker>(
                std::move(node), std::move(transport), flags, initial_portals,
                std::move(callback)),
            IPCZ_RESULT_OK};
  }

  if (to_broker) {
    return {MakeRefCounted<NodeConnectorForNonBrokerToBroker>(
                std::move(node), std::move(transport), flags, initial_portals,
                std::move(callback)),
            IPCZ_RESULT_OK};
  }

  // TODO: Implement non-broker to non-broker connections (broker sharing.)
  ABSL_ASSERT(!inherit_broker);
  return {nullptr, IPCZ_RESULT_FAILED_PRECONDITION};
}

}  // namespace

// static
IpczResult NodeConnector::ConnectNode(
    Ref<Node> node,
    Ref<DriverTransport> transport,
    IpczConnectNodeFlags flags,
    const std::vector<Ref<Portal>>& initial_portals,
    ConnectCallback callback) {
  const bool from_broker = node->type() == Node::Type::kBroker;
  const bool to_broker = (flags & IPCZ_CONNECT_NODE_TO_BROKER) != 0;
  const bool inherit_broker = (flags & IPCZ_CONNECT_NODE_INHERIT_BROKER) != 0;
  const bool share_broker = (flags & IPCZ_CONNECT_NODE_SHARE_BROKER) != 0;
  Ref<NodeLink> broker_link = node->GetBrokerLink();
  if (share_broker && (from_broker || to_broker || inherit_broker)) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }
  if ((to_broker || from_broker) && (inherit_broker || share_broker)) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  // TODO: Implement broker sharing and inheritance.
  ABSL_ASSERT(!share_broker && !inherit_broker);

  auto [connector, result] =
      CreateConnector(std::move(node), std::move(transport), flags,
                      initial_portals, std::move(callback));
  if (result != IPCZ_RESULT_OK) {
    return result;
  }

  if (!connector->ActivateTransportAndConnect()) {
    // The driver either failed to activate its transport, or failed to transmit
    // a handshake message.
    return IPCZ_RESULT_UNKNOWN;
  }

  return IPCZ_RESULT_OK;
}

NodeConnector::NodeConnector(Ref<Node> node,
                             Ref<DriverTransport> transport,
                             IpczConnectNodeFlags flags,
                             std::vector<Ref<Portal>> waiting_portals,
                             ConnectCallback callback)
    : node_(std::move(node)),
      transport_(std::move(transport)),
      flags_(flags),
      waiting_portals_(std::move(waiting_portals)),
      callback_(std::move(callback)) {}

NodeConnector::~NodeConnector() = default;

void NodeConnector::AcceptConnection(Ref<NodeLink> new_link,
                                     LinkSide link_side,
                                     uint32_t num_remote_portals) {
  node_->AddLink(new_link->remote_node_name(), new_link);
  if (callback_) {
    callback_(new_link);
  }
  EstablishWaitingPortals(std::move(new_link), link_side, num_remote_portals);
  active_self_.reset();
}

void NodeConnector::RejectConnection() {
  if (callback_) {
    callback_(nullptr);
  }
  EstablishWaitingPortals(nullptr, LinkSide::kA, 0);
  transport_->Deactivate();
  active_self_.reset();
}

bool NodeConnector::ActivateTransportAndConnect() {
  active_self_ = WrapRefCounted(this);
  transport_->set_listener(this);
  if (transport_->Activate() != IPCZ_RESULT_OK) {
    RejectConnection();
    return false;
  }

  return Connect();
}

void NodeConnector::EstablishWaitingPortals(Ref<NodeLink> to_link,
                                            LinkSide link_side,
                                            size_t max_valid_portals) {
  ABSL_ASSERT(to_link != nullptr || max_valid_portals == 0);
  const size_t num_valid_portals =
      std::min(max_valid_portals, waiting_portals_.size());
  for (size_t i = 0; i < num_valid_portals; ++i) {
    const Ref<Router> router = waiting_portals_[i]->router();
    router->SetOutwardLink(to_link->AddRemoteRouterLink(
        SublinkId(i), LinkType::kCentral, link_side, router));
  }

  // Elicit immediate peer closure on any surplus portals that were established
  // on this side of the link.
  for (size_t i = num_valid_portals; i < waiting_portals_.size(); ++i) {
    waiting_portals_[i]->router()->AcceptRouteClosureFrom(LinkType::kCentral,
                                                          SequenceNumber(0));
  }
}

void NodeConnector::OnTransportError() {
  RejectConnection();
}

}  // namespace ipcz
