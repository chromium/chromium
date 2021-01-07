// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/touchui/touch_selection_menu_views.h"

#include <memory>
#include <utility>

#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/pointer/touch_editing_controller.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/text_utils.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/touch_selection/touch_selection_menu_runner.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"

namespace views {
namespace {

struct MenuCommand {
  int command_id;
  int message_id;
} kMenuCommands[] = {
    {ui::TouchEditable::kCut, IDS_APP_CUT},
    {ui::TouchEditable::kCopy, IDS_APP_COPY},
    {ui::TouchEditable::kPaste, IDS_APP_PASTE},
};

constexpr int kSpacingBetweenButtons = 2;

}  // namespace

TouchSelectionMenuViews::TouchSelectionMenuViews(
    TouchSelectionMenuRunnerViews* owner,
    ui::TouchSelectionMenuClient* client,
    aura::Window* context)
    : BubbleDialogDelegateView(nullptr, BubbleBorder::BOTTOM_CENTER),
      owner_(owner),
      client_(client) {
  DCHECK(owner_);
  DCHECK(client_);

  DialogDelegate::SetButtons(ui::DIALOG_BUTTON_NONE);
  set_shadow(BubbleBorder::STANDARD_SHADOW);
  set_parent_window(context);
  constexpr gfx::Insets kMenuMargins = gfx::Insets(1);
  set_margins(kMenuMargins);
  SetCanActivate(false);
  set_adjust_if_offscreen(true);
  SetFlipCanvasOnPaintForRTLUI(true);

  SetLayoutManager(
      std::make_unique<BoxLayout>(BoxLayout::Orientation::kHorizontal,
                                  gfx::Insets(), kSpacingBetweenButtons));
}

void TouchSelectionMenuViews::ShowMenu(const gfx::Rect& anchor_rect,
                                       const gfx::Size& handle_image_size) {
  CreateButtons();

  // After buttons are created, check if there is enough room between handles to
  // show the menu and adjust anchor rect properly if needed, just in case the
  // menu is needed to be shown under the selection.
  gfx::Rect adjusted_anchor_rect(anchor_rect);
  int menu_width = GetPreferredSize().width();
  // TODO(mfomitchev): This assumes that the handles are center-aligned to the
  // |achor_rect| edges, which is not true. We should fix this, perhaps by
  // passing down the cumulative width occupied by the handles within
  // |anchor_rect| plus the handle image height instead of |handle_image_size|.
  // Perhaps we should also allow for some minimum padding.
  if (menu_width > anchor_rect.width() - handle_image_size.width())
    adjusted_anchor_rect.Inset(0, 0, 0, -handle_image_size.height());
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
  // Using BubbleDialogDelegateView engages its CreateBubbleWidget() which
  // invokes widget->StackAbove(context). That causes the bubble to stack
  // _immediately_ above |context|; below any already-existing bubbles. That
  // doesn't make sense for a menu, so put it back on top.
  widget->StackAtTop();
  widget->Show();
}

bool TouchSelectionMenuViews::IsMenuAvailable(
    const ui::TouchSelectionMenuClient* client) {
  DCHECK(client);

  const auto is_enabled = [client](MenuCommand command) {
    return client->IsCommandIdEnabled(command.command_id);
  };
  return std::any_of(std::cbegin(kMenuCommands), std::cend(kMenuCommands),
                     is_enabled);
}

void TouchSelectionMenuViews::CloseMenu() {
  DisconnectOwner();
  // Closing the widget will self-destroy this object.
  Widget* widget = GetWidget();
  if (widget && !widget->IsClosed())
    widget->Close();
}

TouchSelectionMenuViews::~TouchSelectionMenuViews() = default;

void TouchSelectionMenuViews::CreateButtons() {
  for (const auto& command : kMenuCommands) {
    if (client_->IsCommandIdEnabled(command.command_id)) {
      CreateButton(
          l10n_util::GetStringUTF16(command.message_id),
          base::BindRepeating(&TouchSelectionMenuViews::ButtonPressed,
                              base::Unretained(this), command.command_id));
    }
  }

  // Finally, add ellipsis button.
  CreateButton(base::ASCIIToUTF16("..."),
               base::BindRepeating(
                   [](TouchSelectionMenuViews* menu) {
                     menu->CloseMenu();
                     menu->client_->RunContextMenu();
                   },
                   base::Unretained(this)))
      ->SetID(ButtonViewId::kEllipsisButton);
  InvalidateLayout();
}

LabelButton* TouchSelectionMenuViews::CreateButton(
    const base::string16& title,
    Button::PressedCallback callback) {
  base::string16 label = gfx::RemoveAccelerator(title);
  auto* button = AddChildView(std::make_unique<LabelButton>(
      std::move(callback), label, style::CONTEXT_TOUCH_MENU));
  constexpr gfx::Size kMenuButtonMinSize = gfx::Size(63, 38);
  button->SetMinSize(kMenuButtonMinSize);
  button->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  return button;
}

void TouchSelectionMenuViews::DisconnectOwner() {
  DCHECK(owner_);
  owner_->menu_ = nullptr;
  owner_ = nullptr;
}

void TouchSelectionMenuViews::OnPaint(gfx::Canvas* canvas) {
  BubbleDialogDelegateView::OnPaint(canvas);
  if (children().empty())
    return;

  // Draw separator bars.
  for (auto i = children().cbegin(); i != std::prev(children().cend()); ++i) {
    const View* child = *i;
    int x = child->bounds().right() + kSpacingBetweenButtons / 2;
    canvas->FillRect(gfx::Rect(x, 0, 1, child->height()),
                     GetNativeTheme()->GetSystemColor(
                         ui::NativeTheme::kColorId_SeparatorColor));
  }
}

void TouchSelectionMenuViews::WindowClosing() {
  DCHECK(!owner_ || owner_->menu_ == this);
  BubbleDialogDelegateView::WindowClosing();
  if (owner_)
    DisconnectOwner();
}

void TouchSelectionMenuViews::ButtonPressed(int command,
                                            const ui::Event& event) {
  CloseMenu();
  client_->ExecuteCommand(command, event.flags());
}

BEGIN_METADATA(TouchSelectionMenuViews, BubbleDialogDelegateView)
END_METADATA

}  // namespace views
