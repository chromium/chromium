// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ipcz/ipcz.h"
#include "reference_drivers/single_process_reference_driver.h"
#include "test/test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ipcz {
namespace {

const IpczDriver& kDefaultDriver =
    reference_drivers::kSingleProcessReferenceDriver;

using APITest = test::TestBase;

TEST_F(APITest, Unimplemented) {
  EXPECT_EQ(IPCZ_RESULT_UNIMPLEMENTED,
            ipcz().ConnectNode(IPCZ_INVALID_HANDLE, IPCZ_INVALID_DRIVER_HANDLE,
                               0, IPCZ_NO_FLAGS, nullptr, nullptr));
  EXPECT_EQ(IPCZ_RESULT_UNIMPLEMENTED,
            ipcz().MergePortals(IPCZ_INVALID_HANDLE, IPCZ_INVALID_HANDLE,
                                IPCZ_NO_FLAGS, nullptr));

  EXPECT_EQ(IPCZ_RESULT_UNIMPLEMENTED,
            ipcz().BeginPut(IPCZ_INVALID_HANDLE, IPCZ_NO_FLAGS, nullptr,
                            nullptr, nullptr));
  EXPECT_EQ(IPCZ_RESULT_UNIMPLEMENTED,
            ipcz().EndPut(IPCZ_INVALID_HANDLE, 0, nullptr, 0, IPCZ_NO_FLAGS,
                          nullptr));
  EXPECT_EQ(IPCZ_RESULT_UNIMPLEMENTED,
            ipcz().BeginGet(IPCZ_INVALID_HANDLE, IPCZ_NO_FLAGS, nullptr,
                            nullptr, nullptr, nullptr));
  EXPECT_EQ(IPCZ_RESULT_UNIMPLEMENTED,
            ipcz().EndGet(IPCZ_INVALID_HANDLE, 0, 0, IPCZ_NO_FLAGS, nullptr,
                          nullptr));
  EXPECT_EQ(IPCZ_RESULT_UNIMPLEMENTED,
            ipcz().Trap(IPCZ_INVALID_HANDLE, nullptr, nullptr, 0, IPCZ_NO_FLAGS,
                        nullptr, nullptr, nullptr));
  EXPECT_EQ(IPCZ_RESULT_UNIMPLEMENTED,
            ipcz().Box(IPCZ_INVALID_HANDLE, IPCZ_INVALID_DRIVER_HANDLE,
                       IPCZ_NO_FLAGS, nullptr, nullptr));
  EXPECT_EQ(IPCZ_RESULT_UNIMPLEMENTED,
            ipcz().Unbox(IPCZ_INVALID_HANDLE, IPCZ_NO_FLAGS, nullptr, nullptr));
}

TEST_F(APITest, CloseInvalid) {
  EXPECT_EQ(IPCZ_RESULT_INVALID_ARGUMENT,
            ipcz().Close(IPCZ_INVALID_HANDLE, IPCZ_NO_FLAGS, nullptr));
}

TEST_F(APITest, CreateNodeInvalid) {
  IpczHandle node;

  // Null driver.
  EXPECT_EQ(IPCZ_RESULT_INVALID_ARGUMENT,
            ipcz().CreateNode(nullptr, IPCZ_INVALID_DRIVER_HANDLE,
                              IPCZ_NO_FLAGS, nullptr, &node));

  // Null output handle.
  EXPECT_EQ(IPCZ_RESULT_INVALID_ARGUMENT,
            ipcz().CreateNode(&kDefaultDriver, IPCZ_INVALID_DRIVER_HANDLE,
                              IPCZ_NO_FLAGS, nullptr, nullptr));
}

TEST_F(APITest, CreateNode) {
  IpczHandle node;
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().CreateNode(&kDefaultDriver, IPCZ_INVALID_DRIVER_HANDLE,
                              IPCZ_NO_FLAGS, nullptr, &node));
  EXPECT_EQ(IPCZ_RESULT_OK, ipcz().Close(node, IPCZ_NO_FLAGS, nullptr));
}

TEST_F(APITest, OpenPortalsInvalid) {
  IpczHandle node;
  ipcz().CreateNode(&kDefaultDriver, IPCZ_INVALID_DRIVER_HANDLE, IPCZ_NO_FLAGS,
                    nullptr, &node);

  IpczHandle a, b;

  // Invalid node.
  EXPECT_EQ(
      IPCZ_RESULT_INVALID_ARGUMENT,
      ipcz().OpenPortals(IPCZ_INVALID_HANDLE, IPCZ_NO_FLAGS, nullptr, &a, &b));

  // Invalid portal handle(s).
  EXPECT_EQ(IPCZ_RESULT_INVALID_ARGUMENT,
            ipcz().OpenPortals(node, IPCZ_NO_FLAGS, nullptr, nullptr, &b));
  EXPECT_EQ(IPCZ_RESULT_INVALID_ARGUMENT,
            ipcz().OpenPortals(node, IPCZ_NO_FLAGS, nullptr, &a, nullptr));
  EXPECT_EQ(IPCZ_RESULT_INVALID_ARGUMENT,
            ipcz().OpenPortals(node, IPCZ_NO_FLAGS, nullptr, nullptr, nullptr));

  ipcz().Close(node, IPCZ_NO_FLAGS, nullptr);
}

TEST_F(APITest, OpenPortals) {
  IpczHandle node;
  ipcz().CreateNode(&kDefaultDriver, IPCZ_INVALID_DRIVER_HANDLE, IPCZ_NO_FLAGS,
                    nullptr, &node);

  IpczHandle a, b;
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().OpenPortals(node, IPCZ_NO_FLAGS, nullptr, &a, &b));
  EXPECT_EQ(IPCZ_RESULT_OK, ipcz().Close(a, IPCZ_NO_FLAGS, nullptr));
  EXPECT_EQ(IPCZ_RESULT_OK, ipcz().Close(b, IPCZ_NO_FLAGS, nullptr));
  EXPECT_EQ(IPCZ_RESULT_OK, ipcz().Close(node, IPCZ_NO_FLAGS, nullptr));
}

TEST_F(APITest, QueryPortalStatusInvalid) {
  IpczHandle node;
  ipcz().CreateNode(&kDefaultDriver, IPCZ_INVALID_DRIVER_HANDLE, IPCZ_NO_FLAGS,
                    nullptr, &node);
  IpczHandle a, b;
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().OpenPortals(node, IPCZ_NO_FLAGS, nullptr, &a, &b));

  // Null portal.
  IpczPortalStatus status = {.size = sizeof(status)};
  EXPECT_EQ(IPCZ_RESULT_INVALID_ARGUMENT,
            ipcz().QueryPortalStatus(IPCZ_INVALID_HANDLE, IPCZ_NO_FLAGS,
                                     nullptr, &status));

  // Not a portal.
  EXPECT_EQ(IPCZ_RESULT_INVALID_ARGUMENT,
            ipcz().QueryPortalStatus(node, IPCZ_NO_FLAGS, nullptr, &status));

  // Null output status.
  EXPECT_EQ(IPCZ_RESULT_INVALID_ARGUMENT,
            ipcz().QueryPortalStatus(a, IPCZ_NO_FLAGS, nullptr, nullptr));

  // Invalid status size.
  status.size = 0;
  EXPECT_EQ(IPCZ_RESULT_INVALID_ARGUMENT,
            ipcz().QueryPortalStatus(a, IPCZ_NO_FLAGS, nullptr, &status));

  EXPECT_EQ(IPCZ_RESULT_OK, ipcz().Close(a, IPCZ_NO_FLAGS, nullptr));
  EXPECT_EQ(IPCZ_RESULT_OK, ipcz().Close(b, IPCZ_NO_FLAGS, nullptr));
  EXPECT_EQ(IPCZ_RESULT_OK, ipcz().Close(node, IPCZ_NO_FLAGS, nullptr));
}

TEST_F(APITest, QueryPortalStatus) {
  IpczHandle node;
  ipcz().CreateNode(&kDefaultDriver, IPCZ_INVALID_DRIVER_HANDLE, IPCZ_NO_FLAGS,
                    nullptr, &node);
  IpczHandle a, b;
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().OpenPortals(node, IPCZ_NO_FLAGS, nullptr, &a, &b));

  IpczPortalStatus status = {.size = sizeof(status)};
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().QueryPortalStatus(a, IPCZ_NO_FLAGS, nullptr, &status));
  EXPECT_EQ(0u, status.flags & IPCZ_PORTAL_STATUS_PEER_CLOSED);
  EXPECT_EQ(0u, status.flags & IPCZ_PORTAL_STATUS_DEAD);
  EXPECT_EQ(0u, status.num_local_parcels);
  EXPECT_EQ(0u, status.num_local_bytes);
  EXPECT_EQ(0u, status.num_remote_parcels);
  EXPECT_EQ(0u, status.num_remote_bytes);

  EXPECT_EQ(IPCZ_RESULT_OK, ipcz().Close(b, IPCZ_NO_FLAGS, nullptr));
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().QueryPortalStatus(a, IPCZ_NO_FLAGS, nullptr, &status));
  EXPECT_EQ(IPCZ_PORTAL_STATUS_PEER_CLOSED,
            status.flags & IPCZ_PORTAL_STATUS_PEER_CLOSED);
  EXPECT_EQ(IPCZ_PORTAL_STATUS_DEAD, status.flags & IPCZ_PORTAL_STATUS_DEAD);

  EXPECT_EQ(IPCZ_RESULT_OK, ipcz().Close(a, IPCZ_NO_FLAGS, nullptr));
  EXPECT_EQ(IPCZ_RESULT_OK, ipcz().Close(node, IPCZ_NO_FLAGS, nullptr));
}

TEST_F(APITest, PutGet) {
  IpczHandle node;
  ipcz().CreateNode(&kDefaultDriver, IPCZ_INVALID_DRIVER_HANDLE, IPCZ_NO_FLAGS,
                    nullptr, &node);
  IpczHandle a, b;
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().OpenPortals(node, IPCZ_NO_FLAGS, nullptr, &a, &b));

  // Get from an empty portal.
  char data[4];
  size_t num_bytes = 4;
  EXPECT_EQ(IPCZ_RESULT_UNAVAILABLE, ipcz().Get(b, IPCZ_NO_FLAGS, nullptr, data,
                                                &num_bytes, nullptr, nullptr));

  // A portal can't transfer itself or its peer.
  EXPECT_EQ(IPCZ_RESULT_INVALID_ARGUMENT,
            ipcz().Put(a, nullptr, 0, &a, 1, IPCZ_NO_FLAGS, nullptr));
  EXPECT_EQ(IPCZ_RESULT_INVALID_ARGUMENT,
            ipcz().Put(a, nullptr, 0, &b, 1, IPCZ_NO_FLAGS, nullptr));

  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().Put(a, "hi", 2, nullptr, 0, IPCZ_NO_FLAGS, nullptr));
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().Put(a, "bye", 3, nullptr, 0, IPCZ_NO_FLAGS, nullptr));
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().Put(a, nullptr, 0, nullptr, 0, IPCZ_NO_FLAGS, nullptr));

  IpczHandle c, d;
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().OpenPortals(node, IPCZ_NO_FLAGS, nullptr, &c, &d));
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().Put(a, nullptr, 0, &d, 1, IPCZ_NO_FLAGS, nullptr));
  d = IPCZ_INVALID_HANDLE;

  IpczPortalStatus status = {.size = sizeof(status)};
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().QueryPortalStatus(b, IPCZ_NO_FLAGS, nullptr, &status));
  EXPECT_EQ(4u, status.num_local_parcels);
  EXPECT_EQ(5u, status.num_local_bytes);

  // Insufficient data storage.
  num_bytes = 0;
  EXPECT_EQ(IPCZ_RESULT_RESOURCE_EXHAUSTED,
            ipcz().Get(b, IPCZ_NO_FLAGS, nullptr, data, &num_bytes, nullptr,
                       nullptr));
  EXPECT_EQ(2u, num_bytes);

  num_bytes = 4;
  EXPECT_EQ(IPCZ_RESULT_OK, ipcz().Get(b, IPCZ_NO_FLAGS, nullptr, data,
                                       &num_bytes, nullptr, nullptr));
  EXPECT_EQ(2u, num_bytes);
  EXPECT_EQ("hi", std::string(data, 2));

  num_bytes = 4;
  EXPECT_EQ(IPCZ_RESULT_OK, ipcz().Get(b, IPCZ_NO_FLAGS, nullptr, data,
                                       &num_bytes, nullptr, nullptr));
  EXPECT_EQ(3u, num_bytes);
  EXPECT_EQ("bye", std::string(data, 3));

  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().QueryPortalStatus(b, IPCZ_NO_FLAGS, nullptr, &status));
  EXPECT_EQ(2u, status.num_local_parcels);
  EXPECT_EQ(0u, status.num_local_bytes);

  // Getting an empty parcel requires no storage.
  EXPECT_EQ(IPCZ_RESULT_OK, ipcz().Get(b, IPCZ_NO_FLAGS, nullptr, nullptr,
                                       nullptr, nullptr, nullptr));
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().QueryPortalStatus(b, IPCZ_NO_FLAGS, nullptr, &status));
  EXPECT_EQ(1u, status.num_local_parcels);
  EXPECT_EQ(0u, status.num_local_bytes);

  // Insufficient handle storage.
  EXPECT_EQ(IPCZ_RESULT_RESOURCE_EXHAUSTED,
            ipcz().Get(b, IPCZ_NO_FLAGS, nullptr, nullptr, nullptr, nullptr,
                       nullptr));

  size_t num_handles = 1;
  EXPECT_EQ(IPCZ_RESULT_OK, ipcz().Get(b, IPCZ_NO_FLAGS, nullptr, nullptr,
                                       nullptr, &d, &num_handles));
  EXPECT_EQ(1u, num_handles);
  EXPECT_EQ(IPCZ_RESULT_OK, ipcz().Close(d, IPCZ_NO_FLAGS, nullptr));

  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().QueryPortalStatus(c, IPCZ_NO_FLAGS, nullptr, &status));
  EXPECT_EQ(IPCZ_PORTAL_STATUS_PEER_CLOSED,
            status.flags & IPCZ_PORTAL_STATUS_PEER_CLOSED);
  EXPECT_EQ(IPCZ_PORTAL_STATUS_DEAD, status.flags & IPCZ_PORTAL_STATUS_DEAD);

  EXPECT_EQ(IPCZ_RESULT_OK, ipcz().Close(a, IPCZ_NO_FLAGS, nullptr));
  EXPECT_EQ(IPCZ_RESULT_OK, ipcz().Close(b, IPCZ_NO_FLAGS, nullptr));
  EXPECT_EQ(IPCZ_RESULT_OK, ipcz().Close(c, IPCZ_NO_FLAGS, nullptr));
  EXPECT_EQ(IPCZ_RESULT_OK, ipcz().Close(node, IPCZ_NO_FLAGS, nullptr));
}

}  // namespace
}  // namespace ipcz
