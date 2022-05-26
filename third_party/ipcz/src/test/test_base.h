// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_TEST_TEST_BASE_H_
#define IPCZ_SRC_TEST_TEST_BASE_H_

#include <functional>
#include <string_view>
#include <utility>
#include <vector>

#include "ipcz/ipcz.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/span.h"

namespace ipcz::test::internal {

// Base class for ipcz unit tests (see ipcz::test::Test in test.h) and multinode
// test fixtures (see ipcz::test::MultinodeTest in multinode_test.h).
class TestBase {
 public:
  using TrapEventHandler = std::function<void(const IpczTrapEvent&)>;

  TestBase();
  ~TestBase();

  const IpczAPI& ipcz() const { return ipcz_; }

  // Some shorthand methods to access the ipcz API more conveniently.
  void Close(IpczHandle handle);
  void CloseAll(const std::vector<IpczHandle>& handles);
  IpczHandle CreateNode(const IpczDriver& driver,
                        IpczCreateNodeFlags flags = IPCZ_NO_FLAGS);
  std::pair<IpczHandle, IpczHandle> OpenPortals(IpczHandle node);
  IpczResult Put(IpczHandle portal,
                 std::string_view message,
                 absl::Span<IpczHandle> handles = {});
  IpczResult Get(IpczHandle portal,
                 std::string* message = nullptr,
                 absl::Span<IpczHandle> handles = {});
  IpczResult Trap(IpczHandle portal,
                  const IpczTrapConditions& conditions,
                  TrapEventHandler fn,
                  IpczTrapConditionFlags* flags = nullptr,
                  IpczPortalStatus* status = nullptr);
  IpczResult WaitForConditions(IpczHandle portal,
                               const IpczTrapConditions& conditions);
  IpczResult WaitForConditionFlags(IpczHandle portal,
                                   IpczTrapConditionFlags flags);
  IpczResult WaitToGet(IpczHandle portal,
                       std::string* message = nullptr,
                       absl::Span<IpczHandle> handles = {});

  // Sends a parcel from each of two portals and waits for them to be received
  // by each other.
  void VerifyEndToEnd(IpczHandle a, IpczHandle b);

 private:
  static void HandleEvent(const IpczTrapEvent* event);

  IpczAPI ipcz_ = {.size = sizeof(ipcz_)};
};

}  // namespace ipcz::test::internal

#endif  // IPCZ_SRC_TEST_TEST_BASE_H_
