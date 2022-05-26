// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_TEST_TEST_BASE_H_
#define IPCZ_SRC_TEST_TEST_BASE_H_

#include <functional>
#include <string_view>
#include <utility>

#include "ipcz/ipcz.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/span.h"

namespace ipcz::test::internal {

// Base class for ipcz unit tests (see ipcz::test::Test in test.h) and multinode
// test fixtures (see ipcz::test::MultinodeTest in multinode_test.h).
//
// Test fixtures should never derive from this class directly. For unit tests,
// use ipcz::test::Test as a base. For multinode tests, use ipcz::test:TestNode
// as a base for MULTINODE_TEST_NODE() invocations, and use
// ipcz::test::MultinodeTest<T> (where T is a subclass of TestNode) for
// TEST_P() invocations for parameterized multinode test bodies.
class TestBase {
 public:
  using TrapEventHandler = std::function<void(const IpczTrapEvent&)>;

  TestBase();
  ~TestBase();

  const IpczAPI& ipcz() const { return ipcz_; }

  // Some trivial shorthand methods to access the ipcz API more conveniently.
  void Close(IpczHandle handle);
  void CloseAll(absl::Span<const IpczHandle> handles);
  IpczHandle CreateNode(const IpczDriver& driver,
                        IpczCreateNodeFlags flags = IPCZ_NO_FLAGS);
  std::pair<IpczHandle, IpczHandle> OpenPortals(IpczHandle node);
  IpczResult Put(IpczHandle portal,
                 std::string_view message,
                 absl::Span<IpczHandle> handles = {});

  // Shorthand for ipcz Get() to retrieve the next available parcel from
  // `portal`.If no parcel is available, or any other condition prevents Get()
  // from succeeding, this returns the same result as the ipcz Get() API.
  //
  // Assuming a parcel is available:
  //
  // If the parcel has data, it's stored  as a string in `*message` iff
  // `message` is non-null. Any handles are stored in `handles` if it's large
  // enough to hold all handles in the parcel.
  //
  // If the available parcel has data but `message` is null, or if the parcel
  // carries has more handles than the capacity of `handles`, the parcel is
  // not retrieved, and this returns IPCZ_RESULT_RESOURCE_EXHAUSTED, like the
  // ipcz Get() API itself.
  IpczResult Get(IpczHandle portal,
                 std::string* message = nullptr,
                 absl::Span<IpczHandle> handles = {});

  // Shorthand for the icpz Trap() API with convenient lambda support.
  IpczResult Trap(IpczHandle portal,
                  const IpczTrapConditions& conditions,
                  TrapEventHandler fn,
                  IpczTrapConditionFlags* flags = nullptr,
                  IpczPortalStatus* status = nullptr);

  // Blocks until one or more conditions indicated by `conditions` are met by
  // `portal`. For simple flag-only conditions like peer closure,
  // WaitForConditionFlags() below may be used instead.
  IpczResult WaitForConditions(IpczHandle portal,
                               const IpczTrapConditions& conditions);

  // Blocks until one or more conditions indicated by `flags` are met by
  // `portal`. For parameterized conditions, use WaitForConditions() above.
  IpczResult WaitForConditionFlags(IpczHandle portal,
                                   IpczTrapConditionFlags flags);

  // Waits to receive any parcel on portal.
  IpczResult WaitToGet(IpczHandle portal,
                       std::string* message = nullptr,
                       absl::Span<IpczHandle> handles = {});

  // Sends a parcel from `portal` and expects to receive a parcel back with the
  // same contents. Typical usage is to call this from two different nodes, on
  // a pair of connected portals, in order to verify working communication
  // between them.
  void VerifyEndToEnd(IpczHandle portal);

  // Similar to above, but useful in unit tests, or situations where both
  // portals are local to the same node. In this case, a message is put into
  // both `a` and `b`, and then this waits to read the same message from both.
  void VerifyEndToEndLocal(IpczHandle a, IpczHandle b);

 private:
  static void HandleEvent(const IpczTrapEvent* event);

  IpczAPI ipcz_ = {.size = sizeof(ipcz_)};
};

}  // namespace ipcz::test::internal

#endif  // IPCZ_SRC_TEST_TEST_BASE_H_
