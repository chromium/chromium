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

// This test validates that CacheChildrenIfNeeded properly caches the children
// and not the grandchildren, and that it accurately track the "cached-children"
// state.
TEST_F(WidgetViewAXCacheTest, CacheChildrenIfNeeded) {
  auto v = std::make_unique<View>();
  auto* child = v->AddChildView(std::make_unique<View>());
  auto* grandchild = child->AddChildView(std::make_unique<View>());

  EXPECT_FALSE(cache().HasCachedChildren(&v->GetViewAccessibility()));
  EXPECT_FALSE(cache().HasCachedChildren(&child->GetViewAccessibility()));
  EXPECT_EQ(cache().Get(v->GetViewAccessibility().GetUniqueId()), nullptr);
  EXPECT_EQ(cache().Get(child->GetViewAccessibility().GetUniqueId()), nullptr);
  EXPECT_EQ(cache().Get(grandchild->GetViewAccessibility().GetUniqueId()),
            nullptr);

  cache().CacheChildrenIfNeeded(&v->GetViewAccessibility());
  EXPECT_TRUE(cache().HasCachedChildren(&v->GetViewAccessibility()));
  EXPECT_FALSE(cache().HasCachedChildren(&child->GetViewAccessibility()));
  EXPECT_EQ(cache().Get(child->GetViewAccessibility().GetUniqueId()),
            &child->GetViewAccessibility());
  EXPECT_EQ(cache().Get(grandchild->GetViewAccessibility().GetUniqueId()),
            nullptr);

  cache().RemoveFromChildCache(&v->GetViewAccessibility());
  EXPECT_FALSE(cache().HasCachedChildren(&v->GetViewAccessibility()));

  // While we want to remove the "cached-children" mark, we still want to keep
  // the nodes in the cache until they're explicitly removed.
  EXPECT_EQ(cache().Get(child->GetViewAccessibility().GetUniqueId()),
            &child->GetViewAccessibility());
}

}  // namespace views::test
