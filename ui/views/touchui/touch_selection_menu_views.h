// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TOUCHUI_TOUCH_SELECTION_MENU_VIEWS_H_
#define UI_VIEWS_TOUCHUI_TOUCH_SELECTION_MENU_VIEWS_H_

#include "base/memory/raw_ptr.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/touchui/touch_selection_menu_runner_views.h"

namespace ui {
class TouchSelectionMenuClient;
}  // namespace ui

namespace views {
class LabelButton;

// A bubble that contains actions available for the selected text. An object of
// this type, as a BubbleDialogDelegateView, manages its own lifetime.
class VIEWS_EXPORT TouchSelectionMenuViews : public BubbleDialogDelegateView {
  METADATA_HEADER(TouchSelectionMenuViews, BubbleDialogDelegateView)

 public:
  enum ButtonViewId : int { kEllipsisButton = 1 };

  TouchSelectionMenuViews(TouchSelectionMenuRunnerViews* owner,
                          base::WeakPtr<ui::TouchSelectionMenuClient> client,
                          aura::Window* context);

  TouchSelectionMenuViews(const TouchSelectionMenuViews&) = delete;
  TouchSelectionMenuViews& operator=(const TouchSelectionMenuViews&) = delete;

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
  LabelButton* CreateButton(const std::u16string& title,
                            Button::PressedCallback callback);

  void CreateSeparator();

 private:
  friend class TouchSelectionMenuRunnerViews::TestApi;

  void ButtonPressed(int command, const ui::Event& event);

  void EllipsisPressed(const ui::Event& event);

  // Helper to disconnect this menu object from its owning menu runner.
  void DisconnectOwner();

  // BubbleDialogDelegateView:
  void WindowClosing() override;

  raw_ptr<TouchSelectionMenuRunnerViews> owner_;
  const base::WeakPtr<ui::TouchSelectionMenuClient> client_;
};

}  // namespace views

#endif  // UI_VIEWS_TOUCHUI_TOUCH_SELECTION_MENU_VIEWS_H_
