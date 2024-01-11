// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_LABEL_BUTTON_IMAGE_CONTAINER_H_
#define UI_VIEWS_CONTROLS_BUTTON_LABEL_BUTTON_IMAGE_CONTAINER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "ui/views/views_export.h"

namespace views {

class View;
class ImageView;
class LabelButton;

class VIEWS_EXPORT LabelButtonImageContainer {
 public:
  LabelButtonImageContainer() = default;
  LabelButtonImageContainer(const LabelButtonImageContainer&) = delete;
  LabelButtonImageContainer& operator=(const LabelButtonImageContainer&) =
      delete;
  virtual ~LabelButtonImageContainer() = default;

  virtual std::unique_ptr<View> CreateView() = 0;

  // Gets a pointer to a previously created view. Returns nullptr if no view was
  // created.
  virtual View* GetView() = 0;
  virtual const View* GetView() const = 0;

  // Updates image based on `ButtonState` of the LabelButton.
  virtual void UpdateImage(const LabelButton* button) = 0;
};

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
  raw_ptr<ImageView> image_;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_BUTTON_LABEL_BUTTON_IMAGE_CONTAINER_H_
