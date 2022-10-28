// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GTK_SETTINGS_PROVIDER_GTK_H_
#define UI_GTK_SETTINGS_PROVIDER_GTK_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/base/glib/glib_signal.h"
#include "ui/gtk/settings_provider.h"
#include "ui/linux/linux_ui.h"

typedef struct _GParamSpec GParamSpec;
typedef struct _GtkSettings GtkSettings;

namespace gtk {

class GtkUi;

class SettingsProviderGtk : public SettingsProvider {
 public:
  explicit SettingsProviderGtk(GtkUi* delegate);

  SettingsProviderGtk(const SettingsProviderGtk&) = delete;
  SettingsProviderGtk& operator=(const SettingsProviderGtk&) = delete;

  ~SettingsProviderGtk() override;

 private:
  class FrameActionSettingWatcher {
   public:
    FrameActionSettingWatcher(SettingsProviderGtk* settings_provider,
                              const std::string& setting_name,
                              ui::LinuxUi::WindowFrameActionSource action_type,
                              ui::LinuxUi::WindowFrameAction default_action);

    FrameActionSettingWatcher(const FrameActionSettingWatcher&) = delete;
    FrameActionSettingWatcher& operator=(const FrameActionSettingWatcher&) =
        delete;

    ~FrameActionSettingWatcher();

    CHROMEG_CALLBACK_1(FrameActionSettingWatcher,
                       void,
                       OnSettingChanged,
                       GtkSettings*,
                       GParamSpec*);

   private:
    raw_ptr<SettingsProviderGtk> settings_provider_;
    std::string setting_name_;
    ui::LinuxUi::WindowFrameActionSource action_type_;
    ui::LinuxUi::WindowFrameAction default_action_;
    unsigned long signal_id_;
  };

  void SetWindowButtonOrderingFromGtkLayout(const std::string& gtk_layout);

  CHROMEG_CALLBACK_1(SettingsProviderGtk,
                     void,
                     OnDecorationButtonLayoutChanged,
                     GtkSettings*,
                     GParamSpec*);

  CHROMEG_CALLBACK_1(SettingsProviderGtk,
                     void,
                     OnThemeChanged,
                     GtkSettings*,
                     GParamSpec*);

  raw_ptr<GtkUi> delegate_;

  unsigned long signal_id_decoration_layout_;

  std::vector<std::unique_ptr<FrameActionSettingWatcher>>
      frame_action_setting_watchers_;
};

}  // namespace gtk

#endif  // UI_GTK_SETTINGS_PROVIDER_GTK_H_
