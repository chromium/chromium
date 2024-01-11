// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/label_button_image_container.h"

#include "ui/base/models/image_model.h"
#include "ui/views/controls/button/label_button.h"

namespace views {

std::unique_ptr<View> SingleImageContainer::CreateView() {
  std::unique_ptr<ImageView> view = std::make_unique<ImageView>();
  view->SetCanProcessEventsWithinSubtree(false);
  image_ = view.get();
  return view;
}

View* SingleImageContainer::GetView() {
  return image_;
}

const View* SingleImageContainer::GetView() const {
  return image_;
}

void SingleImageContainer::UpdateImage(const LabelButton* button) {
  image_->SetImage(ui::ImageModel::FromImageSkia(
      button->GetImage(button->GetVisualState())));
}

}  // namespace views
