// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/bubble/bubble_dialog_utils.h"

#include "ui/base/models/dialog_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view_class_properties.h"

namespace views {

void ConfigureBubbleButtonForParams(
    views::BubbleDialogDelegate& bubble_delegate,
    views::Button* button_view,
    ui::mojom::DialogButton dialog_button,
    const ui::DialogModel::Button& model) {
  bubble_delegate.SetButtonLabel(dialog_button, model.label());
  bubble_delegate.SetButtonStyle(dialog_button, model.style());
  bubble_delegate.SetButtonEnabled(dialog_button, model.is_enabled());

  if (button_view) {
    button_view->SetVisible(model.is_visible());
    button_view->SetProperty(kElementIdentifierKey, model.id());
  }
}

}  // namespace views
