// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_runner_impl_remote_cocoa.h"

#include "components/remote_cocoa/browser/window.h"
#include "components/remote_cocoa/common/native_widget_ns_window.mojom.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/menu_model.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/controls/menu/menu_controller_cocoa_delegate_params.h"
#include "ui/views/widget/widget.h"

namespace views {

MenuRunnerImplRemoteCocoa::MenuRunnerImplRemoteCocoa(
    ui::MenuModel* menu_model,
    base::RepeatingClosure on_menu_closed_callback)
    : menu_model_(menu_model),
      on_menu_closed_callback_(std::move(on_menu_closed_callback)) {}

void MenuRunnerImplRemoteCocoa::RunMenu(Widget* widget,
                                        const gfx::Point& anchor,
                                        uint64_t target_view_id) {
  CHECK(!running_);
  running_ = true;

  // Reset fields from a potential previous run.
  closing_event_time_ = base::TimeTicks();
  host_receiver_.reset();
  menu_remote_.reset();

  menu_model_->MenuWillShow();
  std::set<int> command_ids;
  auto menu = remote_cocoa::mojom::ContextMenu::New(
      ModelToMojo(*menu_model_, command_ids), anchor, target_view_id,
      MenuControllerParamsForWidget(widget));
  remote_cocoa::mojom::NativeWidgetNSWindow* remote_window =
      remote_cocoa::GetWindowMojoInterface(widget->GetNativeWindow());
  remote_window->DisplayContextMenu(std::move(menu),
                                    host_receiver_.BindNewPipeAndPassRemote(),
                                    menu_remote_.BindNewPipeAndPassReceiver());
}

void MenuRunnerImplRemoteCocoa::UpdateMenuItem(int command_id,
                                               bool enabled,
                                               bool hidden,
                                               const std::u16string& title) {
  menu_remote_->UpdateMenuItem(command_id, enabled, !hidden, title);
}

bool MenuRunnerImplRemoteCocoa::IsRunning() const {
  return running_;
}

void MenuRunnerImplRemoteCocoa::Release() {
  // No need to delay destroying this, as destroying will automatically make
  // sure no more methods on this class or its menu model will be called.
  if (IsRunning()) {
    Cancel();
  }
  delete this;
}

void MenuRunnerImplRemoteCocoa::RunMenuAt(
    Widget* parent,
    MenuButtonController* button_controller,
    const gfx::Rect& bounds,
    MenuAnchorPosition anchor,
    int32_t run_types,
    gfx::NativeView native_view_for_gestures,
    std::optional<gfx::RoundedCornersF> corners,
    std::optional<std::string> show_menu_host_duration_histogram) {
  RunMenu(parent, bounds.CenterPoint());
}

void MenuRunnerImplRemoteCocoa::Cancel() {
  CHECK(IsRunning());
  CHECK(menu_remote_.is_bound());
  menu_remote_->Cancel();
}

base::TimeTicks MenuRunnerImplRemoteCocoa::GetClosingEventTime() const {
  return closing_event_time_;
}

MenuRunnerImplRemoteCocoa::~MenuRunnerImplRemoteCocoa() = default;

void MenuRunnerImplRemoteCocoa::CommandActivated(int32_t command_id,
                                                 int32_t event_flags) {
  ui::MenuModel* model = menu_model_;
  size_t index = 0;
  if (ui::MenuModel::GetModelAndIndexForCommandId(command_id, &model, &index)) {
    model->ActivatedAt(index, event_flags);
  }
}

void MenuRunnerImplRemoteCocoa::MenuClosed() {
  closing_event_time_ = ui::EventTimeForNow();
  running_ = false;
  menu_model_->MenuWillClose();
  if (on_menu_closed_callback_) {
    std::move(on_menu_closed_callback_).Run();
  }
}

std::vector<remote_cocoa::mojom::MenuItemPtr>
MenuRunnerImplRemoteCocoa::ModelToMojo(const ui::MenuModel& model,
                                       std::set<int>& command_ids) {
  std::vector<remote_cocoa::mojom::MenuItemPtr> result;
  const size_t count = model.GetItemCount();
  result.reserve(count);

  for (size_t index = 0; index < count; ++index) {
    if (model.GetTypeAt(index) == ui::MenuModel::TYPE_SEPARATOR) {
      result.push_back(remote_cocoa::mojom::MenuItem::NewSeparator(
          remote_cocoa::mojom::MenuItemCommonFields::New()));
    } else {
      auto common = remote_cocoa::mojom::MenuItemCommonFields::New();
      common->command_id = model.GetCommandIdAt(index);
      common->label = model.GetLabelAt(index);
      common->may_have_mnemonics = model.MayHaveMnemonicsAt(index);
      common->is_checked = model.IsItemCheckedAt(index);
      ui::ImageModel icon = model.GetIconAt(index);
      if (icon.IsImage()) {
        common->icon = icon.GetImage().AsImageSkia();
      }
      common->is_enabled = model.IsEnabledAt(index);
      common->is_visible = model.IsVisibleAt(index);
      // Note that we don't check IsItemDynamicAt. A new mojom::MenuModelPtr is
      // created every time the menu is shown, so there is no need for extra
      // support for dynamic content.
      common->is_alerted = model.IsAlertedAt(index);
      common->is_new_feature = model.IsNewFeatureAt(index);

      if (common->is_visible &&
          model.GetTypeAt(index) == ui::MenuModel::TYPE_SUBMENU) {
        ui::MenuModel* submenuModel = model.GetSubmenuModelAt(index);
        auto children = ModelToMojo(*submenuModel, command_ids);
        result.push_back(remote_cocoa::mojom::MenuItem::NewSubmenu(
            remote_cocoa::mojom::SubmenuMenuItem::New(std::move(common),
                                                      std::move(children))));
      } else {
        CHECK(command_ids.insert(model.GetCommandIdAt(index)).second)
            << "Duplicate command Id in menu: " << model.GetCommandIdAt(index);
        result.push_back(
            remote_cocoa::mojom::MenuItem::NewRegular(std::move(common)));
      }
    }
  }

  return result;
}

}  // namespace views
