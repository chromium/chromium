// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TOUCHUI_TOUCH_SELECTION_MENU_RUNNER_VIEWS_H_
#define UI_VIEWS_TOUCHUI_TOUCH_SELECTION_MENU_RUNNER_VIEWS_H_

#include <memory>

#include "base/macros.h"
#include "ui/touch_selection/touch_selection_menu_runner.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/views_export.h"

namespace views {
class LabelButton;
class TouchSelectionMenuViews;
class Widget;

// Views implementation for TouchSelectionMenuRunner.
class VIEWS_EXPORT TouchSelectionMenuRunnerViews
    : public ui::TouchSelectionMenuRunner {
 public:
  // Test API to access internal state in tests.
  class VIEWS_EXPORT TestApi {
   public:
    explicit TestApi(TouchSelectionMenuRunnerViews* menu_runner);
    ~TestApi();

    gfx::Rect GetAnchorRect() const;
    LabelButton* GetFirstButton();
    Widget* GetWidget();

   private:
    TouchSelectionMenuRunnerViews* menu_runner_;

    DISALLOW_COPY_AND_ASSIGN(TestApi);
  };

  TouchSelectionMenuRunnerViews();
  ~TouchSelectionMenuRunnerViews() override;

 protected:
  // Sets the menu as the currently runner menu and shows it.
  void ShowMenu(TouchSelectionMenuViews* menu,
                const gfx::Rect& anchor_rect,
                const gfx::Size& handle_image_size);

  // ui::TouchSelectionMenuRunner:
  bool IsMenuAvailable(
      const ui::TouchSelectionMenuClient* client) const override;
  void CloseMenu() override;
  void OpenMenu(ui::TouchSelectionMenuClient* client,
                const gfx::Rect& anchor_rect,
                const gfx::Size& handle_image_size,
                aura::Window* context) override;
  bool IsRunning() const override;

 private:
  friend class TouchSelectionMenuViews;

  // A pointer to the currently running menu, or |nullptr| if no menu is
  // running. The menu manages its own lifetime and deletes itself when closed.
  TouchSelectionMenuViews* menu_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TouchSelectionMenuRunnerViews);
};

}  // namespace views

#endif  // UI_VIEWS_TOUCHUI_TOUCH_SELECTION_MENU_RUNNER_VIEWS_H_
