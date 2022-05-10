// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ipcz/ipcz.h"
#include "ipcz/node_messages.h"
#include "test/multinode_test.h"
#include "test/test_transport_listener.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ipcz {
namespace {

using ConnectTest = test::MultinodeTestWithDriver;

TEST_P(ConnectTest, BrokerToNonBroker) {
  IpczHandle broker = CreateBrokerNode();
  IpczHandle non_broker = CreateNonBrokerNode();

  IpczDriverHandle broker_transport;
  IpczDriverHandle non_broker_transport;
  CreateBrokerToNonBrokerTransports(&broker_transport, &non_broker_transport);

  IpczHandle non_broker_portal;
  ASSERT_EQ(IPCZ_RESULT_OK, ipcz().ConnectNode(non_broker, non_broker_transport,
                                               1, IPCZ_CONNECT_NODE_TO_BROKER,
                                               nullptr, &non_broker_portal));

  IpczHandle broker_portal;
  ASSERT_EQ(IPCZ_RESULT_OK,
            ipcz().ConnectNode(broker, broker_transport, 1, IPCZ_NO_FLAGS,
                               nullptr, &broker_portal));

  Close(broker_portal);
  EXPECT_EQ(IPCZ_RESULT_OK,
            WaitForConditionFlags(non_broker_portal, IPCZ_TRAP_PEER_CLOSED));

  CloseAll({non_broker_portal, non_broker, broker});
}

TEST_P(ConnectTest, SurplusPortals) {
  IpczHandle broker = CreateBrokerNode();
  IpczHandle non_broker = CreateNonBrokerNode();

  IpczDriverHandle broker_transport;
  IpczDriverHandle non_broker_transport;
  CreateBrokerToNonBrokerTransports(&broker_transport, &non_broker_transport);

  constexpr size_t kNumBrokerPortals = 2;
  constexpr size_t kNumNonBrokerPortals = 5;
  static_assert(kNumBrokerPortals < kNumNonBrokerPortals,
                "Test requires fewer broker portals than non-broker portals");

  IpczHandle broker_portals[kNumBrokerPortals];
  ASSERT_EQ(
      IPCZ_RESULT_OK,
      ipcz().ConnectNode(broker, broker_transport, std::size(broker_portals),
                         IPCZ_NO_FLAGS, nullptr, broker_portals));

  IpczHandle non_broker_portals[kNumNonBrokerPortals];
  ASSERT_EQ(IPCZ_RESULT_OK, ipcz().ConnectNode(non_broker, non_broker_transport,
                                               std::size(non_broker_portals),
                                               IPCZ_CONNECT_NODE_TO_BROKER,
                                               nullptr, non_broker_portals));

  // All of the surplus broker portals should observe peer closure.
  for (size_t i = kNumBrokerPortals; i < kNumNonBrokerPortals; ++i) {
    EXPECT_EQ(IPCZ_RESULT_OK, WaitForConditionFlags(non_broker_portals[i],
                                                    IPCZ_TRAP_PEER_CLOSED));
  }

  for (IpczHandle portal : non_broker_portals) {
    Close(portal);
  }
  for (IpczHandle portal : broker_portals) {
    Close(portal);
  }
  CloseAll({non_broker, broker});
}

TEST_P(ConnectTest, DisconnectWithoutHandshake) {
  IpczHandle broker = CreateBrokerNode();
  IpczHandle non_broker = CreateNonBrokerNode();

  // First fail to connect a broker.
  IpczDriverHandle broker_transport, non_broker_transport;
  CreateBrokerToNonBrokerTransports(&broker_transport, &non_broker_transport);

  IpczHandle portal;
  {
    // This listener is scoped such that it closes the non-broker's transport
    // after the broker issues its ConnectNode(). This should trigger a
    // rejection and ultimately portal closure by the broker.
    test::TestTransportListener non_broker_listener(non_broker,
                                                    non_broker_transport);
    non_broker_listener.DiscardMessages<msg::ConnectFromBrokerToNonBroker>();

    ASSERT_EQ(IPCZ_RESULT_OK,
              ipcz().ConnectNode(broker, broker_transport, 1, IPCZ_NO_FLAGS,
                                 nullptr, &portal));
  }

  EXPECT_EQ(IPCZ_RESULT_OK,
            WaitForConditionFlags(portal, IPCZ_TRAP_PEER_CLOSED));
  Close(portal);

  // Next fail to connect a non-broker.
  CreateBrokerToNonBrokerTransports(&broker_transport, &non_broker_transport);

  {
    // This listener is scoped such that it closes the broker transport after
    // the non-broker issues its ConnectNode(). This should trigger a rejection
    // and ultimately portal closure by the non-broker.
    test::TestTransportListener broker_listener(broker, broker_transport);
    broker_listener.DiscardMessages<msg::ConnectFromNonBrokerToBroker>();

    ASSERT_EQ(
        IPCZ_RESULT_OK,
        ipcz().ConnectNode(non_broker, non_broker_transport, 1,
                           IPCZ_CONNECT_NODE_TO_BROKER, nullptr, &portal));
  }

  EXPECT_EQ(IPCZ_RESULT_OK,
            WaitForConditionFlags(portal, IPCZ_TRAP_PEER_CLOSED));

  CloseAll({portal, non_broker, broker});
}

TEST_P(ConnectTest, DisconnectOnBadMessage) {
  IpczHandle broker = CreateBrokerNode();
  IpczHandle non_broker = CreateNonBrokerNode();

  IpczDriverHandle broker_transport, non_broker_transport;
  CreateBrokerToNonBrokerTransports(&broker_transport, &non_broker_transport);

  // First fail to connect a broker.
  IpczHandle portal;
  ASSERT_EQ(IPCZ_RESULT_OK,
            ipcz().ConnectNode(broker, broker_transport, 1, IPCZ_NO_FLAGS,
                               nullptr, &portal));

  test::TestTransportListener non_broker_listener(non_broker,
                                                  non_broker_transport);
  non_broker_listener.DiscardMessages<msg::ConnectFromBrokerToNonBroker>();

  const char kBadMessage[] = "this will never be a valid handshake message!";
  EXPECT_EQ(IPCZ_RESULT_OK,
            GetDriver().Transmit(non_broker_transport, kBadMessage,
                                 std::size(kBadMessage), nullptr, 0,
                                 IPCZ_NO_FLAGS, nullptr));

  EXPECT_EQ(IPCZ_RESULT_OK,
            WaitForConditionFlags(portal, IPCZ_TRAP_PEER_CLOSED));

  non_broker_listener.StopListening();
  Close(portal);

  // Next fail to connect a non-broker.
  CreateBrokerToNonBrokerTransports(&broker_transport, &non_broker_transport);

  test::TestTransportListener broker_listener(broker, broker_transport);
  broker_listener.DiscardMessages<msg::ConnectFromNonBrokerToBroker>();

  ASSERT_EQ(IPCZ_RESULT_OK,
            ipcz().ConnectNode(non_broker, non_broker_transport, 1,
                               IPCZ_CONNECT_NODE_TO_BROKER, nullptr, &portal));
  EXPECT_EQ(IPCZ_RESULT_OK,
            GetDriver().Transmit(broker_transport, kBadMessage,
                                 std::size(kBadMessage), nullptr, 0,
                                 IPCZ_NO_FLAGS, nullptr));
  EXPECT_EQ(IPCZ_RESULT_OK,
            WaitForConditionFlags(portal, IPCZ_TRAP_PEER_CLOSED));

  broker_listener.StopListening();
  CloseAll({portal, non_broker, broker});
}

INSTANTIATE_MULTINODE_TEST_SUITE_P(ConnectTest);

}  // namespace
}  // namespace ipcz
