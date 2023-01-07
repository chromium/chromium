// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/stack_trace.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "util/log.h"

namespace ipcz {
namespace {

TEST(StackTraceTest, SmokeTest) {
  LOG(INFO) << StackTrace().ToString();
}

}  // namespace
}  // namespace ipcz
