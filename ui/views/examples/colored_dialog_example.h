// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_COLORED_DIALOG_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_COLORED_DIALOG_EXAMPLE_H_

#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/examples/example_base.h"
#include "ui/views/view.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {

class Label;

namespace examples {

class ColoredDialog : public views::DialogDelegateView,
                      public views::TextfieldController {
  METADATA_HEADER(ColoredDialog, views::DialogDelegateView)

 public:
  using AcceptCallback = base::OnceCallback<void(std::u16string)>;

  explicit ColoredDialog(AcceptCallback accept_callback);
  ColoredDialog(const ColoredDialog&) = delete;
  ColoredDialog& operator=(const ColoredDialog&) = delete;
  ~ColoredDialog() override;

 protected:
  // views::DialogDelegateView
  bool ShouldShowCloseButton() const override;

  // views::TextfieldController
  void ContentsChanged(Textfield* sender,
                       const std::u16string& new_contents) override;

 private:
  raw_ptr<views::Textfield> textfield_;
};

class ColoredDialogChooser : public views::View {
  METADATA_HEADER(ColoredDialogChooser, views::View)

 public:
  ColoredDialogChooser();
  ColoredDialogChooser(const ColoredDialogChooser&) = delete;
  ColoredDialogChooser& operator=(const ColoredDialogChooser&) = delete;
  ~ColoredDialogChooser() override;

  void ButtonPressed();

 private:
  void OnFeedbackSubmit(std::u16string text);

  raw_ptr<views::Label> confirmation_label_;
  base::OneShotTimer confirmation_timer_;
};

// An example that exercises BubbleDialogDelegateView or DialogDelegateView.
class VIEWS_EXAMPLES_EXPORT ColoredDialogExample : public ExampleBase {
 public:
  ColoredDialogExample();
  ColoredDialogExample(const ColoredDialogExample&) = delete;
  ColoredDialogExample& operator=(const ColoredDialogExample&) = delete;
  ~ColoredDialogExample() override;

  // ExampleBase
  void CreateExampleView(views::View* container) override;
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_COLORED_DIALOG_EXAMPLE_H_
