// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/test_base.h"

#include "api.h"
#include "ipcz/ipcz.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ipcz::test {

namespace {

void CreateNodeChecked(const IpczAPI& ipcz,
                       const IpczDriver& driver,
                       IpczCreateNodeFlags flags,
                       IpczHandle& handle) {
  const IpczResult result = ipcz.CreateNode(&driver, IPCZ_INVALID_DRIVER_HANDLE,
                                            flags, nullptr, &handle);
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

void TestBase::CloseAll(const std::vector<IpczHandle>& handles) {
  for (IpczHandle handle : handles) {
    Close(handle);
  }
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
                         std::vector<IpczHandle>* handles) {
  if (message) {
    message->clear();
  }
  if (handles) {
    handles->clear();
  }

  size_t num_bytes = 0;
  size_t num_handles = 0;
  IpczResult result = ipcz().Get(portal, IPCZ_NO_FLAGS, nullptr, nullptr,
                                 &num_bytes, nullptr, &num_handles);
  if (result != IPCZ_RESULT_RESOURCE_EXHAUSTED) {
    return result;
  }

  void* data_storage = nullptr;
  IpczHandle* handle_storage = nullptr;
  if (message) {
    message->resize(num_bytes);
    data_storage = message->data();
  }
  if (handles) {
    handles->resize(num_handles);
    handle_storage = handles->data();
  }

  return ipcz().Get(portal, IPCZ_NO_FLAGS, nullptr, data_storage, &num_bytes,
                    handle_storage, &num_handles);
}

IpczResult TestBase::Trap(IpczHandle portal,
                          const IpczTrapConditions& conditions,
                          TrapEventHandler fn,
                          IpczTrapConditionFlags* flags,
                          IpczPortalStatus* status) {
  auto handler = std::make_unique<TrapEventHandler>(std::move(fn));
  auto context = reinterpret_cast<uintptr_t>(handler.get());
  const IpczResult result =
      ipcz().Trap(portal, &conditions, &HandleEvent, context, IPCZ_NO_FLAGS,
                  nullptr, flags, status);
  if (result == IPCZ_RESULT_OK) {
    std::ignore = handler.release();
  }
  return result;
}

void TestBase::HandleEvent(const IpczTrapEvent* event) {
  auto handler =
      absl::WrapUnique(reinterpret_cast<TrapEventHandler*>(event->context));
  (*handler)(*event);
}

}  // namespace ipcz::test
