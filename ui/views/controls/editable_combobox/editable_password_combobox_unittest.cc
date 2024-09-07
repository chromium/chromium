// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/editable_combobox/editable_password_combobox.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/models/simple_combobox_model.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/render_text.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/editable_combobox/editable_combobox.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

namespace views {

using ::testing::Return;
using ::testing::StrictMock;

class EditablePasswordComboboxTest : public ViewsTestBase {
 public:
  static constexpr int kComboboxId = 123;

  EditablePasswordComboboxTest() = default;

  EditablePasswordComboboxTest(const EditablePasswordComboboxTest&) = delete;
  EditablePasswordComboboxTest& operator=(const EditablePasswordComboboxTest&) =
      delete;

  // Initializes the combobox and the widget containing it.
  void SetUp() override;

  void TearDown() override;

 protected:
  size_t GetItemCount() {
    return combobox()->GetMenuModelForTesting()->GetItemCount();
  }

  std::u16string GetItemAt(size_t index) {
    return combobox()->GetItemTextForTesting(index);
  }

  // Clicks the eye button to reveal or obscure the password.
  void ClickEye() {
    ToggleImageButton* eye = combobox()->GetEyeButtonForTesting();
    generator_->MoveMouseTo(eye->GetBoundsInScreen().CenterPoint());
    generator_->ClickLeftButton();
  }

  EditablePasswordCombobox* combobox() {
    return static_cast<EditablePasswordCombobox*>(
        widget_->GetContentsView()->GetViewByID(kComboboxId));
  }

  base::MockCallback<Button::PressedCallback::Callback>* eye_mock_callback() {
    return &eye_callback_;
  }

 private:
  std::unique_ptr<Widget> widget_;
  base::MockCallback<Button::PressedCallback::Callback> eye_callback_;

  // Used for simulating eye button clicks.
  std::unique_ptr<ui::test::EventGenerator> generator_;
};

// Initializes the combobox with the given items.
void EditablePasswordComboboxTest::SetUp() {
  ViewsTestBase::SetUp();

  auto combobox = std::make_unique<EditablePasswordCombobox>(
      std::make_unique<ui::SimpleComboboxModel>(
          std::vector<ui::SimpleComboboxModel::Item>{
              ui::SimpleComboboxModel::Item(u"item0"),
              ui::SimpleComboboxModel::Item(u"item1")}),
      style::CONTEXT_BUTTON, style::STYLE_PRIMARY, /* display_arrow=*/true,
      Button::PressedCallback(eye_callback_.Get()));
  // Set dummy tooltips and name to avoid running into a11y-related DCHECKs.
  combobox->SetPasswordIconTooltips(u"Show password", u"Hide password");
  combobox->GetViewAccessibility().SetName(u"Password field");
  combobox->SetID(kComboboxId);

  widget_ = std::make_unique<Widget>();
  Widget::InitParams params =
      CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                   Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.bounds = gfx::Rect(0, 0, 1000, 1000);
  combobox->SetBoundsRect(gfx::Rect(0, 0, 500, 40));

  widget_->Init(std::move(params));
  View* container = widget_->SetContentsView(std::make_unique<View>());
  container->AddChildView(std::move(combobox));

  generator_ =
      std::make_unique<ui::test::EventGenerator>(GetRootWindow(widget_.get()));

  widget_->Show();

#if BUILDFLAG(IS_MAC)
  // The event loop needs to be flushed here, otherwise in various tests:
  // 1. The actual showing of the native window backing the widget gets delayed
  //    until a spin of the event loop.
  // 2. The combobox menu object is triggered, and it starts listening for the
  //    "window did become key" notification as a sign that it lost focus and
  //    should close.
  // 3. The event loop is spun, and the actual showing of the native window
  //    triggers the close of the menu opened from within the window.
  base::RunLoop().RunUntilIdle();
#endif
}

void EditablePasswordComboboxTest::TearDown() {
  generator_.reset();
  if (widget_) {
    widget_->Close();
  }
  ViewsTestBase::TearDown();
}

TEST_F(EditablePasswordComboboxTest, PasswordCanBeHiddenAndRevealed) {
  const std::u16string kObscuredPassword(
      5, gfx::RenderText::kPasswordReplacementChar);

  ASSERT_EQ(2u, GetItemCount());
  EXPECT_FALSE(combobox()->ArePasswordsRevealed());
  EXPECT_EQ(kObscuredPassword, GetItemAt(0));
  EXPECT_EQ(kObscuredPassword, GetItemAt(1));

  combobox()->RevealPasswords(/*revealed=*/true);
  EXPECT_TRUE(combobox()->ArePasswordsRevealed());
  EXPECT_EQ(u"item0", GetItemAt(0));
  EXPECT_EQ(u"item1", GetItemAt(1));

  combobox()->RevealPasswords(/*revealed=*/false);
  EXPECT_FALSE(combobox()->ArePasswordsRevealed());
  EXPECT_EQ(kObscuredPassword, GetItemAt(0));
  EXPECT_EQ(kObscuredPassword, GetItemAt(1));
}

TEST_F(EditablePasswordComboboxTest, EyeButtonClickInvokesCallback) {
  EXPECT_CALL(*eye_mock_callback(), Run);
  ClickEye();
}

TEST_F(EditablePasswordComboboxTest, NoCrashWithoutWidget) {
  auto combobox = std::make_unique<EditablePasswordCombobox>(
      std::make_unique<ui::SimpleComboboxModel>(
          std::vector<ui::SimpleComboboxModel::Item>{
              ui::SimpleComboboxModel::Item(u"item0"),
              ui::SimpleComboboxModel::Item(u"item1")}));
  // Showing the dropdown should silently fail.
  combobox->RevealPasswords(true);
}

}  // namespace views
