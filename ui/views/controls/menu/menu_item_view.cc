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
#include "components/vector_icons/vector_icons.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/platform/ax_platform.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/ui_base_features.h"
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
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/badge_painter.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_scroll_view_container.h"
#include "ui/views/controls/menu/menu_separator.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace views {

namespace {

// VerticalSeparator ----------------------------------------------------------

class VerticalSeparator : public Separator {
  METADATA_HEADER(VerticalSeparator, Separator)

 public:
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

BEGIN_METADATA(VerticalSeparator)
END_METADATA

}  // namespace

// MenuItemView ---------------------------------------------------------------

MenuItemView::MenuItemView(MenuDelegate* delegate)
    : MenuItemView(/* parent */ nullptr,
                   /* command */ 0,
                   Type::kSubMenu,
                   delegate) {}

MenuItemView::~MenuItemView() {
  if (GetMenuController()) {
    GetMenuController()->OnMenuItemDestroying(this);
  }
  for (views::View* item : removed_items_) {
    delete item;
  }
}

void MenuItemView::ChildPreferredSizeChanged(View* child) {
  invalidate_dimensions();
  PreferredSizeChanged();
}

void MenuItemView::OnThemeChanged() {
  View::OnThemeChanged();
  // Force updating as the colors may have changed.
  if (!IsScheduledForDeletion())
    UpdateSelectionBasedState(ShouldPaintAsSelected(PaintMode::kNormal));

  // Update the name when the theme changes, as the name depends on few
  // attributes like title, minor_text, which are likely to change with the
  // theme.
  UpdateAccessibleName();
}

void MenuItemView::UpdateAccessibleCheckedState() {
  if (type_ == Type::kCheckbox || type_ == Type::kRadio) {
    bool is_checked =
        GetDelegate() && GetDelegate()->IsItemChecked(GetCommand());
    const ax::mojom::CheckedState checked_state =
        is_checked ? ax::mojom::CheckedState::kTrue
                   : ax::mojom::CheckedState::kFalse;
    GetViewAccessibility().SetCheckedState(checked_state);
    if (auto* submenuview_accessibility = GetSubmenuViewAccessibility()) {
      submenuview_accessibility->SetCheckedState(checked_state);
    }
    if (auto* scrollview_accessibility =
            GetScrollViewContainerViewAccessibility()) {
      scrollview_accessibility->SetCheckedState(checked_state);
    }
  } else {
    GetViewAccessibility().RemoveCheckedState();
    if (auto* submenuview_accessibility = GetSubmenuViewAccessibility()) {
      submenuview_accessibility->RemoveCheckedState();
    }
    if (auto* scrollview_accessibility =
            GetScrollViewContainerViewAccessibility()) {
      scrollview_accessibility->RemoveCheckedState();
    }
  }
}

void MenuItemView::SetCommand(int command) {
  command_ = command;
  UpdateAccessibleCheckedState();
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

  if (GetRootMenuItem()->canceled_) {
    // TODO(sky): if |canceled_| is true, controller->exit_type() should be
    // something other than ExitType::kNone, but crash reports seem to indicate
    // otherwise. Figure out why this is needed.
    return std::u16string();
  }

  const MenuDelegate* delegate = GetDelegate();
  return delegate
             ? delegate->GetTooltipText(command_, ConvertPointToScreen(this, p))
             : std::u16string();
}

bool MenuItemView::HandleAccessibleAction(const ui::AXActionData& action_data) {
  // Ensure that if menu-controller is null, default action should be
  // performend.
  if (!GetMenuController()) {
    return View::HandleAccessibleAction(action_data);
  }

  switch (action_data.action) {
    case ax::mojom::Action::kExpand: {
      DCHECK(HasSubmenu());
      [[fallthrough]];
    }
    case ax::mojom::Action::kDoDefault: {
      // kDoDefault in View would simulate a mouse click in the center of this
      // MenuItemView. However, mouse events for menus are dispatched via
      // Widget::SetCapture() to the MenuController rather than to
      // MenuItemView, so there is no effect. VKEY_RETURN provides a better UX
      // anyway, since it will move focus to a submenu.
      ui::KeyEvent event(ui::EventType::kKeyPressed, ui::VKEY_RETURN,
                         ui::DomCode::ENTER, ui::EF_NONE, ui::DomKey::ENTER,
                         ui::EventTimeForNow());
      GetMenuController()->SetSelection(this,
                                        MenuController::SELECTION_DEFAULT);
      GetMenuController()->OnWillDispatchKeyEvent(&event);
      return true;
    }

    case ax::mojom::Action::kCollapse: {
      DCHECK(HasSubmenu());
      GetMenuController()->CloseSubmenu();
      return true;
    }

    default:
      return View::HandleAccessibleAction(action_data);
  }
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
    accessible_name.append(GetNewBadgeAccessibleDescription());
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
    ui::MenuSeparatorType separator_style,
    std::optional<ui::ColorId> submenu_background_color,
    std::optional<ui::ColorId> foreground_color,
    std::optional<ui::ColorId> selected_color_id) {
  DCHECK_NE(type, Type::kEmpty);
  if (!submenu_) {
    CreateSubmenu();
    // Set the submenu border color to be the same as the first submenu
    // background color;
    submenu_->SetBorderColorId(submenu_background_color);
    if (submenu_background_color.has_value()) {
      submenu_->SetBackground(
          views::CreateThemedSolidBackground(submenu_background_color.value()));
    }
  }
  DCHECK_LE(index, submenu_->children().size());
  if (type == Type::kSeparator) {
    submenu_->AddChildViewAt(std::make_unique<MenuSeparator>(separator_style),
                             index);
    return nullptr;
  }
  MenuItemView* item = new MenuItemView(this, item_id, type);
  item->SetTitle(label.empty() && GetDelegate()
                     ? GetDelegate()->GetLabel(item_id)
                     : label);
  item->SetSecondaryTitle(secondary_label);
  item->SetMinorText(minor_text);
  item->SetMinorIcon(minor_icon);
  item->SetIcon(icon);
  item->SetForegroundColorId(foreground_color);
  item->SetSelectedColorId(selected_color_id);
  if (type == Type::kSubMenu || type == Type::kActionableSubMenu)
    item->CreateSubmenu();
  if (type == Type::kHighlighted) {
    item->set_vertical_margin(MenuConfig::instance().footnote_vertical_margin);
  }
  if (GetDelegate() && !GetDelegate()->IsCommandVisible(item_id))
    item->SetVisible(false);
  return submenu_->AddChildViewAt(item, index);
}

void MenuItemView::RemoveMenuItem(View* item) {
  DCHECK(item);
  DCHECK(submenu_);
  DCHECK_EQ(submenu_.get(), item->parent());
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

MenuItemView* MenuItemView::AppendTitle(const std::u16string& label) {
  return AppendMenuItemImpl(ui::MenuModel::kTitleId, label, ui::ImageModel(),
                            Type::kTitle);
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
    return submenu_.get();

  submenu_ = std::make_unique<SubmenuView>(/*parent=*/this);
  submenu_->SetProperty(kElementIdentifierKey, submenu_id_);
  UpdateAccessibleHasPopup();
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

  return submenu_.get();
}

bool MenuItemView::HasSubmenu() const {
  return (submenu_ != nullptr);
}

SubmenuView* MenuItemView::GetSubmenu() const {
  return submenu_.get();
}

bool MenuItemView::SubmenuIsShowing() const {
  return HasSubmenu() && GetSubmenu()->IsShowing();
}

void MenuItemView::SetSubmenuId(ui::ElementIdentifier id) {
  CHECK(type_ == Type::kSubMenu || type_ == Type::kActionableSubMenu)
      << "SetSubmenuId called on menu item with type "
      << static_cast<int>(type_);
  submenu_id_ = id;
  if (submenu_) {
    submenu_->SetProperty(kElementIdentifierKey, id);
  }
}

void MenuItemView::SetTitle(const std::u16string& title) {
  title_ = title;
  invalidate_dimensions();  // Triggers preferred size recalculation.
  UpdateAccessibleKeyShortcuts();
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
  UpdateAccessibleSelection();
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

const ui::ImageModel MenuItemView::GetIcon() const {
  return icon_view_ ? icon_view_->GetImageModel() : ui::ImageModel();
}

void MenuItemView::SetIconView(std::unique_ptr<ImageView> icon_view) {
  {
    // See comment in `update_selection_based_state_in_view_herarchy_changed_`
    // as to why setting the field and explicitly calling
    // UpdateSelectionBasedStateIfChanged() is necessary.
    base::AutoReset setter(
        &update_selection_based_state_in_view_herarchy_changed_, false);
    if (icon_view_) {
      RemoveChildViewT(icon_view_.ExtractAsDangling());
    }

    if (icon_view)
      icon_view_ = AddChildView(std::move(icon_view));
  }

  UpdateSelectionBasedStateIfChanged(PaintMode::kNormal);

  InvalidateLayout();
  SchedulePaint();
}

gfx::Size MenuItemView::GetIconPreferredSize() const {
  return icon_view_ ? icon_view_->GetPreferredSize({}) : gfx::Size();
}

void MenuItemView::OnDropOrSelectionStatusMayHaveChanged() {
  UpdateSelectionBasedStateIfChanged(PaintMode::kNormal);
}

void MenuItemView::OnPaint(gfx::Canvas* canvas) {
  OnPaintImpl(canvas, PaintMode::kNormal);
}

gfx::Size MenuItemView::CalculatePreferredSize(
    const SizeBounds& available_size) const {
  // If this is a container, we can just use the preferred size.
  if (IsContainer()) {
    const gfx::Insets margins = GetContainerMargins();
    gfx::Size content_size =
        children().front()->GetPreferredSize(available_size.Inset(margins));
    content_size.set_height(ApplyMinIconHeight(content_size.height()));
    content_size.Enlarge(margins.width(), margins.height());
    return content_size;
  }

  const MenuItemDimensions& dimensions(GetDimensions());
  return gfx::Size(dimensions.standard_width + dimensions.children_width,
                   dimensions.height);
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

int MenuItemView::GetContentStart() const {
  const MenuConfig& config = MenuConfig::instance();
  const auto* const controller = GetMenuController();
  return GetItemHorizontalBorder() +
         ((controller && controller->use_ash_system_ui_layout())
              ? config.touchable_item_horizontal_padding
              : config.item_horizontal_padding);
}

MenuController* MenuItemView::GetMenuController() {
  return GetRootMenuItem()->controller_.get();
}

const MenuController* MenuItemView::GetMenuController() const {
  return GetRootMenuItem()->controller_.get();
}

MenuDelegate* MenuItemView::GetDelegate() {
  return const_cast<MenuDelegate*>(std::as_const(*this).GetDelegate());
}

const MenuDelegate* MenuItemView::GetDelegate() const {
  const auto* const root = GetRootMenuItem();
  return root ? root->delegate_ : nullptr;
}

MenuItemView* MenuItemView::GetRootMenuItem() {
  return const_cast<MenuItemView*>(std::as_const(*this).GetRootMenuItem());
}

const MenuItemView* MenuItemView::GetRootMenuItem() const {
  const MenuItemView* item = this;
  while (item->parent_menu_item_) {
    item = item->parent_menu_item_;
  }
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
    UpdateEmptyMenusAndMetrics();

    controller->MenuChildrenChanged(this);

    if (submenu_) {
      // Force a paint and a synchronous layout. This needs a synchronous layout
      // as UpdateSubmenuSelection() looks at bounds. This handles the case of
      // the top level window's size remaining the same, resulting in no change
      // to the submenu's size and no layout.
      submenu_->DeprecatedLayoutImmediately();
      submenu_->SchedulePaint();
      // Update the menu selection after layout.
      controller->UpdateSubmenuSelection(submenu_.get());
    }
  }

  for (views::View* item : removed_items_) {
    delete item;
  }
  removed_items_.clear();
}

ProposedLayout MenuItemView::CalculateProposedLayout(
    const SizeBounds& size_bounds) const {
  ProposedLayout layout;
  DCHECK(size_bounds.is_fully_bounded());
  layout.host_size =
      gfx::Size(size_bounds.width().value(), size_bounds.height().value());

  if (children().empty()) {
    return layout;
  }

  if (IsContainer()) {
    // This MenuItemView is acting as a thin wrapper around the sole child view,
    // and the size has already been set appropriately for the child's preferred
    // size and margins. The child's bounds can simply be set to the content
    // bounds, less the margins.
    gfx::Rect bounds = GetContentsBounds();
    bounds.Inset(GetContainerMargins());
    layout.child_layouts.emplace_back(children().front(),
                                      children().front()->GetVisible(), bounds);
  } else {
    // Child views are laid out right aligned and given the full height. To
    // right align start with the last view and progress to the first.
    const SubmenuView* const submenu = GetContainingSubmenu();
    int child_end =
        layout.host_size.width() -
        (children_use_full_width_ ? 0 : submenu->trailing_padding());
    for (View* child : base::Reversed(children())) {
      if (icon_view_ == child) {
        continue;
      }
      if (radio_check_image_view_ == child) {
        continue;
      }
      if (submenu_arrow_image_view_ == child) {
        continue;
      }
      if (vertical_separator_ == child) {
        continue;
      }
      int width = child->GetPreferredSize({}).width();
      layout.child_layouts.emplace_back(
          child, child->GetVisible(),
          gfx::Rect(child_end - width, 0, width, layout.host_size.height()));
      child_end -= width + kChildHorizontalPadding;
    }

    // Position the icons.
    const MenuConfig& config = MenuConfig::instance();
    const int icon_x = GetContentStart();
    if (radio_check_image_view_) {
      const int y = (layout.host_size.height() - kMenuCheckSize) / 2;
      layout.child_layouts.emplace_back(
          radio_check_image_view_.get(), radio_check_image_view_->GetVisible(),
          gfx::Rect(icon_x, y, kMenuCheckSize, kMenuCheckSize));
    }
    if (icon_view_) {
      const gfx::Size preferred_size = icon_view_->GetPreferredSize({});
      int x = (config.icons_in_label ? submenu->label_start() : icon_x) +
              ((submenu->icon_area_width() - preferred_size.width()) / 2);
      // If this is a checkbox or radio, then it needs space for both the
      // radio/check image and an icon, so move the icon to where the label
      // would start.
      if (type_ == Type::kCheckbox || type_ == Type::kRadio) {
        x = submenu->label_start();
      }
      const int y = (layout.host_size.height() - preferred_size.height()) / 2;
      layout.child_layouts.emplace_back(
          icon_view_.get(), icon_view_->GetVisible(),
          gfx::Rect(x, y, preferred_size.width(), preferred_size.height()));
    }

    if (submenu_arrow_image_view_) {
      const int x = layout.host_size.width() - GetItemHorizontalBorder() -
                    (type_ == Type::kActionableSubMenu
                         ? config.actionable_submenu_arrow_to_edge_padding
                         : config.arrow_to_edge_padding) -
                    config.arrow_size;
      const int y = (layout.host_size.height() - config.arrow_size) / 2;
      layout.child_layouts.emplace_back(
          submenu_arrow_image_view_.get(),
          submenu_arrow_image_view_->GetVisible(),
          gfx::Rect(x, y, config.arrow_size, config.arrow_size));
    }

    if (vertical_separator_) {
      const gfx::Size preferred_size =
          vertical_separator_->GetPreferredSize({});
      const int x = layout.host_size.width() - config.actionable_submenu_width -
                    config.actionable_submenu_vertical_separator_width;
      const int y = (layout.host_size.height() - preferred_size.height()) / 2;
      layout.child_layouts.emplace_back(
          vertical_separator_.get(), vertical_separator_->GetVisible(),
          gfx::Rect(x, y, preferred_size.width(), preferred_size.height()));
    }
  }

  return layout;
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
  bool ignore_enabled =
      ui::AXPlatform::GetInstance().GetMode().has_mode(ui::AXMode::kNativeAPIs);
  return GetVisible() && (ignore_enabled || GetEnabled());
}

int MenuItemView::GetItemHorizontalBorder() const {
  const auto* const controller = GetMenuController();
  const MenuConfig& config = MenuConfig::instance();
  return (controller && controller->use_ash_system_ui_layout())
             ? config.ash_item_horizontal_border_padding
             : config.item_horizontal_border_padding;
}

std::u16string MenuItemView::GetNewBadgeAccessibleDescription() {
  return l10n_util::GetStringUTF16(IDS_NEW_BADGE_SCREEN_READER_MESSAGE);
}

MenuItemView::MenuItemView(MenuItemView* parent,
                           int command,
                           MenuItemView::Type type)
    : MenuItemView(parent, command, type, /* delegate */ nullptr) {}

MenuItemView::MenuItemView(MenuItemView* parent,
                           int command,
                           Type type,
                           MenuDelegate* delegate)
    : delegate_(delegate),
      parent_menu_item_(parent),
      type_(type),
      command_(command) {
  GetViewAccessibility().set_needs_ax_tree_manager(true);
  UpdateAccessibleRole();
  UpdateAccessibleHasPopup();
  if (type_ == Type::kCheckbox || type_ == Type::kRadio) {
    radio_check_image_view_ = AddChildView(std::make_unique<ImageView>());
    bool show_check_radio_icon =
        type_ == Type::kRadio || (type_ == Type::kCheckbox && GetDelegate() &&
                                  GetDelegate()->IsItemChecked(GetCommand()));
    radio_check_image_view_->SetVisible(show_check_radio_icon);
    radio_check_image_view_->SetCanProcessEventsWithinSubtree(false);
    UpdateAccessibleCheckedState();
  }

  if (type_ == Type::kActionableSubMenu)
    vertical_separator_ = AddChildView(std::make_unique<VerticalSeparator>());

  // Don't request enabled status from the root menu item as it is just
  // a container for real items. kEmpty items will be disabled.
  MenuDelegate* root_delegate = GetDelegate();
  if (parent && type != Type::kEmpty && root_delegate)
    SetEnabled(root_delegate->IsCommandEnabled(command));
  SetLayoutManager(std::make_unique<DelegatingLayoutManager>(this));

  visible_changed_callback_ = AddVisibleChangedCallback(base::BindRepeating(
      &MenuItemView::UpdateAccessibleSelection, base::Unretained(this)));
  enabled_changed_callback_ = AddEnabledChangedCallback(base::BindRepeating(
      &MenuItemView::UpdateAccessibleSelection, base::Unretained(this)));

  UpdateAccessibleSelection();
  UpdateAccessibleKeyShortcuts();
}

void MenuItemView::PrepareForRun(bool has_mnemonics, bool show_mnemonics) {
  // Currently we only support showing the root.
  DCHECK(!parent_menu_item_);

  // Force us to have a submenu.
  CreateSubmenu();
  actual_menu_position_ = requested_menu_position_;
  canceled_ = false;

  has_mnemonics_ = has_mnemonics;
  show_mnemonics_ = has_mnemonics && show_mnemonics;

  UpdateEmptyMenusAndMetrics();

  // Update accessible key shortcuts for all menu items when root's
  // `has_mnemonics_` changes.
  for (MenuItemView* item : GetSubmenu()->GetMenuItems()) {
    item->UpdateAccessibleKeyShortcuts();
  }
}

int MenuItemView::GetDrawStringFlags() const {
  int flags = base::i18n::IsRTL() ? gfx::Canvas::TEXT_ALIGN_RIGHT
                                  : gfx::Canvas::TEXT_ALIGN_LEFT;

  if (GetRootMenuItem()->has_mnemonics_ && may_have_mnemonics()) {
    flags |= (MenuConfig::instance().show_mnemonics ||
              GetRootMenuItem()->show_mnemonics_)
                 ? gfx::Canvas::SHOW_PREFIX
                 : gfx::Canvas::HIDE_PREFIX;
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
  auto* menu_controller = GetMenuController();
  if (menu_controller && menu_controller->use_ash_system_ui_layout()) {
    return TypographyProvider::Get().GetFont(style::CONTEXT_TOUCH_MENU,
                                             style::STYLE_PRIMARY);
  }
  return menu_controller && menu_controller->IsContextMenu()
             ? MenuConfig::instance().context_menu_font_list
             : MenuConfig::instance().font_list;
}

const std::optional<SkColor> MenuItemView::GetMenuLabelColor() const {
  if (const MenuDelegate* delegate = GetDelegate()) {
    if (const auto& label_color = delegate->GetLabelColor(GetCommand()))
      return label_color;
  }
  return std::nullopt;
}

void MenuItemView::UpdateEmptyMenusAndMetrics() {
  CHECK(HasSubmenu());

  // Remove any existing empty menu items, and see whether that leaves any other
  // visible items. Copy the children, since we may mutate them as we go.
  const Views children = submenu_->children();
  bool has_visible_menu_items = false;
  for (View* child : children) {
    if (IsViewClass<EmptyMenuMenuItem>(child)) {
      submenu_->RemoveChildViewT(child);
      submenu_
          ->InvalidateLayout();  // Ideally the submenu would have a layout
                                 // manager that would do this automatically.
    } else {
      has_visible_menu_items |=
          IsViewClass<MenuItemView>(child) && child->GetVisible();
    }
  }

  // Now add back an empty menu item if need be.
  if (!has_visible_menu_items) {
    submenu_->AddChildViewAt(std::make_unique<EmptyMenuMenuItem>(this), 0);
    submenu_->InvalidateLayout();  // Ideally the submenu would have a layout
                                   // manager that would do this automatically.
  }

  submenu_->UpdateMenuPartSizes();
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
  const int vertical_margin = GetVerticalMargin();
  const int available_height = height() - vertical_margin * 2;
  const int text_height = font_list.GetHeight();
  const int total_text_height =
      secondary_title().empty() ? text_height : text_height * 2;
  const int top_margin =
      vertical_margin + (available_height - total_text_height) / 2;

  // Render the foreground.
  const SubmenuView* const submenu = GetContainingSubmenu();
  int accel_width = submenu->max_minor_text_width();
  int label_start = GetLabelStartForThisItem();

  int width =
      this->width() - submenu->trailing_padding() - accel_width - label_start;
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

  if (ShouldShowNewBadge()) {
    BadgePainter::PaintBadge(canvas, this,
                             label_start +
                                 gfx::GetStringWidth(title(), font_list) +
                                 BadgePainter::kBadgeHorizontalMargin,
                             top_margin, new_badge_text_, font_list);
  }

  PaintMinorIconAndText(canvas, colors.minor_fg_color);
}

void MenuItemView::PaintBackground(gfx::Canvas* canvas,
                                   PaintMode mode,
                                   bool paint_as_selected) {
  if (menu_item_background_.has_value()) {
    MenuItemBackground background_info = menu_item_background_.value();
    gfx::Rect bounds = GetLocalBounds();
    bounds.Inset(gfx::Insets::VH(0, GetItemHorizontalBorder()));
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(
        GetColorProvider()->GetColor(background_info.background_color_id));
    canvas->DrawRoundRect(bounds, background_info.corner_radius, flags);
  }
  const auto& config = MenuConfig::instance();
  if (type_ == Type::kHighlighted || is_alerted_ ||
      (paint_as_selected && selected_color_id_.has_value())) {
    SkColor color = gfx::kPlaceholderColor;

    ui::ColorProvider* color_provider = GetColorProvider();
    if (type_ == Type::kHighlighted || selected_color_id_.has_value()) {
      const ui::ColorId color_id = selected_color_id_.value_or(
          paint_as_selected ? ui::kColorMenuItemBackgroundSelected
                            : ui::kColorMenuItemBackgroundHighlighted);
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
                              config.actionable_submenu_width - 1);
      }
    }
    AdjustBoundsForRTLUI(&item_bounds);

    ui::NativeTheme::MenuItemExtraParams menu_item_extra_params;
    menu_item_extra_params.corner_radius = config.item_corner_radius;
    GetNativeTheme()->Paint(
        canvas->sk_canvas(), GetColorProvider(),
        ui::NativeTheme::kMenuItemBackground, ui::NativeTheme::kHovered,
        item_bounds, ui::NativeTheme::ExtraParams(menu_item_extra_params));
  }
}

void MenuItemView::PaintMinorIconAndText(gfx::Canvas* canvas, SkColor color) {
  const std::u16string minor_text = GetMinorText();
  if (minor_text.empty() && minor_icon_.IsEmpty()) {
    return;
  }

  const SubmenuView* const submenu = GetContainingSubmenu();
  const int max_minor_text_width = submenu->max_minor_text_width();
  const MenuConfig& config = MenuConfig::instance();
  const int vertical_margin = GetVerticalMargin();
  gfx::Rect minor_text_bounds(
      width() - submenu->trailing_padding() - max_minor_text_width,
      vertical_margin, max_minor_text_width, height() - vertical_margin * 2);
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

  if (!minor_icon_.IsEmpty()) {
    const gfx::ImageSkia image = minor_icon_.Rasterize(GetColorProvider());

    const int image_x =
        GetMirroredRect(minor_text_bounds).right() -
        render_text->GetContentWidth() -
        (minor_text.empty() ? 0 : config.item_horizontal_padding) -
        image.width();
    const int image_y = minor_text_bounds.y() +
                        (minor_text_bounds.height() - image.height()) / 2;
    canvas->DrawImageInt(
        image, GetMirroredXWithWidthInView(image_x, image.width()), image_y);
  }
}

SkColor MenuItemView::GetTextColor(bool minor, bool paint_as_selected) const {
  // Use a custom color if provided by the controller. If the item is selected,
  // use the default color.
  if (!paint_as_selected && foreground_color_id_.has_value()) {
    return GetColorProvider()->GetColor(foreground_color_id_.value());
  }

  // If the menu item is highlighted and a custom selected color and foreground
  // color have been set, use the custom foreground color. Otherwise, use the
  // default color.
  if (paint_as_selected && selected_color_id_.has_value() &&
      foreground_color_id_.has_value()) {
    return GetColorProvider()->GetColor(foreground_color_id_.value());
  }

  style::TextContext context =
      GetMenuController() && GetMenuController()->use_ash_system_ui_layout()
          ? style::CONTEXT_TOUCH_MENU
          : style::CONTEXT_MENU;

  style::TextStyle text_style = style::STYLE_PRIMARY;
  if (type_ == Type::kHighlighted) {
    text_style = style::STYLE_HIGHLIGHTED;
  } else if (!GetEnabled()) {
    text_style = style::STYLE_DISABLED;
  } else if (paint_as_selected) {
    text_style = style::STYLE_SELECTED;
  } else if (minor) {
    text_style = style::STYLE_SECONDARY;
  }

  return GetColorProvider()->GetColor(
      TypographyProvider::Get().GetColorId(context, text_style));
}

MenuItemView::Colors MenuItemView::CalculateColors(
    bool paint_as_selected) const {
  const std::optional<SkColor> label_color_from_delegate = GetMenuLabelColor();
  Colors colors;
  colors.fg_color = label_color_from_delegate
                        ? *label_color_from_delegate
                        : GetTextColor(/*minor=*/false, paint_as_selected);
  colors.icon_color = color_utils::DeriveDefaultIconColor(colors.fg_color);
  colors.minor_fg_color = GetTextColor(/*minor=*/true, paint_as_selected);
  return colors;
}

std::u16string MenuItemView::CalculateAccessibleName() const {
  std::u16string item_text = View::GetViewAccessibility().GetCachedName();
  if (!item_text.empty())
    return item_text;

  // Use the default accessible name if none is provided.
  if (IsContainer()) {
    // The first child is taking over, just use its accessible name instead of
    // |title_|.
    View* child = children().front();
    ui::AXNodeData child_node_data;
    child->GetViewAccessibility().GetAccessibleNodeData(&child_node_data);
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

gfx::Size MenuItemView::GetChildPreferredSize() const {
  if (children().empty())
    return gfx::Size();

  if (IsContainer())
    return children().front()->GetPreferredSize({});

  const auto add_width = [this](int width, const View* child) {
    if (child == icon_view_ || child == radio_check_image_view_ ||
        child == submenu_arrow_image_view_ || child == vertical_separator_)
      return width;
    if (width)
      width += kChildHorizontalPadding;
    return width + child->GetPreferredSize({}).width();
  };
  const int width =
      std::accumulate(children().cbegin(), children().cend(), 0, add_width);

  // If there is no icon view it returns a height of 0 to indicate that
  // we should use the title height instead.
  const int height = icon_view_ ? icon_view_->GetPreferredSize({}).height() : 0;

  return gfx::Size(width, height);
}

MenuItemView::MenuItemDimensions MenuItemView::CalculateDimensions() const {
  const gfx::Size child_size = GetChildPreferredSize();
  const bool use_ash_system_ui_layout =
      GetMenuController() && GetMenuController()->use_ash_system_ui_layout();

  MenuItemDimensions dimensions;
  dimensions.children_width = child_size.width();
  const MenuConfig& config = MenuConfig::instance();
  dimensions.height = use_ash_system_ui_layout ? config.touchable_menu_height
                                               : child_size.height();

  // In the container case, only the child size plus margins need to be
  // considered.
  if (IsContainer()) {
    const gfx::Insets margins = GetContainerMargins();
    dimensions.height =
        ApplyMinIconHeight(dimensions.height) + margins.height();
    dimensions.children_width += margins.width();
    ApplyMinimumDimensions(&dimensions);
    return dimensions;
  }

  const gfx::FontList& font_list = GetFontList();
  const int title_width = gfx::GetStringWidth(title_, font_list);
  dimensions.standard_width = GetLabelStartForThisItem() + title_width +
                              GetContainingSubmenu()->trailing_padding();
  // Add additional padding to ensure that titles have enough space between
  // themselves and child views.
  if (title_width && dimensions.children_width) {
    dimensions.standard_width += LayoutProvider::Get()->GetDistanceMetric(
        views::DISTANCE_RELATED_LABEL_HORIZONTAL);
  }
  if (ShouldShowNewBadge()) {
    dimensions.standard_width +=
        BadgePainter::kBadgeHorizontalMargin +
        views::BadgePainter::GetBadgeSize(new_badge_text_, font_list).width();
  }

  if (use_ash_system_ui_layout) {
    // Calculate total item width to make sure the current |title_|
    // has enough room within the context menu.
    dimensions.standard_width =
        std::clamp(dimensions.standard_width, config.touchable_menu_min_width,
                   config.touchable_menu_max_width);

    if (icon_view_) {
      dimensions.height =
          std::max(dimensions.height,
                   icon_view_->GetPreferredSize({}).height() +
                       2 * config.vertical_touchable_menu_item_padding);
    }
    return dimensions;
  }

  const int vertical_margins = GetVerticalMargin() * 2;
  dimensions.height = ApplyMinIconHeight(dimensions.height) + vertical_margins;

  // Determine the length of the right-side text.
  std::u16string minor_text = GetMinorText();
  dimensions.minor_text_width =
      (minor_text.empty() ? 0 : gfx::GetStringWidth(minor_text, font_list));
  if (int minor_icon_width = minor_icon_.Size().width()) {
    if (dimensions.minor_text_width) {
      dimensions.minor_text_width += config.item_horizontal_padding;
    }
    dimensions.minor_text_width += minor_icon_width;
  }
  if (!config.reserve_dedicated_arrow_column && HasSubmenu()) {
    if (dimensions.minor_text_width) {
      dimensions.minor_text_width += config.item_horizontal_padding;
    }
    dimensions.minor_text_width +=
        config.arrow_size +
        ((type_ == Type::kActionableSubMenu)
             ? config.actionable_submenu_arrow_to_edge_padding
             : config.arrow_to_edge_padding);
  }

  // Determine the height to use.
  int label_text_height = secondary_title().empty() ? font_list.GetHeight()
                                                    : font_list.GetHeight() * 2;
  dimensions.height =
      std::max(dimensions.height, label_text_height + vertical_margins);

  ApplyMinimumDimensions(&dimensions);
  return dimensions;
}

void MenuItemView::ApplyMinimumDimensions(MenuItemDimensions* dims) const {
  // Don't apply minimums to menus without controllers or to comboboxes.
  if (const auto* const controller = GetMenuController();
      !controller || controller->IsCombobox()) {
    return;
  }

  const MenuConfig& config = MenuConfig::instance();
  dims->height = std::max(dims->height,
                          IsContainer() ? config.minimum_container_item_height
                                        : config.minimum_text_item_height);
}

int MenuItemView::ApplyMinIconHeight(int height) const {
  // Separators have their own config values for minimum height, and don't look
  // odd if they happen to be "shorter" than normal items, so exclude them.
  return type_ == Type::kSeparator
             ? height
             : std::max(height, GetContainingSubmenu()->min_icon_height());
}

int MenuItemView::GetLabelStartForThisItem() const {
  const bool icons_in_label = MenuConfig::instance().icons_in_label;

  // Titles without icons should always be flush left. (This happens
  // automatically when `icons_in_label` is true.)
  if (!icons_in_label && type_ == Type::kTitle && !icon_view_) {
    return GetContentStart();
  }

  // When `!icons_in_label`, checkbox or radio items that also have icons draw
  // the check/radio in the normal icon space and move the icon to where the
  // label starts; see comments in Layout().
  const SubmenuView* const submenu = GetContainingSubmenu();
  if (!icon_view_ ||
      (!icons_in_label && type_ != Type::kCheckbox && type_ != Type::kRadio)) {
    return submenu->label_start();
  }

  // The icon will be drawn starting where the label normally starts, so indent
  // past it.
  const int icon_width = icons_in_label
                             ? submenu->icon_area_width()
                             : icon_view_->GetPreferredSize({}).width();
  return submenu->label_start() + icon_width +
         LayoutProvider::Get()->GetDistanceMetric(
             DISTANCE_RELATED_LABEL_HORIZONTAL);
}

std::u16string MenuItemView::GetMinorText() const {
  std::u16string accel_text;
  return MenuConfig::instance().ShouldShowAcceleratorText(this, &accel_text)
             ? accel_text
             : minor_text_;
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
  const int vertical_margin = GetVerticalMargin();
  margins.set_top(std::max(margins.top(), vertical_margin));
  margins.set_bottom(std::max(margins.bottom(), vertical_margin));
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
    submenu_arrow_image_view_->SetImage(ui::ImageModel::FromVectorIcon(
        vector_icons::kSubmenuArrowChromeRefreshIcon, colors.icon_color));
  }
  MenuDelegate* delegate = GetDelegate();
  if (type_ == Type::kCheckbox && delegate &&
      delegate->IsItemChecked(GetCommand())) {
    radio_check_image_view_->SetImage(
        ui::ImageModel::FromVectorIcon(kMenuCheckIcon, colors.icon_color));
  } else if (type_ == Type::kRadio) {
    const bool toggled = delegate && delegate->IsItemChecked(GetCommand());
    const gfx::VectorIcon& radio_icon =
        toggled ? kMenuRadioSelectedIcon : kMenuRadioEmptyIcon;
    const SkColor radio_icon_color = GetColorProvider()->GetColor(
        toggled ? ui::kColorRadioButtonForegroundChecked
                : ui::kColorRadioButtonForegroundUnchecked);
    radio_check_image_view_->SetImage(ui::ImageModel::FromVectorIcon(
        radio_icon, radio_icon_color, kMenuCheckSize));
  }

  // Update any vector icons if a custom color is used or if the icon is
  // disabled.
  if ((!GetEnabled() || foreground_color_id_.has_value()) && icon_view_) {
    ui::ImageModel icon_model = icon_view_->GetImageModel();
    if (!icon_model.IsEmpty() && icon_model.IsVectorIcon()) {
      ui::VectorIconModel model = icon_model.GetVectorIcon();
      const gfx::VectorIcon* icon = model.vector_icon();
      const ui::ImageModel& image_model = ui::ImageModel::FromVectorIcon(
          *icon,
          GetEnabled()
              ? GetColorProvider()->GetColor(foreground_color_id_.value())
              : GetColorProvider()->GetColor(ui::kColorMenuIconDisabled),
          model.icon_size());
      icon_view_->SetImage(image_model);
    }
  }
}

bool MenuItemView::ShouldPaintAsSelected(PaintMode mode) const {
  return forced_visual_selection_.value_or(
      mode == PaintMode::kNormal && IsSelected() &&
      GetContainingSubmenu()->GetShowSelection(this) &&
      (NonIconChildViewsCount() == 0 ||
       highlight_when_selected_with_child_views_));
}

bool MenuItemView::IsScheduledForDeletion() const {
  return parent_menu_item_ &&
         (base::Contains(parent_menu_item_->removed_items_, this) ||
          parent_menu_item_->IsScheduledForDeletion());
}

int MenuItemView::GetVerticalMargin() const {
  if (vertical_margin_.has_value()) {
    return vertical_margin_.value();
  }

  const auto* const controller = GetMenuController();
  const MenuConfig& config = MenuConfig::instance();
  return (controller && controller->use_ash_system_ui_layout())
             ? config.ash_item_vertical_margin
             : config.item_vertical_margin;
}

ViewAccessibility* MenuItemView::GetSubmenuViewAccessibility() {
  return submenu_ ? &submenu_->GetViewAccessibility() : nullptr;
}

ViewAccessibility* MenuItemView::GetScrollViewContainerViewAccessibility() {
  return submenu_ && submenu_->GetScrollViewContainer()
             ? &submenu_->GetScrollViewContainer()->GetViewAccessibility()
             : nullptr;
}

void MenuItemView::UpdateAccessibleKeyShortcuts() {
  char16_t mnemonic = GetMnemonic();
  if (mnemonic != '\0') {
    std::string key_shortcuts = base::UTF16ToUTF8(std::u16string(1, mnemonic));
    GetViewAccessibility().SetKeyShortcuts(key_shortcuts);
    if (auto* submenuview_accessibility = GetSubmenuViewAccessibility()) {
      submenuview_accessibility->SetKeyShortcuts(key_shortcuts);
    }
    if (auto* scrollview_accessibility =
            GetScrollViewContainerViewAccessibility()) {
      scrollview_accessibility->SetKeyShortcuts(key_shortcuts);
    }
  } else {
    GetViewAccessibility().RemoveKeyShortcuts();
    if (auto* submenuview_accessibility = GetSubmenuViewAccessibility()) {
      submenuview_accessibility->RemoveKeyShortcuts();
    }
    if (auto* scrollview_accessibility =
            GetScrollViewContainerViewAccessibility()) {
      scrollview_accessibility->RemoveKeyShortcuts();
    }
  }
}

void MenuItemView::UpdateAccessibleSelection() {
  GetViewAccessibility().SetIsSelected(IsTraversableByKeyboard() && selected_);
}

void MenuItemView::UpdateAccessibleRole() {
  // Set the role based on the type of menu item.
  switch (type_) {
    case Type::kCheckbox:
      GetViewAccessibility().SetRole(ax::mojom::Role::kMenuItemCheckBox);
      break;
    case Type::kRadio:
      GetViewAccessibility().SetRole(ax::mojom::Role::kMenuItemRadio);
      break;
    default:
      GetViewAccessibility().SetRole(ax::mojom::Role::kMenuItem);
      break;
  }
}

void MenuItemView::UpdateAccessibleHasPopup() {
  switch (type_) {
    case Type::kSubMenu:
    case Type::kActionableSubMenu:
      // Note: This is neither necessary nor sufficient for macOS. See
      // CreateSubmenu() for virtual child creation and explanation.
      GetViewAccessibility().SetHasPopup(ax::mojom::HasPopup::kMenu);
      if (auto* submenuview_accessibility = GetSubmenuViewAccessibility()) {
        submenuview_accessibility->SetHasPopup(ax::mojom::HasPopup::kMenu);
      }
      if (auto* scrollview_accessibility =
              GetScrollViewContainerViewAccessibility()) {
        scrollview_accessibility->SetHasPopup(ax::mojom::HasPopup::kMenu);
      }
      break;
    case Type::kCheckbox:
    case Type::kRadio:
    case Type::kTitle:
    case Type::kNormal:
    case Type::kSeparator:
    case Type::kEmpty:
    case Type::kHighlighted:
      // No additional accessibility states currently for these menu states.
      break;
  }
}

void MenuItemView::UpdateAccessibleName() {
  std::u16string accessible_name = CalculateAccessibleName();
  if (accessible_name.empty()) {
    GetViewAccessibility().SetName(
        std::string(), ax::mojom::NameFrom::kAttributeExplicitlyEmpty);
    if (auto* submenuview_accessibility = GetSubmenuViewAccessibility()) {
      submenuview_accessibility->SetName(
          std::string(), ax::mojom::NameFrom::kAttributeExplicitlyEmpty);
    }
    if (auto* scrollview_accessibility =
            GetScrollViewContainerViewAccessibility()) {
      scrollview_accessibility->SetName(
          std::string(), ax::mojom::NameFrom::kAttributeExplicitlyEmpty);
    }
  } else {
    GetViewAccessibility().SetName(accessible_name);
    if (auto* submenuview_accessibility = GetSubmenuViewAccessibility()) {
      submenuview_accessibility->SetName(accessible_name);
    }
    if (auto* scrollview_accessibility =
            GetScrollViewContainerViewAccessibility()) {
      scrollview_accessibility->SetName(accessible_name);
    }
  }
}

BEGIN_METADATA(MenuItemView)
END_METADATA

// EmptyMenuMenuItem ----------------------------------------------------------

EmptyMenuMenuItem::EmptyMenuMenuItem(MenuItemView* parent)
    : MenuItemView(parent, 0, Type::kEmpty) {
  SetTitle(l10n_util::GetStringUTF16(IDS_APP_MENU_EMPTY_SUBMENU));
  SetEnabled(false);
}

BEGIN_METADATA(EmptyMenuMenuItem)
END_METADATA

}  // namespace views
