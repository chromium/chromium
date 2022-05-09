// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/node.h"

#include <vector>

#include "ipcz/ipcz.h"
#include "ipcz/node_connector.h"
#include "ipcz/node_link.h"
#include "ipcz/portal.h"
#include "ipcz/router.h"
#include "third_party/abseil-cpp/absl/base/macros.h"
#include "third_party/abseil-cpp/absl/synchronization/mutex.h"
#include "util/log.h"
#include "util/ref_counted.h"

namespace ipcz {

Node::Node(Type type, const IpczDriver& driver, IpczDriverHandle driver_node)
    : type_(type), driver_(driver), driver_node_(driver_node) {
  if (type_ == Type::kBroker) {
    // Only brokers assign their own names.
    assigned_name_ = GenerateRandomName();
    DVLOG(4) << "Created new broker node " << assigned_name_.ToString();
  } else {
    DVLOG(4) << "Created new non-broker node " << this;
  }
}

Node::~Node() = default;

IpczResult Node::Close() {
  absl::flat_hash_map<NodeName, Ref<NodeLink>> node_links;
  {
    absl::MutexLock lock(&mutex_);
    node_links_.swap(node_links);
    broker_link_.reset();
  }

  for (const auto& entry : node_links) {
    entry.second->Deactivate();
  }
  return IPCZ_RESULT_OK;
}

NodeName Node::GetAssignedName() {
  absl::MutexLock lock(&mutex_);
  return assigned_name_;
}

Ref<NodeLink> Node::GetBrokerLink() {
  absl::MutexLock lock(&mutex_);
  return broker_link_;
}

void Node::SetBrokerLink(Ref<NodeLink> link) {
  absl::MutexLock lock(&mutex_);
  ABSL_ASSERT(!broker_link_);
  broker_link_ = std::move(link);
}

void Node::SetAssignedName(const NodeName& name) {
  absl::MutexLock lock(&mutex_);
  ABSL_ASSERT(!assigned_name_.is_valid());
  assigned_name_ = name;
}

bool Node::AddLink(const NodeName& remote_node_name, Ref<NodeLink> link) {
  absl::MutexLock lock(&mutex_);
  auto [it, inserted] = node_links_.insert({remote_node_name, std::move(link)});
  return inserted;
}

NodeName Node::GenerateRandomName() const {
  NodeName name;
  IpczResult result =
      driver_.GenerateRandomBytes(sizeof(name), IPCZ_NO_FLAGS, nullptr, &name);
  ABSL_ASSERT(result == IPCZ_RESULT_OK);
  return name;
}

}  // namespace ipcz
