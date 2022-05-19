// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_TEST_MULTINODE_TEST_H_
#define IPCZ_SRC_TEST_MULTINODE_TEST_H_

#include "ipcz/ipcz.h"
#include "test/test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ipcz::test {

// Base test fixture to support tests which exercise behavior across multiple
// ipcz nodes. These may be single-process on a synchronous driver,
// single-process on an asynchronous (e.g. multiprocess) driver, or fully
// multiprocess.
//
// This fixture mostly provides convenience methods for creating and connecting
// nodes in various useful configurations.
class MultinodeTest : public TestBase {
 public:
  // Selects which driver a new node will use. Interconnecting nodes must always
  // use the same driver.
  //
  // Multinode tests are parameterized over these modes to provide coverage of
  // various interesting constraints encountered in production. Some platforms
  // require driver objects to be relayed through a broker. Some environments
  // prevent nodes from allocating their own shared memory regions.
  //
  // Incongruity between synchronous and asynchronous test failures generally
  // indicates race conditions within ipcz, but many bugs will cause failures in
  // all driver modes. The synchronous version is deterministic and generally
  // easier to debug in such cases.
  enum class DriverMode {
    // Use the fully synchronous, single-process reference driver. This driver
    // does not create any background threads and all ipcz operations will
    // complete synchronously from end-to-end.
    kSync,

    // Use the async multiprocess driver as-is. All nodes can allocate their own
    // shared memory directly through the driver.
    kAsync,

    // Use the async multiprocess driver, and force non-broker nodes to delegate
    // shared memory allocation to their broker.
    kAsyncDelegatedAlloc,

    // Use the async multiprocess driver, and force non-broker-to-non-broker
    // transmission of driver objects to be relayed through a broker. All nodes
    // can allocate their own shared memory directly through the driver.
    kAsyncObjectBrokering,

    // Use the async multiprocess driver, forcing shared memory AND driver
    // object relay both to be delegated to a broker.
    kAsyncObjectBrokeringAndDelegatedAlloc,
  };

  MultinodeTest();
  ~MultinodeTest() override;

  const IpczDriver& GetDriver(DriverMode mode) const;

  // Creates a new broker node using the given DriverMode.
  IpczHandle CreateBrokerNode(DriverMode mode);

  // Creates a new broker node using the given DriverMode.
  IpczHandle CreateNonBrokerNode(DriverMode mode);

  // Creates a pair of transports for the given driver mode.
  void CreateBrokerToNonBrokerTransports(
      DriverMode mode,
      IpczDriverHandle* broker_transport,
      IpczDriverHandle* non_broker_transport);

  std::pair<IpczHandle, IpczHandle> ConnectBrokerToNonBroker(
      DriverMode mode,
      IpczHandle broker_node,
      IpczHandle non_broker_node);
};

// Helper for a MultinodeTest parameterized over DriverMode. Most integration
// tests should use this for parameterization.
class MultinodeTestWithDriver
    : public MultinodeTest,
      public testing::WithParamInterface<MultinodeTest::DriverMode> {
 public:
  const IpczDriver& GetDriver() const {
    return MultinodeTest::GetDriver(GetParam());
  }

  IpczHandle CreateBrokerNode() {
    return MultinodeTest::CreateBrokerNode(GetParam());
  }

  IpczHandle CreateNonBrokerNode() {
    return MultinodeTest::CreateNonBrokerNode(GetParam());
  }

  void CreateBrokerToNonBrokerTransports(
      IpczDriverHandle* broker_transport,
      IpczDriverHandle* non_broker_transport) {
    MultinodeTest::CreateBrokerToNonBrokerTransports(
        GetParam(), broker_transport, non_broker_transport);
  }

  std::pair<IpczHandle, IpczHandle> ConnectBrokerToNonBroker(
      IpczHandle broker_node,
      IpczHandle non_broker_node) {
    return MultinodeTest::ConnectBrokerToNonBroker(GetParam(), broker_node,
                                                   non_broker_node);
  }
};

}  // namespace ipcz::test

// TODO: Add other DriverMode enumerators here as support is landed.
#define INSTANTIATE_MULTINODE_TEST_SUITE_P(suite) \
  INSTANTIATE_TEST_SUITE_P(                       \
      , suite,                                    \
      ::testing::Values(ipcz::test::MultinodeTest::DriverMode::kSync))

#endif  // IPCZ_SRC_TEST_MULTINODE_TEST_H_
