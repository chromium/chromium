// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/flatland/flatland_sysmem_buffer_manager.h"

#include <fuchsia/sysmem2/cpp/fidl.h>
#include <fuchsia/ui/composition/cpp/fidl.h>
#include <lib/ui/scenic/cpp/testing/fake_flatland.h>
#include <lib/zx/eventpair.h>
#include <zircon/rights.h>

#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_pump_type.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/ozone/platform/flatland/flatland_sysmem_buffer_collection.h"

namespace ui {

namespace {

fuchsia::ui::composition::RegisterBufferCollectionArgs MakeArgs(
    zx::eventpair* import_token_out,
    fidl::InterfaceRequest<fuchsia::sysmem2::BufferCollectionToken>*
        token_request_out) {
  fuchsia::ui::composition::RegisterBufferCollectionArgs args;

  fuchsia::ui::composition::BufferCollectionExportToken export_token;
  zx::eventpair::create(0, &export_token.value, import_token_out);
  args.set_export_token(std::move(export_token));

  fidl::InterfaceHandle<fuchsia::sysmem2::BufferCollectionToken> token;
  *token_request_out = token.NewRequest();
  args.set_buffer_collection_token2(std::move(token));

  args.set_usage(
      fuchsia::ui::composition::RegisterBufferCollectionUsage::DEFAULT);
  return args;
}

}  // namespace

class FlatlandSysmemBufferManagerTest : public ::testing::Test {
 protected:
  FlatlandSysmemBufferManagerTest()
      : manager_(/*flatland_surface_factory=*/nullptr) {}

  void SetUp() override {
    // The sysmem allocator handle is not served; one-way calls made during
    // Initialize() are simply buffered on the channel.
    fuchsia::sysmem2::AllocatorHandle sysmem_handle;
    sysmem_request_ = sysmem_handle.NewRequest();

    fuchsia::ui::composition::AllocatorHandle flatland_handle;
    fake_flatland_.GetAllocatorRequestHandler()(flatland_handle.NewRequest());

    manager_.Initialize(std::move(sysmem_handle), std::move(flatland_handle));
  }

  void RegisterWithFlatlandAllocator(
      fuchsia::ui::composition::RegisterBufferCollectionArgs args) {
    manager_.RegisterWithFlatlandAllocator(std::move(args));
  }

  bool RegisterCollection(
      scoped_refptr<FlatlandSysmemBufferCollection> collection) {
    auto id = collection->id();
    auto* collection_ptr = collection.get();
    {
      base::AutoLock auto_lock(manager_.collections_lock_);
      manager_.collections_[id] = nullptr;
    }
    manager_.RegisterCollection(std::move(collection));
    base::AutoLock auto_lock(manager_.collections_lock_);
    auto it = manager_.collections_.find(id);
    return it != manager_.collections_.end() &&
           it->second.get() == collection_ptr;
  }

  void RegisterCollectionDirect(
      scoped_refptr<FlatlandSysmemBufferCollection> collection) {
    manager_.RegisterCollection(std::move(collection));
  }

  void ClearCollections() {
    base::AutoLock auto_lock(manager_.collections_lock_);
    manager_.collections_.clear();
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  scenic::FakeFlatland fake_flatland_;
  FlatlandSysmemBufferManager manager_{nullptr};

  fidl::InterfaceRequest<fuchsia::sysmem2::Allocator> sysmem_request_;
};

TEST_F(FlatlandSysmemBufferManagerTest, RegisterCollectionRejectsDuplicateId) {
  zx::eventpair peer;
  zx::eventpair service;
  ASSERT_EQ(zx::eventpair::create(0, &peer, &service), ZX_OK);

  zx::eventpair service_dup;
  ASSERT_EQ(service.duplicate(ZX_RIGHT_SAME_RIGHTS, &service_dup), ZX_OK);

  auto collection_a = base::MakeRefCounted<FlatlandSysmemBufferCollection>();
  collection_a->InitializeForTesting(std::move(service),
                                     NativePixmapBufferUsage::kScanout);

  auto collection_b = base::MakeRefCounted<FlatlandSysmemBufferCollection>();
  collection_b->InitializeForTesting(std::move(service_dup),
                                     NativePixmapBufferUsage::kScanout);

  ASSERT_EQ(collection_a->id(), collection_b->id());

  EXPECT_TRUE(RegisterCollection(collection_a));
  EXPECT_DEATH(RegisterCollectionDirect(collection_b), "");

  // The first registered collection must remain in the registry; a second
  // collection with the same id must not displace it.
  EXPECT_EQ(manager_.GetCollectionByHandle(peer), collection_a);

  // The manager must still hold a reference to the first collection.
  collection_b.reset();
  EXPECT_FALSE(collection_a->HasOneRef());

  ClearCollections();
}

TEST_F(FlatlandSysmemBufferManagerTest, RegisterBufferCollectionOnBoundThread) {
  zx::eventpair import_token;
  fidl::InterfaceRequest<fuchsia::sysmem2::BufferCollectionToken> token_request;

  RegisterWithFlatlandAllocator(MakeArgs(&import_token, &token_request));

  EXPECT_EQ(fake_flatland_.graph_bindings().buffer_collections.size(), 0U);
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return fake_flatland_.graph_bindings().buffer_collections.size() == 1U;
  }));
}

TEST_F(FlatlandSysmemBufferManagerTest,
       RegisterBufferCollectionFromAnotherThread) {
  zx::eventpair import_token;
  fidl::InterfaceRequest<fuchsia::sysmem2::BufferCollectionToken> token_request;
  auto args = MakeArgs(&import_token, &token_request);

  base::Thread other_thread("FlatlandSysmemBufferManagerTest");
  ASSERT_TRUE(other_thread.StartWithOptions(
      base::Thread::Options(base::MessagePumpType::IO, 0)));

  base::WaitableEvent registered;
  other_thread.task_runner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        RegisterWithFlatlandAllocator(std::move(args));
        registered.Signal();
      }));
  registered.Wait();

  // The registration must be dispatched on the thread that bound the Flatland
  // allocator, so it is queued until the bound thread runs.
  EXPECT_EQ(fake_flatland_.graph_bindings().buffer_collections.size(), 0U);
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return fake_flatland_.graph_bindings().buffer_collections.size() == 1U;
  }));

  other_thread.Stop();
}

}  // namespace ui
