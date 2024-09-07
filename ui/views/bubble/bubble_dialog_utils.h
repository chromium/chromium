// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_BUBBLE_BUBBLE_DIALOG_UTILS_H_
#define UI_VIEWS_BUBBLE_BUBBLE_DIALOG_UTILS_H_

#include "ui/base/models/dialog_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/views_export.h"

namespace views {

class BubbleDialogDelegate;
class Button;

// Configures views::BubbleDialogDelegate button based on `model`.
void VIEWS_EXPORT
ConfigureBubbleButtonForParams(views::BubbleDialogDelegate& bubble_delegate,
                               views::Button* button_view,
                               ui::mojom::DialogButton dialog_button,
                               const ui::DialogModel::Button& model);

}  // namespace views

#endif  // UI_VIEWS_BUBBLE_BUBBLE_DIALOG_UTILS_H_
