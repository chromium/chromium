// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_item_view.h"

#include <math.h>
#include <stddef.h>

#include <algorithm>
#include <memory>
#include <numeric>
#include <utility>

#include "base/containers/adapters.h"
#include "base/i18n/case_conversion.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_utils.h"
#include "ui/native_theme/native_theme.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_image_util.h"
#include "ui/views/controls/menu/menu_scroll_view_container.h"
#include "ui/views/controls/menu/menu_separator.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/style/typography.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/views_features.h"
#include "ui/views/widget/widget.h"

namespace views {

namespace {

// Returns the appropriate font to use for the "new" badge based on the font
// currently being used to render the title of the menu item.
gfx::FontList DeriveNewBadgeFont(const gfx::FontList& primary_font) {
  // Preferred font is slightly smaller and slightly more bold than the title
  // font. The size change is required to make it look correct in the badge; we
  // add a small degree of bold to prevent color smearing/blurring due to font
  // smoothing. This ensures readability on all platforms and in both light and
  // dark modes.
  return primary_font.Derive(MenuConfig::kNewBadgeFontSizeAdjustment,
                             gfx::Font::NORMAL, gfx::Font::Weight::MEDIUM);
}

// Returns the horizontal space required for the "new" badge.
int GetNewBadgeRequiredWidth(const gfx::FontList& primary_font) {
  const base::string16 new_text =
      l10n_util::GetStringUTF16(IDS_MENU_ITEM_NEW_BADGE);
  gfx::FontList badge_font = DeriveNewBadgeFont(primary_font);
  return gfx::GetStringWidth(new_text, badge_font) +
         2 * MenuConfig::kNewBadgeInternalPadding +
         2 * MenuConfig::kNewBadgeHorizontalMargin;
}

// Returns the highlight rect for the "new" badge given the font and text rect
// for the badge text.
gfx::Rect GetNewBadgeRectOutsetAroundText(const gfx::FontList& badge_font,
                                          const gfx::Rect& badge_text_rect) {
  gfx::Rect badge_rect = badge_text_rect;
  badge_rect.Inset(-gfx::AdjustVisualBorderForFont(
      badge_font, gfx::Insets(MenuConfig::kNewBadgeInternalPadding)));
  return badge_rect;
}

// EmptyMenuMenuItem ---------------------------------------------------------

// EmptyMenuMenuItem is used when a menu has no menu items. EmptyMenuMenuItem
// is itself a MenuItemView, but it uses a different ID so that it isn't
// identified as a MenuItemView.

class EmptyMenuMenuItem : public MenuItemView {
 public:
  explicit EmptyMenuMenuItem(MenuItemView* parent)
      : MenuItemView(parent, 0, Type::kEmpty) {
    // Set this so that we're not identified as a normal menu item.
    SetID(kEmptyMenuItemViewID);
    SetTitle(l10n_util::GetStringUTF16(IDS_APP_MENU_EMPTY_SUBMENU));
    SetEnabled(false);
  }

  base::string16 GetTooltipText(const gfx::Point& p) const override {
    // Empty menu items shouldn't have a tooltip.
    return base::string16();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(EmptyMenuMenuItem);
};

}  // namespace

// Padding between child views.
static constexpr int kChildXPadding = 8;

// MenuItemView ---------------------------------------------------------------

// static
const int MenuItemView::kMenuItemViewID = 1001;

// static
const int MenuItemView::kEmptyMenuItemViewID =
    MenuItemView::kMenuItemViewID + 1;

// static
int MenuItemView::icon_area_width_ = 0;

// static
int MenuItemView::label_start_;

// static
int MenuItemView::item_right_margin_;

// static
int MenuItemView::pref_menu_height_;

MenuItemView::MenuItemView(MenuDelegate* delegate) : delegate_(delegate) {
  // NOTE: don't check the delegate for NULL, UpdateMenuPartSizes() supplies a
  // NULL delegate.
  Init(nullptr, 0, Type::kSubMenu);
}

void MenuItemView::ChildPreferredSizeChanged(View* child) {
  invalidate_dimensions();
  PreferredSizeChanged();
}

base::string16 MenuItemView::GetTooltipText(const gfx::Point& p) const {
  if (!tooltip_.empty())
    return tooltip_;

  if (type_ == Type::kSeparator)
    return base::string16();

  const MenuController* controller = GetMenuController();
  if (!controller ||
      controller->exit_type() != MenuController::ExitType::kNone) {
    // Either the menu has been closed or we're in the process of closing the
    // menu. Don't attempt to query the delegate as it may no longer be valid.
    return base::string16();
  }

  const MenuItemView* root_menu_item = GetRootMenuItem();
  if (root_menu_item->canceled_) {
    // TODO(sky): if |canceled_| is true, controller->exit_type() should be
    // something other than ExitType::kNone, but crash reports seem to indicate
    // otherwise. Figure out why this is needed.
    return base::string16();
  }

  const MenuDelegate* delegate = GetDelegate();
  if (!delegate)
    return base::string16();

  gfx::Point location(p);
  ConvertPointToScreen(this, &location);
  return delegate->GetTooltipText(command_, location);
}

void MenuItemView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  // Set the role based on the type of menu item.
  switch (type_) {
    case Type::kCheckbox:
      node_data->role = ax::mojom::Role::kMenuItemCheckBox;
      break;
    case Type::kRadio:
      node_data->role = ax::mojom::Role::kMenuItemRadio;
      break;
    default:
      node_data->role = ax::mojom::Role::kMenuItem;
      break;
  }

  base::string16 item_text;
  if (IsContainer()) {
    // The first child is taking over, just use its accessible name instead of
    // |title_|.
    View* child = children().front();
    ui::AXNodeData child_node_data;
    child->GetAccessibleNodeData(&child_node_data);
    item_text =
        child_node_data.GetString16Attribute(ax::mojom::StringAttribute::kName);
  } else {
    item_text = title_;
  }
  node_data->SetName(GetAccessibleNameForMenuItem(item_text, GetMinorText(),
                                                  ShouldShowNewBadge()));

  switch (type_) {
    case Type::kSubMenu:
    case Type::kActionableSubMenu:
      node_data->SetHasPopup(ax::mojom::HasPopup::kMenu);
      break;
    case Type::kCheckbox:
    case Type::kRadio: {
      const bool is_checked =
          GetDelegate() && GetDelegate()->IsItemChecked(GetCommand());
      node_data->SetCheckedState(is_checked ? ax::mojom::CheckedState::kTrue
                                            : ax::mojom::CheckedState::kFalse);
    } break;
    case Type::kTitle:
    case Type::kNormal:
    case Type::kSeparator:
    case Type::kEmpty:
    case Type::kHighlighted:
      // No additional accessibility states currently for these menu states.
      break;
  }

  base::char16 mnemonic = GetMnemonic();
  if (mnemonic != '\0') {
    node_data->AddStringAttribute(
        ax::mojom::StringAttribute::kKeyShortcuts,
        base::UTF16ToUTF8(base::string16(1, mnemonic)));
  }
}

bool MenuItemView::HandleAccessibleAction(const ui::AXActionData& action_data) {
  if (action_data.action != ax::mojom::Action::kDoDefault)
    return View::HandleAccessibleAction(action_data);

  // kDoDefault in View would simulate a mouse click in the center of this
  // MenuItemView. However, mouse events for menus are dispatched via
  // Widget::SetCapture() to the MenuController rather than to MenuItemView, so
  // there is no effect. VKEY_RETURN provides a better UX anyway, since it will
  // move focus to a submenu.
  ui::KeyEvent event(ui::ET_KEY_PRESSED, ui::VKEY_RETURN, ui::DomCode::ENTER,
                     ui::EF_NONE, ui::DomKey::ENTER, ui::EventTimeForNow());
  GetMenuController()->SetSelection(this, MenuController::SELECTION_DEFAULT);
  GetMenuController()->OnWillDispatchKeyEvent(&event);
  return true;
}

// static
bool MenuItemView::IsBubble(MenuAnchorPosition anchor) {
  return anchor == MenuAnchorPosition::kBubbleAbove ||
         anchor == MenuAnchorPosition::kBubbleLeft ||
         anchor == MenuAnchorPosition::kBubbleRight;
}

// static
base::string16 MenuItemView::GetAccessibleNameForMenuItem(
    const base::string16& item_text,
    const base::string16& minor_text,
    bool is_new_feature) {
  base::string16 accessible_name = item_text;

  // Filter out the "&" for accessibility clients.
  size_t index = 0;
  const base::char16 amp = '&';
  while ((index = accessible_name.find(amp, index)) != base::string16::npos &&
         index + 1 < accessible_name.length()) {
    accessible_name.replace(index, accessible_name.length() - index,
                            accessible_name.substr(index + 1));

    // Special case for "&&" (escaped for "&").
    if (accessible_name[index] == '&')
      ++index;
  }

  // Append subtext.
  if (!minor_text.empty()) {
    accessible_name.push_back(' ');
    accessible_name.append(minor_text);
  }

  if (is_new_feature) {
    accessible_name.push_back(' ');
    accessible_name.append(l10n_util::GetStringUTF16(
        IDS_MENU_ITEM_NEW_BADGE_SCREEN_READER_MESSAGE));
  }

  return accessible_name;
}

void MenuItemView::Cancel() {
  if (controller_ && !canceled_) {
    canceled_ = true;
    controller_->Cancel(MenuController::ExitType::kAll);
  }
}

MenuItemView* MenuItemView::AddMenuItemAt(
    int index,
    int item_id,
    const base::string16& label,
    const base::string16& secondary_label,
    const base::string16& minor_text,
    const ui::ThemedVectorIcon& minor_icon,
    const gfx::ImageSkia& icon,
    const ui::ThemedVectorIcon& vector_icon,
    Type type,
    ui::MenuSeparatorType separator_style) {
  DCHECK_NE(type, Type::kEmpty);
  DCHECK_GE(index, 0);
  if (!submenu_)
    CreateSubmenu();
  DCHECK_LE(size_t{index}, submenu_->children().size());
  if (type == Type::kSeparator) {
    submenu_->AddChildViewAt(std::make_unique<MenuSeparator>(separator_style),
                             index);
    return nullptr;
  }
  MenuItemView* item = new MenuItemView(this, item_id, type);
  if (label.empty() && GetDelegate())
    item->SetTitle(GetDelegate()->GetLabel(item_id));
  else
    item->SetTitle(label);
  item->SetSecondaryTitle(secondary_label);
  item->SetMinorText(minor_text);
  item->SetMinorIcon(minor_icon);
  if (!vector_icon.empty()) {
    DCHECK(icon.isNull());
    item->SetIcon(vector_icon);
  }
  if (!icon.isNull())
    item->SetIcon(icon);
  if (type == Type::kSubMenu || type == Type::kActionableSubMenu)
    item->CreateSubmenu();
  if (type == Type::kHighlighted) {
    const MenuConfig& config = MenuConfig::instance();
    item->SetMargins(config.footnote_vertical_margin,
                     config.footnote_vertical_margin);
  }
  if (GetDelegate() && !GetDelegate()->IsCommandVisible(item_id))
    item->SetVisible(false);
  return submenu_->AddChildViewAt(item, index);
}

void MenuItemView::RemoveMenuItem(View* item) {
  DCHECK(item);
  DCHECK(submenu_);
  DCHECK_EQ(submenu_, item->parent());
  removed_items_.push_back(item);
  submenu_->RemoveChildView(item);
}

void MenuItemView::RemoveAllMenuItems() {
  DCHECK(submenu_);

  removed_items_.insert(removed_items_.end(), submenu_->children().begin(),
                        submenu_->children().end());

  submenu_->RemoveAllChildViews(false);
}

MenuItemView* MenuItemView::AppendMenuItem(int item_id,
                                           const base::string16& label,
                                           const gfx::ImageSkia& icon) {
  return AppendMenuItemImpl(item_id, label, icon, Type::kNormal);
}

MenuItemView* MenuItemView::AppendSubMenu(int item_id,
                                          const base::string16& label,
                                          const gfx::ImageSkia& icon) {
  return AppendMenuItemImpl(item_id, label, icon, Type::kSubMenu);
}

void MenuItemView::AppendSeparator() {
  AppendMenuItemImpl(0, base::string16(), gfx::ImageSkia(), Type::kSeparator);
}

void MenuItemView::AddSeparatorAt(int index) {
  AddMenuItemAt(index, /*item_id=*/0, /*label=*/base::string16(),
                /*secondary_label=*/base::string16(),
                /*minor_text=*/base::string16(),
                /*minor_icon=*/ui::ThemedVectorIcon(),
                /*icon=*/gfx::ImageSkia(),
                /*vector_icon=*/ui::ThemedVectorIcon(),
                /*type=*/Type::kSeparator,
                /*separator_style=*/ui::NORMAL_SEPARATOR);
}

MenuItemView* MenuItemView::AppendMenuItemImpl(int item_id,
                                               const base::string16& label,
                                               const gfx::ImageSkia& icon,
                                               Type type) {
  const int index = submenu_ ? int{submenu_->children().size()} : 0;
  return AddMenuItemAt(index, item_id, label, base::string16(),
                       base::string16(), ui::ThemedVectorIcon(), icon,
                       ui::ThemedVectorIcon(), type, ui::NORMAL_SEPARATOR);
}

SubmenuView* MenuItemView::CreateSubmenu() {
  if (!submenu_) {
    submenu_ = new SubmenuView(this);

    // Initialize the submenu indicator icon (arrow).
    submenu_arrow_image_view_ = AddChildView(std::make_unique<ImageView>());
  }

  return submenu_;
}

bool MenuItemView::HasSubmenu() const {
  return (submenu_ != nullptr);
}

SubmenuView* MenuItemView::GetSubmenu() const {
  return submenu_;
}

bool MenuItemView::SubmenuIsShowing() const {
  return HasSubmenu() && GetSubmenu()->IsShowing();
}

void MenuItemView::SetTitle(const base::string16& title) {
  title_ = title;
  invalidate_dimensions();  // Triggers preferred size recalculation.
}

void MenuItemView::SetSecondaryTitle(const base::string16& secondary_title) {
  secondary_title_ = secondary_title;
  invalidate_dimensions();  // Triggers preferred size recalculation.
}

void MenuItemView::SetMinorText(const base::string16& minor_text) {
  minor_text_ = minor_text;
  invalidate_dimensions();  // Triggers preferred size recalculation.
}

void MenuItemView::SetMinorIcon(const ui::ThemedVectorIcon& minor_icon) {
  minor_icon_ = minor_icon;
  invalidate_dimensions();  // Triggers preferred size recalculation.
}

void MenuItemView::SetSelected(bool selected) {
  selected_ = selected;
  OnPropertyChanged(&selected_, kPropertyEffectsPaint);
}

PropertyChangedSubscription MenuItemView::AddSelectedChangedCallback(
    PropertyChangedCallback callback) {
  return AddPropertyChangedCallback(&selected_, std::move(callback));
}

void MenuItemView::SetSelectionOfActionableSubmenu(
    bool submenu_area_of_actionable_submenu_selected) {
  DCHECK_EQ(Type::kActionableSubMenu, type_);
  if (submenu_area_of_actionable_submenu_selected_ ==
      submenu_area_of_actionable_submenu_selected) {
    return;
  }

  submenu_area_of_actionable_submenu_selected_ =
      submenu_area_of_actionable_submenu_selected;
  SchedulePaint();
}

void MenuItemView::SetTooltip(const base::string16& tooltip, int item_id) {
  MenuItemView* item = GetMenuItemByID(item_id);
  DCHECK(item);
  item->tooltip_ = tooltip;
}

void MenuItemView::SetIcon(const gfx::ImageSkia& icon, int item_id) {
  MenuItemView* item = GetMenuItemByID(item_id);
  DCHECK(item);
  item->SetIcon(icon);
}

void MenuItemView::SetIcon(const gfx::ImageSkia& icon) {
  vector_icon_.clear();

  if (icon.isNull()) {
    SetIconView(nullptr);
    return;
  }

  auto icon_view = std::make_unique<ImageView>();
  icon_view->SetImage(&icon);
  SetIconView(std::move(icon_view));
}

void MenuItemView::SetIcon(const ui::ThemedVectorIcon& icon) {
  vector_icon_ = icon;
  UpdateIconViewFromVectorIconAndTheme();
}

void MenuItemView::UpdateIconViewFromVectorIconAndTheme() {
  if (vector_icon_.empty())
    return;

  auto icon_view = std::make_unique<ImageView>();
  icon_view->SetImage(vector_icon_.GetImageSkia(GetNativeTheme()));
  SetIconView(std::move(icon_view));
}

void MenuItemView::SetIconView(std::unique_ptr<ImageView> icon_view) {
  if (icon_view_) {
    RemoveChildViewT(icon_view_);
    icon_view_ = nullptr;
  }

  if (icon_view)
    icon_view_ = AddChildView(std::move(icon_view));

  InvalidateLayout();
  SchedulePaint();
}

void MenuItemView::OnPaint(gfx::Canvas* canvas) {
  PaintButton(canvas, PaintButtonMode::kNormal);
}

gfx::Size MenuItemView::CalculatePreferredSize() const {
  const MenuItemDimensions& dimensions(GetDimensions());
  return gfx::Size(dimensions.standard_width + dimensions.children_width,
                   dimensions.height);
}

int MenuItemView::GetHeightForWidth(int width) const {
  // If this isn't a container, we can just use the preferred size's height.
  if (!IsContainer())
    return GetPreferredSize().height();

  const gfx::Insets margins = GetContainerMargins();
  int height = children().front()->GetHeightForWidth(width - margins.width());
  if (!icon_view_ && GetRootMenuItem()->has_icons())
    height = std::max(height, MenuConfig::instance().check_height);

  height += margins.height();

  return height;
}

void MenuItemView::OnThemeChanged() {
  View::OnThemeChanged();
  UpdateIconViewFromVectorIconAndTheme();
}

gfx::Rect MenuItemView::GetSubmenuAreaOfActionableSubmenu() const {
  DCHECK_EQ(Type::kActionableSubMenu, type_);
  const MenuConfig& config = MenuConfig::instance();
  return gfx::Rect(gfx::Point(vertical_separator_->bounds().right(), 0),
                   gfx::Size(config.actionable_submenu_width, height()));
}

const MenuItemView::MenuItemDimensions& MenuItemView::GetDimensions() const {
  if (!is_dimensions_valid())
    dimensions_ = CalculateDimensions();
  DCHECK(is_dimensions_valid());
  return dimensions_;
}

MenuController* MenuItemView::GetMenuController() {
  return GetRootMenuItem()->controller_.get();
}

const MenuController* MenuItemView::GetMenuController() const {
  return GetRootMenuItem()->controller_.get();
}

MenuDelegate* MenuItemView::GetDelegate() {
  return GetRootMenuItem()->delegate_;
}

const MenuDelegate* MenuItemView::GetDelegate() const {
  return GetRootMenuItem()->delegate_;
}

MenuItemView* MenuItemView::GetRootMenuItem() {
  return const_cast<MenuItemView*>(
      static_cast<const MenuItemView*>(this)->GetRootMenuItem());
}

const MenuItemView* MenuItemView::GetRootMenuItem() const {
  const MenuItemView* item = this;
  for (const MenuItemView* parent = GetParentMenuItem(); parent;
       parent = item->GetParentMenuItem())
    item = parent;
  return item;
}

base::char16 MenuItemView::GetMnemonic() {
  if (!GetRootMenuItem()->has_mnemonics_ ||
      !MenuConfig::instance().use_mnemonics) {
    return 0;
  }

  size_t index = 0;
  do {
    index = title_.find('&', index);
    if (index != base::string16::npos) {
      if (index + 1 != title_.size() && title_[index + 1] != '&') {
        base::char16 char_array[] = {title_[index + 1], 0};
        // TODO(jshin): What about Turkish locale? See http://crbug.com/81719.
        // If the mnemonic is capital I and the UI language is Turkish,
        // lowercasing it results in 'small dotless i', which is different
        // from a 'dotted i'. Similar issues may exist for az and lt locales.
        return base::i18n::ToLower(char_array)[0];
      }
      index++;
    }
  } while (index != base::string16::npos);
  return 0;
}

MenuItemView* MenuItemView::GetMenuItemByID(int id) {
  if (GetCommand() == id)
    return this;
  if (!HasSubmenu())
    return nullptr;
  for (MenuItemView* item : GetSubmenu()->GetMenuItems()) {
    MenuItemView* result = item->GetMenuItemByID(id);
    if (result)
      return result;
  }
  return nullptr;
}

void MenuItemView::ChildrenChanged() {
  MenuController* controller = GetMenuController();
  if (controller) {
    // Handles the case where we were empty and are no longer empty.
    RemoveEmptyMenus();

    // Handles the case where we were not empty, but now are.
    AddEmptyMenus();

    controller->MenuChildrenChanged(this);

    if (submenu_) {
      // Force a paint and a synchronous layout. This needs a synchronous layout
      // as UpdateSubmenuSelection() looks at bounds. This handles the case of
      // the top level window's size remaining the same, resulting in no change
      // to the submenu's size and no layout.
      submenu_->Layout();
      submenu_->SchedulePaint();
      // Update the menu selection after layout.
      controller->UpdateSubmenuSelection(submenu_);
    }
  }

  for (auto* item : removed_items_)
    delete item;
  removed_items_.clear();
}

void MenuItemView::Layout() {
  if (children().empty())
    return;

  if (IsContainer()) {
    // This MenuItemView is acting as a thin wrapper around the sole child view,
    // and the size has already been set appropriately for the child's preferred
    // size and margins. The child's bounds can simply be set to the content
    // bounds, less the margins.
    gfx::Rect bounds = GetContentsBounds();
    bounds.Inset(GetContainerMargins());
    children().front()->SetBoundsRect(bounds);
  } else {
    // Child views are laid out right aligned and given the full height. To
    // right align start with the last view and progress to the first.
    int child_x = width() - (use_right_margin_ ? item_right_margin_ : 0);
    for (View* child : base::Reversed(children())) {
      if (icon_view_ == child)
        continue;
      if (radio_check_image_view_ == child)
        continue;
      if (submenu_arrow_image_view_ == child)
        continue;
      if (vertical_separator_ == child)
        continue;
      int width = child->GetPreferredSize().width();
      child->SetBounds(child_x - width, 0, width, height());
      child_x -= width + kChildXPadding;
    }
    // Position |icon_view|.
    const MenuConfig& config = MenuConfig::instance();
    if (icon_view_) {
      icon_view_->SizeToPreferredSize();
      gfx::Size size = icon_view_->GetPreferredSize();
      int x = config.item_horizontal_padding + left_icon_margin_ +
              (icon_area_width_ - size.width()) / 2;
      if (config.icons_in_label || type_ == Type::kCheckbox ||
          type_ == Type::kRadio)
        x = label_start_;
      if (GetMenuController() && GetMenuController()->use_touchable_layout())
        x = config.touchable_item_horizontal_padding;

      int y =
          (height() + GetTopMargin() - GetBottomMargin() - size.height()) / 2;
      icon_view_->SetPosition(gfx::Point(x, y));
    }

    if (radio_check_image_view_) {
      int x = config.item_horizontal_padding + left_icon_margin_;
      if (GetMenuController() && GetMenuController()->use_touchable_layout())
        x = config.touchable_item_horizontal_padding;
      int y =
          (height() + GetTopMargin() - GetBottomMargin() - kMenuCheckSize) / 2;
      radio_check_image_view_->SetBounds(x, y, kMenuCheckSize, kMenuCheckSize);
    }

    if (submenu_arrow_image_view_) {
      int x = width() - config.arrow_width -
              (type_ == Type::kActionableSubMenu
                   ? config.actionable_submenu_arrow_to_edge_padding
                   : config.arrow_to_edge_padding);
      int y =
          (height() + GetTopMargin() - GetBottomMargin() - kSubmenuArrowSize) /
          2;
      submenu_arrow_image_view_->SetBounds(x, y, config.arrow_width,
                                           kSubmenuArrowSize);
    }

    if (vertical_separator_) {
      const gfx::Size preferred_size = vertical_separator_->GetPreferredSize();
      int x = width() - config.actionable_submenu_width -
              config.actionable_submenu_vertical_separator_width;
      int y = (height() - preferred_size.height()) / 2;
      vertical_separator_->SetBoundsRect(
          gfx::Rect(gfx::Point(x, y), preferred_size));
    }
  }
}

void MenuItemView::SetMargins(int top_margin, int bottom_margin) {
  top_margin_ = top_margin;
  bottom_margin_ = bottom_margin;

  invalidate_dimensions();
}

void MenuItemView::SetForcedVisualSelection(bool selected) {
  forced_visual_selection_ = selected;
  SchedulePaint();
}

void MenuItemView::SetCornerRadius(int radius) {
  DCHECK_EQ(Type::kHighlighted, type_);
  corner_radius_ = radius;
  invalidate_dimensions();  // Triggers preferred size recalculation.
}

void MenuItemView::SetAlerted() {
  is_alerted_ = true;
  SchedulePaint();
}

bool MenuItemView::ShouldShowNewBadge() const {
  static const bool feature_enabled =
      base::FeatureList::IsEnabled(features::kEnableNewBadgeOnMenuItems);
  return feature_enabled && is_new_;
}

MenuItemView::MenuItemView(MenuItemView* parent,
                           int command,
                           MenuItemView::Type type) {
  Init(parent, command, type);
}

MenuItemView::~MenuItemView() {
  if (GetMenuController())
    GetMenuController()->OnMenuItemDestroying(this);
  delete submenu_;
  for (auto* item : removed_items_)
    delete item;
}

// Calculates all sizes that we can from the OS.
//
// This is invoked prior to Running a menu.
void MenuItemView::UpdateMenuPartSizes() {
  const MenuConfig& config = MenuConfig::instance();

  item_right_margin_ = config.label_to_arrow_padding + config.arrow_width +
                       config.arrow_to_edge_padding;
  icon_area_width_ = config.check_width;
  if (has_icons_)
    icon_area_width_ = std::max(icon_area_width_, GetMaxIconViewWidth());

  const bool use_touchable_layout =
      GetMenuController() && GetMenuController()->use_touchable_layout();
  label_start_ =
      (use_touchable_layout ? config.touchable_item_horizontal_padding
                            : config.item_horizontal_padding) +
      icon_area_width_;
  int padding = 0;
  if (config.always_use_icon_to_label_padding) {
    padding = LayoutProvider::Get()->GetDistanceMetric(
        DISTANCE_RELATED_LABEL_HORIZONTAL);
  } else if (!config.icons_in_label) {
    padding = (has_icons_ || HasChecksOrRadioButtons())
                  ? LayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_RELATED_LABEL_HORIZONTAL)
                  : 0;
  }
  if (use_touchable_layout)
    padding = LayoutProvider::Get()->GetDistanceMetric(
        DISTANCE_RELATED_LABEL_HORIZONTAL);

  label_start_ += padding;

  EmptyMenuMenuItem menu_item(this);
  menu_item.set_controller(GetMenuController());
  pref_menu_height_ = menu_item.GetPreferredSize().height();

  UpdateIconViewFromVectorIconAndTheme();
}

void MenuItemView::Init(MenuItemView* parent,
                        int command,
                        MenuItemView::Type type) {
  parent_menu_item_ = parent;
  type_ = type;
  command_ = command;
  // Assign our ID, this allows SubmenuItemView to find MenuItemViews.
  SetID(kMenuItemViewID);
  has_icons_ = false;

  if (type_ == Type::kCheckbox || type_ == Type::kRadio) {
    radio_check_image_view_ = AddChildView(std::make_unique<ImageView>());
    bool show_check_radio_icon =
        type_ == Type::kRadio || (type_ == Type::kCheckbox && GetDelegate() &&
                                  GetDelegate()->IsItemChecked(GetCommand()));
    radio_check_image_view_->SetVisible(show_check_radio_icon);
    radio_check_image_view_->SetCanProcessEventsWithinSubtree(false);
  }

  if (type_ == Type::kActionableSubMenu) {
    vertical_separator_ = AddChildView(std::make_unique<Separator>());
    vertical_separator_->SetVisible(true);
    vertical_separator_->SetFocusBehavior(FocusBehavior::NEVER);
    const MenuConfig& config = MenuConfig::instance();
    vertical_separator_->SetColor(GetNativeTheme()->GetSystemColor(
        ui::NativeTheme::kColorId_MenuSeparatorColor));
    vertical_separator_->SetPreferredSize(
        gfx::Size(config.actionable_submenu_vertical_separator_width,
                  config.actionable_submenu_vertical_separator_height));
    vertical_separator_->SetCanProcessEventsWithinSubtree(false);
  }

  if (submenu_arrow_image_view_)
    submenu_arrow_image_view_->SetVisible(HasSubmenu());

  // Don't request enabled status from the root menu item as it is just
  // a container for real items. kEmpty items will be disabled.
  MenuDelegate* root_delegate = GetDelegate();
  if (parent && type != Type::kEmpty && root_delegate)
    SetEnabled(root_delegate->IsCommandEnabled(command));
}

void MenuItemView::PrepareForRun(bool is_first_menu,
                                 bool has_mnemonics,
                                 bool show_mnemonics) {
  // Currently we only support showing the root.
  DCHECK(!parent_menu_item_);

  // Force us to have a submenu.
  CreateSubmenu();
  actual_menu_position_ = requested_menu_position_;
  canceled_ = false;

  has_mnemonics_ = has_mnemonics;
  show_mnemonics_ = has_mnemonics && show_mnemonics;

  AddEmptyMenus();

  if (is_first_menu) {
    // Only update the menu size if there are no menus showing, otherwise
    // things may shift around.
    UpdateMenuPartSizes();
  }
}

int MenuItemView::GetDrawStringFlags() {
  int flags = 0;
  if (base::i18n::IsRTL())
    flags |= gfx::Canvas::TEXT_ALIGN_RIGHT;
  else
    flags |= gfx::Canvas::TEXT_ALIGN_LEFT;

  if (GetRootMenuItem()->has_mnemonics_) {
    if (MenuConfig::instance().show_mnemonics ||
        GetRootMenuItem()->show_mnemonics_) {
      flags |= gfx::Canvas::SHOW_PREFIX;
    } else {
      flags |= gfx::Canvas::HIDE_PREFIX;
    }
  }
  return flags;
}

void MenuItemView::GetLabelStyle(MenuDelegate::LabelStyle* style) const {
  // Start with the default font:
  style->font_list = MenuConfig::instance().font_list;

  // Replace it with the touchable font in touchable menus:
  if (GetMenuController() && GetMenuController()->use_touchable_layout()) {
    style->font_list =
        style::GetFont(style::CONTEXT_TOUCH_MENU, style::STYLE_PRIMARY);
  }

  // Then let the delegate replace any part of |style|.
  const MenuDelegate* delegate = GetDelegate();
  if (delegate)
    delegate->GetLabelStyle(GetCommand(), style);
}

void MenuItemView::AddEmptyMenus() {
  DCHECK(HasSubmenu());
  if (!submenu_->HasVisibleChildren() && !submenu_->HasEmptyMenuItemView()) {
    submenu_->AddChildViewAt(std::make_unique<EmptyMenuMenuItem>(this), 0);
  } else {
    for (MenuItemView* item : submenu_->GetMenuItems()) {
      if (item->HasSubmenu())
        item->AddEmptyMenus();
    }
  }
}

void MenuItemView::RemoveEmptyMenus() {
  DCHECK(HasSubmenu());
  // Copy the children, since we may mutate them as we go.
  const Views children = submenu_->children();
  for (View* child : children) {
    if (child->GetID() == MenuItemView::kMenuItemViewID) {
      MenuItemView* menu_item = static_cast<MenuItemView*>(child);
      if (menu_item->HasSubmenu())
        menu_item->RemoveEmptyMenus();
    } else if (child->GetID() == EmptyMenuMenuItem::kEmptyMenuItemViewID) {
      submenu_->RemoveChildView(child);
      delete child;
    }
  }
}

void MenuItemView::AdjustBoundsForRTLUI(gfx::Rect* rect) const {
  rect->set_x(GetMirroredXForRect(*rect));
}

void MenuItemView::PaintButton(gfx::Canvas* canvas, PaintButtonMode mode) {
  const MenuConfig& config = MenuConfig::instance();
  bool render_selection =
      (mode == PaintButtonMode::kNormal && IsSelected() &&
       parent_menu_item_->GetSubmenu()->GetShowSelection(this) &&
       (NonIconChildViewsCount() == 0));
  if (forced_visual_selection_.has_value())
    render_selection = *forced_visual_selection_;

  // Render the background. As MenuScrollViewContainer draws the background, we
  // only need the background when we want it to look different, as when we're
  // selected.
  PaintBackground(canvas, mode, render_selection);

  // Calculate some colors.
  MenuDelegate::LabelStyle style;
  style.foreground = GetTextColor(/*minor=*/false, render_selection);
  GetLabelStyle(&style);

  SkColor icon_color = color_utils::DeriveDefaultIconColor(style.foreground);

  // Calculate the margins.
  int top_margin = GetTopMargin();
  const int bottom_margin = GetBottomMargin();
  const int available_height = height() - top_margin - bottom_margin;
  const int text_height = style.font_list.GetHeight();
  const int total_text_height =
      secondary_title().empty() ? text_height : text_height * 2;
  top_margin += (available_height - total_text_height) / 2;

  // Render the check.
  MenuDelegate* delegate = GetDelegate();
  if (type_ == Type::kCheckbox && delegate &&
      delegate->IsItemChecked(GetCommand())) {
    radio_check_image_view_->SetImage(GetMenuCheckImage(icon_color));
  } else if (type_ == Type::kRadio) {
    const bool toggled = delegate && delegate->IsItemChecked(GetCommand());
    const gfx::VectorIcon& radio_icon =
        toggled ? kMenuRadioSelectedIcon : kMenuRadioEmptyIcon;
    const SkColor radio_icon_color = GetNativeTheme()->GetSystemColor(
        toggled ? ui::NativeTheme::kColorId_ButtonCheckedColor
                : ui::NativeTheme::kColorId_ButtonUncheckedColor);
    radio_check_image_view_->SetImage(
        gfx::CreateVectorIcon(radio_icon, kMenuCheckSize, radio_icon_color));
  }

  // Render the foreground.
  int accel_width = parent_menu_item_->GetSubmenu()->max_minor_text_width();
  int label_start = GetLabelStartForThisItem();

  int width = this->width() - label_start - accel_width -
              (!delegate || delegate->ShouldReserveSpaceForSubmenuIndicator()
                   ? item_right_margin_
                   : config.arrow_to_edge_padding);
  gfx::Rect text_bounds(label_start, top_margin, width, text_height);
  text_bounds.set_x(GetMirroredXForRect(text_bounds));
  int flags = GetDrawStringFlags();
  if (mode == PaintButtonMode::kForDrag)
    flags |= gfx::Canvas::NO_SUBPIXEL_RENDERING;
  canvas->DrawStringRectWithFlags(title(), style.font_list, style.foreground,
                                  text_bounds, flags);

  // The rest should be drawn with the minor foreground color.
  style.foreground = GetTextColor(/*minor=*/true, render_selection);
  if (!secondary_title().empty()) {
    text_bounds.set_y(text_bounds.y() + text_height);
    canvas->DrawStringRectWithFlags(secondary_title(), style.font_list,
                                    style.foreground, text_bounds, flags);
  }

  PaintMinorIconAndText(canvas, style);

  if (ShouldShowNewBadge()) {
    DrawNewBadge(
        canvas,
        gfx::Point(label_start + gfx::GetStringWidth(title(), style.font_list) +
                       MenuConfig::kNewBadgeHorizontalMargin,
                   top_margin),
        style.font_list, flags);
  }

  // Set the submenu indicator (arrow) image and color.
  if (HasSubmenu())
    submenu_arrow_image_view_->SetImage(GetSubmenuArrowImage(icon_color));
}

void MenuItemView::PaintBackground(gfx::Canvas* canvas,
                                   PaintButtonMode mode,
                                   bool render_selection) {
  if (type_ == Type::kHighlighted || is_alerted_) {
    SkColor color = gfx::kPlaceholderColor;

    if (type_ == Type::kHighlighted) {
      const ui::NativeTheme::ColorId color_id =
          render_selection
              ? ui::NativeTheme::kColorId_FocusedMenuItemBackgroundColor
              : ui::NativeTheme::kColorId_HighlightedMenuItemBackgroundColor;
      color = GetNativeTheme()->GetSystemColor(color_id);
    } else {
      const auto* animation = GetMenuController()->GetAlertAnimation();
      color = gfx::Tween::ColorValueBetween(
          animation->GetCurrentValue(),
          GetNativeTheme()->GetSystemColor(
              ui::NativeTheme::kColorId_MenuItemInitialAlertBackgroundColor),
          GetNativeTheme()->GetSystemColor(
              ui::NativeTheme::kColorId_MenuItemTargetAlertBackgroundColor));
    }

    DCHECK_NE(color, gfx::kPlaceholderColor);

    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(color);
    // Draw a rounded rect that spills outside of the clipping area, so that the
    // rounded corners only show in the bottom 2 corners. Note that
    // |corner_radius_| should only be set when the highlighted item is at the
    // end of the menu.
    gfx::RectF spilling_rect(GetLocalBounds());
    spilling_rect.set_y(spilling_rect.y() - corner_radius_);
    spilling_rect.set_height(spilling_rect.height() + corner_radius_);
    canvas->DrawRoundRect(spilling_rect, corner_radius_, flags);
  } else if (render_selection) {
    gfx::Rect item_bounds = GetLocalBounds();
    if (type_ == Type::kActionableSubMenu) {
      if (submenu_area_of_actionable_submenu_selected_) {
        item_bounds = GetSubmenuAreaOfActionableSubmenu();
      } else {
        item_bounds.set_width(item_bounds.width() -
                              MenuConfig::instance().actionable_submenu_width -
                              1);
      }
    }
    AdjustBoundsForRTLUI(&item_bounds);

    GetNativeTheme()->Paint(
        canvas->sk_canvas(), ui::NativeTheme::kMenuItemBackground,
        ui::NativeTheme::kHovered, item_bounds, ui::NativeTheme::ExtraParams());
  }
}

void MenuItemView::PaintMinorIconAndText(
    gfx::Canvas* canvas,
    const MenuDelegate::LabelStyle& style) {
  base::string16 minor_text = GetMinorText();
  const ui::ThemedVectorIcon minor_icon = GetMinorIcon();
  if (minor_text.empty() && minor_icon.empty())
    return;

  int available_height = height() - GetTopMargin() - GetBottomMargin();
  int max_minor_text_width =
      parent_menu_item_->GetSubmenu()->max_minor_text_width();
  const MenuConfig& config = MenuConfig::instance();
  int minor_text_right_margin = config.align_arrow_and_shortcut
                                    ? config.arrow_to_edge_padding
                                    : item_right_margin_;
  gfx::Rect minor_text_bounds(
      width() - minor_text_right_margin - max_minor_text_width, GetTopMargin(),
      max_minor_text_width, available_height);
  minor_text_bounds.set_x(GetMirroredXForRect(minor_text_bounds));

  std::unique_ptr<gfx::RenderText> render_text =
      gfx::RenderText::CreateRenderText();
  if (!minor_text.empty()) {
    render_text->SetText(minor_text);
    render_text->SetFontList(style.font_list);
    render_text->SetColor(style.foreground);
    render_text->SetDisplayRect(minor_text_bounds);
    render_text->SetHorizontalAlignment(base::i18n::IsRTL() ? gfx::ALIGN_LEFT
                                                            : gfx::ALIGN_RIGHT);
    render_text->Draw(canvas);
  }

  if (!minor_icon.empty()) {
    gfx::ImageSkia image = minor_icon.GetImageSkia(style.foreground);

    int image_x = GetMirroredRect(minor_text_bounds).right() -
                  render_text->GetContentWidth() -
                  (minor_text.empty() ? 0 : config.item_horizontal_padding) -
                  image.width();
    int minor_text_center_y =
        minor_text_bounds.y() + minor_text_bounds.height() / 2;
    int image_y = minor_text_center_y - image.height() / 2;
    canvas->DrawImageInt(
        image, GetMirroredXWithWidthInView(image_x, image.width()), image_y);
  }
}

SkColor MenuItemView::GetTextColor(bool minor, bool render_selection) const {
  style::TextContext context =
      GetMenuController() && GetMenuController()->use_touchable_layout()
          ? style::CONTEXT_TOUCH_MENU
          : style::CONTEXT_MENU;

  style::TextStyle text_style = style::STYLE_PRIMARY;
  if (type_ == Type::kTitle)
    text_style = style::STYLE_PRIMARY;
  else if (type_ == Type::kHighlighted)
    text_style = style::STYLE_HIGHLIGHTED;
  else if (!GetEnabled())
    text_style = style::STYLE_DISABLED;
  else if (render_selection)
    text_style = style::STYLE_SELECTED;
  else if (minor)
    text_style = style::STYLE_SECONDARY;

  return style::GetColor(*this, context, text_style);
}

void MenuItemView::DestroyAllMenuHosts() {
  if (!HasSubmenu())
    return;

  submenu_->Close();
  for (MenuItemView* item : submenu_->GetMenuItems())
    item->DestroyAllMenuHosts();
}

int MenuItemView::GetTopMargin() const {
  int margin = top_margin_;
  if (margin < 0) {
    const MenuItemView* root = GetRootMenuItem();
    margin = root && root->has_icons_
                 ? MenuConfig::instance().item_top_margin
                 : MenuConfig::instance().item_no_icon_top_margin;
  }

  return margin;
}

int MenuItemView::GetBottomMargin() const {
  int margin = bottom_margin_;
  if (margin < 0) {
    const MenuItemView* root = GetRootMenuItem();
    margin = root && root->has_icons_
                 ? MenuConfig::instance().item_bottom_margin
                 : MenuConfig::instance().item_no_icon_bottom_margin;
  }

  return margin;
}

gfx::Size MenuItemView::GetChildPreferredSize() const {
  if (children().empty())
    return gfx::Size();

  if (IsContainer())
    return children().front()->GetPreferredSize();

  const auto add_width = [this](int width, const View* child) {
    if (child == icon_view_ || child == radio_check_image_view_ ||
        child == submenu_arrow_image_view_ || child == vertical_separator_)
      return width;
    if (width)
      width += kChildXPadding;
    return width + child->GetPreferredSize().width();
  };
  const int width =
      std::accumulate(children().cbegin(), children().cend(), 0, add_width);

  // If there is no icon view it returns a height of 0 to indicate that
  // we should use the title height instead.
  const int height = icon_view_ ? icon_view_->GetPreferredSize().height() : 0;

  return gfx::Size(width, height);
}

MenuItemView::MenuItemDimensions MenuItemView::CalculateDimensions() const {
  gfx::Size child_size = GetChildPreferredSize();

  MenuItemDimensions dimensions;
  dimensions.children_width = child_size.width();
  const MenuConfig& menu_config = MenuConfig::instance();

  if (GetMenuController() && GetMenuController()->use_touchable_layout()) {
    dimensions.height = menu_config.touchable_menu_height;

    // For container MenuItemViews, the width components should only include the
    // |children_width|. Setting a |standard_width| would result in additional
    // width being added to the container because the total width used in layout
    // is |children_width| + |standard_width|.
    if (IsContainer())
      return dimensions;

    dimensions.standard_width = menu_config.touchable_menu_width;

    if (icon_view_) {
      dimensions.height = icon_view_->GetPreferredSize().height() +
                          2 * menu_config.vertical_touchable_menu_item_padding;
    }
    return dimensions;
  }

  MenuDelegate::LabelStyle style;
  GetLabelStyle(&style);
  base::string16 minor_text = GetMinorText();

  dimensions.height = child_size.height();
  // Adjust item content height if menu has both items with and without icons.
  // This way all menu items will have the same height.
  if (!icon_view_ && GetRootMenuItem()->has_icons()) {
    dimensions.height =
        std::max(dimensions.height, MenuConfig::instance().check_height);
  }

  // In the container case, only the child size plus margins need to be
  // considered.
  if (IsContainer()) {
    const gfx::Insets margins = GetContainerMargins();
    dimensions.height += margins.height();
    dimensions.children_width += margins.width();
    ApplyMinimumDimensions(&dimensions);
    return dimensions;
  }

  dimensions.height += GetBottomMargin() + GetTopMargin();

  // Get Icon margin overrides for this particular item.
  const MenuDelegate* delegate = GetDelegate();
  if (delegate) {
    delegate->GetHorizontalIconMargins(command_, icon_area_width_,
                                       &left_icon_margin_, &right_icon_margin_);
  } else {
    left_icon_margin_ = 0;
    right_icon_margin_ = 0;
  }
  int label_start = GetLabelStartForThisItem();

  // Determine the length of the label text.
  int string_width = gfx::GetStringWidth(title_, style.font_list);
  dimensions.standard_width = string_width + label_start + item_right_margin_;
  // Determine the length of the right-side text.
  dimensions.minor_text_width =
      (minor_text.empty() ? 0
                          : gfx::GetStringWidth(minor_text, style.font_list));

  if (ShouldShowNewBadge())
    dimensions.minor_text_width += GetNewBadgeRequiredWidth(style.font_list);

  // Determine the height to use.
  int label_text_height = secondary_title().empty()
                              ? style.font_list.GetHeight()
                              : style.font_list.GetHeight() * 2;
  dimensions.height =
      std::max(dimensions.height,
               label_text_height + GetBottomMargin() + GetTopMargin());
  dimensions.height =
      std::max(dimensions.height, MenuConfig::instance().item_min_height);

  ApplyMinimumDimensions(&dimensions);
  return dimensions;
}

void MenuItemView::ApplyMinimumDimensions(MenuItemDimensions* dims) const {
  // Don't apply minimums to menus without controllers or to comboboxes.
  if (!GetMenuController() || GetMenuController()->IsCombobox())
    return;

  // TODO(nicolaso): PaintBackground() doesn't cover the whole area in footnotes
  // when minimum height is set too high. For now, just ignore minimum height
  // for kHighlighted elements.
  if (type_ == Type::kHighlighted)
    return;

  int used =
      dims->standard_width + dims->children_width + dims->minor_text_width;
  const MenuConfig& config = MenuConfig::instance();
  if (used < config.minimum_menu_width)
    dims->standard_width += (config.minimum_menu_width - used);

  dims->height = std::max(dims->height,
                          IsContainer() ? config.minimum_container_item_height
                                        : config.minimum_text_item_height);
}

int MenuItemView::GetLabelStartForThisItem() const {
  const MenuConfig& config = MenuConfig::instance();

  // Touchable items with icons do not respect |label_start_|.
  if (GetMenuController() && GetMenuController()->use_touchable_layout() &&
      icon_view_) {
    return 2 * config.touchable_item_horizontal_padding + icon_view_->width();
  }

  int label_start = label_start_ + left_icon_margin_ + right_icon_margin_;
  if ((config.icons_in_label || type_ == Type::kCheckbox ||
       type_ == Type::kRadio) &&
      icon_view_) {
    label_start +=
        icon_view_->size().width() + LayoutProvider::Get()->GetDistanceMetric(
                                         DISTANCE_RELATED_LABEL_HORIZONTAL);
  }

  return label_start;
}

void MenuItemView::DrawNewBadge(gfx::Canvas* canvas,
                                const gfx::Point& unmirrored_badge_start,
                                const gfx::FontList& primary_font,
                                int text_render_flags) {
  gfx::FontList badge_font = DeriveNewBadgeFont(primary_font);
  const base::string16 new_text =
      l10n_util::GetStringUTF16(IDS_MENU_ITEM_NEW_BADGE);

  // Calculate bounding box for badge text.
  gfx::Rect badge_text_bounds(unmirrored_badge_start,
                              gfx::GetStringSize(new_text, badge_font));
  badge_text_bounds.Offset(
      MenuConfig::kNewBadgeInternalPadding,
      gfx::GetFontCapHeightCenterOffset(primary_font, badge_font));
  if (base::i18n::IsRTL())
    badge_text_bounds.set_x(GetMirroredXForRect(badge_text_bounds));

  // Render the badge itself.
  cc::PaintFlags new_flags;
  const SkColor background_color = GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_ProminentButtonColor);
  new_flags.setColor(background_color);
  canvas->DrawRoundRect(
      GetNewBadgeRectOutsetAroundText(badge_font, badge_text_bounds),
      MenuConfig::kNewBadgeCornerRadius, new_flags);

  // Render the badge text.
  const SkColor foreground_color = GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_TextOnProminentButtonColor);
  canvas->DrawStringRectWithFlags(new_text, badge_font, foreground_color,
                                  badge_text_bounds, text_render_flags);
}

base::string16 MenuItemView::GetMinorText() const {
  if (GetID() == kEmptyMenuItemViewID) {
    // Don't query the delegate for menus that represent no children.
    return base::string16();
  }

  base::string16 accel_text;
  if (MenuConfig::instance().ShouldShowAcceleratorText(this, &accel_text))
    return accel_text;

  return minor_text_;
}

ui::ThemedVectorIcon MenuItemView::GetMinorIcon() const {
  return minor_icon_;
}

bool MenuItemView::IsContainer() const {
  // Let the first child take over |this| when we only have one child and no
  // title.
  return (NonIconChildViewsCount() == 1) && title_.empty();
}

gfx::Insets MenuItemView::GetContainerMargins() const {
  DCHECK(IsContainer());

  // Use the child's preferred margins but take the standard top and bottom
  // margins as minimums.
  const gfx::Insets* margins_prop =
      children().front()->GetProperty(views::kMarginsKey);
  gfx::Insets margins = margins_prop ? *margins_prop : gfx::Insets();
  margins.set_top(std::max(margins.top(), GetTopMargin()));
  margins.set_bottom(std::max(margins.bottom(), GetBottomMargin()));
  return margins;
}

int MenuItemView::NonIconChildViewsCount() const {
  return int{children().size()} - (icon_view_ ? 1 : 0) -
         (radio_check_image_view_ ? 1 : 0) -
         (submenu_arrow_image_view_ ? 1 : 0) - (vertical_separator_ ? 1 : 0);
}

int MenuItemView::GetMaxIconViewWidth() const {
  DCHECK(submenu_);
  const auto menu_items = submenu_->GetMenuItems();
  if (menu_items.empty())
    return 0;

  std::vector<int> widths(menu_items.size());
  const auto get_width = [](MenuItemView* item) {
    if (item->type_ == Type::kCheckbox || item->type_ == Type::kRadio) {
      // If this item has a radio or checkbox, the icon will not affect
      // alignment of other items.
      return 0;
    }
    if (item->HasSubmenu())
      return item->GetMaxIconViewWidth();
    return (item->icon_view_ && !MenuConfig::instance().icons_in_label)
               ? item->icon_view_->GetPreferredSize().width()
               : 0;
  };
  std::transform(menu_items.cbegin(), menu_items.cend(), widths.begin(),
                 get_width);
  return *std::max_element(widths.cbegin(), widths.cend());
}

bool MenuItemView::HasChecksOrRadioButtons() const {
  if (type_ == Type::kCheckbox || type_ == Type::kRadio)
    return true;
  if (!HasSubmenu())
    return false;
  const auto menu_items = submenu_->GetMenuItems();
  return std::any_of(
      menu_items.cbegin(), menu_items.cend(),
      [](const auto* item) { return item->HasChecksOrRadioButtons(); });
}

BEGIN_METADATA(MenuItemView, View)
END_METADATA

}  // namespace views
