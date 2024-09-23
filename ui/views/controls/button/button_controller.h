// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_BUTTON_CONTROLLER_H_
#define UI_VIEWS_CONTROLS_BUTTON_BUTTON_CONTROLLER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "ui/events/event.h"
#include "ui/views/controls/button/button.h"

namespace views {

// Handles logic not related to the visual aspects of a Button such as event
// handling and state changes.
class VIEWS_EXPORT ButtonController {
 public:
  ButtonController(Button* button,
                   std::unique_ptr<ButtonControllerDelegate> delegate);

  ButtonController(const ButtonController&) = delete;
  ButtonController& operator=(const ButtonController&) = delete;

  virtual ~ButtonController();

  // An enum describing the events on which a button should notify its listener.
  enum class NotifyAction {
    kOnPress,
    kOnRelease,
  };

  Button* button() { return button_; }

  // Sets the event on which the button's listener should be notified.
  void set_notify_action(NotifyAction notify_action) {
    notify_action_ = notify_action;
  }

  NotifyAction notify_action() const { return notify_action_; }

  // Methods that parallel View::On<Event> handlers:
  virtual bool OnMousePressed(const ui::MouseEvent& event);
  virtual void OnMouseReleased(const ui::MouseEvent& event);
  virtual void OnMouseMoved(const ui::MouseEvent& event);
  virtual void OnMouseEntered(const ui::MouseEvent& event);
  virtual void OnMouseExited(const ui::MouseEvent& event);
  virtual bool OnKeyPressed(const ui::KeyEvent& event);
  virtual bool OnKeyReleased(const ui::KeyEvent& event);
  virtual void OnGestureEvent(ui::GestureEvent* event);

  // Cause the button to notify the listener that a click occurred.
  virtual void NotifyClick();

  virtual void UpdateButtonAccessibleDefaultActionVerb();

  // Methods that parallel respective methods in Button:
  virtual bool IsTriggerableEvent(const ui::Event& event);

 protected:
  ButtonControllerDelegate* delegate() {
    return button_controller_delegate_.get();
  }

 private:
  const raw_ptr<Button> button_;

  // TODO(cyan): Remove |button_| and access everything via the delegate.
  std::unique_ptr<ButtonControllerDelegate> button_controller_delegate_;

  // The event on which the button's listener should be notified.
  NotifyAction notify_action_ = NotifyAction::kOnRelease;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_BUTTON_BUTTON_CONTROLLER_H_
