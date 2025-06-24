// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/modal_dialog_wrapper.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/android/fake_modal_dialog_manager_bridge.h"
#include "ui/android/window_android.h"
#include "ui/base/models/dialog_model.h"

namespace ui {

class ModalDialogWrapperTest : public testing::Test {
 protected:
  void SetUp() override {
    window_ = ui::WindowAndroid::CreateForTesting();
    fake_dialog_manager_ = FakeModalDialogManagerBridge::CreateForTab(
        window_->get(), /*use_empty_java_presenter=*/false);
  }

  // Helper function to build the dialog model.
  std::unique_ptr<ui::DialogModel> CreateDialogModel(
      base::OnceClosure ok_callback = base::DoNothing(),
      ui::ButtonStyle ok_button_style = ui::ButtonStyle::kDefault,
      bool cancel_button = false,
      base::OnceClosure cancel_callback = base::DoNothing(),
      ui::ButtonStyle cancel_button_style = ui::ButtonStyle::kDefault,
      base::OnceClosure close_callback = base::DoNothing(),
      std::optional<mojom::DialogButton> override_button = std::nullopt,
      const std::vector<std::u16string>& paragraphs = {u"paragraph"}) {
    ui::DialogModel::Builder dialog_builder;
    dialog_builder.SetTitle(u"title");

    for (const auto& paragraph_text : paragraphs) {
      dialog_builder.AddParagraph(ui::DialogModelLabel(paragraph_text));
    }

    dialog_builder.AddOkButton(
        std::move(ok_callback),
        ui::DialogModel::Button::Params().SetLabel(u"ok").SetStyle(
            ok_button_style));
    if (cancel_button) {
      dialog_builder.AddCancelButton(
          std::move(cancel_callback),
          ui::DialogModel::Button::Params().SetLabel(u"cancel").SetStyle(
              cancel_button_style));
    }
    dialog_builder.SetCloseActionCallback(std::move(close_callback))
        .SetDialogDestroyingCallback(
            base::BindLambdaForTesting([&]() { dialog_destroyed_ = true; }));
    if (override_button.has_value()) {
      dialog_builder.OverrideDefaultButton(override_button.value());
    }
    return dialog_builder.Build();
  }

  bool dialog_destroyed_ = false;
  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting> window_;
  std::unique_ptr<FakeModalDialogManagerBridge> fake_dialog_manager_;
};

TEST_F(ModalDialogWrapperTest, CallOkButton) {
  bool ok_called = false;

  auto dialog_model = CreateDialogModel(
      /*ok_callback=*/base::BindLambdaForTesting([&]() { ok_called = true; }),
      /*ok_button_style=*/ui::ButtonStyle::kDefault,
      /*cancel_button=*/false);

  ModalDialogWrapper::ShowTabModal(std::move(dialog_model), window_->get());
  fake_dialog_manager_->ClickPositiveButton();

  EXPECT_TRUE(ok_called);
  EXPECT_TRUE(dialog_destroyed_);
}

TEST_F(ModalDialogWrapperTest, CallCancelButton) {
  bool ok_called = false, cancel_called = false;

  auto dialog_model = CreateDialogModel(
      /*ok_callback=*/base::BindLambdaForTesting([&]() { ok_called = true; }),
      /*ok_button_style=*/ui::ButtonStyle::kDefault,
      /*cancel_button=*/true,
      /*cancel_callback=*/base::BindLambdaForTesting([&]() {
        cancel_called = true;
      }),
      /*cancel_button_style=*/ui::ButtonStyle::kDefault);

  ModalDialogWrapper::ShowTabModal(std::move(dialog_model), window_->get());
  fake_dialog_manager_->ClickNegativeButton();

  EXPECT_FALSE(ok_called);
  EXPECT_TRUE(cancel_called);
  EXPECT_TRUE(dialog_destroyed_);
}

TEST_F(ModalDialogWrapperTest, CloseDialogFromNative) {
  bool ok_called = false, cancel_called = false, closed = false;

  auto dialog_model = CreateDialogModel(
      /*ok_callback=*/base::BindLambdaForTesting([&]() { ok_called = true; }),
      /*ok_button_style=*/ui::ButtonStyle::kDefault,
      /*cancel_button=*/true,
      /*cancel_callback=*/base::BindLambdaForTesting([&]() {
        cancel_called = true;
      }),
      /*cancel_button_style=*/ui::ButtonStyle::kDefault,
      /*close_callback=*/base::BindLambdaForTesting([&]() { closed = true; }));

  ModalDialogWrapper::ShowTabModal(std::move(dialog_model), window_->get());
  ModalDialogWrapper::GetDialogForTesting()->Close();

  EXPECT_FALSE(ok_called);
  EXPECT_FALSE(cancel_called);
  EXPECT_TRUE(closed);
  EXPECT_TRUE(dialog_destroyed_);
}

TEST_F(ModalDialogWrapperTest, ModalButtonsNoProminent) {
  auto dialog_model = CreateDialogModel();

  ModalDialogWrapper::ShowTabModal(std::move(dialog_model), window_->get());

  EXPECT_EQ(static_cast<ui::ModalDialogWrapper::ModalDialogButtonStyles>(
                fake_dialog_manager_->GetButtonStyles()),
            ui::ModalDialogWrapper::ModalDialogButtonStyles::
                kPrimaryOutlineNegativeOutline);
  EXPECT_FALSE(dialog_destroyed_);
}

TEST_F(ModalDialogWrapperTest, ModalButtonsPrimaryProminentNoNegative) {
  auto dialog_model = CreateDialogModel(
      /*ok_callback=*/base::DoNothing(),
      /*ok_button_style=*/ui::ButtonStyle::kProminent);

  ModalDialogWrapper::ShowTabModal(std::move(dialog_model), window_->get());

  EXPECT_EQ(static_cast<ui::ModalDialogWrapper::ModalDialogButtonStyles>(
                fake_dialog_manager_->GetButtonStyles()),
            ui::ModalDialogWrapper::ModalDialogButtonStyles::
                kPrimaryFilledNoNegative);
  EXPECT_FALSE(dialog_destroyed_);
}

TEST_F(ModalDialogWrapperTest, ModalButtonsPrimaryProminent) {
  auto dialog_model = CreateDialogModel(
      /*ok_callback=*/base::DoNothing(),
      /*ok_button_style=*/ui::ButtonStyle::kProminent,
      /*cancel_button=*/true,
      /*cancel_callback=*/base::DoNothing(),
      /*cancel_button_style=*/ui::ButtonStyle::kDefault);

  ModalDialogWrapper::ShowTabModal(std::move(dialog_model), window_->get());

  EXPECT_EQ(static_cast<ui::ModalDialogWrapper::ModalDialogButtonStyles>(
                fake_dialog_manager_->GetButtonStyles()),
            ui::ModalDialogWrapper::ModalDialogButtonStyles::
                kPrimaryFilledNegativeOutline);
  EXPECT_FALSE(dialog_destroyed_);
}

TEST_F(ModalDialogWrapperTest, ModalButtonsNegativeProminent) {
  auto dialog_model = CreateDialogModel(
      /*ok_callback=*/base::DoNothing(),
      /*ok_button_style=*/ui::ButtonStyle::kDefault,
      /*cancel_button=*/true,
      /*cancel_callback=*/base::DoNothing(),
      /*cancel_button_style=*/ui::ButtonStyle::kProminent);

  ModalDialogWrapper::ShowTabModal(std::move(dialog_model), window_->get());

  EXPECT_EQ(static_cast<ui::ModalDialogWrapper::ModalDialogButtonStyles>(
                fake_dialog_manager_->GetButtonStyles()),
            ui::ModalDialogWrapper::ModalDialogButtonStyles::
                kPrimaryOutlineNegativeFilled);
  EXPECT_FALSE(dialog_destroyed_);
}

TEST_F(ModalDialogWrapperTest, ModalButtonsOverriddenNone) {
  auto dialog_model = CreateDialogModel(
      /*ok_callback=*/base::DoNothing(),
      /*ok_button_style=*/ui::ButtonStyle::kProminent,
      /*cancel_button=*/true,
      /*cancel_callback=*/base::DoNothing(),
      /*cancel_button_style=*/ui::ButtonStyle::kProminent,
      /*close_callback=*/base::DoNothing(),
      /*override_button=*/ui::mojom::DialogButton::kNone);

  ModalDialogWrapper::ShowTabModal(std::move(dialog_model), window_->get());

  EXPECT_EQ(static_cast<ui::ModalDialogWrapper::ModalDialogButtonStyles>(
                fake_dialog_manager_->GetButtonStyles()),
            ui::ModalDialogWrapper::ModalDialogButtonStyles::
                kPrimaryOutlineNegativeOutline);
  EXPECT_FALSE(dialog_destroyed_);
}

TEST_F(ModalDialogWrapperTest, ModalButtonsOverriddenPositive) {
  auto dialog_model = CreateDialogModel(
      /*ok_callback=*/base::DoNothing(),
      /*ok_button_style=*/ui::ButtonStyle::kDefault,
      /*cancel_button=*/true,
      /*cancel_callback=*/base::DoNothing(),
      /*cancel_button_style=*/ui::ButtonStyle::kProminent,
      /*close_callback=*/base::DoNothing(),
      /*override_button=*/ui::mojom::DialogButton::kOk);

  ModalDialogWrapper::ShowTabModal(std::move(dialog_model), window_->get());

  EXPECT_EQ(static_cast<ui::ModalDialogWrapper::ModalDialogButtonStyles>(
                fake_dialog_manager_->GetButtonStyles()),
            ui::ModalDialogWrapper::ModalDialogButtonStyles::
                kPrimaryFilledNegativeOutline);
  EXPECT_FALSE(dialog_destroyed_);
}

TEST_F(ModalDialogWrapperTest, ModalButtonsOverriddenNegative) {
  auto dialog_model = CreateDialogModel(
      /*ok_callback=*/base::DoNothing(),
      /*ok_button_style=*/ui::ButtonStyle::kProminent,
      /*cancel_button=*/true,
      /*cancel_callback=*/base::DoNothing(),
      /*cancel_button_style=*/ui::ButtonStyle::kDefault,
      /*close_callback=*/base::DoNothing(),
      /*override_button=*/ui::mojom::DialogButton::kCancel);

  ModalDialogWrapper::ShowTabModal(std::move(dialog_model), window_->get());

  EXPECT_EQ(static_cast<ui::ModalDialogWrapper::ModalDialogButtonStyles>(
                fake_dialog_manager_->GetButtonStyles()),
            ui::ModalDialogWrapper::ModalDialogButtonStyles::
                kPrimaryOutlineNegativeFilled);
  EXPECT_FALSE(dialog_destroyed_);
}

TEST_F(ModalDialogWrapperTest, ParagraphsAreSetAndReplaced) {
  std::vector<std::u16string> paragraphs = {u"This is the first paragraph.",
                                            u"This is the second paragraph."};

  auto dialog_model_1 = CreateDialogModel(
      /*ok_callback=*/base::DoNothing(),
      /*ok_button_style=*/ui::ButtonStyle::kDefault,
      /*cancel_button=*/false,
      /*cancel_callback=*/base::DoNothing(),
      /*cancel_button_style=*/ui::ButtonStyle::kDefault,
      /*close_callback=*/base::DoNothing(),
      /*override_button=*/std::nullopt,
      /*paragraphs=*/paragraphs);

  ModalDialogWrapper::ShowTabModal(std::move(dialog_model_1), window_->get());

  std::vector<std::u16string> displayed_paragraphs_1 =
      fake_dialog_manager_->GetMessageParagraphs();
  ASSERT_EQ(displayed_paragraphs_1.size(), 2u);
  EXPECT_EQ(displayed_paragraphs_1.front(), paragraphs.front());
  EXPECT_EQ(displayed_paragraphs_1.back(), paragraphs.back());

  // Remove the last element and confirm the behavior.
  paragraphs.pop_back();

  auto dialog_model_2 = CreateDialogModel(
      /*ok_callback=*/base::DoNothing(),
      /*ok_button_style=*/ui::ButtonStyle::kDefault,
      /*cancel_button=*/false,
      /*cancel_callback=*/base::DoNothing(),
      /*cancel_button_style=*/ui::ButtonStyle::kDefault,
      /*close_callback=*/base::DoNothing(),
      /*override_button=*/std::nullopt,
      /*paragraphs=*/paragraphs);

  ModalDialogWrapper::ShowTabModal(std::move(dialog_model_2), window_->get());

  std::vector<std::u16string> displayed_paragraphs_2 =
      fake_dialog_manager_->GetMessageParagraphs();
  ASSERT_EQ(displayed_paragraphs_2.size(), 1u);
  EXPECT_EQ(displayed_paragraphs_2.front(), paragraphs.front());
}

}  // namespace ui
