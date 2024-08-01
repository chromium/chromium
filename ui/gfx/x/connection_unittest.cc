// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/x/connection.h"

#include "base/memory/ref_counted_memory.h"
#include "base/numerics/safe_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/x/event.h"
#include "ui/gfx/x/future.h"
#include "ui/gfx/x/xproto.h"

namespace x11 {

namespace {

Window CreateWindow(Connection* connection) {
  Window window = connection->GenerateId<Window>();
  auto create_window_future = connection->CreateWindow({
      .depth = connection->default_root_depth().depth,
      .wid = window,
      .parent = connection->default_screen().root,
      .width = 1,
      .height = 1,
      .override_redirect = Bool32(true),
  });
  auto create_window_response = create_window_future.Sync();
  EXPECT_FALSE(create_window_response.error);
  return window;
}

}  // namespace

// Connection setup and teardown.
TEST(X11ConnectionTest, Basic) {
  Connection connection;
  ASSERT_TRUE(connection.Ready());
}

TEST(X11ConnectionTest, Request) {
  Connection connection;
  ASSERT_TRUE(connection.Ready());

  Window window = CreateWindow(&connection);

  auto attributes = connection.GetWindowAttributes({window}).Sync();
  ASSERT_TRUE(attributes);
  EXPECT_EQ(attributes->map_state, MapState::Unmapped);
  EXPECT_TRUE(attributes->override_redirect);

  auto geometry = connection.GetGeometry(window).Sync();
  ASSERT_TRUE(geometry);
  EXPECT_EQ(geometry->x, 0);
  EXPECT_EQ(geometry->y, 0);
  EXPECT_EQ(geometry->width, 1u);
  EXPECT_EQ(geometry->height, 1u);
}

TEST(X11ConnectionTest, Event) {
  Connection connection;
  ASSERT_TRUE(connection.Ready());

  Window window = CreateWindow(&connection);

  auto cwa_future = connection.ChangeWindowAttributes({
      .window = window,
      .event_mask = EventMask::PropertyChange,
  });
  EXPECT_FALSE(cwa_future.Sync().error);

  std::vector<uint8_t> data{0};
  auto prop_future = connection.ChangeProperty({
      .window = static_cast<Window>(window),
      .property = Atom::WM_NAME,
      .type = Atom::STRING,
      .format = CHAR_BIT,
      .data_len = base::checked_cast<uint32_t>(data.size()),
      .data = base::MakeRefCounted<base::RefCountedBytes>(std::move(data)),
  });
  EXPECT_FALSE(prop_future.Sync().error);

  connection.ReadResponses();
  ASSERT_EQ(connection.events().size(), 1u);
  auto* prop = connection.events().front().As<PropertyNotifyEvent>();
  ASSERT_TRUE(prop);
  EXPECT_EQ(prop->atom, Atom::WM_NAME);
  EXPECT_EQ(prop->state, Property::NewValue);
}

TEST(X11ConnectionTest, Error) {
  Connection connection;
  ASSERT_TRUE(connection.Ready());

  Window invalid_window = connection.GenerateId<Window>();

  auto geometry = connection.GetGeometry(invalid_window).Sync();
  ASSERT_FALSE(geometry);
  auto* error = geometry.error.get();
  ASSERT_TRUE(error);
  // TODO(thomasanderson): Implement As<> for errors, similar to events.
  auto* drawable_error = reinterpret_cast<DrawableError*>(error);
  EXPECT_EQ(drawable_error->bad_value, static_cast<uint32_t>(invalid_window));
}

TEST(X11ConnectionTest, LargeQueryTree) {
  Connection connection;
  ASSERT_TRUE(connection.Ready());

  Window root = CreateWindow(&connection);
  for (size_t i = 0; i < 0x10000; i++) {
    connection.CreateWindow({
        .depth = connection.default_root_depth().depth,
        .wid = connection.GenerateId<Window>(),
        .parent = root,
        .width = 1,
        .height = 1,
        .override_redirect = Bool32(true),
    });
  }

  // Ensure large QueryTree requests don't cause a crash.
  connection.QueryTree(root).Sync();
}

}  // namespace x11
