// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/remote_router_link.h"

#include <sstream>
#include <utility>

#include "ipcz/node_link.h"
#include "ipcz/node_messages.h"
#include "ipcz/portal.h"
#include "ipcz/router.h"
#include "util/log.h"

namespace ipcz {

RemoteRouterLink::RemoteRouterLink(Ref<NodeLink> node_link,
                                   SublinkId sublink,
                                   LinkType type,
                                   LinkSide side)
    : node_link_(std::move(node_link)),
      sublink_(sublink),
      type_(type),
      side_(side) {}

RemoteRouterLink::~RemoteRouterLink() = default;

// static
Ref<RemoteRouterLink> RemoteRouterLink::Create(Ref<NodeLink> node_link,
                                               SublinkId sublink,
                                               LinkType type,
                                               LinkSide side) {
  return AdoptRef(
      new RemoteRouterLink(std::move(node_link), sublink, type, side));
}

LinkType RemoteRouterLink::GetType() const {
  return type_;
}

bool RemoteRouterLink::HasLocalPeer(const Router& router) {
  return false;
}

void RemoteRouterLink::AcceptParcel(Parcel& parcel) {
  const absl::Span<Ref<APIObject>> objects = parcel.objects_view();

  msg::AcceptParcel accept;
  accept.params().sublink = sublink_;
  accept.params().sequence_number = parcel.sequence_number();

  // TODO: Support attaching boxes as well as portals.
  const size_t num_portals = objects.size();
  for (Ref<APIObject>& object : objects) {
    ABSL_ASSERT(object->object_type() == APIObject::kPortal);
  }

  // Allocate all the arrays in the message. Note that each allocation may
  // relocate the parcel data in memory, so views into these arrays should not
  // be acquired until all allocations are complete.
  accept.params().parcel_data =
      accept.AllocateArray<uint8_t>(parcel.data_view().size());
  accept.params().handle_types =
      accept.AllocateArray<HandleType>(objects.size());
  accept.params().new_routers =
      accept.AllocateArray<RouterDescriptor>(num_portals);

  const absl::Span<uint8_t> parcel_data =
      accept.GetArrayView<uint8_t>(accept.params().parcel_data);
  const absl::Span<HandleType> handle_types =
      accept.GetArrayView<HandleType>(accept.params().handle_types);
  const absl::Span<RouterDescriptor> new_routers =
      accept.GetArrayView<RouterDescriptor>(accept.params().new_routers);

  if (!parcel_data.empty()) {
    memcpy(parcel_data.data(), parcel.data_view().data(), parcel.data_size());
  }

  // Serialize attached objects. We accumulate the Routers of all attached
  // portals, because we need to reference them again after transmission, with
  // a 1:1 correspondence to the serialized RouterDescriptors.
  absl::InlinedVector<Ref<Router>, 4> routers_to_proxy;
  for (size_t i = 0; i < objects.size(); ++i) {
    APIObject& object = *objects[i];

    // TODO: Support attaching boxes as well as portals.
    ABSL_ASSERT(object.object_type() == APIObject::kPortal);
    handle_types[i] = HandleType::kPortal;

    Ref<Router> router = Portal::FromObject(&object)->router();
    router->SerializeNewRouter(*node_link(), new_routers[i]);
    routers_to_proxy.push_back(std::move(router));
  }

  // TODO: When box attachments are supported, their driver objects will be
  // appended here.
  accept.params().driver_objects = {};

  DVLOG(4) << "Transmitting " << parcel.Describe() << " over " << Describe();

  node_link()->Transmit(accept);

  // Now that the parcel has been transmitted, it's safe to start proxying from
  // any routers whose routes have just been extended to the destination.
  ABSL_ASSERT(routers_to_proxy.size() == new_routers.size());
  for (size_t i = 0; i < routers_to_proxy.size(); ++i) {
    routers_to_proxy[i]->BeginProxyingToNewRouter(*node_link(), new_routers[i]);
  }

  // Finally, a Parcel will normally close all attached objects when destroyed.
  // Since we've successfully transmitted this parcel and all its objects, we
  // prevent that behavior by taking away all its object references.
  for (Ref<APIObject>& object : objects) {
    Ref<APIObject> released_object = std::move(object);
  }
}

void RemoteRouterLink::AcceptRouteClosure(SequenceNumber sequence_length) {
  msg::RouteClosed route_closed;
  route_closed.params().sublink = sublink_;
  route_closed.params().sequence_length = sequence_length;
  node_link()->Transmit(route_closed);
}

void RemoteRouterLink::Deactivate() {
  node_link()->RemoveRemoteRouterLink(sublink_);
}

std::string RemoteRouterLink::Describe() const {
  std::stringstream ss;
  ss << type_.ToString() << " link on "
     << node_link_->local_node_name().ToString() << " to "
     << node_link_->remote_node_name().ToString() << " via sublink "
     << sublink_;
  return ss.str();
}

}  // namespace ipcz
