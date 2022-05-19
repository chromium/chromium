// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/multinode_test.h"

#include "ipcz/ipcz.h"
#include "reference_drivers/single_process_reference_driver.h"
#include "third_party/abseil-cpp/absl/base/macros.h"

namespace ipcz::test {

namespace {

const IpczDriver* GetDriverImpl(MultinodeTest::DriverMode mode) {
  switch (mode) {
    case MultinodeTest::DriverMode::kSync:
      return &reference_drivers::kSingleProcessReferenceDriver;

    default:
      // Other modes not yet supported.
      return nullptr;
  }
}

void DoConnect(const IpczAPI& ipcz,
               IpczHandle node,
               IpczDriverHandle transport,
               IpczConnectNodeFlags flags,
               IpczHandle& portal) {
  const IpczResult result =
      ipcz.ConnectNode(node, transport, 1, flags, nullptr, &portal);
  ASSERT_EQ(IPCZ_RESULT_OK, result);
}

}  // namespace

MultinodeTest::MultinodeTest() = default;

MultinodeTest::~MultinodeTest() = default;

const IpczDriver& MultinodeTest::GetDriver(DriverMode mode) const {
  const IpczDriver* driver = GetDriverImpl(mode);
  ABSL_ASSERT(driver);
  return *driver;
}

IpczHandle MultinodeTest::CreateBrokerNode(DriverMode mode) {
  IpczHandle node;
  ipcz().CreateNode(&GetDriver(mode), IPCZ_INVALID_DRIVER_HANDLE,
                    IPCZ_CREATE_NODE_AS_BROKER, nullptr, &node);
  return node;
}

IpczHandle MultinodeTest::CreateNonBrokerNode(DriverMode mode) {
  IpczHandle node;
  ipcz().CreateNode(&GetDriver(mode), IPCZ_INVALID_DRIVER_HANDLE, IPCZ_NO_FLAGS,
                    nullptr, &node);
  return node;
}

void MultinodeTest::CreateBrokerToNonBrokerTransports(
    DriverMode mode,
    IpczDriverHandle* transport0,
    IpczDriverHandle* transport1) {
  // TODO: Support other DriverModes.
  ABSL_ASSERT(mode == DriverMode::kSync);
  IpczResult result = GetDriver(mode).CreateTransports(
      IPCZ_INVALID_DRIVER_HANDLE, IPCZ_INVALID_DRIVER_HANDLE, IPCZ_NO_FLAGS,
      nullptr, transport0, transport1);
  ABSL_ASSERT(result == IPCZ_RESULT_OK);
}

std::pair<IpczHandle, IpczHandle> MultinodeTest::ConnectBrokerToNonBroker(
    DriverMode mode,
    IpczHandle broker_node,
    IpczHandle non_broker_node) {
  IpczDriverHandle broker_transport;
  IpczDriverHandle non_broker_transport;
  CreateBrokerToNonBrokerTransports(mode, &broker_transport,
                                    &non_broker_transport);

  IpczHandle broker_portal;
  DoConnect(ipcz(), broker_node, broker_transport, IPCZ_NO_FLAGS,
            broker_portal);

  IpczHandle non_broker_portal;
  DoConnect(ipcz(), non_broker_node, non_broker_transport,
            IPCZ_CONNECT_NODE_TO_BROKER, non_broker_portal);

  return {broker_portal, non_broker_portal};
}

}  // namespace ipcz::test
