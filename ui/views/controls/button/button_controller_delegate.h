// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_BUTTON_CONTROLLER_DELEGATE_H_
#define UI_VIEWS_CONTROLS_BUTTON_BUTTON_CONTROLLER_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "ui/views/views_export.h"

namespace gfx {
class Point;
}

namespace ui {
class Event;
}

namespace views {

class Button;
class InkDrop;

// Captures the Button and View methods required for sharing the logic in
// ButtonController between different Button types.
class VIEWS_EXPORT ButtonControllerDelegate {
 public:
  explicit ButtonControllerDelegate(Button* button) : button_(button) {}

  ButtonControllerDelegate(const ButtonControllerDelegate&) = delete;
  ButtonControllerDelegate& operator=(const ButtonControllerDelegate&) = delete;

  virtual ~ButtonControllerDelegate() = default;

  // Parallels methods in views::Button:
  virtual void RequestFocusFromEvent() = 0;
  virtual void NotifyClick(const ui::Event& event) = 0;
  virtual void OnClickCanceled(const ui::Event& event) = 0;
  virtual bool IsTriggerableEvent(const ui::Event& event) = 0;
  virtual bool ShouldEnterPushedState(const ui::Event& event) = 0;
  virtual bool ShouldEnterHoveredState() = 0;

  // Parallels method views::InkDropEventHandler::GetInkDrop:
  virtual InkDrop* GetInkDrop() = 0;

  // Parallels methods in views::View:
  virtual int GetDragOperations(const gfx::Point& press_pt) = 0;
  virtual bool InDrag() = 0;

 protected:
  Button* button() { return button_; }

 private:
  raw_ptr<Button> button_;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_BUTTON_BUTTON_CONTROLLER_DELEGATE_H_
