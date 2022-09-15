// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/accessibility_alert_window.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer_type.h"
#include "ui/views/accessibility/ax_aura_obj_cache.h"
#include "ui/views/accessibility/ax_aura_obj_wrapper.h"
#include "ui/views/test/views_test_base.h"

namespace views {

class FakeAXAuraObjCacheDelegate : public AXAuraObjCache::Delegate {
 public:
  FakeAXAuraObjCacheDelegate() = default;
  FakeAXAuraObjCacheDelegate(const FakeAXAuraObjCacheDelegate&) = delete;
  FakeAXAuraObjCacheDelegate& operator=(const FakeAXAuraObjCacheDelegate&) =
      delete;
  ~FakeAXAuraObjCacheDelegate() override = default;

  void OnChildWindowRemoved(AXAuraObjWrapper* parent) override {}
  void OnEvent(AXAuraObjWrapper* aura_obj,
               ax::mojom::Event event_type) override {
    if (event_type == ax::mojom::Event::kAlert)
      count_++;
  }

  int count() { return count_; }
  void set_count(int count) { count_ = count; }

 private:
  int count_ = 0;
};

class AccessibilityAlertWindowTest : public ViewsTestBase {
 public:
  AccessibilityAlertWindowTest() = default;
  AccessibilityAlertWindowTest(const AccessibilityAlertWindowTest&) = delete;
  AccessibilityAlertWindowTest& operator=(const AccessibilityAlertWindowTest&) =
      delete;
  ~AccessibilityAlertWindowTest() override = default;

 protected:
  void SetUp() override {
    ViewsTestBase::SetUp();

    parent_ = std::make_unique<aura::Window>(nullptr);
    parent_->Init(ui::LAYER_SOLID_COLOR);
  }

  std::unique_ptr<aura::Window> parent_;
  AXAuraObjCache cache;
};

TEST_F(AccessibilityAlertWindowTest, HandleAlert) {
  FakeAXAuraObjCacheDelegate delegate;
  cache.SetDelegate(&delegate);

  AccessibilityAlertWindow window(parent_.get(), &cache);

  window.HandleAlert("test");
  EXPECT_EQ(1, delegate.count());

  delegate.set_count(0);
  window.OnWillDestroyEnv();

  window.HandleAlert("test");
  EXPECT_EQ(0, delegate.count());
}

TEST_F(AccessibilityAlertWindowTest, OnWillDestroyEnv) {
  AccessibilityAlertWindow window(parent_.get(), &cache);
  window.OnWillDestroyEnv();

  EXPECT_FALSE(window.observation_.IsObserving());
  EXPECT_FALSE(window.alert_window_);
}

}  // namespace views
