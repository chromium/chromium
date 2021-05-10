// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_BUTTON_CONTROLLER_DELEGATE_H_
#define UI_VIEWS_CONTROLS_BUTTON_BUTTON_CONTROLLER_DELEGATE_H_

namespace views {

class Button;

// Captures the Button and View methods required for sharing the logic in
// ButtonController between different Button types.
class VIEWS_EXPORT ButtonControllerDelegate {
 public:
  explicit ButtonControllerDelegate(Button* button) : button_(button) {}

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
  Button* button_;

  DISALLOW_COPY_AND_ASSIGN(ButtonControllerDelegate);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_BUTTON_BUTTON_CONTROLLER_DELEGATE_H_
