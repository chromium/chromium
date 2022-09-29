// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/client/screen_position_client.h"

#include <memory>

#include "ui/aura/test/aura_test_base.h"
#include "ui/gfx/geometry/transform.h"

namespace aura {
namespace client {

using ScreenPositionClientTest = test::AuraTestBase;

class TestScreenPositionClient : public ScreenPositionClient {
 public:
  TestScreenPositionClient() = default;
  TestScreenPositionClient(const TestScreenPositionClient&) = delete;
  TestScreenPositionClient& operator=(const TestScreenPositionClient&) = delete;
  ~TestScreenPositionClient() override = default;

  // ScreenPositionClient:
  void ConvertPointToScreen(const Window* window, gfx::PointF* point) override {
  }
  void ConvertPointFromScreen(const Window* window,
                              gfx::PointF* point) override {}
  void ConvertHostPointToScreen(Window* root_window,
                                gfx::Point* point) override {}
  void SetBounds(Window* window,
                 const gfx::Rect& bounds,
                 const display::Display& display) override {}

 protected:
  // ScreenPositionClient:
  gfx::Point GetRootWindowOriginInScreen(
      const aura::Window* root_window) override {
    return gfx::Point();
  }
};

TEST_F(ScreenPositionClientTest, ConvertPointToRootWindowIgnoringTransforms) {
  std::unique_ptr<Window> parent(CreateNormalWindow(1, root_window(), nullptr));
  std::unique_ptr<Window> child(CreateNormalWindow(2, parent.get(), nullptr));

  parent->SetBounds(gfx::Rect(50, 50, 200, 200));
  child->SetBounds(gfx::Rect(150, 150, 200, 200));

  TestScreenPositionClient test_client;
  gfx::Point point(100, 100);
  test_client.ConvertPointToRootWindowIgnoringTransforms(parent.get(), &point);
  EXPECT_EQ(gfx::Point(150, 150), point);

  point = gfx::Point(100, 100);
  test_client.ConvertPointToRootWindowIgnoringTransforms(child.get(), &point);
  EXPECT_EQ(gfx::Point(300, 300), point);

  point = gfx::Point(100, 100);
  child->SetTransform(gfx::Transform::MakeTranslation(100, 100));
  test_client.ConvertPointToRootWindowIgnoringTransforms(child.get(), &point);
  EXPECT_EQ(gfx::Point(300, 300), point);
}

}  // namespace client
}  // namespace aura
