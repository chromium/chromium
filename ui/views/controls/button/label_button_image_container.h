// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_LABEL_BUTTON_IMAGE_CONTAINER_H_
#define UI_VIEWS_CONTROLS_BUTTON_LABEL_BUTTON_IMAGE_CONTAINER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "ui/views/view_tracker.h"
#include "ui/views/views_export.h"

namespace views {

class View;
class LabelButton;

// Abstract interface used by LabelButton to handle updates to the button
// image(s). This interface lets callers configure the button to have one image
// or multiple without LabelButton itself needing to understand the details.
// LabelButton can simply call CreateView() to get a view it can add as a child,
// then call UpdateImage() any time state changes in such a way that the
// image(s) might need to be updated. Concrete instances of this class are
// responsible for laying out any image(s) and updating them in response to
// calls to UpdateImage(), as well as any other relevant signals.
class VIEWS_EXPORT LabelButtonImageContainer {
 public:
  LabelButtonImageContainer() = default;
  LabelButtonImageContainer(const LabelButtonImageContainer&) = delete;
  LabelButtonImageContainer& operator=(const LabelButtonImageContainer&) =
      delete;
  virtual ~LabelButtonImageContainer() = default;

  // Returns a view holding whatever image(s) are desired. Calls to
  // UpdateImage() outside this view's lifetime will have no effect.
  virtual std::unique_ptr<View> CreateView() = 0;

  // Gets a pointer to a previously created view. Returns nullptr if no view was
  // created.
  virtual View* GetView() = 0;
  virtual const View* GetView() const = 0;

  // Called when image(s) in the created view may need updating. `button` is the
  // LabelButton which is displaying the images. Subclasses should respond to
  // this by updating any image(s) appropriately based on the button's current
  // state and any other state the container is tracking.
  virtual void UpdateImage(const LabelButton* button) = 0;
};

// The common-case implementation of LabelButtonImageContainer, which provides a
// single image that tracks the LabelButton's ButtonState.
class VIEWS_EXPORT SingleImageContainer final
    : public LabelButtonImageContainer {
 public:
  SingleImageContainer() = default;
  SingleImageContainer(const SingleImageContainer&) = delete;
  SingleImageContainer& operator=(const SingleImageContainer&) = delete;
  ~SingleImageContainer() override = default;

  // LabelButtonImageContainer
  std::unique_ptr<View> CreateView() override;
  View* GetView() override;
  const View* GetView() const override;
  void UpdateImage(const LabelButton* button) override;

 private:
  views::ViewTracker image_view_tracker_;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_BUTTON_LABEL_BUTTON_IMAGE_CONTAINER_H_
