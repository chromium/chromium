// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/route_edge.h"

#include <utility>

#include "ipcz/router.h"
#include "ipcz/router_link.h"
#include "util/log.h"

namespace ipcz {

RouteEdge::RouteEdge() = default;

RouteEdge::~RouteEdge() = default;

void RouteEdge::SetPrimaryLink(Ref<RouterLink> link) {
  ABSL_ASSERT(!primary_link_);
  if (is_decay_deferred()) {
    // This edge was set to decay its primary link before it had a primary link,
    // so this primary link must be immediately set to decay.
    decaying_link_->link = std::move(link);
    if (decaying_link_->link) {
      DVLOG(4) << "Edge adopted decaying " << decaying_link_->link->Describe();
    }
  } else {
    primary_link_ = std::move(link);
    if (primary_link_) {
      DVLOG(4) << "Edge adopted " << primary_link_->Describe();
    }
  }
}

Ref<RouterLink> RouteEdge::ReleasePrimaryLink() {
  return std::move(primary_link_);
}

Ref<RouterLink> RouteEdge::ReleaseDecayingLink() {
  if (!decaying_link_) {
    return nullptr;
  }

  Ref<RouterLink> link = std::move(decaying_link_->link);
  decaying_link_.reset();
  return link;
}

Ref<Router> RouteEdge::GetLocalPeer() {
  return primary_link_ ? primary_link_->GetLocalPeer() : nullptr;
}

Ref<Router> RouteEdge::GetDecayingLocalPeer() {
  const auto& link = decaying_link();
  return link ? link->GetLocalPeer() : nullptr;
}

bool RouteEdge::BeginPrimaryLinkDecay() {
  if (decaying_link_) {
    return false;
  }

  decaying_link_ = std::make_unique<DecayingLink>();
  decaying_link_->link = std::move(primary_link_);
  return true;
}

bool RouteEdge::ShouldTransmitOnDecayingLink(SequenceNumber n) const {
  return decaying_link_ && (!decaying_link_->outgoing_length ||
                            n < *decaying_link_->outgoing_length);
}

bool RouteEdge::MaybeFinishDecay(SequenceNumber length_sent,
                                 SequenceNumber length_received) {
  if (!decaying_link_ || !decaying_link_->link) {
    return false;
  }

  if (!decaying_link_->outgoing_length) {
    DVLOG(4) << "Cannot decay yet with no known sequence length to "
             << decaying_link_->link->Describe();
    return false;
  }

  if (!decaying_link_->incoming_length) {
    DVLOG(4) << "Cannot decay yet with no known sequence length to "
             << decaying_link_->link->Describe();
    return false;
  }

  if (length_sent < *decaying_link_->outgoing_length) {
    DVLOG(4) << "Cannot decay yet without sending full sequence up to "
             << *decaying_link_->outgoing_length << " on "
             << decaying_link_->link->Describe();
    return false;
  }

  if (length_received < *decaying_link_->incoming_length) {
    DVLOG(4) << "Cannot decay yet without receiving full sequence up to "
             << *decaying_link_->incoming_length << " on "
             << decaying_link_->link->Describe();
    return false;
  }

  decaying_link_.reset();
  return true;
}

}  // namespace ipcz
