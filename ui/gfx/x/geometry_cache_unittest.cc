// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/x/geometry_cache.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/xproto.h"

namespace x11 {

namespace {

class ScopedWindow {
 public:
  ScopedWindow(Connection* connection, Window parent, const gfx::Rect& bounds)
      : connection_(connection) {
    id_ = connection_->GenerateId<Window>();
    connection_->CreateWindow({
        .wid = id_,
        .parent = parent,
        .x = static_cast<int16_t>(bounds.x()),
        .y = static_cast<int16_t>(bounds.y()),
        .width = static_cast<uint16_t>(bounds.width()),
        .height = static_cast<uint16_t>(bounds.height()),
        .c_class = WindowClass::InputOnly,
        .override_redirect = Bool32(true),
    });
  }

  ScopedWindow(ScopedWindow&&) = delete;
  ScopedWindow& operator=(ScopedWindow&&) = delete;

  ~ScopedWindow() { connection_->DestroyWindow(id_); }

  Window id() const { return id_; }

 private:
  const raw_ptr<Connection> connection_;
  Window id_;
};

}  // namespace

TEST(GeometryCacheTest, ConstructAndDestruct) {
  Connection* connection = Connection::Get();
  ScopedWindow window(connection, connection->default_root(),
                      gfx::Rect(12, 34, 56, 78));
  GeometryCache geometry_cache(
      connection, window.id(),
      base::BindRepeating([](const std::optional<gfx::Rect>& old_bounds,
                             const gfx::Rect& new_bounds) {}));
}

TEST(GeometryCacheTest, GetBounds) {
  Connection* connection = Connection::Get();
  gfx::Rect bounds(12, 34, 56, 78);
  ScopedWindow window(connection, connection->default_root(), bounds);
  GeometryCache geometry_cache(
      connection, window.id(),
      base::BindRepeating([](const std::optional<gfx::Rect>& old_bounds,
                             const gfx::Rect& new_bounds) {}));

  // Calling GetBoundsPx() should return the initial bounds of the window.
  EXPECT_EQ(geometry_cache.GetBoundsPx(), bounds);

  // Simulate setting window geometry.
  gfx::Rect new_geometry(10, 10, 100, 100);
  connection->ConfigureWindow({
      .window = window.id(),
      .x = new_geometry.x(),
      .y = new_geometry.y(),
      .width = new_geometry.width(),
      .height = new_geometry.height(),
  });
  connection->Sync();
  connection->DispatchAll();

  // The geometry cache should now reflect the new geometry.
  EXPECT_EQ(geometry_cache.GetBoundsPx(), new_geometry);
}

TEST(GeometryCacheTest, BoundsChangedCallback) {
  Connection* connection = Connection::Get();
  gfx::Rect bounds(12, 34, 56, 78);
  ScopedWindow window(connection, connection->default_root(), bounds);

  std::optional<gfx::Rect> last_bounds;
  auto bounds_changed_callback = [](std::optional<gfx::Rect>* last_bounds,
                                    const std::optional<gfx::Rect>& old_bounds,
                                    const gfx::Rect& new_bounds) {
    *last_bounds = new_bounds;
  };

  GeometryCache geometry_cache(
      connection, window.id(),
      base::BindRepeating(bounds_changed_callback, &last_bounds));

  // Initially, no bounds change should have been recorded.
  EXPECT_FALSE(last_bounds.has_value());
  EXPECT_EQ(geometry_cache.GetBoundsPx(), bounds);

  // `last_bounds` should have gotten set after the call to GetBoundsPx().
  // Reset it for the next part of the test.
  EXPECT_TRUE(last_bounds.has_value());
  last_bounds = std::nullopt;

  // Simulate setting window geometry.
  gfx::Rect new_geometry(10, 10, 100, 100);
  connection->ConfigureWindow({
      .window = window.id(),
      .x = new_geometry.x(),
      .y = new_geometry.y(),
      .width = new_geometry.width(),
      .height = new_geometry.height(),
  });
  connection->Sync();
  connection->DispatchAll();

  // The callback should have been invoked with the new geometry.
  ASSERT_TRUE(last_bounds.has_value());
  EXPECT_EQ(last_bounds.value(), new_geometry);
}

TEST(GeometryCacheTest, NestedWindows) {
  Connection* connection = Connection::Get();
  gfx::Rect parent_bounds(12, 34, 56, 78);
  ScopedWindow parent_window(connection, connection->default_root(),
                             parent_bounds);

  gfx::Rect child_bounds(5, 5, 30, 30);
  ScopedWindow child_window(connection, parent_window.id(), child_bounds);

  // Set up the GeometryCache for the child window.
  GeometryCache child_geometry_cache(
      connection, child_window.id(),
      base::BindRepeating([](const std::optional<gfx::Rect>& old_bounds,
                             const gfx::Rect& new_bounds) {}));

  // Initial bounds should be the sum of child window position and parent window
  // position.
  EXPECT_EQ(child_geometry_cache.GetBoundsPx(),
            child_bounds + gfx::Vector2d(parent_bounds.x(), parent_bounds.y()));

  // Simulate moving the child window within the parent window.
  gfx::Rect new_child_bounds(10, 10, 40, 40);
  connection->ConfigureWindow({
      .window = child_window.id(),
      .x = new_child_bounds.x(),
      .y = new_child_bounds.y(),
      .width = new_child_bounds.width(),
      .height = new_child_bounds.height(),
  });
  connection->Sync();
  connection->DispatchAll();

  // Verify the child window's new bounds are updated correctly.
  EXPECT_EQ(
      child_geometry_cache.GetBoundsPx(),
      new_child_bounds + gfx::Vector2d(parent_bounds.x(), parent_bounds.y()));
}

}  // namespace x11
