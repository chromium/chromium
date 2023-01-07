// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_WIN_POWER_SETTING_CHANGE_LISTENER_H_
#define UI_BASE_WIN_POWER_SETTING_CHANGE_LISTENER_H_

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"

namespace ui {

// Interface for classes that want to listen to power setting changes.
class COMPONENT_EXPORT(UI_BASE) PowerSettingChangeListener {
 public:
  virtual void OnDisplayStateChanged(bool display_on) = 0;
  virtual void OnResume() = 0;
  virtual void OnSuspend() = 0;

 protected:
  virtual ~PowerSettingChangeListener() = default;
};

// Create an instance of this class in any object that wants to listen
// for power setting changes.
class COMPONENT_EXPORT(UI_BASE) ScopedPowerSettingChangeListener {
 public:
  explicit ScopedPowerSettingChangeListener(
      PowerSettingChangeListener* listener);
  ~ScopedPowerSettingChangeListener();

 private:
  raw_ptr<PowerSettingChangeListener> listener_;

  ScopedPowerSettingChangeListener(const ScopedPowerSettingChangeListener&) =
      delete;
  const ScopedPowerSettingChangeListener& operator=(
      const ScopedPowerSettingChangeListener&) = delete;
};

}  // namespace ui

#endif  // UI_BASE_WIN_POWER_SETTING_CHANGE_LISTENER_H_
