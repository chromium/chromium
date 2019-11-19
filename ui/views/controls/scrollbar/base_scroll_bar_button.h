// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_SCROLLBAR_BASE_SCROLL_BAR_BUTTON_H_
#define UI_VIEWS_CONTROLS_SCROLLBAR_BASE_SCROLL_BAR_BUTTON_H_

#include "ui/views/controls/button/button.h"

#include "base/macros.h"
#include "build/build_config.h"
#include "ui/views/repeat_controller.h"

namespace views {

///////////////////////////////////////////////////////////////////////////////
//
// ScrollBarButton
//
//  A button that activates on mouse pressed rather than released, and that
//  continues to fire the clicked action as the mouse button remains pressed
//  down on the button.
//
///////////////////////////////////////////////////////////////////////////////
class VIEWS_EXPORT BaseScrollBarButton : public Button {
 public:
  METADATA_HEADER(BaseScrollBarButton);

  explicit BaseScrollBarButton(ButtonListener* listener);
  ~BaseScrollBarButton() override;

 protected:
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseCaptureLost() override;

 private:
  void RepeaterNotifyClick();

  // The repeat controller that we use to repeatedly click the button when the
  // mouse button is down.
  RepeatController repeater_;

  DISALLOW_COPY_AND_ASSIGN(BaseScrollBarButton);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_SCROLLBAR_BASE_SCROLL_BAR_BUTTON_H_
