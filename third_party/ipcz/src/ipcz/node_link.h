// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_IPCZ_NODE_LINK_H_
#define IPCZ_SRC_IPCZ_NODE_LINK_H_

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "ipcz/driver_memory.h"
#include "ipcz/driver_transport.h"
#include "ipcz/link_side.h"
#include "ipcz/link_type.h"
#include "ipcz/node.h"
#include "ipcz/node_messages.h"
#include "ipcz/node_name.h"
#include "ipcz/sequence_number.h"
#include "ipcz/sublink_id.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/synchronization/mutex.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/span.h"
#include "util/ref_counted.h"

namespace ipcz {

class Message;
class RemoteRouterLink;
class Router;

// A NodeLink instance encapsulates all communication from its owning node to
// exactly one other remote node in the sytem. Each NodeLink manages a
// DriverTransport for general-purpose I/O to and from the remote node.
//
// NodeLinks may also allocate an arbitrary number of sublinks which are used
// to multiplex the link and facilitate point-to-point communication between
// specific Router instances on either end.
class NodeLink : public RefCounted, private msg::NodeMessageListener {
 public:
  struct Sublink {
    Sublink(Ref<RemoteRouterLink> link, Ref<Router> receiver);
    Sublink(Sublink&&);
    Sublink(const Sublink&);
    Sublink& operator=(Sublink&&);
    Sublink& operator=(const Sublink&);
    ~Sublink();

    Ref<RemoteRouterLink> router_link;
    Ref<Router> receiver;
  };

  static Ref<NodeLink> Create(Ref<Node> node,
                              LinkSide link_side,
                              const NodeName& local_node_name,
                              const NodeName& remote_node_name,
                              Node::Type remote_node_type,
                              uint32_t remote_protocol_version,
                              Ref<DriverTransport> transport);

  const Ref<Node>& node() const { return node_; }
  LinkSide link_side() const { return link_side_; }
  const NodeName& local_node_name() const { return local_node_name_; }
  const NodeName& remote_node_name() const { return remote_node_name_; }
  Node::Type remote_node_type() const { return remote_node_type_; }
  uint32_t remote_protocol_version() const { return remote_protocol_version_; }
  const Ref<DriverTransport>& transport() const { return transport_; }

  // Binds `sublink` on this NodeLink to the given `router`. `link_side`
  // specifies which side of the link this end identifies as (A or B), and
  // `type` specifies the type of link this is, from the perspective of
  // `router`.
  Ref<RemoteRouterLink> AddRemoteRouterLink(SublinkId sublink,
                                            LinkType type,
                                            LinkSide side,
                                            Ref<Router> router);

  // Removes the route specified by `sublink`. Once removed, any messages
  // received for that sublink are ignored.
  void RemoveRemoteRouterLink(SublinkId sublink);

  // Retrieves the Router and RemoteRouterLink currently bound to `sublink`
  // on this NodeLink.
  absl::optional<Sublink> GetSublink(SublinkId sublink);

  // Retrieves only the Router currently bound to `sublink` on this NodeLink.
  Ref<Router> GetRouter(SublinkId sublink);

  // Permanently deactivates this NodeLink. Once this call returns the NodeLink
  // will no longer receive transport messages. It may still be used to transmit
  // outgoing messages, but it cannot be reactivated. Transmissions over a
  // deactivated transport may or may not guarantee delivery to the peer
  // transport, as this is left to driver's discretion.
  void Deactivate();

  // Finalizes serialization of DriverObjects within `message` and transmits it
  // to the NodeLink's peer, either over the DriverTransport or through shared
  // memory.
  void Transmit(Message& message);

 private:
  NodeLink(Ref<Node> node,
           LinkSide link_side,
           const NodeName& local_node_name,
           const NodeName& remote_node_name,
           Node::Type remote_node_type,
           uint32_t remote_protocol_version,
           Ref<DriverTransport> transport);
  ~NodeLink() override;

  SequenceNumber GenerateOutgoingSequenceNumber();

  // NodeMessageListener overrides:
  bool OnRouteClosed(msg::RouteClosed& route_closed) override;
  void OnTransportError() override;

  const Ref<Node> node_;
  const LinkSide link_side_;
  const NodeName local_node_name_;
  const NodeName remote_node_name_;
  const Node::Type remote_node_type_;
  const uint32_t remote_protocol_version_;
  const Ref<DriverTransport> transport_;

  absl::Mutex mutex_;
  bool active_ ABSL_GUARDED_BY(mutex_) = true;

  // Messages transmitted from this NodeLink may traverse either the driver
  // transport OR a shared memory queue. Messages transmitted from the same
  // thread need to be processed in the same order by the receiving node. This
  // is used to generate a sequence number for every message so they can be
  // reordered on the receiving end.
  std::atomic<uint64_t> next_outgoing_sequence_number_generator_{0};

  using SublinkMap = absl::flat_hash_map<SublinkId, Sublink>;
  SublinkMap sublinks_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace ipcz

#endif  // IPCZ_SRC_IPCZ_NODE_LINK_H_
