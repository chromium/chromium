// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/combobox/combobox.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/menu_source_utils.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_types.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/controls/combobox/combobox_menu_model.h"
#include "ui/views/controls/combobox/combobox_util.h"
#include "ui/views/controls/combobox/empty_combobox_model.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/focusable_border.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/prefix_selector.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/mouse_constants.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace views {

namespace {

constexpr int kBorderThickness = 1;

float GetCornerRadius() {
  return LayoutProvider::Get()->GetCornerRadiusMetric(
      ShapeContextTokens::kComboboxRadius);
}

SkColor GetTextColorForEnableState(const Combobox& combobox, bool enabled) {
  const int style = enabled ? style::STYLE_PRIMARY : style::STYLE_DISABLED;
  return combobox.GetColorProvider()->GetColor(
      TypographyProvider::Get().GetColorId(style::CONTEXT_TEXTFIELD, style));
}

// The transparent button which holds a button state but is not rendered.
class TransparentButton : public Button {
  METADATA_HEADER(TransparentButton, Button)

 public:
  explicit TransparentButton(PressedCallback callback)
      : Button(std::move(callback)) {
    SetFocusBehavior(FocusBehavior::NEVER);
    button_controller()->set_notify_action(
        ButtonController::NotifyAction::kOnPress);

    views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                  GetCornerRadius());
    ConfigureComboboxButtonInkDrop(this);
  }
  TransparentButton(const TransparentButton&) = delete;
  TransparentButton& operator&=(const TransparentButton&) = delete;
  ~TransparentButton() override = default;

  bool OnMousePressed(const ui::MouseEvent& mouse_event) override {
#if !BUILDFLAG(IS_MAC)
    // On Mac, comboboxes do not take focus on mouse click, but on other
    // platforms they do.
    parent()->RequestFocus();
#endif
    return Button::OnMousePressed(mouse_event);
  }

  double GetAnimationValue() const {
    return hover_animation().GetCurrentValue();
  }

  void UpdateInkDrop(bool show_on_press_and_hover) {
    if (show_on_press_and_hover) {
      // We must use UseInkDropForFloodFillRipple here because
      // UseInkDropForSquareRipple hides the InkDrop when the ripple effect is
      // active instead of layering underneath it causing flashing.
      InkDrop::UseInkDropForFloodFillRipple(InkDrop::Get(this),
                                            /*highlight_on_hover=*/true);
    } else {
      InkDrop::UseInkDropForSquareRipple(InkDrop::Get(this),
                                         /*highlight_on_hover=*/false);
    }
  }
};

BEGIN_METADATA(TransparentButton)
END_METADATA

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// Combobox, public:

Combobox::Combobox()
    : Combobox(std::make_unique<internal::EmptyComboboxModel>()) {}

Combobox::Combobox(std::unique_ptr<ui::ComboboxModel> model)
    : Combobox(model.get()) {
  owned_model_ = std::move(model);
}

Combobox::Combobox(ui::ComboboxModel* model) {
  SetModel(model);
#if BUILDFLAG(IS_MAC)
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
#else
  SetFocusBehavior(FocusBehavior::ALWAYS);
#endif

  SetBackgroundColorId(ui::kColorComboboxBackground);

  UpdateBorder();

  FocusRing::Install(this);
  views::FocusRing::Get(this)->SetOutsetFocusRingDisabled(true);

  arrow_button_ =
      AddChildView(std::make_unique<TransparentButton>(base::BindRepeating(
          &Combobox::ArrowButtonPressed, base::Unretained(this))));

  // TODO(crbug.com/40250124): This setter should be removed and the behavior
  // made default when ChromeRefresh2023 is finalized.
  SetEventHighlighting(true);
  enabled_changed_subscription_ = AddEnabledChangedCallback(base::BindRepeating(
      [](Combobox* combobox) {
        combobox->SetBackgroundColorId(
            combobox->GetEnabled() ? ui::kColorComboboxBackground
                                   : ui::kColorComboboxBackgroundDisabled);
        combobox->UpdateBorder();
        combobox->UpdateAccessibleDefaultActionVerb();
      },
      base::Unretained(this)));

  // A layer is applied to make sure that canvas bounds are snapped to pixel
  // boundaries (for the sake of drawing the arrow).
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                GetCornerRadius());

  GetViewAccessibility().SetRole(ax::mojom::Role::kComboBoxSelect);

  UpdateAccessibleValue();
  UpdateExpandedCollapsedAccessibleState();
  UpdateAccessibleDefaultActionVerb();
}

Combobox::~Combobox() {
  if (GetInputMethod() && selector_.get()) {
    // Combobox should have been blurred before destroy.
    DCHECK(selector_.get() != GetInputMethod()->GetTextInputClient());
  }
}

const gfx::FontList& Combobox::GetFontList() const {
  return TypographyProvider::Get().GetFont(kContext, kStyle);
}

void Combobox::SetSelectedIndex(std::optional<size_t> index) {
  if (selected_index_ == index)
    return;
  // TODO(pbos): Add (D)CHECKs to validate the selected index.
  selected_index_ = index;

  if (selected_index_.has_value()) {
    GetViewAccessibility().SetPosInSet(
        base::checked_cast<int>(selected_index_.value()));
  }

  if (size_to_largest_label_) {
    OnPropertyChanged(&selected_index_, kPropertyEffectsPaint);
  } else {
    content_size_ = GetContentSize();
    OnPropertyChanged(&selected_index_, kPropertyEffectsPreferredSizeChanged);
  }

  UpdateAccessibleValue();
}

base::CallbackListSubscription Combobox::AddSelectedIndexChangedCallback(
    views::PropertyChangedCallback callback) {
  return AddPropertyChangedCallback(&selected_index_, std::move(callback));
}

bool Combobox::SelectValue(const std::u16string& value) {
  for (size_t i = 0; i < GetModel()->GetItemCount(); ++i) {
    if (value == GetModel()->GetItemAt(i)) {
      SetSelectedIndex(i);
      return true;
    }
  }
  return false;
}

void Combobox::SetOwnedModel(std::unique_ptr<ui::ComboboxModel> model) {
  // The swap keeps the outgoing model alive for SetModel().
  owned_model_.swap(model);
  SetModel(owned_model_.get());
}

void Combobox::SetModel(ui::ComboboxModel* model) {
  if (!model) {
    SetOwnedModel(std::make_unique<internal::EmptyComboboxModel>());
    return;
  }

  if (model_) {
    DCHECK(observation_.IsObservingSource(model_.get()));
    observation_.Reset();
  }

  model_ = model;

  if (model_) {
    model_ = model;
    menu_model_ = std::make_unique<ComboboxMenuModel>(this, model_);
    observation_.Observe(model_.get());
    SetSelectedIndex(model_->GetDefaultIndex());
    OnComboboxModelChanged(model_);
  }
}

std::u16string Combobox::GetTooltipTextAndAccessibleName() const {
  return arrow_button_->GetTooltipText();
}

void Combobox::SetTooltipTextAndAccessibleName(
    const std::u16string& tooltip_text) {
  arrow_button_->SetTooltipText(tooltip_text);
  if (GetViewAccessibility().GetCachedName().empty()) {
    GetViewAccessibility().SetName(tooltip_text);
  }
}

void Combobox::SetInvalid(bool invalid) {
  if (invalid == invalid_)
    return;

  invalid_ = invalid;

  if (views::FocusRing::Get(this))
    views::FocusRing::Get(this)->SetInvalid(invalid);

  UpdateBorder();
  OnPropertyChanged(&selected_index_, kPropertyEffectsPaint);
}

void Combobox::SetBorderColorId(ui::ColorId color_id) {
  border_color_id_ = color_id;
  UpdateBorder();
}

void Combobox::SetBackgroundColorId(ui::ColorId color_id) {
  SetBackground(CreateThemedRoundedRectBackground(color_id, GetCornerRadius()));
}

void Combobox::SetForegroundColorId(ui::ColorId color_id) {
  foreground_color_id_ = color_id;
  SchedulePaint();
}

void Combobox::SetForegroundIconColorId(ui::ColorId color_id) {
  foreground_icon_color_id_ = color_id;
  SchedulePaint();
}

void Combobox::SetForegroundTextStyle(style::TextStyle text_style) {
  foreground_text_style_ = text_style;
  SchedulePaint();
}

void Combobox::SetEventHighlighting(bool should_highlight) {
  should_highlight_ = should_highlight;
  AsViewClass<TransparentButton>(arrow_button_)
      ->UpdateInkDrop(should_highlight);
}

void Combobox::SetSizeToLargestLabel(bool size_to_largest_label) {
  if (size_to_largest_label_ == size_to_largest_label)
    return;

  size_to_largest_label_ = size_to_largest_label;
  content_size_ = GetContentSize();
  OnPropertyChanged(&selected_index_, kPropertyEffectsPreferredSizeChanged);
}

bool Combobox::IsMenuRunning() const {
  return menu_runner_ && menu_runner_->IsRunning();
}

void Combobox::OnThemeChanged() {
  View::OnThemeChanged();
  OnContentSizeMaybeChanged();
}

size_t Combobox::GetRowCount() {
  return GetModel()->GetItemCount();
}

std::optional<size_t> Combobox::GetSelectedRow() {
  return selected_index_;
}

void Combobox::SetSelectedRow(std::optional<size_t> row) {
  std::optional<size_t> prev_index = selected_index_;
  SetSelectedIndex(row);
  if (selected_index_ != prev_index)
    OnPerformAction();
}

std::u16string Combobox::GetTextForRow(size_t row) {
  return GetModel()->IsItemSeparatorAt(row) ? std::u16string()
                                            : GetModel()->GetItemAt(row);
}

base::CallbackListSubscription Combobox::AddMenuWillShowCallback(
    MenuWillShowCallback callback) {
  return on_menu_will_show_.Add(std::move(callback));
}

////////////////////////////////////////////////////////////////////////////////
// Combobox, View overrides:

gfx::Size Combobox::CalculatePreferredSize(
    const SizeBounds& /*available_size*/) const {
  // Limit how small a combobox can be.
  constexpr int kMinComboboxWidth = 25;

  // The preferred size will drive the local bounds which in turn is used to set
  // the minimum width for the dropdown list.
  int width = std::max(kMinComboboxWidth, content_size_.width()) +
              LayoutProvider::Get()->GetDistanceMetric(
                  DISTANCE_TEXTFIELD_HORIZONTAL_TEXT_PADDING) *
                  2 +
              GetInsets().width();

  // If an arrow is being shown, add extra width to include that arrow.
  if (should_show_arrow_) {
    width += GetComboboxArrowContainerWidthAndMargins();
  }

  return gfx::Size(width, LayoutProvider::GetControlHeightForFont(
                              kContext, kStyle, GetForegroundFontList()));
}

void Combobox::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  arrow_button_->SetBounds(0, 0, width(), height());
}

bool Combobox::SkipDefaultKeyEventProcessing(const ui::KeyEvent& e) {
  // Escape should close the drop down list when it is active, not host UI.
  if (e.key_code() != ui::VKEY_ESCAPE || e.IsShiftDown() || e.IsControlDown() ||
      e.IsAltDown() || e.IsAltGrDown()) {
    return false;
  }
  return !!menu_runner_;
}

bool Combobox::OnKeyPressed(const ui::KeyEvent& e) {
  // TODO(oshima): handle IME.
  CHECK_EQ(e.type(), ui::EventType::kKeyPressed);

  if (!selected_index_.has_value()) {
    CHECK_EQ(model_->GetItemCount(), 0u);
    return false;
  }
  CHECK_LT(selected_index_.value(), GetModel()->GetItemCount());

#if BUILDFLAG(IS_MAC)
  if (e.key_code() != ui::VKEY_DOWN && e.key_code() != ui::VKEY_UP &&
      e.key_code() != ui::VKEY_SPACE && e.key_code() != ui::VKEY_HOME &&
      e.key_code() != ui::VKEY_END) {
    return false;
  }
  ShowDropDownMenu(ui::MENU_SOURCE_KEYBOARD);
  return true;
#else
  const auto index_at_or_after = [](ui::ComboboxModel* model,
                                    size_t index) -> std::optional<size_t> {
    for (; index < model->GetItemCount(); ++index) {
      if (!model->IsItemSeparatorAt(index) && model->IsItemEnabledAt(index))
        return index;
    }
    return std::nullopt;
  };
  const auto index_before = [](ui::ComboboxModel* model,
                               size_t index) -> std::optional<size_t> {
    for (; index > 0; --index) {
      const auto prev = index - 1;
      if (!model->IsItemSeparatorAt(prev) && model->IsItemEnabledAt(prev))
        return prev;
    }
    return std::nullopt;
  };

  std::optional<size_t> new_index;
  switch (e.key_code()) {
    // Show the menu on F4 without modifiers.
    case ui::VKEY_F4:
      if (e.IsAltDown() || e.IsAltGrDown() || e.IsControlDown())
        return false;
      ShowDropDownMenu(ui::MENU_SOURCE_KEYBOARD);
      return true;

    // Move to the next item if any, or show the menu on Alt+Down like Windows.
    case ui::VKEY_DOWN:
      if (e.IsAltDown()) {
        ShowDropDownMenu(ui::MENU_SOURCE_KEYBOARD);
        return true;
      }
      new_index = index_at_or_after(GetModel(), selected_index_.value() + 1);
      break;

    // Move to the end of the list.
    case ui::VKEY_END:
    case ui::VKEY_NEXT:  // Page down.
      new_index = index_before(GetModel(), GetModel()->GetItemCount());
      break;

    // Move to the beginning of the list.
    case ui::VKEY_HOME:
    case ui::VKEY_PRIOR:  // Page up.
      new_index = index_at_or_after(GetModel(), 0);
      break;

    // Move to the previous item if any.
    case ui::VKEY_UP:
      new_index = index_before(GetModel(), selected_index_.value());
      break;

    case ui::VKEY_RETURN:
    case ui::VKEY_SPACE:
      ShowDropDownMenu(ui::MENU_SOURCE_KEYBOARD);
      return true;

    default:
      return false;
  }

  if (new_index.has_value()) {
    SetSelectedIndex(new_index);
    OnPerformAction();
  }
  return true;
#endif  // BUILDFLAG(IS_MAC)
}

void Combobox::OnPaint(gfx::Canvas* canvas) {
  OnPaintBackground(canvas);
  PaintIconAndText(canvas);
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

bool Combobox::HandleAccessibleAction(const ui::AXActionData& action_data) {
  // The action handling in View would generate a mouse event and send it to
  // |this|. However, mouse events for Combobox are handled by |arrow_button_|,
  // which is hidden from the a11y tree (so can't expose actions). Rather than
  // forwarding ax::mojom::Action::kDoDefault to View and then forwarding the
  // mouse event it generates to |arrow_button_| to have it forward back to the
  // callback on |this|, just handle the action explicitly here and bypass View.
  if (GetEnabled() && action_data.action == ax::mojom::Action::kDoDefault) {
    ShowDropDownMenu(ui::MENU_SOURCE_KEYBOARD);
    return true;
  }
  return View::HandleAccessibleAction(action_data);
}

void Combobox::OnComboboxModelChanged(ui::ComboboxModel* model) {
  DCHECK_EQ(model_, model);

  if (IsMenuRunning()) {
    menu_runner_.reset();
    UpdateExpandedCollapsedAccessibleState();
  }

  // If the selection is no longer valid (or the model is empty), restore the
  // default index.
  if (!selected_index_.has_value() ||
      selected_index_ >= model_->GetItemCount() ||
      model_->GetItemCount() == 0 ||
      model_->IsItemSeparatorAt(selected_index_.value())) {
    SetSelectedIndex(model_->GetDefaultIndex());
  }

  OnContentSizeMaybeChanged();
  SchedulePaint();

  GetViewAccessibility().SetSetSize(
      base::checked_cast<int>(model_->GetItemCount()));
}

void Combobox::OnComboboxModelDestroying(ui::ComboboxModel* model) {
  SetModel(nullptr);
}

const base::RepeatingClosure& Combobox::GetCallback() const {
  return callback_;
}

const std::unique_ptr<ui::ComboboxModel>& Combobox::GetOwnedModel() const {
  return owned_model_;
}

void Combobox::UpdateBorder() {
  if (!GetEnabled()) {
    SetBorder(nullptr);
    return;
  }
  SetBorder(CreateThemedRoundedRectBorder(
      kBorderThickness, GetCornerRadius(),
      invalid_
          ? ui::kColorAlertHighSeverity
          : border_color_id_.value_or(ui::kColorComboboxContainerOutline)));
}

void Combobox::AdjustBoundsForRTLUI(gfx::Rect* rect) const {
  rect->set_x(GetMirroredXForRect(*rect));
}

void Combobox::PaintIconAndText(gfx::Canvas* canvas) {
  if (!selected_index_.has_value()) {
    return;
  }
  CHECK_LT(selected_index_.value(), GetModel()->GetItemCount());

  gfx::Insets insets = GetInsets();
  insets += gfx::Insets::VH(0, LayoutProvider::Get()->GetDistanceMetric(
                                   DISTANCE_TEXTFIELD_HORIZONTAL_TEXT_PADDING));

  gfx::ScopedCanvas scoped_canvas(canvas);
  canvas->ClipRect(GetContentsBounds());

  int x = insets.left();
  int y = insets.top();
  int contents_height = height() - insets.height();

  // Draw the icon.
  ui::ImageModel icon = GetModel()->GetIconAt(selected_index_.value());
  if (!icon.IsEmpty()) {
    // Update the icon color if provided and if the icon color can be changed.
    if (foreground_icon_color_id_ && icon.IsVectorIcon()) {
      icon = ui::ImageModel::FromVectorIcon(*icon.GetVectorIcon().vector_icon(),
                                            foreground_icon_color_id_.value(),
                                            icon.GetVectorIcon().icon_size());
    }
    gfx::ImageSkia icon_skia = icon.Rasterize(GetColorProvider());
    int icon_y = y + (contents_height - icon_skia.height()) / 2;
    gfx::Rect icon_bounds(x, icon_y, icon_skia.width(), icon_skia.height());
    AdjustBoundsForRTLUI(&icon_bounds);
    canvas->DrawImageInt(icon_skia, icon_bounds.x(), icon_bounds.y());
    x += icon_skia.width();
  }

  // Draw the text.
  SkColor text_color = foreground_color_id_
                           ? GetColorProvider()->GetColor(*foreground_color_id_)
                           : GetTextColorForEnableState(*this, GetEnabled());
  std::u16string text = GetModel()->GetItemAt(*selected_index_);
  const gfx::FontList& font_list = GetForegroundFontList();

  // If the text is not empty, add padding between it and the icon. If there
  // was an empty icon, this padding is not necessary.
  if (!text.empty() && !icon.IsEmpty()) {
    x += LayoutProvider::Get()->GetDistanceMetric(
        DISTANCE_RELATED_LABEL_HORIZONTAL);
  }

  // The total width of the text is the minimum of either the string width,
  // or the available space, accounting for optional arrow.
  int text_width = gfx::GetStringWidth(text, font_list);
  int available_width = width() - x - insets.right();
  if (should_show_arrow_) {
    available_width -= GetComboboxArrowContainerWidthAndMargins();
  }
  text_width = std::min(text_width, available_width);

  gfx::Rect text_bounds(x, y, text_width, contents_height);
  AdjustBoundsForRTLUI(&text_bounds);
  canvas->DrawStringRect(text, font_list, text_color, text_bounds);

  // Draw the arrow.
  // TODO(crbug.com/40247801): Replace placeholder spacing and color values for
  // ChromeRefresh2023.
  if (should_show_arrow_) {
    gfx::Rect arrow_bounds(width() - GetComboboxArrowContainerWidthAndMargins(),
                           0, GetComboboxArrowContainerWidth(), height());
    arrow_bounds.ClampToCenteredSize(ComboboxArrowSize());
    AdjustBoundsForRTLUI(&arrow_bounds);

    PaintComboboxArrow(text_color, arrow_bounds, canvas);
  }
}

void Combobox::ArrowButtonPressed(const ui::Event& event) {
  if (!GetEnabled())
    return;

  // TODO(hajimehoshi): Fix the problem that the arrow button blinks when
  // cliking this while the dropdown menu is opened.
  if ((base::TimeTicks::Now() - closed_time_) > kMinimumTimeBetweenButtonClicks)
    ShowDropDownMenu(ui::GetMenuSourceTypeForEvent(event));
}

void Combobox::ShowDropDownMenu(ui::MenuSourceType source_type) {
  on_menu_will_show_.Notify();

  constexpr int kMenuBorderWidthTop = 1;
  // Menu's requested position's width should be the same as local bounds so the
  // border of the menu lines up with the border of the combobox. The y
  // coordinate however should be shifted to the bottom with the border with not
  // to overlap with the combobox border.
  gfx::Rect lb = GetLocalBounds();
  gfx::Point menu_position(lb.origin());
  menu_position.set_y(menu_position.y() + kMenuBorderWidthTop);

  View::ConvertPointToScreen(this, &menu_position);

  gfx::Rect bounds(menu_position, lb.size());
  // If check marks exist in the combobox, adjust with bounds width to account
  // for them.
  if (!size_to_largest_label_)
    bounds.set_width(MaybeAdjustWidthForCheckmarks(bounds.width()));

  Button::ButtonState original_state = arrow_button_->GetState();
  arrow_button_->SetState(Button::STATE_PRESSED);

  // Allow |menu_runner_| to be set by the testing API, but if this method is
  // ever invoked recursively, ensure the old menu is closed.
  if (!menu_runner_ || IsMenuRunning()) {
    menu_runner_ = std::make_unique<MenuRunner>(
        menu_model_.get(), MenuRunner::COMBOBOX,
        base::BindRepeating(&Combobox::OnMenuClosed, base::Unretained(this),
                            original_state));
  }
  if (should_highlight_) {
    InkDrop::Get(arrow_button_)
        ->AnimateToState(InkDropState::ACTIVATED, nullptr);
  }
  menu_runner_->RunMenuAt(GetWidget(), nullptr, bounds,
                          MenuAnchorPosition::kTopLeft, source_type);
  UpdateExpandedCollapsedAccessibleState();
}

void Combobox::OnMenuClosed(Button::ButtonState original_button_state) {
  if (should_highlight_) {
    InkDrop::Get(arrow_button_)
        ->AnimateToState(InkDropState::DEACTIVATED, nullptr);
    InkDrop::Get(arrow_button_)->GetInkDrop()->SetHovered(IsMouseHovered());
  }
  menu_runner_.reset();
  arrow_button_->SetState(original_button_state);
  closed_time_ = base::TimeTicks::Now();
  UpdateExpandedCollapsedAccessibleState();
}

void Combobox::MenuSelectionAt(size_t index) {
  if (!menu_selection_at_callback_ || !menu_selection_at_callback_.Run(index)) {
    SetSelectedIndex(index);
    OnPerformAction();
  }
}

void Combobox::OnPerformAction() {
  SchedulePaint();

  if (callback_)
    callback_.Run();

  // Note |this| may be deleted by |callback_|.
}

gfx::Size Combobox::GetContentSize() const {
  const gfx::FontList& font_list = GetForegroundFontList();
  int height = font_list.GetHeight();
  int width = 0;
  for (size_t i = 0; i < GetModel()->GetItemCount(); ++i) {
    if (model_->IsItemSeparatorAt(i))
      continue;

    if (size_to_largest_label_ || i == selected_index_) {
      int item_width = 0;
      ui::ImageModel icon = GetModel()->GetIconAt(i);
      std::u16string text = GetModel()->GetItemAt(i);
      if (!icon.IsEmpty()) {
        gfx::ImageSkia icon_skia;
        if (GetWidget())
          icon_skia = icon.Rasterize(GetColorProvider());
        item_width += icon_skia.width();
        height = std::max(height, icon_skia.height());

        // If both the text and icon are not empty, include padding between.
        // We do not include this padding if there is no icon present.
        if (!text.empty()) {
          item_width += LayoutProvider::Get()->GetDistanceMetric(
              DISTANCE_RELATED_LABEL_HORIZONTAL);
        }
      }

      // If text is not empty, the content size needs to include the text width
      if (!text.empty()) {
        item_width += gfx::GetStringWidth(GetModel()->GetItemAt(i), font_list);
      }

      if (size_to_largest_label_)
        item_width = MaybeAdjustWidthForCheckmarks(item_width);
      width = std::max(width, item_width);
    }
  }
  return gfx::Size(width, height);
}

int Combobox::MaybeAdjustWidthForCheckmarks(int original_width) const {
  return MenuConfig::instance().check_selected_combobox_item
             ? original_width + kMenuCheckSize +
                   LayoutProvider::Get()->GetDistanceMetric(
                       DISTANCE_RELATED_BUTTON_HORIZONTAL)
             : original_width;
}

void Combobox::OnContentSizeMaybeChanged() {
  content_size_ = GetContentSize();
  PreferredSizeChanged();
}

PrefixSelector* Combobox::GetPrefixSelector() {
  if (!selector_)
    selector_ = std::make_unique<PrefixSelector>(this, this);
  return selector_.get();
}

const gfx::FontList& Combobox::GetForegroundFontList() const {
  if (foreground_text_style_) {
    return TypographyProvider::Get().GetFont(kContext, *foreground_text_style_);
  }
  return GetFontList();
}

void Combobox::UpdateExpandedCollapsedAccessibleState() const {
  if (menu_runner_) {
    GetViewAccessibility().SetIsExpanded();
  } else {
    GetViewAccessibility().SetIsCollapsed();
  }
}

void Combobox::UpdateAccessibleValue() const {
  if (model_->GetItemCount() > 0 && selected_index_.has_value()) {
    GetViewAccessibility().SetValue(model_->GetItemAt(selected_index_.value()));
  } else {
    GetViewAccessibility().RemoveValue();
  }
}

void Combobox::UpdateAccessibleDefaultActionVerb() {
  if (GetEnabled()) {
    GetViewAccessibility().SetDefaultActionVerb(
        ax::mojom::DefaultActionVerb::kOpen);
  } else {
    GetViewAccessibility().RemoveDefaultActionVerb();
  }
}

BEGIN_METADATA(Combobox)
ADD_PROPERTY_METADATA(base::RepeatingClosure, Callback)
ADD_PROPERTY_METADATA(std::unique_ptr<ui::ComboboxModel>, OwnedModel)
ADD_PROPERTY_METADATA(ui::ComboboxModel*, Model)
ADD_PROPERTY_METADATA(std::optional<size_t>, SelectedIndex)
ADD_PROPERTY_METADATA(bool, Invalid)
ADD_PROPERTY_METADATA(bool, SizeToLargestLabel)
ADD_PROPERTY_METADATA(std::u16string, TooltipTextAndAccessibleName)
END_METADATA

}  // namespace views
