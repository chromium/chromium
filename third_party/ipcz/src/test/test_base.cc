// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/test_base.h"

#include <chrono>
#include <thread>

#include "api.h"
#include "ipcz/ipcz.h"
#include "ipcz/router.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/synchronization/notification.h"
#include "util/ref_counted.h"

namespace ipcz::test::internal {

namespace {

void CreateNodeChecked(const IpczAPI& ipcz,
                       const IpczDriver& driver,
                       IpczCreateNodeFlags flags,
                       IpczHandle& handle) {
  const IpczResult result = ipcz.CreateNode(&driver, flags, nullptr, &handle);
  ASSERT_EQ(IPCZ_RESULT_OK, result);
}

void OpenPortalsChecked(const IpczAPI& ipcz,
                        IpczHandle node,
                        IpczHandle& first,
                        IpczHandle& second) {
  const IpczResult result =
      ipcz.OpenPortals(node, IPCZ_NO_FLAGS, nullptr, &first, &second);
  ASSERT_EQ(IPCZ_RESULT_OK, result);
}

}  // namespace

TestBase::TestBase() {
  IpczGetAPI(&ipcz_);
}

TestBase::~TestBase() = default;

void TestBase::Close(IpczHandle handle) {
  ASSERT_EQ(IPCZ_RESULT_OK, ipcz().Close(handle, IPCZ_NO_FLAGS, nullptr));
}

void TestBase::CloseAll(absl::Span<const IpczHandle> handles) {
  for (IpczHandle handle : handles) {
    Close(handle);
  }
}

IpczResult TestBase::Merge(IpczHandle a, IpczHandle b) {
  return ipcz().MergePortals(a, b, IPCZ_NO_FLAGS, nullptr);
}

IpczHandle TestBase::CreateNode(const IpczDriver& driver,
                                IpczCreateNodeFlags flags) {
  IpczHandle node;
  CreateNodeChecked(ipcz(), driver, flags, node);
  return node;
}

std::pair<IpczHandle, IpczHandle> TestBase::OpenPortals(IpczHandle node) {
  IpczHandle a, b;
  OpenPortalsChecked(ipcz(), node, a, b);
  return {a, b};
}

IpczResult TestBase::Put(IpczHandle portal,
                         std::string_view message,
                         absl::Span<IpczHandle> handles) {
  return ipcz().Put(portal, message.data(), message.size(), handles.data(),
                    handles.size(), IPCZ_NO_FLAGS, nullptr);
}

IpczResult TestBase::Get(IpczHandle portal,
                         std::string* message,
                         absl::Span<IpczHandle> handles) {
  if (message) {
    message->clear();
  }

  size_t num_bytes = 0;
  IpczHandle* handle_storage = handles.empty() ? nullptr : handles.data();
  size_t num_handles = handles.size();
  IpczResult result =
      ipcz().Get(portal, IPCZ_NO_FLAGS, nullptr, nullptr, &num_bytes,
                 handle_storage, &num_handles, nullptr);
  if (result != IPCZ_RESULT_RESOURCE_EXHAUSTED) {
    return result;
  }

  void* data_storage = nullptr;
  if (message) {
    message->resize(num_bytes);
    data_storage = message->data();
  }

  return ipcz().Get(portal, IPCZ_NO_FLAGS, nullptr, data_storage, &num_bytes,
                    handle_storage, &num_handles, nullptr);
}

IpczResult TestBase::Trap(IpczHandle portal,
                          const IpczTrapConditions& conditions,
                          TrapEventHandler fn,
                          IpczTrapConditionFlags* flags,
                          IpczPortalStatus* status) {
  auto handler = std::make_unique<TrapEventHandler>(std::move(fn));
  auto context = reinterpret_cast<uintptr_t>(handler.get());

  // For convenience, set the `size` field correctly so callers don't have to.
  IpczTrapConditions sized_conditions = conditions;
  sized_conditions.size = sizeof(sized_conditions);

  const IpczResult result =
      ipcz().Trap(portal, &sized_conditions, &HandleEvent, context,
                  IPCZ_NO_FLAGS, nullptr, flags, status);
  if (result == IPCZ_RESULT_OK) {
    std::ignore = handler.release();
  }
  return result;
}

IpczResult TestBase::WaitForConditions(IpczHandle portal,
                                       const IpczTrapConditions& conditions) {
  absl::Notification notification;
  const IpczResult result = Trap(
      portal, conditions, [&](const IpczTrapEvent&) { notification.Notify(); });

  switch (result) {
    case IPCZ_RESULT_OK:
      notification.WaitForNotification();
      return IPCZ_RESULT_OK;

    case IPCZ_RESULT_FAILED_PRECONDITION:
      return IPCZ_RESULT_OK;

    default:
      return result;
  }
}

IpczResult TestBase::WaitForConditionFlags(IpczHandle portal,
                                           IpczTrapConditionFlags flags) {
  const IpczTrapConditions conditions = {
      .size = sizeof(conditions),
      .flags = flags,
  };
  return WaitForConditions(portal, conditions);
}

IpczResult TestBase::WaitToGet(IpczHandle portal,
                               std::string* message,
                               absl::Span<IpczHandle> handles) {
  const IpczTrapConditions conditions = {
      .size = sizeof(conditions),
      .flags = IPCZ_TRAP_ABOVE_MIN_LOCAL_PARCELS,
      .min_local_parcels = 0,
  };
  IpczResult result = WaitForConditions(portal, conditions);
  if (result != IPCZ_RESULT_OK) {
    return result;
  }

  return Get(portal, message, handles);
}

std::string TestBase::WaitToGetString(IpczHandle portal) {
  std::string message;
  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(portal, &message));
  return message;
}

void TestBase::PingPong(IpczHandle portal) {
  EXPECT_EQ(IPCZ_RESULT_OK, Put(portal, {}));
  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(portal));
}

void TestBase::WaitForPingAndReply(IpczHandle portal) {
  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(portal));
  EXPECT_EQ(IPCZ_RESULT_OK, Put(portal, {}));
}

void TestBase::VerifyEndToEnd(IpczHandle portal) {
  static const char kTestMessage[] = "Ping!!!";
  std::string message;
  EXPECT_EQ(IPCZ_RESULT_OK, Put(portal, kTestMessage));
  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(portal, &message));
  EXPECT_EQ(kTestMessage, message);
}

void TestBase::VerifyEndToEndLocal(IpczHandle a, IpczHandle b) {
  const std::string kMessage1 = "psssst";
  const std::string kMessage2 = "ssshhh";

  Put(a, kMessage1);
  Put(b, kMessage2);

  std::string message;
  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(a, &message));
  EXPECT_EQ(kMessage2, message);
  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(b, &message));
  EXPECT_EQ(kMessage1, message);
}

void TestBase::WaitForDirectRemoteLink(IpczHandle portal) {
  Router* const router = Router::FromHandle(portal);
  while (!router->IsOnCentralRemoteLink()) {
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(8ms);
  }

  const std::string kMessage = "very direct wow";
  EXPECT_EQ(IPCZ_RESULT_OK, Put(portal, kMessage));

  std::string message;
  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(portal, &message));
  EXPECT_EQ(kMessage, message);
}

void TestBase::WaitForDirectLocalLink(IpczHandle a, IpczHandle b) {
  Router* const router_a = Router::FromHandle(a);
  Router* const router_b = Router::FromHandle(b);
  while (!router_a->HasLocalPeer(*router_b) &&
         !router_b->HasLocalPeer(*router_a)) {
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(8ms);
  }
}

void TestBase::HandleEvent(const IpczTrapEvent* event) {
  auto handler =
      absl::WrapUnique(reinterpret_cast<TrapEventHandler*>(event->context));
  (*handler)(*event);
}

}  // namespace ipcz::test::internal
