// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/caption_style.h"

#include <Windows.Media.ClosedCaptioning.h>
#include <wrl/client.h>

#include <string>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/trace_event/trace_event.h"
#include "base/win/core_winrt_util.h"
#include "base/win/registry.h"
#include "skia/ext/skia_utils_win.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/caption_style_win.h"

namespace CC = ABI::Windows::Media::ClosedCaptioning;

namespace ui {

namespace {

// Adds !important to all captions styles. They should always override any
// styles added by the video author or by a user stylesheet. This is because on
// Windows, there is an option to turn off captions styles, so any time the
// captions are on, the styles should take priority.
std::string AddCSSImportant(const std::string& css_string) {
  return css_string + " !important";
}

// The GUID for the Windows "Default" closed caption theme. When this theme is
// selected, the user has not intentionally customized their caption style, so
// we should not add !important to the CSS values. This allows HTML author
// styles to override the default caption appearance via the ::cue
// pseudo-element per the WebVTT specification.
constexpr wchar_t kDefaultCaptionThemeGuid[] =
    L"{642F4BD2-475F-4802-9B13-95261896CB1C}";

// Checks whether the currently selected Windows closed caption theme is the
// built-in "Default" theme. The Windows ClosedCaptionProperties API does not
// expose theme identity, so we read the CurrentSelectedTheme registry value.
// When the Default theme is active, its individual property values (e.g.
// FontColor=White, BackgroundColor=Black) are non-Default enum values, but they
// represent the platform defaults rather than intentional user customization.
bool IsDefaultCaptionTheme() {
  base::win::RegKey key;
  if (key.Open(
          HKEY_CURRENT_USER,
          L"Software\\Microsoft\\Windows\\CurrentVersion\\ClosedCaptioning",
          KEY_READ) != ERROR_SUCCESS) {
    // If the key doesn't exist, captions are uninitialized — treat as default.
    return true;
  }

  std::wstring theme_guid;
  if (key.ReadValue(L"CurrentSelectedTheme", &theme_guid) != ERROR_SUCCESS) {
    // No theme value means uninitialized — treat as default.
    return true;
  }

  return theme_guid == kDefaultCaptionThemeGuid;
}

// Translates a Windows::Media::ClosedCaptioning::ClosedCaptionStyle to a
// CSS FontFamily value.
// These fonts were chosen to satisfy the characteristics represented by values
// of ClosedCaptionStyle Enum in Windows Settings.
void GetFontFamilyString(CC::ClosedCaptionStyle closed_caption_style,
                         std::string* css_font_family,
                         std::string* css_font_variant) {
  *css_font_variant = "normal";
  switch (closed_caption_style) {
    case CC::ClosedCaptionStyle_MonospacedWithSerifs:
      *css_font_family = "Courier New";
      break;
    case CC::ClosedCaptionStyle_ProportionalWithSerifs:
      *css_font_family = "Times New Roman";
      break;
    case CC::ClosedCaptionStyle_MonospacedWithoutSerifs:
      *css_font_family = "Consolas";
      break;
    case CC::ClosedCaptionStyle_ProportionalWithoutSerifs:
      *css_font_family = "Tahoma";
      break;
    case CC::ClosedCaptionStyle_Casual:
      *css_font_family = "Segoe Print";
      break;
    case CC::ClosedCaptionStyle_Cursive:
      *css_font_family = "Segoe Script";
      break;
    case CC::ClosedCaptionStyle_SmallCapitals:
      *css_font_family = "Tahoma";
      *css_font_variant = "small-caps";
      break;
    case CC::ClosedCaptionStyle_Default:
      // We shouldn't override with OS Styling for Default case.
      NOTREACHED();
  }
}

// Translates a Windows::Media::ClosedCaptioning::ClosedCaptionEdgeEffect to a
// CSS style value.
std::string GetEdgeEffectString(CC::ClosedCaptionEdgeEffect edge_effect) {
  switch (edge_effect) {
    case CC::ClosedCaptionEdgeEffect_None:
      return "none";
    case CC::ClosedCaptionEdgeEffect_Raised:
      return "-1px 0px 0px silver, 0px -1px 0px silver, 1px 1px 0px black, 2px "
             "2px 0px black, 3px 3px 0px black";
    case CC::ClosedCaptionEdgeEffect_Depressed:
      return "1px 1px 0px silver, 0px 1px 0px silver, -1px -1px 0px black, "
             "-1px "
             "0px 0px black";
    case CC::ClosedCaptionEdgeEffect_Uniform:
      return "0px 0px 4px black, 0px 0px 4px black, 0px 0px 4px black, 0px 0px "
             "4px black";
    case CC::ClosedCaptionEdgeEffect_DropShadow:
      return "0px 0px 2px rgba(0, 0, 0, 0.5), 2px 2px 2px black";
    case CC::ClosedCaptionEdgeEffect_Default:
      // We shouldn't override with OS Styling for Default case.
      NOTREACHED();
  }
}

// Translates a Windows::Media::ClosedCaptioning::ClosedCaptionSize to a CSS
// style value.
std::string GetCaptionSizeString(CC::ClosedCaptionSize caption_size) {
  switch (caption_size) {
    case CC::ClosedCaptionSize_FiftyPercent:
      return "50%";
    case CC::ClosedCaptionSize_OneHundredPercent:
      return "100%";
    case CC::ClosedCaptionSize_OneHundredFiftyPercent:
      return "150%";
    case CC::ClosedCaptionSize_TwoHundredPercent:
      return "200%";
    case CC::ClosedCaptionSize_Default:
      // We shouldn't override with OS Styling for Default case.
      NOTREACHED();
  }
}

// Translates a Windows::Media::ClosedCaptioning::ClosedCaptionOpacity to an
// SkAlpha value.
SkAlpha GetCaptionOpacity(CC::ClosedCaptionOpacity caption_opacity) {
  switch (caption_opacity) {
    case CC::ClosedCaptionOpacity_ZeroPercent:
      return SK_AlphaTRANSPARENT;
    case CC::ClosedCaptionOpacity_TwentyFivePercent:
      return base::ClampRound<SkAlpha>(SK_AlphaOPAQUE * 0.25);
    case CC::ClosedCaptionOpacity_SeventyFivePercent:
      return base::ClampRound<SkAlpha>(SK_AlphaOPAQUE * 0.75);
    case CC::ClosedCaptionOpacity_OneHundredPercent:
    case CC::ClosedCaptionOpacity_Default:
    default:
      return SK_AlphaOPAQUE;
  }
}

// Translates a Windows::Media::ClosedCaptioning::ClosedCaptionColor to an
// SkColor value.
SkColor GetCaptionColor(CC::ClosedCaptionColor caption_color) {
  switch (caption_color) {
    case CC::ClosedCaptionColor_Black:
      return SK_ColorBLACK;
    case CC::ClosedCaptionColor_Red:
      return SK_ColorRED;
    case CC::ClosedCaptionColor_Green:
      return SK_ColorGREEN;
    case CC::ClosedCaptionColor_Blue:
      return SK_ColorBLUE;
    case CC::ClosedCaptionColor_Yellow:
      return SK_ColorYELLOW;
    case CC::ClosedCaptionColor_Magenta:
      return SK_ColorMAGENTA;
    case CC::ClosedCaptionColor_Cyan:
      return SK_ColorCYAN;
    case CC::ClosedCaptionColor_White:
      return SK_ColorWHITE;
    case CC::ClosedCaptionColor_Default:
    default:
      // We shouldn't override with OS Styling for Default case.
      NOTREACHED();
  }
}

// Translates a Windows::Media::ClosedCaptioning::ClosedCaptionColor and a
// Windows::Media::ClosedCaptioning::ClosedCaptionOpacity to an RGBA CSS color
// string.
std::string GetCssColorWithAlpha(CC::ClosedCaptionColor caption_color,
                                 CC::ClosedCaptionOpacity caption_opacity) {
  const SkAlpha opacity = GetCaptionOpacity(caption_opacity);
  const SkColor color = GetCaptionColor(caption_color);
  return color_utils::SkColorToRgbaString(SkColorSetA(color, opacity));
}

std::optional<CaptionStyle> InitializeFromSystemSettings() {
  TRACE_EVENT0("ui", "InitializeFromSystemSettings");

  base::win::ScopedHString closed_caption_properties_string =
      base::win::ScopedHString::Create(
          RuntimeClass_Windows_Media_ClosedCaptioning_ClosedCaptionProperties);
  Microsoft::WRL::ComPtr<CC::IClosedCaptionPropertiesStatics>
      closed_caption_properties_statics;
  HRESULT hr = base::win::RoGetActivationFactory(
      closed_caption_properties_string.get(),
      IID_PPV_ARGS(&closed_caption_properties_statics));
  if (FAILED(hr)) {
    return std::nullopt;
  }

  CC::ClosedCaptionSize font_size = CC::ClosedCaptionSize_Default;
  hr = closed_caption_properties_statics->get_FontSize(&font_size);
  if (FAILED(hr)) {
    return std::nullopt;
  }

  CC::ClosedCaptionEdgeEffect edge_effect = CC::ClosedCaptionEdgeEffect_Default;
  hr = closed_caption_properties_statics->get_FontEffect(&edge_effect);
  if (FAILED(hr)) {
    return std::nullopt;
  }

  CC::ClosedCaptionStyle font_family = CC::ClosedCaptionStyle_Default;
  hr = closed_caption_properties_statics->get_FontStyle(&font_family);
  if (FAILED(hr)) {
    return std::nullopt;
  }

  CC::ClosedCaptionColor font_color = CC::ClosedCaptionColor_Default;
  hr = closed_caption_properties_statics->get_FontColor(&font_color);
  if (FAILED(hr)) {
    return std::nullopt;
  }

  CC::ClosedCaptionOpacity font_opacity = CC::ClosedCaptionOpacity_Default;
  hr = closed_caption_properties_statics->get_FontOpacity(&font_opacity);
  if (FAILED(hr)) {
    return std::nullopt;
  }

  CC::ClosedCaptionColor background_color = CC::ClosedCaptionColor_Default;
  hr =
      closed_caption_properties_statics->get_BackgroundColor(&background_color);
  if (FAILED(hr)) {
    return std::nullopt;
  }

  CC::ClosedCaptionOpacity background_opacity =
      CC::ClosedCaptionOpacity_Default;
  hr = closed_caption_properties_statics->get_BackgroundOpacity(
      &background_opacity);
  if (FAILED(hr)) {
    return std::nullopt;
  }

  CC::ClosedCaptionColor region_color = CC::ClosedCaptionColor_Default;
  hr = closed_caption_properties_statics->get_RegionColor(&region_color);
  if (FAILED(hr)) {
    return std::nullopt;
  }

  CC::ClosedCaptionOpacity region_opacity = CC::ClosedCaptionOpacity_Default;
  hr = closed_caption_properties_statics->get_RegionOpacity(&region_opacity);
  if (FAILED(hr)) {
    return std::nullopt;
  }

  CaptionStyle caption_style;

  // When the Default caption theme is active, the WinRT API still returns
  // non-Default enum values (e.g. White, Black, Tahoma) that represent the
  // platform defaults. In that case, populate the fields without !important so
  // they serve as UA defaults that HTML author ::cue styles can override. Only
  // when the user has intentionally selected a non-default theme do we mark
  // the values !important so the user's accessibility preference takes
  // priority over author styles.
  auto maybe_important = IsDefaultCaptionTheme()
                             ? [](const std::string& str) { return str; }
                             : AddCSSImportant;

  if (font_family != CC::ClosedCaptionStyle_Default) {
    GetFontFamilyString(font_family, &(caption_style.font_family),
                        &(caption_style.font_variant));
    caption_style.font_family = maybe_important(caption_style.font_family);
    caption_style.font_variant = maybe_important(caption_style.font_variant);
  }

  if (font_size != CC::ClosedCaptionSize_Default) {
    caption_style.text_size = maybe_important(GetCaptionSizeString(font_size));
  }

  if (edge_effect != CC::ClosedCaptionEdgeEffect_Default) {
    caption_style.text_shadow =
        maybe_important(GetEdgeEffectString(edge_effect));
  }

  if (font_color != CC::ClosedCaptionColor_Default) {
    caption_style.text_color =
        maybe_important(GetCssColorWithAlpha(font_color, font_opacity));
  }

  if (background_color != CC::ClosedCaptionColor_Default) {
    caption_style.background_color = maybe_important(
        GetCssColorWithAlpha(background_color, background_opacity));
  }

  if (region_color != CC::ClosedCaptionColor_Default) {
    caption_style.window_color =
        maybe_important(GetCssColorWithAlpha(region_color, region_opacity));
  }

  return caption_style;
}

// Thread-safe cache for the system caption style. Caching is only active while
// a ScopedClosureRunner returned by EnableCaptionStyleCaching() is held.
//
// Style computation (InitializeFromSystemSettings()) is slow and may block, so
// it is performed outside the lock. To prevent stale data from being published
// if the style changes or is invalidated concurrently while
// InitializeFromSystemSettings() is running, a `generation_` counter is
// incremented on every invalidation and when caching is disabled. A freshly
// computed style is only committed to the cache if the current generation
// matches the generation when computation began.
class SystemCaptionStyleCache {
 public:
  static SystemCaptionStyleCache& GetInstance() {
    static base::NoDestructor<SystemCaptionStyleCache> instance;
    return *instance;
  }

  SystemCaptionStyleCache() = default;
  SystemCaptionStyleCache(const SystemCaptionStyleCache&) = delete;
  SystemCaptionStyleCache& operator=(const SystemCaptionStyleCache&) = delete;

  std::optional<CaptionStyle> GetStyle() {
    uint64_t generation;
    {
      base::AutoLock auto_lock(lock_);
      if (caching_enabled_ && is_valid_) {
        return style_;
      }
      generation = generation_;
    }

    // Computed without holding the lock, since reading the OS settings may
    // block. Concurrent callers may duplicate this work, which is harmless
    // because they compute the same value.
    std::optional<CaptionStyle> style = InitializeFromSystemSettings();

    base::AutoLock auto_lock(lock_);
    // Only publish the result if caching is enabled, and if the settings did
    // not change while the style was being computed (in which case it may
    // already be stale).
    if (caching_enabled_ && generation == generation_) {
      style_ = style;
      is_valid_ = true;
    }
    return style;
  }

  void EnableCaching() {
    base::AutoLock auto_lock(lock_);
    CHECK(!caching_enabled_);
    caching_enabled_ = true;
  }

  void DisableCaching() {
    base::AutoLock auto_lock(lock_);
    CHECK(caching_enabled_);
    caching_enabled_ = false;
    ++generation_;
    is_valid_ = false;
    style_.reset();
  }

  void Invalidate() {
    base::AutoLock auto_lock(lock_);
    ++generation_;
    is_valid_ = false;
    style_.reset();
  }

 private:
  base::Lock lock_;

  // Whether the style may be cached at all. False until a watcher of the OS
  // caption settings takes responsibility for invalidating the cache.
  bool caching_enabled_ GUARDED_BY(lock_) = false;

  // Whether `style_` holds the style last read from the OS. Note that `style_`
  // is itself optional, since reading the OS settings can fail.
  bool is_valid_ GUARDED_BY(lock_) = false;
  std::optional<CaptionStyle> style_ GUARDED_BY(lock_);

  // Incremented on every invalidation, to detect results computed against
  // settings that have since changed.
  uint64_t generation_ GUARDED_BY(lock_) = 0;
};

}  // namespace

base::ScopedClosureRunner EnableCaptionStyleCaching() {
  SystemCaptionStyleCache::GetInstance().EnableCaching();
  return base::ScopedClosureRunner(base::BindOnce(
      &SystemCaptionStyleCache::DisableCaching,
      base::Unretained(&SystemCaptionStyleCache::GetInstance())));
}

void InvalidateCaptionStyleCache() {
  SystemCaptionStyleCache::GetInstance().Invalidate();
}

std::optional<CaptionStyle> CaptionStyle::FromSystemSettings() {
  return SystemCaptionStyleCache::GetInstance().GetStyle();
}

}  // namespace ui
