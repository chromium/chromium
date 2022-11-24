// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/node_connector.h"

#include <algorithm>
#include <utility>

#include "ipcz/driver_memory.h"
#include "ipcz/driver_transport.h"
#include "ipcz/ipcz.h"
#include "ipcz/link_side.h"
#include "ipcz/link_type.h"
#include "ipcz/node_link.h"
#include "ipcz/node_link_memory.h"
#include "ipcz/operation_context.h"
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
                      std::move(callback)),
        link_memory_allocation_(
            NodeLinkMemory::AllocateMemory(node_->driver())) {
    ABSL_ASSERT(link_memory_allocation_.mapping.is_valid());
  }

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
    connect.params().buffer = connect.AppendDriverObject(
        link_memory_allocation_.memory.TakeDriverObject());
    return IPCZ_RESULT_OK == transport_->Transmit(connect);
  }

  // NodeMessageListener overrides:
  bool OnConnectFromNonBrokerToBroker(
      msg::ConnectFromNonBrokerToBroker& connect) override {
    DVLOG(4) << "Accepting ConnectFromNonBrokerToBroker on broker "
             << broker_name_.ToString() << " from new node "
             << new_remote_node_name_.ToString();

    Ref<NodeLink> link = NodeLink::CreateActive(
        node_, LinkSide::kA, broker_name_, new_remote_node_name_,
        Node::Type::kNormal, connect.params().protocol_version, transport_,
        NodeLinkMemory::Create(node_,
                               std::move(link_memory_allocation_.mapping)));
    AcceptConnection({.link = link}, connect.params().num_initial_portals);
    return true;
  }

 private:
  const NodeName broker_name_{node_->GetAssignedName()};
  const NodeName new_remote_node_name_{node_->GenerateRandomName()};
  DriverMemoryWithMapping link_memory_allocation_;
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

    DriverMemoryMapping mapping =
        DriverMemory(connect.TakeDriverObject(connect.params().buffer)).Map();
    if (!mapping.is_valid()) {
      return false;
    }

    auto new_link = NodeLink::CreateActive(
        node_, LinkSide::kB, connect.params().receiver_name,
        connect.params().broker_name, Node::Type::kBroker,
        connect.params().protocol_version, transport_,
        NodeLinkMemory::Create(node_, std::move(mapping)));
    node_->SetAssignedName(connect.params().receiver_name);
    if ((flags_ & IPCZ_CONNECT_NODE_TO_ALLOCATION_DELEGATE) != 0) {
      node_->SetAllocationDelegate(new_link);
    }

    AcceptConnection({.link = new_link, .broker = new_link},
                     connect.params().num_initial_portals);
    return true;
  }
};

// Unlike other NodeConnectors, this one doesn't activate its transport or
// listen for any messages. Instead it uses an existing broker link to pass the
// transport along to a broker and wait for a reply to confirm either
// acceptance or rejection of the referral.
class NodeConnectorForReferrer : public NodeConnector {
 public:
  NodeConnectorForReferrer(Ref<Node> node,
                           Ref<DriverTransport> transport,
                           IpczConnectNodeFlags flags,
                           std::vector<Ref<Portal>> waiting_portals,
                           Ref<NodeLink> broker_link,
                           ConnectCallback callback)
      : NodeConnector(std::move(node),
                      /*transport=*/nullptr,
                      flags,
                      std::move(waiting_portals),
                      std::move(callback)),
        transport_for_broker_(std::move(transport)),
        broker_link_(std::move(broker_link)) {}

  ~NodeConnectorForReferrer() override = default;

  // NodeConnector:
  bool Connect() override {
    ABSL_ASSERT(node_->type() == Node::Type::kNormal);
    if (!broker_link_) {
      // If there's no broker link yet, wait for one.
      node_->WaitForBrokerLinkAsync(
          [connector = WrapRefCounted(this)](Ref<NodeLink> broker_link) {
            connector->broker_link_ = std::move(broker_link);
            connector->Connect();
          });
      return true;
    }

    broker_link_->ReferNonBroker(
        std::move(transport_for_broker_), checked_cast<uint32_t>(num_portals()),
        [connector = WrapRefCounted(this), broker = broker_link_](
            Ref<NodeLink> link_to_referred_node,
            uint32_t remote_num_initial_portals) {
          if (link_to_referred_node) {
            connector->AcceptConnection(
                {.link = link_to_referred_node, .broker = broker},
                remote_num_initial_portals);
          } else {
            connector->RejectConnection();
          }
        });
    return true;
  }

 private:
  Ref<DriverTransport> transport_for_broker_;
  Ref<NodeLink> broker_link_;
};

// A NodeConnector used by a referred node to await acceptance by the broker.
class NodeConnectorForReferredNonBroker : public NodeConnector {
 public:
  NodeConnectorForReferredNonBroker(Ref<Node> node,
                                    Ref<DriverTransport> transport,
                                    IpczConnectNodeFlags flags,
                                    std::vector<Ref<Portal>> waiting_portals,
                                    ConnectCallback callback)
      : NodeConnector(std::move(node),
                      std::move(transport),
                      flags,
                      std::move(waiting_portals),
                      std::move(callback)) {}

  ~NodeConnectorForReferredNonBroker() override = default;

  // NodeConnector:
  bool Connect() override {
    ABSL_ASSERT(node_->type() == Node::Type::kNormal);
    msg::ConnectToReferredBroker connect;
    connect.params().protocol_version = msg::kProtocolVersion;
    connect.params().num_initial_portals =
        checked_cast<uint32_t>(num_portals());
    return IPCZ_RESULT_OK == transport_->Transmit(connect);
  }

  // NodeMessageListener overrides:
  bool OnConnectToReferredNonBroker(
      msg::ConnectToReferredNonBroker& connect) override {
    DVLOG(4) << "New node accepting ConnectToReferredNonBroker with assigned "
             << "name " << connect.params().name.ToString() << " from broker "
             << connect.params().broker_name.ToString() << " as referred by "
             << connect.params().referrer_name.ToString();

    DriverMemoryMapping broker_mapping =
        DriverMemory(
            connect.TakeDriverObject(connect.params().broker_link_buffer))
            .Map();
    DriverMemoryMapping referrer_mapping =
        DriverMemory(
            connect.TakeDriverObject(connect.params().referrer_link_buffer))
            .Map();
    Ref<DriverTransport> referrer_transport = MakeRefCounted<DriverTransport>(
        connect.TakeDriverObject(connect.params().referrer_link_transport));
    if (!broker_mapping.is_valid() || !referrer_mapping.is_valid() ||
        !referrer_transport->driver_object().is_valid()) {
      return false;
    }

    // Ensure this NodeConnector stays alive until this method returns.
    // Otherwise the last reference may be dropped when the new NodeLink takes
    // over listening on `transport_`.
    Ref<NodeConnector> self(this);
    const uint32_t broker_protocol_version = std::min(
        connect.params().broker_protocol_version, msg::kProtocolVersion);
    auto broker_link = NodeLink::CreateActive(
        node_, LinkSide::kB, connect.params().name,
        connect.params().broker_name, Node::Type::kBroker,
        broker_protocol_version, transport_,
        NodeLinkMemory::Create(node_, std::move(broker_mapping)));
    node_->SetAssignedName(connect.params().name);
    if ((flags_ & IPCZ_CONNECT_NODE_TO_ALLOCATION_DELEGATE) != 0) {
      node_->SetAllocationDelegate(broker_link);
    }
    node_->AddConnection(connect.params().broker_name,
                         {
                             .link = broker_link,
                             .broker = broker_link,
                         });

    const uint32_t referrer_protocol_version = std::min(
        connect.params().referrer_protocol_version, msg::kProtocolVersion);
    auto referrer_link = NodeLink::CreateInactive(
        node_, LinkSide::kB, connect.params().name,
        connect.params().referrer_name, Node::Type::kNormal,
        referrer_protocol_version, std::move(referrer_transport),
        NodeLinkMemory::Create(node_, std::move(referrer_mapping)));

    AcceptConnection({.link = referrer_link, .broker = broker_link},
                     connect.params().num_initial_portals);
    referrer_link->Activate();
    return true;
  }
};

// The NodeConnector used by a broker to await a handshake from the referred
// node before responding to both that node and the referrer.
class NodeConnectorForBrokerReferral : public NodeConnector {
 public:
  NodeConnectorForBrokerReferral(Ref<Node> node,
                                 uint64_t referral_id,
                                 uint32_t num_initial_portals,
                                 Ref<NodeLink> referrer,
                                 Ref<DriverTransport> transport)
      : NodeConnector(std::move(node),
                      std::move(transport),
                      IPCZ_NO_FLAGS,
                      /*waiting_portals=*/{},
                      /*callback=*/nullptr),
        referral_id_(referral_id),
        num_initial_portals_(num_initial_portals),
        referrer_(std::move(referrer)) {
    ABSL_ASSERT(link_memory_.mapping.is_valid());
    ABSL_ASSERT(client_link_memory_.mapping.is_valid());
  }

  ~NodeConnectorForBrokerReferral() override = default;

  // NodeConnector:
  bool Connect() override { return true; }

  // NodeMessageListener overrides:
  bool OnConnectToReferredBroker(
      msg::ConnectToReferredBroker& connect_to_broker) override {
    DVLOG(4) << "Accepting ConnectToReferredBroker on broker "
             << broker_name_.ToString() << " from new referred node "
             << referred_node_name_.ToString();

    // Ensure this NodeConnector stays alive until this method returns.
    // Otherwise the last reference may be dropped when the new NodeLink below
    // takes over listening on `transport_`.
    Ref<NodeConnector> self(this);

    // First, accept the new non-broker client on this broker node. There are
    // no initial portals on this link, as this link was not established
    // directly by the application. Note that this takes over listsening on
    // `transport_`
    const uint32_t protocol_version = std::min(
        connect_to_broker.params().protocol_version, msg::kProtocolVersion);
    Ref<NodeLink> link_to_referree = NodeLink::CreateActive(
        node_, LinkSide::kA, broker_name_, referred_node_name_,
        Node::Type::kNormal, protocol_version, transport_,
        NodeLinkMemory::Create(node_, std::move(link_memory_.mapping)));
    AcceptConnection({.link = link_to_referree}, /*num_remote_portals=*/0);

    // Now we can create a new link to introduce both clients -- the referrer
    // and the referree -- to each other.
    auto [referrer, referree] = DriverTransport::CreatePair(
        node_->driver(), referrer_->transport().get(), transport_.get());

    // Give the referred node a reply with sufficient details for it to
    // establish links to both this broker and the referrer simultaneously.
    //
    // SUBTLE: It's important that this message is sent before the message to
    // the referrer below. Otherwise the referrer might begin relaying messages
    // through the broker to the referree before this handshake is sent to the
    // referree, which would be bad.
    msg::ConnectToReferredNonBroker connect;
    connect.params().name = referred_node_name_;
    connect.params().broker_name = broker_name_;
    connect.params().referrer_name = referrer_->remote_node_name();
    connect.params().broker_protocol_version = protocol_version;
    connect.params().referrer_protocol_version =
        referrer_->remote_protocol_version();
    connect.params().num_initial_portals = num_initial_portals_;
    connect.params().broker_link_buffer =
        connect.AppendDriverObject(link_memory_.memory.TakeDriverObject());
    connect.params().referrer_link_transport =
        connect.AppendDriverObject(referree->TakeDriverObject());
    connect.params().referrer_link_buffer = connect.AppendDriverObject(
        client_link_memory_.memory.Clone().TakeDriverObject());
    link_to_referree->Transmit(connect);

    // Finally, give the referrer a repy which includes details of its new link
    // to the referred node.
    msg::NonBrokerReferralAccepted accepted;
    accepted.params().referral_id = referral_id_;
    accepted.params().protocol_version =
        connect_to_broker.params().protocol_version;
    accepted.params().num_initial_portals =
        connect_to_broker.params().num_initial_portals;
    accepted.params().name = referred_node_name_;
    accepted.params().transport =
        accepted.AppendDriverObject(referrer->TakeDriverObject());
    accepted.params().buffer = accepted.AppendDriverObject(
        client_link_memory_.memory.TakeDriverObject());
    referrer_->Transmit(accepted);
    return true;
  }

  void OnTransportError() override {
    msg::NonBrokerReferralRejected rejected;
    rejected.params().referral_id = referral_id_;
    referrer_->Transmit(rejected);

    NodeConnector::OnTransportError();
  }

 private:
  const uint64_t referral_id_;
  const uint32_t num_initial_portals_;
  const Ref<NodeLink> referrer_;
  const NodeName broker_name_{node_->GetAssignedName()};
  const NodeName referred_node_name_{node_->GenerateRandomName()};
  DriverMemoryWithMapping link_memory_{
      NodeLinkMemory::AllocateMemory(node_->driver())};
  DriverMemoryWithMapping client_link_memory_{
      NodeLinkMemory::AllocateMemory(node_->driver())};
};

class NodeConnectorForBrokerToBroker : public NodeConnector {
 public:
  NodeConnectorForBrokerToBroker(Ref<Node> node,
                                 Ref<DriverTransport> transport,
                                 IpczConnectNodeFlags flags,
                                 std::vector<Ref<Portal>> waiting_portals,
                                 ConnectCallback callback)
      : NodeConnector(std::move(node),
                      std::move(transport),
                      flags,
                      std::move(waiting_portals),
                      std::move(callback)),
        link_memory_allocation_(
            NodeLinkMemory::AllocateMemory(node_->driver())) {
    ABSL_ASSERT(link_memory_allocation_.mapping.is_valid());
  }

  ~NodeConnectorForBrokerToBroker() override = default;

  // NodeConnector:
  bool Connect() override {
    DVLOG(4) << "Sending direct ConnectFromBrokerToBroker from broker "
             << local_name_.ToString() << " with " << num_portals()
             << " initial portals";

    ABSL_ASSERT(node_->type() == Node::Type::kBroker);
    msg::ConnectFromBrokerToBroker connect;
    connect.params().name = local_name_;
    connect.params().protocol_version = msg::kProtocolVersion;
    connect.params().num_initial_portals =
        checked_cast<uint32_t>(num_portals());
    connect.params().buffer = connect.AppendDriverObject(
        link_memory_allocation_.memory.TakeDriverObject());
    return IPCZ_RESULT_OK == transport_->Transmit(connect);
  }

  // NodeMessageListener overrides:
  bool OnConnectFromBrokerToBroker(
      msg::ConnectFromBrokerToBroker& connect) override {
    const NodeName& remote_name = connect.params().name;
    DVLOG(4) << "Accepting ConnectFromBrokerToBroker on broker "
             << local_name_.ToString() << " from other broker "
             << remote_name.ToString();

    const LinkSide this_side =
        remote_name < local_name_ ? LinkSide::kA : LinkSide::kB;
    DriverMemory their_memory(
        connect.TakeDriverObject(connect.params().buffer));
    DriverMemoryMapping primary_buffer_mapping =
        this_side.is_side_a() ? std::move(link_memory_allocation_.mapping)
                              : their_memory.Map();
    if (!primary_buffer_mapping.is_valid()) {
      return false;
    }

    Ref<NodeLink> link = NodeLink::CreateActive(
        node_, this_side, local_name_, remote_name, Node::Type::kBroker,
        connect.params().protocol_version, transport_,
        NodeLinkMemory::Create(node_, std::move(primary_buffer_mapping)));
    AcceptConnection({.link = link, .broker = link},
                     connect.params().num_initial_portals);
    return true;
  }

 private:
  const NodeName local_name_{node_->GetAssignedName()};
  DriverMemoryWithMapping link_memory_allocation_;
};

std::pair<Ref<NodeConnector>, IpczResult> CreateConnector(
    Ref<Node> node,
    Ref<DriverTransport> transport,
    IpczConnectNodeFlags flags,
    const std::vector<Ref<Portal>>& initial_portals,
    Ref<NodeLink> broker_link,
    NodeConnector::ConnectCallback callback) {
  const bool from_broker = node->type() == Node::Type::kBroker;
  const bool to_broker = (flags & IPCZ_CONNECT_NODE_TO_BROKER) != 0;
  const bool share_broker = (flags & IPCZ_CONNECT_NODE_SHARE_BROKER) != 0;
  const bool inherit_broker = (flags & IPCZ_CONNECT_NODE_INHERIT_BROKER) != 0;
  if (from_broker) {
    if (to_broker) {
      return {MakeRefCounted<NodeConnectorForBrokerToBroker>(
                  std::move(node), std::move(transport), flags, initial_portals,
                  std::move(callback)),
              IPCZ_RESULT_OK};
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

  if (share_broker) {
    return {MakeRefCounted<NodeConnectorForReferrer>(
                std::move(node), std::move(transport), flags, initial_portals,
                std::move(broker_link), std::move(callback)),
            IPCZ_RESULT_OK};
  }

  if (inherit_broker) {
    return {MakeRefCounted<NodeConnectorForReferredNonBroker>(
                std::move(node), std::move(transport), flags, initial_portals,
                std::move(callback)),
            IPCZ_RESULT_OK};
  }

  return {nullptr, IPCZ_RESULT_INVALID_ARGUMENT};
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

  auto [connector, result] = CreateConnector(
      std::move(node), std::move(transport), flags, initial_portals,
      std::move(broker_link), std::move(callback));
  if (result != IPCZ_RESULT_OK) {
    return result;
  }

  if (!share_broker && !connector->ActivateTransport()) {
    // Note that when referring another node to our own broker, we don't
    // activate the transport, since the transport will be passed to the broker.
    // See NodeConnectorForReferrer.
    return IPCZ_RESULT_UNKNOWN;
  }

  if (!connector->Connect()) {
    return IPCZ_RESULT_UNKNOWN;
  }

  return IPCZ_RESULT_OK;
}

// static
bool NodeConnector::HandleNonBrokerReferral(
    Ref<Node> node,
    uint64_t referral_id,
    uint32_t num_initial_portals,
    Ref<NodeLink> referrer,
    Ref<DriverTransport> transport_to_referred_node) {
  ABSL_ASSERT(node->type() == Node::Type::kBroker);
  auto connector = MakeRefCounted<NodeConnectorForBrokerReferral>(
      std::move(node), referral_id, num_initial_portals, std::move(referrer),
      std::move(transport_to_referred_node));

  // The connector effectively owns itself and lives only until its transport is
  // disconnected or it receives a greeting from the referred node.
  return connector->ActivateTransport();
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

void NodeConnector::AcceptConnection(Node::Connection connection,
                                     uint32_t num_remote_portals) {
  node_->AddConnection(connection.link->remote_node_name(), connection);
  if (callback_) {
    callback_(connection.link);
  }
  EstablishWaitingPortals(connection.link, num_remote_portals);
}

void NodeConnector::RejectConnection() {
  if (callback_) {
    callback_(nullptr);
  }
  EstablishWaitingPortals(nullptr, 0);
  if (transport_) {
    transport_->Deactivate();
  }
}

bool NodeConnector::ActivateTransport() {
  transport_->set_listener(WrapRefCounted(this));
  if (transport_->Activate() != IPCZ_RESULT_OK) {
    RejectConnection();
    return false;
  }
  return true;
}

void NodeConnector::EstablishWaitingPortals(Ref<NodeLink> to_link,
                                            size_t max_valid_portals) {
  // All paths to this function come from a transport notification.
  const OperationContext context{OperationContext::kTransportNotification};

  ABSL_ASSERT(to_link != nullptr || max_valid_portals == 0);
  const size_t num_valid_portals =
      std::min(max_valid_portals, waiting_portals_.size());
  for (size_t i = 0; i < num_valid_portals; ++i) {
    const Ref<Router> router = waiting_portals_[i]->router();
    Ref<RouterLink> link = to_link->AddRemoteRouterLink(
        context, SublinkId(i), to_link->memory().GetInitialRouterLinkState(i),
        LinkType::kCentral, to_link->link_side(), router);
    if (link) {
      router->SetOutwardLink(context, std::move(link));
    } else {
      router->AcceptRouteDisconnectedFrom(context, LinkType::kCentral);
    }
  }

  // Elicit immediate peer closure on any surplus portals that were established
  // on this side of the link.
  for (size_t i = num_valid_portals; i < waiting_portals_.size(); ++i) {
    waiting_portals_[i]->router()->AcceptRouteClosureFrom(
        context, LinkType::kCentral, SequenceNumber(0));
  }
}

void NodeConnector::OnTransportError() {
  RejectConnection();
}

}  // namespace ipcz
