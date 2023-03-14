// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_MOCK_DRAG_CONTROLLER_H_
#define UI_VIEWS_TEST_MOCK_DRAG_CONTROLLER_H_

#include "ui/views/drag_controller.h"

#include "testing/gmock/include/gmock/gmock.h"

namespace gfx {
class Point;
}  // namespace gfx

namespace ui {
class OSExchangeData;
}  // namespace ui

namespace views {
class View;

// A mocked drag controller for testing.
class MockDragController : public DragController {
 public:
  MockDragController();
  MockDragController(const MockDragController&) = delete;
  MockDragController& operator=(const MockDragController&) = delete;
  ~MockDragController() override;

  // DragController:
  void WriteDragDataForView(View* sender,
                            const gfx::Point& press_pt,
                            ui::OSExchangeData* data) override;
  int GetDragOperationsForView(View* sender, const gfx::Point& p) override;
  bool CanStartDragForView(View* sender,
                           const gfx::Point& press_pt,
                           const gfx::Point& p) override;
  MOCK_METHOD(void, OnWillStartDragForView, (View * dragged_view), (override));
};

}  // namespace views

#endif  // UI_VIEWS_TEST_MOCK_DRAG_CONTROLLER_H_
