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
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_provider_manager.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/strings/grit/ui_strings.h"

namespace ui {

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kCheckboxId);
// Icons used in tests below.
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
const gfx::VectorIcon kTestIcon(kTestIconReps,
                                std::size(kTestIconReps),
                                "test_icon");

ui::ImageModel CreateBitmapImage(SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(kIconDim, kIconDim);
  bitmap.eraseColor(color);
  return ui::ImageModel::FromImage(gfx::Image::CreateFrom1xBitmap(bitmap));
}

}  // namespace

class ModalDialogWrapperTest : public testing::Test {
 protected:
  // Test helper class to build a DialogModel with a fluent interface.
  class DialogModelBuilder {
   public:
    explicit DialogModelBuilder(bool* dialog_destroyed_flag)
        : dialog_destroyed_flag_(dialog_destroyed_flag) {}

    DialogModelBuilder& WithOkButton(
        base::OnceClosure callback,
        ui::ButtonStyle style = ui::ButtonStyle::kDefault) {
      ok_callback_ = std::move(callback);
      ok_button_style_ = style;
      return *this;
    }

    DialogModelBuilder& WithCancelButton(
        base::OnceClosure callback,
        ui::ButtonStyle style = ui::ButtonStyle::kDefault) {
      has_cancel_button_ = true;
      cancel_callback_ = std::move(callback);
      cancel_button_style_ = style;
      return *this;
    }

    DialogModelBuilder& WithCloseAction(base::OnceClosure callback) {
      close_callback_ = std::move(callback);
      return *this;
    }

    DialogModelBuilder& OverrideDefaultButton(
        mojom::DialogButton override_button) {
      override_button_ = override_button;
      return *this;
    }

    DialogModelBuilder& WithParagraphs(
        const std::vector<std::u16string>& paragraphs) {
      paragraphs_ = paragraphs;
      return *this;
    }

    DialogModelBuilder& WithCheckbox(bool is_checked) {
      has_checkbox_ = true;
      checkbox_is_checked_ = is_checked;
      return *this;
    }

    DialogModelBuilder& WithIcon(ui::ImageModel icon) {
      icon_ = std::move(icon);
      return *this;
    }

    DialogModelBuilder& AddMenuItem(ui::ImageModel icon,
                                    const std::u16string& label) {
      menu_items_.emplace_back(std::move(icon), label, base::DoNothing());
      return *this;
    }

    DialogModelBuilder& AddMenuItemWithCallback(
        ui::ImageModel icon,
        const std::u16string& label,
        base::RepeatingClosure callback) {
      menu_items_.emplace_back(std::move(icon), label, std::move(callback));
      return *this;
    }

    std::unique_ptr<ui::DialogModel> Build() {
      ui::DialogModel::Builder dialog_builder;
      dialog_builder.SetTitle(u"title");

      if (icon_) {
        dialog_builder.SetIcon(*icon_);
      }

      for (const auto& paragraph_text : paragraphs_) {
        dialog_builder.AddParagraph(ui::DialogModelLabel(paragraph_text));
      }

      for (auto& item : menu_items_) {
        dialog_builder.AddMenuItem(
            std::move(std::get<0>(item)), std::get<1>(item),
            base::BindRepeating([](base::RepeatingClosure callback,
                                   int event_flags) { callback.Run(); },
                                std::move(std::get<2>(item))));
      }

      if (has_checkbox_) {
        dialog_builder.AddCheckbox(
            kCheckboxId, ui::DialogModelLabel(u"checkbox label"),
            ui::DialogModelCheckbox::Params().SetIsChecked(
                checkbox_is_checked_));
      }

      dialog_builder.AddOkButton(
          std::move(ok_callback_),
          ui::DialogModel::Button::Params().SetLabel(u"ok").SetStyle(
              ok_button_style_));

      if (has_cancel_button_) {
        dialog_builder.AddCancelButton(
            std::move(cancel_callback_),
            ui::DialogModel::Button::Params().SetLabel(u"cancel").SetStyle(
                cancel_button_style_));
      }

      // Capture the pointer to the destruction flag by value. This prevents
      // the lambda from holding a dangling reference to the temporary builder
      // instance.
      bool* flag_ptr = dialog_destroyed_flag_;
      dialog_builder.SetCloseActionCallback(std::move(close_callback_))
          .SetDialogDestroyingCallback(
              base::BindLambdaForTesting([flag_ptr]() { *flag_ptr = true; }));

      if (override_button_) {
        dialog_builder.OverrideDefaultButton(*override_button_);
      }
      return dialog_builder.Build();
    }

   private:
    raw_ptr<bool> dialog_destroyed_flag_;

    // Default values match the original CreateDialogModel function.
    base::OnceClosure ok_callback_ = base::DoNothing();
    ui::ButtonStyle ok_button_style_ = ui::ButtonStyle::kDefault;

    bool has_cancel_button_ = false;
    base::OnceClosure cancel_callback_ = base::DoNothing();
    ui::ButtonStyle cancel_button_style_ = ui::ButtonStyle::kDefault;

    base::OnceClosure close_callback_ = base::DoNothing();
    std::optional<mojom::DialogButton> override_button_;
    std::vector<std::u16string> paragraphs_ = {u"paragraph"};
    std::vector<
        std::tuple<ui::ImageModel, std::u16string, base::RepeatingClosure>>
        menu_items_;

    bool has_checkbox_ = false;
    bool checkbox_is_checked_ = false;
    std::optional<ui::ImageModel> icon_;
  };

  void SetUp() override {
    window_ = ui::WindowAndroid::CreateForTesting();
    fake_dialog_manager_ = FakeModalDialogManagerBridge::CreateForTab(
        window_->get(), /*use_empty_java_presenter=*/false);
  }

  bool dialog_destroyed_ = false;
  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting> window_;
  std::unique_ptr<FakeModalDialogManagerBridge> fake_dialog_manager_;
};

TEST_F(ModalDialogWrapperTest, CallOkButton) {
  bool ok_called = false;

  auto dialog_model =
      DialogModelBuilder(&dialog_destroyed_)
          .WithOkButton(base::BindLambdaForTesting([&]() { ok_called = true; }))
          .Build();

  ModalDialogWrapper::ShowTabModal(std::move(dialog_model), window_->get());
  fake_dialog_manager_->ClickPositiveButton();

  EXPECT_TRUE(ok_called);
  EXPECT_TRUE(dialog_destroyed_);
}

TEST_F(ModalDialogWrapperTest, CallCancelButton) {
  bool ok_called = false, cancel_called = false;

  auto dialog_model =
      DialogModelBuilder(&dialog_destroyed_)
          .WithOkButton(base::BindLambdaForTesting([&]() { ok_called = true; }))
          .WithCancelButton(
              base::BindLambdaForTesting([&]() { cancel_called = true; }))
          .Build();

  ModalDialogWrapper::ShowTabModal(std::move(dialog_model), window_->get());
  fake_dialog_manager_->ClickNegativeButton();

  EXPECT_FALSE(ok_called);
  EXPECT_TRUE(cancel_called);
  EXPECT_TRUE(dialog_destroyed_);
}

TEST_F(ModalDialogWrapperTest, CloseDialogFromNative) {
  bool ok_called = false, cancel_called = false, closed = false;

  auto dialog_model =
      DialogModelBuilder(&dialog_destroyed_)
          .WithOkButton(base::BindLambdaForTesting([&]() { ok_called = true; }))
          .WithCancelButton(
              base::BindLambdaForTesting([&]() { cancel_called = true; }))
          .WithCloseAction(base::BindLambdaForTesting([&]() { closed = true; }))
          .Build();

  ModalDialogWrapper::ShowTabModal(std::move(dialog_model), window_->get());
  ModalDialogWrapper::GetDialogForTesting()->Close();

  EXPECT_FALSE(ok_called);
  EXPECT_FALSE(cancel_called);
  EXPECT_TRUE(closed);
  EXPECT_TRUE(dialog_destroyed_);
}

TEST_F(ModalDialogWrapperTest, ModalButtonsNoProminent) {
  auto dialog_model = DialogModelBuilder(&dialog_destroyed_).Build();

  ModalDialogWrapper::ShowTabModal(std::move(dialog_model), window_->get());

  EXPECT_EQ(static_cast<ui::ModalDialogWrapper::ModalDialogButtonStyles>(
                fake_dialog_manager_->GetButtonStyles()),
            ui::ModalDialogWrapper::ModalDialogButtonStyles::
                kPrimaryOutlineNegativeOutline);
  EXPECT_FALSE(dialog_destroyed_);
}

TEST_F(ModalDialogWrapperTest, ModalButtonsPrimaryProminentNoNegative) {
  auto dialog_model =
      DialogModelBuilder(&dialog_destroyed_)
          .WithOkButton(base::DoNothing(), ui::ButtonStyle::kProminent)
          .Build();

  ModalDialogWrapper::ShowTabModal(std::move(dialog_model), window_->get());

  EXPECT_EQ(static_cast<ui::ModalDialogWrapper::ModalDialogButtonStyles>(
                fake_dialog_manager_->GetButtonStyles()),
            ui::ModalDialogWrapper::ModalDialogButtonStyles::
                kPrimaryFilledNoNegative);
  EXPECT_FALSE(dialog_destroyed_);
}

TEST_F(ModalDialogWrapperTest, ModalButtonsPrimaryProminent) {
  auto dialog_model =
      DialogModelBuilder(&dialog_destroyed_)
          .WithOkButton(base::DoNothing(), ui::ButtonStyle::kProminent)
          .WithCancelButton(base::DoNothing())
          .Build();

  ModalDialogWrapper::ShowTabModal(std::move(dialog_model), window_->get());

  EXPECT_EQ(static_cast<ui::ModalDialogWrapper::ModalDialogButtonStyles>(
                fake_dialog_manager_->GetButtonStyles()),
            ui::ModalDialogWrapper::ModalDialogButtonStyles::
                kPrimaryFilledNegativeOutline);
  EXPECT_FALSE(dialog_destroyed_);
}

TEST_F(ModalDialogWrapperTest, ModalButtonsNegativeProminent) {
  auto dialog_model =
      DialogModelBuilder(&dialog_destroyed_)
          .WithCancelButton(base::DoNothing(), ui::ButtonStyle::kProminent)
          .Build();

  ModalDialogWrapper::ShowTabModal(std::move(dialog_model), window_->get());

  EXPECT_EQ(static_cast<ui::ModalDialogWrapper::ModalDialogButtonStyles>(
                fake_dialog_manager_->GetButtonStyles()),
            ui::ModalDialogWrapper::ModalDialogButtonStyles::
                kPrimaryOutlineNegativeFilled);
  EXPECT_FALSE(dialog_destroyed_);
}

TEST_F(ModalDialogWrapperTest, ModalButtonsOverriddenNone) {
  auto dialog_model =
      DialogModelBuilder(&dialog_destroyed_)
          .WithOkButton(base::DoNothing(), ui::ButtonStyle::kProminent)
          .WithCancelButton(base::DoNothing(), ui::ButtonStyle::kProminent)
          .OverrideDefaultButton(ui::mojom::DialogButton::kNone)
          .Build();

  ModalDialogWrapper::ShowTabModal(std::move(dialog_model), window_->get());

  EXPECT_EQ(static_cast<ui::ModalDialogWrapper::ModalDialogButtonStyles>(
                fake_dialog_manager_->GetButtonStyles()),
            ui::ModalDialogWrapper::ModalDialogButtonStyles::
                kPrimaryOutlineNegativeOutline);
  EXPECT_FALSE(dialog_destroyed_);
}

TEST_F(ModalDialogWrapperTest, ModalButtonsOverriddenPositive) {
  auto dialog_model =
      DialogModelBuilder(&dialog_destroyed_)
          .WithCancelButton(base::DoNothing(), ui::ButtonStyle::kProminent)
          .OverrideDefaultButton(ui::mojom::DialogButton::kOk)
          .Build();

  ModalDialogWrapper::ShowTabModal(std::move(dialog_model), window_->get());

  EXPECT_EQ(static_cast<ui::ModalDialogWrapper::ModalDialogButtonStyles>(
                fake_dialog_manager_->GetButtonStyles()),
            ui::ModalDialogWrapper::ModalDialogButtonStyles::
                kPrimaryFilledNegativeOutline);
  EXPECT_FALSE(dialog_destroyed_);
}

TEST_F(ModalDialogWrapperTest, ModalButtonsOverriddenNegative) {
  auto dialog_model =
      DialogModelBuilder(&dialog_destroyed_)
          .WithOkButton(base::DoNothing(), ui::ButtonStyle::kProminent)
          .WithCancelButton(base::DoNothing())
          .OverrideDefaultButton(ui::mojom::DialogButton::kCancel)
          .Build();

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

  auto dialog_model_1 =
      DialogModelBuilder(&dialog_destroyed_).WithParagraphs(paragraphs).Build();

  ModalDialogWrapper::ShowTabModal(std::move(dialog_model_1), window_->get());

  std::vector<std::u16string> displayed_paragraphs_1 =
      fake_dialog_manager_->GetMessageParagraphs();
  ASSERT_EQ(displayed_paragraphs_1.size(), 2u);
  EXPECT_EQ(displayed_paragraphs_1.front(), paragraphs.front());
  EXPECT_EQ(displayed_paragraphs_1.back(), paragraphs.back());

  // Remove the last element and confirm the behavior.
  paragraphs.pop_back();

  auto dialog_model_2 =
      DialogModelBuilder(&dialog_destroyed_).WithParagraphs(paragraphs).Build();

  ModalDialogWrapper::ShowTabModal(std::move(dialog_model_2), window_->get());

  std::vector<std::u16string> displayed_paragraphs_2 =
      fake_dialog_manager_->GetMessageParagraphs();
  ASSERT_EQ(displayed_paragraphs_2.size(), 1u);
  EXPECT_EQ(displayed_paragraphs_2.front(), paragraphs.front());
}

TEST_F(ModalDialogWrapperTest, ParagraphsContainReplacements) {
  // Use a vector of booleans to track the invocation of distinct callbacks.
  std::vector<bool> callbacks_called = {false, false, false};

  // In unit tests, the full resource pack isn't loaded.
  ResourceBundle::GetSharedInstance().OverrideLocaleStringResource(IDS_APP_OK,
                                                                   u"OK");
  ResourceBundle::GetSharedInstance().OverrideLocaleStringResource(
      IDS_APP_CANCEL, u"Cancel");
  ResourceBundle::GetSharedInstance().OverrideLocaleStringResource(
      IDS_APP_CLOSE, u"Close");

  ui::DialogModel::Builder dialog_builder;
  dialog_builder.SetTitle(u"Mixed Content Paragraph Test")
      .SetDialogDestroyingCallback(
          base::BindLambdaForTesting([&]() { dialog_destroyed_ = true; }));

  // Paragraph 0: A standard paragraph with only plain text.
  dialog_builder.AddParagraph(
      DialogModelLabel(u"This is a paragraph with no links."));

  // Paragraph 1: Contains multiple links and plain text spans to test
  // correct callback indexing across a single paragraph.
  auto replacements = std::vector<DialogModelLabel::TextReplacement>();

  // Helper lambda to create the correct callback type, avoiding ambiguity.
  auto make_callback = [&](int index) -> DialogModelLabel::Callback {
    return base::BindRepeating(
        [](std::vector<bool>* callbacks, int i, const ui::Event& event) {
          (*callbacks)[i] = true;
        },
        &callbacks_called, index);
  };

  // Link with global index 0.
  replacements.push_back(
      DialogModelLabel::CreateLink(IDS_APP_OK, make_callback(0)));
  replacements.push_back(
      DialogModelLabel::CreatePlainText(u" is the first link. "));
  // Link with global index 1.
  replacements.push_back(
      DialogModelLabel::CreateLink(IDS_APP_CANCEL, make_callback(1)));
  replacements.push_back(
      DialogModelLabel::CreatePlainText(u" is the second. Finally, "));
  // Link with global index 2.
  replacements.push_back(
      DialogModelLabel::CreateLink(IDS_APP_CLOSE, make_callback(2)));
  dialog_builder.AddParagraph(
      DialogModelLabel::CreateWithReplacements(0, std::move(replacements)));

  // Paragraph 2: An empty paragraph to test this edge case.
  dialog_builder.AddParagraph(DialogModelLabel(u""));

  // Paragraph 3: A trailing plain text paragraph to ensure correct handling
  // after paragraphs with links and empty ones.
  dialog_builder.AddParagraph(
      DialogModelLabel(u"This is a final plain paragraph."));

  auto dialog_model = dialog_builder.Build();

  // Trigger Dialog.
  ModalDialogWrapper::ShowTabModal(std::move(dialog_model), window_->get());

  // Verify the paragraphs text.
  std::vector<std::u16string> displayed_paragraphs =
      fake_dialog_manager_->GetMessageParagraphs();
  ASSERT_EQ(displayed_paragraphs.size(), 4u);
  EXPECT_EQ(displayed_paragraphs[0], u"This is a paragraph with no links.");
  EXPECT_EQ(displayed_paragraphs[1],
            u"OK is the first link. Cancel is the second. Finally, Close");
  EXPECT_EQ(displayed_paragraphs[2], u"");
  EXPECT_EQ(displayed_paragraphs[3], u"This is a final plain paragraph.");

  // Simulate link clicks.
  EXPECT_FALSE(callbacks_called[0]);
  EXPECT_FALSE(callbacks_called[1]);
  EXPECT_FALSE(callbacks_called[2]);

  // Click the middle link.
  fake_dialog_manager_->ClickLinkInMessageParagraphs(1);
  EXPECT_FALSE(callbacks_called[0]);
  EXPECT_TRUE(callbacks_called[1]);
  EXPECT_FALSE(callbacks_called[2]);

  // Click the last link.
  fake_dialog_manager_->ClickLinkInMessageParagraphs(2);
  EXPECT_FALSE(callbacks_called[0]);
  EXPECT_TRUE(callbacks_called[1]);
  EXPECT_TRUE(callbacks_called[2]);

  // Click the first link.
  fake_dialog_manager_->ClickLinkInMessageParagraphs(0);
  EXPECT_TRUE(callbacks_called[0]);
  EXPECT_TRUE(callbacks_called[1]);
  EXPECT_TRUE(callbacks_called[2]);
}

TEST_F(ModalDialogWrapperTest, Checkbox_InitialStateUnchecked) {
  auto dialog_model =
      DialogModelBuilder(&dialog_destroyed_).WithCheckbox(false).Build();
  ModalDialogWrapper::ShowTabModal(std::move(dialog_model), window_->get());

  EXPECT_FALSE(fake_dialog_manager_->IsCheckboxChecked());
}

TEST_F(ModalDialogWrapperTest, Checkbox_InitialStateChecked) {
  auto dialog_model =
      DialogModelBuilder(&dialog_destroyed_).WithCheckbox(true).Build();
  ModalDialogWrapper::ShowTabModal(std::move(dialog_model), window_->get());

  EXPECT_TRUE(fake_dialog_manager_->IsCheckboxChecked());
}

TEST_F(ModalDialogWrapperTest, Checkbox_StateSynchronizedAfterToggle) {
  auto dialog_model =
      DialogModelBuilder(&dialog_destroyed_).WithCheckbox(false).Build();
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

TEST_F(ModalDialogWrapperTest, TitleIcon_ShowsVectorIcon) {
  auto icon =
      ui::ImageModel::FromVectorIcon(kTestIcon, SK_ColorBLACK, kIconDim);

  // Convert icon to bitmap for comparison.
  ui::ColorProvider color_provider;
  color_provider.GenerateColorMapForTesting();
  const SkBitmap expected_bitmap = *icon.Rasterize(&color_provider).bitmap();

  auto dialog_model =
      DialogModelBuilder(&dialog_destroyed_).WithIcon(std::move(icon)).Build();

  ModalDialogWrapper::ShowTabModal(std::move(dialog_model), window_->get());

  SkBitmap actual_bitmap = fake_dialog_manager_->GetTitleIcon();

  EXPECT_TRUE(gfx::test::AreBitmapsEqual(expected_bitmap, actual_bitmap));
  EXPECT_FALSE(dialog_destroyed_);
}

TEST_F(ModalDialogWrapperTest, TitleIcon_ShowsGfxImage) {
  auto icon = CreateBitmapImage(SK_ColorGREEN);
  ui::ColorProvider color_provider;
  color_provider.GenerateColorMapForTesting();
  const SkBitmap expected_bitmap = *icon.Rasterize(&color_provider).bitmap();

  auto dialog_model =
      DialogModelBuilder(&dialog_destroyed_).WithIcon(std::move(icon)).Build();

  ModalDialogWrapper::ShowTabModal(std::move(dialog_model), window_->get());

  SkBitmap actual_bitmap = fake_dialog_manager_->GetTitleIcon();

  EXPECT_TRUE(gfx::test::AreBitmapsEqual(expected_bitmap, actual_bitmap));
  EXPECT_FALSE(dialog_destroyed_);
}

TEST_F(ModalDialogWrapperTest, ShowsMenuItems) {
  // --- Item 1: SkBitmap-based ---
  auto icon1 = CreateBitmapImage(SK_ColorRED);
  ui::ColorProvider color_provider;
  color_provider.GenerateColorMapForTesting();
  const SkBitmap expected_bitmap1 = *icon1.Rasterize(&color_provider).bitmap();
  const std::u16string label1 = u"Red Bitmap Item";

  // --- Item 2: VectorIcon-based ---
  auto icon2 =
      ui::ImageModel::FromVectorIcon(kTestIcon, SK_ColorBLUE, kIconDim);
  const SkBitmap expected_bitmap2 = *icon2.Rasterize(&color_provider).bitmap();
  const std::u16string label2 = u"Blue Vector Item";

  // --- Build and Show ---
  auto dialog_model = DialogModelBuilder(&dialog_destroyed_)
                          .AddMenuItem(std::move(icon1), label1)
                          .AddMenuItem(std::move(icon2), label2)
                          .Build();
  ModalDialogWrapper::ShowTabModal(std::move(dialog_model), window_->get());

  // --- Assert ---
  std::vector<std::u16string> actual_labels =
      fake_dialog_manager_->GetMenuItemTexts();
  std::vector<SkBitmap> actual_icons = fake_dialog_manager_->GetMenuItemIcons();
  // Try a null callback
  fake_dialog_manager_->ClickMenuItem(0);

  ASSERT_EQ(actual_labels.size(), 2u);
  ASSERT_EQ(actual_icons.size(), 2u);

  EXPECT_EQ(actual_labels[0], label1);
  EXPECT_EQ(actual_labels[1], label2);
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(actual_icons[0], expected_bitmap1));
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(actual_icons[1], expected_bitmap2));

  EXPECT_FALSE(dialog_destroyed_);
}

TEST_F(ModalDialogWrapperTest, MenuItem_Callbacks) {
  bool callback1_called = false;
  bool callback2_called = false;
  auto icon = CreateBitmapImage(SK_ColorRED);

  auto dialog_model =
      DialogModelBuilder(&dialog_destroyed_)
          .AddMenuItemWithCallback(
              icon, u"Item 1",
              base::BindLambdaForTesting([&]() { callback1_called = true; }))
          .AddMenuItemWithCallback(
              icon, u"Item 2",
              base::BindLambdaForTesting([&]() { callback2_called = true; }))
          .Build();
  ModalDialogWrapper::ShowTabModal(std::move(dialog_model), window_->get());

  fake_dialog_manager_->ClickMenuItem(1);

  EXPECT_FALSE(callback1_called);
  EXPECT_TRUE(callback2_called);

  fake_dialog_manager_->ClickMenuItem(0);
  EXPECT_TRUE(callback1_called);
}

TEST_F(ModalDialogWrapperTest, MenuItem_CallbackDismissesDialog) {
  auto dialog_model =
      DialogModelBuilder(&dialog_destroyed_)
          .AddMenuItemWithCallback(
              CreateBitmapImage(SK_ColorRED), u"Item",
              base::BindLambdaForTesting([&]() {
                ModalDialogWrapper::GetDialogForTesting()->Close();
              }))
          .Build();
  ModalDialogWrapper::ShowTabModal(std::move(dialog_model), window_->get());

  fake_dialog_manager_->ClickMenuItem(0);

  EXPECT_TRUE(dialog_destroyed_);
}

}  // namespace ui
