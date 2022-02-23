// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/x/window_cache.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/future.h"

namespace x11 {

class WindowCacheTest : public testing::Test {
 public:
  ~WindowCacheTest() override = default;

 protected:
  void ResetCache() {
    cache_.reset();
    cache_ = std::make_unique<WindowCache>(connection_, root_);
    cache_->SyncForTest();
  }

  Window CreateWindow(Window parent) {
    Window window = connection_->GenerateId<Window>();
    connection_->CreateWindow({
        .wid = window,
        .parent = parent,
        .width = 512,
        .height = 1024,
        .override_redirect = static_cast<Bool32>(true),
    });
    return window;
  }

  Connection* connection() { return connection_; }

  Window root() const { return root_; }

  WindowCache* cache() { return cache_.get(); }

 private:
  void SetUp() override {
    connection_ = Connection::Get();
    root_ = CreateWindow(connection_->default_root());
    ResetCache();
  }

  void TearDown() override {
    cache_.reset();
    connection_->DestroyWindow({root_}).Sync();
    root_ = Window::None;
    connection_ = nullptr;
  }

  Connection* connection_;
  Window root_ = Window::None;
  std::unique_ptr<WindowCache> cache_;
};

// Ensure creating the cache doesn't timeout.
TEST_F(WindowCacheTest, Basic) {
  const WindowCache::WindowInfo& info = cache()->windows().at(root());
  EXPECT_EQ(info.parent, connection()->default_root());
  EXPECT_FALSE(info.mapped);
  EXPECT_EQ(info.x_px, 0);
  EXPECT_EQ(info.y_px, 0);
  EXPECT_EQ(info.width_px, 512);
  EXPECT_EQ(info.height_px, 1024);
  EXPECT_EQ(info.border_width_px, 0);
  EXPECT_TRUE(info.children.empty());
}

TEST_F(WindowCacheTest, ConfigureNotify) {
  connection()->ConfigureWindow({.window = root(),
                                 .x = 10,
                                 .y = 10,
                                 .width = 20,
                                 .height = 20,
                                 .border_width = 5});
  cache()->SyncForTest();
  const WindowCache::WindowInfo& info = cache()->windows().at(root());
  EXPECT_EQ(info.x_px, 10);
  EXPECT_EQ(info.y_px, 10);
  EXPECT_EQ(info.width_px, 20);
  EXPECT_EQ(info.height_px, 20);
  EXPECT_EQ(info.border_width_px, 5);
}

TEST_F(WindowCacheTest, CreateAndDestroyNotify) {
  Window r = root();
  Window a = CreateWindow(r);
  Window aa = CreateWindow(a);
  Window ab = CreateWindow(a);
  Window b = CreateWindow(r);
  Window ba = CreateWindow(b);
  Window bb = CreateWindow(b);

  cache()->SyncForTest();
  EXPECT_EQ(cache()->windows().at(r).children, (std::vector<Window>{a, b}));
  EXPECT_EQ(cache()->windows().at(a).children, (std::vector<Window>{aa, ab}));
  EXPECT_EQ(cache()->windows().at(b).children, (std::vector<Window>{ba, bb}));

  connection()->DestroyWindow(ab);
  connection()->DestroyWindow(ba);
  cache()->SyncForTest();
  EXPECT_EQ(cache()->windows().at(r).children, (std::vector<Window>{a, b}));
  EXPECT_EQ(cache()->windows().at(a).children, (std::vector<Window>{aa}));
  EXPECT_EQ(cache()->windows().at(b).children, (std::vector<Window>{bb}));
}

TEST_F(WindowCacheTest, Restack) {
  auto restack = [&](Window window, Window sibling, bool above) {
    connection()->ConfigureWindow(
        {.window = window,
         .sibling = sibling,
         .stack_mode = above ? StackMode::Above : StackMode::Below});
    cache()->SyncForTest();
  };
  Window a = CreateWindow(root());
  Window b = CreateWindow(root());
  Window c = CreateWindow(root());
  Window d = CreateWindow(root());
  Window e = CreateWindow(root());
  Window f = CreateWindow(root());

  cache()->SyncForTest();
  auto& windows = cache()->windows().at(root()).children;
  EXPECT_EQ(windows, (std::vector<Window>{a, b, c, d, e, f}));

  restack(b, e, true);
  EXPECT_EQ(windows, (std::vector<Window>{a, c, d, e, b, f}));

  restack(f, a, false);
  EXPECT_EQ(windows, (std::vector<Window>{f, a, c, d, e, b}));

  restack(c, d, true);
  EXPECT_EQ(windows, (std::vector<Window>{f, a, d, c, e, b}));

  restack(d, b, true);
  EXPECT_EQ(windows, (std::vector<Window>{f, a, c, e, b, d}));

  restack(e, c, false);
  EXPECT_EQ(windows, (std::vector<Window>{f, a, e, c, b, d}));

  restack(b, a, false);
  EXPECT_EQ(windows, (std::vector<Window>{f, b, a, e, c, d}));

  // Test a configure event where the stacking doesn't change.
  connection()->ConfigureWindow({.window = a, .width = 100});
  cache()->SyncForTest();
  EXPECT_EQ(windows, (std::vector<Window>{f, b, a, e, c, d}));
}

TEST_F(WindowCacheTest, MapAndUnmapNotify) {
  Window window = CreateWindow(root());
  cache()->SyncForTest();
  auto& info = cache()->windows().at(window);
  EXPECT_FALSE(info.mapped);

  connection()->MapWindow(window);
  cache()->SyncForTest();
  EXPECT_TRUE(info.mapped);

  connection()->UnmapWindow(window);
  cache()->SyncForTest();
  EXPECT_FALSE(info.mapped);
}

TEST_F(WindowCacheTest, ReparentNotify) {
  Window r = root();
  Window a = CreateWindow(r);
  Window b = CreateWindow(r);
  Window w = CreateWindow(b);

  cache()->SyncForTest();
  auto& info_a = cache()->windows().at(a);
  auto& info_b = cache()->windows().at(b);
  auto& info_w = cache()->windows().at(w);
  EXPECT_EQ(info_a.children, (std::vector<Window>{}));
  EXPECT_EQ(info_b.children, (std::vector<Window>{w}));
  EXPECT_EQ(info_w.parent, b);

  connection()->ReparentWindow(w, a);
  cache()->SyncForTest();
  EXPECT_EQ(info_a.children, (std::vector<Window>{w}));
  EXPECT_EQ(info_b.children, (std::vector<Window>{}));
  EXPECT_EQ(info_w.parent, a);
}

TEST_F(WindowCacheTest, GravityNotify) {
  Window parent = CreateWindow(root());
  Window child = connection()->GenerateId<Window>();
  connection()->CreateWindow({
      .wid = child,
      .parent = parent,
      .x = 512,
      .y = 1024,
      .width = 10,
      .height = 10,
      .win_gravity = Gravity::SouthEast,
      .override_redirect = static_cast<Bool32>(true),
  });

  cache()->SyncForTest();
  auto& info = cache()->windows().at(child);
  EXPECT_EQ(info.x_px, 512);
  EXPECT_EQ(info.y_px, 1024);

  connection()->ConfigureWindow(
      {.window = parent, .width = 256, .height = 512});
  cache()->SyncForTest();
  EXPECT_EQ(info.x_px, 256);
  EXPECT_EQ(info.y_px, 512);
}

TEST_F(WindowCacheTest, CirculateNotify) {
  Window a = CreateWindow(root());
  Window b = CreateWindow(root());
  Window c = CreateWindow(root());
  Window d = CreateWindow(root());
  for (Window w : {a, b, c, d})
    connection()->MapWindow(w);

  cache()->SyncForTest();
  auto& windows = cache()->windows().at(root()).children;
  EXPECT_EQ(windows, (std::vector<Window>{a, b, c, d}));

  connection()->CirculateWindow(Circulate::RaiseLowest, root());
  cache()->SyncForTest();
  EXPECT_EQ(windows, (std::vector<Window>{b, c, d, a}));

  connection()->CirculateWindow(Circulate::LowerHighest, root());
  cache()->SyncForTest();
  EXPECT_EQ(windows, (std::vector<Window>{a, b, c, d}));
}

TEST_F(WindowCacheTest, ShapeExtension) {
  auto& shape = connection()->shape();
  if (!shape.present())
    return;

  const WindowCache::WindowInfo& info = cache()->windows().at(root());
  EXPECT_EQ(info.bounding_rects_px,
            (std::vector<Rectangle>{{0, 0, 512, 1024}}));
  EXPECT_EQ(info.input_rects_px, (std::vector<Rectangle>{{0, 0, 512, 1024}}));

  std::vector<Rectangle> bounding_rects{{10, 10, 100, 100}};
  std::vector<Rectangle> input_rects{{20, 20, 10, 10}, {50, 50, 10, 10}};
  shape.Rectangles({.operation = Shape::So::Set,
                    .destination_kind = Shape::Sk::Bounding,
                    .destination_window = root(),
                    .rectangles = bounding_rects});
  shape.Rectangles({.operation = Shape::So::Set,
                    .destination_kind = Shape::Sk::Input,
                    .destination_window = root(),
                    .rectangles = input_rects});
  cache()->SyncForTest();
  EXPECT_EQ(info.bounding_rects_px, bounding_rects);
  EXPECT_EQ(info.input_rects_px, input_rects);
}

}  // namespace x11