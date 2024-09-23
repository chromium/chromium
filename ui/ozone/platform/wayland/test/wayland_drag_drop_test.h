// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_WAYLAND_DRAG_DROP_TEST_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_WAYLAND_DRAG_DROP_TEST_H_

#include <cstdint>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/dragdrop/os_exchange_data_provider_factory_ozone.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"

namespace gfx {
class Point;
}

namespace ui {

class WaylandWindow;

class TestWaylandOSExchangeDataProvideFactory
    : public OSExchangeDataProviderFactoryOzone {
 public:
  TestWaylandOSExchangeDataProvideFactory();
  ~TestWaylandOSExchangeDataProvideFactory() override;

  std::unique_ptr<OSExchangeDataProvider> CreateProvider() override;
};

// Base class for Wayland drag-and-drop tests. Public methods allow test code to
// emulate dnd-related events from the test compositor and can be used in both
// data and window dragging test cases.
class WaylandDragDropTest : public WaylandTest {
 public:
  WaylandDragDropTest();
  WaylandDragDropTest(const WaylandDragDropTest&) = delete;
  WaylandDragDropTest& operator=(const WaylandDragDropTest&) = delete;
  ~WaylandDragDropTest() override;

  // These are public for convenience, as they must be callable from lambda
  // functions, usually posted to task queue while the drag loop runs.
  void SendDndEnter(WaylandWindow* window, const gfx::Point& location);
  void SendDndLeave();
  void SendDndMotion(const gfx::Point& location);
  void SendDndDropPerformed();
  void SendDndFinished();
  void SendDndCancelled();
  void SendDndAction(uint32_t action);
  void ReadAndCheckData(const std::string& mime_type,
                        const std::string& expected_data);

  virtual void SendPointerEnter(WaylandWindow* window,
                                MockPlatformWindowDelegate* delegate);
  virtual void SendPointerLeave(WaylandWindow* window,
                                MockPlatformWindowDelegate* delegate);
  virtual void SendPointerButton(WaylandWindow* window,
                                 MockPlatformWindowDelegate* delegate,
                                 int button,
                                 bool pressed);

  virtual void SendTouchDown(WaylandWindow* window,
                             MockPlatformWindowDelegate* delegate,
                             int id,
                             const gfx::Point& location);
  virtual void SendTouchUp(int id);
  virtual void SendTouchMotion(WaylandWindow* window,
                               MockPlatformWindowDelegate* delegate,
                               int id,
                               const gfx::Point& location);

 protected:
  // WaylandTest:
  void SetUp() override;

  void ScheduleTestTask(base::OnceClosure test_task);

  WaylandWindowManager* window_manager() const {
    return connection_->window_manager();
  }

 private:
  void MaybeRunScheduledTasks();
  void RunTestTask(base::OnceClosure test_task);

  TestWaylandOSExchangeDataProvideFactory os_exchange_factory_;

  // This is used to ensure FIFO of tasks and that they complete to run before
  // a next task is posted.
  bool is_task_running_ = false;
  std::vector<base::OnceClosure> scheduled_tasks_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_WAYLAND_DRAG_DROP_TEST_H_
