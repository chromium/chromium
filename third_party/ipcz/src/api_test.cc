// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
            ipcz().Put(IPCZ_INVALID_HANDLE, nullptr, 0, nullptr, 0,
                       IPCZ_NO_FLAGS, nullptr));
  EXPECT_EQ(IPCZ_RESULT_UNIMPLEMENTED,
            ipcz().BeginPut(IPCZ_INVALID_HANDLE, IPCZ_NO_FLAGS, nullptr,
                            nullptr, nullptr));
  EXPECT_EQ(IPCZ_RESULT_UNIMPLEMENTED,
            ipcz().EndPut(IPCZ_INVALID_HANDLE, 0, nullptr, 0, IPCZ_NO_FLAGS,
                          nullptr));
  EXPECT_EQ(IPCZ_RESULT_UNIMPLEMENTED,
            ipcz().Get(IPCZ_INVALID_HANDLE, IPCZ_NO_FLAGS, nullptr, nullptr,
                       nullptr, nullptr, nullptr));
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

  EXPECT_EQ(IPCZ_RESULT_OK, ipcz().Close(a, IPCZ_NO_FLAGS, nullptr));
  EXPECT_EQ(IPCZ_RESULT_OK, ipcz().Close(b, IPCZ_NO_FLAGS, nullptr));
  EXPECT_EQ(IPCZ_RESULT_OK, ipcz().Close(node, IPCZ_NO_FLAGS, nullptr));
}
}  // namespace
}  // namespace ipcz
