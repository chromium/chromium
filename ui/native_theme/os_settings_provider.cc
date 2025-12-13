// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/os_settings_provider.h"

#include <array>
#include <forward_list>
#include <optional>
#include <tuple>
#include <utility>

#include "base/callback_list.h"
#include "base/dcheck_is_on.h"
#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/types/cxx23_to_underlying.h"
#include "build/build_config.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_provider_key.h"
#include "ui/gfx/color_conversions.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/native_theme.h"

// `OsSettingsProviderImpl` is an alias to a forward-declared type; to construct
// it in `Get()` below, we must have the full type definition.
#if BUILDFLAG(IS_ANDROID)
#include "ui/native_theme/os_settings_provider_android.h"
#elif BUILDFLAG(IS_CHROMEOS)
#include "ui/native_theme/os_settings_provider_ash.h"
#elif BUILDFLAG(IS_MAC)
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
using CallbackList =
    base::RepeatingCallbackList<OsSettingsProvider::SettingsChangedCallbackT>;
CallbackList* GetOsSettingsChangedCallbacks() {
  // All `OsSettingsProvider`s must access on the same thread to avoid data
  // races on the list below.
#if DCHECK_IS_ON()  // Guard to avoid assertion failure from `NoDestructor`.
  static base::NoDestructor<base::SequenceChecker> s_sequence_checker;
  DCHECK_CALLED_ON_VALID_SEQUENCE(*s_sequence_checker);
#endif

  static base::NoDestructor<CallbackList> s_callbacks;
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
    base::RepeatingCallback<SettingsChangedCallbackT> cb) {
  return GetOsSettingsChangedCallbacks()->Add(std::move(cb));
}

bool OsSettingsProvider::DarkColorSchemeAvailable() const {
  return true;
}

NativeTheme::PreferredColorScheme OsSettingsProvider::PreferredColorScheme()
    const {
  if (ForcedColorsActive()) {
    // According to the spec, the preferred color scheme for web content is
    // "dark" if the Canvas color has L<33% and "light" if L>67%, where "L" is
    // LAB lightness. The Canvas color is mapped to the Window system color.
    // https://www.w3.org/TR/css-color-adjust-1/#forced
    if (const auto bg_color = Color(ColorId::kWindow)) {
      const SkColor srgb_legacy = bg_color.value();
      const auto [r, g, b] = gfx::SRGBLegacyToSRGB(SkColorGetR(srgb_legacy),
                                                   SkColorGetG(srgb_legacy),
                                                   SkColorGetB(srgb_legacy));
      const auto [x, y, z] = gfx::SRGBToXYZD50(r, g, b);
      const float lab_lightness = std::get<0>(gfx::XYZD50ToLab(x, y, z));
      if (lab_lightness < 33.0f) {
        return NativeTheme::PreferredColorScheme::kDark;
      }
      if (lab_lightness > 67.0f) {
        return NativeTheme::PreferredColorScheme::kLight;
      }
    }
  }

  return NativeTheme::PreferredColorScheme::kNoPreference;
}

ColorProviderKey::UserColorSource OsSettingsProvider::PreferredColorSource()
    const {
  return ColorProviderKey::UserColorSource::kAccent;
}

NativeTheme::PreferredContrast OsSettingsProvider::PreferredContrast() const {
  if (ForcedColorsActive()) {
    // TODO(sartang@microsoft.com): Update the spec page at
    // https://www.w3.org/TR/css-color-adjust-1/#forced, it currently does not
    // mention the relation between forced-colors-active and prefers-contrast.
    //
    // According to spec [1], "in addition to forced-colors: active, the user
    // agent must also match one of prefers-contrast: more or prefers-contrast:
    // less if it can determine that the forced color palette chosen by the user
    // has a particularly high or low contrast, and must make prefers-contrast:
    // custom match otherwise".
    //
    // Using WCAG definitions [2], we have decided to match 'more' in Forced
    // Colors Mode if the contrast ratio between the foreground and background
    // color is 7:1 or greater.
    //
    // "A contrast ratio of 3:1 is the minimum level recommended by
    // [[ISO-9241-3]] and [[ANSI-HFES-100-1988]] for standard text and
    // vision"[2]. Given this, we will start by matching to 'less' in Forced
    // Colors Mode if the contrast ratio between the foreground and background
    // color is 2.5:1 or less.
    //
    // These ratios will act as an experimental baseline that we can adjust
    // based on user feedback.
    //
    // [1]
    // https://drafts.csswg.org/mediaqueries-5/#valdef-media-forced-colors-active
    // [2] https://www.w3.org/WAI/WCAG21/Understanding/contrast-enhanced
    if (const auto bg_color = Color(ColorId::kWindow),
        fg_color = Color(ColorId::kWindowText);
        bg_color.has_value() && fg_color.has_value()) {
      const float contrast_ratio =
          color_utils::GetContrastRatio(bg_color.value(), fg_color.value());
      if (contrast_ratio >= 7) {
        return NativeTheme::PreferredContrast::kMore;
      }
      return contrast_ratio <= 2.5 ? NativeTheme::PreferredContrast::kLess
                                   : NativeTheme::PreferredContrast::kCustom;
    }
  }

  return NativeTheme::PreferredContrast::kNoPreference;
}

bool OsSettingsProvider::PrefersReducedTransparency() const {
  return false;
}

bool OsSettingsProvider::PrefersInvertedColors() const {
  return false;
}

bool OsSettingsProvider::ForcedColorsActive() const {
  return false;
}

std::optional<SkColor> OsSettingsProvider::AccentColor() const {
  return std::nullopt;
}

std::optional<SkColor> OsSettingsProvider::Color(ColorId color_id) const {
  return std::nullopt;
}

std::optional<ColorProviderKey::SchemeVariant>
OsSettingsProvider::SchemeVariant() const {
  return std::nullopt;
}

base::TimeDelta OsSettingsProvider::CaretBlinkInterval() const {
  return kDefaultCaretBlinkInterval;
}

void OsSettingsProvider::NotifyOnSettingsChanged(bool force_notify) {
  // Don't notify if this provider isn't the active one.
  if (&Get() == this) {
    GetOsSettingsChangedCallbacks()->Notify(force_notify);
  }
}

}  // namespace ui
