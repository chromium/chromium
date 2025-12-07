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

std::pair<Ref<NodeLink>, Ref<NodeLink>> LinkNodesWithoutActivation(
    Ref<Node> broker,
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
  return {link0, link1};
}

std::pair<Ref<NodeLink>, Ref<NodeLink>> LinkNodes(Ref<Node> broker,
                                                  Ref<Node> non_broker) {
  auto [link0, link1] = LinkNodesWithoutActivation(broker, non_broker);
  link0->Activate();
  link1->Activate();
  return {link0, link1};
}

std::pair<Ref<Router>, Ref<Router>> AttachRouters(Ref<NodeLink> link0,
                                                  Ref<NodeLink> link1) {
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
  EXPECT_FALSE(router0->IsPeerClosed());
  return {router0, router1};
}

using NodeLinkTest = testing::Test;

TEST_F(NodeLinkTest, BasicTransmission) {
  Ref<Node> node0 = MakeRefCounted<Node>(Node::Type::kBroker, kDriver);
  Ref<Node> node1 = MakeRefCounted<Node>(Node::Type::kNormal, kDriver);

  auto [link0, link1] = LinkNodes(node0, node1);
  auto [router0, router1] = AttachRouters(link0, link1);

  EXPECT_FALSE(router1->IsPeerClosed());
  router0->CloseRoute();
  EXPECT_TRUE(router1->IsPeerClosed());
  router1->CloseRoute();

  link0->Deactivate();
  link1->Deactivate();
}

constexpr char kMessage[] = "hello";

absl::Span<const uint8_t> MessageSpan() {
  return absl::MakeSpan(reinterpret_cast<const uint8_t*>(kMessage),
                        sizeof(kMessage));
}

TEST_F(NodeLinkTest, BasicTransmissionWithMessage) {
  Ref<Node> node0 = MakeRefCounted<Node>(Node::Type::kBroker, kDriver);
  Ref<Node> node1 = MakeRefCounted<Node>(Node::Type::kNormal, kDriver);

  auto [link0, link1] = LinkNodes(node0, node1);
  auto [router0, router1] = AttachRouters(link0, link1);

  // Send message.
  router0->Put(MessageSpan(), {});

  // Verify that one parcel arrived.
  IpczPortalStatus status = {.size = sizeof(status)};
  router1->QueryStatus(status);
  EXPECT_EQ(1u, status.num_local_parcels);
  EXPECT_EQ(sizeof(kMessage), status.num_local_bytes);

  // Verify message contents.
  char received_data[100];
  size_t received_data_size = sizeof(received_data);
  IpczResult result = router1->Get(0, received_data, &received_data_size,
                                   nullptr, nullptr, nullptr);
  EXPECT_EQ(IPCZ_RESULT_OK, result);
  EXPECT_EQ(sizeof(kMessage), received_data_size);
  EXPECT_STREQ("hello", received_data);

  router0->CloseRoute();
  router1->CloseRoute();

  link0->Deactivate();
  link1->Deactivate();
}

TEST_F(NodeLinkTest, AcceptEarlyParcel) {
  Ref<Node> node0 = MakeRefCounted<Node>(Node::Type::kBroker, kDriver);
  Ref<Node> node1 = MakeRefCounted<Node>(Node::Type::kNormal, kDriver);
  auto [link0, link1] = LinkNodes(node0, node1);

  // Set up the central link, but delay attaching the Router on the receiving
  // end.
  auto router0 = MakeRefCounted<Router>();
  FragmentRef<RouterLinkState> link_state =
      link0->memory().GetInitialRouterLinkState(0);
  router0->SetOutwardLink(link0->AddRemoteRouterLink(
      SublinkId(0), link_state, LinkType::kCentral, LinkSide::kA, router0));
  link_state->status = RouterLinkState::kStable;

  // Send message. The parcel is sent immediately, but should be queued as
  // 'early' because there is no Router+RouterLink for it.
  router0->Put(MessageSpan(), {});

  // Attach the Router.
  auto router1 = MakeRefCounted<Router>();
  router1->SetOutwardLink(link1->AddRemoteRouterLink(
      SublinkId(0), link_state, LinkType::kCentral, LinkSide::kB, router1));

  // Verify that no parcels arrived to the receiving Router.
  IpczPortalStatus status = {.size = sizeof(status)};
  router1->QueryStatus(status);
  EXPECT_EQ(0u, status.num_local_parcels);

  // Take the early parcels. Normally it is done during Router deserialization.
  // Here, to avoid deserialization verbosity simulate it for a Router that
  // already exists (router1).
  link1->AcceptEarlyParcelsForSublink(SublinkId(0));

  // Verify that the parcel is accepted after the simulated deserialization.
  router1->QueryStatus(status);
  EXPECT_EQ(1u, status.num_local_parcels);

  // Verify message contents.
  char received_data[100];
  size_t received_data_size = sizeof(received_data);
  IpczResult result = router1->Get(0, received_data, &received_data_size,
                                   nullptr, nullptr, nullptr);
  EXPECT_EQ(IPCZ_RESULT_OK, result);
  EXPECT_EQ(sizeof(kMessage), received_data_size);
  EXPECT_STREQ("hello", received_data);

  link0->Deactivate();
  link1->Deactivate();
}

TEST_F(NodeLinkTest, RejectParcelForClosedRouter) {
  Ref<Node> node0 = MakeRefCounted<Node>(Node::Type::kBroker, kDriver);
  Ref<Node> node1 = MakeRefCounted<Node>(Node::Type::kNormal, kDriver);

  // Link nodes and avoid activating link1 to let the incoming message wait
  // until transport activation.
  auto [link0, link1] = LinkNodesWithoutActivation(node0, node1);
  link0->Activate();
  auto [router0, router1] = AttachRouters(link0, link1);

  // Send message. The parcel should get queued in router0 because the transport
  // is not activated.
  router0->Put(MessageSpan(), {});

  // Simulate early closing of the Router while the parcel is in flight.
  link1->RemoveRemoteRouterLink(SublinkId(0));
  EXPECT_EQ(1u, link1->DeletedSublinkCountForTesting());

  // Activate transport.
  link1->Activate();

  // Verify that the early parcels are gone.
  EXPECT_EQ(0u, link1->EarlyParcelCountForTesting());
  char received_data[100];
  size_t received_data_size = sizeof(received_data);
  IpczResult result = router1->Get(0, received_data, &received_data_size,
                                   nullptr, nullptr, nullptr);
  EXPECT_EQ(IPCZ_RESULT_UNAVAILABLE, result);

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
