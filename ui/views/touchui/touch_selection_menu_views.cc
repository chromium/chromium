// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/touchui/touch_selection_menu_views.h"

#include <memory>

#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
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

namespace views {
namespace {

constexpr int kMenuCommands[] = {IDS_APP_CUT, IDS_APP_COPY, IDS_APP_PASTE};
constexpr int kSpacingBetweenButtons = 2;
constexpr int kButtonSeparatorColor = SkColorSetARGB(13, 0, 0, 0);
constexpr int kMenuButtonMinHeight = 38;
constexpr int kMenuButtonMinWidth = 63;
constexpr int kMenuMargin = 1;

constexpr char kEllipsesButtonText[] = "...";
constexpr int kEllipsesButtonTag = -1;

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

  set_shadow(BubbleBorder::SMALL_SHADOW);
  set_parent_window(context);
  set_margins(gfx::Insets(kMenuMargin, kMenuMargin, kMenuMargin, kMenuMargin));
  set_can_activate(false);
  set_adjust_if_offscreen(true);
  EnableCanvasFlippingForRTLUI(true);

  SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::kHorizontal, gfx::Insets(), kSpacingBetweenButtons));
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

  for (size_t i = 0; i < base::size(kMenuCommands); i++) {
    if (client->IsCommandIdEnabled(kMenuCommands[i]))
      return true;
  }
  return false;
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
  for (size_t i = 0; i < base::size(kMenuCommands); i++) {
    int command_id = kMenuCommands[i];
    if (!client_->IsCommandIdEnabled(command_id))
      continue;

    Button* button =
        CreateButton(l10n_util::GetStringUTF16(command_id), command_id);
    AddChildView(button);
  }

  // Finally, add ellipses button.
  AddChildView(
      CreateButton(base::UTF8ToUTF16(kEllipsesButtonText), kEllipsesButtonTag));
  Layout();
}

LabelButton* TouchSelectionMenuViews::CreateButton(const base::string16& title,
                                                   int tag) {
  base::string16 label =
      gfx::RemoveAcceleratorChar(title, '&', nullptr, nullptr);
  LabelButton* button = new LabelButton(this, label, style::CONTEXT_TOUCH_MENU);
  button->SetMinSize(gfx::Size(kMenuButtonMinWidth, kMenuButtonMinHeight));
  button->SetFocusForPlatform();
  button->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  button->set_tag(tag);
  return button;
}

void TouchSelectionMenuViews::DisconnectOwner() {
  DCHECK(owner_);
  owner_->menu_ = nullptr;
  owner_ = nullptr;
}

void TouchSelectionMenuViews::OnPaint(gfx::Canvas* canvas) {
  BubbleDialogDelegateView::OnPaint(canvas);

  // Draw separator bars.
  for (int i = 0; i < child_count() - 1; ++i) {
    View* child = child_at(i);
    int x = child->bounds().right() + kSpacingBetweenButtons / 2;
    canvas->FillRect(gfx::Rect(x, 0, 1, child->height()),
                     kButtonSeparatorColor);
  }
}

void TouchSelectionMenuViews::WindowClosing() {
  DCHECK(!owner_ || owner_->menu_ == this);
  BubbleDialogDelegateView::WindowClosing();
  if (owner_)
    DisconnectOwner();
}

int TouchSelectionMenuViews::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

void TouchSelectionMenuViews::ButtonPressed(Button* sender,
                                            const ui::Event& event) {
  CloseMenu();
  if (sender->tag() != kEllipsesButtonTag)
    client_->ExecuteCommand(sender->tag(), event.flags());
  else
    client_->RunContextMenu();
}

}  // namespace views
