// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "ui/base/models/combobox_model.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/test/focus_manager_test.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"

using base::ASCIIToUTF16;

namespace views {

namespace {

enum {
  TOP_CHECKBOX_ID = 1,  // 1
  LEFT_CONTAINER_ID,
  APPLE_LABEL_ID,
  APPLE_TEXTFIELD_ID,
  ORANGE_LABEL_ID,  // 5
  ORANGE_TEXTFIELD_ID,
  BANANA_LABEL_ID,
  BANANA_TEXTFIELD_ID,
  KIWI_LABEL_ID,
  KIWI_TEXTFIELD_ID,  // 10
  FRUIT_BUTTON_ID,
  FRUIT_CHECKBOX_ID,
  COMBOBOX_ID,

  RIGHT_CONTAINER_ID,
  ASPARAGUS_BUTTON_ID,  // 15
  BROCCOLI_BUTTON_ID,
  CAULIFLOWER_BUTTON_ID,

  INNER_CONTAINER_ID,
  SCROLL_VIEW_ID,
  ROSETTA_LINK_ID,  // 20
  STUPEUR_ET_TREMBLEMENT_LINK_ID,
  DINER_GAME_LINK_ID,
  RIDICULE_LINK_ID,
  CLOSET_LINK_ID,
  VISITING_LINK_ID,  // 25
  AMELIE_LINK_ID,
  JOYEUX_NOEL_LINK_ID,
  CAMPING_LINK_ID,
  BRICE_DE_NICE_LINK_ID,
  TAXI_LINK_ID,  // 30
  ASTERIX_LINK_ID,

  OK_BUTTON_ID,
  CANCEL_BUTTON_ID,
  HELP_BUTTON_ID,

  STYLE_CONTAINER_ID,  // 35
  BOLD_CHECKBOX_ID,
  ITALIC_CHECKBOX_ID,
  UNDERLINED_CHECKBOX_ID,
  STYLE_HELP_LINK_ID,
  STYLE_TEXT_EDIT_ID,  // 40

  SEARCH_CONTAINER_ID,
  SEARCH_TEXTFIELD_ID,
  SEARCH_BUTTON_ID,
  HELP_LINK_ID,

  THUMBNAIL_CONTAINER_ID,  // 45
  THUMBNAIL_STAR_ID,
  THUMBNAIL_SUPER_STAR_ID,
};

class DummyComboboxModel : public ui::ComboboxModel {
 public:
  // Overridden from ui::ComboboxModel:
  int GetItemCount() const override { return 10; }
  base::string16 GetItemAt(int index) override {
    return ASCIIToUTF16("Item ") + base::NumberToString16(index);
  }
};

// A View that can act as a pane.
class PaneView : public View, public FocusTraversable {
 public:
  PaneView() = default;

  // If this method is called, this view will use GetPaneFocusTraversable to
  // have this provided FocusSearch used instead of the default one, allowing
  // you to trap focus within the pane.
  void EnablePaneFocus(FocusSearch* focus_search) {
    focus_search_ = focus_search;
  }

  // Overridden from View:
  FocusTraversable* GetPaneFocusTraversable() override {
    if (focus_search_)
      return this;
    else
      return nullptr;
  }

  // Overridden from FocusTraversable:
  views::FocusSearch* GetFocusSearch() override { return focus_search_; }
  FocusTraversable* GetFocusTraversableParent() override { return nullptr; }
  View* GetFocusTraversableParentView() override { return nullptr; }

 private:
  FocusSearch* focus_search_ = nullptr;
};

// BorderView is a view containing a native window with its own view hierarchy.
// It is interesting to test focus traversal from a view hierarchy to an inner
// view hierarchy.
class BorderView : public NativeViewHost {
 public:
  explicit BorderView(View* child) : child_(child) {
    DCHECK(child);
    SetFocusBehavior(FocusBehavior::NEVER);
  }

  virtual internal::RootView* GetContentsRootView() {
    return static_cast<internal::RootView*>(widget_->GetRootView());
  }

  FocusTraversable* GetFocusTraversable() override {
    return static_cast<internal::RootView*>(widget_->GetRootView());
  }

  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override {
    NativeViewHost::ViewHierarchyChanged(details);

    if (details.child == this && details.is_add) {
      if (!widget_) {
        widget_ = std::make_unique<Widget>();
        Widget::InitParams params(Widget::InitParams::TYPE_CONTROL);
        params.parent = details.parent->GetWidget()->GetNativeView();
        params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
        widget_->Init(std::move(params));
        widget_->SetFocusTraversableParentView(this);
        widget_->SetContentsView(child_);
      }

      // We have been added to a view hierarchy, attach the native view.
      Attach(widget_->GetNativeView());
      // Also update the FocusTraversable parent so the focus traversal works.
      static_cast<internal::RootView*>(widget_->GetRootView())->
          SetFocusTraversableParent(GetWidget()->GetFocusTraversable());
    }
  }

 private:
  View* child_;
  std::unique_ptr<Widget> widget_;

  DISALLOW_COPY_AND_ASSIGN(BorderView);
};

}  // namespace

class FocusTraversalTest : public FocusManagerTest {
 public:
  ~FocusTraversalTest() override;

  void InitContentView() override;

 protected:
  FocusTraversalTest();

  View* FindViewByID(int id) {
    View* view = GetContentsView()->GetViewByID(id);
    if (view)
      return view;
    if (style_tab_)
      view = style_tab_->GetSelectedTabContentView()->GetViewByID(id);
    if (view)
      return view;
    view = search_border_view_->GetContentsRootView()->GetViewByID(id);
    if (view)
      return view;
    return nullptr;
  }

 protected:
  // Helper function to advance focus multiple times in a loop. |traversal_ids|
  // is an array of view ids of length |N|. |reverse| denotes the direction in
  // which focus should be advanced.
  template <size_t N>
  void AdvanceEntireFocusLoop(const int (&traversal_ids)[N], bool reverse) {
    for (size_t i = 0; i < 3; ++i) {
      for (size_t j = 0; j < N; j++) {
        SCOPED_TRACE(testing::Message() << "reverse:" << reverse << " i:" << i
                                        << " j:" << j);
        GetFocusManager()->AdvanceFocus(reverse);
        View* focused_view = GetFocusManager()->GetFocusedView();
        EXPECT_NE(nullptr, focused_view);
        if (focused_view)
          EXPECT_EQ(traversal_ids[reverse ? N - j - 1 : j],
                    focused_view->GetID());
      }
    }
  }

  TabbedPane* style_tab_ = nullptr;
  BorderView* search_border_view_ = nullptr;
  DummyComboboxModel combobox_model_;
  PaneView* left_container_;
  PaneView* right_container_;

  DISALLOW_COPY_AND_ASSIGN(FocusTraversalTest);
};

FocusTraversalTest::FocusTraversalTest() = default;

FocusTraversalTest::~FocusTraversalTest() = default;

void FocusTraversalTest::InitContentView() {
  // Create a complicated view hierarchy with lots of control types for
  // use by all of the focus traversal tests.
  //
  // Class name, ID, and asterisk next to focusable views:
  //
  // View
  //   Checkbox            * TOP_CHECKBOX_ID
  //   PaneView              LEFT_CONTAINER_ID
  //     Label               APPLE_LABEL_ID
  //     Textfield         * APPLE_TEXTFIELD_ID
  //     Label               ORANGE_LABEL_ID
  //     Textfield         * ORANGE_TEXTFIELD_ID
  //     Label               BANANA_LABEL_ID
  //     Textfield         * BANANA_TEXTFIELD_ID
  //     Label               KIWI_LABEL_ID
  //     Textfield         * KIWI_TEXTFIELD_ID
  //     NativeButton      * FRUIT_BUTTON_ID
  //     Checkbox          * FRUIT_CHECKBOX_ID
  //     Combobox          * COMBOBOX_ID
  //   PaneView              RIGHT_CONTAINER_ID
  //     RadioButton       * ASPARAGUS_BUTTON_ID
  //     RadioButton       * BROCCOLI_BUTTON_ID
  //     RadioButton       * CAULIFLOWER_BUTTON_ID
  //     View                INNER_CONTAINER_ID
  //       ScrollView        SCROLL_VIEW_ID
  //         View
  //           Link        * ROSETTA_LINK_ID
  //           Link        * STUPEUR_ET_TREMBLEMENT_LINK_ID
  //           Link        * DINER_GAME_LINK_ID
  //           Link        * RIDICULE_LINK_ID
  //           Link        * CLOSET_LINK_ID
  //           Link        * VISITING_LINK_ID
  //           Link        * AMELIE_LINK_ID
  //           Link        * JOYEUX_NOEL_LINK_ID
  //           Link        * CAMPING_LINK_ID
  //           Link        * BRICE_DE_NICE_LINK_ID
  //           Link        * TAXI_LINK_ID
  //           Link        * ASTERIX_LINK_ID
  //   NativeButton        * OK_BUTTON_ID
  //   NativeButton        * CANCEL_BUTTON_ID
  //   NativeButton        * HELP_BUTTON_ID
  //   TabbedPane          * STYLE_CONTAINER_ID
  //     TabStrip
  //       Tab ("Style")
  //       Tab ("Other")
  //     View
  //       View
  //         Checkbox      * BOLD_CHECKBOX_ID
  //         Checkbox      * ITALIC_CHECKBOX_ID
  //         Checkbox      * UNDERLINED_CHECKBOX_ID
  //         Link          * STYLE_HELP_LINK_ID
  //         Textfield     * STYLE_TEXT_EDIT_ID
  //       View
  //   BorderView            SEARCH_CONTAINER_ID
  //     View
  //       Textfield       * SEARCH_TEXTFIELD_ID
  //       NativeButton    * SEARCH_BUTTON_ID
  //       Link            * HELP_LINK_ID
  //   View                * THUMBNAIL_CONTAINER_ID
  //     NativeButton      * THUMBNAIL_STAR_ID
  //     NativeButton      * THUMBNAIL_SUPER_STAR_ID

  GetContentsView()->SetBackground(CreateSolidBackground(SK_ColorWHITE));

  auto cb = std::make_unique<Checkbox>(ASCIIToUTF16("This is a checkbox"));
  auto* cb_ptr = GetContentsView()->AddChildView(std::move(cb));
  // In this fast paced world, who really has time for non hard-coded layout?
  cb_ptr->SetBounds(10, 10, 200, 20);
  cb_ptr->SetID(TOP_CHECKBOX_ID);

  auto container = std::make_unique<PaneView>();
  container->SetBorder(CreateSolidBorder(1, SK_ColorBLACK));
  container->SetBackground(CreateSolidBackground(SkColorSetRGB(240, 240, 240)));
  container->SetID(LEFT_CONTAINER_ID);
  left_container_ = GetContentsView()->AddChildView(std::move(container));
  left_container_->SetBounds(10, 35, 250, 200);

  int label_x = 5;
  int label_width = 50;
  int label_height = 15;
  int text_field_width = 150;
  int y = 10;
  int gap_between_labels = 10;

  auto label = std::make_unique<Label>(ASCIIToUTF16("Apple:"));
  label->SetID(APPLE_LABEL_ID);
  auto* label_ptr = left_container_->AddChildView(std::move(label));
  label_ptr->SetBounds(label_x, y, label_width, label_height);

  auto text_field = std::make_unique<Textfield>();
  text_field->SetID(APPLE_TEXTFIELD_ID);
  auto* text_field_ptr = left_container_->AddChildView(std::move(text_field));
  text_field_ptr->SetBounds(label_x + label_width + 5, y, text_field_width,
                            label_height);

  y += label_height + gap_between_labels;

  label = std::make_unique<Label>(ASCIIToUTF16("Orange:"));
  label->SetID(ORANGE_LABEL_ID);
  label_ptr = left_container_->AddChildView(std::move(label));
  label_ptr->SetBounds(label_x, y, label_width, label_height);

  text_field = std::make_unique<Textfield>();
  text_field->SetID(ORANGE_TEXTFIELD_ID);
  text_field_ptr = left_container_->AddChildView(std::move(text_field));
  text_field_ptr->SetBounds(label_x + label_width + 5, y, text_field_width,
                            label_height);

  y += label_height + gap_between_labels;

  label = std::make_unique<Label>(ASCIIToUTF16("Banana:"));
  label->SetID(BANANA_LABEL_ID);
  label_ptr = left_container_->AddChildView(std::move(label));
  label_ptr->SetBounds(label_x, y, label_width, label_height);

  text_field = std::make_unique<Textfield>();
  text_field->SetID(BANANA_TEXTFIELD_ID);
  text_field_ptr = left_container_->AddChildView(std::move(text_field));
  text_field_ptr->SetBounds(label_x + label_width + 5, y, text_field_width,
                            label_height);

  y += label_height + gap_between_labels;

  label = std::make_unique<Label>(ASCIIToUTF16("Kiwi:"));
  label->SetID(KIWI_LABEL_ID);
  label_ptr = left_container_->AddChildView(std::move(label));
  label_ptr->SetBounds(label_x, y, label_width, label_height);

  text_field = std::make_unique<Textfield>();
  text_field->SetID(KIWI_TEXTFIELD_ID);
  text_field_ptr = left_container_->AddChildView(std::move(text_field));
  text_field_ptr->SetBounds(label_x + label_width + 5, y, text_field_width,
                            label_height);

  y += label_height + gap_between_labels;

  auto button = MdTextButton::Create(nullptr, ASCIIToUTF16("Click me"));
  button->SetBounds(label_x, y + 10, 80, 30);
  button->SetID(FRUIT_BUTTON_ID);
  left_container_->AddChildView(std::move(button));
  y += 40;

  cb = std::make_unique<Checkbox>(ASCIIToUTF16("This is another check box"));
  cb->SetBounds(label_x + label_width + 5, y, 180, 20);
  cb->SetID(FRUIT_CHECKBOX_ID);
  left_container_->AddChildView(std::move(cb));
  y += 20;

  auto combobox = std::make_unique<Combobox>(&combobox_model_);
  combobox->SetBounds(label_x + label_width + 5, y, 150, 30);
  combobox->SetID(COMBOBOX_ID);
  left_container_->AddChildView(std::move(combobox));

  container = std::make_unique<PaneView>();
  container->SetBorder(CreateSolidBorder(1, SK_ColorBLACK));
  container->SetBackground(CreateSolidBackground(SkColorSetRGB(240, 240, 240)));
  container->SetID(RIGHT_CONTAINER_ID);
  right_container_ = GetContentsView()->AddChildView(std::move(container));
  right_container_->SetBounds(270, 35, 300, 200);

  y = 10;
  int radio_button_height = 18;
  int gap_between_radio_buttons = 10;
  auto radio_button =
      std::make_unique<RadioButton>(ASCIIToUTF16("Asparagus"), 1);
  radio_button->SetID(ASPARAGUS_BUTTON_ID);
  auto* radio_button_ptr =
      right_container_->AddChildView(std::move(radio_button));
  radio_button_ptr->SetBounds(5, y, 70, radio_button_height);
  radio_button_ptr->SetGroup(1);
  y += radio_button_height + gap_between_radio_buttons;
  radio_button = std::make_unique<RadioButton>(ASCIIToUTF16("Broccoli"), 1);
  radio_button->SetID(BROCCOLI_BUTTON_ID);
  radio_button_ptr = right_container_->AddChildView(std::move(radio_button));
  radio_button_ptr->SetBounds(5, y, 70, radio_button_height);
  radio_button_ptr->SetGroup(1);
  RadioButton* radio_button_to_check = radio_button_ptr;
  y += radio_button_height + gap_between_radio_buttons;
  radio_button = std::make_unique<RadioButton>(ASCIIToUTF16("Cauliflower"), 1);
  radio_button->SetID(CAULIFLOWER_BUTTON_ID);
  radio_button_ptr = right_container_->AddChildView(std::move(radio_button));
  radio_button_ptr->SetBounds(5, y, 70, radio_button_height);
  radio_button_ptr->SetGroup(1);
  y += radio_button_height + gap_between_radio_buttons;

  auto inner_container = std::make_unique<View>();
  inner_container->SetBorder(CreateSolidBorder(1, SK_ColorBLACK));
  inner_container->SetBackground(
      CreateSolidBackground(SkColorSetRGB(230, 230, 230)));
  inner_container->SetID(INNER_CONTAINER_ID);
  auto* inner_container_ptr =
      right_container_->AddChildView(std::move(inner_container));
  inner_container_ptr->SetBounds(100, 10, 150, 180);

  auto scroll_view = std::make_unique<ScrollView>();
  scroll_view->SetID(SCROLL_VIEW_ID);
  auto* scroll_view_ptr =
      inner_container_ptr->AddChildView(std::move(scroll_view));
  scroll_view_ptr->SetBounds(1, 1, 148, 178);

  auto scroll_content = std::make_unique<View>();
  scroll_content->SetBounds(0, 0, 200, 200);
  scroll_content->SetBackground(
      CreateSolidBackground(SkColorSetRGB(200, 200, 200)));
  auto* scroll_content_ptr =
      scroll_view_ptr->SetContents(std::move(scroll_content));

  static const char* const kTitles[] = {
      "Rosetta", "Stupeur et tremblement", "The diner game",
      "Ridicule", "Le placard", "Les Visiteurs", "Amelie",
      "Joyeux Noel", "Camping", "Brice de Nice",
      "Taxi", "Asterix"
  };

  static const int kIDs[] = {ROSETTA_LINK_ID,    STUPEUR_ET_TREMBLEMENT_LINK_ID,
                             DINER_GAME_LINK_ID, RIDICULE_LINK_ID,
                             CLOSET_LINK_ID,     VISITING_LINK_ID,
                             AMELIE_LINK_ID,     JOYEUX_NOEL_LINK_ID,
                             CAMPING_LINK_ID,    BRICE_DE_NICE_LINK_ID,
                             TAXI_LINK_ID,       ASTERIX_LINK_ID};

  DCHECK(base::size(kTitles) == base::size(kIDs));

  y = 5;
  for (size_t i = 0; i < base::size(kTitles); ++i) {
    auto link = std::make_unique<Link>(ASCIIToUTF16(kTitles[i]));
    link->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    link->SetID(kIDs[i]);
    auto* link_ptr = scroll_content_ptr->AddChildView(std::move(link));
    link_ptr->SetBounds(5, y, 300, 15);
    y += 15;
  }

  y = 250;
  int width = 60;
  button = MdTextButton::Create(nullptr, ASCIIToUTF16("OK"));
  button->SetID(OK_BUTTON_ID);
  button->SetIsDefault(true);
  button->SetBounds(150, y, width, 30);
  GetContentsView()->AddChildView(std::move(button));

  button = MdTextButton::Create(nullptr, ASCIIToUTF16("Cancel"));
  button->SetID(CANCEL_BUTTON_ID);
  button->SetBounds(220, y, width, 30);
  GetContentsView()->AddChildView(std::move(button));

  button = MdTextButton::Create(nullptr, ASCIIToUTF16("Help"));
  button->SetID(HELP_BUTTON_ID);
  button->SetBounds(290, y, width, 30);
  GetContentsView()->AddChildView(std::move(button));

  y += 40;

  // Left bottom box with style checkboxes.
  auto tabbed_pane_contents = std::make_unique<View>();
  tabbed_pane_contents->SetBackground(CreateSolidBackground(SK_ColorWHITE));
  cb = std::make_unique<Checkbox>(ASCIIToUTF16("Bold"));
  cb_ptr = tabbed_pane_contents->AddChildView(std::move(cb));
  cb_ptr->SetBounds(10, 10, 50, 20);
  cb_ptr->SetID(BOLD_CHECKBOX_ID);

  cb = std::make_unique<Checkbox>(ASCIIToUTF16("Italic"));
  cb_ptr = tabbed_pane_contents->AddChildView(std::move(cb));
  cb_ptr->SetBounds(70, 10, 50, 20);
  cb_ptr->SetID(ITALIC_CHECKBOX_ID);

  cb = std::make_unique<Checkbox>(ASCIIToUTF16("Underlined"));
  cb_ptr = tabbed_pane_contents->AddChildView(std::move(cb));
  cb_ptr->SetBounds(130, 10, 70, 20);
  cb_ptr->SetID(UNDERLINED_CHECKBOX_ID);

  auto link = std::make_unique<Link>(ASCIIToUTF16("Help"));
  auto* link_ptr = tabbed_pane_contents->AddChildView(std::move(link));
  link_ptr->SetBounds(10, 35, 70, 10);
  link_ptr->SetID(STYLE_HELP_LINK_ID);

  text_field = std::make_unique<Textfield>();
  text_field_ptr = tabbed_pane_contents->AddChildView(std::move(text_field));
  text_field_ptr->SetBounds(10, 50, 100, 20);
  text_field_ptr->SetID(STYLE_TEXT_EDIT_ID);

  auto style_tab = std::make_unique<TabbedPane>();
  style_tab_ = GetContentsView()->AddChildView(std::move(style_tab));
  style_tab_->SetBounds(10, y, 210, 100);
  style_tab_->AddTab(ASCIIToUTF16("Style"), std::move(tabbed_pane_contents));
  style_tab_->GetSelectedTab()->SetID(STYLE_CONTAINER_ID);
  style_tab_->AddTab(ASCIIToUTF16("Other"), std::make_unique<View>());

  // Right bottom box with search.
  auto border_contents = std::make_unique<View>();
  border_contents->SetBackground(CreateSolidBackground(SK_ColorWHITE));
  text_field = std::make_unique<Textfield>();
  text_field_ptr = border_contents->AddChildView(std::move(text_field));
  text_field_ptr->SetBounds(10, 10, 100, 20);
  text_field_ptr->SetID(SEARCH_TEXTFIELD_ID);

  button = MdTextButton::Create(nullptr, ASCIIToUTF16("Search"));
  button->SetBounds(112, 5, 60, 30);
  button->SetID(SEARCH_BUTTON_ID);
  border_contents->AddChildView(std::move(button));

  link = std::make_unique<Link>(ASCIIToUTF16("Help"));
  link->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  link->SetID(HELP_LINK_ID);
  link_ptr = border_contents->AddChildView(std::move(link));
  link_ptr->SetBounds(175, 10, 30, 20);

  auto search_border_view =
      std::make_unique<BorderView>(border_contents.release());
  search_border_view->SetID(SEARCH_CONTAINER_ID);
  search_border_view_ =
      GetContentsView()->AddChildView(std::move(search_border_view));
  search_border_view_->SetBounds(300, y, 240, 50);

  y += 60;

  auto view_contents = std::make_unique<View>();
  view_contents->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  view_contents->SetBackground(CreateSolidBackground(SK_ColorBLUE));
  view_contents->SetID(THUMBNAIL_CONTAINER_ID);
  button = MdTextButton::Create(nullptr, ASCIIToUTF16("Star"));
  button->SetBounds(5, 5, 50, 30);
  button->SetID(THUMBNAIL_STAR_ID);
  view_contents->AddChildView(std::move(button));
  button = MdTextButton::Create(nullptr, ASCIIToUTF16("SuperStar"));
  button->SetBounds(60, 5, 100, 30);
  button->SetID(THUMBNAIL_SUPER_STAR_ID);
  view_contents->AddChildView(std::move(button));

  auto* contents_ptr =
      GetContentsView()->AddChildView(std::move(view_contents));
  contents_ptr->SetBounds(250, y, 200, 50);
  // We can only call RadioButton::SetChecked() on the radio-button is part of
  // the view hierarchy.
  radio_button_to_check->SetChecked(true);

  // Perform any pending layouts.
  GetWidget()->LayoutRootViewIfNecessary();
}

TEST_F(FocusTraversalTest, NormalTraversal) {
  const int kTraversalIDs[] = {TOP_CHECKBOX_ID,
                               APPLE_TEXTFIELD_ID,
                               ORANGE_TEXTFIELD_ID,
                               BANANA_TEXTFIELD_ID,
                               KIWI_TEXTFIELD_ID,
                               FRUIT_BUTTON_ID,
                               FRUIT_CHECKBOX_ID,
                               COMBOBOX_ID,
                               BROCCOLI_BUTTON_ID,
                               ROSETTA_LINK_ID,
                               STUPEUR_ET_TREMBLEMENT_LINK_ID,
                               DINER_GAME_LINK_ID,
                               RIDICULE_LINK_ID,
                               CLOSET_LINK_ID,
                               VISITING_LINK_ID,
                               AMELIE_LINK_ID,
                               JOYEUX_NOEL_LINK_ID,
                               CAMPING_LINK_ID,
                               BRICE_DE_NICE_LINK_ID,
                               TAXI_LINK_ID,
                               ASTERIX_LINK_ID,
                               OK_BUTTON_ID,
                               CANCEL_BUTTON_ID,
                               HELP_BUTTON_ID,
                               STYLE_CONTAINER_ID,
                               BOLD_CHECKBOX_ID,
                               ITALIC_CHECKBOX_ID,
                               UNDERLINED_CHECKBOX_ID,
                               STYLE_HELP_LINK_ID,
                               STYLE_TEXT_EDIT_ID,
                               SEARCH_TEXTFIELD_ID,
                               SEARCH_BUTTON_ID,
                               HELP_LINK_ID,
                               THUMBNAIL_CONTAINER_ID,
                               THUMBNAIL_STAR_ID,
                               THUMBNAIL_SUPER_STAR_ID};

  SCOPED_TRACE("NormalTraversal");

  // Let's traverse the whole focus hierarchy (several times, to make sure it
  // loops OK).
  GetFocusManager()->ClearFocus();
  AdvanceEntireFocusLoop(kTraversalIDs, false);

  // Let's traverse in reverse order.
  GetFocusManager()->ClearFocus();
  AdvanceEntireFocusLoop(kTraversalIDs, true);
}

#if defined(OS_MACOSX)
// Test focus traversal with full keyboard access off on Mac.
TEST_F(FocusTraversalTest, NormalTraversalMac) {
  GetFocusManager()->SetKeyboardAccessible(false);

  // Now only views with FocusBehavior of ALWAYS will be focusable.
  const int kTraversalIDs[] = {APPLE_TEXTFIELD_ID,    ORANGE_TEXTFIELD_ID,
                               BANANA_TEXTFIELD_ID,   KIWI_TEXTFIELD_ID,
                               STYLE_TEXT_EDIT_ID,    SEARCH_TEXTFIELD_ID,
                               THUMBNAIL_CONTAINER_ID};

  SCOPED_TRACE("NormalTraversalMac");

  // Let's traverse the whole focus hierarchy (several times, to make sure it
  // loops OK).
  GetFocusManager()->ClearFocus();
  AdvanceEntireFocusLoop(kTraversalIDs, false);

  // Let's traverse in reverse order.
  GetFocusManager()->ClearFocus();
  AdvanceEntireFocusLoop(kTraversalIDs, true);
}

// Test toggling full keyboard access correctly changes the focused view on Mac.
TEST_F(FocusTraversalTest, FullKeyboardToggle) {
  // Give focus to TOP_CHECKBOX_ID .
  FindViewByID(TOP_CHECKBOX_ID)->RequestFocus();
  EXPECT_EQ(TOP_CHECKBOX_ID, GetFocusManager()->GetFocusedView()->GetID());

  // Turn off full keyboard access. Focus should move to next view with ALWAYS
  // focus behavior.
  GetFocusManager()->SetKeyboardAccessible(false);
  EXPECT_EQ(APPLE_TEXTFIELD_ID, GetFocusManager()->GetFocusedView()->GetID());

  // Turning on full keyboard access should not change the focused view.
  GetFocusManager()->SetKeyboardAccessible(true);
  EXPECT_EQ(APPLE_TEXTFIELD_ID, GetFocusManager()->GetFocusedView()->GetID());

  // Give focus to SEARCH_BUTTON_ID.
  FindViewByID(SEARCH_BUTTON_ID)->RequestFocus();
  EXPECT_EQ(SEARCH_BUTTON_ID, GetFocusManager()->GetFocusedView()->GetID());

  // Turn off full keyboard access. Focus should move to next view with ALWAYS
  // focus behavior.
  GetFocusManager()->SetKeyboardAccessible(false);
  EXPECT_EQ(THUMBNAIL_CONTAINER_ID,
            GetFocusManager()->GetFocusedView()->GetID());

  // See focus advances correctly in both directions.
  GetFocusManager()->AdvanceFocus(false);
  EXPECT_EQ(APPLE_TEXTFIELD_ID, GetFocusManager()->GetFocusedView()->GetID());

  GetFocusManager()->AdvanceFocus(true);
  EXPECT_EQ(THUMBNAIL_CONTAINER_ID,
            GetFocusManager()->GetFocusedView()->GetID());
}
#endif  // OS_MACOSX

TEST_F(FocusTraversalTest, TraversalWithNonEnabledViews) {
  const int kDisabledIDs[] = {
      BANANA_TEXTFIELD_ID, FRUIT_CHECKBOX_ID,    COMBOBOX_ID,
      ASPARAGUS_BUTTON_ID, CAULIFLOWER_BUTTON_ID, CLOSET_LINK_ID,
      VISITING_LINK_ID,    BRICE_DE_NICE_LINK_ID, TAXI_LINK_ID,
      ASTERIX_LINK_ID,     HELP_BUTTON_ID,        BOLD_CHECKBOX_ID,
      SEARCH_TEXTFIELD_ID, HELP_LINK_ID};

  const int kTraversalIDs[] = {
      TOP_CHECKBOX_ID,    APPLE_TEXTFIELD_ID,
      ORANGE_TEXTFIELD_ID, KIWI_TEXTFIELD_ID,
      FRUIT_BUTTON_ID,     BROCCOLI_BUTTON_ID,
      ROSETTA_LINK_ID,     STUPEUR_ET_TREMBLEMENT_LINK_ID,
      DINER_GAME_LINK_ID,  RIDICULE_LINK_ID,
      AMELIE_LINK_ID,      JOYEUX_NOEL_LINK_ID,
      CAMPING_LINK_ID,     OK_BUTTON_ID,
      CANCEL_BUTTON_ID,    STYLE_CONTAINER_ID,
      ITALIC_CHECKBOX_ID, UNDERLINED_CHECKBOX_ID,
      STYLE_HELP_LINK_ID,  STYLE_TEXT_EDIT_ID,
      SEARCH_BUTTON_ID,    THUMBNAIL_CONTAINER_ID,
      THUMBNAIL_STAR_ID,   THUMBNAIL_SUPER_STAR_ID};

  SCOPED_TRACE("TraversalWithNonEnabledViews");

  // Let's disable some views.
  for (int kDisabledID : kDisabledIDs) {
    View* v = FindViewByID(kDisabledID);
    ASSERT_TRUE(v != nullptr);
    v->SetEnabled(false);
  }

  // Let's do one traversal (several times, to make sure it loops ok).
  GetFocusManager()->ClearFocus();
  AdvanceEntireFocusLoop(kTraversalIDs, false);

  // Same thing in reverse.
  GetFocusManager()->ClearFocus();
  AdvanceEntireFocusLoop(kTraversalIDs, true);
}

TEST_F(FocusTraversalTest, TraversalWithInvisibleViews) {
  const int kInvisibleIDs[] = {TOP_CHECKBOX_ID, OK_BUTTON_ID,
                               THUMBNAIL_CONTAINER_ID};

  const int kTraversalIDs[] = {
      APPLE_TEXTFIELD_ID,  ORANGE_TEXTFIELD_ID,
      BANANA_TEXTFIELD_ID, KIWI_TEXTFIELD_ID,
      FRUIT_BUTTON_ID,     FRUIT_CHECKBOX_ID,
      COMBOBOX_ID,         BROCCOLI_BUTTON_ID,
      ROSETTA_LINK_ID,     STUPEUR_ET_TREMBLEMENT_LINK_ID,
      DINER_GAME_LINK_ID,  RIDICULE_LINK_ID,
      CLOSET_LINK_ID,      VISITING_LINK_ID,
      AMELIE_LINK_ID,      JOYEUX_NOEL_LINK_ID,
      CAMPING_LINK_ID,     BRICE_DE_NICE_LINK_ID,
      TAXI_LINK_ID,        ASTERIX_LINK_ID,
      CANCEL_BUTTON_ID,    HELP_BUTTON_ID,
      STYLE_CONTAINER_ID,  BOLD_CHECKBOX_ID,
      ITALIC_CHECKBOX_ID, UNDERLINED_CHECKBOX_ID,
      STYLE_HELP_LINK_ID,  STYLE_TEXT_EDIT_ID,
      SEARCH_TEXTFIELD_ID, SEARCH_BUTTON_ID,
      HELP_LINK_ID};

  SCOPED_TRACE("TraversalWithInvisibleViews");

  // Let's make some views invisible.
  for (int kInvisibleID : kInvisibleIDs) {
    View* v = FindViewByID(kInvisibleID);
    ASSERT_TRUE(v != nullptr);
    v->SetVisible(false);
  }

  // Let's do one traversal (several times, to make sure it loops ok).
  GetFocusManager()->ClearFocus();
  AdvanceEntireFocusLoop(kTraversalIDs, false);

  // Same thing in reverse.
  GetFocusManager()->ClearFocus();
  AdvanceEntireFocusLoop(kTraversalIDs, true);
}

TEST_F(FocusTraversalTest, PaneTraversal) {
  // Tests trapping the traversal within a pane - useful for full
  // keyboard accessibility for toolbars.

  // First test the left container.
  const int kLeftTraversalIDs[] = {APPLE_TEXTFIELD_ID,  ORANGE_TEXTFIELD_ID,
                                   BANANA_TEXTFIELD_ID, KIWI_TEXTFIELD_ID,
                                   FRUIT_BUTTON_ID,     FRUIT_CHECKBOX_ID,
                                   COMBOBOX_ID};

  SCOPED_TRACE("PaneTraversal");

  FocusSearch focus_search_left(left_container_, true, false);
  left_container_->EnablePaneFocus(&focus_search_left);
  FindViewByID(COMBOBOX_ID)->RequestFocus();

  // Traverse the focus hierarchy within the pane several times.
  AdvanceEntireFocusLoop(kLeftTraversalIDs, false);

  // Traverse in reverse order.
  FindViewByID(APPLE_TEXTFIELD_ID)->RequestFocus();
  AdvanceEntireFocusLoop(kLeftTraversalIDs, true);

  // Now test the right container, but this time with accessibility mode.
  // Make some links not focusable, but mark one of them as
  // "accessibility focusable", so it should show up in the traversal.
  const int kRightTraversalIDs[] = {
      BROCCOLI_BUTTON_ID,  DINER_GAME_LINK_ID, RIDICULE_LINK_ID,
      CLOSET_LINK_ID,      VISITING_LINK_ID,   AMELIE_LINK_ID,
      JOYEUX_NOEL_LINK_ID, CAMPING_LINK_ID,    BRICE_DE_NICE_LINK_ID,
      TAXI_LINK_ID,        ASTERIX_LINK_ID};

  FocusSearch focus_search_right(right_container_, true, true);
  right_container_->EnablePaneFocus(&focus_search_right);
  FindViewByID(ROSETTA_LINK_ID)->SetFocusBehavior(View::FocusBehavior::NEVER);
  FindViewByID(STUPEUR_ET_TREMBLEMENT_LINK_ID)
      ->SetFocusBehavior(View::FocusBehavior::NEVER);
  FindViewByID(DINER_GAME_LINK_ID)
      ->SetFocusBehavior(View::FocusBehavior::ACCESSIBLE_ONLY);
  FindViewByID(ASTERIX_LINK_ID)->RequestFocus();

  // Traverse the focus hierarchy within the pane several times.
  AdvanceEntireFocusLoop(kRightTraversalIDs, false);

  // Traverse in reverse order.
  FindViewByID(BROCCOLI_BUTTON_ID)->RequestFocus();
  AdvanceEntireFocusLoop(kRightTraversalIDs, true);
}

class FocusTraversalNonFocusableTest : public FocusManagerTest {
 public:
  ~FocusTraversalNonFocusableTest() override = default;

  void InitContentView() override;

 protected:
  FocusTraversalNonFocusableTest() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(FocusTraversalNonFocusableTest);
};

void FocusTraversalNonFocusableTest::InitContentView() {
  // Create a complex nested view hierarchy with no focusable views. This is a
  // regression test for http://crbug.com/453699. There was previously a bug
  // where advancing focus backwards through this tree resulted in an
  // exponential number of nodes being searched. (Each time it traverses one of
  // the x1-x3-x2 triangles, it will traverse the left sibling of x1, (x+1)0,
  // twice, which means it will visit O(2^n) nodes.)
  //
  // |              0         |
  // |            /   \       |
  // |          /       \     |
  // |         10        1    |
  // |        /  \      / \   |
  // |      /      \   /   \  |
  // |     20      11  2   3  |
  // |    / \      / \        |
  // |   /   \    /   \       |
  // |  ...  21  12   13      |
  // |       / \              |
  // |      /   \             |
  // |     22   23            |

  View* v = GetContentsView();
  // Create 30 groups of 4 nodes. |v| is the top of each group.
  for (int i = 0; i < 300; i += 10) {
    // |v|'s left child is the top of the next group. If |v| is 20, this is 30.
    View* v10 = new View;
    v10->SetID(i + 10);
    v->AddChildView(v10);

    // |v|'s right child. If |v| is 20, this is 21.
    View* v1 = new View;
    v1->SetID(i + 1);
    v->AddChildView(v1);

    // |v|'s right child has two children. If |v| is 20, these are 22 and 23.
    View* v2 = new View;
    v2->SetID(i + 2);
    View* v3 = new View;
    v3->SetID(i + 3);
    v1->AddChildView(v2);
    v1->AddChildView(v3);

    v = v10;
  }
}

// See explanation in InitContentView.
// NOTE: The failure mode of this test (if http://crbug.com/453699 were to
// regress) is a timeout, due to exponential run time.
TEST_F(FocusTraversalNonFocusableTest, PathologicalSiblingTraversal) {
  // Advance forwards from the root node.
  GetFocusManager()->ClearFocus();
  GetFocusManager()->AdvanceFocus(false);
  EXPECT_FALSE(GetFocusManager()->GetFocusedView());

  // Advance backwards from the root node.
  GetFocusManager()->ClearFocus();
  GetFocusManager()->AdvanceFocus(true);
  EXPECT_FALSE(GetFocusManager()->GetFocusedView());
}

}  // namespace views
