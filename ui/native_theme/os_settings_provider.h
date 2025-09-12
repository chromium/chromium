// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_OS_SETTINGS_PROVIDER_H_
#define UI_NATIVE_THEME_OS_SETTINGS_PROVIDER_H_

#include <optional>

#include "base/callback_list.h"
#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "build/build_config.h"

namespace ui {

#if BUILDFLAG(IS_MAC)
class OsSettingsProviderMac;
using OsSettingsProviderImpl = OsSettingsProviderMac;
#elif BUILDFLAG(IS_WIN)
class OsSettingsProviderWin;
using OsSettingsProviderImpl = OsSettingsProviderWin;
#else
class OsSettingsProvider;
using OsSettingsProviderImpl = OsSettingsProvider;
#endif

// A singleton used to query the operating system for theme-related settings.
// Callers should use `Get()` to obtain the current instance.
class COMPONENT_EXPORT(NATIVE_THEME) OsSettingsProvider {
 public:
  // Higher-numbered (i.e. higher-priority) providers override lower-priority
  // ones. Within a priority class, the most-recently-created provider wins.
  enum class PriorityLevel { kProduction = 0, kTesting, kLast = kTesting };

  // The caret blink interval reported when the OS provides no value.
  static constexpr auto kDefaultCaretBlinkInterval = base::Milliseconds(500);

  // Creating a provider sets it as the object to be returned by `Get()`.
  //
  // NOTE: If you want to control behavior in tests, you probably want to use
  // `MockOsSettingsProvider` instead of instantiating this or some other
  // subclass.
  explicit OsSettingsProvider(
      PriorityLevel priority_level = PriorityLevel::kProduction);

  OsSettingsProvider(const OsSettingsProvider&) = delete;
  OsSettingsProvider& operator=(const OsSettingsProvider&) = delete;

  // Unhooks this provider from the list of providers checked by `Get()`.
  // Typically, this provider was the currently-active one, which means the
  // previous provider will become active.
  virtual ~OsSettingsProvider();

  // Returns the most recently-constructed `OsSettingsProvider` that has not
  // been destroyed. If there is no such provider, first creates an instance of
  // `OsSettingsProviderImpl`.
  //
  // CAUTION: On Windows, this will not create an `OsSettingsProviderWin`
  // outside the browser process, since it won't work elsewhere (due to sandbox)
  // and shouldn't be needed anyway. It will still return a non-null pointer,
  // but the default instance will be the base `OsSettingsProvider`, not the
  // platform-specific one.
  static OsSettingsProvider& Get();

  // Registers a callback to be invoked when the OS settings change. The
  // `force_notify` argument will be true if settings may have changed in a way
  // that does not affect the return values of the functions below, but
  // nevertheless should trigger theme updates. For example, a change that does
  // not affect the default `ColorProviderKey`, but affects the values of colors
  // inside that key's provider, would set this to `true`.
  static base::CallbackListSubscription RegisterOsSettingsChangedCallback(
      base::RepeatingClosure cb);

  // Returns the interval between caret blinks. If this is zero, the caret will
  // not blink.
  virtual base::TimeDelta CaretBlinkInterval() const;

 protected:
  // Invokes all registered callbacks.
  void NotifyOnSettingsChanged();

 private:
  PriorityLevel priority_level_;
};

}  // namespace ui

#endif  // UI_NATIVE_THEME_OS_SETTINGS_PROVIDER_H_
