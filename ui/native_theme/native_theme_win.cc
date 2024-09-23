// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/native_theme/native_theme_win.h"

#include <windows.h>

#include <stddef.h>
#include <uxtheme.h>
#include <vsstyle.h>
#include <vssym32.h>

#include <optional>
#include <tuple>

#include "base/check.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/win/dark_mode_support.h"
#include "base/win/scoped_gdi_object.h"
#include "base/win/scoped_hdc.h"
#include "base/win/scoped_select_object.h"
#include "base/win/win_util.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"
#include "skia/ext/platform_canvas.h"
#include "skia/ext/skia_utils_win.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkColorPriv.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkShader.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"
#include "ui/color/color_provider.h"
#include "ui/display/win/screen_win.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/gdi_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/native_theme/common_theme.h"
#include "ui/native_theme/native_theme.h"

// This was removed from Winvers.h but is still used.
#if !defined(COLOR_MENUHIGHLIGHT)
#define COLOR_MENUHIGHLIGHT 29
#endif

namespace {

// Windows system color IDs cached and updated by the native theme.
const int kSysColors[] = {
    COLOR_BTNFACE,       COLOR_BTNTEXT,    COLOR_GRAYTEXT,      COLOR_HIGHLIGHT,
    COLOR_HIGHLIGHTTEXT, COLOR_HOTLIGHT,   COLOR_MENUHIGHLIGHT, COLOR_SCROLLBAR,
    COLOR_WINDOW,        COLOR_WINDOWTEXT,
};

void SetCheckerboardShader(SkPaint* paint, const RECT& align_rect) {
  // Create a 2x2 checkerboard pattern using the 3D face and highlight colors.
  const SkColor face = color_utils::GetSysSkColor(COLOR_3DFACE);
  const SkColor highlight = color_utils::GetSysSkColor(COLOR_3DHILIGHT);
  SkColor buffer[] = { face, highlight, highlight, face };
  // Confusing bit: we first create a temporary bitmap with our desired pattern,
  // then copy it to another bitmap.  The temporary bitmap doesn't take
  // ownership of the pixel data, and so will point to garbage when this
  // function returns.  The copy will copy the pixel data into a place owned by
  // the bitmap, which is in turn owned by the shader, etc., so it will live
  // until we're done using it.
  SkImageInfo info = SkImageInfo::MakeN32Premul(2, 2);
  SkBitmap temp_bitmap;
  temp_bitmap.installPixels(info, buffer, info.minRowBytes());
  SkBitmap bitmap;
  if (bitmap.tryAllocPixels(info))
    temp_bitmap.readPixels(info, bitmap.getPixels(), bitmap.rowBytes(), 0, 0);

  // Align the pattern with the upper corner of |align_rect|.
  SkMatrix local_matrix;
  local_matrix.setTranslate(SkIntToScalar(align_rect.left),
                            SkIntToScalar(align_rect.top));
  paint->setShader(bitmap.makeShader(SkTileMode::kRepeat, SkTileMode::kRepeat,
                                     SkSamplingOptions(), &local_matrix));
}

//    <-a->
// [  *****             ]
//  ____ |              |
//  <-a-> <------b----->
// a: object_width
// b: frame_width
// *: animating object
//
// - the animation goes from "[" to "]" repeatedly.
// - the animation offset is at first "|"
//
int ComputeAnimationProgress(int frame_width,
                             int object_width,
                             int pixels_per_second,
                             double animated_seconds) {
  int animation_width = frame_width + object_width;
  double interval = static_cast<double>(animation_width) / pixels_per_second;
  double ratio = fmod(animated_seconds, interval) / interval;
  return static_cast<int>(animation_width * ratio) - object_width;
}

// Custom scoped object for storing DC and a bitmap that was selected into it,
// and making sure that they are deleted in the right order.
class ScopedCreateDCWithBitmap {
 public:
  explicit ScopedCreateDCWithBitmap(base::win::ScopedCreateDC::Handle hdc)
      : dc_(hdc) {}

  ScopedCreateDCWithBitmap(const ScopedCreateDCWithBitmap&) = delete;
  ScopedCreateDCWithBitmap& operator=(const ScopedCreateDCWithBitmap&) = delete;

  ~ScopedCreateDCWithBitmap() {
    // Delete DC before the bitmap, since objects should not be deleted while
    // selected into a DC.
    dc_.Close();
  }

  bool IsValid() const { return dc_.IsValid(); }

  base::win::ScopedCreateDC::Handle Get() const { return dc_.Get(); }

  // Selects |handle| to bitmap into DC. Returns false if handle is not valid.
  bool SelectBitmap(base::win::ScopedBitmap::element_type handle) {
    bitmap_.reset(handle);
    if (!bitmap_.is_valid())
      return false;

    SelectObject(dc_.Get(), bitmap_.get());
    return true;
  }

 private:
  base::win::ScopedCreateDC dc_;
  base::win::ScopedBitmap bitmap_;
};

base::win::RegKey OpenThemeRegKey(REGSAM access) {
  base::win::RegKey hkcu_themes_regkey;
  // Validity is checked at time-of-use.
  std::ignore = hkcu_themes_regkey.Open(HKEY_CURRENT_USER,
                                        L"Software\\Microsoft\\Windows\\"
                                        L"CurrentVersion\\Themes\\Personalize",
                                        access);
  return hkcu_themes_regkey;
}

base::win::RegKey OpenColorFilteringRegKey(REGSAM access) {
  base::win::RegKey hkcu_color_filtering_regkey;
  // Validity is checked at time-of-use.
  std::ignore = hkcu_color_filtering_regkey.Open(
      HKEY_CURRENT_USER, L"Software\\Microsoft\\ColorFiltering", access);
  return hkcu_color_filtering_regkey;
}

}  // namespace

namespace ui {

NativeTheme::SystemThemeColor SysColorToSystemThemeColor(int system_color) {
  switch (system_color) {
    case COLOR_BTNFACE:
      return NativeTheme::SystemThemeColor::kButtonFace;
    case COLOR_BTNTEXT:
      return NativeTheme::SystemThemeColor::kButtonText;
    case COLOR_GRAYTEXT:
      return NativeTheme::SystemThemeColor::kGrayText;
    case COLOR_HIGHLIGHT:
      return NativeTheme::SystemThemeColor::kHighlight;
    case COLOR_HIGHLIGHTTEXT:
      return NativeTheme::SystemThemeColor::kHighlightText;
    case COLOR_HOTLIGHT:
      return NativeTheme::SystemThemeColor::kHotlight;
    case COLOR_MENUHIGHLIGHT:
      return NativeTheme::SystemThemeColor::kMenuHighlight;
    case COLOR_SCROLLBAR:
      return NativeTheme::SystemThemeColor::kScrollbar;
    case COLOR_WINDOW:
      return NativeTheme::SystemThemeColor::kWindow;
    case COLOR_WINDOWTEXT:
      return NativeTheme::SystemThemeColor::kWindowText;
    default:
      return NativeTheme::SystemThemeColor::kNotSupported;
  }
}

NativeTheme* NativeTheme::GetInstanceForNativeUi() {
  static base::NoDestructor<NativeThemeWin> s_native_theme(true, false);
  return s_native_theme.get();
}

NativeTheme* NativeTheme::GetInstanceForDarkUI() {
  static base::NoDestructor<NativeThemeWin> s_dark_native_theme(false, true);
  return s_dark_native_theme.get();
}

// static
bool NativeTheme::SystemDarkModeSupported() {
  static bool system_supports_dark_mode =
      ([]() { return OpenThemeRegKey(KEY_READ).Valid(); })();
  return system_supports_dark_mode;
}

// static
void NativeThemeWin::CloseHandles() {
  static_cast<NativeThemeWin*>(NativeTheme::GetInstanceForNativeUi())
      ->CloseHandlesInternal();
}

gfx::Size NativeThemeWin::GetPartSize(Part part,
                                      State state,
                                      const ExtraParams& extra) const {
  // The GetThemePartSize call below returns the default size without
  // accounting for user customization (crbug/218291).
  switch (part) {
    case kScrollbarDownArrow:
    case kScrollbarLeftArrow:
    case kScrollbarRightArrow:
    case kScrollbarUpArrow:
    case kScrollbarHorizontalThumb:
    case kScrollbarVerticalThumb:
    case kScrollbarHorizontalTrack:
    case kScrollbarVerticalTrack: {
      int size = display::win::ScreenWin::GetSystemMetricsInDIP(SM_CXVSCROLL);
      if (size == 0)
        size = 17;
      return gfx::Size(size, size);
    }
    default:
      break;
  }

  int part_id = GetWindowsPart(part, state, extra);
  int state_id = GetWindowsState(part, state, extra);

  base::win::ScopedGetDC screen_dc(nullptr);
  SIZE size;
  HANDLE handle = GetThemeHandle(GetThemeName(part));
  if (handle && SUCCEEDED(GetThemePartSize(handle, screen_dc, part_id, state_id,
                                           nullptr, TS_TRUE, &size)))
    return gfx::Size(size.cx, size.cy);

  // TODO(rogerta): For now, we need to support radio buttons and checkboxes
  // when theming is not enabled.  Support for other parts can be added
  // if/when needed.
  return (part == kCheckbox || part == kRadio) ?
      gfx::Size(13, 13) : gfx::Size();
}

void NativeThemeWin::Paint(cc::PaintCanvas* canvas,
                           const ui::ColorProvider* color_provider,
                           Part part,
                           State state,
                           const gfx::Rect& rect,
                           const ExtraParams& extra,
                           ColorScheme color_scheme,
                           bool in_forced_colors,
                           const std::optional<SkColor>& accent_color) const {
  if (rect.IsEmpty())
    return;

  switch (part) {
    case kMenuPopupGutter:
      PaintMenuGutter(canvas, color_provider, rect);
      return;
    case kMenuPopupSeparator:
      PaintMenuSeparator(canvas, color_provider,
                         absl::get<MenuSeparatorExtraParams>(extra));
      return;
    case kMenuPopupBackground:
      PaintMenuBackground(canvas, color_provider, rect);
      return;
    case kMenuItemBackground:
      CommonThemePaintMenuItemBackground(this, color_provider, canvas, state,
                                         rect,
                                         absl::get<MenuItemExtraParams>(extra));
      return;
    default:
      PaintIndirect(canvas, part, state, rect, extra);
      return;
  }
}

NativeThemeWin::NativeThemeWin(bool configure_web_instance,
                               bool should_only_use_dark_colors)
    : NativeTheme(should_only_use_dark_colors),
      supports_windows_dark_mode_(base::win::IsDarkModeAvailable()),
      color_change_listener_(this) {
  // By default UI should not use the system accent color.
  set_should_use_system_accent_color(false);

  // If there's no sequenced task runner handle, we can't be called back for
  // registry changes. This generally happens in tests.
  const bool observers_can_operate =
      base::SequencedTaskRunner::HasCurrentDefault();

  hkcu_themes_regkey_ = OpenThemeRegKey(KEY_READ | KEY_NOTIFY);
  if (hkcu_themes_regkey_.Valid()) {
    if (!should_only_use_dark_colors && !IsForcedDarkMode() &&
        !IsForcedHighContrast()) {
      UpdateDarkModeStatus();
    }
    UpdatePrefersReducedTransparency();
    if (observers_can_operate) {
      RegisterThemeRegkeyObserver();
    }
  }

  hkcu_color_filtering_regkey_ =
      OpenColorFilteringRegKey(KEY_READ | KEY_NOTIFY);
  if (hkcu_color_filtering_regkey_.Valid()) {
    UpdateInvertedColors();
    if (observers_can_operate) {
      RegisterColorFilteringRegkeyObserver();
    }
  }

  if (!IsForcedHighContrast()) {
    set_forced_colors(IsUsingHighContrastThemeInternal());
  }

  // Initialize the cached system colors.
  UpdateSystemColors();
  set_preferred_color_scheme(CalculatePreferredColorScheme());
  SetPreferredContrast(CalculatePreferredContrast());

  memset(theme_handles_, 0, sizeof(theme_handles_));

  if (configure_web_instance)
    ConfigureWebInstance();
}

void NativeThemeWin::ConfigureWebInstance() {
  // Add the web native theme as an observer to stay in sync with color scheme
  // changes.
  color_scheme_observer_ =
      std::make_unique<NativeTheme::ColorSchemeNativeThemeObserver>(
          NativeTheme::GetInstanceForWeb());
  AddObserver(color_scheme_observer_.get());

  // Initialize the native theme web instance with the system color info.
  NativeTheme* web_instance = NativeTheme::GetInstanceForWeb();
  web_instance->set_use_dark_colors(ShouldUseDarkColors());
  web_instance->set_forced_colors(InForcedColorsMode());
  web_instance->set_preferred_color_scheme(GetPreferredColorScheme());
  web_instance->SetPreferredContrast(GetPreferredContrast());
  web_instance->set_prefers_reduced_transparency(
      GetPrefersReducedTransparency());
  web_instance->set_system_colors(GetSystemColors());
  web_instance->set_should_use_system_accent_color(
      should_use_system_accent_color());
}

std::optional<base::TimeDelta> NativeThemeWin::GetPlatformCaretBlinkInterval()
    const {
  static const size_t system_value = ::GetCaretBlinkTime();
  if (system_value != 0) {
    return (system_value == INFINITE) ? base::TimeDelta()
                                      : base::Milliseconds(system_value);
  }
  return std::nullopt;
}

NativeThemeWin::~NativeThemeWin() {
  // TODO(crbug.com/40551168): Calling CloseHandles() here breaks
  // certain tests and the reliability bots.
  // CloseHandles();
}

bool NativeThemeWin::IsUsingHighContrastThemeInternal() const {
  HIGHCONTRAST result;
  result.cbSize = sizeof(HIGHCONTRAST);
  return SystemParametersInfo(SPI_GETHIGHCONTRAST, result.cbSize, &result, 0) &&
         (result.dwFlags & HCF_HIGHCONTRASTON) == HCF_HIGHCONTRASTON;
}

void NativeThemeWin::CloseHandlesInternal() {
  for (int i = 0; i < LAST; ++i) {
    if (theme_handles_[i]) {
      CloseThemeData(theme_handles_[i]);
      theme_handles_[i] = nullptr;
    }
  }
}

void NativeThemeWin::OnSysColorChange() {
  UpdateSystemColors();
  if (!IsForcedHighContrast())
    set_forced_colors(IsUsingHighContrastThemeInternal());
  set_preferred_color_scheme(CalculatePreferredColorScheme());
  SetPreferredContrast(CalculatePreferredContrast());
  NotifyOnNativeThemeUpdated();
}

void NativeThemeWin::UpdateSystemColors() {
  for (int sys_color : kSysColors)
    system_colors_[SysColorToSystemThemeColor(sys_color)] =
        color_utils::GetSysSkColor(sys_color);
}

void NativeThemeWin::PaintMenuSeparator(
    cc::PaintCanvas* canvas,
    const ColorProvider* color_provider,
    const MenuSeparatorExtraParams& params) const {
  DCHECK(color_provider);
  const gfx::RectF rect(*params.paint_rect);
  gfx::PointF start = rect.CenterPoint();
  gfx::PointF end = start;
  if (params.type == ui::VERTICAL_SEPARATOR) {
    start.set_y(rect.y());
    end.set_y(rect.bottom());
  } else {
    start.set_x(rect.x());
    end.set_x(rect.right());
  }

  cc::PaintFlags flags;
  flags.setColor(color_provider->GetColor(kColorMenuSeparator));
  canvas->drawLine(start.x(), start.y(), end.x(), end.y(), flags);
}

void NativeThemeWin::PaintMenuGutter(cc::PaintCanvas* canvas,
                                     const ColorProvider* color_provider,
                                     const gfx::Rect& rect) const {
  DCHECK(color_provider);
  cc::PaintFlags flags;
  flags.setColor(color_provider->GetColor(kColorMenuSeparator));
  int position_x = rect.x() + rect.width() / 2;
  canvas->drawLine(position_x, rect.y(), position_x, rect.bottom(), flags);
}

void NativeThemeWin::PaintMenuBackground(cc::PaintCanvas* canvas,
                                         const ColorProvider* color_provider,
                                         const gfx::Rect& rect) const {
  DCHECK(color_provider);
  cc::PaintFlags flags;
  flags.setColor(color_provider->GetColor(kColorMenuBackground));
  canvas->drawRect(gfx::RectToSkRect(rect), flags);
}

void NativeThemeWin::PaintDirect(SkCanvas* destination_canvas,
                                 HDC hdc,
                                 Part part,
                                 State state,
                                 const gfx::Rect& rect,
                                 const ExtraParams& extra) const {
  if (part == kScrollbarCorner) {
    // Special-cased here since there is no theme name for kScrollbarCorner.
    destination_canvas->drawColor(SK_ColorWHITE, SkBlendMode::kSrc);
    return;
  }

  RECT rect_win = rect.ToRECT();
  if (part == kTrackbarTrack) {
    // Make the channel be 4 px thick in the center of the supplied rect.  (4 px
    // matches what XP does in various menus; GetThemePartSize() doesn't seem to
    // return good values here.)
    constexpr int kChannelThickness = 4;
    if (absl::get<TrackbarExtraParams>(extra).vertical) {
      rect_win.top += (rect_win.bottom - rect_win.top - kChannelThickness) / 2;
      rect_win.bottom = rect_win.top + kChannelThickness;
    } else {
      rect_win.left += (rect_win.right - rect_win.left - kChannelThickness) / 2;
      rect_win.right = rect_win.left + kChannelThickness;
    }
  }

  // Most parts can be drawn simply when there is a theme handle.
  const HANDLE handle = GetThemeHandle(GetThemeName(part));
  const int part_id = GetWindowsPart(part, state, extra);
  const int state_id = GetWindowsState(part, state, extra);
  if (handle) {
    switch (part) {
      case kMenuPopupArrow:
        // The right-pointing arrow can use the common code, but the
        // left-pointing one needs custom code.
        if (!absl::get<MenuArrowExtraParams>(extra).pointing_right) {
          PaintLeftMenuArrowThemed(hdc, handle, part_id, state_id, rect);
          return;
        }
        [[fallthrough]];
      case kCheckbox:
      case kInnerSpinButton:
      case kMenuCheck:
      case kMenuCheckBackground:
      case kMenuList:
      case kProgressBar:
      case kPushButton:
      case kRadio:
      case kScrollbarHorizontalTrack:
      case kScrollbarVerticalTrack:
      case kTabPanelBackground:
      case kTrackbarThumb:
      case kTrackbarTrack:
      case kWindowResizeGripper:
        DrawThemeBackground(handle, hdc, part_id, state_id, &rect_win, nullptr);
        if (part == kProgressBar)
          break;  // Further painting to do below.
        return;
      case kScrollbarDownArrow:
      case kScrollbarHorizontalGripper:
      case kScrollbarHorizontalThumb:
      case kScrollbarLeftArrow:
      case kScrollbarRightArrow:
      case kScrollbarUpArrow:
      case kScrollbarVerticalGripper:
      case kScrollbarVerticalThumb:
        PaintScaledTheme(handle, hdc, part_id, state_id, rect);
        return;
      case kTextField:
        break;  // Handled entirely below.
      case kMenuItemBackground:
      case kMenuPopupBackground:
      case kMenuPopupGutter:
      case kMenuPopupSeparator:
      case kScrollbarCorner:
      case kSliderTrack:
      case kSliderThumb:
      case kMaxPart:
        NOTREACHED_IN_MIGRATION();
    }
  }

  // Do any further painting the common code couldn't handle.
  switch (part) {
    case kCheckbox:
    case kPushButton:
    case kRadio:
      PaintButtonClassic(hdc, part, state, &rect_win,
                         absl::get<ButtonExtraParams>(extra));
      return;
    case kInnerSpinButton:
      DrawFrameControl(
          hdc, &rect_win, DFC_SCROLL,
          absl::get<InnerSpinButtonExtraParams>(extra).classic_state);
      return;
    case kMenuCheck: {
      const auto& menu_check = absl::get<MenuCheckExtraParams>(extra);
      PaintFrameControl(hdc, rect, DFC_MENU,
                        menu_check.is_radio ? DFCS_MENUBULLET : DFCS_MENUCHECK,
                        menu_check.is_selected, state);
      return;
    }
    case kMenuList:
      DrawFrameControl(hdc, &rect_win, DFC_SCROLL,
                       DFCS_SCROLLCOMBOBOX |
                           absl::get<MenuListExtraParams>(extra).classic_state);
      return;
    case kMenuPopupArrow: {
      const auto& menu_arrow = absl::get<MenuArrowExtraParams>(extra);
      // For some reason, Windows uses the name DFCS_MENUARROWRIGHT to indicate
      // a left pointing arrow.
      PaintFrameControl(
          hdc, rect, DFC_MENU,
          menu_arrow.pointing_right ? DFCS_MENUARROW : DFCS_MENUARROWRIGHT,
          menu_arrow.is_selected, state);
      return;
    }
    case kProgressBar: {
      const auto& progress_bar = absl::get<ProgressBarExtraParams>(extra);
      RECT value_rect =
          gfx::Rect(progress_bar.value_rect_x, progress_bar.value_rect_y,
                    progress_bar.value_rect_width,
                    progress_bar.value_rect_height)
              .ToRECT();
      if (handle) {
        PaintProgressBarOverlayThemed(hdc, handle, &rect_win, &value_rect,
                                      progress_bar);
      } else {
        FillRect(hdc, &rect_win, GetSysColorBrush(COLOR_BTNFACE));
        FillRect(hdc, &value_rect, GetSysColorBrush(COLOR_BTNSHADOW));
        DrawEdge(hdc, &rect_win, EDGE_SUNKEN, BF_RECT | BF_ADJUST);
      }
      return;
    }
    case kScrollbarDownArrow:
    case kScrollbarLeftArrow:
    case kScrollbarRightArrow:
    case kScrollbarUpArrow:
      PaintScrollbarArrowClassic(hdc, part, state, &rect_win);
      return;
    case kScrollbarHorizontalThumb:
    case kScrollbarVerticalThumb:
      DrawEdge(hdc, &rect_win, EDGE_RAISED, BF_RECT | BF_MIDDLE);
      return;
    case kScrollbarHorizontalTrack:
    case kScrollbarVerticalTrack:
      PaintScrollbarTrackClassic(destination_canvas, hdc, &rect_win,
                                 absl::get<ScrollbarTrackExtraParams>(extra));
      return;
    case kTabPanelBackground:
      // Classic just renders a flat color background.
      FillRect(hdc, &rect_win, reinterpret_cast<HBRUSH>(COLOR_3DFACE + 1));
      return;
    case kTextField: {
      // TODO(mpcomplete): can we detect if the color is specified by the user,
      // and if not, just use the system color?
      // CreateSolidBrush() accepts a RGB value but alpha must be 0.
      const auto& text_field = absl::get<TextFieldExtraParams>(extra);
      base::win::ScopedGDIObject<HBRUSH> bg_brush(CreateSolidBrush(
          skia::SkColorToCOLORREF(text_field.background_color)));
      if (handle) {
        PaintTextFieldThemed(hdc, handle, bg_brush.get(), part_id, state_id,
                             &rect_win, text_field);
      } else {
        PaintTextFieldClassic(hdc, bg_brush.get(), &rect_win, text_field);
      }
      return;
    }
    case kTrackbarThumb: {
      const auto& trackbar = absl::get<TrackbarExtraParams>(extra);
      if (trackbar.vertical) {
        DrawEdge(hdc, &rect_win, EDGE_RAISED, BF_RECT | BF_SOFT | BF_MIDDLE);
      } else {
        PaintHorizontalTrackbarThumbClassic(destination_canvas, hdc, rect_win,
                                            trackbar);
      }
      return;
    }
    case kTrackbarTrack:
      DrawEdge(hdc, &rect_win, EDGE_SUNKEN, BF_RECT);
      return;
    case kWindowResizeGripper:
      // Draw a windows classic scrollbar gripper.
      DrawFrameControl(hdc, &rect_win, DFC_SCROLL, DFCS_SCROLLSIZEGRIP);
      return;
    case kMenuCheckBackground:
    case kScrollbarHorizontalGripper:
    case kScrollbarVerticalGripper:
      return;  // No further painting necessary.
    case kMenuItemBackground:
    case kMenuPopupBackground:
    case kMenuPopupGutter:
    case kMenuPopupSeparator:
    case kScrollbarCorner:
    case kSliderTrack:
    case kSliderThumb:
    case kMaxPart:
      NOTREACHED_IN_MIGRATION();
  }
}

bool NativeThemeWin::SupportsNinePatch(Part part) const {
  // The only nine-patch resources currently supported (overlay scrollbar) are
  // painted by NativeThemeAura on Windows.
  return false;
}

gfx::Size NativeThemeWin::GetNinePatchCanvasSize(Part part) const {
  NOTREACHED_IN_MIGRATION()
      << "NativeThemeWin doesn't support nine-patch resources.";
  return gfx::Size();
}

gfx::Rect NativeThemeWin::GetNinePatchAperture(Part part) const {
  NOTREACHED_IN_MIGRATION()
      << "NativeThemeWin doesn't support nine-patch resources.";
  return gfx::Rect();
}

bool NativeThemeWin::ShouldUseDarkColors() const {
  // Windows high contrast modes are entirely different themes,
  // so let them take priority over dark mode.
  // ...unless --force-dark-mode was specified in which case caveat emptor.
  if (InForcedColorsMode() && !IsForcedDarkMode())
    return false;
  return NativeTheme::ShouldUseDarkColors();
}

NativeTheme::PreferredColorScheme
NativeThemeWin::CalculatePreferredColorScheme() const {
  if (!InForcedColorsMode())
    return NativeTheme::CalculatePreferredColorScheme();

  // According to the spec, the preferred color scheme for web content is 'dark'
  // if 'Canvas' has L<33% and 'light' if L>67%. On Windows, the 'Canvas'
  // keyword is mapped to the 'Window' system color. As such, we use the
  // luminance of 'Window' to calculate the corresponding luminance of 'Canvas'.
  // https://www.w3.org/TR/css-color-adjust-1/#forced
  SkColor bg_color = system_colors_[SystemThemeColor::kWindow];
  float luminance = color_utils::GetRelativeLuminance(bg_color);
  if (luminance < 0.33)
    return NativeTheme::PreferredColorScheme::kDark;
  return NativeTheme::PreferredColorScheme::kLight;
}

NativeTheme::PreferredContrast NativeThemeWin::CalculatePreferredContrast()
    const {
  if (!InForcedColorsMode())
    return NativeTheme::CalculatePreferredContrast();

  // TODO(sartang@microsoft.com): Update the spec page at
  // https://www.w3.org/TR/css-color-adjust-1/#forced, it currently does not
  // mention the relation between forced-colors-active and prefers-contrast.
  //
  // According to spec [1], "in addition to forced-colors: active, the user
  // agent must also match one of prefers-contrast: more or
  // prefers-contrast: less if it can determine that the forced color
  // palette chosen by the user has a particularly high or low contrast,
  // and must make prefers-contrast: custom match otherwise".
  //
  // Using WCAG definitions [2], we have decided to match 'more' in Forced
  // Colors Mode if the contrast ratio between the foreground and background
  // color is 7:1 or greater.
  //
  // "A contrast ratio of 3:1 is the minimum level recommended by [[ISO-9241-3]]
  // and [[ANSI-HFES-100-1988]] for standard text and vision"[2]. Given this,
  // we will start by matching to 'less' in Forced Colors Mode if the contrast
  // ratio between the foreground and background color is 2.5:1 or less.
  //
  // These ratios will act as an experimental baseline that we can adjust based
  // on user feedback.
  //
  // [1]
  // https://drafts.csswg.org/mediaqueries-5/#valdef-media-forced-colors-active
  // [2] https://www.w3.org/WAI/WCAG21/Understanding/contrast-enhanced
  SkColor bg_color = system_colors_[SystemThemeColor::kWindow];
  SkColor fg_color = system_colors_[SystemThemeColor::kWindowText];
  float contrast_ratio = color_utils::GetContrastRatio(bg_color, fg_color);
  if (contrast_ratio >= 7)
    return NativeTheme::PreferredContrast::kMore;
  return contrast_ratio <= 2.5 ? NativeTheme::PreferredContrast::kLess
                               : NativeTheme::PreferredContrast::kCustom;
}

NativeTheme::ColorScheme NativeThemeWin::GetDefaultSystemColorScheme() const {
  return InForcedColorsMode() ? ColorScheme::kPlatformHighContrast
                              : NativeTheme::GetDefaultSystemColorScheme();
}

void NativeThemeWin::PaintIndirect(cc::PaintCanvas* destination_canvas,
                                   Part part,
                                   State state,
                                   const gfx::Rect& rect,
                                   const ExtraParams& extra) const {
  // TODO(asvitkine): This path is pretty inefficient - for each paint operation
  // it creates a new offscreen bitmap Skia canvas. This can be sped up by doing
  // it only once per part/state and keeping a cache of the resulting bitmaps.
  //
  // TODO(enne): This could also potentially be sped up for software raster
  // by moving these draw ops into PaintRecord itself and then moving the
  // PaintDirect code to be part of the raster for PaintRecord.

  // If this process doesn't have access to GDI, we'd need to use shared memory
  // segment instead but that is not supported right now.
  if (!base::win::IsUser32AndGdi32Available())
    return;

  ScopedCreateDCWithBitmap offscreen_hdc(CreateCompatibleDC(nullptr));
  if (!offscreen_hdc.IsValid())
    return;

  skia::InitializeDC(offscreen_hdc.Get());
  HRGN clip = CreateRectRgn(0, 0, rect.width(), rect.height());
  if ((SelectClipRgn(offscreen_hdc.Get(), clip) == ERROR) ||
      !DeleteObject(clip)) {
    return;
  }

  base::win::ScopedBitmap hbitmap = skia::CreateHBitmapXRGB8888(
      rect.width(), rect.height(), nullptr, nullptr);
  if (!offscreen_hdc.SelectBitmap(hbitmap.release()))
    return;

  // Will be NULL if lower-level Windows calls fail, or if the backing
  // allocated is 0 pixels in size (which should never happen according to
  // Windows documentation).
  sk_sp<SkSurface> offscreen_surface =
      skia::MapPlatformSurface(offscreen_hdc.Get());
  if (!offscreen_surface)
    return;

  SkCanvas* offscreen_canvas = offscreen_surface->getCanvas();
  DCHECK(offscreen_canvas);

  // Some of the Windows theme drawing operations do not write correct alpha
  // values for fully-opaque pixels; instead the pixels get alpha 0. This is
  // especially a problem on Windows XP or when using the Classic theme.
  //
  // To work-around this, mark all pixels with a placeholder value, to detect
  // which pixels get touched by the paint operation. After paint, set any
  // pixels that have alpha 0 to opaque and placeholders to fully-transparent.
  constexpr SkColor placeholder = SkColorSetARGB(1, 0, 0, 0);
  offscreen_canvas->clear(placeholder);

  // Offset destination rects to have origin (0,0).
  gfx::Rect adjusted_rect(rect.size());
  ExtraParams adjusted_extra = extra;
  switch (part) {
    case kProgressBar: {
      auto progress_bar = absl::get<ProgressBarExtraParams>(adjusted_extra);
      progress_bar.value_rect_x = 0;
      progress_bar.value_rect_y = 0;
      break;
    }
    case kScrollbarHorizontalTrack:
    case kScrollbarVerticalTrack: {
      auto scrollbar_track =
          absl::get<ScrollbarTrackExtraParams>(adjusted_extra);
      scrollbar_track.track_x = 0;
      scrollbar_track.track_y = 0;
      break;
    }
    default:
      break;
  }
  // Draw the theme controls using existing HDC-drawing code.
  PaintDirect(offscreen_canvas, offscreen_hdc.Get(), part, state,
              adjusted_rect, adjusted_extra);

  SkBitmap offscreen_bitmap = skia::MapPlatformBitmap(offscreen_hdc.Get());

  // Post-process the pixels to fix up the alpha values (see big comment above).
  const SkPMColor placeholder_value = SkPreMultiplyColor(placeholder);
  const int pixel_count = rect.width() * rect.height();
  SkPMColor* pixels = offscreen_bitmap.getAddr32(0, 0);
  for (int i = 0; i < pixel_count; i++) {
    if (pixels[i] == placeholder_value) {
      // Pixel wasn't touched - make it fully transparent.
      pixels[i] = SkPackARGB32(0, 0, 0, 0);
    } else if (SkGetPackedA32(pixels[i]) == 0) {
      // Pixel was touched but has incorrect alpha of 0, make it fully opaque.
      pixels[i] = SkPackARGB32(0xFF,
                               SkGetPackedR32(pixels[i]),
                               SkGetPackedG32(pixels[i]),
                               SkGetPackedB32(pixels[i]));
    }
  }

  destination_canvas->drawImage(
      cc::PaintImage::CreateFromBitmap(std::move(offscreen_bitmap)), rect.x(),
      rect.y());
}

void NativeThemeWin::PaintButtonClassic(HDC hdc,
                                        Part part,
                                        State state,
                                        RECT* rect,
                                        const ButtonExtraParams& extra) const {
  int classic_state = extra.classic_state;
  switch (part) {
    case kCheckbox:
      classic_state |= DFCS_BUTTONCHECK;
      break;
    case kPushButton:
      classic_state |= DFCS_BUTTONRADIO;
      break;
    case kRadio:
      classic_state |= DFCS_BUTTONPUSH;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  if (state == kDisabled)
    classic_state |= DFCS_INACTIVE;
  else if (state == kPressed)
    classic_state |= DFCS_PUSHED;

  if (extra.checked)
    classic_state |= DFCS_CHECKED;

  if ((part == kPushButton) && ((state == kPressed) || extra.is_default)) {
    // Pressed or defaulted buttons have a shadow replacing the outer 1 px.
    HBRUSH brush = GetSysColorBrush(COLOR_3DDKSHADOW);
    if (brush) {
      FrameRect(hdc, rect, brush);
      InflateRect(rect, -1, -1);
    }
  }

  DrawFrameControl(hdc, rect, DFC_BUTTON, classic_state);

  // Draw a focus rectangle (the dotted line box) on defaulted buttons.
  if ((part == kPushButton) && extra.is_default) {
    InflateRect(rect, -GetSystemMetrics(SM_CXEDGE),
                -GetSystemMetrics(SM_CYEDGE));
    DrawFocusRect(hdc, rect);
  }

  // Classic theme doesn't support indeterminate checkboxes.  We draw a
  // recangle inside a checkbox like IE10 does.
  if ((part == kCheckbox) && extra.indeterminate) {
    RECT inner_rect = *rect;
    // "4 / 13" is same as IE10 in classic theme.
    int padding = (inner_rect.right - inner_rect.left) * 4 / 13;
    InflateRect(&inner_rect, -padding, -padding);
    int color_index = (state == kDisabled) ? COLOR_GRAYTEXT : COLOR_WINDOWTEXT;
    FillRect(hdc, &inner_rect, GetSysColorBrush(color_index));
  }
}

void NativeThemeWin::PaintLeftMenuArrowThemed(HDC hdc,
                                              HANDLE handle,
                                              int part_id,
                                              int state_id,
                                              const gfx::Rect& rect) const {
  // There is no way to tell the uxtheme API to draw a left pointing arrow; it
  // doesn't have a flag equivalent to DFCS_MENUARROWRIGHT.  But they are needed
  // for RTL locales on Vista.  So use a memory DC and mirror the region with
  // GDI's StretchBlt.
  base::win::ScopedCreateDC mem_dc(CreateCompatibleDC(hdc));
  base::win::ScopedBitmap mem_bitmap(
      CreateCompatibleBitmap(hdc, rect.width(), rect.height()));
  base::win::ScopedSelectObject select_bitmap(mem_dc.Get(), mem_bitmap.get());
  // Copy and horizontally mirror the background from hdc into mem_dc. Use a
  // negative-width source rect, starting at the rightmost pixel.
  StretchBlt(mem_dc.Get(), 0, 0, rect.width(), rect.height(), hdc,
             rect.right() - 1, rect.y(), -rect.width(), rect.height(), SRCCOPY);
  // Draw the arrow.
  RECT theme_rect = {0, 0, rect.width(), rect.height()};
  DrawThemeBackground(handle, mem_dc.Get(), part_id, state_id, &theme_rect,
                      nullptr);
  // Copy and mirror the result back into mem_dc.
  StretchBlt(hdc, rect.x(), rect.y(), rect.width(), rect.height(), mem_dc.Get(),
             rect.width() - 1, 0, -rect.width(), rect.height(), SRCCOPY);
}

void NativeThemeWin::PaintScrollbarArrowClassic(HDC hdc,
                                                Part part,
                                                State state,
                                                RECT* rect) const {
  int classic_state = DFCS_SCROLLDOWN;
  switch (part) {
    case kScrollbarDownArrow:
      break;
    case kScrollbarLeftArrow:
      classic_state = DFCS_SCROLLLEFT;
      break;
    case kScrollbarRightArrow:
      classic_state = DFCS_SCROLLRIGHT;
      break;
    case kScrollbarUpArrow:
      classic_state = DFCS_SCROLLUP;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  switch (state) {
    case kDisabled:
      classic_state |= DFCS_INACTIVE;
      break;
    case kHovered:
      classic_state |= DFCS_HOT;
      break;
    case kNormal:
      break;
    case kPressed:
      classic_state |= DFCS_PUSHED;
      break;
    case kNumStates:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  DrawFrameControl(hdc, rect, DFC_SCROLL, classic_state);
}

void NativeThemeWin::PaintScrollbarTrackClassic(
    SkCanvas* canvas,
    HDC hdc,
    RECT* rect,
    const ScrollbarTrackExtraParams& extra) const {
  if ((system_colors_[SystemThemeColor::kScrollbar] !=
       system_colors_[SystemThemeColor::kButtonFace]) &&
      (system_colors_[SystemThemeColor::kScrollbar] !=
       system_colors_[SystemThemeColor::kWindow])) {
    FillRect(hdc, rect, reinterpret_cast<HBRUSH>(COLOR_SCROLLBAR + 1));
  } else {
    SkPaint paint;
    RECT align_rect = gfx::Rect(extra.track_x, extra.track_y, extra.track_width,
                                extra.track_height)
                          .ToRECT();
    SetCheckerboardShader(&paint, align_rect);
    canvas->drawIRect(skia::RECTToSkIRect(*rect), paint);
  }
  if (extra.classic_state & DFCS_PUSHED)
    InvertRect(hdc, rect);
}

void NativeThemeWin::PaintHorizontalTrackbarThumbClassic(
    SkCanvas* canvas,
    HDC hdc,
    const RECT& rect,
    const TrackbarExtraParams& extra) const {
  // Split rect into top and bottom pieces.
  RECT top_section = rect;
  RECT bottom_section = rect;
  top_section.bottom -= ((bottom_section.right - bottom_section.left) / 2);
  bottom_section.top = top_section.bottom;
  DrawEdge(hdc, &top_section, EDGE_RAISED,
           BF_LEFT | BF_TOP | BF_RIGHT | BF_SOFT | BF_MIDDLE | BF_ADJUST);

  // Split triangular piece into two diagonals.
  RECT& left_half = bottom_section;
  RECT right_half = bottom_section;
  right_half.left += ((bottom_section.right - bottom_section.left) / 2);
  left_half.right = right_half.left;
  DrawEdge(hdc, &left_half, EDGE_RAISED,
           BF_DIAGONAL_ENDTOPLEFT | BF_SOFT | BF_MIDDLE | BF_ADJUST);
  DrawEdge(hdc, &right_half, EDGE_RAISED,
           BF_DIAGONAL_ENDBOTTOMLEFT | BF_SOFT | BF_MIDDLE | BF_ADJUST);

  // If the button is pressed, draw hatching.
  if (extra.classic_state & DFCS_PUSHED) {
    SkPaint paint;
    SetCheckerboardShader(&paint, rect);

    // Fill all three pieces with the pattern.
    canvas->drawIRect(skia::RECTToSkIRect(top_section), paint);

    SkScalar left_triangle_top = SkIntToScalar(left_half.top);
    SkScalar left_triangle_right = SkIntToScalar(left_half.right);
    SkPath left_triangle;
    left_triangle.moveTo(SkIntToScalar(left_half.left), left_triangle_top);
    left_triangle.lineTo(left_triangle_right, left_triangle_top);
    left_triangle.lineTo(left_triangle_right, SkIntToScalar(left_half.bottom));
    left_triangle.close();
    canvas->drawPath(left_triangle, paint);

    SkScalar right_triangle_left = SkIntToScalar(right_half.left);
    SkScalar right_triangle_top = SkIntToScalar(right_half.top);
    SkPath right_triangle;
    right_triangle.moveTo(right_triangle_left, right_triangle_top);
    right_triangle.lineTo(SkIntToScalar(right_half.right), right_triangle_top);
    right_triangle.lineTo(right_triangle_left,
                          SkIntToScalar(right_half.bottom));
    right_triangle.close();
    canvas->drawPath(right_triangle, paint);
  }
}

void NativeThemeWin::PaintProgressBarOverlayThemed(
    HDC hdc,
    HANDLE handle,
    RECT* bar_rect,
    RECT* value_rect,
    const ProgressBarExtraParams& extra) const {
  // There is no documentation about the animation speed, frame-rate, nor
  // size of moving overlay of the indeterminate progress bar.
  // So we just observed real-world programs and guessed following parameters.
  constexpr int kDeterminateOverlayWidth = 120;
  constexpr int kDeterminateOverlayPixelsPerSecond = 300;
  constexpr int kIndeterminateOverlayWidth = 120;
  constexpr int kIndeterminateOverlayPixelsPerSecond = 175;

  int bar_width = bar_rect->right - bar_rect->left;
  if (!extra.determinate) {
    // The glossy overlay for the indeterminate progress bar has a small pause
    // after each animation. We emulate this by adding an invisible margin the
    // animation has to traverse.
    int width_with_margin = bar_width + kIndeterminateOverlayPixelsPerSecond;
    int overlay_width = kIndeterminateOverlayWidth;
    RECT overlay_rect = *bar_rect;
    overlay_rect.left += ComputeAnimationProgress(
        width_with_margin, overlay_width, kIndeterminateOverlayPixelsPerSecond,
        extra.animated_seconds);
    overlay_rect.right = overlay_rect.left + overlay_width;
    DrawThemeBackground(handle, hdc, PP_MOVEOVERLAY, 0, &overlay_rect,
                        bar_rect);
    return;
  }

  // We care about the direction here because PP_CHUNK painting is asymmetric.
  // TODO(morrita): This RTL guess can be wrong.  We should pass in the
  // direction from WebKit.
  const bool mirror = bar_rect->right == value_rect->right &&
                      bar_rect->left != value_rect->left;
  const DTBGOPTS value_draw_options = {sizeof(DTBGOPTS),
                                       mirror ? DTBG_MIRRORDC : 0u, *bar_rect};

  // On Vista or later, the progress bar part has a single-block value part
  // and a glossy effect. The value part has exactly same height as the bar
  // part, so we don't need to shrink the rect.
  DrawThemeBackgroundEx(handle, hdc, PP_FILL, 0, value_rect,
                        &value_draw_options);

  RECT overlay_rect = *value_rect;
  overlay_rect.left += ComputeAnimationProgress(
      bar_width, kDeterminateOverlayWidth, kDeterminateOverlayPixelsPerSecond,
      extra.animated_seconds);
  overlay_rect.right = overlay_rect.left + kDeterminateOverlayWidth;
  DrawThemeBackground(handle, hdc, PP_MOVEOVERLAY, 0, &overlay_rect,
                      value_rect);
}

void NativeThemeWin::PaintTextFieldThemed(
    HDC hdc,
    HANDLE handle,
    HBRUSH bg_brush,
    int part_id,
    int state_id,
    RECT* rect,
    const TextFieldExtraParams& extra) const {
  static constexpr DTBGOPTS kOmitBorderOptions = {
      sizeof(DTBGOPTS), DTBG_OMITBORDER, {0, 0, 0, 0}};
  DrawThemeBackgroundEx(handle, hdc, part_id, state_id, rect,
                        extra.draw_edges ? nullptr : &kOmitBorderOptions);

  if (extra.fill_content_area) {
    RECT content_rect;
    GetThemeBackgroundContentRect(handle, hdc, part_id, state_id, rect,
                                  &content_rect);
    FillRect(hdc, &content_rect, bg_brush);
  }
}

void NativeThemeWin::PaintTextFieldClassic(
    HDC hdc,
    HBRUSH bg_brush,
    RECT* rect,
    const TextFieldExtraParams& extra) const {
  if (extra.draw_edges)
    DrawEdge(hdc, rect, EDGE_SUNKEN, BF_RECT | BF_ADJUST);

  if (extra.fill_content_area) {
    if (extra.classic_state & DFCS_INACTIVE)
      bg_brush = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    FillRect(hdc, rect, bg_brush);
  }
}

void NativeThemeWin::PaintScaledTheme(HANDLE theme,
                                      HDC hdc,
                                      int part_id,
                                      int state_id,
                                      const gfx::Rect& rect) const {
  // Correct the scaling and positioning of sub-components such as scrollbar
  // arrows and thumb grippers in the event that the world transform applies
  // scaling (e.g. in high-DPI mode).
  XFORM save_transform;
  if (GetWorldTransform(hdc, &save_transform)) {
    float scale = save_transform.eM11;
    if (scale != 1 && save_transform.eM12 == 0) {
      ModifyWorldTransform(hdc, NULL, MWT_IDENTITY);
      gfx::Rect scaled_rect = gfx::ScaleToEnclosedRect(rect, scale);
      scaled_rect.Offset(save_transform.eDx, save_transform.eDy);
      RECT bounds = scaled_rect.ToRECT();
      DrawThemeBackground(theme, hdc, part_id, state_id, &bounds, nullptr);
      SetWorldTransform(hdc, &save_transform);
      return;
    }
  }
  RECT bounds = rect.ToRECT();
  DrawThemeBackground(theme, hdc, part_id, state_id, &bounds, nullptr);
}

// static
NativeThemeWin::ThemeName NativeThemeWin::GetThemeName(Part part) {
  switch (part) {
    case kCheckbox:
    case kPushButton:
    case kRadio:
      return BUTTON;
    case kMenuList:
    case kMenuCheck:
    case kMenuCheckBackground:
    case kMenuPopupArrow:
    case kMenuPopupGutter:
    case kMenuPopupSeparator:
      return MENU;
    case kProgressBar:
      return PROGRESS;
    case kScrollbarDownArrow:
    case kScrollbarLeftArrow:
    case kScrollbarRightArrow:
    case kScrollbarUpArrow:
    case kScrollbarHorizontalGripper:
    case kScrollbarVerticalGripper:
    case kScrollbarHorizontalThumb:
    case kScrollbarVerticalThumb:
    case kScrollbarHorizontalTrack:
    case kScrollbarVerticalTrack:
      return SCROLLBAR;
    case kInnerSpinButton:
      return SPIN;
    case kWindowResizeGripper:
      return STATUS;
    case kTabPanelBackground:
      return TAB;
    case kTextField:
      return TEXTFIELD;
    case kTrackbarThumb:
    case kTrackbarTrack:
      return TRACKBAR;
    case kMenuPopupBackground:
    case kMenuItemBackground:
    case kScrollbarCorner:
    case kSliderTrack:
    case kSliderThumb:
    case kMaxPart:
      NOTREACHED_IN_MIGRATION();
  }
  return LAST;
}

// static
int NativeThemeWin::GetWindowsPart(Part part,
                                   State state,
                                   const ExtraParams& extra) {
  switch (part) {
    case kCheckbox:
      return BP_CHECKBOX;
    case kPushButton:
      return BP_PUSHBUTTON;
    case kRadio:
      return BP_RADIOBUTTON;
    case kMenuList:
      return CP_DROPDOWNBUTTON;
    case kTextField:
      return EP_EDITTEXT;
    case kMenuCheck:
      return MENU_POPUPCHECK;
    case kMenuCheckBackground:
      return MENU_POPUPCHECKBACKGROUND;
    case kMenuPopupGutter:
      return MENU_POPUPGUTTER;
    case kMenuPopupSeparator:
      return MENU_POPUPSEPARATOR;
    case kMenuPopupArrow:
      return MENU_POPUPSUBMENU;
    case kProgressBar:
      return PP_BAR;
    case kScrollbarDownArrow:
    case kScrollbarLeftArrow:
    case kScrollbarRightArrow:
    case kScrollbarUpArrow:
      return SBP_ARROWBTN;
    case kScrollbarHorizontalGripper:
      return SBP_GRIPPERHORZ;
    case kScrollbarVerticalGripper:
      return SBP_GRIPPERVERT;
    case kScrollbarHorizontalThumb:
      return SBP_THUMBBTNHORZ;
    case kScrollbarVerticalThumb:
      return SBP_THUMBBTNVERT;
    case kScrollbarHorizontalTrack:
      return absl::get<ScrollbarTrackExtraParams>(extra).is_upper
                 ? SBP_UPPERTRACKHORZ
                 : SBP_LOWERTRACKHORZ;
    case kScrollbarVerticalTrack:
      return absl::get<ScrollbarTrackExtraParams>(extra).is_upper
                 ? SBP_UPPERTRACKVERT
                 : SBP_LOWERTRACKVERT;
    case kWindowResizeGripper:
      // Use the status bar gripper.  There doesn't seem to be a standard
      // gripper in Windows for the space between scrollbars.  This is pretty
      // close, but it's supposed to be painted over a status bar.
      return SP_GRIPPER;
    case kInnerSpinButton:
      return absl::get<InnerSpinButtonExtraParams>(extra).spin_up ? SPNP_UP
                                                                  : SPNP_DOWN;
    case kTabPanelBackground:
      return TABP_BODY;
    case kTrackbarThumb:
      return absl::get<TrackbarExtraParams>(extra).vertical ? TKP_THUMBVERT
                                                            : TKP_THUMBBOTTOM;
    case kTrackbarTrack:
      return absl::get<TrackbarExtraParams>(extra).vertical ? TKP_TRACKVERT
                                                            : TKP_TRACK;
    case kMenuPopupBackground:
    case kMenuItemBackground:
    case kScrollbarCorner:
    case kSliderTrack:
    case kSliderThumb:
    case kMaxPart:
      NOTREACHED_IN_MIGRATION();
  }
  return 0;
}

int NativeThemeWin::GetWindowsState(Part part,
                                    State state,
                                    const ExtraParams& extra) {
  switch (part) {
    case kScrollbarDownArrow:
      switch (state) {
        case kDisabled:
          return ABS_DOWNDISABLED;
        case kHovered:
          return absl::get<ScrollbarArrowExtraParams>(extra).is_hovering
                     ? ABS_DOWNHOVER
                     : ABS_DOWNHOT;
        case kNormal:
          return ABS_DOWNNORMAL;
        case kPressed:
          return ABS_DOWNPRESSED;
        case kNumStates:
          NOTREACHED_IN_MIGRATION();
          return 0;
      }
    case kScrollbarLeftArrow:
      switch (state) {
        case kDisabled:
          return ABS_LEFTDISABLED;
        case kHovered:
          return absl::get<ScrollbarArrowExtraParams>(extra).is_hovering
                     ? ABS_LEFTHOVER
                     : ABS_LEFTHOT;
        case kNormal:
          return ABS_LEFTNORMAL;
        case kPressed:
          return ABS_LEFTPRESSED;
        case kNumStates:
          NOTREACHED_IN_MIGRATION();
          return 0;
      }
    case kScrollbarRightArrow:
      switch (state) {
        case kDisabled:
          return ABS_RIGHTDISABLED;
        case kHovered:
          return absl::get<ScrollbarArrowExtraParams>(extra).is_hovering
                     ? ABS_RIGHTHOVER
                     : ABS_RIGHTHOT;
        case kNormal:
          return ABS_RIGHTNORMAL;
        case kPressed:
          return ABS_RIGHTPRESSED;
        case kNumStates:
          NOTREACHED_IN_MIGRATION();
          return 0;
      }
    case kScrollbarUpArrow:
      switch (state) {
        case kDisabled:
          return ABS_UPDISABLED;
        case kHovered:
          return absl::get<ScrollbarArrowExtraParams>(extra).is_hovering
                     ? ABS_UPHOVER
                     : ABS_UPHOT;
        case kNormal:
          return ABS_UPNORMAL;
        case kPressed:
          return ABS_UPPRESSED;
        case kNumStates:
          NOTREACHED_IN_MIGRATION();
          return 0;
      }
    case kCheckbox: {
      const auto& button = absl::get<ButtonExtraParams>(extra);
      switch (state) {
        case kDisabled:
          return button.checked
                     ? CBS_CHECKEDDISABLED
                     : (button.indeterminate ? CBS_MIXEDDISABLED
                                             : CBS_UNCHECKEDDISABLED);
        case kHovered:
          return button.checked
                     ? CBS_CHECKEDHOT
                     : (button.indeterminate ? CBS_MIXEDHOT : CBS_UNCHECKEDHOT);
        case kNormal:
          return button.checked ? CBS_CHECKEDNORMAL
                                : (button.indeterminate ? CBS_MIXEDNORMAL
                                                        : CBS_UNCHECKEDNORMAL);
        case kPressed:
          return button.checked ? CBS_CHECKEDPRESSED
                                : (button.indeterminate ? CBS_MIXEDPRESSED
                                                        : CBS_UNCHECKEDPRESSED);
        case kNumStates:
          NOTREACHED_IN_MIGRATION();
          return 0;
      }
    }
    case kMenuList:
      switch (state) {
        case kDisabled:
          return CBXS_DISABLED;
        case kHovered:
          return CBXS_HOT;
        case kNormal:
          return CBXS_NORMAL;
        case kPressed:
          return CBXS_PRESSED;
        case kNumStates:
          NOTREACHED_IN_MIGRATION();
          return 0;
      }
    case kTextField:
      switch (state) {
        case kDisabled:
          return ETS_DISABLED;
        case kHovered:
          return ETS_HOT;
        case kNormal: {
          const auto& text_filed = absl::get<TextFieldExtraParams>(extra);
          if (text_filed.is_read_only) {
            return ETS_READONLY;
          }
          return text_filed.is_focused ? ETS_FOCUSED : ETS_NORMAL;
        }
        case kPressed:
          return ETS_SELECTED;
        case kNumStates:
          NOTREACHED_IN_MIGRATION();
          return 0;
      }
    case kMenuPopupArrow:
      return (state == kDisabled) ? MSM_DISABLED : MSM_NORMAL;
    case kMenuCheck: {
      const auto& menu_check = absl::get<MenuCheckExtraParams>(extra);
      if (state == kDisabled) {
        return menu_check.is_radio ? MC_BULLETDISABLED : MC_CHECKMARKDISABLED;
      }
      return menu_check.is_radio ? MC_BULLETNORMAL : MC_CHECKMARKNORMAL;
    }
    case kMenuCheckBackground:
      return (state == kDisabled) ? MCB_DISABLED : MCB_NORMAL;
    case kPushButton:
      switch (state) {
        case kDisabled:
          return PBS_DISABLED;
        case kHovered:
          return PBS_HOT;
        case kNormal:
          return absl::get<ButtonExtraParams>(extra).is_default ? PBS_DEFAULTED
                                                                : PBS_NORMAL;
        case kPressed:
          return PBS_PRESSED;
        case kNumStates:
          NOTREACHED_IN_MIGRATION();
          return 0;
      }
    case kRadio: {
      const auto& button = absl::get<ButtonExtraParams>(extra);
      switch (state) {
        case kDisabled:
          return button.checked ? RBS_CHECKEDDISABLED : RBS_UNCHECKEDDISABLED;
        case kHovered:
          return button.checked ? RBS_CHECKEDHOT : RBS_UNCHECKEDHOT;
        case kNormal:
          return button.checked ? RBS_CHECKEDNORMAL : RBS_UNCHECKEDNORMAL;
        case kPressed:
          return button.checked ? RBS_CHECKEDPRESSED : RBS_UNCHECKEDPRESSED;
        case kNumStates:
          NOTREACHED_IN_MIGRATION();
          return 0;
      }
    }
    case kScrollbarHorizontalGripper:
    case kScrollbarVerticalGripper:
    case kScrollbarHorizontalThumb:
    case kScrollbarVerticalThumb:
      if ((state == kHovered) &&
          !absl::get<ScrollbarThumbExtraParams>(extra).is_hovering) {
        return SCRBS_HOT;
      }
      [[fallthrough]];
    case kScrollbarHorizontalTrack:
    case kScrollbarVerticalTrack:
      switch (state) {
        case kDisabled:
          return SCRBS_DISABLED;
        case kHovered:
          return SCRBS_HOVER;
        case kNormal:
          return SCRBS_NORMAL;
        case kPressed:
          return SCRBS_PRESSED;
        case kNumStates:
          NOTREACHED_IN_MIGRATION();
          return 0;
      }
    case kTrackbarThumb:
    case kTrackbarTrack:
      switch (state) {
        case kDisabled:
          return TUS_DISABLED;
        case kHovered:
          return TUS_HOT;
        case kNormal:
          return TUS_NORMAL;
        case kPressed:
          return TUS_PRESSED;
        case kNumStates:
          NOTREACHED_IN_MIGRATION();
          return 0;
      }
    case kInnerSpinButton: {
      const auto& inner_spin = absl::get<InnerSpinButtonExtraParams>(extra);
      switch (state) {
        case kDisabled:
          return inner_spin.spin_up ? static_cast<int>(UPS_DISABLED)
                                    : static_cast<int>(DNS_DISABLED);
        case kHovered:
          return inner_spin.spin_up ? static_cast<int>(UPS_HOT)
                                    : static_cast<int>(DNS_HOT);
        case kNormal:
          return inner_spin.spin_up ? static_cast<int>(UPS_NORMAL)
                                    : static_cast<int>(DNS_NORMAL);
        case kPressed:
          return inner_spin.spin_up ? static_cast<int>(UPS_PRESSED)
                                    : static_cast<int>(DNS_PRESSED);
        case kNumStates:
          NOTREACHED_IN_MIGRATION();
          return 0;
      }
    }
    case kMenuPopupGutter:
    case kMenuPopupSeparator:
    case kProgressBar:
    case kTabPanelBackground:
    case kWindowResizeGripper:
      switch (state) {
        case kDisabled:
        case kHovered:
        case kNormal:
        case kPressed:
          return 0;
        case kNumStates:
          NOTREACHED_IN_MIGRATION();
          return 0;
      }
    case kMenuPopupBackground:
    case kMenuItemBackground:
    case kScrollbarCorner:
    case kSliderTrack:
    case kSliderThumb:
    case kMaxPart:
      NOTREACHED_IN_MIGRATION();
  }
  return 0;
}

HRESULT NativeThemeWin::PaintFrameControl(HDC hdc,
                                          const gfx::Rect& rect,
                                          UINT type,
                                          UINT state,
                                          bool is_selected,
                                          State control_state) const {
  const int width = rect.width();
  const int height = rect.height();

  // DrawFrameControl for menu arrow/check wants a monochrome bitmap.
  base::win::ScopedBitmap mask_bitmap(CreateBitmap(width, height, 1, 1, NULL));

  if (mask_bitmap == NULL)
    return E_OUTOFMEMORY;

  base::win::ScopedCreateDC bitmap_dc(CreateCompatibleDC(NULL));
  base::win::ScopedSelectObject select_bitmap(bitmap_dc.Get(),
                                              mask_bitmap.get());
  RECT local_rect = { 0, 0, width, height };
  DrawFrameControl(bitmap_dc.Get(), &local_rect, type, state);

  // We're going to use BitBlt with a b&w mask. This results in using the dest
  // dc's text color for the black bits in the mask, and the dest dc's
  // background color for the white bits in the mask. DrawFrameControl draws the
  // check in black, and the background in white.
  int bg_color_key = COLOR_MENU;
  int text_color_key = COLOR_MENUTEXT;
  switch (control_state) {
    case kDisabled:
      bg_color_key = is_selected ? COLOR_HIGHLIGHT : COLOR_MENU;
      text_color_key = COLOR_GRAYTEXT;
      break;
    case kHovered:
      bg_color_key = COLOR_HIGHLIGHT;
      text_color_key = COLOR_HIGHLIGHTTEXT;
      break;
    case kNormal:
      break;
    case kPressed:
    case kNumStates:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  COLORREF old_bg_color = SetBkColor(hdc, GetSysColor(bg_color_key));
  COLORREF old_text_color = SetTextColor(hdc, GetSysColor(text_color_key));
  BitBlt(hdc, rect.x(), rect.y(), width, height, bitmap_dc.Get(), 0, 0,
         SRCCOPY);
  SetBkColor(hdc, old_bg_color);
  SetTextColor(hdc, old_text_color);

  return S_OK;
}

HANDLE NativeThemeWin::GetThemeHandle(ThemeName theme_name) const {
  if (theme_name < 0 || theme_name >= LAST)
    return nullptr;

  if (theme_handles_[theme_name])
    return theme_handles_[theme_name];

  // Not found, try to load it.
  HANDLE handle = nullptr;
  switch (theme_name) {
  case BUTTON:
    handle = OpenThemeData(nullptr, L"Button");
    break;
  case LIST:
    handle = OpenThemeData(nullptr, L"Listview");
    break;
  case MENU:
    handle = OpenThemeData(nullptr, L"Menu");
    break;
  case MENULIST:
    handle = OpenThemeData(nullptr, L"Combobox");
    break;
  case SCROLLBAR:
    handle = OpenThemeData(nullptr, supports_windows_dark_mode_
                                        ? L"Explorer::Scrollbar"
                                        : L"Scrollbar");
    break;
  case STATUS:
    handle = OpenThemeData(nullptr, L"Status");
    break;
  case TAB:
    handle = OpenThemeData(nullptr, L"Tab");
    break;
  case TEXTFIELD:
    handle = OpenThemeData(nullptr, L"Edit");
    break;
  case TRACKBAR:
    handle = OpenThemeData(nullptr, L"Trackbar");
    break;
  case WINDOW:
    handle = OpenThemeData(nullptr, L"Window");
    break;
  case PROGRESS:
    handle = OpenThemeData(nullptr, L"Progress");
    break;
  case SPIN:
    handle = OpenThemeData(nullptr, L"Spin");
    break;
  case LAST:
    NOTREACHED_IN_MIGRATION();
    break;
  }
  theme_handles_[theme_name] = handle;
  return handle;
}

void NativeThemeWin::RegisterThemeRegkeyObserver() {
  DCHECK(hkcu_themes_regkey_.Valid());
  DCHECK(base::SequencedTaskRunner::HasCurrentDefault());
  hkcu_themes_regkey_.StartWatching(base::BindOnce(
      [](NativeThemeWin* native_theme) {
        native_theme->UpdateDarkModeStatus();
        native_theme->UpdatePrefersReducedTransparency();
        // RegKey::StartWatching only provides one notification. Reregistration
        // is required to get future notifications.
        native_theme->RegisterThemeRegkeyObserver();
      },
      base::Unretained(this)));
}

void NativeThemeWin::RegisterColorFilteringRegkeyObserver() {
  DCHECK(hkcu_color_filtering_regkey_.Valid());
  DCHECK(base::SequencedTaskRunner::HasCurrentDefault());
  hkcu_color_filtering_regkey_.StartWatching(base::BindOnce(
      [](NativeThemeWin* native_theme) {
        native_theme->UpdateInvertedColors();
        // RegKey::StartWatching only provides one notification. Reregistration
        // is required to get future notifications.
        native_theme->RegisterColorFilteringRegkeyObserver();
      },
      base::Unretained(this)));
}

void NativeThemeWin::UpdateDarkModeStatus() {
  bool dark_mode_enabled = false;
  if (hkcu_themes_regkey_.Valid()) {
    DWORD apps_use_light_theme = 1;
    hkcu_themes_regkey_.ReadValueDW(L"AppsUseLightTheme",
                                    &apps_use_light_theme);
    dark_mode_enabled = (apps_use_light_theme == 0);
  }
  set_use_dark_colors(dark_mode_enabled);
  set_preferred_color_scheme(CalculatePreferredColorScheme());
  CloseHandlesInternal();
  NotifyOnNativeThemeUpdated();
}

void NativeThemeWin::UpdatePrefersReducedTransparency() {
  bool prefers_reduced_transparency = false;
  if (hkcu_themes_regkey_.Valid()) {
    DWORD enable_transparency = 1;
    hkcu_themes_regkey_.ReadValueDW(L"EnableTransparency",
                                    &enable_transparency);
    prefers_reduced_transparency = (enable_transparency == 0);
  }
  set_prefers_reduced_transparency(prefers_reduced_transparency);
  CloseHandlesInternal();
  NotifyOnNativeThemeUpdated();
}

void NativeThemeWin::UpdateInvertedColors() {
  bool inverted_colors = false;
  if (hkcu_color_filtering_regkey_.Valid()) {
    DWORD active = 0;
    hkcu_color_filtering_regkey_.ReadValueDW(L"Active", &active);
    if (active == 1) {
    // 0 = Greyscale
    // 1 = Invert
    // 2 = Greyscale Inverted
    // 3 = Deuteranopia
    // 4 = Protanopia
    // 5 = Tritanopia
    DWORD filter_type = 0;
    hkcu_color_filtering_regkey_.ReadValueDW(L"FilterType", &filter_type);
    inverted_colors = (filter_type == 1);
    }
  }
  set_inverted_colors(inverted_colors);
  CloseHandlesInternal();
  NotifyOnNativeThemeUpdated();
}

}  // namespace ui
