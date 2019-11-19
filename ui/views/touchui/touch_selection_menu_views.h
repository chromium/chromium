// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TOUCHUI_TOUCH_SELECTION_MENU_VIEWS_H_
#define UI_VIEWS_TOUCHUI_TOUCH_SELECTION_MENU_VIEWS_H_

#include "base/macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/touchui/touch_selection_menu_runner_views.h"

namespace ui {
class TouchSelectionMenuClient;
}  // namespace ui

namespace views {
class LabelButton;

// A bubble that contains actions available for the selected text. An object of
// this type, as a BubbleDialogDelegateView, manages its own lifetime.
class VIEWS_EXPORT TouchSelectionMenuViews : public BubbleDialogDelegateView,
                                             public ButtonListener {
 public:
  METADATA_HEADER(TouchSelectionMenuViews);

  TouchSelectionMenuViews(TouchSelectionMenuRunnerViews* owner,
                          ui::TouchSelectionMenuClient* client,
                          aura::Window* context);

  void ShowMenu(const gfx::Rect& anchor_rect,
                const gfx::Size& handle_image_size);

  // Checks whether there is any command available to show in the menu.
  static bool IsMenuAvailable(const ui::TouchSelectionMenuClient* client);

  // Closes the menu. This will eventually self-destroy the object.
  void CloseMenu();

 protected:
  ~TouchSelectionMenuViews() override;

  // Queries the |client_| for what commands to show in the menu and sizes the
  // menu appropriately.
  virtual void CreateButtons();

  // Helper method to create a single button.
  LabelButton* CreateButton(const base::string16& title, int tag);

  // ButtonListener:
  void ButtonPressed(Button* sender, const ui::Event& event) override;

 private:
  friend class TouchSelectionMenuRunnerViews::TestApi;

  // Helper to disconnect this menu object from its owning menu runner.
  void DisconnectOwner();

  // BubbleDialogDelegateView:
  void OnPaint(gfx::Canvas* canvas) override;
  void WindowClosing() override;
  int GetDialogButtons() const override;

  TouchSelectionMenuRunnerViews* owner_;
  ui::TouchSelectionMenuClient* const client_;

  DISALLOW_COPY_AND_ASSIGN(TouchSelectionMenuViews);
};

}  // namespace views

#endif  // UI_VIEWS_TOUCHUI_TOUCH_SELECTION_MENU_VIEWS_H_
