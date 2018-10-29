// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/hit_test_data_provider_aura.h"

#include "components/viz/client/hit_test_data_provider.h"
#include "components/viz/common/hit_test/hit_test_region_list.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/mus/window_port_mus_test_helper.h"
#include "ui/aura/window.h"
#include "ui/aura/window_targeter.h"
#include "ui/gfx/geometry/rect.h"

namespace aura {

namespace {

// Custom WindowTargeter that replaces hit-test area on a window with a frame
// rectangle and a hole in the middle 1/3.
//  ----------------------
// |   hit     hit        |
// |      ----------      |
// |     |          |     |
// |     |  No hit  | hit |
// |     |          |     |
// | hit |          |     |
// |      ----------      |
// |   hit        hit     |
//  ----------------------
class TestHoleWindowTargeter : public WindowTargeter {
 public:
  TestHoleWindowTargeter() = default;
  ~TestHoleWindowTargeter() override {}

 private:
  // WindowTargeter:
  std::unique_ptr<WindowTargeter::HitTestRects> GetExtraHitTestShapeRects(
      Window* target) const override {
    gfx::Rect bounds = target->bounds();
    int x0 = 0;
    int x1 = bounds.width() / 3;
    int x2 = bounds.width() - bounds.width() / 3;
    int x3 = bounds.width();
    int y0 = 0;
    int y1 = bounds.height() / 3;
    int y2 = bounds.height() - bounds.height() / 3;
    int y3 = bounds.height();
    auto shape_rects = std::make_unique<WindowTargeter::HitTestRects>();
    shape_rects->emplace_back(x0, y0, bounds.width(), y1 - y0);
    shape_rects->emplace_back(x0, y1, x1 - x0, y2 - y1);
    shape_rects->emplace_back(x2, y1, x3 - x2, y2 - y1);
    shape_rects->emplace_back(x0, y2, bounds.width(), y3 - y2);
    return shape_rects;
  }

  DISALLOW_COPY_AND_ASSIGN(TestHoleWindowTargeter);
};

}  // namespace

// Creates a root window, child windows and a viz::HitTestDataProvider.
// root
//   +- window2
//   |_ window3
//        |_ window4
class HitTestDataProviderAuraTest : public test::AuraTestBaseMus {
 public:
  HitTestDataProviderAuraTest() {}
  ~HitTestDataProviderAuraTest() override {}

  void SetUp() override {
    test::AuraTestBaseMus::SetUp();

    root_ = std::make_unique<Window>(nullptr);
    root_->Init(ui::LAYER_NOT_DRAWN);
    root_->SetEventTargeter(std::make_unique<WindowTargeter>());
    root_->SetBounds(gfx::Rect(0, 0, 300, 200));
    root_->Show();

    window2_ = new Window(nullptr);
    window2_->Init(ui::LAYER_TEXTURED);
    window2_->SetBounds(gfx::Rect(20, 30, 40, 60));
    window2_->Show();

    window3_ = new Window(nullptr);
    window3_->Init(ui::LAYER_TEXTURED);
    window3_->SetEventTargeter(std::make_unique<WindowTargeter>());
    window3_->SetBounds(gfx::Rect(50, 60, 100, 40));
    window3_->Show();

    window4_ = new Window(nullptr);
    window4_->Init(ui::LAYER_TEXTURED);
    window4_->SetBounds(gfx::Rect(20, 10, 60, 30));
    window4_->Show();

    window3_->AddChild(window4_);
    root_->AddChild(window2_);
    root_->AddChild(window3_);

    compositor_frame_ = viz::CompositorFrame();
    hit_test_data_provider_ = std::make_unique<HitTestDataProviderAura>(root());
  }

 protected:
  const viz::HitTestDataProvider* hit_test_data_provider() const {
    return hit_test_data_provider_.get();
  }

  Window* root() { return root_.get(); }
  Window* window2() { return window2_; }
  Window* window3() { return window3_; }
  Window* window4() { return window4_; }
  viz::CompositorFrame compositor_frame_;

 private:
  std::unique_ptr<Window> root_;
  Window* window2_;
  Window* window3_;
  Window* window4_;
  std::unique_ptr<viz::HitTestDataProvider> hit_test_data_provider_;

  DISALLOW_COPY_AND_ASSIGN(HitTestDataProviderAuraTest);
};

// TODO(riajiang): Add test cases for kHitTestChildSurface to ensure
// that local_surface_id is set and used correctly.

// Tests that the order of reported hit-test regions matches windows Z-order.
TEST_F(HitTestDataProviderAuraTest, Stacking) {
  const base::Optional<viz::HitTestRegionList> hit_test_data_1 =
      hit_test_data_provider()->GetHitTestData(compositor_frame_);
  ASSERT_TRUE(hit_test_data_1);
  EXPECT_EQ(hit_test_data_1->flags, viz::HitTestRegionFlags::kHitTestMine);
  EXPECT_EQ(hit_test_data_1->bounds, root()->bounds());
  Window* expected_order_1[] = {window3(), window4(), window2()};
  EXPECT_EQ(hit_test_data_1->regions.size(), arraysize(expected_order_1));
  int i = 0;
  for (const auto& region : hit_test_data_1->regions) {
    EXPECT_EQ(region.flags, viz::HitTestRegionFlags::kHitTestMine |
                                viz::HitTestRegionFlags::kHitTestMouse |
                                viz::HitTestRegionFlags::kHitTestTouch);
    EXPECT_EQ(region.frame_sink_id, expected_order_1[i]->GetFrameSinkId());
    EXPECT_EQ(region.rect.ToString(), expected_order_1[i]->bounds().ToString());
    i++;
  }

  root()->StackChildAbove(window2(), window3());
  const base::Optional<viz::HitTestRegionList> hit_test_data_2 =
      hit_test_data_provider()->GetHitTestData(compositor_frame_);
  ASSERT_TRUE(hit_test_data_2);
  EXPECT_EQ(hit_test_data_2->flags, viz::HitTestRegionFlags::kHitTestMine);
  EXPECT_EQ(hit_test_data_2->bounds, root()->bounds());

  Window* expected_order_2[] = {window2(), window3(), window4()};
  EXPECT_EQ(hit_test_data_2->regions.size(), arraysize(expected_order_2));
  i = 0;
  for (const auto& region : hit_test_data_2->regions) {
    EXPECT_EQ(region.flags, viz::HitTestRegionFlags::kHitTestMine |
                                viz::HitTestRegionFlags::kHitTestMouse |
                                viz::HitTestRegionFlags::kHitTestTouch);
    EXPECT_EQ(region.frame_sink_id, expected_order_2[i]->GetFrameSinkId());
    EXPECT_EQ(region.rect.ToString(), expected_order_2[i]->bounds().ToString());
    i++;
  }
}

// Tests that the hit-test regions get expanded with a custom event targeter.
TEST_F(HitTestDataProviderAuraTest, CustomTargeter) {
  constexpr int kMouseInset = -5;
  constexpr int kTouchInset = -10;
  auto targeter = std::make_unique<WindowTargeter>();
  targeter->SetInsets(gfx::Insets(kMouseInset), gfx::Insets(kTouchInset));
  window3()->SetEventTargeter(std::move(targeter));

  targeter = std::make_unique<WindowTargeter>();
  targeter->SetInsets(gfx::Insets(kMouseInset), gfx::Insets(kTouchInset));
  window4()->SetEventTargeter(std::move(targeter));

  window2()->SetEmbedFrameSinkId(viz::FrameSinkId(1, 2));
  const base::Optional<viz::HitTestRegionList> hit_test_data =
      hit_test_data_provider()->GetHitTestData(compositor_frame_);
  ASSERT_TRUE(hit_test_data);
  EXPECT_EQ(hit_test_data->flags, viz::HitTestRegionFlags::kHitTestMine);
  EXPECT_EQ(hit_test_data->bounds, root()->bounds());

  // Children of a window that has the custom targeter installed as well as that
  // window will get reported twice, once with hit-test bounds optimized for
  // mouse events and another time with bounds expanded more for touch input.
  struct {
    Window* window;
    uint32_t flags;
    int insets;
  } expected[] = {{window3(),
                   viz::HitTestRegionFlags::kHitTestMine |
                       viz::HitTestRegionFlags::kHitTestMouse,
                   kMouseInset},
                  {window3(),
                   viz::HitTestRegionFlags::kHitTestMine |
                       viz::HitTestRegionFlags::kHitTestTouch,
                   kTouchInset},
                  {window4(),
                   viz::HitTestRegionFlags::kHitTestMine |
                       viz::HitTestRegionFlags::kHitTestMouse,
                   kMouseInset},
                  {window4(),
                   viz::HitTestRegionFlags::kHitTestMine |
                       viz::HitTestRegionFlags::kHitTestTouch,
                   kTouchInset},
                  {window2(),
                   viz::HitTestRegionFlags::kHitTestChildSurface |
                       viz::HitTestRegionFlags::kHitTestMouse |
                       viz::HitTestRegionFlags::kHitTestTouch,
                   0}};
  ASSERT_EQ(hit_test_data->regions.size(), arraysize(expected));
  ASSERT_EQ(hit_test_data->regions.size(), arraysize(expected));
  ASSERT_EQ(hit_test_data->regions.size(), arraysize(expected));
  int i = 0;
  for (const auto& region : hit_test_data->regions) {
    EXPECT_EQ(region.frame_sink_id, expected[i].window->GetFrameSinkId());
    EXPECT_EQ(region.flags, expected[i].flags);
    gfx::Rect expected_bounds = expected[i].window->bounds();
    expected_bounds.Inset(gfx::Insets(expected[i].insets));
    EXPECT_EQ(region.rect.ToString(), expected_bounds.ToString());
    i++;
  }
}

// Tests that the complex hit-test shape can be set with a custom targeter.
TEST_F(HitTestDataProviderAuraTest, HoleTargeter) {
  window3()->SetEventTargeter(std::make_unique<TestHoleWindowTargeter>());
  const base::Optional<viz::HitTestRegionList> hit_test_data =
      hit_test_data_provider()->GetHitTestData(compositor_frame_);
  ASSERT_TRUE(hit_test_data);
  EXPECT_EQ(hit_test_data->flags, viz::HitTestRegionFlags::kHitTestMine);
  EXPECT_EQ(hit_test_data->bounds, root()->bounds());

  // Children of a container that has the custom targeter installed as well as
  // that container will get reported 4 times for each of the hit test regions
  // defined by the custom targeter.
  // original window3 is at gfx::Rect(50, 60, 100, 40).
  // original window4 is at gfx::Rect(20, 10, 60, 30).
  struct {
    Window* window;
    gfx::Rect bounds;
  } expected[] = {
      {window3(), {50, 60, 100, 13}},  {window3(), {50, 73, 33, 14}},
      {window3(), {117, 73, 33, 14}},  {window3(), {50, 87, 100, 13}},
      {window4(), {20, 10, 60, 10}},   {window4(), {20, 20, 20, 10}},
      {window4(), {60, 20, 20, 10}},   {window4(), {20, 30, 60, 10}},
      {window2(), window2()->bounds()}};
  constexpr uint32_t expected_flags = viz::HitTestRegionFlags::kHitTestMine |
                                      viz::HitTestRegionFlags::kHitTestMouse |
                                      viz::HitTestRegionFlags::kHitTestTouch;
  ASSERT_EQ(hit_test_data->regions.size(), arraysize(expected));
  int i = 0;
  for (const auto& region : hit_test_data->regions) {
    EXPECT_EQ(region.frame_sink_id, expected[i].window->GetFrameSinkId());
    EXPECT_EQ(region.flags, expected_flags);
    EXPECT_EQ(region.rect.ToString(), expected[i].bounds.ToString());
    i++;
  }
}

TEST_F(HitTestDataProviderAuraTest, TargetingPolicies) {
  root()->SetEventTargetingPolicy(ws::mojom::EventTargetingPolicy::NONE);
  base::Optional<viz::HitTestRegionList> hit_test_data =
      hit_test_data_provider()->GetHitTestData(compositor_frame_);
  ASSERT_FALSE(hit_test_data);

  root()->SetEventTargetingPolicy(ws::mojom::EventTargetingPolicy::TARGET_ONLY);
  window3()->SetEventTargetingPolicy(
      ws::mojom::EventTargetingPolicy::TARGET_AND_DESCENDANTS);
  hit_test_data = hit_test_data_provider()->GetHitTestData(compositor_frame_);
  ASSERT_TRUE(hit_test_data);
  EXPECT_EQ(hit_test_data->flags, viz::HitTestRegionFlags::kHitTestMine);
  EXPECT_EQ(hit_test_data->regions.size(), 3u);

  root()->SetEventTargetingPolicy(ws::mojom::EventTargetingPolicy::TARGET_ONLY);
  window3()->SetEventTargetingPolicy(
      ws::mojom::EventTargetingPolicy::TARGET_ONLY);
  hit_test_data = hit_test_data_provider()->GetHitTestData(compositor_frame_);
  ASSERT_TRUE(hit_test_data);
  EXPECT_EQ(hit_test_data->flags, viz::HitTestRegionFlags::kHitTestMine);
  EXPECT_EQ(hit_test_data->regions.size(), 2u);

  root()->SetEventTargetingPolicy(
      ws::mojom::EventTargetingPolicy::DESCENDANTS_ONLY);
  window3()->SetEventTargetingPolicy(
      ws::mojom::EventTargetingPolicy::DESCENDANTS_ONLY);
  hit_test_data = hit_test_data_provider()->GetHitTestData(compositor_frame_);
  ASSERT_TRUE(hit_test_data);
  EXPECT_EQ(hit_test_data->flags, viz::HitTestRegionFlags::kHitTestIgnore);
  EXPECT_EQ(hit_test_data->regions.size(), 2u);

  root()->SetEventTargetingPolicy(
      ws::mojom::EventTargetingPolicy::TARGET_AND_DESCENDANTS);
  window3()->SetEventTargetingPolicy(
      ws::mojom::EventTargetingPolicy::TARGET_AND_DESCENDANTS);
  hit_test_data = hit_test_data_provider()->GetHitTestData(compositor_frame_);
  ASSERT_TRUE(hit_test_data);
  EXPECT_EQ(hit_test_data->flags, viz::HitTestRegionFlags::kHitTestMine);
  EXPECT_EQ(hit_test_data->regions.size(), 3u);
}

// Tests that we do not submit hit-test data for invisible windows and for
// children of a child surface.
TEST_F(HitTestDataProviderAuraTest, DoNotSubmit) {
  base::Optional<viz::HitTestRegionList> hit_test_data =
      hit_test_data_provider()->GetHitTestData(compositor_frame_);
  ASSERT_TRUE(hit_test_data);
  EXPECT_EQ(hit_test_data->regions.size(), 3u);

  window2()->Hide();
  hit_test_data = hit_test_data_provider()->GetHitTestData(compositor_frame_);
  ASSERT_TRUE(hit_test_data);
  EXPECT_EQ(hit_test_data->regions.size(), 2u);

  window3()->SetEmbedFrameSinkId(viz::FrameSinkId(1, 3));
  hit_test_data = hit_test_data_provider()->GetHitTestData(compositor_frame_);
  ASSERT_TRUE(hit_test_data);
  EXPECT_EQ(hit_test_data->regions.size(), 1u);

  root()->Hide();
  hit_test_data = hit_test_data_provider()->GetHitTestData(compositor_frame_);
  ASSERT_FALSE(hit_test_data);

  root()->Show();
  hit_test_data = hit_test_data_provider()->GetHitTestData(compositor_frame_);
  ASSERT_TRUE(hit_test_data);
  EXPECT_EQ(hit_test_data->regions.size(), 1u);
  root()->SetEmbedFrameSinkId(viz::FrameSinkId(1, 1));
  hit_test_data = hit_test_data_provider()->GetHitTestData(compositor_frame_);
  ASSERT_TRUE(hit_test_data);
  EXPECT_EQ(hit_test_data->regions.size(), 0u);
}

}  // namespace aura
