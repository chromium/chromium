// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_SLIDE_OUT_CONTROLLER_DELEGATE_H_
#define UI_VIEWS_ANIMATION_SLIDE_OUT_CONTROLLER_DELEGATE_H_

#include "ui/views/views_export.h"

namespace ui {
class Layer;
}  // namespace ui

namespace views {

class VIEWS_EXPORT SlideOutControllerDelegate {
 public:
  // Returns the layer for slide operations.
  virtual ui::Layer* GetSlideOutLayer() = 0;

  // Called when a manual slide starts.
  virtual void OnSlideStarted() {}

  // Called when a manual slide updates or ends. The argument is true if the
  // slide starts or in progress, false if it ends.
  virtual void OnSlideChanged(bool in_progress) = 0;

  // Called when user intends to close the View by sliding it out.
  virtual void OnSlideOut() = 0;

 protected:
  virtual ~SlideOutControllerDelegate() = default;
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_SLIDE_OUT_CONTROLLER_DELEGATE_H_
