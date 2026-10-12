// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_VIEWS_CANVAS_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_VIEWS_CANVAS_EXAMPLE_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/selected_file_info.h"
#include "ui/views/examples/example_base.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class DialogDelegate;
class Label;
class Textarea;
}  // namespace views

namespace views::examples {

class VIEWS_EXAMPLES_EXPORT ViewsCanvasExample
    : public ExampleBase,
      public ui::SelectFileDialog::Listener,
      public views::WidgetObserver {
 public:
  ViewsCanvasExample();
  ViewsCanvasExample(const ViewsCanvasExample&) = delete;
  ViewsCanvasExample& operator=(const ViewsCanvasExample&) = delete;
  ~ViewsCanvasExample() override;

  // ExampleBase:
  void CreateExampleView(View* container) override;

  // ui::SelectFileDialog::Listener:
  void FileSelected(const ui::SelectedFileInfo& file, int index) override;
  void FileSelectionCanceled() override;

  // views::WidgetObserver:
  void OnWidgetDestroyed(Widget* widget) override;

 private:
  void OnRenderButtonPressed();
  void OnOpenButtonPressed();
  void OnShowPopupPressed(View* anchor_view);
  void CleanupPopup();
  void RebuildPreview(const std::string& json_text);
  void OnAddedToWidget();

  raw_ptr<Textarea> json_editor_ = nullptr;
  raw_ptr<Label> status_label_ = nullptr;
  raw_ptr<View> preview_container_ = nullptr;

  // Popup management for Dialogs, Bubbles, and DialogModels.
  // The caller owns both popup_widget_ and popup_delegate_.
  // In C++, member destruction is reverse-declaration order, so popup_widget_
  // is destroyed first, followed by popup_delegate_.
  std::unique_ptr<views::DialogDelegate> popup_delegate_;
  std::unique_ptr<views::Widget> popup_widget_;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};

  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;

  base::WeakPtrFactory<ViewsCanvasExample> weak_factory_{this};
};

}  // namespace views::examples

#endif  // UI_VIEWS_EXAMPLES_VIEWS_CANVAS_EXAMPLE_H_
