// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/ipcz.h"
#include "test/test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ipcz {
namespace {

using APITest = test::TestBase;

TEST_F(APITest, Unimplemented) {
  EXPECT_EQ(IPCZ_RESULT_UNIMPLEMENTED,
            ipcz.Close(IPCZ_INVALID_HANDLE, IPCZ_NO_FLAGS, nullptr));
  EXPECT_EQ(IPCZ_RESULT_UNIMPLEMENTED,
            ipcz.CreateNode(nullptr, IPCZ_INVALID_DRIVER_HANDLE, IPCZ_NO_FLAGS,
                            nullptr, nullptr));
  EXPECT_EQ(IPCZ_RESULT_UNIMPLEMENTED,
            ipcz.ConnectNode(IPCZ_INVALID_HANDLE, IPCZ_INVALID_DRIVER_HANDLE, 0,
                             IPCZ_NO_FLAGS, nullptr, nullptr));
  EXPECT_EQ(IPCZ_RESULT_UNIMPLEMENTED,
            ipcz.OpenPortals(IPCZ_INVALID_HANDLE, IPCZ_NO_FLAGS, nullptr,
                             nullptr, nullptr));
  EXPECT_EQ(IPCZ_RESULT_UNIMPLEMENTED,
            ipcz.MergePortals(IPCZ_INVALID_HANDLE, IPCZ_INVALID_HANDLE,
                              IPCZ_NO_FLAGS, nullptr));
  EXPECT_EQ(IPCZ_RESULT_UNIMPLEMENTED,
            ipcz.QueryPortalStatus(IPCZ_INVALID_HANDLE, IPCZ_NO_FLAGS, nullptr,
                                   nullptr));
  EXPECT_EQ(IPCZ_RESULT_UNIMPLEMENTED,
            ipcz.Put(IPCZ_INVALID_HANDLE, nullptr, 0, nullptr, 0, IPCZ_NO_FLAGS,
                     nullptr));
  EXPECT_EQ(IPCZ_RESULT_UNIMPLEMENTED,
            ipcz.BeginPut(IPCZ_INVALID_HANDLE, IPCZ_NO_FLAGS, nullptr, nullptr,
                          nullptr));
  EXPECT_EQ(
      IPCZ_RESULT_UNIMPLEMENTED,
      ipcz.EndPut(IPCZ_INVALID_HANDLE, 0, nullptr, 0, IPCZ_NO_FLAGS, nullptr));
  EXPECT_EQ(IPCZ_RESULT_UNIMPLEMENTED,
            ipcz.Get(IPCZ_INVALID_HANDLE, IPCZ_NO_FLAGS, nullptr, nullptr,
                     nullptr, nullptr, nullptr));
  EXPECT_EQ(IPCZ_RESULT_UNIMPLEMENTED,
            ipcz.BeginGet(IPCZ_INVALID_HANDLE, IPCZ_NO_FLAGS, nullptr, nullptr,
                          nullptr, nullptr));
  EXPECT_EQ(
      IPCZ_RESULT_UNIMPLEMENTED,
      ipcz.EndGet(IPCZ_INVALID_HANDLE, 0, 0, IPCZ_NO_FLAGS, nullptr, nullptr));
  EXPECT_EQ(IPCZ_RESULT_UNIMPLEMENTED,
            ipcz.Trap(IPCZ_INVALID_HANDLE, nullptr, nullptr, 0, IPCZ_NO_FLAGS,
                      nullptr, nullptr, nullptr));
  EXPECT_EQ(IPCZ_RESULT_UNIMPLEMENTED,
            ipcz.Box(IPCZ_INVALID_HANDLE, IPCZ_INVALID_DRIVER_HANDLE,
                     IPCZ_NO_FLAGS, nullptr, nullptr));
  EXPECT_EQ(IPCZ_RESULT_UNIMPLEMENTED,
            ipcz.Unbox(IPCZ_INVALID_HANDLE, IPCZ_NO_FLAGS, nullptr, nullptr));
}

}  // namespace
}  // namespace ipcz
