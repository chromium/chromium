// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_TEST_TEST_BASE_H_
#define IPCZ_SRC_TEST_TEST_BASE_H_

#include "ipcz/ipcz.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ipcz::test {

class TestBase : public testing::Test {
 public:
  TestBase();
  ~TestBase() override;

  IpczAPI ipcz = {.size = sizeof(ipcz)};
};

}  // namespace ipcz::test

#endif  // IPCZ_SRC_TEST_TEST_BASE_H_
