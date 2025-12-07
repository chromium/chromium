// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/tree/widget_view_ax_cache.h"

#include <memory>
#include <utility>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view.h"

namespace views::test {

class WidgetViewAXCacheTest : public testing::Test {
 protected:
  WidgetViewAXCacheTest() = default;
  ~WidgetViewAXCacheTest() override = default;

  WidgetViewAXCache& cache() { return cache_; }

 private:
  WidgetViewAXCache cache_;
};

TEST_F(WidgetViewAXCacheTest, CacheInsertGetRemove) {
  auto v = std::make_unique<View>();

  EXPECT_EQ(cache().Get(v->GetViewAccessibility().GetUniqueId()), nullptr);

  cache().Insert(&v->GetViewAccessibility());
  EXPECT_EQ(cache().Get(v->GetViewAccessibility().GetUniqueId()),
            &v->GetViewAccessibility());

  cache().Remove(v->GetViewAccessibility().GetUniqueId());
  EXPECT_EQ(cache().Get(v->GetViewAccessibility().GetUniqueId()), nullptr);
  EXPECT_FALSE(cache().HasCachedChildren(&v->GetViewAccessibility()));
}

TEST_F(WidgetViewAXCacheTest, ChildSnapshotIsStableUntilCleared) {
  auto parent = std::make_unique<View>();
  auto* a = parent->AddChildView(std::make_unique<View>());
  auto* b = parent->AddChildView(std::make_unique<View>());

  // Take snapshot.
  cache().CacheChildrenIfNeeded(&parent->GetViewAccessibility());

  // Snapshot sees exactly [a, b].
  EXPECT_TRUE(cache().HasCachedChildren(&parent->GetViewAccessibility()));
  EXPECT_EQ(cache().CachedChildCount(&parent->GetViewAccessibility()), 2u);
  EXPECT_EQ(cache().CachedChildAt(&parent->GetViewAccessibility(), 0),
            &a->GetViewAccessibility());
  EXPECT_EQ(cache().CachedChildAt(&parent->GetViewAccessibility(), 1),
            &b->GetViewAccessibility());
  EXPECT_EQ(cache().CachedChildAt(&parent->GetViewAccessibility(), 2), nullptr);

  // Mutate the live view hierarchy after the snapshot, which shouldn't update
  // the snapshot.
  auto removed_b = parent->RemoveChildViewT(b);
  auto* c = parent->AddChildView(std::make_unique<View>());

  // Snapshot must not change until cleared.
  EXPECT_EQ(cache().CachedChildCount(&parent->GetViewAccessibility()), 2u);
  EXPECT_EQ(cache().CachedChildAt(&parent->GetViewAccessibility(), 0),
            &a->GetViewAccessibility());
  EXPECT_EQ(cache().CachedChildAt(&parent->GetViewAccessibility(), 1),
            &removed_b->GetViewAccessibility());
  EXPECT_EQ(cache().CachedChildAt(&parent->GetViewAccessibility(), 2), nullptr);

  // Clear and re-snapshot and validate the current live children are [a, c].
  cache().RemoveFromChildCache(&parent->GetViewAccessibility());
  EXPECT_FALSE(cache().HasCachedChildren(&parent->GetViewAccessibility()));
  cache().CacheChildrenIfNeeded(&parent->GetViewAccessibility());
  EXPECT_EQ(cache().CachedChildCount(&parent->GetViewAccessibility()), 2u);
  EXPECT_EQ(cache().CachedChildAt(&parent->GetViewAccessibility(), 0),
            &a->GetViewAccessibility());
  EXPECT_EQ(cache().CachedChildAt(&parent->GetViewAccessibility(), 1),
            &c->GetViewAccessibility());

  // Clear the cache again and validate that no children are cached.
  cache().RemoveFromChildCache(&parent->GetViewAccessibility());
  EXPECT_FALSE(cache().HasCachedChildren(&parent->GetViewAccessibility()));
  EXPECT_EQ(cache().CachedChildCount(&parent->GetViewAccessibility()), 0u);
  EXPECT_EQ(cache().CachedChildAt(&parent->GetViewAccessibility(), 0), nullptr);
  EXPECT_EQ(cache().CachedChildAt(&parent->GetViewAccessibility(), 1), nullptr);
  EXPECT_EQ(cache().CachedChildAt(&parent->GetViewAccessibility(), 2), nullptr);
}

TEST_F(WidgetViewAXCacheTest, Init_RootOnly) {
  auto parent = std::make_unique<View>();
  auto* a = parent->AddChildView(std::make_unique<View>());
  auto* b = parent->AddChildView(std::make_unique<View>());
  auto* b1 = b->AddChildView(std::make_unique<View>());
  auto* b2 = b->AddChildView(std::make_unique<View>());
  auto excluded = std::make_unique<View>();

  cache().Init(parent->GetViewAccessibility(), false /* full_tree */);
  EXPECT_EQ(cache().Get(parent->GetViewAccessibility().GetUniqueId()),
            &parent->GetViewAccessibility());
  EXPECT_EQ(cache().Get(a->GetViewAccessibility().GetUniqueId()), nullptr);
  EXPECT_EQ(cache().Get(b->GetViewAccessibility().GetUniqueId()), nullptr);
  EXPECT_EQ(cache().Get(b1->GetViewAccessibility().GetUniqueId()), nullptr);
  EXPECT_EQ(cache().Get(b2->GetViewAccessibility().GetUniqueId()), nullptr);
  EXPECT_EQ(cache().Get(excluded->GetViewAccessibility().GetUniqueId()),
            nullptr);
}

TEST_F(WidgetViewAXCacheTest, Init_FullTree) {
  auto parent = std::make_unique<View>();
  auto* a = parent->AddChildView(std::make_unique<View>());
  auto* b = parent->AddChildView(std::make_unique<View>());
  auto* b1 = b->AddChildView(std::make_unique<View>());
  auto* b2 = b->AddChildView(std::make_unique<View>());
  auto excluded = std::make_unique<View>();

  cache().Init(parent->GetViewAccessibility());
  EXPECT_EQ(cache().Get(parent->GetViewAccessibility().GetUniqueId()),
            &parent->GetViewAccessibility());
  EXPECT_EQ(cache().Get(a->GetViewAccessibility().GetUniqueId()),
            &a->GetViewAccessibility());
  EXPECT_EQ(cache().Get(b->GetViewAccessibility().GetUniqueId()),
            &b->GetViewAccessibility());
  EXPECT_EQ(cache().Get(b1->GetViewAccessibility().GetUniqueId()),
            &b1->GetViewAccessibility());
  EXPECT_EQ(cache().Get(b2->GetViewAccessibility().GetUniqueId()),
            &b2->GetViewAccessibility());
  EXPECT_EQ(cache().Get(excluded->GetViewAccessibility().GetUniqueId()),
            nullptr);
}

TEST_F(WidgetViewAXCacheTest, Init_FirstRootOnlyThenFullTree) {
  auto parent = std::make_unique<View>();
  auto* a = parent->AddChildView(std::make_unique<View>());
  auto* b = parent->AddChildView(std::make_unique<View>());
  auto* b1 = b->AddChildView(std::make_unique<View>());
  auto* b2 = b->AddChildView(std::make_unique<View>());
  auto excluded = std::make_unique<View>();

  cache().Init(parent->GetViewAccessibility(), false /* full_tree */);
  EXPECT_EQ(cache().Get(parent->GetViewAccessibility().GetUniqueId()),
            &parent->GetViewAccessibility());
  EXPECT_EQ(cache().Get(a->GetViewAccessibility().GetUniqueId()), nullptr);
  EXPECT_EQ(cache().Get(b->GetViewAccessibility().GetUniqueId()), nullptr);
  EXPECT_EQ(cache().Get(b1->GetViewAccessibility().GetUniqueId()), nullptr);
  EXPECT_EQ(cache().Get(b2->GetViewAccessibility().GetUniqueId()), nullptr);
  EXPECT_EQ(cache().Get(excluded->GetViewAccessibility().GetUniqueId()),
            nullptr);

  cache().Init(parent->GetViewAccessibility());
  EXPECT_EQ(cache().Get(parent->GetViewAccessibility().GetUniqueId()),
            &parent->GetViewAccessibility());
  EXPECT_EQ(cache().Get(a->GetViewAccessibility().GetUniqueId()),
            &a->GetViewAccessibility());
  EXPECT_EQ(cache().Get(b->GetViewAccessibility().GetUniqueId()),
            &b->GetViewAccessibility());
  EXPECT_EQ(cache().Get(b1->GetViewAccessibility().GetUniqueId()),
            &b1->GetViewAccessibility());
  EXPECT_EQ(cache().Get(b2->GetViewAccessibility().GetUniqueId()),
            &b2->GetViewAccessibility());
  EXPECT_EQ(cache().Get(excluded->GetViewAccessibility().GetUniqueId()),
            nullptr);
}

TEST_F(WidgetViewAXCacheTest, CachedChildrenSnapshotIgnoresMutations) {
  auto parent = std::make_unique<View>();
  auto* a = parent->AddChildView(std::make_unique<View>());
  auto* b = parent->AddChildView(std::make_unique<View>());
  auto& pax = parent->GetViewAccessibility();

  cache().CacheChildrenIfNeeded(&pax);

  // Mutate the live hierarchy after the snapshot.
  parent->AddChildView(std::make_unique<View>());

  EXPECT_TRUE(cache().HasCachedChildren(&pax));
  EXPECT_EQ(cache().CachedChildCount(&pax), 2u);
  EXPECT_EQ(cache().CachedChildAt(&pax, 0), &a->GetViewAccessibility());
  EXPECT_EQ(cache().CachedChildAt(&pax, 1), &b->GetViewAccessibility());
  EXPECT_EQ(cache().CachedChildAt(&pax, 2), nullptr);
}

TEST_F(WidgetViewAXCacheTest, RemoveFromChildCache_ClearsSnapshot) {
  auto parent = std::make_unique<View>();
  auto* a = parent->AddChildView(std::make_unique<View>());
  auto* b = parent->AddChildView(std::make_unique<View>());
  auto& pax = parent->GetViewAccessibility();

  cache().Insert(&pax);
  cache().Insert(&a->GetViewAccessibility());
  cache().Insert(&b->GetViewAccessibility());

  cache().CacheChildrenIfNeeded(&pax);
  ASSERT_TRUE(cache().HasCachedChildren(&pax));
  ASSERT_EQ(cache().CachedChildCount(&pax), 2u);

  cache().RemoveFromChildCache(&pax);

  EXPECT_FALSE(cache().HasCachedChildren(&pax));
  EXPECT_EQ(cache().CachedChildCount(&pax), 0u);
  EXPECT_EQ(cache().CachedChildAt(&pax, 0), nullptr);

  EXPECT_EQ(cache().Get(a->GetViewAccessibility().GetUniqueId()),
            &a->GetViewAccessibility());
  EXPECT_EQ(cache().Get(b->GetViewAccessibility().GetUniqueId()),
            &b->GetViewAccessibility());
}

}  // namespace views::test
