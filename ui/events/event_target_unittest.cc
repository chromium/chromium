// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/event_target.h"

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/events_test_utils.h"
#include "ui/events/test/test_event_handler.h"
#include "ui/events/test/test_event_processor.h"
#include "ui/events/test/test_event_target.h"

namespace ui {

namespace {

TEST(EventTargetTest, AddsAndRemovesHandlers) {
  test::TestEventTarget target;
  EventTargetTestApi test_api(&target);
  test::TestEventHandler handler;
  EventHandlerList list;

  // Try at the default priority
  target.AddPreTargetHandler(&handler);
  list = test_api.GetPreTargetHandlers();
  ASSERT_EQ(1u, list.size());
  target.RemovePreTargetHandler(&handler);
  list = test_api.GetPreTargetHandlers();
  ASSERT_EQ(0u, list.size());

  // Try at a different priority
  target.AddPreTargetHandler(&handler, EventTarget::Priority::kAccessibility);
  list = test_api.GetPreTargetHandlers();
  ASSERT_EQ(1u, list.size());
  target.RemovePreTargetHandler(&handler);
  list = test_api.GetPreTargetHandlers();
  ASSERT_EQ(0u, list.size());

  // Doesn't crash if we remove a handler that doesn't exist.
  target.RemovePreTargetHandler(&handler);
}

TEST(EventTargetTest, HandlerOrdering) {
  test::TestEventTarget target;
  EventTargetTestApi test_api(&target);
  test::TestEventHandler default_handler;
  test::TestEventHandler system_handler;
  test::TestEventHandler a11y_handler;
  EventHandlerList list;

  // Try adding default then system then a11y, which is backwards of the
  // desired order.
  target.AddPreTargetHandler(&default_handler, EventTarget::Priority::kDefault);
  target.AddPreTargetHandler(&system_handler, EventTarget::Priority::kSystem);
  target.AddPreTargetHandler(&a11y_handler,
                             EventTarget::Priority::kAccessibility);
  list = test_api.GetPreTargetHandlers();
  ASSERT_EQ(3u, list.size());
  EXPECT_EQ(list[0], &a11y_handler);
  EXPECT_EQ(list[1], &system_handler);
  EXPECT_EQ(list[2], &default_handler);
  target.RemovePreTargetHandler(&default_handler);
  target.RemovePreTargetHandler(&system_handler);
  target.RemovePreTargetHandler(&a11y_handler);
}

TEST(EventTargetTest, HandlerOrderingComplex) {
  test::TestEventTarget target;
  EventTargetTestApi test_api(&target);
  test::TestEventHandler default_handler_1;
  test::TestEventHandler default_handler_2;
  test::TestEventHandler system_handler_1;
  test::TestEventHandler system_handler_2;
  test::TestEventHandler system_handler_3;
  test::TestEventHandler a11y_handler_1;
  test::TestEventHandler a11y_handler_2;
  EventHandlerList list;

  // Adding a new system or accessibility handler will insert it before others
  // of its type. Adding a new default handler puts it at the end of the list,
  // for historical reasons. Re-arranging default handlers causes test failures
  // in many unittests and may also cause real-life bugs, so for now default
  // still is at the end of the list.
  target.AddPreTargetHandler(&system_handler_3, EventTarget::Priority::kSystem);
  target.AddPreTargetHandler(&default_handler_1,
                             EventTarget::Priority::kDefault);
  target.AddPreTargetHandler(&system_handler_2, EventTarget::Priority::kSystem);
  target.AddPreTargetHandler(&a11y_handler_2,
                             EventTarget::Priority::kAccessibility);
  target.AddPreTargetHandler(&system_handler_1, EventTarget::Priority::kSystem);
  target.AddPreTargetHandler(&default_handler_2,
                             EventTarget::Priority::kDefault);
  target.AddPreTargetHandler(&a11y_handler_1,
                             EventTarget::Priority::kAccessibility);
  list = test_api.GetPreTargetHandlers();

  ASSERT_EQ(7u, list.size());
  EXPECT_EQ(list[0], &a11y_handler_1);
  EXPECT_EQ(list[1], &a11y_handler_2);
  EXPECT_EQ(list[2], &system_handler_1);
  EXPECT_EQ(list[3], &system_handler_2);
  EXPECT_EQ(list[4], &system_handler_3);
  EXPECT_EQ(list[5], &default_handler_1);
  EXPECT_EQ(list[6], &default_handler_2);

  target.RemovePreTargetHandler(&system_handler_3);
  target.RemovePreTargetHandler(&default_handler_1);
  target.RemovePreTargetHandler(&system_handler_2);
  target.RemovePreTargetHandler(&a11y_handler_2);
  target.RemovePreTargetHandler(&system_handler_1);
  target.RemovePreTargetHandler(&default_handler_2);
  target.RemovePreTargetHandler(&a11y_handler_1);
}

TEST(EventTargetTest, HandlerOrderingAcrossEventTargets) {
  // Child needs to be a unique pointer so that TestEventTarget::AddChild works.
  std::unique_ptr<test::TestEventTarget> child =
      std::make_unique<test::TestEventTarget>();
  test::TestEventTarget parent;
  test::TestEventHandler default_handler_1;
  test::TestEventHandler default_handler_2;
  test::TestEventHandler default_handler_3;
  test::TestEventHandler system_handler_1;
  test::TestEventHandler system_handler_2;
  test::TestEventHandler system_handler_3;
  test::TestEventHandler a11y_handler_1;
  test::TestEventHandler a11y_handler_2;
  test::TestEventHandler a11y_handler_3;

  // Parent handlers should be called before children handlers.
  parent.AddPreTargetHandler(&default_handler_1,
                             EventTarget::Priority::kDefault);
  parent.AddPreTargetHandler(&system_handler_2, EventTarget::Priority::kSystem);
  parent.AddPreTargetHandler(&a11y_handler_2,
                             EventTarget::Priority::kAccessibility);

  child->AddPreTargetHandler(&default_handler_3,
                             EventTarget::Priority::kDefault);
  child->AddPreTargetHandler(&a11y_handler_3,
                             EventTarget::Priority::kAccessibility);
  child->AddPreTargetHandler(&system_handler_3, EventTarget::Priority::kSystem);

  parent.AddPreTargetHandler(&system_handler_1, EventTarget::Priority::kSystem);
  parent.AddPreTargetHandler(&default_handler_2,
                             EventTarget::Priority::kDefault);
  parent.AddPreTargetHandler(&a11y_handler_1,
                             EventTarget::Priority::kAccessibility);

  // Connect the parent and child in a EventTargetTestAPI.
  EventTargetTestApi test_api(child.get());
  test::TestEventTarget* child_ptr = child.get();
  parent.AddChild(std::move(child));

  EventHandlerList list;
  list = test_api.GetPreTargetHandlers();

  ASSERT_EQ(9u, list.size());
  // Parent handlers are called before child handlers, so a11y_handler_1 and
  // 2 should be called before a11y_handler3, and similarly all the system and
  // default handlers added to the parent should be called before those added
  // to the child.
  // In addition, all a11y handlers should be called before all system handlers,
  // which should be called before all default handlers.
  EXPECT_EQ(list[0], &a11y_handler_1);
  EXPECT_EQ(list[1], &a11y_handler_2);
  EXPECT_EQ(list[2], &a11y_handler_3);
  EXPECT_EQ(list[3], &system_handler_1);
  EXPECT_EQ(list[4], &system_handler_2);
  EXPECT_EQ(list[5], &system_handler_3);
  EXPECT_EQ(list[6], &default_handler_1);
  EXPECT_EQ(list[7], &default_handler_2);
  EXPECT_EQ(list[8], &default_handler_3);

  parent.RemovePreTargetHandler(&default_handler_1);
  parent.RemovePreTargetHandler(&system_handler_2);
  parent.RemovePreTargetHandler(&a11y_handler_2);

  child_ptr->RemovePreTargetHandler(&default_handler_3);
  child_ptr->RemovePreTargetHandler(&a11y_handler_3);
  child_ptr->RemovePreTargetHandler(&system_handler_3);

  parent.RemovePreTargetHandler(&system_handler_1);
  parent.RemovePreTargetHandler(&default_handler_2);
  parent.RemovePreTargetHandler(&a11y_handler_1);
}

}  // namespace

}  // namespace ui
