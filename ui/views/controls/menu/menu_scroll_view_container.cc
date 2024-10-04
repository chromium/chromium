// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_scroll_view_container.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "cc/paint/paint_flags.h"
#include "chromeos/constants/chromeos_features.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/round_rect_painter.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace views {

namespace {

static constexpr float kBackgroundBlurSigma = 30.f;
static constexpr float kBackgroundBlurQuality = 0.33f;

// MenuScrollButton ------------------------------------------------------------

// MenuScrollButton is used for the scroll buttons when not all menu items fit
// on screen. MenuScrollButton forwards appropriate events to the
// MenuController.

class MenuScrollButton : public View {
  METADATA_HEADER(MenuScrollButton, View)

 public:
  MenuScrollButton(SubmenuView* host, bool is_up)
      : host_(host),
        is_up_(is_up),
        // Make our height the same as that of other MenuItemViews.
        pref_height_(host_->GetPreferredItemHeight()) {}
  MenuScrollButton(const MenuScrollButton&) = delete;
  MenuScrollButton& operator=(const MenuScrollButton&) = delete;

  gfx::Size CalculatePreferredSize(
      const SizeBounds& /*available_size*/) const override {
    return gfx::Size(MenuConfig::instance().scroll_arrow_height * 2 - 1,
                     pref_height_);
  }

  bool CanDrop(const OSExchangeData& data) override {
    return true;  // Always return true so that drop events are targeted to us.
  }

  void OnDragEntered(const ui::DropTargetEvent& event) override {
    GetMenuController()->OnDragEnteredScrollButton(host_, is_up_);
  }

  int OnDragUpdated(const ui::DropTargetEvent& event) override {
    return ui::DragDropTypes::DRAG_NONE;
  }

  void OnDragExited() override {
    GetMenuController()->OnDragExitedScrollButton(host_);
  }

  DropCallback GetDropCallback(const ui::DropTargetEvent& event) override {
    return base::DoNothing();
  }

  void OnPaint(gfx::Canvas* canvas) override {
    // The background.
    const auto* const color_provider = GetColorProvider();
    GetNativeTheme()->Paint(
        canvas->sk_canvas(), color_provider,
        ui::NativeTheme::kMenuItemBackground, ui::NativeTheme::kNormal,
        GetLocalBounds(),
        ui::NativeTheme::ExtraParams(
            absl::in_place_type<ui::NativeTheme::MenuItemExtraParams>));

    // Then the arrow.
    const int x = width() / 2;
    const MenuConfig& config = MenuConfig::instance();
    int y = (height() - config.scroll_arrow_height) / 2;
    int y_bottom = y + config.scroll_arrow_height;
    if (!is_up_) {
      std::swap(y, y_bottom);
    }

    SkPath path;
    path.setFillType(SkPathFillType::kWinding);
    path.moveTo(SkIntToScalar(x), SkIntToScalar(y));
    path.lineTo(SkIntToScalar(x - config.scroll_arrow_height),
                SkIntToScalar(y_bottom));
    path.lineTo(SkIntToScalar(x + config.scroll_arrow_height),
                SkIntToScalar(y_bottom));
    path.lineTo(SkIntToScalar(x), SkIntToScalar(y));
    cc::PaintFlags flags;
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setAntiAlias(true);
    flags.setColor(color_provider->GetColor(ui::kColorMenuItemForeground));
    canvas->DrawPath(path, flags);
  }

 private:
  MenuController* GetMenuController() {
    auto* const menu_controller = host_->GetMenuItem()->GetMenuController();
    CHECK(menu_controller);
    return menu_controller;
  }

  // SubmenuView we were created for.
  const raw_ptr<SubmenuView> host_;

  // Direction of the button.
  const bool is_up_;

  // Preferred height.
  const int pref_height_;
};

BEGIN_METADATA(MenuScrollButton)
END_METADATA

}  // namespace

// MenuScrollView --------------------------------------------------------------

// MenuScrollView is a viewport for the SubmenuView. It's reason to exist is so
// that ScrollRectToVisible works.
//
// NOTE: It is possible to use ScrollView directly (after making it deal with
// null scrollbars), but clicking on a child of ScrollView forces the window to
// become active, which we don't want. As we really only need a fraction of
// what ScrollView does, so we use a one off variant.

class MenuScrollViewContainer::MenuScrollView : public View {
  METADATA_HEADER(MenuScrollView, View)

 public:
  MenuScrollView(View* child, MenuScrollViewContainer* owner) : owner_(owner) {
    AddChildView(child);
  }
  MenuScrollView(const MenuScrollView&) = delete;
  MenuScrollView& operator=(const MenuScrollView&) = delete;

  void ScrollRectToVisible(const gfx::Rect& rect) override {
    // NOTE: this assumes we only want to scroll in the y direction.

    // If the rect is already visible, do not scroll.
    if (GetLocalBounds().Contains(rect))
      return;

    // Scroll just enough so that the rect is visible.
    int dy = 0;
    if (rect.bottom() > GetLocalBounds().bottom())
      dy = rect.bottom() - GetLocalBounds().bottom();
    else
      dy = rect.y();

    // Convert rect.y() to view's coordinates and make sure we don't show past
    // the bottom of the view.
    View* child = GetContents();
    int old_y = child->y();
    int y = -std::max(
        0, std::min(child->GetPreferredSize({}).height() - this->height(),
                    dy - child->y()));
    child->SetY(y);

    const int min_y = 0;
    const int max_y = -(child->GetPreferredSize({}).height() - this->height());

    if (old_y == min_y && old_y != y)
      owner_->DidScrollAwayFromTop();
    if (old_y == max_y && old_y != y)
      owner_->DidScrollAwayFromBottom();

    if (y == min_y)
      owner_->DidScrollToTop();
    if (y == max_y)
      owner_->DidScrollToBottom();
  }

  // Returns the contents, which is the SubmenuView.
  View* GetContents() { return children().front(); }
  const View* GetContents() const { return children().front(); }

 private:
  raw_ptr<MenuScrollViewContainer> owner_;
};

BEGIN_METADATA(MenuScrollViewContainer, MenuScrollView)
END_METADATA

// MenuScrollViewContainer ----------------------------------------------------

MenuScrollViewContainer::MenuScrollViewContainer(SubmenuView* content_view)
    : content_view_(content_view),
      use_ash_system_ui_layout_(content_view->GetMenuItem()
                                    ->GetMenuController()
                                    ->use_ash_system_ui_layout()) {
  SetUseDefaultFillLayout(true);
  background_view_ = AddChildView(std::make_unique<View>());
  if (use_ash_system_ui_layout_) {
    // Enable background blur for ChromeOS system context menu.
    background_view_->SetPaintToLayer();
    auto* background_layer = background_view_->layer();
    background_layer->SetFillsBoundsOpaquely(false);
    background_layer->SetBackgroundBlur(kBackgroundBlurSigma);
    background_layer->SetBackdropFilterQuality(kBackgroundBlurQuality);
  }

  auto* layout =
      background_view_->SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kVertical);

  scroll_up_button_ = background_view_->AddChildView(
      std::make_unique<MenuScrollButton>(content_view, true));

  scroll_view_ = background_view_->AddChildView(
      std::make_unique<MenuScrollView>(content_view_, this));
  scroll_view_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kUnbounded));

  scroll_down_button_ = background_view_->AddChildView(
      std::make_unique<MenuScrollButton>(content_view, false));

  arrow_ = BubbleBorderTypeFromAnchor(
      content_view_->GetMenuItem()->GetMenuController()->GetAnchorPosition());

  // The correct insets must be set before returning, since the menu creation
  // code needs to know the final size of the menu.  Calling CreateBorder() is
  // the easiest way to do that.
  CreateBorder();

  GetViewAccessibility().SetRole(ax::mojom::Role::kMenuBar);
  GetViewAccessibility().SetIsVertical(true);
  // On macOS, NSMenus are not supposed to have anything wrapped around them. To
  // allow VoiceOver to recognize this as a menu and to read aloud the total
  // number of items inside it, we ignore the MenuScrollViewContainer (which
  // holds the menu itself: the SubmenuView).
#if BUILDFLAG(IS_MAC)
  GetViewAccessibility().SetIsIgnored(true);
#endif
}

bool MenuScrollViewContainer::HasBubbleBorder() const {
  return arrow_ != BubbleBorder::NONE ||
         (MenuConfig::instance().use_bubble_border && GetCornerRadius());
}

MenuItemView* MenuScrollViewContainer::GetFootnote() const {
  MenuItemView* const footnote = content_view_->GetLastItem();
  return (footnote && footnote->GetType() == MenuItemView::Type::kHighlighted)
             ? footnote
             : nullptr;
}

int MenuScrollViewContainer::GetCornerRadius() const {
  return MenuConfig::instance().CornerRadiusForMenu(
      content_view_->GetMenuItem()->GetMenuController());
}

gfx::RoundedCornersF MenuScrollViewContainer::GetRoundedCorners() const {
  // The controller could be null during context menu being closed.
  auto* menu_controller = content_view_->GetMenuItem()->GetMenuController();
  if (!menu_controller)
    return gfx::RoundedCornersF(corner_radius_);

  std::optional<gfx::RoundedCornersF> rounded_corners =
      menu_controller->rounded_corners();
  if (rounded_corners.has_value())
    return rounded_corners.value();

  return gfx::RoundedCornersF(corner_radius_);
}

gfx::Insets MenuScrollViewContainer::GetInsets() const {
  return View::GetInsets() + additional_insets_;
}


gfx::Size MenuScrollViewContainer::CalculatePreferredSize(
    const SizeBounds& available_size) const {
  gfx::Size prefsize =
      scroll_view_->GetContents()->GetPreferredSize(available_size);
  const gfx::Insets insets = GetInsets();
  prefsize.Enlarge(insets.width(), insets.height());
  return prefsize;
}

void MenuScrollViewContainer::OnPaintBackground(gfx::Canvas* canvas) {
  if (background()) {
    View::OnPaintBackground(canvas);
    return;
  }

  // ChromeOS system UI menu uses 'background_view_' to paint background.
  if (use_ash_system_ui_layout_ && background_view_->background())
    return;

  gfx::Rect bounds(0, 0, width(), height());
  ui::NativeTheme::MenuBackgroundExtraParams menu_background;
  menu_background.corner_radius = GetCornerRadius();
  const auto* const color_provider = GetColorProvider();
  if (border_color_id_.has_value()) {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(color_provider->GetColor(border_color_id_.value()));
    canvas->DrawRoundRect(GetLocalBounds(), menu_background.corner_radius,
                          flags);
    return;
  }
  GetNativeTheme()->Paint(canvas->sk_canvas(), color_provider,
                          ui::NativeTheme::kMenuPopupBackground,
                          ui::NativeTheme::kNormal, bounds,
                          ui::NativeTheme::ExtraParams(menu_background));
}

void MenuScrollViewContainer::OnThemeChanged() {
  View::OnThemeChanged();
  CreateBorder();
}

void MenuScrollViewContainer::OnBoundsChanged(
    const gfx::Rect& previous_bounds) {
  // When the bounds on the MenuScrollViewContainer itself change, the scroll
  // offset is always reset to 0, so always hide the scroll-up control, and only
  // show the scroll-down control if it's going to be useful.
  scroll_up_button_->SetVisible(false);
  scroll_down_button_->SetVisible(content_view_->GetPreferredSize({}).height() >
                                  GetContentsBounds().height());

  const bool any_scroll_button_visible =
      scroll_up_button_->GetVisible() || scroll_down_button_->GetVisible();

  MenuItemView* const footnote = GetFootnote();
  if (footnote)
    footnote->SetCornerRadius(any_scroll_button_visible ? 0 : corner_radius_);
}

void MenuScrollViewContainer::DidScrollToTop() {
  scroll_up_button_->SetVisible(false);
}

void MenuScrollViewContainer::DidScrollToBottom() {
  scroll_down_button_->SetVisible(false);
}

void MenuScrollViewContainer::DidScrollAwayFromTop() {
  scroll_up_button_->SetVisible(true);
}

void MenuScrollViewContainer::DidScrollAwayFromBottom() {
  scroll_down_button_->SetVisible(true);
}

void MenuScrollViewContainer::CreateBorder() {
  if (HasBubbleBorder()) {
    CreateBubbleBorder();
  } else {
    CreateDefaultBorder();
  }
}

void MenuScrollViewContainer::CreateDefaultBorder() {
  DCHECK_EQ(arrow_, BubbleBorder::NONE);
  corner_radius_ = GetCornerRadius();
  outside_border_insets_ = {};

  const auto& menu_config = MenuConfig::instance();
  const int vertical_inset =
      corner_radius_ ? menu_config.rounded_menu_vertical_border_size.value_or(
                           corner_radius_)
                     : menu_config.nonrounded_menu_vertical_border_size;
  const int horizontal_inset = menu_config.menu_horizontal_border_size;
  const bool has_footnote = !!GetFootnote();
  auto insets =
      gfx::Insets::TLBR(vertical_inset, horizontal_inset,
                        has_footnote ? 0 : vertical_inset, horizontal_inset);

  if (!menu_config.use_outer_border) {
    SetBorder(CreateEmptyBorder(insets));
    return;
  }

  // When a custom background color is used, ensure that the border uses
  // the custom background color for its insets.
  if (border_color_id_.has_value()) {
    SetBorder(
        views::CreateThemedSolidSidedBorder(insets, border_color_id_.value()));
    return;
  }

  SetBackground(CreateThemedRoundedRectBackground(
      ui::kColorMenuBackground, corner_radius_,
      views::RoundRectPainter::kBorderWidth));

  const auto* const color_provider = GetColorProvider();
  SkColor color = color_provider
                      ? color_provider->GetColor(ui::kColorMenuBorder)
                      : gfx::kPlaceholderColor;
  if (has_footnote) {
    insets.set_bottom(views::RoundRectPainter::kBorderWidth);
  }
  SetBorder(views::CreateBorderPainter(
      std::make_unique<views::RoundRectPainter>(color, corner_radius_),
      insets));
}

void MenuScrollViewContainer::CreateBubbleBorder() {
  BubbleBorder::Shadow shadow_type = BubbleBorder::STANDARD_SHADOW;
  ui::ColorId id = ui::kColorMenuBackground;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (use_ash_system_ui_layout_) {
    shadow_type = BubbleBorder::CHROMEOS_SYSTEM_UI_SHADOW;
  }
  id = ui::kColorAshSystemUIMenuBackground;
  if (use_ash_system_ui_layout_) {
    shadow_type = BubbleBorder::CHROMEOS_SYSTEM_UI_SHADOW;
  }
#endif
  id = border_color_id_.value_or(id);
  auto bubble_border = std::make_unique<BubbleBorder>(arrow_, shadow_type, id);
  const MenuConfig& menu_config = MenuConfig::instance();
  bubble_border->set_md_shadow_elevation(
      content_view_->GetMenuItem()->GetParentMenuItem()
          ? menu_config.bubble_submenu_shadow_elevation
          : menu_config.bubble_menu_shadow_elevation);
  bubble_border->set_draw_border_stroke(menu_config.use_outer_border);

  const int border_radius = GetCornerRadius();
  if (use_ash_system_ui_layout_ || border_radius) {
    if (const auto* const menu_controller =
            content_view_->GetMenuItem()->GetMenuController();
        use_ash_system_ui_layout_ && menu_controller &&
        menu_controller->rounded_corners().has_value()) {
      bubble_border->set_rounded_corners(GetRoundedCorners());
    } else {
      bubble_border->SetCornerRadius(border_radius);
    }
  }

  // The border stroke is expected to appear inside the "border size" region,
  // but BubbleBorder outsets it.
  const gfx::Insets border_thickness(
      menu_config.use_outer_border ? BubbleBorder::kBorderThicknessDip : 0);
  outside_border_insets_ = bubble_border->GetInsets() - border_thickness;
  additional_insets_ =
      gfx::Insets::VH(
          use_ash_system_ui_layout_
              ? menu_config.vertical_touchable_menu_item_padding
              : menu_config.rounded_menu_vertical_border_size.value_or(
                    border_radius),
          menu_config.menu_horizontal_border_size) -
      // Omit any portion of the bubble border insets that are inside the
      // border, lest we double-count them.
      border_thickness;
  if (GetFootnote()) {
    additional_insets_.set_bottom(0);
  }

  corner_radius_ = bubble_border->corner_radius();
  // If the menu uses Ash system UI layout, use `background_view` to build a
  // blurry background with highlight border. Otherwise, use default
  // BubbleBackground.
  if (use_ash_system_ui_layout_) {
    // The bubble border only draws the shadow, while the background view draws
    // the visible border, so the insets must be inside the background view's
    // content, not outside the background view.
    scroll_view_->GetContents()->SetBorder(
        CreateEmptyBorder(std::exchange(additional_insets_, {})));

    background_view_->SetBackground(
        CreateThemedRoundedRectBackground(id, corner_radius_));
    background_view_->layer()->SetRoundedCornerRadius(GetRoundedCorners());

#if BUILDFLAG(IS_CHROMEOS_ASH)
    background_view_->SetBorder(std::make_unique<HighlightBorder>(
        GetRoundedCorners(), HighlightBorder::Type::kHighlightBorderOnShadow));
#endif
  } else {
    SetBackground(std::make_unique<BubbleBackground>(bubble_border.get()));
  }
  SetBorder(std::move(bubble_border));
}

BubbleBorder::Arrow MenuScrollViewContainer::BubbleBorderTypeFromAnchor(
    MenuAnchorPosition anchor) {
  switch (anchor) {
    case MenuAnchorPosition::kTopLeft:
    case MenuAnchorPosition::kTopRight:
    case MenuAnchorPosition::kBottomCenter:
      return BubbleBorder::NONE;
    case MenuAnchorPosition::kBubbleTopLeft:
    case MenuAnchorPosition::kBubbleTopRight:
    case MenuAnchorPosition::kBubbleLeft:
    case MenuAnchorPosition::kBubbleRight:
    case MenuAnchorPosition::kBubbleBottomLeft:
    case MenuAnchorPosition::kBubbleBottomRight:
      return BubbleBorder::FLOAT;
  }
}

BEGIN_METADATA(MenuScrollViewContainer)
END_METADATA

}  // namespace views
