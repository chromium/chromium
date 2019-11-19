// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/editable_combobox/editable_combobox.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/models/combobox_model_observer.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/models/menu_separator_types.h"
#include "ui/base/ui_base_types.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/render_text.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host_view.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_ripple.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/controls/combobox/combobox_util.h"
#include "ui/views/controls/editable_combobox/editable_combobox_listener.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/menu_types.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace views {

namespace {

class Arrow : public Button {
 public:
  Arrow(const SkColor color, ButtonListener* listener)
      : Button(listener), color_(color) {
    // Similar to Combobox's TransparentButton.
    SetFocusBehavior(FocusBehavior::NEVER);
    button_controller()->set_notify_action(
        ButtonController::NotifyAction::kOnPress);

    SetInkDropMode(InkDropMode::ON);
    set_has_ink_drop_action_on_click(true);
  }
  ~Arrow() override = default;

  double GetAnimationValue() const {
    return hover_animation().GetCurrentValue();
  }

  // Overridden from InkDropHost:
  // Similar to Combobox's TransparentButton.
  std::unique_ptr<InkDrop> CreateInkDrop() override {
    std::unique_ptr<views::InkDropImpl> ink_drop = CreateDefaultInkDropImpl();
    ink_drop->SetShowHighlightOnHover(false);
    return std::move(ink_drop);
  }

  // Similar to Combobox's TransparentButton.
  std::unique_ptr<InkDropRipple> CreateInkDropRipple() const override {
    return std::make_unique<views::FloodFillInkDropRipple>(
        size(), GetInkDropCenterBasedOnLastEvent(),
        GetNativeTheme()->GetSystemColor(
            ui::NativeTheme::kColorId_LabelEnabledColor),
        ink_drop_visible_opacity());
  }

 private:
  void PaintButtonContents(gfx::Canvas* canvas) override {
    gfx::ScopedCanvas scoped_canvas(canvas);
    canvas->ClipRect(GetContentsBounds());
    gfx::Rect arrow_bounds = GetLocalBounds();
    arrow_bounds.ClampToCenteredSize(ComboboxArrowSize());
    PaintComboboxArrow(color_, arrow_bounds, canvas);
  }

  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    node_data->role = ax::mojom::Role::kComboBoxMenuButton;
    node_data->SetName(GetAccessibleName());
    node_data->SetHasPopup(ax::mojom::HasPopup::kMenu);
    if (GetEnabled())
      node_data->SetDefaultActionVerb(ax::mojom::DefaultActionVerb::kOpen);
  }

  const SkColor color_;

  DISALLOW_COPY_AND_ASSIGN(Arrow);
};

}  // namespace

// Adapts a ui::ComboboxModel to a ui::MenuModel to be used by EditableCombobox.
// Also provides a filtering capability.
class EditableCombobox::EditableComboboxMenuModel
    : public ui::MenuModel,
      public ui::ComboboxModelObserver {
 public:
  EditableComboboxMenuModel(EditableCombobox* owner,
                            ui::ComboboxModel* combobox_model,
                            const bool filter_on_edit,
                            const bool show_on_empty)
      : owner_(owner),
        combobox_model_(combobox_model),
        filter_on_edit_(filter_on_edit),
        show_on_empty_(show_on_empty) {
    UpdateItemsShown();
    combobox_model_->AddObserver(this);
  }

  ~EditableComboboxMenuModel() override {
    combobox_model_->RemoveObserver(this);
  }

  void UpdateItemsShown() {
    if (!update_items_shown_enabled_)
      return;
    items_shown_.clear();
    items_shown_enabled_.clear();
    if (show_on_empty_ || !owner_->GetText().empty()) {
      for (int i = 0; i < combobox_model_->GetItemCount(); ++i) {
        if (!filter_on_edit_ ||
            base::StartsWith(combobox_model_->GetItemAt(i), owner_->GetText(),
                             base::CompareCase::INSENSITIVE_ASCII)) {
          items_shown_.push_back(combobox_model_->GetItemAt(i));
          items_shown_enabled_.push_back(combobox_model_->IsItemEnabledAt(i));
        }
      }
    }
    if (menu_model_delegate())
      menu_model_delegate()->OnMenuStructureChanged();
  }

  void DisableUpdateItemsShown() { update_items_shown_enabled_ = false; }

  void EnableUpdateItemsShown() { update_items_shown_enabled_ = true; }

  bool UseCheckmarks() const {
    return MenuConfig::instance().check_selected_combobox_item;
  }

  base::string16 GetItemTextAt(int index, bool showing_password_text) const {
    return showing_password_text
               ? items_shown_[index]
               : base::string16(items_shown_[index].length(),
                                gfx::RenderText::kPasswordReplacementChar);
  }

  //////////////////////////////////////////////////////////////////////////////
  // Overridden from ComboboxModelObserver:
  void OnComboboxModelChanged(ui::ComboboxModel* model) override {
    UpdateItemsShown();
  }

  //////////////////////////////////////////////////////////////////////////////
  // Overridden from MenuModel:

  int GetItemCount() const override { return items_shown_.size(); }

 private:
  bool HasIcons() const override { return false; }

  ItemType GetTypeAt(int index) const override {
    return UseCheckmarks() ? TYPE_CHECK : TYPE_COMMAND;
  }

  ui::MenuSeparatorType GetSeparatorTypeAt(int index) const override {
    return ui::NORMAL_SEPARATOR;
  }

  int GetCommandIdAt(int index) const override {
    constexpr int kFirstMenuItemId = 1000;
    return index + kFirstMenuItemId;
  }

  base::string16 GetLabelAt(int index) const override {
    base::string16 text = GetItemTextAt(index, owner_->showing_password_text_);
    base::i18n::AdjustStringForLocaleDirection(&text);
    return text;
  }

  bool IsItemDynamicAt(int index) const override { return false; }

  const gfx::FontList* GetLabelFontListAt(int index) const override {
    return &owner_->GetFontList();
  }

  bool GetAcceleratorAt(int index,
                        ui::Accelerator* accelerator) const override {
    return false;
  }

  bool IsItemCheckedAt(int index) const override {
    return UseCheckmarks() && items_shown_[index] == owner_->GetText();
  }

  int GetGroupIdAt(int index) const override { return -1; }

  bool GetIconAt(int index, gfx::Image* icon) const override { return false; }

  ui::ButtonMenuItemModel* GetButtonMenuItemAt(int index) const override {
    return nullptr;
  }

  bool IsEnabledAt(int index) const override {
    return items_shown_enabled_[index];
  }

  void ActivatedAt(int index) override { owner_->OnItemSelected(index); }

  MenuModel* GetSubmenuModelAt(int index) const override { return nullptr; }

  EditableCombobox* owner_;            // Weak. Owns |this|.
  ui::ComboboxModel* combobox_model_;  // Weak.

  // Whether to adapt the items shown to the textfield content.
  const bool filter_on_edit_;

  // Whether to show options when the textfield is empty.
  const bool show_on_empty_;

  // The items from |combobox_model_| that we are currently showing.
  std::vector<base::string16> items_shown_;
  std::vector<bool> items_shown_enabled_;

  // When false, UpdateItemsShown doesn't do anything.
  bool update_items_shown_enabled_ = true;

  DISALLOW_COPY_AND_ASSIGN(EditableComboboxMenuModel);
};

// This class adds itself as the pre-target handler for the RootView of the
// EditableCombobox. We use it to close the menu when press events happen in the
// RootView but not inside the EditableComboobox's textfield.
class EditableCombobox::EditableComboboxPreTargetHandler
    : public ui::EventHandler {
 public:
  EditableComboboxPreTargetHandler(EditableCombobox* owner, View* root_view)
      : owner_(owner), root_view_(root_view) {
    root_view_->AddPreTargetHandler(this);
  }

  ~EditableComboboxPreTargetHandler() override { StopObserving(); }

  // ui::EventHandler overrides.
  void OnMouseEvent(ui::MouseEvent* event) override {
    if (event->type() == ui::ET_MOUSE_PRESSED &&
        event->button_flags() == event->changed_button_flags())
      HandlePressEvent(event->root_location());
  }

  void OnTouchEvent(ui::TouchEvent* event) override {
    if (event->type() == ui::ET_TOUCH_PRESSED)
      HandlePressEvent(event->root_location());
  }

 private:
  void HandlePressEvent(const gfx::Point& root_location) {
    View* handler = root_view_->GetEventHandlerForPoint(root_location);
    if (handler == owner_->textfield_ || handler == owner_->arrow_)
      return;
    owner_->CloseMenu();
  }

  void StopObserving() {
    if (!root_view_)
      return;
    root_view_->RemovePreTargetHandler(this);
    root_view_ = nullptr;
  }

  EditableCombobox* owner_;
  View* root_view_;

  DISALLOW_COPY_AND_ASSIGN(EditableComboboxPreTargetHandler);
};

////////////////////////////////////////////////////////////////////////////////
// EditableCombobox, public, non-overridden methods:
EditableCombobox::EditableCombobox(
    std::unique_ptr<ui::ComboboxModel> combobox_model,
    const bool filter_on_edit,
    const bool show_on_empty,
    const Type type,
    const int text_context,
    const int text_style,
    const bool display_arrow)
    : textfield_(new Textfield()),
      combobox_model_(std::move(combobox_model)),
      menu_model_(
          std::make_unique<EditableComboboxMenuModel>(this,
                                                      combobox_model_.get(),
                                                      filter_on_edit,
                                                      show_on_empty)),
      text_context_(text_context),
      text_style_(text_style),
      type_(type),
      showing_password_text_(type != Type::kPassword) {
  textfield_->AddObserver(this);
  textfield_->set_controller(this);
  textfield_->SetFontList(GetFontList());
  textfield_->SetTextInputType((type == Type::kPassword)
                                   ? ui::TEXT_INPUT_TYPE_PASSWORD
                                   : ui::TEXT_INPUT_TYPE_TEXT);
  AddChildView(textfield_);
  if (display_arrow) {
    textfield_->SetExtraInsets(gfx::Insets(
        /*top=*/0, /*left=*/0, /*bottom=*/0,
        /*right=*/kComboboxArrowContainerWidth - kComboboxArrowPaddingWidth));
    arrow_ = new Arrow(textfield_->GetTextColor(), this);
    AddChildView(arrow_);
  }
  SetLayoutManager(std::make_unique<views::FillLayout>());
}

EditableCombobox::~EditableCombobox() {
  CloseMenu();
  textfield_->set_controller(nullptr);
  textfield_->RemoveObserver(this);
}

const base::string16& EditableCombobox::GetText() const {
  return textfield_->GetText();
}

void EditableCombobox::SetText(const base::string16& text) {
  textfield_->SetText(text);
  // SetText does not actually notify the TextfieldController, so we call the
  // handling code directly.
  HandleNewContent(text);
}

const gfx::FontList& EditableCombobox::GetFontList() const {
  return style::GetFont(text_context_, text_style_);
}

void EditableCombobox::SelectRange(const gfx::Range& range) {
  textfield_->SetSelectedRange(range);
}

void EditableCombobox::SetAccessibleName(const base::string16& name) {
  textfield_->SetAccessibleName(name);
  if (arrow_)
    arrow_->SetAccessibleName(name);
}

void EditableCombobox::SetAssociatedLabel(View* labelling_view) {
  textfield_->SetAssociatedLabel(labelling_view);
}

void EditableCombobox::RevealPasswords(bool revealed) {
  DCHECK_EQ(Type::kPassword, type_);
  if (revealed == showing_password_text_)
    return;
  showing_password_text_ = revealed;
  textfield_->SetTextInputType(revealed ? ui::TEXT_INPUT_TYPE_TEXT
                                        : ui::TEXT_INPUT_TYPE_PASSWORD);
  menu_model_->UpdateItemsShown();
}

int EditableCombobox::GetItemCountForTest() {
  return menu_model_->GetItemCount();
}

base::string16 EditableCombobox::GetItemForTest(int index) {
  return menu_model_->GetItemTextAt(index, showing_password_text_);
}

////////////////////////////////////////////////////////////////////////////////
// EditableCombobox, View overrides:

void EditableCombobox::Layout() {
  View::Layout();
  if (arrow_) {
    gfx::Rect arrow_bounds(/*x=*/width() - kComboboxArrowContainerWidth,
                           /*y=*/0, kComboboxArrowContainerWidth, height());
    arrow_->SetBoundsRect(arrow_bounds);
  }
}

void EditableCombobox::OnThemeChanged() {
  textfield_->OnThemeChanged();
}

void EditableCombobox::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kComboBoxGrouping;

  node_data->SetName(textfield_->GetAccessibleName());
  node_data->SetValue(GetText());
}

////////////////////////////////////////////////////////////////////////////////
// EditableCombobox, TextfieldController overrides:

void EditableCombobox::ContentsChanged(Textfield* sender,
                                       const base::string16& new_contents) {
  HandleNewContent(new_contents);
  ShowDropDownMenu(ui::MENU_SOURCE_KEYBOARD);
}

bool EditableCombobox::HandleKeyEvent(Textfield* sender,
                                      const ui::KeyEvent& key_event) {
  if (key_event.type() == ui::ET_KEY_PRESSED &&
      (key_event.key_code() == ui::VKEY_UP ||
       key_event.key_code() == ui::VKEY_DOWN)) {
    ShowDropDownMenu(ui::MENU_SOURCE_KEYBOARD);
    return true;
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// EditableCombobox, View overrides:

void EditableCombobox::OnViewBlurred(View* observed_view) {
  CloseMenu();
}

////////////////////////////////////////////////////////////////////////////////
// EditableCombobox, ButtonListener overrides:

void EditableCombobox::ButtonPressed(Button* sender, const ui::Event& event) {
  textfield_->RequestFocus();
  if (menu_runner_ && menu_runner_->IsRunning()) {
    CloseMenu();
    return;
  }
  ui::MenuSourceType source_type = ui::MENU_SOURCE_MOUSE;
  if (event.IsKeyEvent())
    source_type = ui::MENU_SOURCE_KEYBOARD;
  else if (event.IsGestureEvent() || event.IsTouchEvent())
    source_type = ui::MENU_SOURCE_TOUCH;
  ShowDropDownMenu(source_type);
}

////////////////////////////////////////////////////////////////////////////////
// EditableCombobox, Private methods:

void EditableCombobox::CloseMenu() {
  menu_runner_.reset();
  pre_target_handler_.reset();
}

void EditableCombobox::OnItemSelected(int index) {
  // |textfield_| can hide the characters on its own so we read the actual
  // characters instead of gfx::RenderText::kPasswordReplacementChar characters.
  base::string16 selected_item_text =
      menu_model_->GetItemTextAt(index, /*showing_password_text=*/true);
  textfield_->SetText(selected_item_text);
  // SetText does not actually notify the TextfieldController, so we call the
  // handling code directly.
  HandleNewContent(selected_item_text);
  NotifyAccessibilityEvent(ax::mojom::Event::kValueChanged,
                           /*xsend_native_event=*/true);
}

void EditableCombobox::HandleNewContent(const base::string16& new_content) {
  DCHECK(GetText() == new_content);
  // We notify |listener_| before updating |menu_model_|'s items shown. This
  // gives the user a chance to modify the ComboboxModel beforehand if they wish
  // to do so.
  // We disable UpdateItemsShown while we notify the listener in case it
  // modifies the ComboboxModel, then calls OnComboboxModelChanged and thus
  // UpdateItemsShown. That way UpdateItemsShown doesn't do its work twice.
  if (listener_) {
    menu_model_->DisableUpdateItemsShown();
    listener_->OnContentChanged(this);
    menu_model_->EnableUpdateItemsShown();
  }
  menu_model_->UpdateItemsShown();
}

void EditableCombobox::ShowDropDownMenu(ui::MenuSourceType source_type) {
  constexpr int kMenuBorderWidthLeft = 1;
  constexpr int kMenuBorderWidthTop = 1;
  constexpr int kMenuBorderWidthRight = 1;

  if (!menu_model_->GetItemCount()) {
    CloseMenu();
    return;
  }
  if (menu_runner_ && menu_runner_->IsRunning())
    return;
  if (!GetWidget())
    return;

  // Since we don't capture the mouse, we want to see the events that happen in
  // the EditableCombobox's RootView to get a chance to close the menu if they
  // happen outside |textfield_|. Events that happen over the menu belong to
  // another Widget and they don't go through this pre-target handler.
  // Events that happen outside both the menu and the RootView cause
  // OnViewBlurred to be called, which also closes the menu.
  pre_target_handler_ = std::make_unique<EditableComboboxPreTargetHandler>(
      this, GetWidget()->GetRootView());

  gfx::Rect local_bounds = textfield_->GetLocalBounds();
  gfx::Point menu_position(local_bounds.origin());
  // Inset the menu's requested position so the border of the menu lines up
  // with the border of the textfield.
  menu_position.set_x(menu_position.x() + kMenuBorderWidthLeft);
  menu_position.set_y(menu_position.y() + kMenuBorderWidthTop);
  local_bounds.set_width(local_bounds.width() -
                         (kMenuBorderWidthLeft + kMenuBorderWidthRight));
  View::ConvertPointToScreen(this, &menu_position);
  gfx::Rect bounds(menu_position, local_bounds.size());

  menu_runner_ = std::make_unique<MenuRunner>(
      menu_model_.get(), MenuRunner::EDITABLE_COMBOBOX,
      base::BindRepeating(&EditableCombobox::CloseMenu,
                          base::Unretained(this)));
  menu_runner_->RunMenuAt(GetWidget(), nullptr, bounds,
                          MenuAnchorPosition::kTopLeft, source_type);
}

BEGIN_METADATA(EditableCombobox)
METADATA_PARENT_CLASS(View)
END_METADATA()

}  // namespace views
