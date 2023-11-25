// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/x/window_cache.h"

#include "base/memory/raw_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/x/atom_cache.h"
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

  Window root_container() const { return root_container_; }

  WindowCache* cache() { return cache_.get(); }

 private:
  void SetUp() override {
    connection_ = Connection::Get();
    root_container_ = CreateWindow(connection_->default_root());
    root_ = CreateWindow(root_container_);
    ResetCache();
  }

  void TearDown() override {
    cache_.reset();
    connection_->DestroyWindow({root_}).Sync();
    connection_->DestroyWindow({root_container_}).Sync();
    root_ = Window::None;
    root_container_ = Window::None;
    connection_ = nullptr;
  }

  raw_ptr<Connection> connection_;
  Window root_container_ = Window::None;
  Window root_ = Window::None;
  std::unique_ptr<WindowCache> cache_;
};

// Ensure creating the cache doesn't timeout.
TEST_F(WindowCacheTest, Basic) {
  const WindowCache::WindowInfo& info = cache()->windows().at(root());
  EXPECT_EQ(info.parent, root_container());
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
  for (Window w : {a, b, c, d}) {
    connection()->MapWindow(w);
  }

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
  if (!shape.present()) {
    return;
  }

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

TEST_F(WindowCacheTest, WmName) {
  const WindowCache::WindowInfo& info = cache()->windows().at(root());
  EXPECT_FALSE(info.has_wm_name);

  connection()->SetStringProperty(root(), Atom::WM_NAME, Atom::STRING, "Foo");
  cache()->SyncForTest();
  EXPECT_TRUE(info.has_wm_name);

  connection()->DeleteProperty(root(), Atom::WM_NAME);
  cache()->SyncForTest();
  EXPECT_FALSE(info.has_wm_name);
}

TEST_F(WindowCacheTest, GtkFrameExtents) {
  const WindowCache::WindowInfo& info = cache()->windows().at(root());
  EXPECT_EQ(info.gtk_frame_extents_px, gfx::Insets());

  const Atom gtk_frame_extents = GetAtom("_GTK_FRAME_EXTENTS");
  connection()->SetArrayProperty(root(), gtk_frame_extents, Atom::CARDINAL,
                                 std::vector<uint32_t>{1, 2, 3, 4});
  cache()->SyncForTest();
  EXPECT_EQ(info.gtk_frame_extents_px, gfx::Insets::TLBR(3, 1, 4, 2));

  connection()->DeleteProperty(root(), gtk_frame_extents);
  cache()->SyncForTest();
  EXPECT_EQ(info.gtk_frame_extents_px, gfx::Insets());

  // Make sure malformed values don't get cached.
  connection()->SetArrayProperty(root(), gtk_frame_extents, Atom::CARDINAL,
                                 std::vector<uint32_t>{1, 2});
  cache()->SyncForTest();
  EXPECT_EQ(info.gtk_frame_extents_px, gfx::Insets());

  connection()->SetArrayProperty(root(), gtk_frame_extents, Atom::CARDINAL,
                                 std::vector<uint8_t>{1, 2, 3, 4});
  cache()->SyncForTest();
  EXPECT_EQ(info.gtk_frame_extents_px, gfx::Insets());
}

TEST_F(WindowCacheTest, GetWindowAtPoint) {
  // Basic test on an undecorated, unobscured window.
  connection()->MapWindow(root());
  connection()->SetStringProperty(root(), Atom::WM_NAME, Atom::STRING, "root");
  cache()->SyncForTest();
  EXPECT_EQ(cache()->GetWindowAtPoint({100, 100}, root()), root());

  // Unmapped windows are hidden.
  connection()->UnmapWindow(root());
  cache()->SyncForTest();
  EXPECT_EQ(cache()->GetWindowAtPoint({100, 100}, root()), Window::None);
  connection()->MapWindow(root());

  // Unnamed windows should not be returned.
  connection()->DeleteProperty(root(), Atom::WM_NAME);
  cache()->SyncForTest();
  EXPECT_EQ(cache()->GetWindowAtPoint({100, 100}, root()), Window::None);

  // GetWindowAtPoint on an uncached window shouldn't crash.
  Window does_not_exist = connection()->GenerateId<Window>();
  EXPECT_EQ(cache()->GetWindowAtPoint({100, 100}, does_not_exist),
            Window::None);

  // Basic hit test.
  Window a = CreateWindow(root());
  connection()->ConfigureWindow({
      .window = a,
      .x = 100,
      .y = 100,
      .width = 100,
      .height = 100,
  });
  connection()->MapWindow(a);
  connection()->SetStringProperty(a, Atom::WM_NAME, Atom::STRING, "a");
  cache()->SyncForTest();
  EXPECT_EQ(cache()->GetWindowAtPoint({150, 150}, root()), a);
  EXPECT_EQ(cache()->GetWindowAtPoint({50, 50}, root()), Window::None);

  // Border hit test.
  auto& shape = connection()->shape();
  if (shape.present()) {
    for (auto kind : {Shape::Sk::Bounding, Shape::Sk::Input}) {
      shape.Rectangles(
          {.destination_kind = kind,
           .destination_window = a,
           .rectangles = std::vector<Rectangle>{{0, 0, 300, 300}}});
    }
  }
  cache()->SyncForTest();
  EXPECT_EQ(cache()->GetWindowAtPoint({250, 250}, root()), Window::None);
  connection()->ConfigureWindow({.window = a, .border_width = 100});
  cache()->SyncForTest();
  EXPECT_EQ(cache()->GetWindowAtPoint({250, 250}, root()), a);
  connection()->ConfigureWindow({.window = a, .border_width = 0});
  if (shape.present()) {
    for (auto kind : {Shape::Sk::Bounding, Shape::Sk::Input}) {
      shape.Mask({.destination_kind = kind, .destination_window = a});
    }
  }

  // GTK_FRAME_EXTENTS insets the window bounds.
  EXPECT_EQ(cache()->GetWindowAtPoint({125, 125}, root()), a);
  const Atom gtk_frame_extents = GetAtom("_GTK_FRAME_EXTENTS");
  connection()->SetArrayProperty(a, gtk_frame_extents, Atom::CARDINAL,
                                 std::vector<uint32_t>{40, 40, 40, 40});
  cache()->SyncForTest();
  EXPECT_EQ(cache()->GetWindowAtPoint({125, 125}, root()), Window::None);
  connection()->DeleteProperty(a, gtk_frame_extents);

  // Hit test in XShape region.
  if (shape.present()) {
    EXPECT_EQ(cache()->GetWindowAtPoint({150, 150}, root()), a);
    for (auto kind : {Shape::Sk::Bounding, Shape::Sk::Input}) {
      // Set to an empty bounding shape.
      shape.Rectangles({.destination_kind = kind, .destination_window = a});
      cache()->SyncForTest();
      EXPECT_EQ(cache()->GetWindowAtPoint({150, 150}, root()), Window::None);
      shape.Mask({.destination_kind = kind, .destination_window = a});
    }
  }

  // Test window stacking order.
  Window b = CreateWindow(root());
  connection()->ConfigureWindow({
      .window = b,
      .x = 100,
      .y = 100,
      .width = 100,
      .height = 100,
  });
  connection()->MapWindow(b);
  connection()->SetStringProperty(b, Atom::WM_NAME, Atom::STRING, "b");
  cache()->SyncForTest();
  EXPECT_EQ(cache()->GetWindowAtPoint({150, 150}, root()), b);

  // Test window nesting.
  Window c = CreateWindow(b);
  connection()->ConfigureWindow({
      .window = c,
      .x = 0,
      .y = 0,
      .width = 100,
      .height = 100,
  });
  connection()->MapWindow(c);
  connection()->SetStringProperty(c, Atom::WM_NAME, Atom::STRING, "c");
  connection()->DeleteProperty(b, Atom::WM_NAME);
  cache()->SyncForTest();
  EXPECT_EQ(cache()->GetWindowAtPoint({150, 150}, root()), c);
}

// Regression test for https://crbug.com/1316735
// If both a parent and child window have a WM_NAME, the child window
// should be returned by GetWindowAtPoint().
TEST_F(WindowCacheTest, NestedWmName) {
  connection()->MapWindow(root());
  connection()->SetStringProperty(root(), Atom::WM_NAME, Atom::STRING, "root");

  Window a = CreateWindow(root());
  connection()->MapWindow(a);
  connection()->SetStringProperty(a, Atom::WM_NAME, Atom::STRING, "a");

  cache()->SyncForTest();
  EXPECT_EQ(cache()->GetWindowAtPoint({100, 100}, root()), a);
}

}  // namespace x11
