// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/os_settings_provider.h"

#include <array>
#include <forward_list>
#include <utility>

#include "base/callback_list.h"
#include "base/dcheck_is_on.h"
#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/types/cxx23_to_underlying.h"
#include "build/build_config.h"

// `OsSettingsProviderImpl` is an alias to a forward-declared type; to construct
// it in `Get()` below, we must have the full type definition.
#if BUILDFLAG(IS_MAC)
#include "ui/native_theme/os_settings_provider_mac.h"
#elif BUILDFLAG(IS_WIN)
#include "base/win/win_util.h"
#include "ui/native_theme/os_settings_provider_win.h"
#endif

namespace ui {

namespace {

// Returns the global list of constructed `OsSettingsProvider`s. Each entry
// overrides subsequent ones.
std::forward_list<OsSettingsProvider*>& GetOsSettingsProviders(
    OsSettingsProvider::PriorityLevel priority_level) {
  // All `OsSettingsProvider`s must access on the same thread to avoid data
  // races on the lists below.
#if DCHECK_IS_ON()  // Guard to avoid assertion failure from `NoDestructor`.
  static base::NoDestructor<base::SequenceChecker> s_sequence_checker;
  DCHECK_CALLED_ON_VALID_SEQUENCE(*s_sequence_checker);
#endif

  static base::NoDestructor<std::array<
      std::forward_list<OsSettingsProvider*>,
      base::to_underlying(OsSettingsProvider::PriorityLevel::kLast) + 1>>
      s_providers;
  return (*s_providers)[base::to_underlying(priority_level)];
}

// Returns the global list of callbacks to notify on setting changes. This is
// not a non-static member of `OsSettingsProvider` since callers of
// `RegisterCallback` should be agnostic to changes in the active provider (e.g.
// when tests override it).
base::RepeatingClosureList* GetOsSettingsChangedCallbacks() {
  // All `OsSettingsProvider`s must access on the same thread to avoid data
  // races on the list below.
#if DCHECK_IS_ON()  // Guard to avoid assertion failure from `NoDestructor`.
  static base::NoDestructor<base::SequenceChecker> s_sequence_checker;
  DCHECK_CALLED_ON_VALID_SEQUENCE(*s_sequence_checker);
#endif

  static base::NoDestructor<base::RepeatingClosureList> s_callbacks;
  return s_callbacks.get();
}

}  // namespace

OsSettingsProvider::OsSettingsProvider(PriorityLevel priority_level)
    : priority_level_(priority_level) {
  GetOsSettingsProviders(priority_level_).push_front(this);
  NotifyOnSettingsChanged();
}

OsSettingsProvider::~OsSettingsProvider() {
  auto& providers = GetOsSettingsProviders(priority_level_);
  const bool was_active = providers.front() == this;
  providers.remove(this);

  // Switching from one provider to another is effectively a settings change. By
  // contrast, when the last provider is destroyed, we're in test code and
  // likely shutting down, so notifying is pointless at best and could trigger
  // strange behavior at worst.
  if (was_active && !providers.empty()) {
    NotifyOnSettingsChanged();
  }
}

// static
OsSettingsProvider& OsSettingsProvider::Get() {
  // Return any higher-than-production-priority providers first.
  for (auto i = PriorityLevel::kLast; i > PriorityLevel::kProduction;
       i = static_cast<PriorityLevel>(base::to_underlying(i) - 1)) {
    if (const auto& providers = GetOsSettingsProviders(i); !providers.empty()) {
      return *providers.front();
    }
  }

  // If there is no production provider, create one.
  const auto& providers = GetOsSettingsProviders(PriorityLevel::kProduction);
  if (providers.empty()) {
#if BUILDFLAG(IS_WIN)
    // `OsSettingsProviderWin` attempts calls to user32.dll, so avoid
    // instantiating it if those calls are not possible.
    if (!base::win::IsUser32AndGdi32Available()) {
      static base::NoDestructor<OsSettingsProvider>
          s_fallback_settings_provider(PriorityLevel::kProduction);
      return *s_fallback_settings_provider;
    }
#endif
    // Construct an `OsSettingsProviderImpl` by default. This is conditional so
    // that if e.g. a test constructs a provider before the first call to
    // `Get()`, that provider won't be overridden.
    static base::NoDestructor<OsSettingsProviderImpl> s_settings_provider;

    // Since the above provider is never destroyed, `providers` should never be
    // empty again, even if other providers are subsequently created/destroyed.
    CHECK(!providers.empty());
  }

  // The first item on the list is the most recently constructed.
  return *providers.front();
}

// static
base::CallbackListSubscription
OsSettingsProvider::RegisterOsSettingsChangedCallback(
    base::RepeatingClosure cb) {
  return GetOsSettingsChangedCallbacks()->Add(std::move(cb));
}

base::TimeDelta OsSettingsProvider::CaretBlinkInterval() const {
  return kDefaultCaretBlinkInterval;
}

void OsSettingsProvider::NotifyOnSettingsChanged() {
  // Don't notify if this provider isn't the active one.
  if (&Get() == this) {
    GetOsSettingsChangedCallbacks()->Notify();
  }
}

}  // namespace ui
