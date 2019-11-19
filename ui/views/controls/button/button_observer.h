// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_BUTTON_OBSERVER_H_
#define UI_VIEWS_CONTROLS_BUTTON_BUTTON_OBSERVER_H_

#include "base/observer_list_types.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/views_export.h"

namespace views {

class VIEWS_EXPORT ButtonObserver : public base::CheckedObserver {
 public:
  virtual void OnHighlightChanged(views::Button* observed_button,
                                  bool highlighted) {}

  virtual void OnStateChanged(views::Button* observed_button,
                              views::Button::ButtonState old_state) {}

 protected:
  ~ButtonObserver() override = default;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_BUTTON_BUTTON_OBSERVER_H_
