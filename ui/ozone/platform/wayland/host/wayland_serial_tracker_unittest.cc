// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/ozone/platform/wayland/host/wayland_serial_tracker.h"

namespace wl {

TEST(WaylandSerialTrackerTest, Basics) {
  SerialTracker serial_tracker;
  std::optional<Serial> serial;

  // Ensure it is initialized as expected.
  EXPECT_FALSE(serial_tracker.GetSerial(SerialType::kMouseEnter));
  EXPECT_FALSE(serial_tracker.GetSerial(SerialType::kMousePress));
  EXPECT_FALSE(serial_tracker.GetSerial(SerialType::kTouchPress));
  EXPECT_FALSE(serial_tracker.GetSerial(SerialType::kKeyPress));

  // Update MouseEnter serial and ensure its properly returned when queried.
  serial_tracker.UpdateSerial(SerialType::kMouseEnter, 1u);
  serial = serial_tracker.GetSerial(SerialType::kMouseEnter);
  ASSERT_TRUE(serial);
  EXPECT_EQ(1u, serial->value);
  EXPECT_EQ(SerialType::kMouseEnter, serial->type);
  // Ensure other entries keep unchanged.
  EXPECT_FALSE(serial_tracker.GetSerial(SerialType::kMousePress));
  EXPECT_FALSE(serial_tracker.GetSerial(SerialType::kTouchPress));
  EXPECT_FALSE(serial_tracker.GetSerial(SerialType::kKeyPress));
  // Verify ResetSerial works.
  serial_tracker.ResetSerial(SerialType::kMouseEnter);
  EXPECT_FALSE(serial_tracker.GetSerial(SerialType::kMouseEnter));

  // Update MousePress serial and ensure its properly returned when queried.
  serial_tracker.UpdateSerial(SerialType::kMousePress, 2u);
  serial = serial_tracker.GetSerial(SerialType::kMousePress);
  ASSERT_TRUE(serial);
  EXPECT_EQ(2u, serial->value);
  EXPECT_EQ(SerialType::kMousePress, serial->type);
  // Ensure other entries keep unchanged.
  EXPECT_FALSE(serial_tracker.GetSerial(SerialType::kMouseEnter));
  EXPECT_FALSE(serial_tracker.GetSerial(SerialType::kTouchPress));
  EXPECT_FALSE(serial_tracker.GetSerial(SerialType::kKeyPress));

  // Verify subsequent UpdateSerial calls properly overwrites previously set
  // values.
  serial_tracker.UpdateSerial(SerialType::kMousePress, 3u);
  serial = serial_tracker.GetSerial(SerialType::kMousePress);
  ASSERT_TRUE(serial);
  EXPECT_EQ(3u, serial->value);
  EXPECT_EQ(SerialType::kMousePress, serial->type);
  // Ensure other entries keep unchanged.
  EXPECT_FALSE(serial_tracker.GetSerial(SerialType::kMouseEnter));
  EXPECT_FALSE(serial_tracker.GetSerial(SerialType::kTouchPress));
  EXPECT_FALSE(serial_tracker.GetSerial(SerialType::kKeyPress));
}

// Verifies GetSerial() behaves correctly when multiple values are set and
// queried.
TEST(WaylandSerialTrackerTest, Queries) {
  base::test::TaskEnvironment env{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  SerialTracker serial_tracker;
  std::optional<Serial> serial;

  serial_tracker.UpdateSerial(SerialType::kMouseEnter, 1u);
  env.FastForwardBy(base::Milliseconds(100));
  serial_tracker.UpdateSerial(SerialType::kMousePress, 2u);
  env.FastForwardBy(base::Milliseconds(100));
  serial_tracker.UpdateSerial(SerialType::kTouchPress, 3u);
  env.FastForwardBy(base::Milliseconds(100));
  serial_tracker.UpdateSerial(SerialType::kKeyPress, 4u);

  serial = serial_tracker.GetSerial(
      {SerialType::kMouseEnter, SerialType::kMousePress});
  ASSERT_TRUE(serial);
  EXPECT_EQ(2u, serial->value);
  EXPECT_EQ(SerialType::kMousePress, serial->type);

  serial = serial_tracker.GetSerial(
      {SerialType::kMousePress, SerialType::kMouseEnter});
  ASSERT_TRUE(serial);
  EXPECT_EQ(2u, serial->value);
  EXPECT_EQ(SerialType::kMousePress, serial->type);

  serial = serial_tracker.GetSerial({SerialType::kMousePress,
                                     SerialType::kMouseEnter,
                                     SerialType::kTouchPress});
  ASSERT_TRUE(serial);
  EXPECT_EQ(3u, serial->value);
  EXPECT_EQ(SerialType::kTouchPress, serial->type);

  serial = serial_tracker.GetSerial({SerialType::kMousePress,
                                     SerialType::kMouseEnter,
                                     SerialType::kKeyPress});
  ASSERT_TRUE(serial);
  EXPECT_EQ(4u, serial->value);
  EXPECT_EQ(SerialType::kKeyPress, serial->type);

  serial_tracker.ResetSerial(SerialType::kMouseEnter);
  serial_tracker.ResetSerial(SerialType::kTouchPress);
  EXPECT_FALSE(serial_tracker.GetSerial(
      {SerialType::kMouseEnter, SerialType::kTouchPress}));

  serial = serial_tracker.GetSerial(
      {SerialType::kTouchPress, SerialType::kMousePress});
  ASSERT_TRUE(serial);
  EXPECT_EQ(2u, serial->value);
  EXPECT_EQ(SerialType::kMousePress, serial->type);

  serial = serial_tracker.GetSerial({SerialType::kTouchPress,
                                     SerialType::kMousePress,
                                     SerialType::kKeyPress});
  ASSERT_TRUE(serial);
  EXPECT_EQ(4u, serial->value);
  EXPECT_EQ(SerialType::kKeyPress, serial->type);
}

}  // namespace wl
