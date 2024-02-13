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
  image_view_tracker_.SetView(view.get());
  return view;
}

View* SingleImageContainer::GetView() {
  return image_view_tracker_.view();
}

const View* SingleImageContainer::GetView() const {
  return image_view_tracker_.view();
}

void SingleImageContainer::UpdateImage(const LabelButton* button) {
  if (auto* view = image_view_tracker_.view(); view) {
    static_cast<ImageView*>(view)->SetImage(ui::ImageModel::FromImageSkia(
        button->GetImage(button->GetVisualState())));
  }
}

}  // namespace views
