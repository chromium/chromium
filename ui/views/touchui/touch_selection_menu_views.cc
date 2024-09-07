// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/touchui/touch_selection_menu_views.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/pointer/touch_editing_controller.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/text_utils.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/touch_selection/touch_selection_menu_runner.h"
#include "ui/touch_selection/touch_selection_metrics.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"

namespace views {
namespace {

struct MenuCommand {
  int command_id;
  int message_id;
};

MenuCommand kMenuCommands[] = {
    {ui::TouchEditable::kCut, IDS_APP_CUT},
    {ui::TouchEditable::kCopy, IDS_APP_COPY},
    {ui::TouchEditable::kPaste, IDS_APP_PASTE},
};

MenuCommand kMenuSelectCommands[] = {
    {ui::TouchEditable::kSelectWord, IDS_APP_SELECT},
    {ui::TouchEditable::kSelectAll, IDS_APP_SELECT_ALL},
};

// Constants to apply when touch text editing redesign is disabled.
constexpr gfx::Insets kMenuMargins = gfx::Insets(1);
constexpr gfx::Size kMenuButtonMinSize = gfx::Size(63, 38);

// Constants to apply when touch text editing redesign is enabled.
constexpr gfx::Insets kEmptyMenuMargins = gfx::Insets(0);
constexpr int kMenuCornerRadius = 8;
// Padding to add space between the menu and the selection bounds and handles.
constexpr int kMenuAnchorRectPadding = 8;
// Padding to apply horizontally around button labels.
constexpr int kButtonHorizontalPadding = 16;
constexpr int kButtonMinHeight = 40;

}  // namespace

TouchSelectionMenuViews::TouchSelectionMenuViews(
    TouchSelectionMenuRunnerViews* owner,
    base::WeakPtr<ui::TouchSelectionMenuClient> client,
    aura::Window* context)
    : BubbleDialogDelegateView(nullptr, BubbleBorder::BOTTOM_CENTER),
      owner_(owner),
      client_(client) {
  DCHECK(owner_);
  DCHECK(client_);

  DialogDelegate::SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  set_shadow(BubbleBorder::STANDARD_SHADOW);
  set_parent_window(context);
  if (::features::IsTouchTextEditingRedesignEnabled()) {
    set_margins(kEmptyMenuMargins);
    set_corner_radius(kMenuCornerRadius);
  } else {
    set_margins(kMenuMargins);
  }
  SetCanActivate(false);
  set_adjust_if_offscreen(true);
  SetFlipCanvasOnPaintForRTLUI(true);

  SetLayoutManager(std::make_unique<BoxLayout>());
}

void TouchSelectionMenuViews::ShowMenu(const gfx::Rect& anchor_rect,
                                       const gfx::Size& handle_image_size) {
  CreateButtons();

  // After buttons are created, check if there is enough room between handles to
  // show the menu and adjust anchor rect properly if needed, just in case the
  // menu is needed to be shown under the selection.
  gfx::Rect adjusted_anchor_rect(anchor_rect);
  int menu_width = GetPreferredSize({}).width();
  // TODO(mfomitchev): This assumes that the handles are center-aligned to the
  // |achor_rect| edges, which is not true. We should fix this, perhaps by
  // passing down the cumulative width occupied by the handles within
  // |anchor_rect| plus the handle image height instead of |handle_image_size|.
  // Perhaps we should also allow for some minimum padding.
  if (menu_width > anchor_rect.width() - handle_image_size.width())
    adjusted_anchor_rect.Inset(
        gfx::Insets::TLBR(0, 0, -handle_image_size.height(), 0));
  if (::features::IsTouchTextEditingRedesignEnabled()) {
    adjusted_anchor_rect.Outset(kMenuAnchorRectPadding);
  }
  SetAnchorRect(adjusted_anchor_rect);

  BubbleDialogDelegateView::CreateBubble(this);
  Widget* widget = GetWidget();
  gfx::Rect bounds = widget->GetWindowBoundsInScreen();
  gfx::Rect work_area = display::Screen::GetScreen()
                            ->GetDisplayNearestPoint(bounds.origin())
                            .work_area();
  if (!work_area.IsEmpty()) {
    bounds.AdjustToFit(work_area);
    widget->SetBounds(bounds);
  }

  widget->SetZOrderLevel(ui::ZOrderLevel::kFloatingWindow);
  widget->Show();
}

bool TouchSelectionMenuViews::IsMenuAvailable(
    const ui::TouchSelectionMenuClient* client) {
  DCHECK(client);

  const auto is_enabled = [client](MenuCommand command) {
    return client->IsCommandIdEnabled(command.command_id);
  };
  bool is_available = base::ranges::any_of(kMenuCommands, is_enabled);
  is_available |= ::features::IsTouchTextEditingRedesignEnabled() &&
                  base::ranges::any_of(kMenuSelectCommands, is_enabled);
  return is_available;
}

void TouchSelectionMenuViews::CloseMenu() {
  if (owner_)
    DisconnectOwner();
  // Closing the widget will self-destroy this object.
  Widget* widget = GetWidget();
  if (widget && !widget->IsClosed())
    widget->Close();
}

TouchSelectionMenuViews::~TouchSelectionMenuViews() = default;

void TouchSelectionMenuViews::CreateButtons() {
  DCHECK(client_);
  for (const auto& command : kMenuCommands) {
    if (!client_->IsCommandIdEnabled(command.command_id)) {
      continue;
    }
    CreateButton(
        l10n_util::GetStringUTF16(command.message_id),
        base::BindRepeating(&TouchSelectionMenuViews::ButtonPressed,
                            base::Unretained(this), command.command_id));
    CreateSeparator();
  }

  if (::features::IsTouchTextEditingRedesignEnabled()) {
    for (const auto& command : kMenuSelectCommands) {
      if (!client_->IsCommandIdEnabled(command.command_id)) {
        continue;
      }
      CreateButton(
          l10n_util::GetStringUTF16(command.message_id),
          base::BindRepeating(&TouchSelectionMenuViews::ButtonPressed,
                              base::Unretained(this), command.command_id));
      CreateSeparator();
    }
  }

  // Finally, add ellipsis button.
  CreateButton(u"...",
               base::BindRepeating(&TouchSelectionMenuViews::EllipsisPressed,
                                   base::Unretained(this)))
      ->SetID(ButtonViewId::kEllipsisButton);
  InvalidateLayout();
}

LabelButton* TouchSelectionMenuViews::CreateButton(
    const std::u16string& title,
    Button::PressedCallback callback) {
  std::u16string label = gfx::RemoveAccelerator(title);
  auto* button = AddChildView(std::make_unique<LabelButton>(
      std::move(callback), label, style::CONTEXT_TOUCH_MENU));
  if (::features::IsTouchTextEditingRedesignEnabled()) {
    button->SetBorder(
        CreateEmptyBorder(gfx::Insets::VH(0, kButtonHorizontalPadding)));
    button->SetMinSize(gfx::Size(0, kButtonMinHeight));
  } else {
    button->SetMinSize(kMenuButtonMinSize);
  }
  button->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  return button;
}

void TouchSelectionMenuViews::CreateSeparator() {
  AddChildView(std::make_unique<Separator>());
}

void TouchSelectionMenuViews::DisconnectOwner() {
  DCHECK(owner_);
  owner_->menu_ = nullptr;
  owner_ = nullptr;
}

void TouchSelectionMenuViews::WindowClosing() {
  DCHECK(!owner_ || owner_->menu_ == this);
  BubbleDialogDelegateView::WindowClosing();
  if (owner_)
    DisconnectOwner();
}

void TouchSelectionMenuViews::ButtonPressed(int command,
                                            const ui::Event& event) {
  ui::RecordTouchSelectionMenuCommandAction(command);
  CloseMenu();
  if (client_) {
    client_->ExecuteCommand(command, event.flags());
  }
}

void TouchSelectionMenuViews::EllipsisPressed(const ui::Event& event) {
  ui::RecordTouchSelectionMenuEllipsisAction();
  CloseMenu();
  if (client_) {
    client_->RunContextMenu();
  }
}

BEGIN_METADATA(TouchSelectionMenuViews)
END_METADATA

}  // namespace views
