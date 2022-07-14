// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/remote_router_link.h"

#include <sstream>
#include <utility>

#include "ipcz/box.h"
#include "ipcz/node_link.h"
#include "ipcz/node_link_memory.h"
#include "ipcz/node_messages.h"
#include "ipcz/portal.h"
#include "ipcz/router.h"
#include "util/log.h"

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
  ABSL_ASSERT(type.is_central() || link_state.is_null());
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
  if (state.is_null()) {
    // By convention, if a central link has no RouterLinkState at construction
    // time, side A is responsible for allocating a new one and sharing it with
    // side B eventually. Side B lives with a null RouterLinkState until then.
    if (side_.is_side_a()) {
      AllocateAndShareLinkState();
    }
    return;
  }

  if (state.is_pending()) {
    // By convention, side A should never be given a pending RouterLinkState
    // fragment.
    ABSL_ASSERT(side_.is_side_b());

    // Side B on the other hand may obtain a RouterLinkState fragment which it
    // can't address yet, and in this case, we wait for the fragment's buffer to
    // be mapped locally.
    Ref<NodeLinkMemory> memory = WrapRefCounted(&node_link()->memory());
    FragmentDescriptor descriptor = state.fragment().descriptor();
    memory->WaitForBufferAsync(
        descriptor.buffer_id(),
        [self = WrapRefCounted(this), memory, descriptor] {
          auto fragment = memory->GetFragment(descriptor);
          self->SetLinkState(
              memory->AdoptFragmentRef<RouterLinkState>(fragment));
        });
    return;
  }

  ABSL_ASSERT(state.is_addressable());

  // SetLinkState() must be called with an addressable fragment only once.
  ABSL_ASSERT(link_state_.load(std::memory_order_acquire) == nullptr);

  // The release when storing `link_state_` is balanced by an acquire in
  // GetLinkState().
  link_state_fragment_ = std::move(state);
  link_state_.store(link_state_fragment_.get(), std::memory_order_release);

  // If this side of the link was already marked stable before the
  // RouterLinkState was available, `side_is_stable_` will be true. In that
  // case, set the stable bit in RouterLinkState immediately. This may unblock
  // some routing work. The acquire here is balanced by a release in
  // MarkSideStable().
  if (side_is_stable_.load(std::memory_order_acquire)) {
    MarkSideStable();
  }
  if (Ref<Router> router = node_link()->GetRouter(sublink_)) {
    router->Flush();
  }
}

LinkType RemoteRouterLink::GetType() const {
  return type_;
}

RouterLinkState* RemoteRouterLink::GetLinkState() const {
  return link_state_.load(std::memory_order_acquire);
}

bool RemoteRouterLink::HasLocalPeer(const Router& router) {
  return false;
}

bool RemoteRouterLink::IsRemoteLinkTo(const NodeLink& node_link,
                                      SublinkId sublink) {
  return node_link_.get() == &node_link && sublink_ == sublink;
}

void RemoteRouterLink::AcceptParcel(Parcel& parcel) {
  const absl::Span<Ref<APIObject>> objects = parcel.objects_view();

  msg::AcceptParcel accept;
  accept.params().sublink = sublink_;
  accept.params().sequence_number = parcel.sequence_number();

  size_t num_portals = 0;
  absl::InlinedVector<DriverObject, 2> driver_objects;
  for (Ref<APIObject>& object : objects) {
    switch (object->object_type()) {
      case APIObject::kPortal:
        ++num_portals;
        break;

      case APIObject::kBox: {
        Box* box = Box::FromObject(object.get());
        ABSL_ASSERT(box);

        // TODO: Support object relay when direct transmission is impossible.
        ABSL_ASSERT(box->object().CanTransmitOn(*node_link()->transport()));

        driver_objects.push_back(std::move(box->object()));
        break;
      }

      default:
        break;
    }
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

    switch (object.object_type()) {
      case APIObject::kPortal: {
        handle_types[i] = HandleType::kPortal;

        Ref<Router> router = Portal::FromObject(&object)->router();
        router->SerializeNewRouter(*node_link(), new_routers[i]);
        routers_to_proxy.push_back(std::move(router));
        break;
      }

      case APIObject::kBox:
        handle_types[i] = HandleType::kBox;
        break;

      default:
        DLOG(FATAL) << "Attempted to transmit an invalid object.";
        break;
    }
  }

  accept.params().driver_objects =
      accept.AppendDriverObjects(absl::MakeSpan(driver_objects));

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

  state->allowed_bypass_request_source = bypass_request_source;

  // Balanced by an acquire in CanNodeRequestBypass().
  std::atomic_thread_fence(std::memory_order_release);
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
  flush.params().sublink = sublink_;
  node_link()->Transmit(flush);
  return true;
}

bool RemoteRouterLink::CanNodeRequestBypass(
    const NodeName& bypass_request_source) {
  RouterLinkState* state = GetLinkState();

  // Balanced by a release in TryLockForBypass().
  std::atomic_thread_fence(std::memory_order_acquire);
  return state && state->is_locked_by(side_.opposite()) &&
         state->allowed_bypass_request_source == bypass_request_source;
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

void RemoteRouterLink::AllocateAndShareLinkState() {
  node_link()->memory().AllocateRouterLinkState(
      [self = WrapRefCounted(this)](FragmentRef<RouterLinkState> state) {
        if (state.is_null()) {
          DLOG(ERROR) << "Unable to allocate RouterLinkState.";
          return;
        }
        ABSL_ASSERT(state.is_addressable());
        self->SetLinkState(state);

        msg::SetRouterLinkState set;
        set.params().sublink = self->sublink();
        set.params().descriptor = state.release().descriptor();
        self->node_link()->Transmit(set);
      });
}

}  // namespace ipcz
