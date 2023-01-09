// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_item_view.h"

#include <math.h>
#include <stddef.h>

#include <algorithm>
#include <memory>
#include <numeric>
#include <utility>

#include "base/auto_reset.h"
#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/i18n/case_conversion.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/menu_model.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
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
#include "ui/views/controls/menu/new_badge.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/style/typography.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_MAC)
#include "ui/views/accessibility/view_accessibility.h"
#endif  //  BUILDFLAG(IS_MAC)

namespace views {

namespace {

// EmptyMenuMenuItem ----------------------------------------------------------

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

  EmptyMenuMenuItem(const EmptyMenuMenuItem&) = delete;
  EmptyMenuMenuItem& operator=(const EmptyMenuMenuItem&) = delete;

  std::u16string GetTooltipText(const gfx::Point& p) const override {
    // Empty menu items shouldn't have a tooltip.
    return std::u16string();
  }
};

// VerticalSeparator ----------------------------------------------------------

class VerticalSeparator : public Separator {
 public:
  METADATA_HEADER(VerticalSeparator);
  VerticalSeparator();
  VerticalSeparator(const VerticalSeparator&) = delete;
  VerticalSeparator& operator=(const VerticalSeparator&) = delete;
  ~VerticalSeparator() override = default;
};

VerticalSeparator::VerticalSeparator() {
  SetFocusBehavior(FocusBehavior::NEVER);
  const MenuConfig& config = MenuConfig::instance();
  SetPreferredSize(
      gfx::Size(config.actionable_submenu_vertical_separator_width,
                config.actionable_submenu_vertical_separator_height));
  SetCanProcessEventsWithinSubtree(false);
  ui::ColorId id = ui::kColorMenuSeparator;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  id = ui::kColorAshSystemUIMenuSeparator;
#endif
  SetColorId(id);
}

BEGIN_METADATA(VerticalSeparator, Separator)
END_METADATA

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

MenuItemView::MenuItemView(MenuDelegate* delegate)
    : MenuItemView(/* parent */ nullptr,
                   /* command */ 0,
                   Type::kSubMenu,
                   delegate) {}

void MenuItemView::ChildPreferredSizeChanged(View* child) {
  invalidate_dimensions();
  PreferredSizeChanged();
}

void MenuItemView::OnThemeChanged() {
  View::OnThemeChanged();
  // Force updating as the colors may have changed.
  if (!IsScheduledForDeletion())
    UpdateSelectionBasedState(ShouldPaintAsSelected(PaintMode::kNormal));
}

void MenuItemView::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  // Whether the selection is painted may change based on the number of
  // children.
  if (details.parent == this &&
      update_selection_based_state_in_view_herarchy_changed_) {
    UpdateSelectionBasedStateIfChanged(PaintMode::kNormal);
  }
}

std::u16string MenuItemView::GetTooltipText(const gfx::Point& p) const {
  if (!tooltip_.empty())
    return tooltip_;

  if (type_ == Type::kSeparator)
    return std::u16string();

  const MenuController* controller = GetMenuController();
  if (!controller ||
      controller->exit_type() != MenuController::ExitType::kNone) {
    // Either the menu has been closed or we're in the process of closing the
    // menu. Don't attempt to query the delegate as it may no longer be valid.
    return std::u16string();
  }

  const MenuItemView* root_menu_item = GetRootMenuItem();
  if (root_menu_item->canceled_) {
    // TODO(sky): if |canceled_| is true, controller->exit_type() should be
    // something other than ExitType::kNone, but crash reports seem to indicate
    // otherwise. Figure out why this is needed.
    return std::u16string();
  }

  const MenuDelegate* delegate = GetDelegate();
  if (!delegate)
    return std::u16string();

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

  node_data->SetName(CalculateAccessibleName());

  switch (type_) {
    case Type::kSubMenu:
    case Type::kActionableSubMenu:
      // Note: This is neither necessary nor sufficient for macOS. See
      // CreateSubmenu() for virtual child creation and explanation.
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

  char16_t mnemonic = GetMnemonic();
  if (mnemonic != '\0') {
    node_data->AddStringAttribute(
        ax::mojom::StringAttribute::kKeyShortcuts,
        base::UTF16ToUTF8(std::u16string(1, mnemonic)));
  }

  if (IsTraversableByKeyboard()) {
    node_data->AddBoolAttribute(ax::mojom::BoolAttribute::kSelected,
                                IsSelected());
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

View::FocusBehavior MenuItemView::GetFocusBehavior() const {
  // If the creator/owner of the MenuItemView has explicitly set the focus
  // behavior to something other than the default NEVER, don't override it.
  View::FocusBehavior focus_behavior = View::GetFocusBehavior();
  if (focus_behavior != FocusBehavior::NEVER)
    return focus_behavior;

  // Some MenuItemView types are presumably never focusable, even by assistive
  // technologies.
  if (type_ == Type::kEmpty || type_ == Type::kSeparator)
    return FocusBehavior::NEVER;

  // The rest of the MenuItemView types are presumably focusable, at least by
  // assistive technologies. But if they lack presentable information, then
  // there won't be anything for ATs to convey to the user. Filter those out.
  if (title_.empty() && secondary_title_.empty() && minor_text_.empty()) {
    return FocusBehavior::NEVER;
  }

  return FocusBehavior::ACCESSIBLE_ONLY;
}

// static
bool MenuItemView::IsBubble(MenuAnchorPosition anchor) {
  switch (anchor) {
    case MenuAnchorPosition::kTopLeft:
    case MenuAnchorPosition::kTopRight:
    case MenuAnchorPosition::kBottomCenter:
      return false;
    case MenuAnchorPosition::kBubbleTopLeft:
    case MenuAnchorPosition::kBubbleTopRight:
    case MenuAnchorPosition::kBubbleLeft:
    case MenuAnchorPosition::kBubbleRight:
    case MenuAnchorPosition::kBubbleBottomLeft:
    case MenuAnchorPosition::kBubbleBottomRight:
      return true;
  }
}

// static
std::u16string MenuItemView::GetAccessibleNameForMenuItem(
    const std::u16string& item_text,
    const std::u16string& minor_text,
    bool is_new_feature) {
  std::u16string accessible_name = item_text;

  // Filter out the "&" for accessibility clients.
  size_t index = 0;
  const char16_t amp = '&';
  while ((index = accessible_name.find(amp, index)) != std::u16string::npos &&
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
    accessible_name.append(NewBadge::GetNewBadgeAccessibleDescription());
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
    size_t index,
    int item_id,
    const std::u16string& label,
    const std::u16string& secondary_label,
    const std::u16string& minor_text,
    const ui::ImageModel& minor_icon,
    const ui::ImageModel& icon,
    Type type,
    ui::MenuSeparatorType separator_style) {
  DCHECK_NE(type, Type::kEmpty);
  if (!submenu_)
    CreateSubmenu();
  DCHECK_LE(index, submenu_->children().size());
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

  submenu_->RemoveAllChildViewsWithoutDeleting();
}

MenuItemView* MenuItemView::AppendMenuItem(int item_id,
                                           const std::u16string& label,
                                           const ui::ImageModel& icon) {
  return AppendMenuItemImpl(item_id, label, icon, Type::kNormal);
}

MenuItemView* MenuItemView::AppendSubMenu(int item_id,
                                          const std::u16string& label,
                                          const ui::ImageModel& icon) {
  return AppendMenuItemImpl(item_id, label, icon, Type::kSubMenu);
}

void MenuItemView::AppendSeparator() {
  AppendMenuItemImpl(0, std::u16string(), ui::ImageModel(), Type::kSeparator);
}

void MenuItemView::AddSeparatorAt(size_t index) {
  AddMenuItemAt(index, /*item_id=*/0, /*label=*/std::u16string(),
                /*secondary_label=*/std::u16string(),
                /*minor_text=*/std::u16string(),
                /*minor_icon=*/ui::ImageModel(),
                /*icon=*/ui::ImageModel(),
                /*type=*/Type::kSeparator,
                /*separator_style=*/ui::NORMAL_SEPARATOR);
}

MenuItemView* MenuItemView::AppendMenuItemImpl(int item_id,
                                               const std::u16string& label,
                                               const ui::ImageModel& icon,
                                               Type type) {
  const size_t index = submenu_ ? submenu_->children().size() : size_t{0};
  return AddMenuItemAt(index, item_id, label, std::u16string(),
                       std::u16string(), ui::ImageModel(), icon, type,
                       ui::NORMAL_SEPARATOR);
}

SubmenuView* MenuItemView::CreateSubmenu() {
  if (submenu_)
    return submenu_;

  submenu_ = new SubmenuView(this);

#if BUILDFLAG(IS_MAC)
  // All MenuItemViews of Type kSubMenu have a respective SubmenuView.
  // However, in the Views hierarchy, this SubmenuView is not a child of the
  // MenuItemView. This confuses VoiceOver, because it expects the submenu
  // itself to be a child of the menu item. To allow VoiceOver to recognize
  // submenu items, we create a virtual child of type Menu.
  std::unique_ptr<AXVirtualView> virtual_child =
      std::make_unique<AXVirtualView>();
  virtual_child->GetCustomData().role = ax::mojom::Role::kMenu;
  GetViewAccessibility().AddVirtualChildView(std::move(virtual_child));
#endif  //  BUILDFLAG(IS_MAC)

  // Initialize the submenu indicator icon (arrow).
  submenu_arrow_image_view_ = AddChildView(std::make_unique<ImageView>());

  // Force an update as `submenu_arrow_image_view_` needs to be updated. The
  // state is also updated when the theme changes (which is also called when
  // added to a widget).
  if (GetWidget())
    UpdateSelectionBasedState(ShouldPaintAsSelected(PaintMode::kNormal));

  SchedulePaint();

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

void MenuItemView::SetTitle(const std::u16string& title) {
  title_ = title;
  invalidate_dimensions();  // Triggers preferred size recalculation.
}

void MenuItemView::SetSecondaryTitle(const std::u16string& secondary_title) {
  secondary_title_ = secondary_title;
  invalidate_dimensions();  // Triggers preferred size recalculation.
}

void MenuItemView::SetMinorText(const std::u16string& minor_text) {
  minor_text_ = minor_text;
  invalidate_dimensions();  // Triggers preferred size recalculation.
}

void MenuItemView::SetMinorIcon(const ui::ImageModel& minor_icon) {
  minor_icon_ = minor_icon;
  invalidate_dimensions();  // Triggers preferred size recalculation.
}

void MenuItemView::SetSelected(bool selected) {
  if (selected_ == selected)
    return;

  selected_ = selected;
  UpdateSelectionBasedStateIfChanged(PaintMode::kNormal);
  OnPropertyChanged(&selected_, kPropertyEffectsPaint);
}

base::CallbackListSubscription MenuItemView::AddSelectedChangedCallback(
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

void MenuItemView::SetTooltip(const std::u16string& tooltip, int item_id) {
  MenuItemView* item = GetMenuItemByID(item_id);
  DCHECK(item);
  item->tooltip_ = tooltip;
}

void MenuItemView::SetIcon(const ui::ImageModel& icon) {
  if (icon.IsEmpty()) {
    SetIconView(nullptr);
    return;
  }

  auto icon_view = std::make_unique<ImageView>();
  icon_view->SetImage(icon);
  SetIconView(std::move(icon_view));
}

void MenuItemView::SetIconView(std::unique_ptr<ImageView> icon_view) {
  {
    // See comment in `update_selection_based_state_in_view_herarchy_changed_`
    // as to why setting the field and explicitly calling
    // UpdateSelectionBasedStateIfChanged() is necessary.
    base::AutoReset setter(
        &update_selection_based_state_in_view_herarchy_changed_, false);
    if (icon_view_) {
      RemoveChildViewT(icon_view_.get());
      icon_view_ = nullptr;
    }

    if (icon_view)
      icon_view_ = AddChildView(std::move(icon_view));
  }

  UpdateSelectionBasedStateIfChanged(PaintMode::kNormal);

  InvalidateLayout();
  SchedulePaint();
}

void MenuItemView::OnDropOrSelectionStatusMayHaveChanged() {
  UpdateSelectionBasedStateIfChanged(PaintMode::kNormal);
}

void MenuItemView::OnPaint(gfx::Canvas* canvas) {
  OnPaintImpl(canvas, PaintMode::kNormal);
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

char16_t MenuItemView::GetMnemonic() {
  if (!GetRootMenuItem()->has_mnemonics_ ||
      !MenuConfig::instance().use_mnemonics || !may_have_mnemonics()) {
    return 0;
  }

  size_t index = 0;
  do {
    index = title_.find('&', index);
    if (index != std::u16string::npos) {
      if (index + 1 != title_.size() && title_[index + 1] != '&') {
        char16_t char_array[] = {title_[index + 1], 0};
        // TODO(jshin): What about Turkish locale? See http://crbug.com/81719.
        // If the mnemonic is capital I and the UI language is Turkish,
        // lowercasing it results in 'small dotless i', which is different
        // from a 'dotted i'. Similar issues may exist for az and lt locales.
        return base::i18n::ToLower(char_array)[0];
      }
      index++;
    }
  } while (index != std::u16string::npos);
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
      int x = config.item_horizontal_padding +
              (icon_area_width_ - size.width()) / 2;
      if (config.icons_in_label || type_ == Type::kCheckbox ||
          type_ == Type::kRadio)
        x = label_start_;
      if (GetMenuController() &&
          GetMenuController()->use_ash_system_ui_layout())
        x = config.touchable_item_horizontal_padding;

      int y =
          (height() + GetTopMargin() - GetBottomMargin() - size.height()) / 2;
      icon_view_->SetPosition(gfx::Point(x, y));
    }

    if (radio_check_image_view_) {
      int x = config.item_horizontal_padding;
      if (GetMenuController() &&
          GetMenuController()->use_ash_system_ui_layout())
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
  if (selected == forced_visual_selection_)
    return;

  forced_visual_selection_ = selected;
  UpdateSelectionBasedStateIfChanged(PaintMode::kNormal);
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
  return is_new_;
}

bool MenuItemView::IsTraversableByKeyboard() const {
  bool ignore_enabled = ui::AXPlatformNode::GetAccessibilityMode().has_mode(
      ui::AXMode::kNativeAPIs);
  return GetVisible() && (ignore_enabled || GetEnabled());
}

MenuItemView::MenuItemView(MenuItemView* parent,
                           int command,
                           MenuItemView::Type type)
    : MenuItemView(parent, command, type, /* delegate */ nullptr) {}

MenuItemView::~MenuItemView() {
  if (GetMenuController())
    GetMenuController()->OnMenuItemDestroying(this);
  delete submenu_;
  for (auto* item : removed_items_)
    delete item;
}

void MenuItemView::UpdateMenuPartSizes() {
  const MenuConfig& config = MenuConfig::instance();

  item_right_margin_ = config.label_to_arrow_padding + config.arrow_width +
                       config.arrow_to_edge_padding;
  icon_area_width_ = config.check_width;
  if (has_icons_)
    icon_area_width_ = std::max(icon_area_width_, GetMaxIconViewWidth());

  const bool use_ash_system_ui_layout =
      GetMenuController() && GetMenuController()->use_ash_system_ui_layout();
  label_start_ =
      (use_ash_system_ui_layout ? config.touchable_item_horizontal_padding
                                : config.item_horizontal_padding) +
      icon_area_width_;

  const bool use_padding = config.always_use_icon_to_label_padding ||
                           (!config.icons_in_label && has_icons_) ||
                           HasChecksOrRadioButtons() ||
                           use_ash_system_ui_layout;
  int padding = use_padding ? LayoutProvider::Get()->GetDistanceMetric(
                                  DISTANCE_RELATED_LABEL_HORIZONTAL)
                            : 0;

  label_start_ += padding;

  EmptyMenuMenuItem menu_item(this);
  menu_item.set_controller(GetMenuController());
  pref_menu_height_ = menu_item.GetPreferredSize().height();
}

MenuItemView::MenuItemView(MenuItemView* parent,
                           int command,
                           Type type,
                           MenuDelegate* delegate)
    : delegate_(delegate),
      parent_menu_item_(parent),
      type_(type),
      command_(command) {
  // Assign our ID, this allows SubmenuItemView to find MenuItemViews.
  SetID(kMenuItemViewID);

  if (type_ == Type::kCheckbox || type_ == Type::kRadio) {
    radio_check_image_view_ = AddChildView(std::make_unique<ImageView>());
    bool show_check_radio_icon =
        type_ == Type::kRadio || (type_ == Type::kCheckbox && GetDelegate() &&
                                  GetDelegate()->IsItemChecked(GetCommand()));
    radio_check_image_view_->SetVisible(show_check_radio_icon);
    radio_check_image_view_->SetCanProcessEventsWithinSubtree(false);
  }

  if (type_ == Type::kActionableSubMenu)
    vertical_separator_ = AddChildView(std::make_unique<VerticalSeparator>());

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

  if (GetRootMenuItem()->has_mnemonics_ && may_have_mnemonics()) {
    if (MenuConfig::instance().show_mnemonics ||
        GetRootMenuItem()->show_mnemonics_) {
      flags |= gfx::Canvas::SHOW_PREFIX;
    } else {
      flags |= gfx::Canvas::HIDE_PREFIX;
    }
  }
  return flags;
}

const gfx::FontList MenuItemView::GetFontList() const {
  if (const MenuDelegate* delegate = GetDelegate()) {
    if (const gfx::FontList* font_list =
            delegate->GetLabelFontList(GetCommand())) {
      return *font_list;
    }
  }
  if (GetMenuController() && GetMenuController()->use_ash_system_ui_layout())
    return style::GetFont(style::CONTEXT_TOUCH_MENU, style::STYLE_PRIMARY);
  return MenuConfig::instance().font_list;
}

const absl::optional<SkColor> MenuItemView::GetMenuLabelColor() const {
  if (const MenuDelegate* delegate = GetDelegate()) {
    if (const auto& label_color = delegate->GetLabelColor(GetCommand()))
      return label_color;
  }
  return absl::nullopt;
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

void MenuItemView::PaintForDrag(gfx::Canvas* canvas) {
  // Selection state may change when painting a drag operation. Set state for
  // a drag operation, paint, and then set state back to non-drag (normal)
  // state.
  UpdateSelectionBasedStateIfChanged(PaintMode::kForDrag);
  OnPaintImpl(canvas, PaintMode::kForDrag);
  UpdateSelectionBasedStateIfChanged(PaintMode::kNormal);
}

void MenuItemView::OnPaintImpl(gfx::Canvas* canvas, PaintMode mode) {
  const bool paint_as_selected = ShouldPaintAsSelected(mode);
  // If these are out of sync, UpdateSelectionBasedStateIfChanged() was not
  // called.
  DCHECK_EQ(paint_as_selected, last_paint_as_selected_);

  // Render the background. As MenuScrollViewContainer draws the background, we
  // only need the background when we want it to look different, as when we're
  // selected.
  PaintBackground(canvas, mode, paint_as_selected);

  const Colors colors = CalculateColors(paint_as_selected);

  const gfx::FontList& font_list = GetFontList();

  // Calculate the margins.
  int top_margin = GetTopMargin();
  const int bottom_margin = GetBottomMargin();
  const int available_height = height() - top_margin - bottom_margin;
  const int text_height = font_list.GetHeight();
  const int total_text_height =
      secondary_title().empty() ? text_height : text_height * 2;
  top_margin += (available_height - total_text_height) / 2;

  // Render the foreground.
  int accel_width = parent_menu_item_->GetSubmenu()->max_minor_text_width();
  int label_start = GetLabelStartForThisItem();

  int width = this->width() - label_start - accel_width - item_right_margin_;
  gfx::Rect text_bounds(label_start, top_margin, width, text_height);
  text_bounds.set_x(GetMirroredXForRect(text_bounds));
  int flags = GetDrawStringFlags();
  if (mode == PaintMode::kForDrag)
    flags |= gfx::Canvas::NO_SUBPIXEL_RENDERING;
  canvas->DrawStringRectWithFlags(title(), font_list, colors.fg_color,
                                  text_bounds, flags);

  // The rest should be drawn with the minor foreground color.
  if (!secondary_title().empty()) {
    text_bounds.set_y(text_bounds.y() + text_height);
    canvas->DrawStringRectWithFlags(secondary_title(), font_list,
                                    colors.minor_fg_color, text_bounds, flags);
  }

  PaintMinorIconAndText(canvas, colors.minor_fg_color);

  if (ShouldShowNewBadge()) {
    NewBadge::DrawNewBadge(canvas, this,
                           label_start +
                               gfx::GetStringWidth(title(), font_list) +
                               NewBadge::kNewBadgeHorizontalMargin,
                           top_margin, font_list);
  }
}

void MenuItemView::PaintBackground(gfx::Canvas* canvas,
                                   PaintMode mode,
                                   bool paint_as_selected) {
  if (type_ == Type::kHighlighted || is_alerted_) {
    SkColor color = gfx::kPlaceholderColor;

    ui::ColorProvider* color_provider = GetColorProvider();
    if (type_ == Type::kHighlighted) {
      const ui::ColorId color_id =
          paint_as_selected ? ui::kColorMenuItemBackgroundSelected
                            : ui::kColorMenuItemBackgroundHighlighted;
      color = color_provider->GetColor(color_id);
    } else {
      const auto* animation = GetMenuController()->GetAlertAnimation();
      color = gfx::Tween::ColorValueBetween(
          animation->GetCurrentValue(),
          color_provider->GetColor(ui::kColorMenuItemBackgroundAlertedInitial),
          color_provider->GetColor(ui::kColorMenuItemBackgroundAlertedTarget));
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
  } else if (paint_as_selected) {
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

    GetNativeTheme()->Paint(canvas->sk_canvas(), GetColorProvider(),
                            ui::NativeTheme::kMenuItemBackground,
                            ui::NativeTheme::kHovered, item_bounds,
                            ui::NativeTheme::ExtraParams());
  }
}

void MenuItemView::PaintMinorIconAndText(gfx::Canvas* canvas, SkColor color) {
  std::u16string minor_text = GetMinorText();
  const ui::ImageModel minor_icon = GetMinorIcon();
  if (minor_text.empty() && minor_icon.IsEmpty())
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
    render_text->SetFontList(GetFontList());
    render_text->SetColor(color);
    render_text->SetDisplayRect(minor_text_bounds);
    render_text->SetHorizontalAlignment(base::i18n::IsRTL() ? gfx::ALIGN_LEFT
                                                            : gfx::ALIGN_RIGHT);
    render_text->Draw(canvas);
  }

  if (!minor_icon.IsEmpty()) {
    const gfx::ImageSkia image = minor_icon.Rasterize(GetColorProvider());

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

SkColor MenuItemView::GetTextColor(bool minor, bool paint_as_selected) const {
  style::TextContext context =
      GetMenuController() && GetMenuController()->use_ash_system_ui_layout()
          ? style::CONTEXT_TOUCH_MENU
          : style::CONTEXT_MENU;

  style::TextStyle text_style = style::STYLE_PRIMARY;
  if (type_ == Type::kTitle)
    text_style = style::STYLE_PRIMARY;
  else if (type_ == Type::kHighlighted)
    text_style = style::STYLE_HIGHLIGHTED;
  else if (!GetEnabled())
    text_style = style::STYLE_DISABLED;
  else if (paint_as_selected)
    text_style = style::STYLE_SELECTED;
  else if (minor)
    text_style = style::STYLE_SECONDARY;

  return style::GetColor(*this, context, text_style);
}

MenuItemView::Colors MenuItemView::CalculateColors(
    bool paint_as_selected) const {
  const absl::optional<SkColor> label_color_from_delegate = GetMenuLabelColor();
  Colors colors;
  colors.fg_color = label_color_from_delegate
                        ? *label_color_from_delegate
                        : GetTextColor(/*minor=*/false, paint_as_selected);
  colors.icon_color = color_utils::DeriveDefaultIconColor(colors.fg_color);
  colors.minor_fg_color = GetTextColor(/*minor=*/true, paint_as_selected);
  return colors;
}

std::u16string MenuItemView::CalculateAccessibleName() const {
  std::u16string item_text = View::GetAccessibleName();
  if (!item_text.empty())
    return item_text;

  // Use the default accessible name if none is provided.
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
  return GetAccessibleNameForMenuItem(item_text, GetMinorText(),
                                      ShouldShowNewBadge());
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

  const gfx::FontList& font_list = GetFontList();

  if (GetMenuController() && GetMenuController()->use_ash_system_ui_layout()) {
    dimensions.height = menu_config.touchable_menu_height;

    // For container MenuItemViews, the width components should only include the
    // |children_width|. Setting a |standard_width| would result in additional
    // width being added to the container because the total width used in layout
    // is |children_width| + |standard_width|.
    if (IsContainer())
      return dimensions;

    // Calculate total item width to make sure the current |title_|
    // has enough room within the context menu.
    int label_start = GetLabelStartForThisItem();
    int string_width = gfx::GetStringWidth(title_, font_list);
    int item_width = string_width + label_start + item_right_margin_;

    item_width = std::max(item_width, menu_config.touchable_menu_min_width);
    item_width = std::min(item_width, menu_config.touchable_menu_max_width);
    dimensions.standard_width = item_width;

    if (icon_view_) {
      dimensions.height = icon_view_->GetPreferredSize().height() +
                          2 * menu_config.vertical_touchable_menu_item_padding;
    }
    return dimensions;
  }

  std::u16string minor_text = GetMinorText();

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

  int string_width = gfx::GetStringWidth(title_, font_list);
  int label_start = GetLabelStartForThisItem();
  dimensions.standard_width = string_width + label_start + item_right_margin_;

  // Determine the length of the right-side text.
  dimensions.minor_text_width =
      (minor_text.empty() ? 0 : gfx::GetStringWidth(minor_text, font_list));

  if (ShouldShowNewBadge())
    dimensions.minor_text_width +=
        NewBadge::GetNewBadgeSize(font_list).width() +
        2 * NewBadge::kNewBadgeHorizontalMargin;

  // Determine the height to use.
  int label_text_height = secondary_title().empty() ? font_list.GetHeight()
                                                    : font_list.GetHeight() * 2;
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
  if (GetMenuController() && GetMenuController()->use_ash_system_ui_layout() &&
      icon_view_) {
    return 2 * config.touchable_item_horizontal_padding +
           icon_view_->GetPreferredSize().width();
  }

  int label_start = label_start_;
  if ((config.icons_in_label || type_ == Type::kCheckbox ||
       type_ == Type::kRadio) &&
      icon_view_) {
    label_start += icon_view_->GetPreferredSize().width() +
                   LayoutProvider::Get()->GetDistanceMetric(
                       DISTANCE_RELATED_LABEL_HORIZONTAL);
  }

  return label_start;
}

std::u16string MenuItemView::GetMinorText() const {
  if (GetID() == kEmptyMenuItemViewID) {
    // Don't query the delegate for menus that represent no children.
    return std::u16string();
  }

  std::u16string accel_text;
  if (MenuConfig::instance().ShouldShowAcceleratorText(this, &accel_text))
    return accel_text;

  return minor_text_;
}

ui::ImageModel MenuItemView::GetMinorIcon() const {
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
  // WARNING: if adding a new field that is checked here you may need to
  // set `update_selection_based_state_in_view_herarchy_changed_` to false
  // when setting the field and explicitly call
  // UpdateSelectionBasedStateIfChanged(). See comment in header
  // for details.
  return static_cast<int>(children().size()) - (icon_view_ ? 1 : 0) -
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
  return base::ranges::any_of(submenu_->GetMenuItems(), [](const auto* item) {
    return item->HasChecksOrRadioButtons();
  });
}

void MenuItemView::UpdateSelectionBasedStateIfChanged(PaintMode mode) {
  // Selection state depends upon NativeTheme. Selection based state could also
  // depend on the menu model so avoid the update if the item is scheduled to be
  // deleted.
  if (!GetWidget() || IsScheduledForDeletion())
    return;

  const bool paint_as_selected = ShouldPaintAsSelected(mode);
  if (paint_as_selected != last_paint_as_selected_)
    UpdateSelectionBasedState(paint_as_selected);
}

void MenuItemView::UpdateSelectionBasedState(bool paint_as_selected) {
  // This code makes use of the NativeTheme, which should only be accessed
  // when in a Widget.
  DCHECK(GetWidget());
  last_paint_as_selected_ = paint_as_selected;
  const Colors colors = CalculateColors(paint_as_selected);
  if (submenu_arrow_image_view_) {
    submenu_arrow_image_view_->SetImage(
        GetSubmenuArrowImage(colors.icon_color));
  }
  MenuDelegate* delegate = GetDelegate();
  if (type_ == Type::kCheckbox && delegate &&
      delegate->IsItemChecked(GetCommand())) {
    radio_check_image_view_->SetImage(GetMenuCheckImage(colors.icon_color));
  } else if (type_ == Type::kRadio) {
    const bool toggled = delegate && delegate->IsItemChecked(GetCommand());
    const gfx::VectorIcon& radio_icon =
        toggled ? kMenuRadioSelectedIcon : kMenuRadioEmptyIcon;
    const SkColor radio_icon_color = GetColorProvider()->GetColor(
        toggled ? ui::kColorButtonForegroundChecked
                : ui::kColorButtonForegroundUnchecked);
    radio_check_image_view_->SetImage(ui::ImageModel::FromVectorIcon(
        radio_icon, radio_icon_color, kMenuCheckSize));
  }
}

bool MenuItemView::ShouldPaintAsSelected(PaintMode mode) const {
  if (forced_visual_selection_.has_value())
    return true;

  return (parent_menu_item_ && mode == PaintMode::kNormal && IsSelected() &&
          parent_menu_item_->GetSubmenu()->GetShowSelection(this) &&
          (NonIconChildViewsCount() == 0));
}

bool MenuItemView::IsScheduledForDeletion() const {
  const MenuItemView* parent = GetParentMenuItem();
  return parent && (base::Contains(parent->removed_items_, this) ||
                    parent->IsScheduledForDeletion());
}

BEGIN_METADATA(MenuItemView, View)
END_METADATA

}  // namespace views
