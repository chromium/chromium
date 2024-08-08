// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_BUBBLE_BUBBLE_DIALOG_MODEL_HOST_H_
#define UI_VIEWS_BUBBLE_BUBBLE_DIALOG_MODEL_HOST_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/types/pass_key.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/view.h"

namespace views {

class VIEWS_EXPORT DialogModelSectionHost : public BoxLayoutView,
                                            public ui::DialogModelFieldHost {
  METADATA_HEADER(DialogModelSectionHost, BoxLayoutView)

 public:
  [[nodiscard]] static std::unique_ptr<DialogModelSectionHost> Create(
      ui::DialogModelSection* section,
      ui::ElementIdentifier initially_focused_field_id =
          ui::ElementIdentifier());

 protected:
  // Prevent accidentally constructing this and not using ::Create().
  using BoxLayoutView::BoxLayoutView;
};

// TODO(pbos): Find a better name and move to a file separate from
// BubbleDialogModelHost. See if we can have BubbleDialogModelHost use
// DialogModelSectionHost directly (by removing more calls into
// BubbleDialogModelHostContentsView).
class BubbleDialogModelHostContentsView;

// BubbleDialogModelHost is a views implementation of ui::DialogModelHost which
// hosts a ui::DialogModel as a BubbleDialogDelegate. This exposes such as
// SetAnchorView(), SetArrow() and SetHighlightedButton(). For methods that are
// reflected in ui::DialogModelHost (such as ::Close()), prefer using the
// ui::DialogModelHost to avoid platform-specific code (GetWidget()->Close())
// where unnecessary. For those methods, note that this can be retrieved as a
// ui::DialogModelHost through DialogModel::host(). This helps minimize
// platform-specific code from platform-agnostic model-delegate code.
class VIEWS_EXPORT BubbleDialogModelHost : public BubbleDialogDelegate,
                                           public ui::DialogModelHost,
                                           public ui::DialogModelFieldHost {
 public:
  enum class FieldType { kText, kControl, kMenuItem };

  class VIEWS_EXPORT CustomView : public ui::DialogModelCustomField::Field {
   public:
    CustomView(std::unique_ptr<View> view,
               FieldType field_type,
               View* focusable_view = nullptr);
    CustomView(const CustomView&) = delete;
    CustomView& operator=(const CustomView&) = delete;
    ~CustomView() override;

    std::unique_ptr<View> TransferView();

    FieldType field_type() const { return field_type_; }

    View* TransferFocusableView() {
      return std::exchange(focusable_view_, nullptr);
    }

   private:
    // `view` is intended to be moved into the View hierarchy.
    std::unique_ptr<View> view_;
    const FieldType field_type_;
    raw_ptr<View> focusable_view_;
  };

  // Constructs a BubbleDialogModelHost, which for most purposes is to used as a
  // BubbleDialogDelegate. The BubbleDialogDelegate is nominally handed to
  // BubbleDialogDelegate::CreateBubble() which returns a Widget that has taken
  // ownership of the bubble. Widget::Show() finally shows the bubble.
  BubbleDialogModelHost(std::unique_ptr<ui::DialogModel> model,
                        View* anchor_view,
                        BubbleBorder::Arrow arrow,
                        bool autosize = true);

  // "Private" constructor (uses base::PassKey), use another constructor or
  // ::CreateModal().
  BubbleDialogModelHost(base::PassKey<BubbleDialogModelHost>,
                        std::unique_ptr<ui::DialogModel> model,
                        View* anchor_view,
                        BubbleBorder::Arrow arrow,
                        ui::mojom::ModalType modal_type,
                        bool autosize);

  ~BubbleDialogModelHost() override;

  static std::unique_ptr<BubbleDialogModelHost> CreateModal(
      std::unique_ptr<ui::DialogModel> model,
      ui::mojom::ModalType modal_type,
      bool autosize = true);

  // BubbleDialogDelegate:
  // TODO(pbos): Populate initparams with initial view instead of overriding
  // GetInitiallyFocusedView().
  View* GetInitiallyFocusedView() override;
  void OnWidgetInitialized() override;

  // ui::DialogModelHost:
  void Close() override;
  void OnDialogButtonChanged() override;

 private:
  // This class observes the ContentsView theme to make sure that the window
  // icon updates with the theme.
  class ThemeChangedObserver : public ViewObserver {
   public:
    ThemeChangedObserver(BubbleDialogModelHost* parent,
                         BubbleDialogModelHostContentsView* contents_view);
    ThemeChangedObserver(const ThemeChangedObserver&) = delete;
    ThemeChangedObserver& operator=(const ThemeChangedObserver&) = delete;
    ~ThemeChangedObserver() override;

    // ViewObserver:
    void OnViewThemeChanged(View*) override;

   private:
    const raw_ptr<BubbleDialogModelHost> parent_;
    base::ScopedObservation<View, ViewObserver> observation_{this};
  };

  [[nodiscard]] BubbleDialogModelHostContentsView* InitContentsView(
      ui::DialogModelSection* contents);

  void OnContentsViewChanged();

  void OnWindowClosing();

  void UpdateDialogButtons();

  void UpdateWindowIcon();
  void UpdateSpacingAndMargins();

  bool IsModalDialog() const;

  std::unique_ptr<ui::DialogModel> model_;
  const raw_ptr<BubbleDialogModelHostContentsView> contents_view_;
  base::CallbackListSubscription on_contents_changed_subscription_;
  ThemeChangedObserver theme_observer_;
};

}  // namespace views

#endif  // UI_VIEWS_BUBBLE_BUBBLE_DIALOG_MODEL_HOST_H_
