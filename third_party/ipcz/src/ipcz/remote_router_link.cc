// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/remote_router_link.h"

#include <algorithm>
#include <sstream>
#include <utility>
#include <vector>

#include "ipcz/application_object.h"
#include "ipcz/box.h"
#include "ipcz/node_link.h"
#include "ipcz/node_link_memory.h"
#include "ipcz/node_messages.h"
#include "ipcz/parcel.h"
#include "ipcz/router.h"
#include "util/log.h"
#include "util/safe_math.h"

namespace ipcz {

RemoteRouterLink::RemoteRouterLink(Ref<NodeLink> node_link,
                                   SublinkId sublink,
                                   FragmentRef<RouterLinkState> link_state,
                                   LinkType type,
                                   LinkSide side)
    : node_link_(std::move(node_link)),
      sublink_(sublink),
      type_(type),
      side_(side) {
  // Central links must be constructed with a valid RouterLinkState fragment.
  // Other links must not.
  ABSL_ASSERT(type.is_central() == !link_state.is_null());

  if (type.is_central()) {
    SetLinkState(std::move(link_state));
  }
}

RemoteRouterLink::~RemoteRouterLink() = default;

// static
Ref<RemoteRouterLink> RemoteRouterLink::Create(
    Ref<NodeLink> node_link,
    SublinkId sublink,
    FragmentRef<RouterLinkState> link_state,
    LinkType type,
    LinkSide side) {
  return AdoptRef(new RemoteRouterLink(std::move(node_link), sublink,
                                       std::move(link_state), type, side));
}

void RemoteRouterLink::SetLinkState(FragmentRef<RouterLinkState> state) {
  ABSL_ASSERT(type_.is_central());
  ABSL_ASSERT(!state.is_null());

  if (state.is_pending()) {
    // Wait for the fragment's buffer to be mapped locally.
    Ref<NodeLinkMemory> memory = WrapRefCounted(&node_link()->memory());
    FragmentDescriptor descriptor = state.fragment().descriptor();
    memory->WaitForBufferAsync(
        descriptor.buffer_id(),
        [self = WrapRefCounted(this), memory, descriptor] {
          self->SetLinkState(memory->AdoptFragmentRef<RouterLinkState>(
              memory->GetFragment(descriptor)));
        });
    return;
  }

  ABSL_ASSERT(state.is_addressable());

  // SetLinkState() must be called with an addressable fragment only once.
  ABSL_ASSERT(link_state_.load(std::memory_order_acquire) == nullptr);

  link_state_fragment_ = std::move(state);

  std::vector<std::function<void()>> callbacks;
  {
    absl::MutexLock lock(&mutex_);
    // This store-release is balanced by a load-acquire in GetLinkState().
    link_state_.store(link_state_fragment_.get(), std::memory_order_release);
    link_state_callbacks_.swap(callbacks);
  }

  for (auto& callback : callbacks) {
    callback();
  }

  // If this side of the link was already marked stable before the
  // RouterLinkState was available, `side_is_stable_` will be true. In that
  // case, set the stable bit in RouterLinkState immediately. This may unblock
  // some routing work. The acquire here is balanced by a release in
  // MarkSideStable().
  if (side_is_stable_.load(std::memory_order_acquire)) {
    MarkSideStable();
  }
  if (Ref<Router> router = node_link()->GetRouter(sublink_)) {
    router->Flush(Router::kForceProxyBypassAttempt);
  }
}

LinkType RemoteRouterLink::GetType() const {
  return type_;
}

RouterLinkState* RemoteRouterLink::GetLinkState() const {
  return link_state_.load(std::memory_order_acquire);
}

void RemoteRouterLink::WaitForLinkStateAsync(std::function<void()> callback) {
  {
    absl::MutexLock lock(&mutex_);
    if (!link_state_.load(std::memory_order_relaxed)) {
      link_state_callbacks_.push_back(std::move(callback));
      return;
    }
  }

  callback();
}

Ref<Router> RemoteRouterLink::GetLocalPeer() {
  return nullptr;
}

RemoteRouterLink* RemoteRouterLink::AsRemoteRouterLink() {
  return this;
}

void RemoteRouterLink::AllocateParcelData(size_t num_bytes,
                                          bool allow_partial,
                                          Parcel& parcel) {
  parcel.AllocateData(num_bytes, allow_partial, &node_link()->memory());
}

void RemoteRouterLink::AcceptParcel(std::unique_ptr<Parcel> parcel) {
  const absl::Span<Ref<APIObject>> objects = parcel->objects_view();

  msg::AcceptParcel accept;
  accept.v0()->sublink = sublink_;
  accept.v0()->sequence_number = parcel->sequence_number();
  accept.v0()->padding = 0;

  size_t num_portals = 0;
  absl::InlinedVector<DriverObject, 2> driver_objects;
  std::vector<Ref<ParcelWrapper>> subparcels;
  bool must_relay_driver_objects = false;
  for (Ref<APIObject>& object : objects) {
    switch (object->object_type()) {
      case APIObject::kPortal:
        ++num_portals;
        break;

      case APIObject::kBox: {
        Box* box = Box::FromObject(object.get());
        ABSL_ASSERT(box);

        switch (box->type()) {
          case Box::Type::kDriverObject: {
            if (!box->driver_object().CanTransmitOn(
                    *node_link()->transport())) {
              must_relay_driver_objects = true;
            }
            driver_objects.push_back(std::move(box->driver_object()));
            break;
          }

          case Box::Type::kApplicationObject: {
            // Application objects must be serialized into subparcels.
            ApplicationObject application_object =
                std::move(box->application_object());
            if (!application_object.IsSerializable()) {
              DLOG(FATAL) << "Cannot transmit unserializable object";
              return;
            }
            subparcels.push_back(application_object.Serialize(*node_link()));
            break;
          }

          case Box::Type::kSubparcel:
            subparcels.push_back(std::move(box->subparcel()));
            break;

          default:
            DLOG(FATAL) << "Attempted to transmit an invalid object";
        }
        break;
      }

      default:
        break;
    }
  }

  // Subparcels cannot contain other subparcels.
  ABSL_ASSERT(parcel->subparcel_index() == 0 || subparcels.empty());

  // Receivers will reject parcels which contain more than this maximum number
  // of subparcels.
  ABSL_ASSERT(subparcels.size() < Parcel::kMaxSubparcelsPerParcel);

  uint32_t num_subparcels;
  if (parcel->subparcel_index() == 0) {
    // The total subparcel count includes this (the main) parcel.
    num_subparcels = checked_cast<uint32_t>(subparcels.size()) + 1;

    // Send the other subparcels separately before sending this main one. All
    // will be collected on the receiving end and reconstituted into a single
    // parcel.
    for (size_t i = 1; i < num_subparcels; ++i) {
      std::unique_ptr<Parcel> subparcel = subparcels[i - 1]->TakeParcel();
      subparcel->set_sequence_number(parcel->sequence_number());
      subparcel->set_num_subparcels(num_subparcels);
      subparcel->set_subparcel_index(i);
      AcceptParcel(std::move(subparcel));
    }
  } else {
    // This is not the main parcel, so the number of subparcels has already been
    // set correctly by the main parcel.
    num_subparcels = parcel->num_subparcels();
  }

  accept.v0()->num_subparcels = num_subparcels;
  accept.v0()->subparcel_index = parcel->subparcel_index();

  // If driver objects will require relaying through the broker, then the parcel
  // must be split into two separate messages: one for the driver objects (which
  // will be relayed), and one for the rest of the message (which will transmit
  // directly).
  //
  // This ensures that many side effects of message receipt are well-ordered
  // with other transmissions on the same link from the same thread. Namely,
  // since a thread may send a message which introduces a new remote Router on a
  // new sublink, followed immediately by a message which targets that Router,
  // it is critical that both messages arrive in the order they were sent. If
  // one of the messages is relayed while the other is not, ordering could not
  // be guaranteed.
  const bool must_split_parcel = must_relay_driver_objects;

  // Allocate all the arrays in the message. Note that each allocation may
  // relocate the parcel data in memory, so views into these arrays should not
  // be acquired until all allocations are complete.
  if (!parcel->has_data_fragment() ||
      parcel->data_fragment_memory() != &node_link()->memory()) {
    // Only inline parcel data within the message when we don't have a separate
    // data fragment allocated already, or if the allocated fragment is on the
    // wrong link. The latter case is possible if the transmitting Router
    // switched links since the Parcel's data was allocated.
    accept.v0()->parcel_data =
        accept.AllocateArray<uint8_t>(parcel->data_size());
  } else {
    // The data for this parcel already exists in this link's memory, so we only
    // stash a reference to it in the message. This relinquishes ownership of
    // the fragment, effectively passing it to the recipient.
    accept.v0()->parcel_fragment = parcel->data_fragment().descriptor();
    parcel->ReleaseDataFragment();
  }
  accept.v0()->handle_types = accept.AllocateArray<HandleType>(objects.size());
  accept.v0()->new_routers =
      accept.AllocateArray<RouterDescriptor>(num_portals);

  const absl::Span<uint8_t> inline_parcel_data =
      accept.GetArrayView<uint8_t>(accept.v0()->parcel_data);
  const absl::Span<HandleType> handle_types =
      accept.GetArrayView<HandleType>(accept.v0()->handle_types);
  const absl::Span<RouterDescriptor> new_routers =
      accept.GetArrayView<RouterDescriptor>(accept.v0()->new_routers);

  if (!inline_parcel_data.empty()) {
    memcpy(inline_parcel_data.data(), parcel->data_view().data(),
           parcel->data_size());
  }

  // Serialize attached objects. We accumulate the Routers of all attached
  // portals, because we need to reference them again after transmission, with
  // a 1:1 correspondence to the serialized RouterDescriptors.
  absl::InlinedVector<Ref<Router>, 4> routers_to_proxy(num_portals);
  absl::InlinedVector<RouterDescriptor, 4> descriptors(num_portals);

  // Explicitly zero the descriptor memory since there may be padding bits
  // within and we'll be copying the full contents into message data below.
  memset(descriptors.data(), 0, descriptors.size() * sizeof(descriptors[0]));

  size_t portal_index = 0;
  for (size_t i = 0; i < objects.size(); ++i) {
    APIObject& object = *objects[i];

    switch (object.object_type()) {
      case APIObject::kPortal: {
        handle_types[i] = HandleType::kPortal;

        Ref<Router> router = WrapRefCounted(Router::FromObject(&object));
        ABSL_ASSERT(portal_index < num_portals);
        router->SerializeNewRouter(*node_link(), descriptors[portal_index]);
        routers_to_proxy[portal_index] = std::move(router);
        ++portal_index;
        break;
      }

      case APIObject::kBox:
        switch (Box::FromObject(&object)->type()) {
          case Box::Type::kDriverObject:
            handle_types[i] = must_split_parcel
                                  ? HandleType::kRelayedBoxedDriverObject
                                  : HandleType::kBoxedDriverObject;
            break;

          // Subparcels and application objects both serialized as subparcels.
          case Box::Type::kApplicationObject:
          case Box::Type::kSubparcel:
            handle_types[i] = HandleType::kBoxedSubparcel;
            break;

          default:
            DLOG(FATAL) << "Attempted to transmit an invalid object.";
        }
        break;

      default:
        DLOG(FATAL) << "Attempted to transmit an invalid object.";
        break;
    }
  }

  // Copy all the serialized router descriptors into the message. Our local
  // copy will supply inputs for BeginProxyingToNewRouter() calls below.
  if (!descriptors.empty()) {
    memcpy(new_routers.data(), descriptors.data(),
           new_routers.size() * sizeof(new_routers[0]));
  }

  if (must_split_parcel) {
    msg::AcceptParcelDriverObjects accept_objects;
    accept_objects.v0()->sublink = sublink_;
    accept_objects.v0()->sequence_number = parcel->sequence_number();
    accept_objects.v0()->driver_objects =
        accept_objects.AppendDriverObjects(absl::MakeSpan(driver_objects));

    DVLOG(4) << "Transmitting objects for " << parcel->Describe() << " over "
             << Describe();
    node_link()->Transmit(accept_objects);
  } else {
    accept.v0()->driver_objects =
        accept.AppendDriverObjects(absl::MakeSpan(driver_objects));
  }

  DVLOG(4) << "Transmitting " << parcel->Describe() << " over " << Describe();

  node_link()->Transmit(accept);

  // Now that the parcel has been transmitted, it's safe to start proxying from
  // any routers whose routes have just been extended to the destination.
  ABSL_ASSERT(routers_to_proxy.size() == descriptors.size());
  for (size_t i = 0; i < routers_to_proxy.size(); ++i) {
    routers_to_proxy[i]->BeginProxyingToNewRouter(*node_link(), descriptors[i]);
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
  route_closed.v0()->sublink = sublink_;
  route_closed.v0()->sequence_length = sequence_length;
  node_link()->Transmit(route_closed);
}

void RemoteRouterLink::AcceptRouteDisconnected() {
  msg::RouteDisconnected route_disconnected;
  route_disconnected.v0()->sublink = sublink_;
  node_link()->Transmit(route_disconnected);
}

void RemoteRouterLink::MarkSideStable() {
  side_is_stable_.store(true, std::memory_order_release);
  if (RouterLinkState* state = GetLinkState()) {
    state->SetSideStable(side_);
  }
}

bool RemoteRouterLink::TryLockForBypass(const NodeName& bypass_request_source) {
  RouterLinkState* state = GetLinkState();
  if (!state || !state->TryLock(side_)) {
    return false;
  }

  state->allowed_bypass_request_source.StoreRelease(bypass_request_source);
  return true;
}

bool RemoteRouterLink::TryLockForClosure() {
  RouterLinkState* state = GetLinkState();
  return state && state->TryLock(side_);
}

void RemoteRouterLink::Unlock() {
  if (RouterLinkState* state = GetLinkState()) {
    state->Unlock(side_);
  }
}

bool RemoteRouterLink::FlushOtherSideIfWaiting() {
  RouterLinkState* state = GetLinkState();
  if (!state || !state->ResetWaitingBit(side_.opposite())) {
    return false;
  }

  msg::FlushRouter flush;
  flush.v0()->sublink = sublink_;
  node_link()->Transmit(flush);
  return true;
}

bool RemoteRouterLink::CanNodeRequestBypass(
    const NodeName& bypass_request_source) {
  RouterLinkState* state = GetLinkState();
  if (!state) {
    return false;
  }

  NodeName allowed_source = state->allowed_bypass_request_source.LoadAcquire();
  return state->is_locked_by(side_.opposite()) &&
         allowed_source == bypass_request_source;
}

void RemoteRouterLink::Deactivate() {
  node_link()->RemoveRemoteRouterLink(sublink_);
}

void RemoteRouterLink::BypassPeer(const NodeName& bypass_target_node,
                                  SublinkId bypass_target_sublink) {
  msg::BypassPeer bypass;
  bypass.v0()->sublink = sublink_;
  bypass.v0()->reserved0 = 0;
  bypass.v0()->bypass_target_node = bypass_target_node;
  bypass.v0()->bypass_target_sublink = bypass_target_sublink;
  node_link()->Transmit(bypass);
}

void RemoteRouterLink::StopProxying(SequenceNumber inbound_sequence_length,
                                    SequenceNumber outbound_sequence_length) {
  msg::StopProxying stop;
  stop.v0()->sublink = sublink_;
  stop.v0()->inbound_sequence_length = inbound_sequence_length;
  stop.v0()->outbound_sequence_length = outbound_sequence_length;
  node_link()->Transmit(stop);
}

void RemoteRouterLink::ProxyWillStop(SequenceNumber inbound_sequence_length) {
  msg::ProxyWillStop will_stop;
  will_stop.v0()->sublink = sublink_;
  will_stop.v0()->inbound_sequence_length = inbound_sequence_length;
  node_link()->Transmit(will_stop);
}

void RemoteRouterLink::BypassPeerWithLink(
    SublinkId new_sublink,
    FragmentRef<RouterLinkState> new_link_state,
    SequenceNumber inbound_sequence_length) {
  msg::BypassPeerWithLink bypass;
  bypass.v0()->sublink = sublink_;
  bypass.v0()->new_sublink = new_sublink;
  bypass.v0()->new_link_state_fragment = new_link_state.release().descriptor();
  bypass.v0()->inbound_sequence_length = inbound_sequence_length;
  node_link()->Transmit(bypass);
}

void RemoteRouterLink::StopProxyingToLocalPeer(
    SequenceNumber outbound_sequence_length) {
  msg::StopProxyingToLocalPeer stop;
  stop.v0()->sublink = sublink_;
  stop.v0()->outbound_sequence_length = outbound_sequence_length;
  node_link()->Transmit(stop);
}

std::string RemoteRouterLink::Describe() const {
  std::stringstream ss;
  ss << type_.ToString() << " link from "
     << node_link_->local_node_name().ToString() << " to "
     << node_link_->remote_node_name().ToString() << " via sublink "
     << sublink_;
  return ss.str();
}

}  // namespace ipcz
