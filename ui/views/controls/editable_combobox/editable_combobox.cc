// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/editable_combobox/editable_combobox.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/menu_source_utils.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/models/combobox_model_observer.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/models/menu_separator_types.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_types.h"
#include "ui/color/color_provider.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_ripple.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/controls/combobox/combobox_util.h"
#include "ui/views/controls/combobox/empty_combobox_model.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/menu_types.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace views {

namespace {

int kEditableComboboxButtonSize = 24;
int kEditableComboboxControlsContainerInsets = 6;

class Arrow : public Button {
  METADATA_HEADER(Arrow, Button)

 public:
  explicit Arrow(PressedCallback callback) : Button(std::move(callback)) {
    SetPreferredSize(
        gfx::Size(kEditableComboboxButtonSize, kEditableComboboxButtonSize));

    button_controller()->set_notify_action(
        ButtonController::NotifyAction::kOnPress);

    ConfigureComboboxButtonInkDrop(this);
    GetViewAccessibility().SetRole(ax::mojom::Role::kButton);
    UpdateAccessibleDefaultActionVerb();
    GetViewAccessibility().SetHasPopup(ax::mojom::HasPopup::kMenu);
  }
  Arrow(const Arrow&) = delete;
  Arrow& operator=(const Arrow&) = delete;
  ~Arrow() override = default;

  double GetAnimationValue() const {
    return hover_animation().GetCurrentValue();
  }

 private:
  void PaintButtonContents(gfx::Canvas* canvas) override {
    gfx::ScopedCanvas scoped_canvas(canvas);
    canvas->ClipRect(GetContentsBounds());
    gfx::Rect arrow_bounds = GetLocalBounds();
    arrow_bounds.ClampToCenteredSize(ComboboxArrowSize());
    // Make sure the arrow use the same color as the text in the combobox.
    PaintComboboxArrow(
        GetColorProvider()->GetColor(TypographyProvider::Get().GetColorId(
            style::CONTEXT_TEXTFIELD,
            GetEnabled() ? style::STYLE_PRIMARY : style::STYLE_DISABLED)),
        arrow_bounds, canvas);
  }

  void UpdateAccessibleDefaultActionVerb() {
    if (GetEnabled()) {
      GetViewAccessibility().SetDefaultActionVerb(
          ax::mojom::DefaultActionVerb::kOpen);
    } else {
      GetViewAccessibility().RemoveDefaultActionVerb();
    }
  }

  base::CallbackListSubscription enabled_changed_subscription_ =
      AddEnabledChangedCallback(
          base::BindRepeating(&Arrow::UpdateAccessibleDefaultActionVerb,
                              base::Unretained(this)));
};

BEGIN_METADATA(Arrow)
END_METADATA

}  // namespace

std::u16string EditableCombobox::MenuDecorationStrategy::DecorateItemText(
    std::u16string text) const {
  return text;
}

// Adapts a ui::ComboboxModel to a ui::MenuModel to be used by EditableCombobox.
// Also provides a filtering capability.
class EditableCombobox::EditableComboboxMenuModel final
    : public ui::MenuModel,
      public ui::ComboboxModelObserver {
 public:
  EditableComboboxMenuModel(EditableCombobox* owner,
                            std::unique_ptr<ui::ComboboxModel> combobox_model,
                            const bool filter_on_edit,
                            const bool show_on_empty)
      : decoration_strategy_(std::make_unique<MenuDecorationStrategy>()),
        owner_(owner),
        combobox_model_(std::move(combobox_model)),
        filter_on_edit_(filter_on_edit),
        show_on_empty_(show_on_empty) {
    UpdateItemsShown();
    observation_.Observe(combobox_model_.get());
  }

  EditableComboboxMenuModel(const EditableComboboxMenuModel&) = delete;
  EditableComboboxMenuModel& operator=(const EditableComboboxMenuModel&) =
      delete;

  ~EditableComboboxMenuModel() override = default;

  void UpdateItemsShown() {
    if (!update_items_shown_enabled_) {
      return;
    }
    items_shown_.clear();
    if (show_on_empty_ || !owner_->GetText().empty()) {
      for (size_t i = 0; i < combobox_model_->GetItemCount(); ++i) {
        if (!filter_on_edit_ ||
            base::StartsWith(combobox_model_->GetItemAt(i), owner_->GetText(),
                             base::CompareCase::INSENSITIVE_ASCII)) {
          items_shown_.push_back({i, combobox_model_->IsItemEnabledAt(i)});
        }
      }
    }
    if (menu_model_delegate()) {
      menu_model_delegate()->OnMenuStructureChanged();
    }
  }

  void SetDecorationStrategy(
      std::unique_ptr<EditableCombobox::MenuDecorationStrategy> strategy) {
    DCHECK(strategy);
    decoration_strategy_ = std::move(strategy);
    UpdateItemsShown();
  }

  void DisableUpdateItemsShown() { update_items_shown_enabled_ = false; }

  void EnableUpdateItemsShown() { update_items_shown_enabled_ = true; }

  bool UseCheckmarks() const {
    return MenuConfig::instance().check_selected_combobox_item;
  }

  std::u16string GetItemTextAt(size_t index) const {
    return combobox_model_->GetItemAt(items_shown_[index].index);
  }

  const ui::ComboboxModel* GetComboboxModel() const {
    return combobox_model_.get();
  }

  ui::ImageModel GetIconAt(size_t index) const override {
    return combobox_model_->GetDropDownIconAt(items_shown_[index].index);
  }

  // ComboboxModelObserver:
  void OnComboboxModelChanged(ui::ComboboxModel* model) override {
    UpdateItemsShown();
  }

  void OnComboboxModelDestroying(ui::ComboboxModel* model) override {
    observation_.Reset();
  }

  base::WeakPtr<ui::MenuModel> AsWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

  size_t GetItemCount() const override { return items_shown_.size(); }

  // ui::MenuModel:
  std::u16string GetLabelAt(size_t index) const override {
    std::u16string text =
        decoration_strategy_->DecorateItemText(GetItemTextAt(index));
    base::i18n::AdjustStringForLocaleDirection(&text);
    return text;
  }

 private:
  struct ShownItem {
    size_t index;
    bool enabled;
  };

  ItemType GetTypeAt(size_t index) const override {
    return UseCheckmarks() ? TYPE_CHECK : TYPE_COMMAND;
  }

  ui::MenuSeparatorType GetSeparatorTypeAt(size_t index) const override {
    return ui::NORMAL_SEPARATOR;
  }

  int GetCommandIdAt(size_t index) const override {
    constexpr int kFirstMenuItemId = 1000;
    return static_cast<int>(index) + kFirstMenuItemId;
  }

  bool IsItemDynamicAt(size_t index) const override { return false; }

  const gfx::FontList* GetLabelFontListAt(size_t index) const override {
    return &owner_->GetFontList();
  }

  bool GetAcceleratorAt(size_t index,
                        ui::Accelerator* accelerator) const override {
    return false;
  }

  bool IsItemCheckedAt(size_t index) const override {
    return UseCheckmarks() &&
           combobox_model_->GetItemAt(items_shown_[index].index) ==
               owner_->GetText();
  }

  int GetGroupIdAt(size_t index) const override { return -1; }

  ui::ButtonMenuItemModel* GetButtonMenuItemAt(size_t index) const override {
    return nullptr;
  }

  bool IsEnabledAt(size_t index) const override {
    return items_shown_[index].enabled;
  }

  void ActivatedAt(size_t index) override { owner_->OnItemSelected(index); }

  MenuModel* GetSubmenuModelAt(size_t index) const override { return nullptr; }

  // The strategy used to customize the display of the dropdown menu.
  std::unique_ptr<MenuDecorationStrategy> decoration_strategy_;

  raw_ptr<EditableCombobox> owner_;            // Weak. Owns |this|.
  std::unique_ptr<ui::ComboboxModel> combobox_model_;

  // Whether to adapt the items shown to the textfield content.
  const bool filter_on_edit_;

  // Whether to show options when the textfield is empty.
  const bool show_on_empty_;

  // The indices of the items from |combobox_model_| that we are currently
  // showing, and whether they are enabled.
  std::vector<ShownItem> items_shown_;

  // When false, UpdateItemsShown doesn't do anything.
  bool update_items_shown_enabled_ = true;

  base::ScopedObservation<ui::ComboboxModel, ui::ComboboxModelObserver>
      observation_{this};

  base::WeakPtrFactory<EditableComboboxMenuModel> weak_ptr_factory_{this};
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

  EditableComboboxPreTargetHandler(const EditableComboboxPreTargetHandler&) =
      delete;
  EditableComboboxPreTargetHandler& operator=(
      const EditableComboboxPreTargetHandler&) = delete;

  ~EditableComboboxPreTargetHandler() override { StopObserving(); }

  // ui::EventHandler overrides.
  void OnMouseEvent(ui::MouseEvent* event) override {
    if (event->type() == ui::EventType::kMousePressed &&
        event->button_flags() == event->changed_button_flags()) {
      HandlePressEvent(event->root_location());
    }
  }

  void OnTouchEvent(ui::TouchEvent* event) override {
    if (event->type() == ui::EventType::kTouchPressed) {
      HandlePressEvent(event->root_location());
    }
  }

 private:
  void HandlePressEvent(const gfx::Point& root_location) {
    View* handler = root_view_->GetEventHandlerForPoint(root_location);
    if (handler == owner_->textfield_ || handler == owner_->arrow_) {
      return;
    }
    owner_->CloseMenu();
  }

  void StopObserving() {
    if (!root_view_) {
      return;
    }
    root_view_->RemovePreTargetHandler(this);
    root_view_ = nullptr;
  }

  raw_ptr<EditableCombobox> owner_;
  raw_ptr<View> root_view_;
};

EditableCombobox::EditableCombobox()
    : EditableCombobox(std::make_unique<internal::EmptyComboboxModel>()) {}

EditableCombobox::EditableCombobox(
    std::unique_ptr<ui::ComboboxModel> combobox_model,
    const bool filter_on_edit,
    const bool show_on_empty,
    const int text_context,
    const int text_style,
    const bool display_arrow)
    : textfield_(AddChildView(std::make_unique<Textfield>())),
      text_context_(text_context),
      text_style_(text_style),
      filter_on_edit_(filter_on_edit),
      show_on_empty_(show_on_empty) {
  SetModel(std::move(combobox_model));
  observation_.Observe(textfield_.get());
  textfield_->set_controller(this);
  textfield_->SetFontList(GetFontList());
  views::FocusRing::Get(textfield_)->SetOutsetFocusRingDisabled(true);

  control_elements_container_ = AddChildView(std::make_unique<BoxLayoutView>());
  control_elements_container_->SetInsideBorderInsets(
      gfx::Insets::TLBR(kEditableComboboxControlsContainerInsets, 0,
                        kEditableComboboxControlsContainerInsets,
                        kEditableComboboxControlsContainerInsets));
  if (display_arrow) {
    arrow_ = AddControlElement(std::make_unique<Arrow>(base::BindRepeating(
        &EditableCombobox::ArrowButtonPressed, base::Unretained(this))));
    // We need this so the arrow icon is not covered when the combo box view is
    // hovered
    arrow_->SetPaintToLayer();
    arrow_->layer()->SetFillsBoundsOpaquely(false);
  }

  SetLayoutManager(std::make_unique<DelegatingLayoutManager>(this));
  GetViewAccessibility().SetRole(ax::mojom::Role::kComboBoxGrouping);
  GetViewAccessibility().SetValue(GetText());
}

EditableCombobox::~EditableCombobox() {
  CloseMenu();
  textfield_->set_controller(nullptr);
}

void EditableCombobox::SetModel(std::unique_ptr<ui::ComboboxModel> model) {
  CloseMenu();
  menu_model_ = std::make_unique<EditableComboboxMenuModel>(
      this, std::move(model), filter_on_edit_, show_on_empty_);
}

const std::u16string& EditableCombobox::GetText() const {
  return textfield_->GetText();
}

void EditableCombobox::SetText(const std::u16string& text) {
  textfield_->SetText(text);
  // SetText does not actually notify the TextfieldController, so we call the
  // handling code directly.
  HandleNewContent(text);
}

void EditableCombobox::SetInvalid(bool invalid) {
  textfield_->SetInvalid(invalid);
}

const std::u16string& EditableCombobox::GetPlaceholderText() const {
  return textfield_->GetPlaceholderText();
}

void EditableCombobox::SetPlaceholderText(const std::u16string& text) {
  textfield_->SetPlaceholderText(text);
}

const gfx::FontList& EditableCombobox::GetFontList() const {
  return TypographyProvider::Get().GetFont(text_context_, text_style_);
}

void EditableCombobox::SelectRange(const gfx::Range& range) {
  textfield_->SetSelectedRange(range);
}

void EditableCombobox::OnAccessibleNameChanged(const std::u16string& new_name) {
  textfield_->GetViewAccessibility().SetName(new_name);
  if (arrow_) {
    arrow_->GetViewAccessibility().SetName(new_name);
  }
}

void EditableCombobox::SetMenuDecorationStrategy(
    std::unique_ptr<MenuDecorationStrategy> strategy) {
  DCHECK(menu_model_);
  menu_model_->SetDecorationStrategy(std::move(strategy));
}

void EditableCombobox::UpdateMenu() {
  menu_model_->UpdateItemsShown();
}

gfx::Size EditableCombobox::CalculatePreferredSize(
    const SizeBounds& available_size) const {
  gfx::Size preferred_size = textfield_->GetPreferredSize({});
  preferred_size.SetToMax(control_elements_container_->GetPreferredSize({}));
  return preferred_size;
}

void EditableCombobox::RequestFocus() {
  textfield_->RequestFocus();
}

bool EditableCombobox::GetNeedsNotificationWhenVisibleBoundsChange() const {
  return true;
}

void EditableCombobox::OnVisibleBoundsChanged() {
  CloseMenu();
}

void EditableCombobox::ContentsChanged(Textfield* sender,
                                       const std::u16string& new_contents) {
  HandleNewContent(new_contents);
  ShowDropDownMenu(ui::MENU_SOURCE_KEYBOARD);
}

bool EditableCombobox::HandleKeyEvent(Textfield* sender,
                                      const ui::KeyEvent& key_event) {
  if (key_event.type() == ui::EventType::kKeyPressed &&
      (key_event.key_code() == ui::VKEY_UP ||
       key_event.key_code() == ui::VKEY_DOWN)) {
    ShowDropDownMenu(ui::MENU_SOURCE_KEYBOARD);
    return true;
  }
  return false;
}

void EditableCombobox::OnViewBlurred(View* observed_view) {
  CloseMenu();
}

void EditableCombobox::OnLayoutIsAnimatingChanged(
    views::AnimatingLayoutManager* source,
    bool is_animating) {
  dropdown_blocked_for_animation_ = is_animating;
  if (dropdown_blocked_for_animation_) {
    CloseMenu();
  }
}

// TODO(crbug.com/329471666): Refactor Textfield to obviate the need for this.
ProposedLayout EditableCombobox::CalculateProposedLayout(
    const SizeBounds& size_bounds) const {
  ProposedLayout layout;
  layout.host_size =
      gfx::Size(size_bounds.width().value(), size_bounds.height().value());
  layout.child_layouts.emplace_back(
      textfield_.get(), textfield_->GetVisible(),
      gfx::Rect(0, 0, layout.host_size.width(), layout.host_size.height()));
  const int preferred_width =
      control_elements_container_->GetPreferredSize({}).width();
  layout.child_layouts.emplace_back(
      control_elements_container_.get(),
      control_elements_container_->GetVisible(),
      gfx::Rect(layout.host_size.width() - preferred_width, 0, preferred_width,
                layout.host_size.height()));
  return layout;
}

bool EditableCombobox::ShouldApplyInkDropEffects() {
  return arrow_ && InkDrop::Get(arrow_) && GetWidget();
}

void EditableCombobox::CloseMenu() {
  if (ShouldApplyInkDropEffects()) {
    InkDrop::Get(arrow_)->AnimateToState(InkDropState::DEACTIVATED, nullptr);
    InkDrop::Get(arrow_)->GetInkDrop()->SetHovered(arrow_->IsMouseHovered());
  }
  menu_runner_.reset();
  pre_target_handler_.reset();
}

void EditableCombobox::OnItemSelected(size_t index) {
  std::u16string selected_item_text = menu_model_->GetItemTextAt(index);
  textfield_->SetText(selected_item_text);
  // SetText does not actually notify the TextfieldController, so we call the
  // handling code directly.
  HandleNewContent(selected_item_text);
}

void EditableCombobox::HandleNewContent(const std::u16string& new_content) {
  DCHECK(GetText() == new_content);
  // We notify |callback_| before updating |menu_model_|'s items shown. This
  // gives the user a chance to modify the ComboboxModel beforehand if they wish
  // to do so.
  // We disable UpdateItemsShown while we notify the listener in case it
  // modifies the ComboboxModel, then calls OnComboboxModelChanged and thus
  // UpdateItemsShown. That way UpdateItemsShown doesn't do its work twice.
  if (!content_changed_callback_.is_null()) {
    menu_model_->DisableUpdateItemsShown();
    content_changed_callback_.Run();
    menu_model_->EnableUpdateItemsShown();
  }
  UpdateMenu();
  GetViewAccessibility().SetValue(GetText());
}

void EditableCombobox::ArrowButtonPressed(const ui::Event& event) {
  textfield_->RequestFocus();
  if (ShouldApplyInkDropEffects()) {
    InkDrop::Get(arrow_)->AnimateToState(InkDropState::ACTIVATED, nullptr);
  }
  if (menu_runner_ && menu_runner_->IsRunning()) {
    CloseMenu();
  } else {
    ShowDropDownMenu(ui::GetMenuSourceTypeForEvent(event));
  }
}

void EditableCombobox::ShowDropDownMenu(ui::MenuSourceType source_type) {
  constexpr int kMenuBorderWidthTop = 1;

  if (dropdown_blocked_for_animation_) {
    return;
  }

  if (!menu_model_->GetItemCount()) {
    CloseMenu();
    return;
  }
  if (menu_runner_ && menu_runner_->IsRunning()) {
    return;
  }
  if (!GetWidget()) {
    return;
  }

  // Since we don't capture the mouse, we want to see the events that happen in
  // the EditableCombobox's RootView to get a chance to close the menu if they
  // happen outside |textfield_|. Events that happen over the menu belong to
  // another Widget and they don't go through this pre-target handler.
  // Events that happen outside both the menu and the RootView cause
  // OnViewBlurred to be called, which also closes the menu.
  pre_target_handler_ = std::make_unique<EditableComboboxPreTargetHandler>(
      this, GetWidget()->GetRootView());

  gfx::Rect local_bounds = textfield_->GetLocalBounds();

  // Menu's requested position's width should be the same as local bounds so the
  // border of the menu lines up with the border of the combobox. The y
  // coordinate however should be shifted to the bottom with the border width
  // not to overlap with the combobox border.
  gfx::Point menu_position(local_bounds.origin());
  menu_position.set_y(menu_position.y() + kMenuBorderWidthTop);
  View::ConvertPointToScreen(this, &menu_position);
  gfx::Rect bounds(menu_position, local_bounds.size());

  menu_runner_ = std::make_unique<MenuRunner>(
      menu_model_.get(), MenuRunner::EDITABLE_COMBOBOX,
      base::BindRepeating(&EditableCombobox::CloseMenu,
                          base::Unretained(this)));
  menu_runner_->RunMenuAt(GetWidget(), nullptr, bounds,
                          MenuAnchorPosition::kTopLeft, source_type);
}

void EditableCombobox::UpdateTextfieldInsets() {
  textfield_->SetExtraInsets(gfx::Insets::TLBR(
      0, 0, 0,
      std::max(control_elements_container_->GetPreferredSize({}).width() -
                   kComboboxArrowPaddingWidth,
               0)));
}

const ui::MenuModel* EditableCombobox::GetMenuModelForTesting() const {
  return menu_model_.get();
}

std::u16string EditableCombobox::GetItemTextForTesting(size_t index) const {
  return menu_model_->GetLabelAt(index);
}

const ui::ComboboxModel* EditableCombobox::GetComboboxModel() const {
  DCHECK(menu_model_);
  return menu_model_->GetComboboxModel();
}

BEGIN_METADATA(EditableCombobox)
ADD_PROPERTY_METADATA(std::u16string, Text)
ADD_PROPERTY_METADATA(std::u16string, PlaceholderText)
END_METADATA

}  // namespace views
