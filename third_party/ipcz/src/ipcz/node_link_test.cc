// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/node_link.h"

#include <utility>

#include "ipcz/driver_memory.h"
#include "ipcz/features.h"
#include "ipcz/link_side.h"
#include "ipcz/link_type.h"
#include "ipcz/node_link_memory.h"
#include "ipcz/remote_router_link.h"
#include "ipcz/router.h"
#include "ipcz/sublink_id.h"
#include "reference_drivers/sync_reference_driver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/base/macros.h"
#include "util/ref_counted.h"

namespace ipcz {
namespace {

const IpczDriver& kDriver = reference_drivers::kSyncReferenceDriver;

std::pair<Ref<NodeLink>, Ref<NodeLink>> LinkNodes(Ref<Node> broker,
                                                  Ref<Node> non_broker) {
  IpczDriverHandle handle0, handle1;
  EXPECT_EQ(IPCZ_RESULT_OK,
            kDriver.CreateTransports(IPCZ_INVALID_DRIVER_HANDLE,
                                     IPCZ_INVALID_DRIVER_HANDLE, IPCZ_NO_FLAGS,
                                     nullptr, &handle0, &handle1));

  auto transport0 =
      MakeRefCounted<DriverTransport>(DriverObject(kDriver, handle0));
  auto transport1 =
      MakeRefCounted<DriverTransport>(DriverObject(kDriver, handle1));

  DriverMemoryWithMapping buffer = NodeLinkMemory::AllocateMemory(kDriver);
  ABSL_ASSERT(buffer.mapping.is_valid());

  const NodeName non_broker_name = broker->GenerateRandomName();
  auto link0 = NodeLink::CreateInactive(
      broker, LinkSide::kA, broker->GetAssignedName(), non_broker_name,
      Node::Type::kNormal, 0, non_broker->features(), transport0,
      NodeLinkMemory::Create(broker, LinkSide::kA, non_broker->features(),
                             std::move(buffer.mapping)));
  auto link1 = NodeLink::CreateInactive(
      non_broker, LinkSide::kB, non_broker_name, broker->GetAssignedName(),
      Node::Type::kNormal, 0, broker->features(), transport1,
      NodeLinkMemory::Create(non_broker, LinkSide::kB, broker->features(),
                             buffer.memory.Map()));
  link0->Activate();
  link1->Activate();
  return {link0, link1};
}

using NodeLinkTest = testing::Test;

TEST_F(NodeLinkTest, BasicTransmission) {
  Ref<Node> node0 = MakeRefCounted<Node>(Node::Type::kBroker, kDriver);
  Ref<Node> node1 = MakeRefCounted<Node>(Node::Type::kNormal, kDriver);

  auto [link0, link1] = LinkNodes(node0, node1);
  auto router0 = MakeRefCounted<Router>();
  auto router1 = MakeRefCounted<Router>();
  FragmentRef<RouterLinkState> link_state =
      link0->memory().GetInitialRouterLinkState(0);
  router0->SetOutwardLink(link0->AddRemoteRouterLink(
      SublinkId(0), link_state, LinkType::kCentral, LinkSide::kA, router0));
  router1->SetOutwardLink(link1->AddRemoteRouterLink(
      SublinkId(0), link_state, LinkType::kCentral, LinkSide::kB, router1));
  link_state->status = RouterLinkState::kStable;

  EXPECT_FALSE(router1->IsPeerClosed());
  router0->CloseRoute();
  EXPECT_TRUE(router1->IsPeerClosed());
  router1->CloseRoute();

  link0->Deactivate();
  link1->Deactivate();
}

TEST_F(NodeLinkTest, AvailableFeatures) {
  const IpczFeature kEnabledFeatures[] = {IPCZ_FEATURE_MEM_V2};
  const IpczCreateNodeOptions options_with_features = {
      .size = sizeof(options_with_features),
      .enabled_features = kEnabledFeatures,
      .num_enabled_features = std::size(kEnabledFeatures),
  };
  Ref<Node> node0 = MakeRefCounted<Node>(Node::Type::kBroker, kDriver,
                                         &options_with_features);
  Ref<Node> node1 = MakeRefCounted<Node>(Node::Type::kNormal, kDriver,
                                         &options_with_features);
  Ref<Node> node2 = MakeRefCounted<Node>(Node::Type::kNormal, kDriver);

  auto [link01, link10] = LinkNodes(node0, node1);
  auto [link02, link20] = LinkNodes(node0, node2);

  // The node0-node1 link supports mem_v2 because both nodes enabled it.
  EXPECT_TRUE(link01->available_features().mem_v2());
  EXPECT_TRUE(link01->memory().available_features().mem_v2());
  EXPECT_TRUE(link10->available_features().mem_v2());
  EXPECT_TRUE(link10->memory().available_features().mem_v2());

  // The node0-node2 link doesn't support mem_v2 because node2 didn't enable it.
  EXPECT_FALSE(link02->available_features().mem_v2());
  EXPECT_FALSE(link02->memory().available_features().mem_v2());
  EXPECT_FALSE(link20->available_features().mem_v2());
  EXPECT_FALSE(link20->memory().available_features().mem_v2());

  link01->Deactivate();
  link10->Deactivate();
  link02->Deactivate();
  link20->Deactivate();
}

}  // namespace
}  // namespace ipcz
