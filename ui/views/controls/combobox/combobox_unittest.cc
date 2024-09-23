// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/combobox/combobox.h"

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/models/combobox_model_observer.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/models/simple_combobox_model.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/test/ax_event_counter.h"
#include "ui/views/test/combobox_test_api.h"
#include "ui/views/test/view_metadata_test_utils.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

using base::ASCIIToUTF16;

namespace views {

using test::ComboboxTestApi;

namespace {

using TestCombobox = Combobox;

// A concrete class is needed to test the combobox.
class TestComboboxModel : public ui::ComboboxModel {
 public:
  TestComboboxModel() = default;

  TestComboboxModel(const TestComboboxModel&) = delete;
  TestComboboxModel& operator=(const TestComboboxModel&) = delete;

  ~TestComboboxModel() override = default;

  static constexpr size_t kItemCount = 10;

  // ui::ComboboxModel:
  size_t GetItemCount() const override { return item_count_; }
  std::u16string GetItemAt(size_t index) const override {
    DCHECK(!IsItemSeparatorAt(index));
    return ASCIIToUTF16(index % 2 == 0 ? "PEANUT BUTTER" : "JELLY");
  }
  bool IsItemSeparatorAt(size_t index) const override {
    return separators_.find(index) != separators_.end();
  }

  std::optional<size_t> GetDefaultIndex() const override {
    // Return the first index that is not a separator.
    for (size_t index = 0; index < kItemCount; ++index) {
      if (separators_.find(index) == separators_.end())
        return index;
    }
    NOTREACHED();
  }

  void SetSeparators(const std::set<size_t>& separators) {
    separators_ = separators;
    OnModelChanged();
  }

  void set_item_count(size_t item_count) {
    item_count_ = item_count;
    OnModelChanged();
  }

 private:
  void OnModelChanged() {
    for (auto& observer : observers())
      observer.OnComboboxModelChanged(this);
  }

  std::set<size_t> separators_;
  size_t item_count_ = kItemCount;
};

// A combobox model which refers to a vector.
class VectorComboboxModel : public ui::ComboboxModel {
 public:
  explicit VectorComboboxModel(std::vector<std::string>* values)
      : values_(values) {}

  VectorComboboxModel(const VectorComboboxModel&) = delete;
  VectorComboboxModel& operator=(const VectorComboboxModel&) = delete;

  ~VectorComboboxModel() override = default;

  void set_default_index(size_t default_index) {
    default_index_ = default_index;
  }

  // ui::ComboboxModel:
  size_t GetItemCount() const override { return values_->size(); }
  std::u16string GetItemAt(size_t index) const override {
    return ASCIIToUTF16((*values_)[index]);
  }
  bool IsItemSeparatorAt(size_t index) const override { return false; }
  std::optional<size_t> GetDefaultIndex() const override {
    return default_index_;
  }

  void ValuesChanged() {
    for (auto& observer : observers())
      observer.OnComboboxModelChanged(this);
  }

 private:
  std::optional<size_t> default_index_ = std::nullopt;
  const raw_ptr<std::vector<std::string>> values_;
};

class EvilListener {
 public:
  EvilListener() {
    combobox()->SetCallback(base::BindRepeating(&EvilListener::OnPerformAction,
                                                base::Unretained(this)));
  }

  EvilListener(const EvilListener&) = delete;
  EvilListener& operator=(const EvilListener&) = delete;

  ~EvilListener() = default;

  TestCombobox* combobox() { return combobox_.get(); }

 private:
  void OnPerformAction() { combobox_.reset(); }

  TestComboboxModel model_;
  std::unique_ptr<TestCombobox> combobox_ =
      std::make_unique<TestCombobox>(&model_);
};

class TestComboboxListener {
 public:
  explicit TestComboboxListener(Combobox* combobox) : combobox_(combobox) {}

  TestComboboxListener(const TestComboboxListener&) = delete;
  TestComboboxListener& operator=(const TestComboboxListener&) = delete;

  ~TestComboboxListener() = default;

  void OnPerformAction() {
    perform_action_index_ = combobox_->GetSelectedIndex();
    actions_performed_++;
  }

  std::optional<size_t> perform_action_index() const {
    return perform_action_index_;
  }

  bool on_perform_action_called() const { return actions_performed_ > 0; }

  int actions_performed() const { return actions_performed_; }

 private:
  raw_ptr<Combobox> combobox_;
  std::optional<size_t> perform_action_index_ = std::nullopt;
  int actions_performed_ = 0;
};

}  // namespace

class ComboboxTest : public ViewsTestBase {
 public:
  ComboboxTest() = default;

  ComboboxTest(const ComboboxTest&) = delete;
  ComboboxTest& operator=(const ComboboxTest&) = delete;

  void TearDown() override {
    widget_.reset();
    ViewsTestBase::TearDown();
  }

  void InitCombobox(const std::set<size_t>* separators) {
    model_ = std::make_unique<TestComboboxModel>();

    if (separators)
      model_->SetSeparators(*separators);

    ASSERT_FALSE(combobox());
    auto box = std::make_unique<TestCombobox>(model_.get());
    ComboboxTestApi(box.get()).InstallTestMenuRunner(&menu_show_count_);
    box->SetID(1);

    widget_ = std::make_unique<Widget>();
    Widget::InitParams params =
        CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                     Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.bounds = gfx::Rect(200, 200, 200, 200);
    widget_->Init(std::move(params));
    View* container = widget_->SetContentsView(std::make_unique<View>());
    container->AddChildView(std::move(box));
    widget_->Show();

    combobox()->RequestFocus();
    combobox()->SizeToPreferredSize();

    event_generator_ = std::make_unique<ui::test::EventGenerator>(
        GetRootWindow(widget_.get()));
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
        ui::EventType::kMousePressed, point, point, ui::EventTimeForNow(),
        ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
    widget_->OnMouseEvent(&pressed_event);
  }

  void PerformMouseRelease(const gfx::Point& point) {
    ui::MouseEvent released_event = ui::MouseEvent(
        ui::EventType::kMouseReleased, point, point, ui::EventTimeForNow(),
        ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
    widget_->OnMouseEvent(&released_event);
  }

  void PerformClick(const gfx::Point& point) {
    PerformMousePress(point);
    PerformMouseRelease(point);
  }

  TestCombobox* combobox() {
    return widget_ ? static_cast<TestCombobox*>(
                         widget_->GetContentsView()->GetViewByID(1))
                   : nullptr;
  }

  // We need widget to populate wrapper class.
  UniqueWidgetPtr widget_;

  // Combobox does not take ownership of the model, hence it needs to be scoped.
  std::unique_ptr<TestComboboxModel> model_;

  // The current menu show count.
  int menu_show_count_ = 0;

  std::unique_ptr<ui::test::EventGenerator> event_generator_;
};

#if BUILDFLAG(IS_MAC)
// Tests whether the various Mac specific keyboard shortcuts invoke the dropdown
// menu or not.
TEST_F(ComboboxTest, KeyTestMac) {
  InitCombobox(nullptr);
  PressKey(ui::VKEY_END);
  EXPECT_EQ(0u, combobox()->GetSelectedIndex());
  EXPECT_EQ(1, menu_show_count_);

  PressKey(ui::VKEY_HOME);
  EXPECT_EQ(0u, combobox()->GetSelectedIndex());
  EXPECT_EQ(2, menu_show_count_);

  PressKey(ui::VKEY_UP, ui::EF_COMMAND_DOWN);
  EXPECT_EQ(0u, combobox()->GetSelectedIndex());
  EXPECT_EQ(3, menu_show_count_);

  PressKey(ui::VKEY_DOWN, ui::EF_COMMAND_DOWN);
  EXPECT_EQ(0u, combobox()->GetSelectedIndex());
  EXPECT_EQ(4, menu_show_count_);

  PressKey(ui::VKEY_DOWN);
  EXPECT_EQ(0u, combobox()->GetSelectedIndex());
  EXPECT_EQ(5, menu_show_count_);

  PressKey(ui::VKEY_RIGHT);
  EXPECT_EQ(0u, combobox()->GetSelectedIndex());
  EXPECT_EQ(5, menu_show_count_);

  PressKey(ui::VKEY_LEFT);
  EXPECT_EQ(0u, combobox()->GetSelectedIndex());
  EXPECT_EQ(5, menu_show_count_);

  PressKey(ui::VKEY_UP);
  EXPECT_EQ(0u, combobox()->GetSelectedIndex());
  EXPECT_EQ(6, menu_show_count_);

  PressKey(ui::VKEY_PRIOR);
  EXPECT_EQ(0u, combobox()->GetSelectedIndex());
  EXPECT_EQ(6, menu_show_count_);

  PressKey(ui::VKEY_NEXT);
  EXPECT_EQ(0u, combobox()->GetSelectedIndex());
  EXPECT_EQ(6, menu_show_count_);
}
#endif

// Iterate through all the metadata and test each property.
TEST_F(ComboboxTest, MetadataTest) {
  InitCombobox(nullptr);
  test::TestViewMetadata(combobox());
}

// Check that if a combobox is disabled before it has a native wrapper, then the
// native wrapper inherits the disabled state when it gets created.
TEST_F(ComboboxTest, DisabilityTest) {
  model_ = std::make_unique<TestComboboxModel>();

  ASSERT_FALSE(combobox());
  auto combobox = std::make_unique<TestCombobox>(model_.get());
  combobox->SetEnabled(false);

  widget_ = std::make_unique<Widget>();
  Widget::InitParams params =
      CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                   Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.bounds = gfx::Rect(100, 100, 100, 100);
  widget_->Init(std::move(params));
  View* container = widget_->SetContentsView(std::make_unique<View>());
  Combobox* combobox_pointer = container->AddChildView(std::move(combobox));
  EXPECT_FALSE(combobox_pointer->GetEnabled());
}

// Ensure the border on the combobox is set correctly when Enabled state
// changes.
TEST_F(ComboboxTest, DisabledBorderTest) {
  InitCombobox(nullptr);
  ASSERT_TRUE(combobox()->GetEnabled());
  ASSERT_NE(combobox()->GetBorder(), nullptr);
  combobox()->SetEnabled(false);
  ASSERT_FALSE(combobox()->GetEnabled());
  ASSERT_EQ(combobox()->GetBorder(), nullptr);
  combobox()->SetEnabled(true);
  ASSERT_TRUE(combobox()->GetEnabled());
  ASSERT_NE(combobox()->GetBorder(), nullptr);
}

// On Mac, key events can't change the currently selected index directly for a
// combobox.
#if !BUILDFLAG(IS_MAC)

// Tests the behavior of various keyboard shortcuts on the currently selected
// index.
TEST_F(ComboboxTest, KeyTest) {
  InitCombobox(nullptr);
  PressKey(ui::VKEY_END);
  EXPECT_EQ(model_->GetItemCount() - 1, combobox()->GetSelectedIndex());
  PressKey(ui::VKEY_HOME);
  EXPECT_EQ(0u, combobox()->GetSelectedIndex());
  PressKey(ui::VKEY_DOWN);
  PressKey(ui::VKEY_DOWN);
  EXPECT_EQ(2u, combobox()->GetSelectedIndex());
  PressKey(ui::VKEY_RIGHT);
  EXPECT_EQ(2u, combobox()->GetSelectedIndex());
  PressKey(ui::VKEY_LEFT);
  EXPECT_EQ(2u, combobox()->GetSelectedIndex());
  PressKey(ui::VKEY_UP);
  EXPECT_EQ(1u, combobox()->GetSelectedIndex());
  PressKey(ui::VKEY_PRIOR);
  EXPECT_EQ(0u, combobox()->GetSelectedIndex());
  PressKey(ui::VKEY_NEXT);
  EXPECT_EQ(model_->GetItemCount() - 1, combobox()->GetSelectedIndex());
}

// Verifies that we don't select a separator line in combobox when navigating
// through keyboard.
TEST_F(ComboboxTest, SkipSeparatorSimple) {
  std::set<size_t> separators;
  separators.insert(2);
  InitCombobox(&separators);
  EXPECT_EQ(0u, combobox()->GetSelectedIndex());
  PressKey(ui::VKEY_DOWN);
  EXPECT_EQ(1u, combobox()->GetSelectedIndex());
  PressKey(ui::VKEY_DOWN);
  EXPECT_EQ(3u, combobox()->GetSelectedIndex());
  PressKey(ui::VKEY_UP);
  EXPECT_EQ(1u, combobox()->GetSelectedIndex());
  PressKey(ui::VKEY_HOME);
  EXPECT_EQ(0u, combobox()->GetSelectedIndex());
  PressKey(ui::VKEY_PRIOR);
  EXPECT_EQ(0u, combobox()->GetSelectedIndex());
  PressKey(ui::VKEY_END);
  EXPECT_EQ(9u, combobox()->GetSelectedIndex());
}

// Verifies that we never select the separator that is in the beginning of the
// combobox list when navigating through keyboard.
TEST_F(ComboboxTest, SkipSeparatorBeginning) {
  std::set<size_t> separators;
  separators.insert(0);
  InitCombobox(&separators);
  EXPECT_EQ(1u, combobox()->GetSelectedIndex());
  PressKey(ui::VKEY_DOWN);
  EXPECT_EQ(2u, combobox()->GetSelectedIndex());
  PressKey(ui::VKEY_DOWN);
  EXPECT_EQ(3u, combobox()->GetSelectedIndex());
  PressKey(ui::VKEY_UP);
  EXPECT_EQ(2u, combobox()->GetSelectedIndex());
  PressKey(ui::VKEY_HOME);
  EXPECT_EQ(1u, combobox()->GetSelectedIndex());
  PressKey(ui::VKEY_PRIOR);
  EXPECT_EQ(1u, combobox()->GetSelectedIndex());
  PressKey(ui::VKEY_END);
  EXPECT_EQ(9u, combobox()->GetSelectedIndex());
}

// Verifies that we never select the separator that is in the end of the
// combobox list when navigating through keyboard.
TEST_F(ComboboxTest, SkipSeparatorEnd) {
  std::set<size_t> separators;
  separators.insert(TestComboboxModel::kItemCount - 1);
  InitCombobox(&separators);
  combobox()->SetSelectedIndex(8);
  PressKey(ui::VKEY_DOWN);
  EXPECT_EQ(8u, combobox()->GetSelectedIndex());
  PressKey(ui::VKEY_UP);
  EXPECT_EQ(7u, combobox()->GetSelectedIndex());
  PressKey(ui::VKEY_END);
  EXPECT_EQ(8u, combobox()->GetSelectedIndex());
}

// Verifies that we never select any of the adjacent separators (multiple
// consecutive) that appear in the beginning of the combobox list when
// navigating through keyboard.
TEST_F(ComboboxTest, SkipMultipleSeparatorsAtBeginning) {
  std::set<size_t> separators;
  separators.insert(0);
  separators.insert(1);
  separators.insert(2);
  InitCombobox(&separators);
  EXPECT_EQ(3u, combobox()->GetSelectedIndex());
  PressKey(ui::VKEY_DOWN);
  EXPECT_EQ(4u, combobox()->GetSelectedIndex());
  PressKey(ui::VKEY_UP);
  EXPECT_EQ(3u, combobox()->GetSelectedIndex());
  PressKey(ui::VKEY_NEXT);
  EXPECT_EQ(9u, combobox()->GetSelectedIndex());
  PressKey(ui::VKEY_HOME);
  EXPECT_EQ(3u, combobox()->GetSelectedIndex());
  PressKey(ui::VKEY_END);
  EXPECT_EQ(9u, combobox()->GetSelectedIndex());
  PressKey(ui::VKEY_PRIOR);
  EXPECT_EQ(3u, combobox()->GetSelectedIndex());
}

// Verifies that we never select any of the adjacent separators (multiple
// consecutive) that appear in the middle of the combobox list when navigating
// through keyboard.
TEST_F(ComboboxTest, SkipMultipleAdjacentSeparatorsAtMiddle) {
  std::set<size_t> separators;
  separators.insert(4);
  separators.insert(5);
  separators.insert(6);
  InitCombobox(&separators);
  combobox()->SetSelectedIndex(3);
  PressKey(ui::VKEY_DOWN);
  EXPECT_EQ(7u, combobox()->GetSelectedIndex());
  PressKey(ui::VKEY_UP);
  EXPECT_EQ(3u, combobox()->GetSelectedIndex());
}

// Verifies that we never select any of the adjacent separators (multiple
// consecutive) that appear in the end of the combobox list when navigating
// through keyboard.
TEST_F(ComboboxTest, SkipMultipleSeparatorsAtEnd) {
  std::set<size_t> separators;
  separators.insert(7);
  separators.insert(8);
  separators.insert(9);
  InitCombobox(&separators);
  combobox()->SetSelectedIndex(6);
  PressKey(ui::VKEY_DOWN);
  EXPECT_EQ(6u, combobox()->GetSelectedIndex());
  PressKey(ui::VKEY_UP);
  EXPECT_EQ(5u, combobox()->GetSelectedIndex());
  PressKey(ui::VKEY_HOME);
  EXPECT_EQ(0u, combobox()->GetSelectedIndex());
  PressKey(ui::VKEY_NEXT);
  EXPECT_EQ(6u, combobox()->GetSelectedIndex());
  PressKey(ui::VKEY_PRIOR);
  EXPECT_EQ(0u, combobox()->GetSelectedIndex());
  PressKey(ui::VKEY_END);
  EXPECT_EQ(6u, combobox()->GetSelectedIndex());
}
#endif  // !BUILDFLAG(IS_MAC)

TEST_F(ComboboxTest, GetTextForRowTest) {
  std::set<size_t> separators;
  separators.insert(0);
  separators.insert(1);
  separators.insert(9);
  InitCombobox(&separators);
  for (size_t i = 0; i < combobox()->GetRowCount(); ++i) {
    if (separators.count(i) != 0) {
      EXPECT_TRUE(combobox()->GetTextForRow(i).empty()) << i;
    } else {
      EXPECT_EQ(ASCIIToUTF16(i % 2 == 0 ? "PEANUT BUTTER" : "JELLY"),
                combobox()->GetTextForRow(i))
          << i;
    }
  }
}

// Verifies selecting the first matching value (and returning whether found).
TEST_F(ComboboxTest, SelectValue) {
  InitCombobox(nullptr);
  ASSERT_EQ(model_->GetDefaultIndex(), combobox()->GetSelectedIndex());
  EXPECT_TRUE(combobox()->SelectValue(u"PEANUT BUTTER"));
  EXPECT_EQ(0u, combobox()->GetSelectedIndex());
  EXPECT_TRUE(combobox()->SelectValue(u"JELLY"));
  EXPECT_EQ(1u, combobox()->GetSelectedIndex());
  EXPECT_FALSE(combobox()->SelectValue(u"BANANAS"));
  EXPECT_EQ(1u, combobox()->GetSelectedIndex());
}

TEST_F(ComboboxTest, ListenerHandlesDelete) {
  auto evil_listener = std::make_unique<EvilListener>();
  ASSERT_TRUE(evil_listener->combobox());
  ASSERT_NO_FATAL_FAILURE({
    ui::MenuModel* model =
        ComboboxTestApi(evil_listener->combobox()).menu_model();
    model->ActivatedAt(2);
  });
  EXPECT_FALSE(evil_listener->combobox());
}

TEST_F(ComboboxTest, Click) {
  InitCombobox(nullptr);

  TestComboboxListener listener(combobox());
  combobox()->SetCallback(base::BindRepeating(
      &TestComboboxListener::OnPerformAction, base::Unretained(&listener)));
  views::test::RunScheduledLayout(combobox());

  // Click the left side. The menu is shown.
  EXPECT_EQ(0, menu_show_count_);
  PerformClick(gfx::Point(combobox()->x() + 1,
                          combobox()->y() + combobox()->height() / 2));
  EXPECT_FALSE(listener.on_perform_action_called());
  EXPECT_EQ(1, menu_show_count_);
}

TEST_F(ComboboxTest, ClickButDisabled) {
  InitCombobox(nullptr);

  TestComboboxListener listener(combobox());
  combobox()->SetCallback(base::BindRepeating(
      &TestComboboxListener::OnPerformAction, base::Unretained(&listener)));

  views::test::RunScheduledLayout(combobox());
  combobox()->SetEnabled(false);

  // Click the left side, but nothing happens since the combobox is disabled.
  PerformClick(gfx::Point(combobox()->x() + 1,
                          combobox()->y() + combobox()->height() / 2));
  EXPECT_FALSE(listener.on_perform_action_called());
  EXPECT_EQ(0, menu_show_count_);
}

TEST_F(ComboboxTest, NotifyOnClickWithReturnKey) {
  InitCombobox(nullptr);

  TestComboboxListener listener(combobox());
  combobox()->SetCallback(base::BindRepeating(
      &TestComboboxListener::OnPerformAction, base::Unretained(&listener)));

  // The click event is ignored. Instead the menu is shown.
  PressKey(ui::VKEY_RETURN);
  EXPECT_EQ(PlatformStyle::kReturnClicksFocusedControl ? 1 : 0,
            menu_show_count_);
  EXPECT_FALSE(listener.on_perform_action_called());
}

TEST_F(ComboboxTest, NotifyOnClickWithSpaceKey) {
  InitCombobox(nullptr);

  TestComboboxListener listener(combobox());
  combobox()->SetCallback(base::BindRepeating(
      &TestComboboxListener::OnPerformAction, base::Unretained(&listener)));

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
  combobox()->HandleAccessibleAction(data);
  EXPECT_EQ(1, menu_show_count_);

  // ax::mojom::Action::kShowContextMenu is specifically for a context menu
  // (e.g. right- click). Combobox should ignore it.
  data.action = ax::mojom::Action::kShowContextMenu;
  combobox()->HandleAccessibleAction(data);
  EXPECT_EQ(1, menu_show_count_);  // No change.

  data.action = ax::mojom::Action::kBlur;
  combobox()->HandleAccessibleAction(data);
  EXPECT_EQ(1, menu_show_count_);  // No change.

  combobox()->SetEnabled(false);
  combobox()->HandleAccessibleAction(data);
  EXPECT_EQ(1, menu_show_count_);  // No change.

  data.action = ax::mojom::Action::kShowContextMenu;
  combobox()->HandleAccessibleAction(data);
  EXPECT_EQ(1, menu_show_count_);  // No change.
}

TEST_F(ComboboxTest, ExpandedCollapsedAccessibleState) {
  InitCombobox(nullptr);

  // Initially the combobox will be collapsed by default.
  ui::AXNodeData node_data;
  combobox()->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_FALSE(node_data.HasState(ax::mojom::State::kExpanded));
  EXPECT_TRUE(node_data.HasState(ax::mojom::State::kCollapsed));

  // Pressing space shows the menu, which sets the expanded state.
  combobox()->OnKeyPressed(
      ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_SPACE, ui::EF_NONE));
  node_data = ui::AXNodeData();
  combobox()->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_TRUE(node_data.HasState(ax::mojom::State::kExpanded));
  EXPECT_FALSE(node_data.HasState(ax::mojom::State::kCollapsed));

  // Closing the menu with the test api sets the collapsed state.
  ComboboxTestApi(combobox()).CloseMenu();
  node_data = ui::AXNodeData();
  combobox()->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_FALSE(node_data.HasState(ax::mojom::State::kExpanded));
  EXPECT_TRUE(node_data.HasState(ax::mojom::State::kCollapsed));

  // Pressing space again reopens the menu and sets the expanded state.
  combobox()->OnKeyPressed(
      ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_SPACE, ui::EF_NONE));
  node_data = ui::AXNodeData();
  combobox()->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_TRUE(node_data.HasState(ax::mojom::State::kExpanded));
  EXPECT_FALSE(node_data.HasState(ax::mojom::State::kCollapsed));

  // Changing the model closes the menu and sets the collapsed state.
  model_->set_item_count(0);
  node_data = ui::AXNodeData();
  combobox()->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_FALSE(node_data.HasState(ax::mojom::State::kExpanded));
  EXPECT_TRUE(node_data.HasState(ax::mojom::State::kCollapsed));
}

TEST_F(ComboboxTest, AccessibleDefaultActionVerb) {
  InitCombobox(nullptr);
  ui::AXNodeData node_data;
  combobox()->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_EQ(ax::mojom::DefaultActionVerb::kOpen,
            node_data.GetDefaultActionVerb());

  node_data = ui::AXNodeData();
  combobox()->SetEnabled(false);
  combobox()->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_FALSE(
      node_data.HasIntAttribute(ax::mojom::IntAttribute::kDefaultActionVerb));

  node_data = ui::AXNodeData();
  combobox()->SetEnabled(true);
  combobox()->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_EQ(ax::mojom::DefaultActionVerb::kOpen,
            node_data.GetDefaultActionVerb());
}

TEST_F(ComboboxTest, SetSizePosInSetAccessibleProperties) {
  InitCombobox(nullptr);

  // Test an empty model.
  model_->set_item_count(0);
  EXPECT_EQ(0u, combobox()->GetRowCount());
  EXPECT_EQ(0u, combobox()->GetSelectedRow());
  ui::AXNodeData node_data;
  combobox()->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_EQ(0, node_data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));
  EXPECT_EQ(0, node_data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));

  // Update item count and selected index.
  model_->set_item_count(5);
  combobox()->SetSelectedIndex(4);
  EXPECT_EQ(5u, combobox()->GetRowCount());
  EXPECT_EQ(4u, combobox()->GetSelectedRow());
  node_data = ui::AXNodeData();
  combobox()->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_EQ(5, node_data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));
  EXPECT_EQ(4, node_data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));

  // Update item count.
  model_->set_item_count(6);
  EXPECT_EQ(6u, combobox()->GetRowCount());
  EXPECT_EQ(4u, combobox()->GetSelectedRow());
  node_data = ui::AXNodeData();
  combobox()->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_EQ(6, node_data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));
  EXPECT_EQ(4, node_data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));

  // Update selected index.
  combobox()->SetSelectedIndex(2);
  EXPECT_EQ(6u, combobox()->GetRowCount());
  EXPECT_EQ(2u, combobox()->GetSelectedRow());
  node_data = ui::AXNodeData();
  combobox()->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_EQ(6, node_data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));
  EXPECT_EQ(2, node_data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
}

TEST_F(ComboboxTest, AccessibleValue) {
  // Empty model kValue check
  auto simple_model = std::make_unique<ui::SimpleComboboxModel>(
      std::vector<ui::SimpleComboboxModel::Item>());
  auto combobox = std::make_unique<Combobox>(simple_model.get());

  ui::AXNodeData node_data;
  combobox->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_EQ(0u, combobox->GetModel()->GetItemCount());
  EXPECT_EQ(std::nullopt, combobox->GetSelectedIndex());
  EXPECT_EQ("",
            node_data.GetStringAttribute(ax::mojom::StringAttribute::kValue));

  // Non-empty model.
  simple_model->UpdateItemList({ui::SimpleComboboxModel::Item(u"Peanut Butter"),
                                ui::SimpleComboboxModel::Item(u"Yogurt")});
  node_data = ui::AXNodeData();
  combobox->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_EQ(2u, combobox->GetModel()->GetItemCount());
  EXPECT_EQ(0u, combobox->GetSelectedIndex());
  EXPECT_EQ("Peanut Butter",
            node_data.GetStringAttribute(ax::mojom::StringAttribute::kValue));

  // set selected index to 1.
  node_data = ui::AXNodeData();
  combobox->SetSelectedIndex(1);
  EXPECT_EQ(1u, combobox->GetSelectedIndex());
  combobox->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_EQ("Yogurt",
            node_data.GetStringAttribute(ax::mojom::StringAttribute::kValue));
}

TEST_F(ComboboxTest, NotifyOnClickWithMouse) {
  InitCombobox(nullptr);

  TestComboboxListener listener(combobox());
  combobox()->SetCallback(base::BindRepeating(
      &TestComboboxListener::OnPerformAction, base::Unretained(&listener)));

  views::test::RunScheduledLayout(combobox());

  // Click the right side (arrow button). The menu is shown.
  const gfx::Point right_point(combobox()->x() + combobox()->width() - 1,
                               combobox()->y() + combobox()->height() / 2);

  EXPECT_EQ(0, menu_show_count_);

  // Menu is shown on mouse down.
  PerformMousePress(right_point);
  EXPECT_EQ(1, menu_show_count_);
  PerformMouseRelease(right_point);
  EXPECT_EQ(1, menu_show_count_);

  // Click the left side (text button). The click event is notified.
  const gfx::Point left_point(gfx::Point(
      combobox()->x() + 1, combobox()->y() + combobox()->height() / 2));

  PerformMousePress(left_point);
  PerformMouseRelease(left_point);

  // Both the text and the arrow may toggle the menu.
  EXPECT_EQ(2, menu_show_count_);
  EXPECT_FALSE(listener.perform_action_index().has_value());
}

TEST_F(ComboboxTest, ConsumingPressKeyEvents) {
  InitCombobox(nullptr);

  EXPECT_TRUE(combobox()->OnKeyPressed(
      ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_SPACE, ui::EF_NONE)));
  EXPECT_EQ(1, menu_show_count_);

  ui::KeyEvent return_press(ui::EventType::kKeyPressed, ui::VKEY_RETURN,
                            ui::EF_NONE);
  if (PlatformStyle::kReturnClicksFocusedControl) {
    EXPECT_TRUE(combobox()->OnKeyPressed(return_press));
    EXPECT_EQ(2, menu_show_count_);
  } else {
    EXPECT_FALSE(combobox()->OnKeyPressed(return_press));
    EXPECT_EQ(1, menu_show_count_);
  }
}

// Test that ensures that the combobox is resized correctly when selecting
// between indices of different label lengths.
TEST_F(ComboboxTest, ContentSizeUpdateOnSetSelectedIndex) {
  const gfx::FontList& font_list =
      TypographyProvider::Get().GetFont(Combobox::kContext, Combobox::kStyle);
  InitCombobox(nullptr);
  combobox()->SetSizeToLargestLabel(false);
  ComboboxTestApi(combobox()).PerformActionAt(1);
  EXPECT_EQ(gfx::GetStringWidth(model_->GetItemAt(1), font_list),
            ComboboxTestApi(combobox()).content_size().width());
  combobox()->SetSelectedIndex(1);
  EXPECT_EQ(gfx::GetStringWidth(model_->GetItemAt(1), font_list),
            ComboboxTestApi(combobox()).content_size().width());

  // Avoid selected_index_ == index optimization and start with index 1 selected
  // to test resizing from a an index with a shorter label to an index with a
  // longer label.
  combobox()->SetSelectedIndex(0);
  combobox()->SetSelectedIndex(1);

  ComboboxTestApi(combobox()).PerformActionAt(0);
  EXPECT_EQ(gfx::GetStringWidth(model_->GetItemAt(0), font_list),
            ComboboxTestApi(combobox()).content_size().width());
  combobox()->SetSelectedIndex(0);
  EXPECT_EQ(gfx::GetStringWidth(model_->GetItemAt(0), font_list),
            ComboboxTestApi(combobox()).content_size().width());
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

  EXPECT_EQ(0u, combobox()->GetSelectedRow());
  EXPECT_EQ(10u, combobox()->GetRowCount());

  combobox()->SetSelectedIndex(4);
  EXPECT_EQ(4u, combobox()->GetSelectedRow());

  model_->set_item_count(5);
  EXPECT_EQ(5u, combobox()->GetRowCount());
  EXPECT_EQ(4u, combobox()->GetSelectedRow());  // Unchanged.

  model_->set_item_count(4);
  EXPECT_EQ(4u, combobox()->GetRowCount());
  EXPECT_EQ(0u, combobox()->GetSelectedRow());  // Resets.

  // Restore a non-zero selection.
  combobox()->SetSelectedIndex(2);
  EXPECT_EQ(2u, combobox()->GetSelectedRow());

  // Make the selected index a separator.
  std::set<size_t> separators;
  separators.insert(2);
  model_->SetSeparators(separators);
  EXPECT_EQ(4u, combobox()->GetRowCount());
  EXPECT_EQ(0u, combobox()->GetSelectedRow());  // Resets.

  // Restore a non-zero selection.
  combobox()->SetSelectedIndex(1);
  EXPECT_EQ(1u, combobox()->GetSelectedRow());

  // Test an empty model.
  model_->set_item_count(0);
  EXPECT_EQ(0u, combobox()->GetRowCount());
  EXPECT_EQ(0u, combobox()->GetSelectedRow());  // Resets.
}

TEST_F(ComboboxTest, TypingPrefixNotifiesListener) {
  InitCombobox(nullptr);

  TestComboboxListener listener(combobox());
  combobox()->SetCallback(base::BindRepeating(
      &TestComboboxListener::OnPerformAction, base::Unretained(&listener)));
  ui::TextInputClient* input_client =
      widget_->GetInputMethod()->GetTextInputClient();

  // Type the first character of the second menu item ("JELLY").
  ui::KeyEvent key_event(ui::EventType::kKeyPressed, ui::VKEY_J,
                         ui::DomCode::US_J, 0, ui::DomKey::FromCharacter('J'),
                         ui::EventTimeForNow());

  input_client->InsertChar(key_event);
  EXPECT_EQ(1, listener.actions_performed());
  EXPECT_EQ(1u, listener.perform_action_index());

  // Type the second character of "JELLY", item shouldn't change and
  // OnPerformAction() shouldn't be re-called.
  key_event =
      ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_E, ui::DomCode::US_E, 0,
                   ui::DomKey::FromCharacter('E'), ui::EventTimeForNow());
  input_client->InsertChar(key_event);
  EXPECT_EQ(1, listener.actions_performed());
  EXPECT_EQ(1u, listener.perform_action_index());

  // Clears the typed text.
  combobox()->OnBlur();
  combobox()->RequestFocus();

  // Type the first character of "PEANUT BUTTER", which should change the
  // selected index and perform an action.
  key_event =
      ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_E, ui::DomCode::US_P, 0,
                   ui::DomKey::FromCharacter('P'), ui::EventTimeForNow());
  input_client->InsertChar(key_event);
  EXPECT_EQ(2, listener.actions_performed());
  EXPECT_EQ(2u, listener.perform_action_index());
}

// Test properties on the Combobox menu model.
TEST_F(ComboboxTest, MenuModel) {
  const int kSeparatorIndex = 3;
  std::set<size_t> separators;
  separators.insert(kSeparatorIndex);
  InitCombobox(&separators);

  ui::MenuModel* menu_model = ComboboxTestApi(combobox()).menu_model();

  EXPECT_EQ(TestComboboxModel::kItemCount, menu_model->GetItemCount());
  EXPECT_EQ(ui::MenuModel::TYPE_SEPARATOR,
            menu_model->GetTypeAt(kSeparatorIndex));

#if BUILDFLAG(IS_MAC)
  // Comboboxes on Mac should have checkmarks, with the selected item checked,
  EXPECT_EQ(ui::MenuModel::TYPE_CHECK, menu_model->GetTypeAt(0));
  EXPECT_EQ(ui::MenuModel::TYPE_CHECK, menu_model->GetTypeAt(1));
  EXPECT_TRUE(menu_model->IsItemCheckedAt(0));
  EXPECT_FALSE(menu_model->IsItemCheckedAt(1));

  combobox()->SetSelectedIndex(1);
  EXPECT_FALSE(menu_model->IsItemCheckedAt(0));
  EXPECT_TRUE(menu_model->IsItemCheckedAt(1));
#else
  EXPECT_EQ(ui::MenuModel::TYPE_COMMAND, menu_model->GetTypeAt(0));
  EXPECT_EQ(ui::MenuModel::TYPE_COMMAND, menu_model->GetTypeAt(1));
#endif

  EXPECT_EQ(u"PEANUT BUTTER", menu_model->GetLabelAt(0));
  EXPECT_EQ(u"JELLY", menu_model->GetLabelAt(1));

  EXPECT_TRUE(menu_model->IsVisibleAt(0));
}

// Verifies SetTooltipTextAndAccessibleName will call NotifyAccessibilityEvent.
TEST_F(ComboboxTest, SetTooltipTextNotifiesAccessibilityEvent) {
  test::AXEventCounter counter(AXEventManager::Get());
  InitCombobox(nullptr);
  std::u16string test_tooltip_text = u"Test Tooltip Text";
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged));

  // `SetTooltipTextAndAccessibleName` does two things:
  // 1. sets the tooltip text on the arrow button. `Button::SetTooltipText`
  //    fires a text-changed event.
  // 2. if the accessible name is empty, calls
  // `View::GetViewAccessibility().SetName`
  //    on the combobox. `GetViewAccessibility().SetName` fires a
  //    text-changed event.
  combobox()->SetTooltipTextAndAccessibleName(test_tooltip_text);
  EXPECT_EQ(test_tooltip_text, combobox()->GetTooltipTextAndAccessibleName());
  EXPECT_EQ(1, counter.GetCount(ax::mojom::Event::kTextChanged,
                                ax::mojom::Role::kButton));
  EXPECT_EQ(1, counter.GetCount(ax::mojom::Event::kTextChanged,
                                ax::mojom::Role::kComboBoxSelect));
  EXPECT_EQ(test_tooltip_text,
            combobox()->GetViewAccessibility().GetCachedName());
  ui::AXNodeData data;
  combobox()->GetViewAccessibility().GetAccessibleNodeData(&data);
  const std::string& name =
      data.GetStringAttribute(ax::mojom::StringAttribute::kName);
  EXPECT_EQ(test_tooltip_text, ASCIIToUTF16(name));
  EXPECT_EQ(u"PEANUT BUTTER",
            data.GetString16Attribute(ax::mojom::StringAttribute::kValue));
}

// Regression test for crbug.com/1264288.
// Should fail in ASan build before the fix.
TEST_F(ComboboxTest, NoCrashWhenComboboxOutlivesModel) {
  auto model = std::make_unique<TestComboboxModel>();
  auto combobox = std::make_unique<TestCombobox>(model.get());
  model.reset();
  combobox.reset();
}

namespace {

std::string GetComboboxA11yValue(Combobox* combobox) {
  const std::optional<size_t>& selected_index = combobox->GetSelectedIndex();
  return selected_index ? base::UTF16ToUTF8(combobox->GetModel()->GetItemAt(
                              selected_index.value()))
                        : std::string();
}

using ComboboxDefaultTest = ViewsTestBase;

class ConfigurableComboboxModel final : public ui::ComboboxModel {
 public:
  explicit ConfigurableComboboxModel(bool* destroyed = nullptr)
      : destroyed_(destroyed) {
    if (destroyed_)
      *destroyed_ = false;
  }
  ConfigurableComboboxModel(ConfigurableComboboxModel&) = delete;
  ConfigurableComboboxModel& operator=(const ConfigurableComboboxModel&) =
      delete;
  ~ConfigurableComboboxModel() override {
    if (destroyed_)
      *destroyed_ = true;
  }

  // ui::ComboboxModel:
  size_t GetItemCount() const override { return item_count_; }
  std::u16string GetItemAt(size_t index) const override {
    DCHECK_LT(index, item_count_);
    return base::NumberToString16(index);
  }
  std::optional<size_t> GetDefaultIndex() const override {
    return default_index_;
  }

  void SetItemCount(size_t item_count) { item_count_ = item_count; }

  void SetDefaultIndex(size_t default_index) { default_index_ = default_index; }

 private:
  const raw_ptr<bool> destroyed_;
  size_t item_count_ = 0;
  std::optional<size_t> default_index_;
};

}  // namespace

TEST_F(ComboboxDefaultTest, Default) {
  auto combobox = std::make_unique<Combobox>();
  EXPECT_EQ(0u, combobox->GetRowCount());
  EXPECT_FALSE(combobox->GetSelectedRow().has_value());
}

TEST_F(ComboboxDefaultTest, SetModel) {
  bool destroyed = false;
  std::unique_ptr<ConfigurableComboboxModel> model =
      std::make_unique<ConfigurableComboboxModel>(&destroyed);
  model->SetItemCount(42);
  model->SetDefaultIndex(27);
  {
    auto combobox = std::make_unique<Combobox>();
    combobox->SetModel(model.get());
    EXPECT_EQ(42u, combobox->GetRowCount());
    EXPECT_EQ(27u, combobox->GetSelectedRow());
  }
  EXPECT_FALSE(destroyed);
}

TEST_F(ComboboxDefaultTest, SetOwnedModel) {
  bool destroyed = false;
  std::unique_ptr<ConfigurableComboboxModel> model =
      std::make_unique<ConfigurableComboboxModel>(&destroyed);
  model->SetItemCount(42);
  model->SetDefaultIndex(27);
  {
    auto combobox = std::make_unique<Combobox>();
    combobox->SetOwnedModel(std::move(model));
    EXPECT_EQ(42u, combobox->GetRowCount());
    EXPECT_EQ(27u, combobox->GetSelectedRow());
  }
  EXPECT_TRUE(destroyed);
}

TEST_F(ComboboxDefaultTest, SetModelOverwriteOwned) {
  bool destroyed = false;
  std::unique_ptr<ConfigurableComboboxModel> model =
      std::make_unique<ConfigurableComboboxModel>(&destroyed);
  auto combobox = std::make_unique<Combobox>();
  combobox->SetModel(model.get());
  ASSERT_FALSE(destroyed);
  combobox->SetOwnedModel(std::make_unique<ConfigurableComboboxModel>());
  EXPECT_FALSE(destroyed);
}

TEST_F(ComboboxDefaultTest, SetOwnedModelOverwriteOwned) {
  bool destroyed_first = false;
  bool destroyed_second = false;
  {
    auto combobox = std::make_unique<Combobox>();
    combobox->SetOwnedModel(
        std::make_unique<ConfigurableComboboxModel>(&destroyed_first));
    ASSERT_FALSE(destroyed_first);
    combobox->SetOwnedModel(
        std::make_unique<ConfigurableComboboxModel>(&destroyed_second));
    EXPECT_TRUE(destroyed_first);
    ASSERT_FALSE(destroyed_second);
  }
  EXPECT_TRUE(destroyed_second);
}

TEST_F(ComboboxDefaultTest, InteractionWithEmptyModel) {
  ui::AXNodeData node_data;

  // Empty model.
  // Verify `GetAccessibleNodeData()` doesn't crash when interacting with empty
  // model.
  auto simple_model = std::make_unique<ui::SimpleComboboxModel>(
      std::vector<ui::SimpleComboboxModel::Item>());
  auto combobox = std::make_unique<Combobox>(simple_model.get());

  IgnoreMissingWidgetForTestingScopedSetter ignore_missing_widget(
      combobox->GetViewAccessibility());

  combobox->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_EQ(0u, combobox->GetModel()->GetItemCount());
  EXPECT_EQ(std::nullopt, combobox->GetSelectedIndex());
  EXPECT_EQ(GetComboboxA11yValue(combobox.get()),
            node_data.GetStringAttribute(ax::mojom::StringAttribute::kValue));

  // Non-empty model.
  node_data = ui::AXNodeData();
  simple_model->UpdateItemList({ui::SimpleComboboxModel::Item(u"item")});
  combobox->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_EQ(1u, combobox->GetModel()->GetItemCount());
  EXPECT_EQ(0u, combobox->GetSelectedIndex());
  EXPECT_EQ(GetComboboxA11yValue(combobox.get()),
            node_data.GetStringAttribute(ax::mojom::StringAttribute::kValue));

  // Empty model.
  // Verify `OnComboboxModelChanged()` doesn't crash when interacting with empty
  // model.
  node_data = ui::AXNodeData();
  simple_model->UpdateItemList({});
  combobox->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_EQ(0u, combobox->GetModel()->GetItemCount());
  EXPECT_EQ(std::nullopt, combobox->GetSelectedIndex());
  EXPECT_EQ(GetComboboxA11yValue(combobox.get()),
            node_data.GetStringAttribute(ax::mojom::StringAttribute::kValue));
}

}  // namespace views
