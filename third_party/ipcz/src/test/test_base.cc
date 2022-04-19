// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/test_base.h"

#include "api.h"
#include "ipcz/ipcz.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ipcz::test {

TestBase::TestBase() {
  IpczGetAPI(&ipcz_);
}

TestBase::~TestBase() = default;

}  // namespace ipcz::test
