// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/models/dialog_model.h"

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/test/test_dialog_model_host.h"
#include "ui/events/event.h"

namespace ui {

class DialogModelButtonTest : public testing::Test {};

TEST_F(DialogModelButtonTest, UsesParamsUniqueId) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kUniqueId);
  auto host = std::make_unique<TestDialogModelHost>(
      DialogModel::Builder()
          .AddOkButton(base::DoNothing(),
                       DialogModel::Button::Params().SetId(kUniqueId))
          .Build());
  EXPECT_EQ(kUniqueId, host->GetId(TestDialogModelHost::ButtonId::kOk));
}

TEST_F(DialogModelButtonTest, UsesParamsAccelerators) {
  const Accelerator accelerator_1;
  const Accelerator accelerator_2(VKEY_Z, EF_SHIFT_DOWN | EF_CONTROL_DOWN);

  auto host = std::make_unique<TestDialogModelHost>(
      DialogModel::Builder()
          .AddOkButton(base::DoNothing(), DialogModel::Button::Params()
                                              .AddAccelerator(accelerator_1)
                                              .AddAccelerator(accelerator_2))
          .Build());
  EXPECT_THAT(host->GetAccelerators(TestDialogModelHost::ButtonId::kOk),
              testing::UnorderedElementsAre(accelerator_1, accelerator_2));
}

TEST_F(DialogModelButtonTest, UsesCallback) {
  int callback_count = 0;
  std::unique_ptr<KeyEvent> last_event;
  auto host = std::make_unique<TestDialogModelHost>(
      DialogModel::Builder()
          .AddExtraButton(base::BindLambdaForTesting([&](const Event& event) {
                            ++callback_count;
                            last_event =
                                std::make_unique<KeyEvent>(*event.AsKeyEvent());
                          }),
                          DialogModel::Button::Params().SetLabel(u"button"))
          .Build());

  KeyEvent first_event(EventType::kKeyPressed, VKEY_RETURN, EF_NONE);
  host->TriggerExtraButton(first_event);
  EXPECT_EQ(1, callback_count);
  EXPECT_EQ(first_event.key_code(), last_event->key_code());

  KeyEvent second_event(EventType::kKeyPressed, VKEY_SPACE, EF_NONE);
  host->TriggerExtraButton(second_event);
  EXPECT_EQ(2, callback_count);
  EXPECT_EQ(second_event.key_code(), last_event->key_code());
}

class DialogModelDialogButtonTest : public testing::Test {
 public:
  void DialogButtonUsesArguments(TestDialogModelHost::ButtonId button_id) {
    DialogModel::Builder builder;

    // Callback to verify that the first parameter is used.
    bool callback_called = false;
    base::OnceClosure callback = base::BindRepeating(
        [](bool* callback_called) { *callback_called = true; },
        &callback_called);

    // The presence of an accelerator in |params| will be used to verify that
    // |params| are forwarded correctly to the DialogModel::Button constructor.
    DialogModel::Button::Params params;
    const Accelerator accelerator(VKEY_Z, EF_SHIFT_DOWN | EF_CONTROL_DOWN);
    const std::u16string label = u"my cool button";
    params.AddAccelerator(accelerator);
    params.SetLabel(label);

    switch (button_id) {
      case TestDialogModelHost::ButtonId::kCancel:
        builder.AddCancelButton(std::move(callback), params);
        break;
      case TestDialogModelHost::ButtonId::kExtra:
        // Wrap the callback into a repeating callback that'll only be called
        // once so the same verification can be used for the extra button.
        builder.AddExtraButton(
            base::BindRepeating(
                [](base::OnceClosure* callback, const Event& event) {
                  std::move(*callback).Run();
                },
                &callback),
            params);
        break;
      case TestDialogModelHost::ButtonId::kOk:
        builder.AddOkButton(std::move(callback), params);
        break;
    }
    auto host = std::make_unique<TestDialogModelHost>(builder.Build());

    EXPECT_EQ(label, host->GetLabel(button_id))
        << "The label parameter wasn't used.";

    EXPECT_THAT(host->GetAccelerators(button_id),
                testing::UnorderedElementsAre(accelerator))
        << "The params parameter wasn't used.";

    // Trigger the corresponding action.
    switch (button_id) {
      case TestDialogModelHost::ButtonId::kCancel:
        TestDialogModelHost::Cancel(std::move(host));
        break;
      case TestDialogModelHost::ButtonId::kExtra:
        host->TriggerExtraButton(
            KeyEvent(EventType::kKeyPressed, VKEY_RETURN, EF_NONE));
        break;
      case TestDialogModelHost::ButtonId::kOk:
        TestDialogModelHost::Accept(std::move(host));
        break;
    }

    EXPECT_TRUE(callback_called) << "The callback parameter wasn't used.";
  }
};

TEST_F(DialogModelDialogButtonTest, OkButtonUsesArguments) {
  DialogButtonUsesArguments(TestDialogModelHost::ButtonId::kOk);
}

TEST_F(DialogModelDialogButtonTest, ExtraButtonUsesArguments) {
  DialogButtonUsesArguments(TestDialogModelHost::ButtonId::kExtra);
}

TEST_F(DialogModelDialogButtonTest, CancelButtonUsesArguments) {
  DialogButtonUsesArguments(TestDialogModelHost::ButtonId::kCancel);
}

}  // namespace ui
