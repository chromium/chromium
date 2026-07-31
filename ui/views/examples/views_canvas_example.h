// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_VIEWS_CANVAS_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_VIEWS_CANVAS_EXAMPLE_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/selected_file_info.h"
#include "ui/views/examples/example_base.h"

namespace views {
class Label;
class Textarea;
}  // namespace views

namespace views::examples {

class VIEWS_EXAMPLES_EXPORT ViewsCanvasExample
    : public ExampleBase,
      public ui::SelectFileDialog::Listener {
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

 private:
  void OnRenderButtonPressed();
  void OnOpenButtonPressed();
  void RebuildPreview(const std::string& json_text);
  void OnAddedToWidget();

  raw_ptr<Textarea> json_editor_ = nullptr;
  raw_ptr<Label> status_label_ = nullptr;
  raw_ptr<View> preview_container_ = nullptr;

  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;
};

}  // namespace views::examples

#endif  // UI_VIEWS_EXAMPLES_VIEWS_CANVAS_EXAMPLE_H_
