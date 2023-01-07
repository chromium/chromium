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
  if (is_decay_deferred_) {
    // This edge was set to decay its primary link before it had a primary link,
    // so this primary link must be immediately set to decay.
    is_decay_deferred_ = false;
    decaying_link_ = std::move(link);
    if (decaying_link_) {
      DVLOG(4) << "Edge adopted decaying " << decaying_link_->Describe();
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
  return std::move(decaying_link_);
}

Ref<Router> RouteEdge::GetLocalPeer() {
  return primary_link_ ? primary_link_->GetLocalPeer() : nullptr;
}

Ref<Router> RouteEdge::GetDecayingLocalPeer() {
  return decaying_link_ ? decaying_link_->GetLocalPeer() : nullptr;
}

bool RouteEdge::BeginPrimaryLinkDecay() {
  if (decaying_link_ || is_decay_deferred_) {
    return false;
  }

  decaying_link_ = std::move(primary_link_);
  is_decay_deferred_ = !decaying_link_;
  return true;
}

bool RouteEdge::ShouldTransmitOnDecayingLink(SequenceNumber n) const {
  return (decaying_link_ || is_decay_deferred_) &&
         (!length_to_decaying_link_ || n < *length_to_decaying_link_);
}

bool RouteEdge::MaybeFinishDecay(SequenceNumber length_sent,
                                 SequenceNumber length_received) {
  if (!decaying_link_) {
    return false;
  }

  if (!length_to_decaying_link_) {
    DVLOG(4) << "Cannot decay yet with no known sequence length to "
             << decaying_link_->Describe();
    return false;
  }

  if (!length_from_decaying_link_) {
    DVLOG(4) << "Cannot decay yet with no known sequence length to "
             << decaying_link_->Describe();
    return false;
  }

  if (length_sent < *length_to_decaying_link_) {
    DVLOG(4) << "Cannot decay yet without sending full sequence up to "
             << *length_to_decaying_link_ << " on "
             << decaying_link_->Describe();
    return false;
  }

  if (length_received < *length_from_decaying_link_) {
    DVLOG(4) << "Cannot decay yet without receiving full sequence up to "
             << *length_from_decaying_link_ << " on "
             << decaying_link_->Describe();
    return false;
  }

  ABSL_ASSERT(!is_decay_deferred_);
  decaying_link_.reset();
  length_to_decaying_link_.reset();
  length_from_decaying_link_.reset();
  return true;
}

}  // namespace ipcz
