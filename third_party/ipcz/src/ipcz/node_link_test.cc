// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/node_link.h"

#include <utility>

#include "ipcz/link_side.h"
#include "ipcz/link_type.h"
#include "ipcz/node_link_memory.h"
#include "ipcz/remote_router_link.h"
#include "ipcz/router.h"
#include "ipcz/sublink_id.h"
#include "reference_drivers/single_process_reference_driver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/base/macros.h"
#include "util/ref_counted.h"

namespace ipcz {
namespace {

const IpczDriver& kDriver = reference_drivers::kSingleProcessReferenceDriver;

std::pair<Ref<NodeLink>, Ref<NodeLink>> LinkNodes(Ref<Node> broker,
                                                  Ref<Node> non_broker) {
  IpczDriverHandle handle0, handle1;
  EXPECT_EQ(IPCZ_RESULT_OK,
            kDriver.CreateTransports(IPCZ_INVALID_DRIVER_HANDLE,
                                     IPCZ_INVALID_DRIVER_HANDLE, IPCZ_NO_FLAGS,
                                     nullptr, &handle0, &handle1));

  auto transport0 =
      MakeRefCounted<DriverTransport>(DriverObject(broker, handle0));
  auto transport1 =
      MakeRefCounted<DriverTransport>(DriverObject(non_broker, handle1));

  NodeLinkMemory::Allocation allocation = NodeLinkMemory::Allocate(broker);
  ABSL_ASSERT(allocation.node_link_memory);

  const NodeName non_broker_name = broker->GenerateRandomName();
  auto link0 =
      NodeLink::Create(broker, LinkSide::kA, broker->GetAssignedName(),
                       non_broker_name, Node::Type::kNormal, 0, transport0,
                       std::move(allocation.node_link_memory));
  auto link1 = NodeLink::Create(
      non_broker, LinkSide::kB, non_broker_name, broker->GetAssignedName(),
      Node::Type::kNormal, 0, transport1,
      NodeLinkMemory::Adopt(non_broker,
                            std::move(allocation.primary_buffer_memory)));

  transport0->Activate();
  transport1->Activate();
  return {link0, link1};
}

using NodeLinkTest = testing::Test;

TEST_F(NodeLinkTest, BasicTransmission) {
  Ref<Node> node0 = MakeRefCounted<Node>(Node::Type::kBroker, kDriver,
                                         IPCZ_INVALID_DRIVER_HANDLE);
  Ref<Node> node1 = MakeRefCounted<Node>(Node::Type::kNormal, kDriver,
                                         IPCZ_INVALID_DRIVER_HANDLE);

  auto [link0, link1] = LinkNodes(node0, node1);
  auto router0 = MakeRefCounted<Router>();
  auto router1 = MakeRefCounted<Router>();
  router0->SetOutwardLink(link0->AddRemoteRouterLink(
      SublinkId(0), LinkType::kCentral, LinkSide::kA, router0));
  router1->SetOutwardLink(link1->AddRemoteRouterLink(
      SublinkId(0), LinkType::kCentral, LinkSide::kB, router1));

  EXPECT_FALSE(router1->IsPeerClosed());
  router0->CloseRoute();
  EXPECT_TRUE(router1->IsPeerClosed());
  router1->CloseRoute();
}

}  // namespace
}  // namespace ipcz
