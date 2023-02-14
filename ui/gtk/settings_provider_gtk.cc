// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gtk/settings_provider_gtk.h"

#include "base/strings/string_split.h"
#include "gtk_compat.h"
#include "ui/gtk/gtk_compat.h"
#include "ui/gtk/gtk_ui.h"
#include "ui/gtk/gtk_util.h"

namespace gtk {

namespace {

std::string GetDecorationLayoutFromGtkWindow() {
  DCHECK(!GtkCheckVersion(4));

  GtkCssContext context = GetStyleContextFromCss("");
  gtk_style_context_add_class(context, "csd");

  gchar* layout_c = nullptr;
  GtkStyleContextGetStyle(context, "decoration-button-layout", &layout_c,
                          nullptr);
  DCHECK(layout_c);
  std::string layout(layout_c);
  g_free(layout_c);
  return layout;
}

void ParseActionString(const std::string& value,
                       GtkUi::WindowFrameAction* action) {
  if (value == "none") {
    *action = ui::LinuxUi::WindowFrameAction::kNone;
  } else if (value == "lower") {
    *action = ui::LinuxUi::WindowFrameAction::kLower;
  } else if (value == "minimize") {
    *action = ui::LinuxUi::WindowFrameAction::kMinimize;
  } else if (value == "toggle-maximize") {
    *action = ui::LinuxUi::WindowFrameAction::kToggleMaximize;
  } else if (value == "menu") {
    *action = ui::LinuxUi::WindowFrameAction::kMenu;
  }
}

}  // namespace

SettingsProviderGtk::FrameActionSettingWatcher::FrameActionSettingWatcher(
    SettingsProviderGtk* settings_provider,
    const std::string& setting_name,
    ui::LinuxUi::WindowFrameActionSource action_type,
    ui::LinuxUi::WindowFrameAction default_action)
    : settings_provider_(settings_provider),
      setting_name_(setting_name),
      action_type_(action_type),
      default_action_(default_action) {
  GtkSettings* settings = gtk_settings_get_default();
  std::string notify_setting = "notify::" + setting_name;
  signal_id_ = g_signal_connect(settings, notify_setting.c_str(),
                                G_CALLBACK(OnSettingChangedThunk), this);
  DCHECK(signal_id_);
  OnSettingChanged(settings, nullptr);
}

SettingsProviderGtk::FrameActionSettingWatcher::~FrameActionSettingWatcher() {
  if (signal_id_) {
    g_signal_handler_disconnect(gtk_settings_get_default(), signal_id_);
  }
}

void SettingsProviderGtk::FrameActionSettingWatcher::OnSettingChanged(
    GtkSettings* settings,
    GParamSpec* param) {
  std::string value =
      GetGtkSettingsStringProperty(settings, setting_name_.c_str());
  GtkUi::WindowFrameAction action = default_action_;
  ParseActionString(value, &action);
  settings_provider_->delegate_->SetWindowFrameAction(action_type_, action);
}

SettingsProviderGtk::SettingsProviderGtk(GtkUi* delegate)
    : delegate_(delegate), signal_id_decoration_layout_(0) {
  DCHECK(delegate_);
  GtkSettings* settings = gtk_settings_get_default();
  signal_id_decoration_layout_ =
      g_signal_connect(settings, "notify::gtk-decoration-layout",
                       G_CALLBACK(OnDecorationButtonLayoutChangedThunk), this);
  DCHECK(signal_id_decoration_layout_);
  OnDecorationButtonLayoutChanged(settings, nullptr);

  frame_action_setting_watchers_.push_back(
      std::make_unique<FrameActionSettingWatcher>(
          this, "gtk-titlebar-middle-click",
          ui::LinuxUi::WindowFrameActionSource::kMiddleClick,
          ui::LinuxUi::WindowFrameAction::kNone));
  frame_action_setting_watchers_.push_back(
      std::make_unique<FrameActionSettingWatcher>(
          this, "gtk-titlebar-double-click",
          ui::LinuxUi::WindowFrameActionSource::kDoubleClick,
          ui::LinuxUi::WindowFrameAction::kToggleMaximize));
  frame_action_setting_watchers_.push_back(
      std::make_unique<FrameActionSettingWatcher>(
          this, "gtk-titlebar-right-click",
          ui::LinuxUi::WindowFrameActionSource::kRightClick,
          ui::LinuxUi::WindowFrameAction::kMenu));
}

SettingsProviderGtk::~SettingsProviderGtk() {
  if (signal_id_decoration_layout_) {
    g_signal_handler_disconnect(gtk_settings_get_default(),
                                signal_id_decoration_layout_);
  }
}

void SettingsProviderGtk::SetWindowButtonOrderingFromGtkLayout(
    const std::string& gtk_layout) {
  std::vector<views::FrameButton> leading_buttons;
  std::vector<views::FrameButton> trailing_buttons;
  ParseButtonLayout(gtk_layout, &leading_buttons, &trailing_buttons);
  delegate_->SetWindowButtonOrdering(leading_buttons, trailing_buttons);
}

void SettingsProviderGtk::OnDecorationButtonLayoutChanged(GtkSettings* settings,
                                                          GParamSpec* param) {
  SetWindowButtonOrderingFromGtkLayout(
      GetGtkSettingsStringProperty(settings, "gtk-decoration-layout"));
}

void SettingsProviderGtk::OnThemeChanged(GtkSettings* settings,
                                         GParamSpec* param) {
  std::string layout = GetDecorationLayoutFromGtkWindow();
  SetWindowButtonOrderingFromGtkLayout(layout);
}

}  // namespace gtk
