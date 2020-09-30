// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_BUBBLE_BUBBLE_DIALOG_MODEL_HOST_H_
#define UI_VIEWS_BUBBLE_BUBBLE_DIALOG_MODEL_HOST_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "ui/base/models/dialog_model.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/button.h"

namespace views {
class GridLayout;

// BubbleDialogModelHost is a views implementation of ui::DialogModelHost which
// hosts a ui::DialogModel as a BubbleDialogDelegateView. This exposes such as
// SetAnchorView(), SetArrow() and SetHighlightedButton(). For methods that are
// reflected in ui::DialogModelHost (such as ::Close()), prefer using the
// ui::DialogModelHost to avoid platform-specific code (GetWidget()->Close())
// where unnecessary. For those methods, note that this can be retrieved as a
// ui::DialogModelHost through DialogModel::host(). This helps minimize
// platform-specific code from platform-agnostic model-delegate code.
class VIEWS_EXPORT BubbleDialogModelHost : public BubbleDialogDelegateView,
                                           public ui::DialogModelHost {
 public:
  METADATA_HEADER(BubbleDialogModelHost);
  // Constructs a BubbleDialogModelHost, which for most purposes is to used as a
  // BubbleDialogDelegateView. The BubbleDialogDelegateView is nominally handed
  // to BubbleDialogDelegateView::CreateBubble() which returns a Widget that has
  // taken ownership of the bubble. Widget::Show() finally shows the bubble.
  BubbleDialogModelHost(std::unique_ptr<ui::DialogModel> model,
                        View* anchor_view,
                        BubbleBorder::Arrow arrow);
  ~BubbleDialogModelHost() override;

  static std::unique_ptr<BubbleDialogModelHost> CreateModal(
      std::unique_ptr<ui::DialogModel> model,
      ui::ModalType modal_type);

  // BubbleDialogDelegateView:
  // TODO(pbos): Populate initparams with initial view instead of overriding
  // GetInitiallyFocusedView().
  View* GetInitiallyFocusedView() override;
  void OnDialogInitialized() override;
  gfx::Size CalculatePreferredSize() const override;

  // ui::DialogModelHost:
  void Close() override;
  void SelectAllText(int unique_id) override;
  void OnFieldAdded(ui::DialogModelField* field) override;

 private:
  void OnWindowClosing();

  GridLayout* GetGridLayout();
  void ConfigureGridLayout();

  void AddInitialFields();
  View* AddOrUpdateBodyText(ui::DialogModelBodyText* field);
  View* AddOrUpdateCheckbox(ui::DialogModelCheckbox* field);
  View* AddOrUpdateCombobox(ui::DialogModelCombobox* field);
  View* AddOrUpdateTextfield(ui::DialogModelTextfield* field);
  void AddLabelAndField(const base::string16& label_text,
                        std::unique_ptr<views::View> field,
                        const gfx::FontList& field_font);

  std::unique_ptr<View> CreateViewForLabel(
      const ui::DialogModelLabel& dialog_label);

  void OnViewCreatedForField(View* view, ui::DialogModelField* field);

  View* FieldToView(ui::DialogModelField* field);

  std::unique_ptr<ui::DialogModel> model_;
  base::flat_map<ui::DialogModelField*, View*> field_to_view_;
  std::vector<PropertyChangedSubscription> property_changed_subscriptions_;
};

}  // namespace views

#endif  // UI_VIEWS_BUBBLE_BUBBLE_DIALOG_MODEL_HOST_H_
