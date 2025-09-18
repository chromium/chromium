// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme.h"

#include <stddef.h>

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>

#include "base/callback_list.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/ui_base_switches.h"
#include "ui/color/color_id.h"
#include "ui/color/color_metrics.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_provider_manager.h"
#include "ui/color/system_theme.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/native_theme/native_theme_observer.h"
#include "ui/native_theme/os_settings_provider.h"

#if BUILDFLAG(IS_MAC)
#include "ui/native_theme/native_theme_aura.h"
#include "ui/native_theme/native_theme_mac.h"
#elif defined(USE_AURA)
#include "ui/native_theme/features/native_theme_features.h"
#include "ui/native_theme/native_theme_aura.h"
#include "ui/native_theme/native_theme_fluent.h"
#if BUILDFLAG(IS_WIN)
#include "ui/native_theme/native_theme_win.h"
#endif
#else
#include "ui/native_theme/native_theme_mobile.h"
#endif

namespace ui {

namespace {

#if BUILDFLAG(IS_MAC)
using NativeUiTheme = NativeThemeMac;
using WebUiTheme = NativeThemeAura;
#elif defined(USE_AURA)
#if BUILDFLAG(IS_WIN)
using NativeUiTheme = NativeThemeWin;
#else
using NativeUiTheme = NativeThemeAura;
#endif
NativeTheme* GetInstanceForWebImpl() {
  static const bool use_fluent = IsFluentScrollbarEnabled();
  if (use_fluent) {
    static base::NoDestructor<NativeThemeFluent> s_web_theme;
    return s_web_theme.get();
  }
  static base::NoDestructor<NativeThemeAura> s_web_theme(
#if BUILDFLAG(IS_CHROMEOS)
      true
#else
      IsOverlayScrollbarEnabledByFeatureFlag()
#endif
  );
  return s_web_theme.get();
}
#else
using NativeUiTheme = NativeThemeMobile;
using WebUiTheme = NativeThemeMobile;
#endif

}  // namespace

NativeTheme::MenuListExtraParams::MenuListExtraParams() = default;

NativeTheme::MenuListExtraParams::MenuListExtraParams(
    const NativeTheme::MenuListExtraParams&) = default;

NativeTheme::MenuListExtraParams& NativeTheme::MenuListExtraParams::operator=(
    const NativeTheme::MenuListExtraParams&) = default;

NativeTheme::TextFieldExtraParams::TextFieldExtraParams() = default;

NativeTheme::TextFieldExtraParams::TextFieldExtraParams(
    const NativeTheme::TextFieldExtraParams&) = default;

NativeTheme::TextFieldExtraParams& NativeTheme::TextFieldExtraParams::operator=(
    const NativeTheme::TextFieldExtraParams&) = default;

NativeTheme::UpdateNotificationDelayScoper::UpdateNotificationDelayScoper() {
  ++num_instances_;
}

NativeTheme::UpdateNotificationDelayScoper::UpdateNotificationDelayScoper(
    const UpdateNotificationDelayScoper&) {
  ++num_instances_;
}

NativeTheme::UpdateNotificationDelayScoper::UpdateNotificationDelayScoper(
    UpdateNotificationDelayScoper&&) {
  ++num_instances_;
}

NativeTheme::UpdateNotificationDelayScoper::~UpdateNotificationDelayScoper() {
  if (--num_instances_ == 0) {
    GetDelayedNotifications().Notify();
  }
}

// static
base::CallbackListSubscription
NativeTheme::UpdateNotificationDelayScoper::RegisterCallback(
    base::PassKey<NativeTheme>,
    base::OnceClosure cb) {
  return GetDelayedNotifications().Add(std::move(cb));
}

// static
base::OnceClosureList&
NativeTheme::UpdateNotificationDelayScoper::GetDelayedNotifications() {
  static base::NoDestructor<base::OnceClosureList> s_delayed_notifications;
  return *s_delayed_notifications;
}

// static
size_t NativeTheme::UpdateNotificationDelayScoper::num_instances_ = 0;

// static
NativeTheme* NativeTheme::GetInstanceForNativeUi() {
  static base::NoDestructor<NativeUiTheme> s_native_theme;
  static bool initialized = false;
  if (!initialized) {
    s_native_theme->BeginObservingOsSettingChanges();
    initialized = true;
  }
  return s_native_theme.get();
}

// static
NativeTheme* NativeTheme::GetInstanceForWeb() {
#if defined(USE_AURA)
  NativeTheme* const native_theme = GetInstanceForWebImpl();
#else
  static base::NoDestructor<WebUiTheme> s_web_theme;
  NativeTheme* const native_theme = s_web_theme.get();
#endif
  static bool initialized = false;
  if (!initialized) {
    GetInstanceForNativeUi()->SetAssociatedWebInstance(native_theme);
    initialized = true;
  }
  return native_theme;
}

// static
float NativeTheme::AdjustBorderWidthByZoom(float border_width,
                                           float zoom_level) {
  return std::max(std::floor(border_width * zoom_level), 1.0f);
}

// static
float NativeTheme::AdjustBorderRadiusByZoom(Part part,
                                            float border_radius,
                                            float zoom) {
  return (part == kCheckbox || part == kTextField || part == kPushButton)
             ? AdjustBorderWidthByZoom(border_radius, zoom)
             : border_radius;
}

gfx::Size NativeTheme::GetPartSize(Part part,
                                   State state,
                                   const ExtraParams& extra_params) const {
  return {};
}

int NativeTheme::GetPaintedScrollbarTrackInset() const {
  return 0;
}

gfx::Insets NativeTheme::GetScrollbarSolidColorThumbInsets(Part part) const {
  return {};
}

float NativeTheme::GetBorderRadiusForPart(Part part,
                                          float width,
                                          float height) const {
  return 0;
}

bool NativeTheme::SupportsNinePatch(Part part) const {
  return false;
}

gfx::Size NativeTheme::GetNinePatchCanvasSize(Part part) const {
  NOTREACHED();
}

gfx::Rect NativeTheme::GetNinePatchAperture(Part part) const {
  NOTREACHED();
}

SkColor NativeTheme::GetScrollbarThumbColor(
    const ColorProvider* color_provider,
    State state,
    const ScrollbarThumbExtraParams& extra_params) const {
  NOTREACHED();
}

SkColor NativeTheme::GetSystemButtonPressedColor(SkColor base_color) const {
  return base_color;
}

void NativeTheme::BeginObservingOsSettingChanges() {
  os_settings_changed_subscription_ =
      OsSettingsProvider::RegisterOsSettingsChangedCallback(base::BindRepeating(
          &NativeTheme::OnToolkitSettingsChanged, base::Unretained(this)));
  UpdateVariablesForToolkitSettings();
}

void NativeTheme::AddObserver(NativeThemeObserver* observer) {
  native_theme_observers_.AddObserver(observer);
}

void NativeTheme::RemoveObserver(NativeThemeObserver* observer) {
  native_theme_observers_.RemoveObserver(observer);
}

void NativeTheme::NotifyOnNativeThemeUpdated() {
  // This specific method is prone to being mistakenly called on the wrong
  // sequence, because it is often invoked from a platform-specific event
  // listener, and those events may be delivered on unexpected sequences.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (base::PassKey<NativeTheme> pass_key;
      UpdateNotificationDelayScoper::exists(pass_key)) {
    // At least one scoper exists, so delay notifications until it's gone.
    if (!update_delay_subscription_) {
      update_delay_subscription_ =
          UpdateNotificationDelayScoper::RegisterCallback(
              pass_key, base::BindOnce(&NativeTheme::NotifyOnNativeThemeUpdated,
                                       base::Unretained(this)));
    }
    return;
  }

  if (update_delay_subscription_) {
    // No scopers exist, but a subscription does: this is the callback from the
    // last scoper being destroyed. Reset the subscription so it can be
    // recreated in the future when necessary.
    update_delay_subscription_ = {};
  }

  base::ElapsedTimer timer;
  auto& color_provider_manager = ColorProviderManager::Get();
  const size_t initial_providers_initialized =
      color_provider_manager.num_providers_initialized();

  // Reset the ColorProviderManager's cache so that ColorProviders requested
  // from this point onwards incorporate the changes to the system theme.
  color_provider_manager.ResetColorProviderCache();

  NotifyOnNativeThemeUpdatedImpl();

  RecordNumColorProvidersInitializedDuringOnNativeThemeUpdated(
      color_provider_manager.num_providers_initialized() -
      initial_providers_initialized);
  RecordTimeSpentProcessingOnNativeThemeUpdatedEvent(timer.Elapsed());
}

void NativeTheme::NotifyOnCaptionStyleUpdated() {
  // This specific method is prone to being mistakenly called on the wrong
  // sequence, because it is often invoked from a platform-specific event
  // listener, and those events may be delivered on unexpected sequences.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  native_theme_observers_.Notify(&NativeThemeObserver::OnCaptionStyleUpdated);
}

void NativeTheme::Paint(cc::PaintCanvas* canvas,
                        const ColorProvider* color_provider,
                        Part part,
                        State state,
                        const gfx::Rect& rect,
                        const ExtraParams& extra_params,
                        bool forced_colors,
                        PreferredColorScheme color_scheme,
                        PreferredContrast contrast,
                        std::optional<SkColor> accent_color) const {
  if (rect.IsEmpty()) {
    return;
  }

  // For `color_scheme`, `kNoPreference` means "use current".
  const bool dark_mode =
      color_scheme == PreferredColorScheme::kDark ||
      (color_scheme == PreferredColorScheme::kNoPreference &&
       preferred_color_scheme() == PreferredColorScheme::kDark);

  // Form control accents shouldn't be drawn with any transparency.
  // TODO(C++23): Replace the below with:
  // ```
  //  const std::optional<SkColor> accent_color_opaque = accent_color.transform(
  //      [](SkColor c) { return SkColorSetA(c, SK_AlphaOPAQUE); });
  // ```
  const std::optional<SkColor> accent_color_opaque =
      accent_color.has_value() ? std::make_optional(SkColorSetA(
                                     accent_color.value(), SK_AlphaOPAQUE))
                               : std::nullopt;

  gfx::Canvas gfx_canvas(canvas, 1.0f);
  gfx::ScopedCanvas scoped_canvas(&gfx_canvas);
  gfx_canvas.ClipRect(rect);

  PaintImpl(canvas, color_provider, part, state, rect, extra_params,
            forced_colors, dark_mode, contrast, accent_color_opaque);
}

ColorProviderKey NativeTheme::GetColorProviderKey(
    scoped_refptr<ColorProviderKey::ThemeInitializerSupplier> custom_theme,
    bool use_custom_frame) const {
  ColorProviderKey key;
  key.color_mode = preferred_color_scheme() == PreferredColorScheme::kDark
                       ? ColorProviderKey::ColorMode::kDark
                       : ColorProviderKey::ColorMode::kLight;
  key.contrast_mode = preferred_contrast() == PreferredContrast::kMore
                          ? ColorProviderKey::ContrastMode::kHigh
                          : ColorProviderKey::ContrastMode::kNormal;
  key.forced_colors = forced_colors();
  key.system_theme = system_theme();
  key.frame_type = use_custom_frame ? ColorProviderKey::FrameType::kChromium
                                    : ColorProviderKey::FrameType::kNative;
  key.user_color_source = preferred_color_source_;
  key.user_color = user_color();
  key.scheme_variant = scheme_variant();
  key.custom_theme = std::move(custom_theme);
  return key;
}

NativeTheme::NativeTheme(SystemTheme system_theme)
    : system_theme_(system_theme) {}

NativeTheme::~NativeTheme() = default;

bool NativeTheme::IsForcedDarkMode() {
  static bool kIsForcedDarkMode =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceDarkMode);
  return kIsForcedDarkMode;
}

bool NativeTheme::IsForcedHighContrast() {
  static bool kIsForcedHighContrast =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceHighContrast);
  return kIsForcedHighContrast;
}

void NativeTheme::PaintMenuItemBackground(
    cc::PaintCanvas* canvas,
    const ColorProvider* color_provider,
    State state,
    const gfx::Rect& rect,
    const MenuItemExtraParams& extra_params) const {
  const SkScalar radius = SkIntToScalar(extra_params.corner_radius);
  cc::PaintFlags flags;
  const ColorId id = (state == kHovered)
#if BUILDFLAG(IS_CHROMEOS)
                         ? kColorAshSystemUIMenuItemBackgroundSelected
                         : kColorAshSystemUIMenuBackground;
#else
                         ? kColorMenuItemBackgroundSelected
                         : kColorMenuBackground;
#endif
  flags.setColor(color_provider->GetColor(id));
  canvas->drawRoundRect(gfx::RectToSkRect(rect), radius, radius, flags);
}

void NativeTheme::OnToolkitSettingsChanged(bool force_notify) {
  if (UpdateVariablesForToolkitSettings() || force_notify) {
    NotifyOnNativeThemeUpdated();
  }
}

void NativeTheme::SetAssociatedWebInstance(
    NativeTheme* associated_web_instance) {
  if (associated_web_instance_ != associated_web_instance) {
    associated_web_instance_ = associated_web_instance;
    if (UpdateWebInstance()) {
      associated_web_instance_->NotifyOnNativeThemeUpdatedImpl();
    }
  }
}

bool NativeTheme::UpdateWebInstance() const {
  if (!associated_web_instance_) {
    return false;
  }

  // NOTE: Intentionally does not copy the native "overlay scrollbar" setting to
  // the web instance, as the web instance often wants to differ there.
  // TODO(crbug.com/444399080): If we had a notion somewhere about "web wants
  // overlay scrollbars even when native doesn't", we could probably copy the
  // setting fearlessly here (and have that override it on the web instance
  // side), making callers who want to toggle overlay scrollbars on/off globally
  // simpler and safer.

  // TODO(pkasting): The code duplication between this function and
  // `UpdateVariablesForToolkitSettings()` is error-prone; e.g. it's easy to
  // forget to update the web instance properly when adding a new member.
  // Refactor to a settings struct or similar.

  bool updated_web_instance = false;
  if (associated_web_instance_->forced_colors() != forced_colors()) {
    associated_web_instance_->forced_colors_ = forced_colors();
    updated_web_instance = true;
  }
  if (associated_web_instance_->preferred_color_scheme() !=
      preferred_color_scheme()) {
    associated_web_instance_->preferred_color_scheme_ =
        preferred_color_scheme();
    updated_web_instance = true;
  }
  if (associated_web_instance_->preferred_contrast() != preferred_contrast()) {
    associated_web_instance_->preferred_contrast_ = preferred_contrast();
    updated_web_instance = true;
  }
  if (associated_web_instance_->prefers_reduced_transparency() !=
      prefers_reduced_transparency()) {
    associated_web_instance_->prefers_reduced_transparency_ =
        prefers_reduced_transparency();
    updated_web_instance = true;
  }
  if (associated_web_instance_->inverted_colors() != inverted_colors()) {
    associated_web_instance_->inverted_colors_ = inverted_colors();
    updated_web_instance = true;
  }
  if (associated_web_instance_->user_color() != user_color()) {
    associated_web_instance_->user_color_ = user_color();
    updated_web_instance = true;
  }
  if (associated_web_instance_->scheme_variant() != scheme_variant()) {
    associated_web_instance_->scheme_variant_ = scheme_variant();
    updated_web_instance = true;
  }
  if (associated_web_instance_->preferred_color_source_ !=
      preferred_color_source_) {
    associated_web_instance_->preferred_color_source_ = preferred_color_source_;
    updated_web_instance = true;
  }
  if (associated_web_instance_->caret_blink_interval() !=
      caret_blink_interval()) {
    associated_web_instance_->caret_blink_interval_ = caret_blink_interval();
    updated_web_instance = true;
  }
  return updated_web_instance;
}

void NativeTheme::NotifyOnNativeThemeUpdatedImpl() {
  // Update any associated web instance's settings before notifying observers,
  // since those observers may attempt to override the web instance's settings
  // (e.g. to implement web-content-specific forced colors).
  const bool updated_web_instance = UpdateWebInstance();

  native_theme_observers_.Notify(&NativeThemeObserver::OnNativeThemeUpdated,
                                 this);

  // If the web instance was modified above, also notify its observers. This is
  // done last so any of our observers that modify the web instance will have
  // already run.
  //
  // NOTE: If any above observers already called `NotifyOnNativeThemeUpdated()`
  // on the web theme, this is unnecessary jank; however, it's not worth the
  // hassle to try to detect this.
  //
  // TODO(pkasting): Adding a scoping object to batch updates would address
  // this; see comments in header above accessors.
  if (updated_web_instance) {
    // Calling `NotifyOnNativeThemeUpdated()` here would unnecessarily churn the
    // color provider cache.
    associated_web_instance_->NotifyOnNativeThemeUpdatedImpl();
  }
}

bool NativeTheme::UpdateVariablesForToolkitSettings() {
  // This should not be called except in an instance that is monitoring OS
  // setting changes. Otherwise, either:
  //   * This is the associated web instance of another instance, and that
  //     instance will update our members via `UpdateWebInstance()`, so updating
  //     them here is both wasteful and potentially incorrect
  //   * Or, whoever created this instance didn't call
  //     `BeginObservingOsSettingChanges()` and should have
  // Getting this right is important, because calling
  // `OsSettingsProvider::Get()` in the renderer may not return the expected
  // instance (see comments on that function), so we shouldn't be introducing
  // new calls to it carelessly.
  CHECK(os_settings_changed_subscription_);

  // Calculate updated values.
  const auto& os_settings_provider = OsSettingsProvider::Get();
  const auto new_forced_colors = CalculateForcedColors();
  const auto new_preferred_color_scheme = CalculatePreferredColorScheme();
  const auto new_preferred_contrast = CalculatePreferredContrast();
  const auto new_prefers_reduced_transparency =
      os_settings_provider.PrefersReducedTransparency();
  const auto new_inverted_colors = os_settings_provider.PrefersInvertedColors();
  const auto new_user_color = os_settings_provider.AccentColor();
  const auto new_scheme_variant = os_settings_provider.SchemeVariant();
  const auto new_preferred_color_source =
      os_settings_provider.PreferredColorSource();
  const auto new_caret_blink_interval =
      os_settings_provider.CaretBlinkInterval();

  // Set updated values and see if anything changed.
  bool updated = false;
  if (forced_colors() != new_forced_colors) {
    forced_colors_ = new_forced_colors;
    updated = true;
  }
  if (preferred_color_scheme() != new_preferred_color_scheme) {
    preferred_color_scheme_ = new_preferred_color_scheme;
    updated = true;
  }
  if (preferred_contrast() != new_preferred_contrast) {
    preferred_contrast_ = new_preferred_contrast;
    updated = true;
  }
  if (prefers_reduced_transparency() != new_prefers_reduced_transparency) {
    prefers_reduced_transparency_ = new_prefers_reduced_transparency;
    updated = true;
  }
  if (inverted_colors() != new_inverted_colors) {
    inverted_colors_ = new_inverted_colors;
    updated = true;
  }
  if (user_color() != new_user_color) {
    user_color_ = new_user_color;
    updated = true;
  }
  if (scheme_variant() != new_scheme_variant) {
    scheme_variant_ = new_scheme_variant;
    updated = true;
  }
  if (preferred_color_source_ != new_preferred_color_source) {
    preferred_color_source_ = new_preferred_color_source;
    updated = true;
  }
  if (caret_blink_interval() != new_caret_blink_interval) {
    caret_blink_interval_ = new_caret_blink_interval;
    updated = true;
  }

  return updated;
}

ColorProviderKey::ForcedColors NativeTheme::CalculateForcedColors() const {
  return (IsForcedHighContrast() ||
          OsSettingsProvider::Get().ForcedColorsActive())
             ? ColorProviderKey::ForcedColors::kSystem
             : ColorProviderKey::ForcedColors::kNone;
}

NativeTheme::PreferredColorScheme NativeTheme::CalculatePreferredColorScheme()
    const {
  return IsForcedDarkMode() ? PreferredColorScheme::kDark
                            : OsSettingsProvider::Get().PreferredColorScheme();
}

NativeTheme::PreferredContrast NativeTheme::CalculatePreferredContrast() const {
  return IsForcedHighContrast() ? PreferredContrast::kMore
                                : OsSettingsProvider::Get().PreferredContrast();
}

}  // namespace ui
