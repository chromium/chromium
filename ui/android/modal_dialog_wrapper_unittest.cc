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
#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/android/fake_modal_dialog_manager_bridge.h"
#include "ui/android/window_android.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/models/dialog_model.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_provider_manager.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/vector_icon_types.h"

namespace ui {

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kCheckboxId);
}  // namespace

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
      const std::vector<std::u16string>& paragraphs = {u"paragraph"},
      bool with_checkbox = false,
      bool checkbox_is_checked = false,
      std::optional<ui::ImageModel> icon = std::nullopt) {
    ui::DialogModel::Builder dialog_builder;
    dialog_builder.SetTitle(u"title");

    if (icon) {
      dialog_builder.SetIcon(*icon);
    }

    for (const auto& paragraph_text : paragraphs) {
      dialog_builder.AddParagraph(ui::DialogModelLabel(paragraph_text));
    }

    if (with_checkbox) {
      dialog_builder.AddCheckbox(
          kCheckboxId, ui::DialogModelLabel(u"checkbox label"),
          ui::DialogModelCheckbox::Params().SetIsChecked(checkbox_is_checked));
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

TEST_F(ModalDialogWrapperTest, Checkbox_InitialStateUnchecked) {
  auto dialog_model = CreateDialogModel(
      /*ok_callback=*/base::DoNothing(),
      /*ok_button_style=*/ui::ButtonStyle::kDefault,
      /*cancel_button=*/false,
      /*cancel_callback=*/base::DoNothing(),
      /*cancel_button_style=*/ui::ButtonStyle::kDefault,
      /*close_callback=*/base::DoNothing(),
      /*override_button=*/std::nullopt,
      /*paragraphs=*/{u"paragraph"},
      /*with_checkbox=*/true);
  ModalDialogWrapper::ShowTabModal(std::move(dialog_model), window_->get());

  EXPECT_FALSE(fake_dialog_manager_->IsCheckboxChecked());
}

TEST_F(ModalDialogWrapperTest, Checkbox_InitialStateChecked) {
  auto dialog_model = CreateDialogModel(
      /*ok_callback=*/base::DoNothing(),
      /*ok_button_style=*/ui::ButtonStyle::kDefault,
      /*cancel_button=*/false,
      /*cancel_callback=*/base::DoNothing(),
      /*cancel_button_style=*/ui::ButtonStyle::kDefault,
      /*close_callback=*/base::DoNothing(),
      /*override_button=*/std::nullopt,
      /*paragraphs=*/{u"paragraph"},
      /*with_checkbox=*/true,
      /*checkbox_is_checked=*/true);
  ModalDialogWrapper::ShowTabModal(std::move(dialog_model), window_->get());

  EXPECT_TRUE(fake_dialog_manager_->IsCheckboxChecked());
}

TEST_F(ModalDialogWrapperTest, Checkbox_StateSynchronizedAfterToggle) {
  auto dialog_model = CreateDialogModel(
      /*ok_callback=*/base::DoNothing(),
      /*ok_button_style=*/ui::ButtonStyle::kDefault,
      /*cancel_button=*/false,
      /*cancel_callback=*/base::DoNothing(),
      /*cancel_button_style=*/ui::ButtonStyle::kDefault,
      /*close_callback=*/base::DoNothing(),
      /*override_button=*/std::nullopt,
      /*paragraphs=*/{u"paragraph"},
      /*with_checkbox=*/true);
  // Get a raw pointer to the model before it's moved.
  auto* dialog_model_ptr = dialog_model.get();

  ModalDialogWrapper::ShowTabModal(std::move(dialog_model), window_->get());

  // Verify initial state.
  EXPECT_FALSE(fake_dialog_manager_->IsCheckboxChecked());
  EXPECT_FALSE(
      dialog_model_ptr->GetCheckboxByUniqueId(kCheckboxId)->is_checked());

  // Simulate a UI click on the checkbox.
  fake_dialog_manager_->ToggleCheckbox();

  // Verify both the Java model (via fake manager) and C++ model are updated.
  EXPECT_TRUE(fake_dialog_manager_->IsCheckboxChecked());
  EXPECT_TRUE(
      dialog_model_ptr->GetCheckboxByUniqueId(kCheckboxId)->is_checked());

  // Toggle it back.
  fake_dialog_manager_->ToggleCheckbox();

  EXPECT_FALSE(fake_dialog_manager_->IsCheckboxChecked());
  EXPECT_FALSE(
      dialog_model_ptr->GetCheckboxByUniqueId(kCheckboxId)->is_checked());
}

TEST_F(ModalDialogWrapperTest, ShowsVectorIcon) {
  constexpr int kIconDim = 24;
  const gfx::PathElement kTestIconPath[] = {
      // A 16x16 canvas.
      gfx::CANVAS_DIMENSIONS,
      16,
      // A square path.
      gfx::MOVE_TO,
      0,
      0,
      gfx::LINE_TO,
      16,
      0,
      gfx::LINE_TO,
      16,
      16,
      gfx::LINE_TO,
      0,
      16,
      gfx::CLOSE,
  };

  const gfx::VectorIconRep kTestIconReps[] = {
      {.path = kTestIconPath},
  };

  const gfx::VectorIcon kTestIcon(kTestIconReps, std::size(kTestIconReps),
                                  "test_icon");
  auto icon =
      ui::ImageModel::FromVectorIcon(kTestIcon, SK_ColorBLACK, kIconDim);

  // Convert icon to bitmap for comparison.
  ui::ColorProvider color_provider;
  color_provider.GenerateColorMapForTesting();
  const SkBitmap expected_bitmap = *icon.Rasterize(&color_provider).bitmap();

  auto dialog_model =
      CreateDialogModel(/*ok_callback=*/base::DoNothing(),
                        /*ok_button_style=*/ui::ButtonStyle::kDefault,
                        /*cancel_button=*/false,
                        /*cancel_callback=*/base::DoNothing(),
                        /*cancel_button_style=*/ui::ButtonStyle::kDefault,
                        /*close_callback=*/base::DoNothing(),
                        /*override_button=*/std::nullopt,
                        /*paragraphs=*/{u"paragraph"},
                        /*with_checkbox=*/false,
                        /*checkbox_is_checked=*/false,
                        /*icon=*/std::move(icon));

  ModalDialogWrapper::ShowTabModal(std::move(dialog_model), window_->get());

  SkBitmap actual_bitmap = fake_dialog_manager_->GetTitleIcon();

  EXPECT_TRUE(gfx::test::AreBitmapsEqual(expected_bitmap, actual_bitmap));
  EXPECT_FALSE(dialog_destroyed_);
}

TEST_F(ModalDialogWrapperTest, ShowsGfxImage) {
  SkBitmap expected_bitmap;
  expected_bitmap.allocN32Pixels(24, 24);
  auto gfx_image = gfx::Image::CreateFrom1xBitmap(expected_bitmap);
  auto icon = ui::ImageModel::FromImage(gfx_image);

  auto dialog_model =
      CreateDialogModel(/*ok_callback=*/base::DoNothing(),
                        /*ok_button_style=*/ui::ButtonStyle::kDefault,
                        /*cancel_button=*/false,
                        /*cancel_callback=*/base::DoNothing(),
                        /*cancel_button_style=*/ui::ButtonStyle::kDefault,
                        /*close_callback=*/base::DoNothing(),
                        /*override_button=*/std::nullopt,
                        /*paragraphs=*/{u"paragraph"},
                        /*with_checkbox=*/false,
                        /*checkbox_is_checked=*/false,
                        /*icon=*/std::move(icon));

  ModalDialogWrapper::ShowTabModal(std::move(dialog_model), window_->get());

  SkBitmap actual_bitmap = fake_dialog_manager_->GetTitleIcon();

  EXPECT_TRUE(gfx::test::AreBitmapsEqual(expected_bitmap, actual_bitmap));
  EXPECT_FALSE(dialog_destroyed_);
}

}  // namespace ui
