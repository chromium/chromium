// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_LOGIN_BUBBLE_DIALOG_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_LOGIN_BUBBLE_DIALOG_EXAMPLE_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/examples/example_base.h"

namespace views {

class LabelButton;

namespace examples {

class LoginBubbleDialogView : public BubbleDialogDelegateView,
                              public TextfieldController {
 public:
  using OnSubmitCallback = base::OnceCallback<void(std::u16string username,
                                                   std::u16string password)>;

  static void Show(View* anchor_view,
                   BubbleBorder::Arrow anchor_position,
                   OnSubmitCallback accept_callback);

  ~LoginBubbleDialogView() override;

  // TextfieldController:
  void ContentsChanged(Textfield* sender,
                       const std::u16string& new_contents) override;

 private:
  LoginBubbleDialogView(View* anchor_view,
                        BubbleBorder::Arrow anchor_position,
                        OnSubmitCallback accept_callback);

  raw_ptr<Textfield> username_ = nullptr;
  raw_ptr<Textfield> password_ = nullptr;
};

// Instantiates the login dialog example.
class LoginBubbleDialogExample : public ExampleBase {
 public:
  LoginBubbleDialogExample();
  ~LoginBubbleDialogExample() override;

  // ExampleBase:
  void CreateExampleView(View* container) override;

  // LoginBubbleDialogController:
  void OnSubmit(std::u16string username, std::u16string password);

 private:
  raw_ptr<LabelButton> button_ = nullptr;
  raw_ptr<Label> username_label_ = nullptr;
  raw_ptr<Label> username_input_ = nullptr;
  raw_ptr<Label> password_label_ = nullptr;
  raw_ptr<Label> password_input_ = nullptr;
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_LOGIN_BUBBLE_DIALOG_EXAMPLE_H_
