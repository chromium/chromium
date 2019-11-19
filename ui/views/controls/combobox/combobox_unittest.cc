// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/combobox/combobox.h"

#include <memory>
#include <set>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/models/combobox_model_observer.h"
#include "ui/base/models/menu_model.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/combobox/combobox_listener.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/test/combobox_test_api.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

using base::ASCIIToUTF16;

namespace views {

using test::ComboboxTestApi;

namespace {

// A wrapper of Combobox to intercept the result of OnKeyPressed() and
// OnKeyReleased() methods.
class TestCombobox : public Combobox {
 public:
  explicit TestCombobox(ui::ComboboxModel* model)
      : Combobox(model), key_handled_(false), key_received_(false) {}

  bool OnKeyPressed(const ui::KeyEvent& e) override {
    key_received_ = true;
    key_handled_ = Combobox::OnKeyPressed(e);
    return key_handled_;
  }

  bool OnKeyReleased(const ui::KeyEvent& e) override {
    key_received_ = true;
    key_handled_ = Combobox::OnKeyReleased(e);
    return key_handled_;
  }

  bool key_handled() const { return key_handled_; }
  bool key_received() const { return key_received_; }

  void clear() {
    key_received_ = key_handled_ = false;
  }

 private:
  bool key_handled_;
  bool key_received_;

  DISALLOW_COPY_AND_ASSIGN(TestCombobox);
};

// A concrete class is needed to test the combobox.
class TestComboboxModel : public ui::ComboboxModel {
 public:
  TestComboboxModel() = default;
  ~TestComboboxModel() override = default;

  enum { kItemCount = 10 };

  // ui::ComboboxModel:
  int GetItemCount() const override { return item_count_; }
  base::string16 GetItemAt(int index) override {
    if (IsItemSeparatorAt(index)) {
      NOTREACHED();
      return ASCIIToUTF16("SEPARATOR");
    }
    return ASCIIToUTF16(index % 2 == 0 ? "PEANUT BUTTER" : "JELLY");
  }
  bool IsItemSeparatorAt(int index) override {
    return separators_.find(index) != separators_.end();
  }

  int GetDefaultIndex() const override {
    // Return the first index that is not a separator.
    for (int index = 0; index < kItemCount; ++index) {
      if (separators_.find(index) == separators_.end())
        return index;
    }
    NOTREACHED();
    return 0;
  }

  void AddObserver(ui::ComboboxModelObserver* observer) override {
    observers_.AddObserver(observer);
  }
  void RemoveObserver(ui::ComboboxModelObserver* observer) override {
    observers_.RemoveObserver(observer);
  }

  void SetSeparators(const std::set<int>& separators) {
    separators_ = separators;
    OnModelChanged();
  }

  void set_item_count(int item_count) {
    item_count_ = item_count;
    OnModelChanged();
  }

 private:
  void OnModelChanged() {
    for (auto& observer : observers_)
      observer.OnComboboxModelChanged(this);
  }

  base::ObserverList<ui::ComboboxModelObserver> observers_;
  std::set<int> separators_;
  int item_count_ = kItemCount;

  DISALLOW_COPY_AND_ASSIGN(TestComboboxModel);
};

// A combobox model which refers to a vector.
class VectorComboboxModel : public ui::ComboboxModel {
 public:
  explicit VectorComboboxModel(std::vector<std::string>* values)
      : values_(values) {}
  ~VectorComboboxModel() override = default;

  void set_default_index(int default_index) { default_index_ = default_index; }

  // ui::ComboboxModel:
  int GetItemCount() const override {
    return static_cast<int>(values_->size());
  }
  base::string16 GetItemAt(int index) override {
    return ASCIIToUTF16(values_->at(index));
  }
  bool IsItemSeparatorAt(int index) override { return false; }
  int GetDefaultIndex() const override { return default_index_; }
  void AddObserver(ui::ComboboxModelObserver* observer) override {
    observers_.AddObserver(observer);
  }
  void RemoveObserver(ui::ComboboxModelObserver* observer) override {
    observers_.RemoveObserver(observer);
  }

  void ValuesChanged() {
    for (auto& observer : observers_)
      observer.OnComboboxModelChanged(this);
  }

 private:
  base::ObserverList<ui::ComboboxModelObserver> observers_;
  int default_index_ = 0;
  std::vector<std::string>* const values_;

  DISALLOW_COPY_AND_ASSIGN(VectorComboboxModel);
};

class EvilListener : public ComboboxListener {
 public:
  EvilListener() = default;
  ~EvilListener() override = default;

  // ComboboxListener:
  void OnPerformAction(Combobox* combobox) override {
    delete combobox;
    deleted_ = true;
  }

  bool deleted() const { return deleted_; }

 private:
  bool deleted_ = false;

  DISALLOW_COPY_AND_ASSIGN(EvilListener);
};

class TestComboboxListener : public views::ComboboxListener {
 public:
  TestComboboxListener() = default;
  ~TestComboboxListener() override = default;

  void OnPerformAction(views::Combobox* combobox) override {
    perform_action_index_ = combobox->GetSelectedIndex();
    actions_performed_++;
  }

  int perform_action_index() const {
    return perform_action_index_;
  }

  bool on_perform_action_called() const {
    return actions_performed_ > 0;
  }

  int actions_performed() const {
    return actions_performed_;
  }

 private:
  int perform_action_index_ = -1;
  int actions_performed_ = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestComboboxListener);
};

}  // namespace

class ComboboxTest : public ViewsTestBase {
 public:
  ComboboxTest() = default;

  void TearDown() override {
    if (widget_)
      widget_->Close();
    ViewsTestBase::TearDown();
  }

  void InitCombobox(const std::set<int>* separators) {
    model_ = std::make_unique<TestComboboxModel>();

    if (separators)
      model_->SetSeparators(*separators);

    ASSERT_FALSE(combobox_);
    combobox_ = new TestCombobox(model_.get());
    test_api_ = std::make_unique<ComboboxTestApi>(combobox_);
    test_api_->InstallTestMenuRunner(&menu_show_count_);
    combobox_->SetID(1);

    widget_ = new Widget;
    Widget::InitParams params =
        CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.bounds = gfx::Rect(200, 200, 200, 200);
    widget_->Init(std::move(params));
    View* container = new View();
    widget_->SetContentsView(container);
    container->AddChildView(combobox_);
    widget_->Show();

    combobox_->RequestFocus();
    combobox_->SizeToPreferredSize();

    event_generator_ =
        std::make_unique<ui::test::EventGenerator>(GetRootWindow(widget_));
    event_generator_->set_target(ui::test::EventGenerator::Target::WINDOW);
  }

 protected:
  void PressKey(ui::KeyboardCode key_code, ui::EventFlags flags = ui::EF_NONE) {
    event_generator_->PressKey(key_code, flags);
  }

  void ReleaseKey(ui::KeyboardCode key_code,
                  ui::EventFlags flags = ui::EF_NONE) {
    event_generator_->ReleaseKey(key_code, flags);
  }

  View* GetFocusedView() {
    return widget_->GetFocusManager()->GetFocusedView();
  }

  void PerformMousePress(const gfx::Point& point) {
    ui::MouseEvent pressed_event = ui::MouseEvent(
        ui::ET_MOUSE_PRESSED, point, point, ui::EventTimeForNow(),
        ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
    widget_->OnMouseEvent(&pressed_event);
  }

  void PerformMouseRelease(const gfx::Point& point) {
    ui::MouseEvent released_event = ui::MouseEvent(
        ui::ET_MOUSE_RELEASED, point, point, ui::EventTimeForNow(),
        ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
    widget_->OnMouseEvent(&released_event);
  }

  void PerformClick(const gfx::Point& point) {
    PerformMousePress(point);
    PerformMouseRelease(point);
  }

  // We need widget to populate wrapper class.
  Widget* widget_ = nullptr;

  // |combobox_| will be allocated InitCombobox() and then owned by |widget_|.
  TestCombobox* combobox_ = nullptr;
  std::unique_ptr<ComboboxTestApi> test_api_;

  // Combobox does not take ownership of the model, hence it needs to be scoped.
  std::unique_ptr<TestComboboxModel> model_;

  // The current menu show count.
  int menu_show_count_ = 0;

  std::unique_ptr<ui::test::EventGenerator> event_generator_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ComboboxTest);
};

#if defined(OS_MACOSX)
// Tests whether the various Mac specific keyboard shortcuts invoke the dropdown
// menu or not.
TEST_F(ComboboxTest, KeyTestMac) {
  InitCombobox(nullptr);
  PressKey(ui::VKEY_END);
  EXPECT_EQ(0, combobox_->GetSelectedIndex());
  EXPECT_EQ(1, menu_show_count_);

  PressKey(ui::VKEY_HOME);
  EXPECT_EQ(0, combobox_->GetSelectedIndex());
  EXPECT_EQ(2, menu_show_count_);

  PressKey(ui::VKEY_UP, ui::EF_COMMAND_DOWN);
  EXPECT_EQ(0, combobox_->GetSelectedIndex());
  EXPECT_EQ(3, menu_show_count_);

  PressKey(ui::VKEY_DOWN, ui::EF_COMMAND_DOWN);
  EXPECT_EQ(0, combobox_->GetSelectedIndex());
  EXPECT_EQ(4, menu_show_count_);

  PressKey(ui::VKEY_DOWN);
  EXPECT_EQ(0, combobox_->GetSelectedIndex());
  EXPECT_EQ(5, menu_show_count_);

  PressKey(ui::VKEY_RIGHT);
  EXPECT_EQ(0, combobox_->GetSelectedIndex());
  EXPECT_EQ(5, menu_show_count_);

  PressKey(ui::VKEY_LEFT);
  EXPECT_EQ(0, combobox_->GetSelectedIndex());
  EXPECT_EQ(5, menu_show_count_);

  PressKey(ui::VKEY_UP);
  EXPECT_EQ(0, combobox_->GetSelectedIndex());
  EXPECT_EQ(6, menu_show_count_);

  PressKey(ui::VKEY_PRIOR);
  EXPECT_EQ(0, combobox_->GetSelectedIndex());
  EXPECT_EQ(6, menu_show_count_);

  PressKey(ui::VKEY_NEXT);
  EXPECT_EQ(0, combobox_->GetSelectedIndex());
  EXPECT_EQ(6, menu_show_count_);
}
#endif

// Check that if a combobox is disabled before it has a native wrapper, then the
// native wrapper inherits the disabled state when it gets created.
TEST_F(ComboboxTest, DisabilityTest) {
  model_ = std::make_unique<TestComboboxModel>();

  ASSERT_FALSE(combobox_);
  combobox_ = new TestCombobox(model_.get());
  combobox_->SetEnabled(false);

  widget_ = new Widget;
  Widget::InitParams params =
      CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.bounds = gfx::Rect(100, 100, 100, 100);
  widget_->Init(std::move(params));
  View* container = new View();
  widget_->SetContentsView(container);
  container->AddChildView(combobox_);
  EXPECT_FALSE(combobox_->GetEnabled());
}

// On Mac, key events can't change the currently selected index directly for a
// combobox.
#if !defined(OS_MACOSX)

// Tests the behavior of various keyboard shortcuts on the currently selected
// index.
TEST_F(ComboboxTest, KeyTest) {
  InitCombobox(nullptr);
  PressKey(ui::VKEY_END);
  EXPECT_EQ(model_->GetItemCount(), combobox_->GetSelectedIndex() + 1);
  PressKey(ui::VKEY_HOME);
  EXPECT_EQ(0, combobox_->GetSelectedIndex());
  PressKey(ui::VKEY_DOWN);
  PressKey(ui::VKEY_DOWN);
  EXPECT_EQ(2, combobox_->GetSelectedIndex());
  PressKey(ui::VKEY_RIGHT);
  EXPECT_EQ(2, combobox_->GetSelectedIndex());
  PressKey(ui::VKEY_LEFT);
  EXPECT_EQ(2, combobox_->GetSelectedIndex());
  PressKey(ui::VKEY_UP);
  EXPECT_EQ(1, combobox_->GetSelectedIndex());
  PressKey(ui::VKEY_PRIOR);
  EXPECT_EQ(0, combobox_->GetSelectedIndex());
  PressKey(ui::VKEY_NEXT);
  EXPECT_EQ(model_->GetItemCount(), combobox_->GetSelectedIndex() + 1);
}

// Verifies that we don't select a separator line in combobox when navigating
// through keyboard.
TEST_F(ComboboxTest, SkipSeparatorSimple) {
  std::set<int> separators;
  separators.insert(2);
  InitCombobox(&separators);
  EXPECT_EQ(0, combobox_->GetSelectedIndex());
  PressKey(ui::VKEY_DOWN);
  EXPECT_EQ(1, combobox_->GetSelectedIndex());
  PressKey(ui::VKEY_DOWN);
  EXPECT_EQ(3, combobox_->GetSelectedIndex());
  PressKey(ui::VKEY_UP);
  EXPECT_EQ(1, combobox_->GetSelectedIndex());
  PressKey(ui::VKEY_HOME);
  EXPECT_EQ(0, combobox_->GetSelectedIndex());
  PressKey(ui::VKEY_PRIOR);
  EXPECT_EQ(0, combobox_->GetSelectedIndex());
  PressKey(ui::VKEY_END);
  EXPECT_EQ(9, combobox_->GetSelectedIndex());
}

// Verifies that we never select the separator that is in the beginning of the
// combobox list when navigating through keyboard.
TEST_F(ComboboxTest, SkipSeparatorBeginning) {
  std::set<int> separators;
  separators.insert(0);
  InitCombobox(&separators);
  EXPECT_EQ(1, combobox_->GetSelectedIndex());
  PressKey(ui::VKEY_DOWN);
  EXPECT_EQ(2, combobox_->GetSelectedIndex());
  PressKey(ui::VKEY_DOWN);
  EXPECT_EQ(3, combobox_->GetSelectedIndex());
  PressKey(ui::VKEY_UP);
  EXPECT_EQ(2, combobox_->GetSelectedIndex());
  PressKey(ui::VKEY_HOME);
  EXPECT_EQ(1, combobox_->GetSelectedIndex());
  PressKey(ui::VKEY_PRIOR);
  EXPECT_EQ(1, combobox_->GetSelectedIndex());
  PressKey(ui::VKEY_END);
  EXPECT_EQ(9, combobox_->GetSelectedIndex());
}

// Verifies that we never select the separator that is in the end of the
// combobox list when navigating through keyboard.
TEST_F(ComboboxTest, SkipSeparatorEnd) {
  std::set<int> separators;
  separators.insert(TestComboboxModel::kItemCount - 1);
  InitCombobox(&separators);
  combobox_->SetSelectedIndex(8);
  PressKey(ui::VKEY_DOWN);
  EXPECT_EQ(8, combobox_->GetSelectedIndex());
  PressKey(ui::VKEY_UP);
  EXPECT_EQ(7, combobox_->GetSelectedIndex());
  PressKey(ui::VKEY_END);
  EXPECT_EQ(8, combobox_->GetSelectedIndex());
}

// Verifies that we never select any of the adjacent separators (multiple
// consecutive) that appear in the beginning of the combobox list when
// navigating through keyboard.
TEST_F(ComboboxTest, SkipMultipleSeparatorsAtBeginning) {
  std::set<int> separators;
  separators.insert(0);
  separators.insert(1);
  separators.insert(2);
  InitCombobox(&separators);
  EXPECT_EQ(3, combobox_->GetSelectedIndex());
  PressKey(ui::VKEY_DOWN);
  EXPECT_EQ(4, combobox_->GetSelectedIndex());
  PressKey(ui::VKEY_UP);
  EXPECT_EQ(3, combobox_->GetSelectedIndex());
  PressKey(ui::VKEY_NEXT);
  EXPECT_EQ(9, combobox_->GetSelectedIndex());
  PressKey(ui::VKEY_HOME);
  EXPECT_EQ(3, combobox_->GetSelectedIndex());
  PressKey(ui::VKEY_END);
  EXPECT_EQ(9, combobox_->GetSelectedIndex());
  PressKey(ui::VKEY_PRIOR);
  EXPECT_EQ(3, combobox_->GetSelectedIndex());
}

// Verifies that we never select any of the adjacent separators (multiple
// consecutive) that appear in the middle of the combobox list when navigating
// through keyboard.
TEST_F(ComboboxTest, SkipMultipleAdjacentSeparatorsAtMiddle) {
  std::set<int> separators;
  separators.insert(4);
  separators.insert(5);
  separators.insert(6);
  InitCombobox(&separators);
  combobox_->SetSelectedIndex(3);
  PressKey(ui::VKEY_DOWN);
  EXPECT_EQ(7, combobox_->GetSelectedIndex());
  PressKey(ui::VKEY_UP);
  EXPECT_EQ(3, combobox_->GetSelectedIndex());
}

// Verifies that we never select any of the adjacent separators (multiple
// consecutive) that appear in the end of the combobox list when navigating
// through keyboard.
TEST_F(ComboboxTest, SkipMultipleSeparatorsAtEnd) {
  std::set<int> separators;
  separators.insert(7);
  separators.insert(8);
  separators.insert(9);
  InitCombobox(&separators);
  combobox_->SetSelectedIndex(6);
  PressKey(ui::VKEY_DOWN);
  EXPECT_EQ(6, combobox_->GetSelectedIndex());
  PressKey(ui::VKEY_UP);
  EXPECT_EQ(5, combobox_->GetSelectedIndex());
  PressKey(ui::VKEY_HOME);
  EXPECT_EQ(0, combobox_->GetSelectedIndex());
  PressKey(ui::VKEY_NEXT);
  EXPECT_EQ(6, combobox_->GetSelectedIndex());
  PressKey(ui::VKEY_PRIOR);
  EXPECT_EQ(0, combobox_->GetSelectedIndex());
  PressKey(ui::VKEY_END);
  EXPECT_EQ(6, combobox_->GetSelectedIndex());
}
#endif  // !OS_MACOSX

TEST_F(ComboboxTest, GetTextForRowTest) {
  std::set<int> separators;
  separators.insert(0);
  separators.insert(1);
  separators.insert(9);
  InitCombobox(&separators);
  for (int i = 0; i < combobox_->GetRowCount(); ++i) {
    if (separators.count(i) != 0) {
      EXPECT_TRUE(combobox_->GetTextForRow(i).empty()) << i;
    } else {
      EXPECT_EQ(ASCIIToUTF16(i % 2 == 0 ? "PEANUT BUTTER" : "JELLY"),
                combobox_->GetTextForRow(i)) << i;
    }
  }
}

// Verifies selecting the first matching value (and returning whether found).
TEST_F(ComboboxTest, SelectValue) {
  InitCombobox(nullptr);
  ASSERT_EQ(model_->GetDefaultIndex(), combobox_->GetSelectedIndex());
  EXPECT_TRUE(combobox_->SelectValue(ASCIIToUTF16("PEANUT BUTTER")));
  EXPECT_EQ(0, combobox_->GetSelectedIndex());
  EXPECT_TRUE(combobox_->SelectValue(ASCIIToUTF16("JELLY")));
  EXPECT_EQ(1, combobox_->GetSelectedIndex());
  EXPECT_FALSE(combobox_->SelectValue(ASCIIToUTF16("BANANAS")));
  EXPECT_EQ(1, combobox_->GetSelectedIndex());
}

TEST_F(ComboboxTest, ListenerHandlesDelete) {
  TestComboboxModel model;

  // |combobox| will be deleted on change.
  TestCombobox* combobox = new TestCombobox(&model);
  std::unique_ptr<EvilListener> evil_listener(new EvilListener());
  combobox->set_listener(evil_listener.get());
  ASSERT_NO_FATAL_FAILURE(ComboboxTestApi(combobox).PerformActionAt(2));
  EXPECT_TRUE(evil_listener->deleted());
}

TEST_F(ComboboxTest, Click) {
  InitCombobox(nullptr);

  TestComboboxListener listener;
  combobox_->set_listener(&listener);
  combobox_->Layout();

  // Click the left side. The menu is shown.
  EXPECT_EQ(0, menu_show_count_);
  PerformClick(gfx::Point(combobox_->x() + 1,
                          combobox_->y() + combobox_->height() / 2));
  EXPECT_FALSE(listener.on_perform_action_called());
  EXPECT_EQ(1, menu_show_count_);
}

TEST_F(ComboboxTest, ClickButDisabled) {
  InitCombobox(nullptr);

  TestComboboxListener listener;
  combobox_->set_listener(&listener);

  combobox_->Layout();
  combobox_->SetEnabled(false);

  // Click the left side, but nothing happens since the combobox is disabled.
  PerformClick(gfx::Point(combobox_->x() + 1,
                          combobox_->y() + combobox_->height() / 2));
  EXPECT_FALSE(listener.on_perform_action_called());
  EXPECT_EQ(0, menu_show_count_);
}

TEST_F(ComboboxTest, NotifyOnClickWithReturnKey) {
  InitCombobox(nullptr);

  TestComboboxListener listener;
  combobox_->set_listener(&listener);

  // The click event is ignored. Instead the menu is shown.
  PressKey(ui::VKEY_RETURN);
  EXPECT_EQ(PlatformStyle::kReturnClicksFocusedControl ? 1 : 0,
            menu_show_count_);
  EXPECT_FALSE(listener.on_perform_action_called());
}

TEST_F(ComboboxTest, NotifyOnClickWithSpaceKey) {
  InitCombobox(nullptr);

  TestComboboxListener listener;
  combobox_->set_listener(&listener);

  // The click event is ignored. Instead the menu is shwon.
  PressKey(ui::VKEY_SPACE);
  EXPECT_EQ(1, menu_show_count_);
  EXPECT_FALSE(listener.on_perform_action_called());

  ReleaseKey(ui::VKEY_SPACE);
  EXPECT_EQ(1, menu_show_count_);
  EXPECT_FALSE(listener.on_perform_action_called());
}

// Test that accessibility action events show the combobox dropdown.
TEST_F(ComboboxTest, ShowViaAccessibleAction) {
  InitCombobox(nullptr);

  ui::AXActionData data;
  data.action = ax::mojom::Action::kDoDefault;

  EXPECT_EQ(0, menu_show_count_);
  combobox_->HandleAccessibleAction(data);
  EXPECT_EQ(1, menu_show_count_);

  // ax::mojom::Action::kShowContextMenu is specifically for a context menu
  // (e.g. right- click). Combobox should ignore it.
  data.action = ax::mojom::Action::kShowContextMenu;
  combobox_->HandleAccessibleAction(data);
  EXPECT_EQ(1, menu_show_count_);  // No change.

  data.action = ax::mojom::Action::kBlur;
  combobox_->HandleAccessibleAction(data);
  EXPECT_EQ(1, menu_show_count_);  // No change.

  combobox_->SetEnabled(false);
  combobox_->HandleAccessibleAction(data);
  EXPECT_EQ(1, menu_show_count_);  // No change.

  data.action = ax::mojom::Action::kShowContextMenu;
  combobox_->HandleAccessibleAction(data);
  EXPECT_EQ(1, menu_show_count_);  // No change.
}

TEST_F(ComboboxTest, NotifyOnClickWithMouse) {
  InitCombobox(nullptr);

  TestComboboxListener listener;
  combobox_->set_listener(&listener);

  combobox_->Layout();

  // Click the right side (arrow button). The menu is shown.
  const gfx::Point right_point(combobox_->x() + combobox_->width() - 1,
                         combobox_->y() + combobox_->height() / 2);

  EXPECT_EQ(0, menu_show_count_);

  // Menu is shown on mouse down.
  PerformMousePress(right_point);
  EXPECT_EQ(1, menu_show_count_);
  PerformMouseRelease(right_point);
  EXPECT_EQ(1, menu_show_count_);

  // Click the left side (text button). The click event is notified.
  const gfx::Point left_point(
      gfx::Point(combobox_->x() + 1, combobox_->y() + combobox_->height() / 2));

  PerformMousePress(left_point);
  PerformMouseRelease(left_point);

  // Both the text and the arrow may toggle the menu.
  EXPECT_EQ(2, menu_show_count_);
  EXPECT_EQ(-1, listener.perform_action_index());  // Nothing selected.
}

TEST_F(ComboboxTest, ConsumingPressKeyEvents) {
  InitCombobox(nullptr);

  EXPECT_TRUE(combobox_->OnKeyPressed(
      ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_SPACE, ui::EF_NONE)));
  EXPECT_EQ(1, menu_show_count_);

  ui::KeyEvent return_press(ui::ET_KEY_PRESSED, ui::VKEY_RETURN, ui::EF_NONE);
  if (PlatformStyle::kReturnClicksFocusedControl) {
    EXPECT_TRUE(combobox_->OnKeyPressed(return_press));
    EXPECT_EQ(2, menu_show_count_);
  } else {
    EXPECT_FALSE(combobox_->OnKeyPressed(return_press));
    EXPECT_EQ(1, menu_show_count_);
  }
}

TEST_F(ComboboxTest, ContentWidth) {
  std::vector<std::string> values;
  VectorComboboxModel model(&values);
  TestCombobox combobox(&model);
  ComboboxTestApi test_api(&combobox);

  std::string long_item = "this is the long item";
  std::string short_item = "s";

  values.resize(1);
  values[0] = long_item;
  model.ValuesChanged();

  const int long_item_width = test_api.content_size().width();

  values[0] = short_item;
  model.ValuesChanged();

  const int short_item_width = test_api.content_size().width();

  values.resize(2);
  values[0] = short_item;
  values[1] = long_item;
  model.ValuesChanged();

  // The width will fit with the longest item.
  EXPECT_EQ(long_item_width, test_api.content_size().width());
  EXPECT_LT(short_item_width, test_api.content_size().width());
}

// Test that model updates preserve the selected index, so long as it is in
// range.
TEST_F(ComboboxTest, ModelChanged) {
  InitCombobox(nullptr);

  EXPECT_EQ(0, combobox_->GetSelectedRow());
  EXPECT_EQ(10, combobox_->GetRowCount());

  combobox_->SetSelectedIndex(4);
  EXPECT_EQ(4, combobox_->GetSelectedRow());

  model_->set_item_count(5);
  EXPECT_EQ(5, combobox_->GetRowCount());
  EXPECT_EQ(4, combobox_->GetSelectedRow());  // Unchanged.

  model_->set_item_count(4);
  EXPECT_EQ(4, combobox_->GetRowCount());
  EXPECT_EQ(0, combobox_->GetSelectedRow());  // Resets.

  // Restore a non-zero selection.
  combobox_->SetSelectedIndex(2);
  EXPECT_EQ(2, combobox_->GetSelectedRow());

  // Make the selected index a separator.
  std::set<int> separators;
  separators.insert(2);
  model_->SetSeparators(separators);
  EXPECT_EQ(4, combobox_->GetRowCount());
  EXPECT_EQ(0, combobox_->GetSelectedRow());  // Resets.

  // Restore a non-zero selection.
  combobox_->SetSelectedIndex(1);
  EXPECT_EQ(1, combobox_->GetSelectedRow());

  // Test an empty model.
  model_->set_item_count(0);
  EXPECT_EQ(0, combobox_->GetRowCount());
  EXPECT_EQ(0, combobox_->GetSelectedRow());  // Resets.
}

TEST_F(ComboboxTest, TypingPrefixNotifiesListener) {
  InitCombobox(nullptr);

  TestComboboxListener listener;
  combobox_->set_listener(&listener);
  ui::TextInputClient* input_client =
      widget_->GetInputMethod()->GetTextInputClient();

  // Type the first character of the second menu item ("JELLY").
  ui::KeyEvent key_event(ui::ET_KEY_PRESSED, ui::VKEY_J, ui::DomCode::US_J, 0,
                         ui::DomKey::FromCharacter('J'), ui::EventTimeForNow());

  input_client->InsertChar(key_event);
  EXPECT_EQ(1, listener.actions_performed());
  EXPECT_EQ(1, listener.perform_action_index());

  // Type the second character of "JELLY", item shouldn't change and
  // OnPerformAction() shouldn't be re-called.
  key_event =
      ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_E, ui::DomCode::US_E, 0,
                   ui::DomKey::FromCharacter('E'), ui::EventTimeForNow());
  input_client->InsertChar(key_event);
  EXPECT_EQ(1, listener.actions_performed());
  EXPECT_EQ(1, listener.perform_action_index());

  // Clears the typed text.
  combobox_->OnBlur();
  combobox_->RequestFocus();

  // Type the first character of "PEANUT BUTTER", which should change the
  // selected index and perform an action.
  key_event =
      ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_E, ui::DomCode::US_P, 0,
                   ui::DomKey::FromCharacter('P'), ui::EventTimeForNow());
  input_client->InsertChar(key_event);
  EXPECT_EQ(2, listener.actions_performed());
  EXPECT_EQ(2, listener.perform_action_index());
}

// Test properties on the Combobox menu model.
TEST_F(ComboboxTest, MenuModel) {
  const int kSeparatorIndex = 3;
  std::set<int> separators;
  separators.insert(kSeparatorIndex);
  InitCombobox(&separators);

  ui::MenuModel* menu_model = test_api_->menu_model();

  EXPECT_EQ(TestComboboxModel::kItemCount, menu_model->GetItemCount());
  EXPECT_EQ(ui::MenuModel::TYPE_SEPARATOR,
            menu_model->GetTypeAt(kSeparatorIndex));

#if defined(OS_MACOSX)
  // Comboboxes on Mac should have checkmarks, with the selected item checked,
  EXPECT_EQ(ui::MenuModel::TYPE_CHECK, menu_model->GetTypeAt(0));
  EXPECT_EQ(ui::MenuModel::TYPE_CHECK, menu_model->GetTypeAt(1));
  EXPECT_TRUE(menu_model->IsItemCheckedAt(0));
  EXPECT_FALSE(menu_model->IsItemCheckedAt(1));

  combobox_->SetSelectedIndex(1);
  EXPECT_FALSE(menu_model->IsItemCheckedAt(0));
  EXPECT_TRUE(menu_model->IsItemCheckedAt(1));
#else
  EXPECT_EQ(ui::MenuModel::TYPE_COMMAND, menu_model->GetTypeAt(0));
  EXPECT_EQ(ui::MenuModel::TYPE_COMMAND, menu_model->GetTypeAt(1));
#endif

  EXPECT_EQ(ASCIIToUTF16("PEANUT BUTTER"), menu_model->GetLabelAt(0));
  EXPECT_EQ(ASCIIToUTF16("JELLY"), menu_model->GetLabelAt(1));

  EXPECT_TRUE(menu_model->IsVisibleAt(0));
}

}  // namespace views
