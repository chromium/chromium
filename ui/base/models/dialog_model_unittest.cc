// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/models/dialog_model.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/test/test_dialog_model_host.h"
#include "ui/events/event.h"

namespace ui {

class DialogModelButtonTest : public testing::Test {};

TEST_F(DialogModelButtonTest, UsesParamsUniqueId) {
  constexpr int kUniqueId = 42;
  // TODO(pbos): Replace AddOkButton() with AddButton() once buttons in dialogs
  // are supported.
  std::unique_ptr<DialogModel> model =
      DialogModel::Builder()
          .AddOkButton(base::OnceClosure(), base::string16(),
                       DialogModelButton::Params().SetUniqueId(kUniqueId))
          .Build();
  EXPECT_EQ(kUniqueId,
            model->ok_button(TestDialogModelHost::GetPassKey())->unique_id_);
}

TEST_F(DialogModelButtonTest, UsesParamsAccelerators) {
  const Accelerator accelerator_1;
  const Accelerator accelerator_2(VKEY_Z, EF_SHIFT_DOWN | EF_CONTROL_DOWN);

  // TODO(pbos): Replace AddOkButton() with AddButton() once buttons in dialogs
  // are supported.
  std::unique_ptr<DialogModel> model =
      DialogModel::Builder()
          .AddOkButton(base::OnceClosure(), base::string16(),
                       DialogModelButton::Params()
                           .AddAccelerator(accelerator_1)
                           .AddAccelerator(accelerator_2))
          .Build();
  EXPECT_THAT(model->ok_button(TestDialogModelHost::GetPassKey())
                  ->accelerators(TestDialogModelHost::GetPassKey()),
              testing::UnorderedElementsAre(accelerator_1, accelerator_2));
}

TEST_F(DialogModelButtonTest, UsesCallback) {
  int callback_count = 0;
  std::unique_ptr<KeyEvent> last_event;
  // TODO(pbos): Replace AddExtraButton() with AddButton() once buttons in
  // dialogs are supported.
  std::unique_ptr<DialogModel> model =
      DialogModel::Builder()
          .AddDialogExtraButton(
              base::BindLambdaForTesting([&](const Event& event) {
                ++callback_count;
                last_event = std::make_unique<KeyEvent>(*event.AsKeyEvent());
              }),
              base::string16())
          .Build();
  DialogModelButton* const button =
      model->extra_button(TestDialogModelHost::GetPassKey());

  KeyEvent first_event(ET_KEY_PRESSED, VKEY_RETURN, EF_NONE);
  button->OnPressed(TestDialogModelHost::GetPassKey(), first_event);
  EXPECT_EQ(1, callback_count);
  EXPECT_EQ(first_event.key_code(), last_event->key_code());

  KeyEvent second_event(ET_KEY_PRESSED, VKEY_SPACE, EF_NONE);
  button->OnPressed(TestDialogModelHost::GetPassKey(), second_event);
  EXPECT_EQ(2, callback_count);
  EXPECT_EQ(second_event.key_code(), last_event->key_code());
}

class DialogModelDialogButtonTest : public testing::Test {
 public:
  enum DialogButtonId {
    kCancelButton,
    kExtraButton,
    kOkButton,
  };

  void DialogButtonUsesArguments(DialogButtonId button_id) {
    DialogModel::Builder builder;

    // Callback to verify that the first parameter is used.
    bool callback_called = false;
    base::OnceClosure callback = base::BindRepeating(
        [](bool* callback_called) { *callback_called = true; },
        &callback_called);

    // Label to verify the second parameter.
    base::string16 label = base::ASCIIToUTF16("my cool button");

    // The presence of an accelerator in |params| will be used to verify that
    // |params| are forwarded correctly to the DialogModelButton constructor.
    DialogModelButton::Params params;
    Accelerator accelerator(VKEY_Z, EF_SHIFT_DOWN | EF_CONTROL_DOWN);
    params.AddAccelerator(accelerator);

    switch (button_id) {
      case kCancelButton:
        builder.AddCancelButton(std::move(callback), label, params);
        break;
      case kExtraButton:
        // Wrap the callback into a repeating callback that'll only be called
        // once so the same verification can be used for the extra button.
        builder.AddDialogExtraButton(
            base::BindRepeating(
                [](base::OnceClosure* callback, const Event& event) {
                  std::move(*callback).Run();
                },
                &callback),
            label, params);
        break;
      case kOkButton:
        builder.AddOkButton(std::move(callback), label, params);
        break;
    }
    std::unique_ptr<DialogModel> model = builder.Build();

    // Get the DialogModelButton and trigger the corresponding callback.
    DialogModelButton* button = nullptr;
    switch (button_id) {
      case kCancelButton:
        button = model->cancel_button(TestDialogModelHost::GetPassKey());
        model->OnDialogCancelled(TestDialogModelHost::GetPassKey());
        break;
      case kExtraButton:
        button = model->extra_button(TestDialogModelHost::GetPassKey());
        button->OnPressed(TestDialogModelHost::GetPassKey(),
                          KeyEvent(ET_KEY_PRESSED, VKEY_RETURN, EF_NONE));
        break;
      case kOkButton:
        button = model->ok_button(TestDialogModelHost::GetPassKey());
        model->OnDialogAccepted(TestDialogModelHost::GetPassKey());
        break;
    }
    ASSERT_TRUE(button);

    EXPECT_TRUE(callback_called) << "The callback parameter wasn't used.";
    EXPECT_EQ(label, button->label(TestDialogModelHost::GetPassKey()))
        << "The label parameter wasn't used.";
    EXPECT_THAT(button->accelerators(TestDialogModelHost::GetPassKey()),
                testing::UnorderedElementsAre(accelerator))
        << "The params parameter wasn't used.";
  }
};

TEST_F(DialogModelDialogButtonTest, OkButtonUsesArguments) {
  DialogButtonUsesArguments(kOkButton);
}

TEST_F(DialogModelDialogButtonTest, ExtraButtonUsesArguments) {
  DialogButtonUsesArguments(kExtraButton);
}

TEST_F(DialogModelDialogButtonTest, CancelButtonUsesArguments) {
  DialogButtonUsesArguments(kCancelButton);
}

}  // namespace ui