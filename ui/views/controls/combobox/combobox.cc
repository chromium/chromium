// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/combobox/combobox.h"

#include <memory>

#include "base/bind.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/models/combobox_model_observer.h"
#include "ui/base/models/menu_model.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/text_utils.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/controls/combobox/combobox_listener.h"
#include "ui/views/controls/combobox/combobox_util.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/focusable_border.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/prefix_selector.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/mouse_constants.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/style/typography.h"
#include "ui/views/widget/widget.h"

namespace views {

namespace {

// Used to indicate that no item is currently selected by the user.
constexpr int kNoSelection = -1;

SkColor GetTextColorForEnableState(const Combobox& combobox, bool enabled) {
  SkColor color =
      style::GetColor(combobox, style::CONTEXT_TEXTFIELD, style::STYLE_PRIMARY);
  if (!enabled)
    color = SkColorSetA(color, gfx::kDisabledControlAlpha);
  return color;
}

// The transparent button which holds a button state but is not rendered.
class TransparentButton : public Button {
 public:
  explicit TransparentButton(ButtonListener* listener) : Button(listener) {
    SetFocusBehavior(FocusBehavior::NEVER);
    button_controller()->set_notify_action(
        ButtonController::NotifyAction::kOnPress);

    SetInkDropMode(InkDropMode::ON);
    set_has_ink_drop_action_on_click(true);
  }
  ~TransparentButton() override = default;

  bool OnMousePressed(const ui::MouseEvent& mouse_event) override {
#if !defined(OS_MACOSX)
    // On Mac, comboboxes do not take focus on mouse click, but on other
    // platforms they do.
    parent()->RequestFocus();
#endif
    return Button::OnMousePressed(mouse_event);
  }

  double GetAnimationValue() const {
    return hover_animation().GetCurrentValue();
  }

  // Overridden from InkDropHost:
  std::unique_ptr<InkDrop> CreateInkDrop() override {
    std::unique_ptr<views::InkDropImpl> ink_drop = CreateDefaultInkDropImpl();
    ink_drop->SetShowHighlightOnHover(false);
    return std::move(ink_drop);
  }

  std::unique_ptr<InkDropRipple> CreateInkDropRipple() const override {
    return std::unique_ptr<views::InkDropRipple>(
        new views::FloodFillInkDropRipple(
            size(), GetInkDropCenterBasedOnLastEvent(),
            GetNativeTheme()->GetSystemColor(
                ui::NativeTheme::kColorId_LabelEnabledColor),
            ink_drop_visible_opacity()));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TransparentButton);
};

#if !defined(OS_MACOSX)
// Returns the next or previous valid index (depending on |increment|'s value).
// Skips separator or disabled indices. Returns -1 if there is no valid adjacent
// index.
int GetAdjacentIndex(ui::ComboboxModel* model, int increment, int index) {
  DCHECK(increment == -1 || increment == 1);

  index += increment;
  while (index >= 0 && index < model->GetItemCount()) {
    if (!model->IsItemSeparatorAt(index) || !model->IsItemEnabledAt(index))
      return index;
    index += increment;
  }
  return kNoSelection;
}
#endif

}  // namespace

// Adapts a ui::ComboboxModel to a ui::MenuModel.
class Combobox::ComboboxMenuModel : public ui::MenuModel {
 public:
  ComboboxMenuModel(Combobox* owner, ui::ComboboxModel* model)
      : owner_(owner), model_(model) {}
  ~ComboboxMenuModel() override = default;

 private:
  bool UseCheckmarks() const {
    return MenuConfig::instance().check_selected_combobox_item;
  }

  // Overridden from MenuModel:
  bool HasIcons() const override { return false; }

  int GetItemCount() const override { return model_->GetItemCount(); }

  ItemType GetTypeAt(int index) const override {
    if (model_->IsItemSeparatorAt(index))
      return TYPE_SEPARATOR;
    return UseCheckmarks() ? TYPE_CHECK : TYPE_COMMAND;
  }

  ui::MenuSeparatorType GetSeparatorTypeAt(int index) const override {
    return ui::NORMAL_SEPARATOR;
  }

  int GetCommandIdAt(int index) const override {
    // Define the id of the first item in the menu (since it needs to be > 0)
    constexpr int kFirstMenuItemId = 1000;
    return index + kFirstMenuItemId;
  }

  base::string16 GetLabelAt(int index) const override {
    // Inserting the Unicode formatting characters if necessary so that the
    // text is displayed correctly in right-to-left UIs.
    base::string16 text = model_->GetItemAt(index);
    base::i18n::AdjustStringForLocaleDirection(&text);
    return text;
  }

  bool IsItemDynamicAt(int index) const override { return true; }

  const gfx::FontList* GetLabelFontListAt(int index) const override {
    return &owner_->GetFontList();
  }

  bool GetAcceleratorAt(int index,
                        ui::Accelerator* accelerator) const override {
    return false;
  }

  bool IsItemCheckedAt(int index) const override {
    return UseCheckmarks() && index == owner_->selected_index_;
  }

  int GetGroupIdAt(int index) const override { return -1; }

  bool GetIconAt(int index, gfx::Image* icon) const override { return false; }

  ui::ButtonMenuItemModel* GetButtonMenuItemAt(int index) const override {
    return nullptr;
  }

  bool IsEnabledAt(int index) const override {
    return model_->IsItemEnabledAt(index);
  }

  void ActivatedAt(int index) override {
    owner_->selected_index_ = index;
    owner_->OnPerformAction();
  }

  void ActivatedAt(int index, int event_flags) override { ActivatedAt(index); }

  MenuModel* GetSubmenuModelAt(int index) const override { return nullptr; }

  Combobox* owner_;           // Weak. Owns this.
  ui::ComboboxModel* model_;  // Weak.

  DISALLOW_COPY_AND_ASSIGN(ComboboxMenuModel);
};

////////////////////////////////////////////////////////////////////////////////
// Combobox, public:

Combobox::Combobox(std::unique_ptr<ui::ComboboxModel> model,
                   int text_context,
                   int text_style)
    : Combobox(model.get(), text_context, text_style) {
  owned_model_ = std::move(model);
}

Combobox::Combobox(ui::ComboboxModel* model, int text_context, int text_style)
    : model_(model),
      text_context_(text_context),
      text_style_(text_style),
      listener_(nullptr),
      selected_index_(model_->GetDefaultIndex()),
      invalid_(false),
      menu_model_(new ComboboxMenuModel(this, model)),
      arrow_button_(new TransparentButton(this)),
      size_to_largest_label_(true) {
  model_->AddObserver(this);
  OnComboboxModelChanged(model_);
#if defined(OS_MACOSX)
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
#else
  SetFocusBehavior(FocusBehavior::ALWAYS);
#endif

  UpdateBorder();

  arrow_button_->SetVisible(true);
  AddChildView(arrow_button_);

  // A layer is applied to make sure that canvas bounds are snapped to pixel
  // boundaries (for the sake of drawing the arrow).
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  focus_ring_ = FocusRing::Install(this);
}

Combobox::~Combobox() {
  if (GetInputMethod() && selector_.get()) {
    // Combobox should have been blurred before destroy.
    DCHECK(selector_.get() != GetInputMethod()->GetTextInputClient());
  }
  model_->RemoveObserver(this);
}

const gfx::FontList& Combobox::GetFontList() const {
  return style::GetFont(text_context_, text_style_);
}

void Combobox::SetSelectedIndex(int index) {
  if (selected_index_ == index)
    return;

  selected_index_ = index;
  if (size_to_largest_label_) {
    OnPropertyChanged(&selected_index_, kPropertyEffectsPaint);
  } else {
    content_size_ = GetContentSize();
    OnPropertyChanged(&selected_index_, kPropertyEffectsPreferredSizeChanged);
  }
}

bool Combobox::SelectValue(const base::string16& value) {
  for (int i = 0; i < model()->GetItemCount(); ++i) {
    if (value == model()->GetItemAt(i)) {
      SetSelectedIndex(i);
      return true;
    }
  }
  return false;
}

void Combobox::SetTooltipText(const base::string16& tooltip_text) {
  arrow_button_->SetTooltipText(tooltip_text);
  if (accessible_name_.empty())
    accessible_name_ = tooltip_text;
}

void Combobox::SetAccessibleName(const base::string16& name) {
  accessible_name_ = name;
}

base::string16 Combobox::GetAccessibleName() const {
  return accessible_name_;
}

void Combobox::SetInvalid(bool invalid) {
  if (invalid == invalid_)
    return;

  invalid_ = invalid;

  if (focus_ring_)
    focus_ring_->SetInvalid(invalid);

  UpdateBorder();
  OnPropertyChanged(&selected_index_, kPropertyEffectsPaint);
}

void Combobox::OnThemeChanged() {
  SetBackground(
      CreateBackgroundFromPainter(Painter::CreateSolidRoundRectPainter(
          GetNativeTheme()->GetSystemColor(
              ui::NativeTheme::kColorId_TextfieldDefaultBackground),
          FocusableBorder::kCornerRadiusDp)));
}

int Combobox::GetRowCount() {
  return model()->GetItemCount();
}

int Combobox::GetSelectedRow() {
  return selected_index_;
}

void Combobox::SetSelectedRow(int row) {
  int prev_index = selected_index_;
  SetSelectedIndex(row);
  if (selected_index_ != prev_index)
    OnPerformAction();
}

base::string16 Combobox::GetTextForRow(int row) {
  return model()->IsItemSeparatorAt(row) ? base::string16() :
                                           model()->GetItemAt(row);
}

////////////////////////////////////////////////////////////////////////////////
// Combobox, View overrides:

gfx::Size Combobox::CalculatePreferredSize() const {
  // Limit how small a combobox can be.
  constexpr int kMinComboboxWidth = 25;

  // The preferred size will drive the local bounds which in turn is used to set
  // the minimum width for the dropdown list.
  gfx::Insets insets = GetInsets();
  const LayoutProvider* provider = LayoutProvider::Get();
  insets += gfx::Insets(
      provider->GetDistanceMetric(DISTANCE_CONTROL_VERTICAL_TEXT_PADDING),
      provider->GetDistanceMetric(DISTANCE_TEXTFIELD_HORIZONTAL_TEXT_PADDING));
  int total_width = std::max(kMinComboboxWidth, content_size_.width()) +
                    insets.width() + kComboboxArrowContainerWidth;
  return gfx::Size(total_width, content_size_.height() + insets.height());
}

void Combobox::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  arrow_button_->SetBounds(0, 0, width(), height());
}

bool Combobox::SkipDefaultKeyEventProcessing(const ui::KeyEvent& e) {
  // Escape should close the drop down list when it is active, not host UI.
  if (e.key_code() != ui::VKEY_ESCAPE ||
      e.IsShiftDown() || e.IsControlDown() || e.IsAltDown()) {
    return false;
  }
  return !!menu_runner_;
}

bool Combobox::OnKeyPressed(const ui::KeyEvent& e) {
  // TODO(oshima): handle IME.
  DCHECK_EQ(e.type(), ui::ET_KEY_PRESSED);

  DCHECK_GE(selected_index_, 0);
  DCHECK_LT(selected_index_, model()->GetItemCount());
  if (selected_index_ < 0 || selected_index_ > model()->GetItemCount())
    selected_index_ = 0;

  bool show_menu = false;
  int new_index = kNoSelection;
  switch (e.key_code()) {
#if defined(OS_MACOSX)
    case ui::VKEY_DOWN:
    case ui::VKEY_UP:
    case ui::VKEY_SPACE:
    case ui::VKEY_HOME:
    case ui::VKEY_END:
      // On Mac, navigation keys should always just show the menu first.
      show_menu = true;
      break;
#else
    // Show the menu on F4 without modifiers.
    case ui::VKEY_F4:
      if (e.IsAltDown() || e.IsAltGrDown() || e.IsControlDown())
        return false;
      show_menu = true;
      break;

    // Move to the next item if any, or show the menu on Alt+Down like Windows.
    case ui::VKEY_DOWN:
      if (e.IsAltDown())
        show_menu = true;
      else
        new_index = GetAdjacentIndex(model(), 1, selected_index_);
      break;

    // Move to the end of the list.
    case ui::VKEY_END:
    case ui::VKEY_NEXT:  // Page down.
      new_index = GetAdjacentIndex(model(), -1, model()->GetItemCount());
      break;

    // Move to the beginning of the list.
    case ui::VKEY_HOME:
    case ui::VKEY_PRIOR:  // Page up.
      new_index = GetAdjacentIndex(model(), 1, -1);
      break;

    // Move to the previous item if any.
    case ui::VKEY_UP:
      new_index = GetAdjacentIndex(model(), -1, selected_index_);
      break;

    case ui::VKEY_RETURN:
    case ui::VKEY_SPACE:
      show_menu = true;
      break;
#endif  // OS_MACOSX
    default:
      return false;
  }

  if (show_menu) {
    ShowDropDownMenu(ui::MENU_SOURCE_KEYBOARD);
  } else if (new_index != selected_index_ && new_index != kNoSelection) {
    DCHECK(!model()->IsItemSeparatorAt(new_index));
    selected_index_ = new_index;
    OnPerformAction();
  }

  return true;
}

void Combobox::OnPaint(gfx::Canvas* canvas) {
  OnPaintBackground(canvas);
  PaintText(canvas);
  OnPaintBorder(canvas);
}

void Combobox::OnFocus() {
  if (GetInputMethod())
    GetInputMethod()->SetFocusedTextInputClient(GetPrefixSelector());

  View::OnFocus();
  // Border renders differently when focused.
  SchedulePaint();
}

void Combobox::OnBlur() {
  if (GetInputMethod())
    GetInputMethod()->DetachTextInputClient(GetPrefixSelector());

  if (selector_)
    selector_->OnViewBlur();
  // Border renders differently when focused.
  SchedulePaint();
}

void Combobox::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  // ax::mojom::Role::kComboBox is for UI elements with a dropdown and
  // an editable text field, which views::Combobox does not have. Use
  // ax::mojom::Role::kPopUpButton to match an HTML <select> element.
  node_data->role = ax::mojom::Role::kPopUpButton;

  node_data->SetName(accessible_name_);
  node_data->SetValue(model_->GetItemAt(selected_index_));
  if (GetEnabled()) {
    node_data->SetDefaultActionVerb(ax::mojom::DefaultActionVerb::kOpen);
  }
  node_data->AddIntAttribute(ax::mojom::IntAttribute::kPosInSet,
                             selected_index_);
  node_data->AddIntAttribute(ax::mojom::IntAttribute::kSetSize,
                             model_->GetItemCount());
}

bool Combobox::HandleAccessibleAction(const ui::AXActionData& action_data) {
  // The action handling in View would generate a mouse event and send it to
  // |this|. However, mouse events for Combobox are handled by |arrow_button_|,
  // which is hidden from the a11y tree (so can't expose actions). Rather than
  // forwarding ax::mojom::Action::kDoDefault to View and then forwarding the
  // mouse event it generates to |arrow_button_| to have it forward back to
  // |this| (as its ButtonListener), just handle the action explicitly here and
  // bypass View.
  if (GetEnabled() && action_data.action == ax::mojom::Action::kDoDefault) {
    ShowDropDownMenu(ui::MENU_SOURCE_KEYBOARD);
    return true;
  }
  return View::HandleAccessibleAction(action_data);
}

void Combobox::ButtonPressed(Button* sender, const ui::Event& event) {
  if (!GetEnabled())
    return;

  // TODO(hajimehoshi): Fix the problem that the arrow button blinks when
  // cliking this while the dropdown menu is opened.
  const base::TimeDelta delta = base::TimeTicks::Now() - closed_time_;
  if (delta <= kMinimumTimeBetweenButtonClicks)
    return;

  ui::MenuSourceType source_type = ui::MENU_SOURCE_MOUSE;
  if (event.IsKeyEvent())
    source_type = ui::MENU_SOURCE_KEYBOARD;
  else if (event.IsGestureEvent() || event.IsTouchEvent())
    source_type = ui::MENU_SOURCE_TOUCH;
  ShowDropDownMenu(source_type);
}

void Combobox::OnComboboxModelChanged(ui::ComboboxModel* model) {
  DCHECK_EQ(model_, model);

  // If the selection is no longer valid (or the model is empty), restore the
  // default index.
  if (selected_index_ >= model_->GetItemCount() ||
      model_->GetItemCount() == 0 ||
      model_->IsItemSeparatorAt(selected_index_)) {
    selected_index_ = model_->GetDefaultIndex();
  }

  content_size_ = GetContentSize();
  PreferredSizeChanged();
  SchedulePaint();
}

void Combobox::UpdateBorder() {
  std::unique_ptr<FocusableBorder> border(new FocusableBorder());
  if (invalid_)
    border->SetColorId(ui::NativeTheme::kColorId_AlertSeverityHigh);
  SetBorder(std::move(border));
}

void Combobox::AdjustBoundsForRTLUI(gfx::Rect* rect) const {
  rect->set_x(GetMirroredXForRect(*rect));
}

void Combobox::PaintText(gfx::Canvas* canvas) {
  gfx::Insets insets = GetInsets();
  insets += gfx::Insets(0, LayoutProvider::Get()->GetDistanceMetric(
                               DISTANCE_TEXTFIELD_HORIZONTAL_TEXT_PADDING));

  gfx::ScopedCanvas scoped_canvas(canvas);
  canvas->ClipRect(GetContentsBounds());

  int x = insets.left();
  int y = insets.top();
  int text_height = height() - insets.height();
  SkColor text_color = GetTextColorForEnableState(*this, GetEnabled());
  DCHECK_GE(selected_index_, 0);
  DCHECK_LT(selected_index_, model()->GetItemCount());
  if (selected_index_ < 0 || selected_index_ > model()->GetItemCount())
    selected_index_ = 0;
  base::string16 text = model()->GetItemAt(selected_index_);

  int disclosure_arrow_offset = width() - kComboboxArrowContainerWidth;

  const gfx::FontList& font_list = GetFontList();
  int text_width = gfx::GetStringWidth(text, font_list);
  if ((text_width + insets.width()) > disclosure_arrow_offset)
    text_width = disclosure_arrow_offset - insets.width();

  gfx::Rect text_bounds(x, y, text_width, text_height);
  AdjustBoundsForRTLUI(&text_bounds);
  canvas->DrawStringRect(text, font_list, text_color, text_bounds);

  gfx::Rect arrow_bounds(disclosure_arrow_offset, 0,
                         kComboboxArrowContainerWidth, height());
  arrow_bounds.ClampToCenteredSize(ComboboxArrowSize());
  AdjustBoundsForRTLUI(&arrow_bounds);

  PaintComboboxArrow(text_color, arrow_bounds, canvas);
}

void Combobox::ShowDropDownMenu(ui::MenuSourceType source_type) {
  // Menu border widths.
  constexpr int kMenuBorderWidthLeft = 1;
  constexpr int kMenuBorderWidthTop = 1;
  constexpr int kMenuBorderWidthRight = 1;

  gfx::Rect lb = GetLocalBounds();
  gfx::Point menu_position(lb.origin());

  // Inset the menu's requested position so the border of the menu lines up
  // with the border of the combobox.
  menu_position.set_x(menu_position.x() + kMenuBorderWidthLeft);
  menu_position.set_y(menu_position.y() + kMenuBorderWidthTop);

  lb.set_width(lb.width() - (kMenuBorderWidthLeft + kMenuBorderWidthRight));

  View::ConvertPointToScreen(this, &menu_position);

  gfx::Rect bounds(menu_position, lb.size());

  Button::ButtonState original_state = arrow_button_->state();
  arrow_button_->SetState(Button::STATE_PRESSED);

  // Allow |menu_runner_| to be set by the testing API, but if this method is
  // ever invoked recursively, ensure the old menu is closed.
  if (!menu_runner_ || menu_runner_->IsRunning()) {
    menu_runner_ = std::make_unique<MenuRunner>(
        menu_model_.get(), MenuRunner::COMBOBOX,
        base::BindRepeating(&Combobox::OnMenuClosed, base::Unretained(this),
                            original_state));
  }
  menu_runner_->RunMenuAt(GetWidget(), nullptr, bounds,
                          MenuAnchorPosition::kTopLeft, source_type);
}

void Combobox::OnMenuClosed(Button::ButtonState original_button_state) {
  menu_runner_.reset();
  arrow_button_->SetState(original_button_state);
  closed_time_ = base::TimeTicks::Now();
}

void Combobox::OnPerformAction() {
  NotifyAccessibilityEvent(ax::mojom::Event::kValueChanged, true);
  SchedulePaint();

  if (listener_)
    listener_->OnPerformAction(this);

  // Note |this| may be deleted by |listener_|.
}

gfx::Size Combobox::GetContentSize() const {
  const gfx::FontList& font_list = GetFontList();

  int width = 0;
  for (int i = 0; i < model()->GetItemCount(); ++i) {
    if (model_->IsItemSeparatorAt(i))
      continue;

    if (size_to_largest_label_ || i == selected_index_) {
      width = std::max(
          width, gfx::GetStringWidth(menu_model_->GetLabelAt(i), font_list));
    }
  }
  return gfx::Size(width, font_list.GetHeight());
}

PrefixSelector* Combobox::GetPrefixSelector() {
  if (!selector_)
    selector_ = std::make_unique<PrefixSelector>(this, this);
  return selector_.get();
}

BEGIN_METADATA(Combobox)
METADATA_PARENT_CLASS(View)
ADD_PROPERTY_METADATA(Combobox, int, SelectedIndex)
ADD_PROPERTY_METADATA(Combobox, bool, Invalid)
ADD_PROPERTY_METADATA(Combobox, base::string16, AccessibleName)
END_METADATA()

}  // namespace views
