// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_item_view.h"

#include <stddef.h>

#include "base/i18n/case_conversion.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_utils.h"
#include "ui/native_theme/common_theme.h"
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
#include "ui/views/widget/widget.h"

namespace views {

namespace {

// EmptyMenuMenuItem ---------------------------------------------------------

// EmptyMenuMenuItem is used when a menu has no menu items. EmptyMenuMenuItem
// is itself a MenuItemView, but it uses a different ID so that it isn't
// identified as a MenuItemView.

class EmptyMenuMenuItem : public MenuItemView {
 public:
  explicit EmptyMenuMenuItem(MenuItemView* parent)
      : MenuItemView(parent, 0, EMPTY) {
    // Set this so that we're not identified as a normal menu item.
    set_id(kEmptyMenuItemViewID);
    SetTitle(l10n_util::GetStringUTF16(IDS_APP_MENU_EMPTY_SUBMENU));
    SetEnabled(false);
  }

  bool GetTooltipText(const gfx::Point& p,
                      base::string16* tooltip) const override {
    // Empty menu items shouldn't have a tooltip.
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(EmptyMenuMenuItem);
};

}  // namespace

// Padding between child views.
static const int kChildXPadding = 8;

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

// static
const char MenuItemView::kViewClassName[] = "MenuItemView";

MenuItemView::MenuItemView(MenuDelegate* delegate)
    : delegate_(delegate),
      controller_(NULL),
      canceled_(false),
      parent_menu_item_(NULL),
      type_(SUBMENU),
      selected_(false),
      command_(0),
      submenu_(NULL),
      has_mnemonics_(false),
      show_mnemonics_(false),
      has_icons_(false),
      icon_view_(NULL),
      top_margin_(-1),
      bottom_margin_(-1),
      left_icon_margin_(0),
      right_icon_margin_(0),
      requested_menu_position_(POSITION_BEST_FIT),
      actual_menu_position_(requested_menu_position_),
      use_right_margin_(true) {
  // NOTE: don't check the delegate for NULL, UpdateMenuPartSizes() supplies a
  // NULL delegate.
  Init(NULL, 0, SUBMENU, delegate);
}

void MenuItemView::ChildPreferredSizeChanged(View* child) {
  invalidate_dimensions();
  PreferredSizeChanged();
}

bool MenuItemView::GetTooltipText(const gfx::Point& p,
                                  base::string16* tooltip) const {
  *tooltip = tooltip_;
  if (!tooltip->empty())
    return true;

  if (GetType() == SEPARATOR)
    return false;

  const MenuController* controller = GetMenuController();
  if (!controller || controller->exit_type() != MenuController::EXIT_NONE) {
    // Either the menu has been closed or we're in the process of closing the
    // menu. Don't attempt to query the delegate as it may no longer be valid.
    return false;
  }

  const MenuItemView* root_menu_item = GetRootMenuItem();
  if (root_menu_item->canceled_) {
    // TODO(sky): if |canceled_| is true, controller->exit_type() should be
    // something other than EXIT_NONE, but crash reports seem to indicate
    // otherwise. Figure out why this is needed.
    return false;
  }

  const MenuDelegate* delegate = GetDelegate();
  CHECK(delegate);
  gfx::Point location(p);
  ConvertPointToScreen(this, &location);
  *tooltip = delegate->GetTooltipText(command_, location);
  return !tooltip->empty();
}

void MenuItemView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  // Set the role based on the type of menu item.
  switch (GetType()) {
    case CHECKBOX:
      node_data->role = ax::mojom::Role::kMenuItemCheckBox;
      break;
    case RADIO:
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
    View* child = child_at(0);
    ui::AXNodeData node_data;
    child->GetAccessibleNodeData(&node_data);
    item_text =
        node_data.GetString16Attribute(ax::mojom::StringAttribute::kName);
  } else {
    item_text = title_;
  }
  node_data->SetName(GetAccessibleNameForMenuItem(item_text, GetMinorText()));

  switch (GetType()) {
    case SUBMENU:
    case ACTIONABLE_SUBMENU:
      node_data->SetHasPopup(ax::mojom::HasPopup::kMenu);
      break;
    case CHECKBOX:
    case RADIO: {
      const bool is_checked = GetDelegate()->IsItemChecked(GetCommand());
      node_data->SetCheckedState(is_checked ? ax::mojom::CheckedState::kTrue
                                            : ax::mojom::CheckedState::kFalse);
    } break;
    case NORMAL:
    case SEPARATOR:
    case EMPTY:
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

// static
bool MenuItemView::IsBubble(MenuAnchorPosition anchor) {
  return anchor == MENU_ANCHOR_BUBBLE_LEFT ||
         anchor == MENU_ANCHOR_BUBBLE_RIGHT ||
         anchor == MENU_ANCHOR_BUBBLE_ABOVE ||
         anchor == MENU_ANCHOR_BUBBLE_BELOW ||
         anchor == MENU_ANCHOR_BUBBLE_TOUCHABLE_ABOVE ||
         anchor == MENU_ANCHOR_BUBBLE_TOUCHABLE_LEFT ||
         anchor == MENU_ANCHOR_BUBBLE_TOUCHABLE_RIGHT;
}

// static
base::string16 MenuItemView::GetAccessibleNameForMenuItem(
      const base::string16& item_text, const base::string16& minor_text) {
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

  return accessible_name;
}

void MenuItemView::Cancel() {
  if (controller_ && !canceled_) {
    canceled_ = true;
    controller_->Cancel(MenuController::EXIT_ALL);
  }
}

MenuItemView* MenuItemView::AddMenuItemAt(
    int index,
    int item_id,
    const base::string16& label,
    const base::string16& sublabel,
    const base::string16& minor_text,
    const gfx::VectorIcon* minor_icon,
    const gfx::ImageSkia& icon,
    Type type,
    ui::MenuSeparatorType separator_style) {
  DCHECK_NE(type, EMPTY);
  DCHECK_LE(0, index);
  if (!submenu_)
    CreateSubmenu();
  DCHECK_GE(submenu_->child_count(), index);
  if (type == SEPARATOR) {
    submenu_->AddChildViewAt(new MenuSeparator(separator_style), index);
    return NULL;
  }
  MenuItemView* item = new MenuItemView(this, item_id, type);
  if (label.empty() && GetDelegate())
    item->SetTitle(GetDelegate()->GetLabel(item_id));
  else
    item->SetTitle(label);
  item->SetSubtitle(sublabel);
  item->SetMinorText(minor_text);
  item->SetMinorIcon(minor_icon);
  if (!icon.isNull())
    item->SetIcon(icon);
  if (type == SUBMENU || type == ACTIONABLE_SUBMENU)
    item->CreateSubmenu();
  if (GetDelegate() && !GetDelegate()->IsCommandVisible(item_id))
    item->SetVisible(false);
  submenu_->AddChildViewAt(item, index);
  return item;
}

void MenuItemView::RemoveMenuItemAt(int index) {
  DCHECK(submenu_);
  DCHECK_LE(0, index);
  DCHECK_GT(submenu_->child_count(), index);

  View* item = submenu_->child_at(index);
  DCHECK(item);
  submenu_->RemoveChildView(item);

  // RemoveChildView() does not delete the item, which is a good thing
  // in case a submenu is being displayed while items are being removed.
  // Deletion will be done by ChildrenChanged() or at destruction.
  removed_items_.push_back(item);
}

MenuItemView* MenuItemView::AppendMenuItem(int item_id,
                                           const base::string16& label,
                                           Type type) {
  return AppendMenuItemImpl(item_id, label, base::string16(), base::string16(),
                            nullptr, gfx::ImageSkia(), type,
                            ui::NORMAL_SEPARATOR);
}

MenuItemView* MenuItemView::AppendSubMenu(int item_id,
                                          const base::string16& label) {
  return AppendMenuItemImpl(item_id, label, base::string16(), base::string16(),
                            nullptr, gfx::ImageSkia(), SUBMENU,
                            ui::NORMAL_SEPARATOR);
}

MenuItemView* MenuItemView::AppendSubMenuWithIcon(int item_id,
                                                  const base::string16& label,
                                                  const gfx::ImageSkia& icon) {
  return AppendMenuItemImpl(item_id, label, base::string16(), base::string16(),
                            nullptr, icon, SUBMENU, ui::NORMAL_SEPARATOR);
}

MenuItemView* MenuItemView::AppendMenuItemWithLabel(
    int item_id,
    const base::string16& label) {
  return AppendMenuItem(item_id, label, NORMAL);
}

MenuItemView* MenuItemView::AppendDelegateMenuItem(int item_id) {
  return AppendMenuItem(item_id, base::string16(), NORMAL);
}

void MenuItemView::AppendSeparator() {
  AppendMenuItemImpl(0, base::string16(), base::string16(), base::string16(),
                     nullptr, gfx::ImageSkia(), SEPARATOR,
                     ui::NORMAL_SEPARATOR);
}

void MenuItemView::AddSeparatorAt(int index) {
  AddMenuItemAt(index, /*item_id=*/0, /*label=*/base::string16(),
                /*sub_label=*/base::string16(),
                /*minor_text=*/base::string16(), /*minor_icon=*/nullptr,
                /*icon=*/gfx::ImageSkia(), /*type=*/SEPARATOR,
                /*separator_style=*/ui::NORMAL_SEPARATOR);
}

MenuItemView* MenuItemView::AppendMenuItemWithIcon(int item_id,
                                                   const base::string16& label,
                                                   const gfx::ImageSkia& icon) {
  return AppendMenuItemImpl(item_id, label, base::string16(), base::string16(),
                            nullptr, icon, NORMAL, ui::NORMAL_SEPARATOR);
}

MenuItemView* MenuItemView::AppendMenuItemImpl(
    int item_id,
    const base::string16& label,
    const base::string16& sublabel,
    const base::string16& minor_text,
    const gfx::VectorIcon* minor_icon,
    const gfx::ImageSkia& icon,
    Type type,
    ui::MenuSeparatorType separator_style) {
  const int index = submenu_ ? submenu_->child_count() : 0;
  return AddMenuItemAt(index, item_id, label, sublabel, minor_text, minor_icon,
                       icon, type, separator_style);
}

SubmenuView* MenuItemView::CreateSubmenu() {
  if (!submenu_) {
    submenu_ = new SubmenuView(this);

    // Initialize the submenu indicator icon (arrow).
    submenu_arrow_image_view_ = new ImageView();
    AddChildView(submenu_arrow_image_view_);
  }

  return submenu_;
}

bool MenuItemView::HasSubmenu() const {
  return (submenu_ != NULL);
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

void MenuItemView::SetSubtitle(const base::string16& subtitle) {
  subtitle_ = subtitle;
  invalidate_dimensions();  // Triggers preferred size recalculation.
}

void MenuItemView::SetMinorText(const base::string16& minor_text) {
  minor_text_ = minor_text;
  invalidate_dimensions();  // Triggers preferred size recalculation.
}

void MenuItemView::SetMinorIcon(const gfx::VectorIcon* minor_icon) {
  minor_icon_ = minor_icon;
  invalidate_dimensions();  // Triggers preferred size recalculation.
}

void MenuItemView::SetSelected(bool selected) {
  selected_ = selected;
  SchedulePaint();
}

void MenuItemView::SetSelectionOfActionableSubmenu(
    bool submenu_area_of_actionable_submenu_selected) {
  DCHECK_EQ(ACTIONABLE_SUBMENU, type_);
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
  if (icon.isNull()) {
    SetIconView(NULL);
    return;
  }

  ImageView* icon_view = new ImageView();
  icon_view->SetImage(&icon);
  SetIconView(icon_view);
}

void MenuItemView::SetIconView(View* icon_view) {
  if (icon_view_) {
    RemoveChildView(icon_view_);
    delete icon_view_;
    icon_view_ = NULL;
  }
  if (icon_view) {
    AddChildView(icon_view);
    icon_view_ = icon_view;
  }
  Layout();
  SchedulePaint();
}

void MenuItemView::OnPaint(gfx::Canvas* canvas) {
  PaintButton(canvas, PB_NORMAL);
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

  int height = child_at(0)->GetHeightForWidth(width);
  if (!icon_view_ && GetRootMenuItem()->has_icons())
    height = std::max(height, MenuConfig::instance().check_height);
  height += GetBottomMargin() + GetTopMargin();

  return height;
}

gfx::Rect MenuItemView::GetSubmenuAreaOfActionableSubmenu() const {
  DCHECK_EQ(ACTIONABLE_SUBMENU, type_);
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
        base::char16 char_array[] = { title_[index + 1], 0 };
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
    return NULL;
  for (int i = 0; i < GetSubmenu()->child_count(); ++i) {
    View* child = GetSubmenu()->child_at(i);
    if (child->id() == MenuItemView::kMenuItemViewID) {
      MenuItemView* result = static_cast<MenuItemView*>(child)->
          GetMenuItemByID(id);
      if (result)
        return result;
    }
  }
  return NULL;
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
      // Force a paint and layout. This handles the case of the top
      // level window's size remaining the same, resulting in no
      // change to the submenu's size and no layout.
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
  if (!has_children())
    return;

  if (IsContainer()) {
    View* child = child_at(0);
    gfx::Size size = child->GetPreferredSize();
    child->SetBounds(0, GetTopMargin(), size.width(), size.height());
  } else {
    // Child views are laid out right aligned and given the full height. To
    // right align start with the last view and progress to the first.
    int x = width() - (use_right_margin_ ? item_right_margin_ : 0);
    for (int i = child_count() - 1; i >= 0; --i) {
      View* child = child_at(i);
      if (icon_view_ == child)
        continue;
      if (radio_check_image_view_ == child)
        continue;
      if (submenu_arrow_image_view_ == child)
        continue;
      if (vertical_separator_ == child)
        continue;
      int width = child->GetPreferredSize().width();
      child->SetBounds(x - width, 0, width, height());
      x -= width + kChildXPadding;
    }
    // Position |icon_view|.
    const MenuConfig& config = MenuConfig::instance();
    if (icon_view_) {
      icon_view_->SizeToPreferredSize();
      gfx::Size size = icon_view_->GetPreferredSize();
      int x = config.item_horizontal_padding + left_icon_margin_ +
              (icon_area_width_ - size.width()) / 2;
      if (config.icons_in_label || type_ == CHECKBOX || type_ == RADIO)
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
              (type_ == ACTIONABLE_SUBMENU
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

MenuItemView::MenuItemView(MenuItemView* parent,
                           int command,
                           MenuItemView::Type type)
    : delegate_(NULL),
      controller_(NULL),
      canceled_(false),
      parent_menu_item_(parent),
      type_(type),
      selected_(false),
      command_(command),
      submenu_(NULL),
      has_mnemonics_(false),
      show_mnemonics_(false),
      has_icons_(false),
      icon_view_(NULL),
      top_margin_(-1),
      bottom_margin_(-1),
      left_icon_margin_(0),
      right_icon_margin_(0),
      requested_menu_position_(POSITION_BEST_FIT),
      actual_menu_position_(requested_menu_position_),
      use_right_margin_(true) {
  Init(parent, command, type, NULL);
}

MenuItemView::~MenuItemView() {
  if (GetMenuController())
    GetMenuController()->OnMenuItemDestroying(this);
  delete submenu_;
  for (auto* item : removed_items_)
    delete item;
}

const char* MenuItemView::GetClassName() const {
  return kViewClassName;
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
    padding = config.item_horizontal_padding;
  } else if (!config.icons_in_label) {
    padding = (has_icons_ || HasChecksOrRadioButtons())
                  ? config.item_horizontal_padding
                  : 0;
  }
  if (use_touchable_layout)
    padding = config.touchable_item_horizontal_padding;

  label_start_ += padding;

  EmptyMenuMenuItem menu_item(this);
  menu_item.set_controller(GetMenuController());
  pref_menu_height_ = menu_item.GetPreferredSize().height();
}

void MenuItemView::Init(MenuItemView* parent,
                        int command,
                        MenuItemView::Type type,
                        MenuDelegate* delegate) {
  delegate_ = delegate;
  controller_ = nullptr;
  canceled_ = false;
  parent_menu_item_ = parent;
  type_ = type;
  selected_ = false;
  command_ = command;
  submenu_ = nullptr;
  radio_check_image_view_ = nullptr;
  submenu_arrow_image_view_ = nullptr;
  vertical_separator_ = nullptr;
  show_mnemonics_ = false;
  // Assign our ID, this allows SubmenuItemView to find MenuItemViews.
  set_id(kMenuItemViewID);
  has_icons_ = false;

  if (type_ == CHECKBOX || type_ == RADIO) {
    radio_check_image_view_ = new ImageView();
    bool show_check_radio_icon =
        type_ == RADIO ||
        (type_ == CHECKBOX && GetDelegate()->IsItemChecked(GetCommand()));
    radio_check_image_view_->SetVisible(show_check_radio_icon);
    radio_check_image_view_->set_can_process_events_within_subtree(false);
    AddChildView(radio_check_image_view_);
  }

  if (type_ == ACTIONABLE_SUBMENU) {
    vertical_separator_ = new Separator();
    vertical_separator_->SetVisible(true);
    vertical_separator_->SetFocusBehavior(FocusBehavior::NEVER);
    const MenuConfig& config = MenuConfig::instance();
    vertical_separator_->SetColor(GetNativeTheme()->GetSystemColor(
        ui::NativeTheme::kColorId_ActionableSubmenuVerticalSeparatorColor));
    vertical_separator_->SetPreferredSize(
        gfx::Size(config.actionable_submenu_vertical_separator_width,
                  config.actionable_submenu_vertical_separator_height));
    vertical_separator_->set_can_process_events_within_subtree(false);
    AddChildView(vertical_separator_);
  }

  if (submenu_arrow_image_view_)
    submenu_arrow_image_view_->SetVisible(HasSubmenu());

  // Don't request enabled status from the root menu item as it is just
  // a container for real items.  EMPTY items will be disabled.
  MenuDelegate* root_delegate = GetDelegate();
  if (parent && type != EMPTY && root_delegate)
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
    submenu_->AddChildViewAt(new EmptyMenuMenuItem(this), 0);
  } else {
    for (int i = 0, item_count = submenu_->GetMenuItemCount(); i < item_count;
         ++i) {
      MenuItemView* child = submenu_->GetMenuItemAt(i);
      if (child->HasSubmenu())
        child->AddEmptyMenus();
    }
  }
}

void MenuItemView::RemoveEmptyMenus() {
  DCHECK(HasSubmenu());
  // Iterate backwards as we may end up removing views, which alters the child
  // view count.
  for (int i = submenu_->child_count() - 1; i >= 0; --i) {
    View* child = submenu_->child_at(i);
    if (child->id() == MenuItemView::kMenuItemViewID) {
      MenuItemView* menu_item = static_cast<MenuItemView*>(child);
      if (menu_item->HasSubmenu())
        menu_item->RemoveEmptyMenus();
    } else if (child->id() == EmptyMenuMenuItem::kEmptyMenuItemViewID) {
      submenu_->RemoveChildView(child);
      delete child;
      child = NULL;
    }
  }
}

void MenuItemView::AdjustBoundsForRTLUI(gfx::Rect* rect) const {
  rect->set_x(GetMirroredXForRect(*rect));
}

void MenuItemView::PaintButton(gfx::Canvas* canvas, PaintButtonMode mode) {
  const MenuConfig& config = MenuConfig::instance();
  bool render_selection =
      (mode == PB_NORMAL && IsSelected() &&
       parent_menu_item_->GetSubmenu()->GetShowSelection(this) &&
       (NonIconChildViewsCount() == 0));
  if (forced_visual_selection_.has_value())
    render_selection = *forced_visual_selection_;

  MenuDelegate *delegate = GetDelegate();
  // Render the background. As MenuScrollViewContainer draws the background, we
  // only need the background when we want it to look different, as when we're
  // selected.
  ui::NativeTheme* native_theme = GetNativeTheme();
  if (render_selection) {
    gfx::Rect item_bounds(0, 0, width(), height());
    if (type_ == ACTIONABLE_SUBMENU) {
      if (submenu_area_of_actionable_submenu_selected_) {
        item_bounds = GetSubmenuAreaOfActionableSubmenu();
      } else {
        item_bounds = gfx::Rect(gfx::Size(
            width() - MenuConfig::instance().actionable_submenu_width - 1,
            height()));
      }
    }
    AdjustBoundsForRTLUI(&item_bounds);

    native_theme->Paint(canvas->sk_canvas(),
                        ui::NativeTheme::kMenuItemBackground,
                        ui::NativeTheme::kHovered,
                        item_bounds,
                        ui::NativeTheme::ExtraParams());
  }

  const int top_margin = GetTopMargin();
  const int bottom_margin = GetBottomMargin();
  const int available_height = height() - top_margin - bottom_margin;

  // Calculate some colors.
  MenuDelegate::LabelStyle style;
  style.foreground = GetTextColor(false, render_selection);
  GetLabelStyle(&style);

  SkColor icon_color = color_utils::DeriveDefaultIconColor(style.foreground);
  if (GetMenuController() && GetMenuController()->use_touchable_layout())
    icon_color = config.touchable_icon_color;

  // Render the check.
  if (type_ == CHECKBOX && delegate->IsItemChecked(GetCommand())) {
    radio_check_image_view_->SetImage(GetMenuCheckImage(icon_color));
  } else if (type_ == RADIO) {
    radio_check_image_view_->SetImage(GetRadioButtonImage(
        delegate->IsItemChecked(GetCommand()), render_selection, icon_color));
  }

  // Render the foreground.
  int accel_width = parent_menu_item_->GetSubmenu()->max_minor_text_width();
  int label_start = GetLabelStartForThisItem();

  int width = this->width() - label_start - accel_width -
      (!delegate ||
       delegate->ShouldReserveSpaceForSubmenuIndicator() ?
           item_right_margin_ : config.arrow_to_edge_padding);
  gfx::Rect text_bounds(label_start, top_margin, width,
                        subtitle_.empty() ? available_height
                                          : available_height / 2);
  text_bounds.set_x(GetMirroredXForRect(text_bounds));
  int flags = GetDrawStringFlags();
  if (mode == PB_FOR_DRAG)
    flags |= gfx::Canvas::NO_SUBPIXEL_RENDERING;
  canvas->DrawStringRectWithFlags(title(), style.font_list, style.foreground,
                                  text_bounds, flags);
  if (!subtitle_.empty()) {
    canvas->DrawStringRectWithFlags(
        subtitle_, style.font_list,
        GetNativeTheme()->GetSystemColor(
            ui::NativeTheme::kColorId_MenuItemMinorTextColor),
        text_bounds + gfx::Vector2d(0, style.font_list.GetHeight()), flags);
  }

  PaintMinorIconAndText(canvas, style);

  // Set the submenu indicator (arrow) image and color.
  if (HasSubmenu())
    submenu_arrow_image_view_->SetImage(GetSubmenuArrowImage(icon_color));
}

void MenuItemView::PaintMinorIconAndText(
    gfx::Canvas* canvas,
    const MenuDelegate::LabelStyle& style) {
  base::string16 minor_text = GetMinorText();
  const gfx::VectorIcon* minor_icon = GetMinorIcon();
  if (minor_text.empty() && !minor_icon)
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

  auto render_text = gfx::RenderText::CreateHarfBuzzInstance();
  if (!minor_text.empty()) {
    render_text->SetText(minor_text);
    render_text->SetFontList(style.font_list);
    render_text->SetColor(style.foreground);
    render_text->SetDisplayRect(minor_text_bounds);
    render_text->SetHorizontalAlignment(base::i18n::IsRTL() ? gfx::ALIGN_LEFT
                                                            : gfx::ALIGN_RIGHT);
    render_text->Draw(canvas);
  }

  if (minor_icon) {
    gfx::ImageSkia image = CreateVectorIcon(*minor_icon, style.foreground);

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
  ui::NativeTheme::ColorId color_id =
      minor ? ui::NativeTheme::kColorId_MenuItemMinorTextColor
            : ui::NativeTheme::kColorId_EnabledMenuItemForegroundColor;
  if (enabled()) {
    if (render_selection)
      color_id = ui::NativeTheme::kColorId_SelectedMenuItemForegroundColor;
  } else {
    color_id = ui::NativeTheme::kColorId_DisabledMenuItemForegroundColor;
  }

  if (GetMenuController() && GetMenuController()->use_touchable_layout())
    color_id = ui::NativeTheme::kColorId_TouchableMenuItemLabelColor;

  return GetNativeTheme()->GetSystemColor(color_id);
}

void MenuItemView::DestroyAllMenuHosts() {
  if (!HasSubmenu())
    return;

  submenu_->Close();
  for (int i = 0, item_count = submenu_->GetMenuItemCount(); i < item_count;
       ++i) {
    submenu_->GetMenuItemAt(i)->DestroyAllMenuHosts();
  }
}

int MenuItemView::GetTopMargin() const {
  if (top_margin_ >= 0)
    return top_margin_;

  const MenuItemView* root = GetRootMenuItem();
  return root && root->has_icons_
             ? MenuConfig::instance().item_top_margin
             : MenuConfig::instance().item_no_icon_top_margin;
}

int MenuItemView::GetBottomMargin() const {
  if (bottom_margin_ >= 0)
    return bottom_margin_;

  const MenuItemView* root = GetRootMenuItem();
  return root && root->has_icons_
             ? MenuConfig::instance().item_bottom_margin
             : MenuConfig::instance().item_no_icon_bottom_margin;
}

gfx::Size MenuItemView::GetChildPreferredSize() const {
  if (!has_children())
    return gfx::Size();

  if (IsContainer())
    return child_at(0)->GetPreferredSize();

  int width = 0;
  for (int i = 0; i < child_count(); ++i) {
    const View* child = child_at(i);
    if (icon_view_ == child)
      continue;
    if (radio_check_image_view_ == child)
      continue;
    if (submenu_arrow_image_view_ == child)
      continue;
    if (vertical_separator_ == child)
      continue;
    if (i)
      width += kChildXPadding;
    width += child->GetPreferredSize().width();
  }
  int height = 0;
  if (icon_view_)
    height = icon_view_->GetPreferredSize().height();

  // If there is no icon view it returns a height of 0 to indicate that
  // we should use the title height instead.
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
      dimensions.height = icon_view_->height() +
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
  dimensions.height += GetBottomMargin() + GetTopMargin();

  // In case of a container, only the container size needs to be filled.
  if (IsContainer()) {
    ApplyMinimumDimensions(&dimensions);
    return dimensions;
  }

  // Get Icon margin overrides for this particular item.
  const MenuDelegate* delegate = GetDelegate();
  if (delegate) {
    delegate->GetHorizontalIconMargins(command_,
                                       icon_area_width_,
                                       &left_icon_margin_,
                                       &right_icon_margin_);
  } else {
    left_icon_margin_ = 0;
    right_icon_margin_ = 0;
  }
  int label_start = GetLabelStartForThisItem();

  // Determine the length of the label text.
  int string_width = gfx::GetStringWidth(title_, style.font_list);
  if (!subtitle_.empty()) {
    string_width =
        std::max(string_width, gfx::GetStringWidth(subtitle_, style.font_list));
  }

  dimensions.standard_width = string_width + label_start +
      item_right_margin_;
  // Determine the length of the right-side text.
  dimensions.minor_text_width =
      minor_text.empty() ? 0 : gfx::GetStringWidth(minor_text, style.font_list);

  // Determine the height to use.
  dimensions.height = std::max(
      dimensions.height, (subtitle_.empty() ? 0 : style.font_list.GetHeight()) +
                             style.font_list.GetHeight() + GetBottomMargin() +
                             GetTopMargin());
  dimensions.height =
      std::max(dimensions.height, MenuConfig::instance().item_min_height);

  ApplyMinimumDimensions(&dimensions);
  return dimensions;
}

void MenuItemView::ApplyMinimumDimensions(MenuItemDimensions* dims) const {
  // Don't apply minimums to menus without controllers or to comboboxes.
  if (!GetMenuController() || GetMenuController()->is_combobox())
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
  if ((config.icons_in_label || type_ == CHECKBOX || type_ == RADIO) &&
      icon_view_) {
    label_start += icon_view_->size().width() + config.item_horizontal_padding;
  }

  return label_start;
}

base::string16 MenuItemView::GetMinorText() const {
  if (id() == kEmptyMenuItemViewID) {
    // Don't query the delegate for menus that represent no children.
    return base::string16();
  }

  base::string16 accel_text;
  if (MenuConfig::instance().ShouldShowAcceleratorText(this, &accel_text))
    return accel_text;

  return minor_text_;
}

const gfx::VectorIcon* MenuItemView::GetMinorIcon() const {
  return minor_icon_;
}

bool MenuItemView::IsContainer() const {
  // Let the first child take over |this| when we only have one child and no
  // title.
  return (NonIconChildViewsCount() == 1) && title_.empty();
}

int MenuItemView::NonIconChildViewsCount() const {
  // Note that what child_count() returns is the number of children,
  // not the number of menu items.
  return child_count() - (icon_view_ ? 1 : 0) -
         (radio_check_image_view_ ? 1 : 0) -
         (submenu_arrow_image_view_ ? 1 : 0) - (vertical_separator_ ? 1 : 0);
}

int MenuItemView::GetMaxIconViewWidth() const {
  int width = 0;
  for (int i = 0; i < submenu_->GetMenuItemCount(); ++i) {
    MenuItemView* menu_item = submenu_->GetMenuItemAt(i);
    int temp_width = 0;
    if (menu_item->GetType() == CHECKBOX ||
        menu_item->GetType() == RADIO) {
      // If this item has a radio or checkbox, the icon will not affect
      // alignment of other items.
      continue;
    } else if (menu_item->HasSubmenu()) {
      temp_width = menu_item->GetMaxIconViewWidth();
    } else if (menu_item->icon_view() &&
               !MenuConfig::instance().icons_in_label) {
      temp_width = menu_item->icon_view()->GetPreferredSize().width();
    }
    width = std::max(width, temp_width);
  }
  return width;
}

bool MenuItemView::HasChecksOrRadioButtons() const {
  for (int i = 0; i < submenu_->GetMenuItemCount(); ++i) {
    MenuItemView* menu_item = submenu_->GetMenuItemAt(i);
    if (menu_item->HasSubmenu()) {
      if (menu_item->HasChecksOrRadioButtons())
        return true;
    } else {
      const Type& type = menu_item->GetType();
      if (type == CHECKBOX || type == RADIO)
        return true;
    }
  }
  return false;
}

}  // namespace views
