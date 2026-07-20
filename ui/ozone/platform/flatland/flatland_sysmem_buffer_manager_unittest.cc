// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/flatland/flatland_sysmem_buffer_manager.h"

#include <lib/zx/eventpair.h>
#include <zircon/rights.h>

#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/ozone/platform/flatland/flatland_sysmem_buffer_collection.h"

namespace ui {

class FlatlandSysmemBufferManagerTest : public ::testing::Test {
 protected:
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
  FlatlandSysmemBufferManager manager_{nullptr};
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

}  // namespace ui
