// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/tabbed_pane/tabbed_pane.h"

#include <algorithm>
#include <utility>

#include "base/check_op.h"
#include "base/i18n/rtl.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/default_style.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane_listener.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace views {

Tab::Tab(TabbedPane* tabbed_pane, const base::string16& title, View* contents)
    : tabbed_pane_(tabbed_pane), contents_(contents) {
  // Calculate the size while the font list is bold.
  auto title_label = std::make_unique<Label>(title, style::CONTEXT_LABEL,
                                             style::STYLE_TAB_ACTIVE);
  title_ = title_label.get();
  UpdatePreferredTitleWidth();

  if (tabbed_pane_->GetOrientation() == TabbedPane::Orientation::kVertical) {
    title_label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);

    const bool is_highlight_style =
        tabbed_pane_->GetStyle() == TabbedPane::TabStripStyle::kHighlight;
    constexpr auto kTabPadding = gfx::Insets(5, 10);
    constexpr auto kTabPaddingHighlight = gfx::Insets(8, 32, 8, 0);
    SetBorder(CreateEmptyBorder(is_highlight_style ? kTabPaddingHighlight
                                                   : kTabPadding));
  } else {
    constexpr auto kBorderThickness = gfx::Insets(2);
    SetBorder(CreateEmptyBorder(kBorderThickness));
  }

  SetState(State::kInactive);
  AddChildView(std::move(title_label));
  SetLayoutManager(std::make_unique<FillLayout>());

  // Use leaf so that name is spoken by screen reader without exposing the
  // children.
  GetViewAccessibility().OverrideIsLeaf(true);

  OnStateChanged();
}

Tab::~Tab() = default;

void Tab::SetSelected(bool selected) {
  contents_->SetVisible(selected);
  contents_->parent()->InvalidateLayout();
  SetState(selected ? State::kActive : State::kInactive);
#if defined(OS_APPLE)
  SetFocusBehavior(selected ? FocusBehavior::ACCESSIBLE_ONLY
                            : FocusBehavior::NEVER);
#else
  SetFocusBehavior(selected ? FocusBehavior::ALWAYS : FocusBehavior::NEVER);
#endif
}

const base::string16& Tab::GetTitleText() const {
  return title_->GetText();
}

void Tab::SetTitleText(const base::string16& text) {
  title_->SetText(text);
  UpdatePreferredTitleWidth();
  PreferredSizeChanged();
}

bool Tab::OnMousePressed(const ui::MouseEvent& event) {
  if (GetEnabled() && event.IsOnlyLeftMouseButton())
    tabbed_pane_->SelectTab(this);
  return true;
}

void Tab::OnMouseEntered(const ui::MouseEvent& event) {
  SetState(selected() ? State::kActive : State::kHovered);
}

void Tab::OnMouseExited(const ui::MouseEvent& event) {
  SetState(selected() ? State::kActive : State::kInactive);
}

void Tab::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_TAP_DOWN:
    case ui::ET_GESTURE_TAP:
      // SelectTab also sets the right tab color.
      tabbed_pane_->SelectTab(this);
      break;
    case ui::ET_GESTURE_TAP_CANCEL:
      SetState(selected() ? State::kActive : State::kInactive);
      break;
    default:
      break;
  }
  event->SetHandled();
}

gfx::Size Tab::CalculatePreferredSize() const {
  int width = preferred_title_width_ + GetInsets().width();
  if (tabbed_pane_->GetStyle() == TabbedPane::TabStripStyle::kHighlight &&
      tabbed_pane_->GetOrientation() == TabbedPane::Orientation::kVertical)
    width = std::max(width, 192);
  return gfx::Size(width, 32);
}

void Tab::GetAccessibleNodeData(ui::AXNodeData* data) {
  data->role = ax::mojom::Role::kTab;
  data->SetName(title_->GetText());
  data->AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, selected());
}

bool Tab::HandleAccessibleAction(const ui::AXActionData& action_data) {
  // If the assistive tool sends kSetSelection, handle it like kDoDefault.
  // These generate a click event handled in Tab::OnMousePressed.
  ui::AXActionData action_data_copy(action_data);
  if (action_data.action == ax::mojom::Action::kSetSelection)
    action_data_copy.action = ax::mojom::Action::kDoDefault;
  return View::HandleAccessibleAction(action_data_copy);
}

void Tab::OnFocus() {
  // Do not draw focus ring in kHighlight mode.
  if (tabbed_pane_->GetStyle() != TabbedPane::TabStripStyle::kHighlight) {
    // Maintain the current Insets with CreatePaddedBorder.
    int border_size = 2;
    SetBorder(CreatePaddedBorder(
        CreateSolidBorder(border_size,
                          GetNativeTheme()->GetSystemColor(
                              ui::NativeTheme::kColorId_FocusedBorderColor)),
        GetInsets() - gfx::Insets(border_size)));
  }

  // When the tab gains focus, send an accessibility event indicating that the
  // contents are focused. When the tab loses focus, whichever new View ends up
  // with focus will send an ax::mojom::Event::kFocus of its own, so there's no
  // need to send one in OnBlur().
  if (contents())
    contents()->NotifyAccessibilityEvent(ax::mojom::Event::kFocus, true);
  SchedulePaint();
}

void Tab::OnBlur() {
  // Do not draw focus ring in kHighlight mode.
  if (tabbed_pane_->GetStyle() != TabbedPane::TabStripStyle::kHighlight)
    SetBorder(CreateEmptyBorder(GetInsets()));
  SchedulePaint();
}

bool Tab::OnKeyPressed(const ui::KeyEvent& event) {
  const ui::KeyboardCode key = event.key_code();
  if (tabbed_pane_->GetOrientation() == TabbedPane::Orientation::kHorizontal) {
    // Use left and right arrows to navigate tabs in horizontal orientation.
    return (key == ui::VKEY_LEFT || key == ui::VKEY_RIGHT) &&
           tabbed_pane_->MoveSelectionBy(key == ui::VKEY_RIGHT ? 1 : -1);
  }
  // Use up and down arrows to navigate tabs in vertical orientation.
  return (key == ui::VKEY_UP || key == ui::VKEY_DOWN) &&
         tabbed_pane_->MoveSelectionBy(key == ui::VKEY_DOWN ? 1 : -1);
}

void Tab::SetState(State state) {
  if (state == state_)
    return;
  state_ = state;
  OnStateChanged();
  SchedulePaint();
}

void Tab::OnStateChanged() {
  const SkColor font_color = GetNativeTheme()->GetSystemColor(
      state_ == State::kActive
          ? ui::NativeTheme::kColorId_TabTitleColorActive
          : ui::NativeTheme::kColorId_TabTitleColorInactive);
  title_->SetEnabledColor(font_color);

  // Tab design spec dictates special handling of font weight for the windows
  // platform when dealing with border style tabs.
#if defined(OS_WIN)
  gfx::Font::Weight font_weight = gfx::Font::Weight::BOLD;
#else
  gfx::Font::Weight font_weight = gfx::Font::Weight::MEDIUM;
#endif
  int font_size_delta = ui::kLabelFontSizeDelta;

  if (tabbed_pane_->GetStyle() == TabbedPane::TabStripStyle::kHighlight) {
    // Notify assistive tools to update this tab's selected status. The way
    // ChromeOS accessibility is implemented right now, firing almost any event
    // will work, we just need to trigger its state to be refreshed.
    if (state_ == State::kInactive)
      NotifyAccessibilityEvent(ax::mojom::Event::kCheckedStateChanged, true);

    // Style the tab text according to the spec for highlight style tabs. We no
    // longer have windows specific bolding of text in this case.
    font_size_delta = 1;
    if (state_ == State::kActive)
      font_weight = gfx::Font::Weight::BOLD;
    else
      font_weight = gfx::Font::Weight::MEDIUM;
  }

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  title_->SetFontList(
      rb.GetFontListWithDelta(font_size_delta, gfx::Font::NORMAL, font_weight));
}

void Tab::OnPaint(gfx::Canvas* canvas) {
  View::OnPaint(canvas);

  // Paints the active tab for the vertical highlighted tabbed pane.
  if (!selected() ||
      tabbed_pane_->GetOrientation() != TabbedPane::Orientation::kVertical ||
      tabbed_pane_->GetStyle() != TabbedPane::TabStripStyle::kHighlight) {
    return;
  }
  constexpr SkScalar kRadius = SkIntToScalar(32);
  constexpr SkScalar kLTRRadii[8] = {0,       0,       kRadius, kRadius,
                                     kRadius, kRadius, 0,       0};
  constexpr SkScalar kRTLRadii[8] = {kRadius, kRadius, 0,       0,
                                     0,       0,       kRadius, kRadius};
  SkPath path;
  path.addRoundRect(gfx::RectToSkRect(GetLocalBounds()),
                    base::i18n::IsRTL() ? kRTLRadii : kLTRRadii);

  cc::PaintFlags fill_flags;
  fill_flags.setAntiAlias(true);
  fill_flags.setColor(GetNativeTheme()->GetSystemColor(
      HasFocus() ? ui::NativeTheme::kColorId_TabHighlightFocusedBackground
                 : ui::NativeTheme::kColorId_TabHighlightBackground));
  canvas->DrawPath(path, fill_flags);
}

void Tab::UpdatePreferredTitleWidth() {
  // Active and inactive states use different font sizes. Find the largest size
  // and reserve that amount of space.
  const State old_state = state_;
  SetState(State::kActive);
  preferred_title_width_ = title_->GetPreferredSize().width();
  SetState(State::kInactive);
  preferred_title_width_ =
      std::max(preferred_title_width_, title_->GetPreferredSize().width());
  SetState(old_state);
}

BEGIN_METADATA(Tab, View)
END_METADATA

// static
constexpr size_t TabStrip::kNoSelectedTab;

TabStrip::TabStrip(TabbedPane::Orientation orientation,
                   TabbedPane::TabStripStyle style)
    : orientation_(orientation), style_(style) {
  std::unique_ptr<BoxLayout> layout;
  if (orientation == TabbedPane::Orientation::kHorizontal) {
    layout = std::make_unique<BoxLayout>(BoxLayout::Orientation::kHorizontal);
    layout->set_main_axis_alignment(BoxLayout::MainAxisAlignment::kCenter);
    layout->set_cross_axis_alignment(BoxLayout::CrossAxisAlignment::kStretch);
    layout->SetDefaultFlex(1);
  } else {
    constexpr auto kEdgePadding = gfx::Insets(8, 0, 0, 0);
    constexpr int kTabSpacing = 8;
    layout = std::make_unique<BoxLayout>(BoxLayout::Orientation::kVertical,
                                         kEdgePadding, kTabSpacing);
    layout->set_main_axis_alignment(BoxLayout::MainAxisAlignment::kStart);
    layout->set_cross_axis_alignment(BoxLayout::CrossAxisAlignment::kStart);
    layout->SetDefaultFlex(0);
  }
  SetLayoutManager(std::move(layout));

  GetViewAccessibility().OverrideRole(ax::mojom::Role::kIgnored);

  // These durations are taken from the Paper Tabs source:
  // https://github.com/PolymerElements/paper-tabs/blob/master/paper-tabs.html
  // See |selectionBar.expand| and |selectionBar.contract|.
  expand_animation_->SetDuration(base::TimeDelta::FromMilliseconds(150));
  contract_animation_->SetDuration(base::TimeDelta::FromMilliseconds(180));
}

TabStrip::~TabStrip() = default;

void TabStrip::AnimationProgressed(const gfx::Animation* animation) {
  SchedulePaint();
}

void TabStrip::AnimationEnded(const gfx::Animation* animation) {
  if (animation == expand_animation_.get())
    contract_animation_->Start();
}

void TabStrip::OnSelectedTabChanged(Tab* from_tab, Tab* to_tab, bool animate) {
  DCHECK(!from_tab->selected());
  DCHECK(to_tab->selected());
  if (!animate)
    return;

  if (GetOrientation() == TabbedPane::Orientation::kHorizontal) {
    animating_from_ = gfx::Range(from_tab->GetMirroredX(),
                                 from_tab->GetMirroredX() + from_tab->width());
    animating_to_ = gfx::Range(to_tab->GetMirroredX(),
                               to_tab->GetMirroredX() + to_tab->width());
  } else {
    animating_from_ = gfx::Range(from_tab->bounds().y(),
                                 from_tab->bounds().y() + from_tab->height());
    animating_to_ = gfx::Range(to_tab->bounds().y(),
                               to_tab->bounds().y() + to_tab->height());
  }

  contract_animation_->Stop();
  expand_animation_->Start();
}

Tab* TabStrip::GetSelectedTab() const {
  size_t index = GetSelectedTabIndex();
  return index == kNoSelectedTab ? nullptr : GetTabAtIndex(index);
}

Tab* TabStrip::GetTabAtDeltaFromSelected(int delta) const {
  const size_t selected_tab_index = GetSelectedTabIndex();
  DCHECK_NE(kNoSelectedTab, selected_tab_index);
  const size_t num_children = children().size();
  // Clamping |delta| here ensures that even a large negative |delta| will not
  // cause the addition in the next statement to wrap below 0.
  delta %= static_cast<int>(num_children);
  return GetTabAtIndex((selected_tab_index + num_children + delta) %
                       num_children);
}

Tab* TabStrip::GetTabAtIndex(size_t index) const {
  DCHECK_LT(index, children().size());
  return static_cast<Tab*>(children()[index]);
}

size_t TabStrip::GetSelectedTabIndex() const {
  for (size_t i = 0; i < children().size(); ++i)
    if (GetTabAtIndex(i)->selected())
      return i;
  return kNoSelectedTab;
}

TabbedPane::Orientation TabStrip::GetOrientation() const {
  return orientation_;
}

TabbedPane::TabStripStyle TabStrip::GetStyle() const {
  return style_;
}

gfx::Size TabStrip::CalculatePreferredSize() const {
  // In horizontal mode, use the preferred size as determined by the largest
  // child or the minimum size necessary to display the tab titles, whichever is
  // larger.
  if (GetOrientation() == TabbedPane::Orientation::kHorizontal) {
    return GetLayoutManager()->GetPreferredSize(this);
  } else {
    // In vertical mode, Tabstrips don't require any minimum space along their
    // main axis, and can shrink all the way to zero size.  Only the cross axis
    // thickness matters.
    const gfx::Size size = GetLayoutManager()->GetPreferredSize(this);
    return gfx::Size(size.width(), 0);
  }
}

void TabStrip::OnPaintBorder(gfx::Canvas* canvas) {
  // Do not draw border line in kHighlight mode.
  if (GetStyle() == TabbedPane::TabStripStyle::kHighlight)
    return;

  // First, draw the unselected border across the TabStrip's entire width or
  // height, depending on the orientation of the tab alignment. The area
  // underneath or on the right of the selected tab will be overdrawn later.
  const bool is_horizontal =
      GetOrientation() == TabbedPane::Orientation::kHorizontal;
  int max_cross_axis;
  gfx::Rect rect;
  constexpr int kUnselectedBorderThickness = 1;
  if (is_horizontal) {
    max_cross_axis = children().front()->bounds().bottom();
    rect = gfx::Rect(0, max_cross_axis - kUnselectedBorderThickness, width(),
                     kUnselectedBorderThickness);
  } else {
    max_cross_axis = width();
    rect = gfx::Rect(max_cross_axis - kUnselectedBorderThickness, 0,
                     kUnselectedBorderThickness, height());
  }
  canvas->FillRect(rect, GetNativeTheme()->GetSystemColor(
                             ui::NativeTheme::kColorId_TabBottomBorder));

  Tab* tab = GetSelectedTab();
  if (!tab)
    return;

  // Now, figure out the range to draw the selection marker underneath. There
  // are three states here:
  // 1) Expand animation is running: use FAST_OUT_LINEAR_IN to grow the
  //    selection marker until it encompasses both the previously selected tab
  //    and the currently selected tab;
  // 2) Contract animation is running: use LINEAR_OUT_SLOW_IN to shrink the
  //    selection marker until it encompasses only the currently selected tab;
  // 3) No animations running: the selection marker is only under the currently
  //    selected tab.
  int min_main_axis = 0;
  int max_main_axis = 0;
  if (expand_animation_->is_animating()) {
    bool animating_leading = animating_to_.start() < animating_from_.start();
    double anim_value = gfx::Tween::CalculateValue(
        gfx::Tween::FAST_OUT_LINEAR_IN, expand_animation_->GetCurrentValue());
    if (animating_leading) {
      min_main_axis = gfx::Tween::IntValueBetween(
          anim_value, animating_from_.start(), animating_to_.start());
      max_main_axis = animating_from_.end();
    } else {
      min_main_axis = animating_from_.start();
      max_main_axis = gfx::Tween::IntValueBetween(
          anim_value, animating_from_.end(), animating_to_.end());
    }
  } else if (contract_animation_->is_animating()) {
    bool animating_leading = animating_to_.start() < animating_from_.start();
    double anim_value = gfx::Tween::CalculateValue(
        gfx::Tween::LINEAR_OUT_SLOW_IN, contract_animation_->GetCurrentValue());
    if (animating_leading) {
      min_main_axis = animating_to_.start();
      max_main_axis = gfx::Tween::IntValueBetween(
          anim_value, animating_from_.end(), animating_to_.end());
    } else {
      min_main_axis = gfx::Tween::IntValueBetween(
          anim_value, animating_from_.start(), animating_to_.start());
      max_main_axis = animating_to_.end();
    }
  } else if (is_horizontal) {
    min_main_axis = tab->GetMirroredX();
    max_main_axis = min_main_axis + tab->width();
  } else {
    min_main_axis = tab->bounds().y();
    max_main_axis = min_main_axis + tab->height();
  }

  DCHECK_NE(min_main_axis, max_main_axis);
  // Draw over the unselected border from above.
  constexpr int kSelectedBorderThickness = 2;
  rect = gfx::Rect(min_main_axis, max_cross_axis - kSelectedBorderThickness,
                   max_main_axis - min_main_axis, kSelectedBorderThickness);
  if (!is_horizontal)
    rect.Transpose();
  canvas->FillRect(rect, GetNativeTheme()->GetSystemColor(
                             ui::NativeTheme::kColorId_TabSelectedBorderColor));
}

DEFINE_ENUM_CONVERTERS(TabbedPane::Orientation,
                       {TabbedPane::Orientation::kHorizontal,
                        base::ASCIIToUTF16("HORIZONTAL")},
                       {TabbedPane::Orientation::kVertical,
                        base::ASCIIToUTF16("VERTICAL")})

DEFINE_ENUM_CONVERTERS(TabbedPane::TabStripStyle,
                       {TabbedPane::TabStripStyle::kBorder,
                        base::ASCIIToUTF16("BORDER")},
                       {TabbedPane::TabStripStyle::kHighlight,
                        base::ASCIIToUTF16("HIGHLIGHT")})

BEGIN_METADATA(TabStrip, View)
ADD_READONLY_PROPERTY_METADATA(int, SelectedTabIndex)
ADD_READONLY_PROPERTY_METADATA(TabbedPane::Orientation, Orientation)
ADD_READONLY_PROPERTY_METADATA(TabbedPane::TabStripStyle, Style)
END_METADATA

TabbedPane::TabbedPane(TabbedPane::Orientation orientation,
                       TabbedPane::TabStripStyle style) {
  DCHECK(orientation != TabbedPane::Orientation::kHorizontal ||
         style != TabbedPane::TabStripStyle::kHighlight);
  auto* layout = SetLayoutManager(std::make_unique<views::FlexLayout>());
  if (orientation == TabbedPane::Orientation::kHorizontal)
    layout->SetOrientation(views::LayoutOrientation::kVertical);
  tab_strip_ = AddChildView(std::make_unique<TabStrip>(orientation, style));
  contents_ = AddChildView(std::make_unique<View>());
  contents_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));
  contents_->SetLayoutManager(std::make_unique<views::FillLayout>());

  // Support navigating tabs by Ctrl+Tab and Ctrl+Shift+Tab.
  AddAccelerator(
      ui::Accelerator(ui::VKEY_TAB, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN));
  AddAccelerator(ui::Accelerator(ui::VKEY_TAB, ui::EF_CONTROL_DOWN));
}

TabbedPane::~TabbedPane() = default;

size_t TabbedPane::GetSelectedTabIndex() const {
  return tab_strip_->GetSelectedTabIndex();
}

size_t TabbedPane::GetTabCount() {
  DCHECK_EQ(tab_strip_->children().size(), contents_->children().size());
  return contents_->children().size();
}

void TabbedPane::AddTabInternal(size_t index,
                                const base::string16& title,
                                std::unique_ptr<View> contents) {
  DCHECK_LE(index, GetTabCount());
  contents->SetVisible(false);
  contents->GetViewAccessibility().OverrideName(title);
  contents->GetViewAccessibility().OverrideRole(ax::mojom::Role::kTab);

  tab_strip_->AddChildViewAt(std::make_unique<Tab>(this, title, contents.get()),
                             static_cast<int>(index));
  contents_->AddChildViewAt(std::move(contents), static_cast<int>(index));
  if (!GetSelectedTab())
    SelectTabAt(index);

  PreferredSizeChanged();
}

void TabbedPane::SelectTab(Tab* new_selected_tab, bool animate) {
  Tab* old_selected_tab = tab_strip_->GetSelectedTab();
  if (old_selected_tab == new_selected_tab)
    return;

  new_selected_tab->SetSelected(true);
  if (old_selected_tab) {
    if (old_selected_tab->HasFocus())
      new_selected_tab->RequestFocus();
    old_selected_tab->SetSelected(false);
    tab_strip_->OnSelectedTabChanged(old_selected_tab, new_selected_tab,
                                     animate);
  }
  tab_strip_->SchedulePaint();

  FocusManager* focus_manager = new_selected_tab->contents()->GetFocusManager();
  if (focus_manager) {
    const View* focused_view = focus_manager->GetFocusedView();
    if (focused_view && contents_->Contains(focused_view) &&
        !new_selected_tab->contents()->Contains(focused_view))
      focus_manager->SetFocusedView(new_selected_tab->contents());
  }

  if (listener())
    listener()->TabSelectedAt(tab_strip_->GetIndexOf(new_selected_tab));
}

void TabbedPane::SelectTabAt(size_t index, bool animate) {
  Tab* tab = tab_strip_->GetTabAtIndex(index);
  if (tab)
    SelectTab(tab, animate);
}

TabbedPane::Orientation TabbedPane::GetOrientation() const {
  return tab_strip_->GetOrientation();
}

TabbedPane::TabStripStyle TabbedPane::GetStyle() const {
  return tab_strip_->GetStyle();
}

Tab* TabbedPane::GetTabAt(size_t index) {
  return tab_strip_->GetTabAtIndex(index);
}

Tab* TabbedPane::GetSelectedTab() {
  return tab_strip_->GetSelectedTab();
}

View* TabbedPane::GetSelectedTabContentView() {
  return GetSelectedTab() ? GetSelectedTab()->contents() : nullptr;
}

bool TabbedPane::MoveSelectionBy(int delta) {
  if (contents_->children().size() <= 1)
    return false;
  SelectTab(tab_strip_->GetTabAtDeltaFromSelected(delta));
  return true;
}

bool TabbedPane::AcceleratorPressed(const ui::Accelerator& accelerator) {
  // Handle Ctrl+Tab and Ctrl+Shift+Tab navigation of pages.
  DCHECK(accelerator.key_code() == ui::VKEY_TAB && accelerator.IsCtrlDown());
  return MoveSelectionBy(accelerator.IsShiftDown() ? -1 : 1);
}

void TabbedPane::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kTabList;
  const Tab* const selected_tab = GetSelectedTab();
  if (selected_tab)
    node_data->SetName(selected_tab->GetTitleText());
}

BEGIN_METADATA(TabbedPane, View)
END_METADATA

}  // namespace views
