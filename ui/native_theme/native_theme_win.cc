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

#include <array>
#include <optional>
#include <tuple>
#include <utility>
#include <variant>

#include "base/check.h"
#include "base/containers/fixed_flat_map.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/win/scoped_gdi_object.h"
#include "base/win/scoped_hdc.h"
#include "base/win/scoped_select_object.h"
#include "base/win/win_util.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"
#include "skia/ext/skia_utils_win.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkBlendMode.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSamplingOptions.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkTileMode.h"
#include "third_party/skia/include/private/chromium/SkPMColor.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/color/win/accent_color_observer.h"
#include "ui/color/win/native_color_mixers_win.h"
#include "ui/display/win/screen_win.h"
#include "ui/gfx/color_conversions.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/os_settings_provider.h"

namespace ui {

namespace {

void SetCheckerboardShader(SkPaint* paint, int left, int top) {
  // Create a 2x2 checkerboard pattern using the 3D face and highlight colors.
  const auto& os_settings_provider = OsSettingsProvider::Get();
  using enum OsSettingsProvider::ColorId;
  const SkColor face = os_settings_provider.Color(kButtonFace)
                           .value_or(SkColorSetRGB(0xC0, 0xC0, 0xC0));
  const SkColor highlight =
      os_settings_provider.Color(kButtonHighlight).value_or(SK_ColorWHITE);
  SkColor buffer[] = {face, highlight, highlight, face};
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
  if (bitmap.tryAllocPixels(info)) {
    temp_bitmap.readPixels(info, bitmap.getPixels(), bitmap.rowBytes(), 0, 0);
  }

  // Align the pattern with the upper corner of |align_rect|.
  SkMatrix local_matrix;
  local_matrix.setTranslate(SkIntToScalar(left), SkIntToScalar(top));
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
  bool SelectBitmap(HBITMAP handle) {
    bitmap_.reset(handle);
    if (!bitmap_.is_valid()) {
      return false;
    }

    SelectObject(dc_.Get(), bitmap_.get());
    return true;
  }

 private:
  base::win::ScopedCreateDC dc_;
  base::win::ScopedGDIObject<HBITMAP> bitmap_;
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

enum ThemeName {
  BUTTON,
  LIST,
  MENU,
  MENULIST,
  SCROLLBAR,
  STATUS,
  TAB,
  TEXTFIELD,
  TRACKBAR,
  WINDOW,
  PROGRESS,
  SPIN,
  LAST
};

// Returns the cached open theme handles. This is process-scoped rather than
// owned by `NativeThemeWin` so that, in case two `NativeThemeWin` instances are
// live simultaneously (e.g. in tests), destroying one (and thus calling
// `CloseThemeData()` on all its handles) won't leave the other holding invalid
// handles.
HANDLE* GetThemeHandles() {
  static HANDLE kThemeHandles[LAST] = {};
  return kThemeHandles;
}

ThemeName GetThemeName(NativeTheme::Part part) {
  switch (part) {
    case NativeTheme::kCheckbox:
    case NativeTheme::kPushButton:
    case NativeTheme::kRadio:
      return BUTTON;
    case NativeTheme::kMenuList:
    case NativeTheme::kMenuCheck:
    case NativeTheme::kMenuCheckBackground:
    case NativeTheme::kMenuPopupArrow:
    case NativeTheme::kMenuPopupGutter:
    case NativeTheme::kMenuPopupSeparator:
      return MENU;
    case NativeTheme::kProgressBar:
      return PROGRESS;
    case NativeTheme::kScrollbarDownArrow:
    case NativeTheme::kScrollbarLeftArrow:
    case NativeTheme::kScrollbarRightArrow:
    case NativeTheme::kScrollbarUpArrow:
    case NativeTheme::kScrollbarHorizontalGripper:
    case NativeTheme::kScrollbarVerticalGripper:
    case NativeTheme::kScrollbarHorizontalThumb:
    case NativeTheme::kScrollbarVerticalThumb:
    case NativeTheme::kScrollbarHorizontalTrack:
    case NativeTheme::kScrollbarVerticalTrack:
      return SCROLLBAR;
    case NativeTheme::kInnerSpinButton:
      return SPIN;
    case NativeTheme::kWindowResizeGripper:
      return STATUS;
    case NativeTheme::kTabPanelBackground:
      return TAB;
    case NativeTheme::kTextField:
      return TEXTFIELD;
    case NativeTheme::kTrackbarThumb:
    case NativeTheme::kTrackbarTrack:
      return TRACKBAR;
    case NativeTheme::kMenuPopupBackground:
    case NativeTheme::kMenuItemBackground:
    case NativeTheme::kScrollbarCorner:
    case NativeTheme::kSliderTrack:
    case NativeTheme::kSliderThumb:
    case NativeTheme::kMaxPart:
      NOTREACHED();
  }
}

// Returns the theme handle for `part`, opening it if necessary.
HANDLE GetThemeHandle(ThemeName theme_name) {
  if (theme_name < 0 || theme_name >= LAST) {
    return nullptr;
  }

  HANDLE* theme_handles = GetThemeHandles();
  if (theme_handles[theme_name]) {
    return theme_handles[theme_name];
  }

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
      handle = OpenThemeData(
          nullptr, OsSettingsProvider::Get().DarkColorSchemeAvailable()
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
      NOTREACHED();
  }
  theme_handles[theme_name] = handle;
  return handle;
}

int GetWindowsPart(NativeTheme::Part part,
                   NativeTheme::State state,
                   const NativeTheme::ExtraParams& extra_params) {
  if (part == NativeTheme::kScrollbarHorizontalTrack) {
    return std::get<NativeTheme::ScrollbarTrackExtraParams>(extra_params)
                   .is_upper
               ? SBP_UPPERTRACKHORZ
               : SBP_LOWERTRACKHORZ;
  }
  if (part == NativeTheme::kScrollbarVerticalTrack) {
    return std::get<NativeTheme::ScrollbarTrackExtraParams>(extra_params)
                   .is_upper
               ? SBP_UPPERTRACKVERT
               : SBP_LOWERTRACKVERT;
  }
  if (part == NativeTheme::kInnerSpinButton) {
    return std::get<NativeTheme::InnerSpinButtonExtraParams>(extra_params)
                   .spin_up
               ? SPNP_UP
               : SPNP_DOWN;
  }
  if (part == NativeTheme::kTrackbarThumb) {
    return std::get<NativeTheme::TrackbarExtraParams>(extra_params).vertical
               ? TKP_THUMBVERT
               : TKP_THUMBBOTTOM;
  }
  if (part == NativeTheme::kTrackbarTrack) {
    return std::get<NativeTheme::TrackbarExtraParams>(extra_params).vertical
               ? TKP_TRACKVERT
               : TKP_TRACK;
  }

  static constexpr auto kPartMap =
      base::MakeFixedFlatMap<NativeTheme::Part, int>(
          {{NativeTheme::kCheckbox, BP_CHECKBOX},
           {NativeTheme::kPushButton, BP_PUSHBUTTON},
           {NativeTheme::kRadio, BP_RADIOBUTTON},
           {NativeTheme::kMenuList, CP_DROPDOWNBUTTON},
           {NativeTheme::kTextField, EP_EDITTEXT},
           {NativeTheme::kMenuCheck, MENU_POPUPCHECK},
           {NativeTheme::kMenuCheckBackground, MENU_POPUPCHECKBACKGROUND},
           {NativeTheme::kMenuPopupGutter, MENU_POPUPGUTTER},
           {NativeTheme::kMenuPopupSeparator, MENU_POPUPSEPARATOR},
           {NativeTheme::kMenuPopupArrow, MENU_POPUPSUBMENU},
           {NativeTheme::kProgressBar, PP_BAR},
           {NativeTheme::kScrollbarDownArrow, SBP_ARROWBTN},
           {NativeTheme::kScrollbarLeftArrow, SBP_ARROWBTN},
           {NativeTheme::kScrollbarRightArrow, SBP_ARROWBTN},
           {NativeTheme::kScrollbarUpArrow, SBP_ARROWBTN},
           {NativeTheme::kScrollbarHorizontalGripper, SBP_GRIPPERHORZ},
           {NativeTheme::kScrollbarVerticalGripper, SBP_GRIPPERVERT},
           {NativeTheme::kScrollbarHorizontalThumb, SBP_THUMBBTNHORZ},
           {NativeTheme::kScrollbarVerticalThumb, SBP_THUMBBTNVERT},
           {NativeTheme::kWindowResizeGripper,
            // Use the status bar gripper.  There doesn't seem to be a standard
            // gripper in Windows for the space between scrollbars.  This is
            // pretty close, but it's supposed to be painted over a status bar.
            SP_GRIPPER},
           {NativeTheme::kTabPanelBackground, TABP_BODY}});
  return kPartMap.at(part);
}

int GetWindowsState(NativeTheme::Part part,
                    NativeTheme::State state,
                    const NativeTheme::ExtraParams& extra_params) {
  if (part >= NativeTheme::kScrollbarDownArrow &&
      part <= NativeTheme::kScrollbarUpArrow &&
      state == NativeTheme::kHovered &&
      std::get<NativeTheme::ScrollbarArrowExtraParams>(extra_params)
          .is_hovering) {
    return std::to_array(
        {ABS_DOWNHOVER, ABS_LEFTHOVER, ABS_RIGHTHOVER,
         ABS_UPHOVER})[part - NativeTheme::kScrollbarDownArrow];
  }
  switch (part) {
    case NativeTheme::kScrollbarDownArrow:
      return std::to_array({ABS_DOWNDISABLED, ABS_DOWNHOT, ABS_DOWNNORMAL,
                            ABS_DOWNPRESSED})[state];
    case NativeTheme::kScrollbarLeftArrow:
      return std::to_array({ABS_LEFTDISABLED, ABS_LEFTHOT, ABS_LEFTNORMAL,
                            ABS_LEFTPRESSED})[state];
    case NativeTheme::kScrollbarRightArrow:
      return std::to_array({ABS_RIGHTDISABLED, ABS_RIGHTHOT, ABS_RIGHTNORMAL,
                            ABS_RIGHTPRESSED})[state];
    case NativeTheme::kScrollbarUpArrow:
      return std::to_array(
          {ABS_UPDISABLED, ABS_UPHOT, ABS_UPNORMAL, ABS_UPPRESSED})[state];
    case NativeTheme::kCheckbox: {
      const auto& button =
          std::get<NativeTheme::ButtonExtraParams>(extra_params);
      if (button.checked) {
        return std::to_array({CBS_CHECKEDDISABLED, CBS_CHECKEDHOT,
                              CBS_CHECKEDNORMAL, CBS_CHECKEDPRESSED})[state];
      }
      if (button.indeterminate) {
        return std::to_array({CBS_MIXEDDISABLED, CBS_MIXEDHOT, CBS_MIXEDNORMAL,
                              CBS_MIXEDPRESSED})[state];
      }
      return std::to_array({CBS_UNCHECKEDDISABLED, CBS_UNCHECKEDHOT,
                            CBS_UNCHECKEDNORMAL, CBS_UNCHECKEDPRESSED})[state];
    }
    case NativeTheme::kMenuList:
      return std::to_array(
          {CBXS_DISABLED, CBXS_HOT, CBXS_NORMAL, CBXS_PRESSED})[state];
    case NativeTheme::kTextField:
      if (state == NativeTheme::kNormal) {
        const auto& text_field =
            std::get<NativeTheme::TextFieldExtraParams>(extra_params);
        if (text_field.is_read_only) {
          return ETS_READONLY;
        }
        if (text_field.is_focused) {
          return ETS_FOCUSED;
        }
      }
      return std::to_array(
          {ETS_DISABLED, ETS_HOT, ETS_NORMAL, ETS_SELECTED})[state];
    case NativeTheme::kMenuPopupArrow:
      return (state == NativeTheme::kDisabled) ? MSM_DISABLED : MSM_NORMAL;
    case NativeTheme::kMenuCheck:
      if (std::get<NativeTheme::MenuCheckExtraParams>(extra_params).is_radio) {
        return (state == NativeTheme::kDisabled) ? MC_BULLETDISABLED
                                                 : MC_BULLETNORMAL;
      }
      return (state == NativeTheme::kDisabled) ? MC_CHECKMARKDISABLED
                                               : MC_CHECKMARKNORMAL;
    case NativeTheme::kMenuCheckBackground:
      return (state == NativeTheme::kDisabled) ? MCB_DISABLED : MCB_NORMAL;
    case NativeTheme::kPushButton:
      if (state == NativeTheme::kNormal &&
          std::get<NativeTheme::ButtonExtraParams>(extra_params).is_default) {
        return PBS_DEFAULTED;
      }
      return std::to_array(
          {PBS_DISABLED, PBS_HOT, PBS_NORMAL, PBS_PRESSED})[state];
    case NativeTheme::kRadio:
      if (std::get<NativeTheme::ButtonExtraParams>(extra_params).checked) {
        return std::to_array({RBS_CHECKEDDISABLED, RBS_CHECKEDHOT,
                              RBS_CHECKEDNORMAL, RBS_CHECKEDPRESSED})[state];
      }
      return std::to_array({RBS_UNCHECKEDDISABLED, RBS_UNCHECKEDHOT,
                            RBS_UNCHECKEDNORMAL, RBS_UNCHECKEDPRESSED})[state];
    case NativeTheme::kScrollbarHorizontalGripper:
    case NativeTheme::kScrollbarVerticalGripper:
    case NativeTheme::kScrollbarHorizontalThumb:
    case NativeTheme::kScrollbarVerticalThumb:
      if ((state == NativeTheme::kHovered) &&
          !std::get<NativeTheme::ScrollbarThumbExtraParams>(extra_params)
               .is_hovering) {
        return SCRBS_HOT;
      }
      [[fallthrough]];
    case NativeTheme::kScrollbarHorizontalTrack:
    case NativeTheme::kScrollbarVerticalTrack:
      return std::to_array(
          {SCRBS_DISABLED, SCRBS_HOVER, SCRBS_NORMAL, SCRBS_PRESSED})[state];
    case NativeTheme::kTrackbarThumb:
    case NativeTheme::kTrackbarTrack:
      return std::to_array(
          {TUS_DISABLED, TUS_HOT, TUS_NORMAL, TUS_PRESSED})[state];
    case NativeTheme::kInnerSpinButton:
      if (std::get<NativeTheme::InnerSpinButtonExtraParams>(extra_params)
              .spin_up) {
        return std::to_array(
            {UPS_DISABLED, UPS_HOT, UPS_NORMAL, UPS_PRESSED})[state];
      }
      return std::to_array(
          {DNS_DISABLED, DNS_HOT, DNS_NORMAL, DNS_PRESSED})[state];
    case NativeTheme::kMenuPopupGutter:
    case NativeTheme::kMenuPopupSeparator:
    case NativeTheme::kProgressBar:
    case NativeTheme::kTabPanelBackground:
    case NativeTheme::kWindowResizeGripper:
      return 0;
    case NativeTheme::kMenuPopupBackground:
    case NativeTheme::kMenuItemBackground:
    case NativeTheme::kScrollbarCorner:
    case NativeTheme::kSliderTrack:
    case NativeTheme::kSliderThumb:
    case NativeTheme::kMaxPart:
      NOTREACHED();
  }
}

void PaintMenuSeparator(
    cc::PaintCanvas* canvas,
    const ColorProvider* color_provider,
    const NativeTheme::MenuSeparatorExtraParams& extra_params) {
  CHECK(color_provider);
  const gfx::RectF rect(*extra_params.paint_rect);
  gfx::PointF start = rect.CenterPoint();
  gfx::PointF end = start;
  if (extra_params.type == ui::VERTICAL_SEPARATOR) {
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

void PaintMenuGutter(cc::PaintCanvas* canvas,
                     const ColorProvider* color_provider,
                     const gfx::Rect& rect) {
  CHECK(color_provider);
  cc::PaintFlags flags;
  flags.setColor(color_provider->GetColor(kColorMenuSeparator));
  const int center_x = rect.x() + rect.width() / 2;
  canvas->drawLine(center_x, rect.y(), center_x, rect.bottom(), flags);
}

void PaintMenuBackground(cc::PaintCanvas* canvas,
                         const ColorProvider* color_provider,
                         const gfx::Rect& rect) {
  CHECK(color_provider);
  cc::PaintFlags flags;
  flags.setColor(color_provider->GetColor(kColorMenuBackground));
  canvas->drawRect(gfx::RectToSkRect(rect), flags);
}

void PaintButtonClassic(HDC hdc,
                        NativeTheme::Part part,
                        NativeTheme::State state,
                        RECT* rect,
                        const NativeTheme::ButtonExtraParams& extra_params) {
  if ((part == NativeTheme::kPushButton) &&
      ((state == NativeTheme::kPressed) || extra_params.is_default)) {
    // Pressed or defaulted buttons have a shadow replacing the outer 1 px.
    HBRUSH brush = GetSysColorBrush(COLOR_3DDKSHADOW);
    if (brush) {
      FrameRect(hdc, rect, brush);
      InflateRect(rect, -1, -1);
    }
  }

  int classic_state = extra_params.classic_state;
  if (part == NativeTheme::kCheckbox) {
    classic_state |= DFCS_BUTTONCHECK;
  } else if (part == NativeTheme::kPushButton) {
    classic_state |= DFCS_BUTTONRADIO;
  } else if (part == NativeTheme::kRadio) {
    classic_state |= DFCS_BUTTONPUSH;
  }
  if (state == NativeTheme::kDisabled) {
    classic_state |= DFCS_INACTIVE;
  } else if (state == NativeTheme::kPressed) {
    classic_state |= DFCS_PUSHED;
  }
  if (extra_params.checked) {
    classic_state |= DFCS_CHECKED;
  }
  DrawFrameControl(hdc, rect, DFC_BUTTON, classic_state);

  // Draw a focus rectangle (the dotted line box) on defaulted buttons.
  if ((part == NativeTheme::kPushButton) && extra_params.is_default) {
    InflateRect(rect, -GetSystemMetrics(SM_CXEDGE),
                -GetSystemMetrics(SM_CYEDGE));
    DrawFocusRect(hdc, rect);
  }

  // Classic theme doesn't support indeterminate checkboxes.  We draw a
  // rectangle inside a checkbox like IE10 does.
  if ((part == NativeTheme::kCheckbox) && extra_params.indeterminate) {
    RECT inner_rect = *rect;
    // "4 / 13" matches IE10 in classic theme.
    const int padding = (inner_rect.right - inner_rect.left) * 4 / 13;
    InflateRect(&inner_rect, -padding, -padding);
    FillRect(
        hdc, &inner_rect,
        GetSysColorBrush((state == NativeTheme::kDisabled) ? COLOR_GRAYTEXT
                                                           : COLOR_WINDOWTEXT));
  }
}

void PaintLeftMenuArrowThemed(HDC hdc,
                              HANDLE handle,
                              int part_id,
                              int state_id,
                              const gfx::Rect& rect) {
  // There is no way to tell the uxtheme API to draw a left pointing arrow; it
  // doesn't have a flag equivalent to `DFCS_MENUARROWRIGHT`.  But they are
  // needed for RTL locales on Vista.  So use a memory DC and mirror the region
  // with GDI's `StretchBlt()`.
  base::win::ScopedCreateDC mem_dc(CreateCompatibleDC(hdc));
  base::win::ScopedGDIObject<HBITMAP> mem_bitmap(
      CreateCompatibleBitmap(hdc, rect.width(), rect.height()));
  base::win::ScopedSelectObject select_bitmap(mem_dc.Get(), mem_bitmap.get());
  // Copy and horizontally mirror the background from hdc into mem_dc. Use a
  // negative-width source rect, starting at the rightmost pixel.
  StretchBlt(mem_dc.Get(), 0, 0, rect.width(), rect.height(), hdc,
             rect.right() - 1, rect.y(), -rect.width(), rect.height(), SRCCOPY);
  // Draw the arrow.
  RECT theme_rect = {
      .left = 0, .top = 0, .right = rect.width(), .bottom = rect.height()};
  DrawThemeBackground(handle, mem_dc.Get(), part_id, state_id, &theme_rect,
                      nullptr);
  // Copy and mirror the result back into mem_dc.
  StretchBlt(hdc, rect.x(), rect.y(), rect.width(), rect.height(), mem_dc.Get(),
             rect.width() - 1, 0, -rect.width(), rect.height(), SRCCOPY);
}

void PaintScrollbarArrowClassic(HDC hdc,
                                NativeTheme::Part part,
                                NativeTheme::State state,
                                RECT* rect) {
  static constexpr auto kParts = std::to_array(
      {DFCS_SCROLLDOWN, DFCS_SCROLLLEFT, DFCS_SCROLLRIGHT, DFCS_SCROLLUP});
  static constexpr auto kStates =
      std::to_array({DFCS_INACTIVE, DFCS_HOT, 0, DFCS_PUSHED});
  DrawFrameControl(
      hdc, rect, DFC_SCROLL,
      kParts[part - NativeTheme::kScrollbarDownArrow] | kStates[state]);
}

void PaintScrollbarTrackClassic(
    SkCanvas* canvas,
    HDC hdc,
    RECT* rect,
    const NativeTheme::ScrollbarTrackExtraParams& extra_params) {
  const auto& os_settings_provider = OsSettingsProvider::Get();
  using enum OsSettingsProvider::ColorId;
  if (const auto scrollbar_color = os_settings_provider.Color(kScrollbar);
      (scrollbar_color != os_settings_provider.Color(kButtonFace)) &&
      (scrollbar_color != os_settings_provider.Color(kWindow))) {
    FillRect(hdc, rect, reinterpret_cast<HBRUSH>(COLOR_SCROLLBAR + 1));
  } else {
    SkPaint paint;
    SetCheckerboardShader(&paint, extra_params.track_x, extra_params.track_y);
    canvas->drawIRect(skia::RECTToSkIRect(*rect), paint);
  }
  if (extra_params.classic_state & DFCS_PUSHED) {
    InvertRect(hdc, rect);
  }
}

void PaintHorizontalTrackbarThumbClassic(
    SkCanvas* canvas,
    HDC hdc,
    const RECT& rect,
    const NativeTheme::TrackbarExtraParams& extra_params) {
  // Split rect into top and bottom pieces.
  const int half_thickness = (rect.right - rect.left) / 2;
  RECT top_section = rect;
  RECT bottom_section = rect;
  top_section.bottom -= half_thickness;
  bottom_section.top = top_section.bottom;
  DrawEdge(hdc, &top_section, EDGE_RAISED,
           BF_LEFT | BF_TOP | BF_RIGHT | BF_SOFT | BF_MIDDLE | BF_ADJUST);

  // Split bottom piece into two halves.
  RECT left_half = bottom_section;
  RECT right_half = bottom_section;
  right_half.left += half_thickness;
  left_half.right = right_half.left;
  DrawEdge(hdc, &left_half, EDGE_RAISED,
           BF_DIAGONAL_ENDTOPLEFT | BF_SOFT | BF_MIDDLE | BF_ADJUST);
  DrawEdge(hdc, &right_half, EDGE_RAISED,
           BF_DIAGONAL_ENDBOTTOMLEFT | BF_SOFT | BF_MIDDLE | BF_ADJUST);

  if (!(extra_params.classic_state & DFCS_PUSHED)) {
    return;
  }

  // Draw hatching.
  SkPaint paint;
  SetCheckerboardShader(&paint, rect.left, rect.top);

  // Fill all three pieces with the pattern.
  canvas->drawIRect(skia::RECTToSkIRect(top_section), paint);

  const SkScalar left_triangle_top = SkIntToScalar(left_half.top);
  const SkScalar left_triangle_right = SkIntToScalar(left_half.right);
  SkPath left_triangle;
  left_triangle.moveTo(SkIntToScalar(left_half.left), left_triangle_top);
  left_triangle.lineTo(left_triangle_right, left_triangle_top);
  left_triangle.lineTo(left_triangle_right, SkIntToScalar(left_half.bottom));
  left_triangle.close();
  canvas->drawPath(left_triangle, paint);

  const SkScalar right_triangle_left = SkIntToScalar(right_half.left);
  const SkScalar right_triangle_top = SkIntToScalar(right_half.top);
  SkPath right_triangle;
  right_triangle.moveTo(right_triangle_left, right_triangle_top);
  right_triangle.lineTo(SkIntToScalar(right_half.right), right_triangle_top);
  right_triangle.lineTo(right_triangle_left, SkIntToScalar(right_half.bottom));
  right_triangle.close();
  canvas->drawPath(right_triangle, paint);
}

void PaintProgressBarOverlayThemed(
    HDC hdc,
    HANDLE handle,
    RECT* bar_rect,
    RECT* value_rect,
    const NativeTheme::ProgressBarExtraParams& extra_params) {
  // There is no documentation about the animation of the indeterminate progress
  // bar. The following are guesses based on observing other programs.
  constexpr int kOverlayWidth = 120;
  const int pixels_per_second = extra_params.determinate ? 300 : 175;

  int bar_width = bar_rect->right - bar_rect->left;
  if (!extra_params.determinate) {
    // The glossy overlay for the indeterminate progress bar has a small pause
    // after each animation. We emulate this by adding an invisible margin the
    // animation has to traverse.
    bar_width += pixels_per_second;
    RECT overlay_rect = *bar_rect;
    overlay_rect.left +=
        ComputeAnimationProgress(bar_width, kOverlayWidth, pixels_per_second,
                                 extra_params.animated_seconds);
    overlay_rect.right = overlay_rect.left + kOverlayWidth;
    DrawThemeBackground(handle, hdc, PP_MOVEOVERLAY, 0, &overlay_rect,
                        bar_rect);
    return;
  }

  // We care about the direction here because PP_CHUNK painting is asymmetric.
  // TODO(morrita): This RTL guess can be wrong.  We should pass in the
  // direction from WebKit.
  const bool mirror = bar_rect->right == value_rect->right &&
                      bar_rect->left != value_rect->left;
  const DTBGOPTS value_draw_options = {
      .dwSize = sizeof(DTBGOPTS),
      .dwFlags = static_cast<DWORD>(mirror ? DTBG_MIRRORDC : 0),
      .rcClip = *bar_rect};

  // On Vista or later, the progress bar part has a single-block value part
  // and a glossy effect. The value part has exactly same height as the bar
  // part, so we don't need to shrink the rect.
  DrawThemeBackgroundEx(handle, hdc, PP_FILL, 0, value_rect,
                        &value_draw_options);

  RECT overlay_rect = *value_rect;
  overlay_rect.left +=
      ComputeAnimationProgress(bar_width, kOverlayWidth, pixels_per_second,
                               extra_params.animated_seconds);
  overlay_rect.right = overlay_rect.left + kOverlayWidth;
  DrawThemeBackground(handle, hdc, PP_MOVEOVERLAY, 0, &overlay_rect,
                      value_rect);
}

void PaintTextFieldThemed(
    HDC hdc,
    HANDLE handle,
    HBRUSH bg_brush,
    int part_id,
    int state_id,
    RECT* rect,
    const NativeTheme::TextFieldExtraParams& extra_params) {
  static constexpr DTBGOPTS kOmitBorderOptions = {.dwSize = sizeof(DTBGOPTS),
                                                  .dwFlags = DTBG_OMITBORDER,
                                                  .rcClip = {0, 0, 0, 0}};
  DrawThemeBackgroundEx(
      handle, hdc, part_id, state_id, rect,
      extra_params.draw_edges ? nullptr : &kOmitBorderOptions);

  if (extra_params.fill_content_area) {
    RECT content_rect;
    GetThemeBackgroundContentRect(handle, hdc, part_id, state_id, rect,
                                  &content_rect);
    FillRect(hdc, &content_rect, bg_brush);
  }
}

void PaintTextFieldClassic(
    HDC hdc,
    HBRUSH bg_brush,
    RECT* rect,
    const NativeTheme::TextFieldExtraParams& extra_params) {
  if (extra_params.draw_edges) {
    DrawEdge(hdc, rect, EDGE_SUNKEN, BF_RECT | BF_ADJUST);
  }

  if (extra_params.fill_content_area) {
    if (extra_params.classic_state & DFCS_INACTIVE) {
      bg_brush = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    }
    FillRect(hdc, rect, bg_brush);
  }
}

void PaintScaledTheme(HANDLE theme,
                      HDC hdc,
                      int part_id,
                      int state_id,
                      const gfx::Rect& rect) {
  // Correct the scaling and positioning of sub-components such as scrollbar
  // arrows and thumb grippers in the event that the world transform applies
  // scaling (e.g. in high-DPI mode).
  if (XFORM save_transform; GetWorldTransform(hdc, &save_transform)) {
    if (float scale = save_transform.eM11;
        scale != 1 && save_transform.eM12 == 0) {
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

void PaintFrameControl(HDC hdc,
                       const gfx::Rect& rect,
                       UINT type,
                       UINT state,
                       bool is_selected,
                       NativeTheme::State control_state) {
  const int width = rect.width();
  const int height = rect.height();

  // `DrawFrameControl()` for menu arrow/check wants a monochrome bitmap.
  base::win::ScopedGDIObject<HBITMAP> mask_bitmap(
      CreateBitmap(width, height, 1, 1, NULL));
  if (!mask_bitmap.get()) {
    return;
  }

  base::win::ScopedCreateDC bitmap_dc(CreateCompatibleDC(NULL));
  base::win::ScopedSelectObject select_bitmap(bitmap_dc.Get(),
                                              mask_bitmap.get());
  RECT local_rect = {.left = 0, .top = 0, .right = width, .bottom = height};
  DrawFrameControl(bitmap_dc.Get(), &local_rect, type, state);

  // We're going to use `BitBlt()` with a black-and-white mask. This results in
  // using the dest DC's text color for the black bits in the mask, and the dest
  // DC's background color for the white bits in the mask. `DrawFrameControl()`
  // draws the check in black, and the background in white.
  int bg_color_key = COLOR_MENU;
  int text_color_key = COLOR_MENUTEXT;
  if (control_state == NativeTheme::kDisabled) {
    bg_color_key = is_selected ? COLOR_HIGHLIGHT : COLOR_MENU;
    text_color_key = COLOR_GRAYTEXT;
  } else if (control_state == NativeTheme::kHovered) {
    bg_color_key = COLOR_HIGHLIGHT;
    text_color_key = COLOR_HIGHLIGHTTEXT;
  }
  const COLORREF old_bg_color = SetBkColor(hdc, GetSysColor(bg_color_key));
  const COLORREF old_text_color =
      SetTextColor(hdc, GetSysColor(text_color_key));
  BitBlt(hdc, rect.x(), rect.y(), width, height, bitmap_dc.Get(), 0, 0,
         SRCCOPY);
  SetBkColor(hdc, old_bg_color);
  SetTextColor(hdc, old_text_color);
}

void PaintDirect(SkCanvas* destination_canvas,
                 HDC hdc,
                 NativeTheme::Part part,
                 NativeTheme::State state,
                 const gfx::Rect& rect,
                 const NativeTheme::ExtraParams& extra_params) {
  if (part == NativeTheme::kScrollbarCorner) {
    // Special-cased here since there is no theme name for kScrollbarCorner.
    destination_canvas->drawColor(SK_ColorWHITE, SkBlendMode::kSrc);
    return;
  }

  RECT rect_win = rect.ToRECT();
  if (part == NativeTheme::kTrackbarTrack) {
    // Make the channel be 4 px thick in the center of the supplied rect.  (4 px
    // matches what XP does in various menus; `GetThemePartSize()` doesn't seem
    // to return good values here.)
    constexpr int kChannelThickness = 4;
    if (std::get<NativeTheme::TrackbarExtraParams>(extra_params).vertical) {
      rect_win.top += (rect_win.bottom - rect_win.top - kChannelThickness) / 2;
      rect_win.bottom = rect_win.top + kChannelThickness;
    } else {
      rect_win.left += (rect_win.right - rect_win.left - kChannelThickness) / 2;
      rect_win.right = rect_win.left + kChannelThickness;
    }
  }

  // Most parts can be drawn simply when there is a theme handle.
  const HANDLE handle = GetThemeHandle(GetThemeName(part));
  const int part_id = GetWindowsPart(part, state, extra_params);
  const int state_id = GetWindowsState(part, state, extra_params);
  if (handle) {
    if (part == NativeTheme::kMenuPopupArrow &&
        !std::get<NativeTheme::MenuArrowExtraParams>(extra_params)
             .pointing_right) {
      // The right-pointing arrow can use the common code, but the left-pointing
      // one needs custom code.
      PaintLeftMenuArrowThemed(hdc, handle, part_id, state_id, rect);
      return;
    }
    if (part >= NativeTheme::kScrollbarDownArrow &&
        part <= NativeTheme::kScrollbarVerticalThumb) {
      PaintScaledTheme(handle, hdc, part_id, state_id, rect);
      return;
    }
    if (part == NativeTheme::kCheckbox ||
        part == NativeTheme::kInnerSpinButton ||
        part == NativeTheme::kMenuList || part == NativeTheme::kMenuCheck ||
        part == NativeTheme::kMenuCheckBackground ||
        part == NativeTheme::kMenuPopupArrow ||
        part == NativeTheme::kProgressBar || part == NativeTheme::kPushButton ||
        part == NativeTheme::kRadio ||
        part == NativeTheme::kScrollbarHorizontalTrack ||
        part == NativeTheme::kScrollbarVerticalTrack ||
        part == NativeTheme::kTabPanelBackground ||
        part == NativeTheme::kTrackbarThumb ||
        part == NativeTheme::kTrackbarTrack ||
        part == NativeTheme::kWindowResizeGripper) {
      DrawThemeBackground(handle, hdc, part_id, state_id, &rect_win, nullptr);
      if (part != NativeTheme::kProgressBar) {
        return;
      }
    }
  }

  // Do any further painting the common code couldn't handle.
  switch (part) {
    case NativeTheme::kCheckbox:
    case NativeTheme::kPushButton:
    case NativeTheme::kRadio:
      PaintButtonClassic(
          hdc, part, state, &rect_win,
          std::get<NativeTheme::ButtonExtraParams>(extra_params));
      return;
    case NativeTheme::kInnerSpinButton:
      DrawFrameControl(
          hdc, &rect_win, DFC_SCROLL,
          std::get<NativeTheme::InnerSpinButtonExtraParams>(extra_params)
              .classic_state);
      return;
    case NativeTheme::kMenuCheck: {
      const auto& menu_check =
          std::get<NativeTheme::MenuCheckExtraParams>(extra_params);
      PaintFrameControl(hdc, rect, DFC_MENU,
                        menu_check.is_radio ? DFCS_MENUBULLET : DFCS_MENUCHECK,
                        menu_check.is_selected, state);
      return;
    }
    case NativeTheme::kMenuList:
      DrawFrameControl(
          hdc, &rect_win, DFC_SCROLL,
          DFCS_SCROLLCOMBOBOX |
              std::get<NativeTheme::MenuListExtraParams>(extra_params)
                  .classic_state);
      return;
    case NativeTheme::kMenuPopupArrow: {
      const auto& menu_arrow =
          std::get<NativeTheme::MenuArrowExtraParams>(extra_params);
      // For some reason, Windows uses the name DFCS_MENUARROWRIGHT to indicate
      // a left pointing arrow.
      PaintFrameControl(
          hdc, rect, DFC_MENU,
          menu_arrow.pointing_right ? DFCS_MENUARROW : DFCS_MENUARROWRIGHT,
          menu_arrow.is_selected, state);
      return;
    }
    case NativeTheme::kProgressBar: {
      const auto& progress_bar =
          std::get<NativeTheme::ProgressBarExtraParams>(extra_params);
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
    case NativeTheme::kScrollbarDownArrow:
    case NativeTheme::kScrollbarLeftArrow:
    case NativeTheme::kScrollbarRightArrow:
    case NativeTheme::kScrollbarUpArrow:
      PaintScrollbarArrowClassic(hdc, part, state, &rect_win);
      return;
    case NativeTheme::kScrollbarHorizontalThumb:
    case NativeTheme::kScrollbarVerticalThumb:
      DrawEdge(hdc, &rect_win, EDGE_RAISED, BF_RECT | BF_MIDDLE);
      return;
    case NativeTheme::kScrollbarHorizontalTrack:
    case NativeTheme::kScrollbarVerticalTrack:
      PaintScrollbarTrackClassic(
          destination_canvas, hdc, &rect_win,
          std::get<NativeTheme::ScrollbarTrackExtraParams>(extra_params));
      return;
    case NativeTheme::kTabPanelBackground:
      // Classic just renders a flat color background.
      FillRect(hdc, &rect_win, reinterpret_cast<HBRUSH>(COLOR_3DFACE + 1));
      return;
    case NativeTheme::kTextField: {
      // TODO(mpcomplete): can we detect if the color is specified by the user,
      // and if not, just use the system color?
      // CreateSolidBrush() accepts a RGB value but alpha must be 0.
      const auto& text_field =
          std::get<NativeTheme::TextFieldExtraParams>(extra_params);
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
    case NativeTheme::kTrackbarThumb: {
      const auto& trackbar =
          std::get<NativeTheme::TrackbarExtraParams>(extra_params);
      if (trackbar.vertical) {
        DrawEdge(hdc, &rect_win, EDGE_RAISED, BF_RECT | BF_SOFT | BF_MIDDLE);
      } else {
        PaintHorizontalTrackbarThumbClassic(destination_canvas, hdc, rect_win,
                                            trackbar);
      }
      return;
    }
    case NativeTheme::kTrackbarTrack:
      DrawEdge(hdc, &rect_win, EDGE_SUNKEN, BF_RECT);
      return;
    case NativeTheme::kWindowResizeGripper:
      // Draw a windows classic scrollbar gripper.
      DrawFrameControl(hdc, &rect_win, DFC_SCROLL, DFCS_SCROLLSIZEGRIP);
      return;
    default:
      return;
  }
}

void PaintIndirect(cc::PaintCanvas* destination_canvas,
                   NativeTheme::Part part,
                   NativeTheme::State state,
                   const gfx::Rect& rect,
                   const NativeTheme::ExtraParams& extra_params) {
  // TODO(asvitkine): This path is pretty inefficient - for each paint operation
  // it creates a new offscreen bitmap Skia canvas. This can be sped up by doing
  // it only once per part/state and keeping a cache of the resulting bitmaps.
  //
  // TODO(enne): This could also potentially be sped up for software raster
  // by moving these draw ops into `PaintRecord` itself and then moving the
  // `PaintDirect()` code to be part of the raster for `PaintRecord`.

  // If this process doesn't have access to GDI, we'd need to use a shared
  // memory segment instead, but that is not supported right now.
  if (!base::win::IsUser32AndGdi32Available()) {
    return;
  }

  ScopedCreateDCWithBitmap offscreen_hdc(CreateCompatibleDC(nullptr));
  if (!offscreen_hdc.IsValid()) {
    return;
  }

  skia::InitializeDC(offscreen_hdc.Get());
  if (const HRGN clip = CreateRectRgn(0, 0, rect.width(), rect.height());
      (SelectClipRgn(offscreen_hdc.Get(), clip) == ERROR) ||
      !DeleteObject(clip)) {
    return;
  }

  base::win::ScopedGDIObject<HBITMAP> hbitmap = skia::CreateHBitmapXRGB8888(
      rect.width(), rect.height(), nullptr, nullptr);
  if (!offscreen_hdc.SelectBitmap(hbitmap.release())) {
    return;
  }

  // Will be null if lower-level Windows calls fail, or if the backing
  // allocated is 0 pixels in size (which should never happen according to
  // Windows documentation).
  const sk_sp<SkSurface> offscreen_surface =
      skia::MapPlatformSurface(offscreen_hdc.Get());
  if (!offscreen_surface) {
    return;
  }

  SkCanvas* const offscreen_canvas = offscreen_surface->getCanvas();
  DCHECK(offscreen_canvas);

  // Some of the Windows theme drawing operations do not write correct alpha
  // values for fully-opaque pixels; instead the pixels get alpha 0. This is
  // especially a problem on Windows XP or when using the Classic theme.
  //
  // To work-around this, mark all pixels with a placeholder value, to detect
  // which pixels get touched by the paint operation. After paint, set any
  // pixels that have alpha 0 to opaque and placeholders to fully-transparent.
  constexpr SkColor kPlaceholder = SkColorSetARGB(1, 0, 0, 0);
  offscreen_canvas->clear(kPlaceholder);

  // Offset destination rects to have origin (0,0).
  gfx::Rect adjusted_rect(rect.size());
  NativeTheme::ExtraParams adjusted_extra_params = extra_params;
  if (part == NativeTheme::kProgressBar) {
    auto progress_bar =
        std::get<NativeTheme::ProgressBarExtraParams>(adjusted_extra_params);
    progress_bar.value_rect_x = 0;
    progress_bar.value_rect_y = 0;
  } else if (part == NativeTheme::kScrollbarHorizontalTrack ||
             part == NativeTheme::kScrollbarVerticalTrack) {
    auto scrollbar_track =
        std::get<NativeTheme::ScrollbarTrackExtraParams>(adjusted_extra_params);
    scrollbar_track.track_x = 0;
    scrollbar_track.track_y = 0;
  }
  // Draw the theme controls using existing HDC-drawing code.
  PaintDirect(offscreen_canvas, offscreen_hdc.Get(), part, state, adjusted_rect,
              adjusted_extra_params);

  SkBitmap offscreen_bitmap = skia::MapPlatformBitmap(offscreen_hdc.Get());

  // Post-process the pixels to fix up the alpha values (see big comment above).
  const SkPMColor placeholder_value = SkPreMultiplyColor(kPlaceholder);
  const int pixel_count = rect.width() * rect.height();
  SkPMColor* pixels = offscreen_bitmap.getAddr32(0, 0);
  for (int i = 0; i < pixel_count; i++) {
    if (pixels[i] == placeholder_value) {
      // Pixel wasn't touched - make it fully transparent.
      pixels[i] = SkPMColorSetARGB(0, 0, 0, 0);
    } else if (SkPMColorGetA(pixels[i]) == 0) {
      // Pixel was touched but has incorrect alpha of 0, make it fully opaque.
      pixels[i] =
          SkPMColorSetARGB(0xFF, SkPMColorGetR(pixels[i]),
                           SkPMColorGetG(pixels[i]), SkPMColorGetB(pixels[i]));
    }
  }

  destination_canvas->drawImage(
      cc::PaintImage::CreateFromBitmap(std::move(offscreen_bitmap)), rect.x(),
      rect.y());
}

}  // namespace

// static
void NativeThemeWin::CloseHandles() {
  static_cast<NativeThemeWin*>(GetInstanceForNativeUi())
      ->CloseHandlesInternal();
}

gfx::Size NativeThemeWin::GetPartSize(Part part,
                                      State state,
                                      const ExtraParams& extra_params) const {
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
      int size =
          display::win::GetScreenWin()->GetSystemMetricsInDIP(SM_CXVSCROLL);
      if (size == 0) {
        size = 17;
      }
      return gfx::Size(size, size);
    }
    default:
      break;
  }

  int part_id = GetWindowsPart(part, state, extra_params);
  int state_id = GetWindowsState(part, state, extra_params);

  base::win::ScopedGetDC screen_dc(nullptr);
  SIZE size;
  HANDLE handle = GetThemeHandle(GetThemeName(part));
  if (handle && SUCCEEDED(GetThemePartSize(handle, screen_dc, part_id, state_id,
                                           nullptr, TS_TRUE, &size))) {
    return gfx::Size(size.cx, size.cy);
  }

  // TODO(rogerta): For now, we need to support radio buttons and checkboxes
  // when theming is not enabled.  Support for other parts can be added
  // if/when needed.
  return (part == kCheckbox || part == kRadio) ? gfx::Size(13, 13)
                                               : gfx::Size();
}

void NativeThemeWin::PaintImpl(cc::PaintCanvas* canvas,
                               const ColorProvider* color_provider,
                               Part part,
                               State state,
                               const gfx::Rect& rect,
                               const ExtraParams& extra_params,
                               bool forced_colors,
                               bool dark_mode,
                               PreferredContrast contrast,
                               std::optional<SkColor> accent_color) const {
  switch (part) {
    case kMenuPopupGutter:
      PaintMenuGutter(canvas, color_provider, rect);
      return;
    case kMenuPopupSeparator:
      PaintMenuSeparator(canvas, color_provider,
                         std::get<MenuSeparatorExtraParams>(extra_params));
      return;
    case kMenuPopupBackground:
      PaintMenuBackground(canvas, color_provider, rect);
      return;
    case kMenuItemBackground:
      PaintMenuItemBackground(canvas, color_provider, state, rect,
                              std::get<MenuItemExtraParams>(extra_params));
      return;
    default:
      PaintIndirect(canvas, part, state, rect, extra_params);
      return;
  }
}

NativeTheme::PreferredColorScheme
NativeThemeWin::CalculatePreferredColorScheme() const {
  if (IsForcedDarkMode()) {
    return PreferredColorScheme::kDark;
  }

  if (forced_colors() != ColorProviderKey::ForcedColors::kNone) {
    // According to the spec, the preferred color scheme for web content is
    // "dark" if the Canvas color has L<33% and "light" if L>67%, where "L" is
    // LAB lightness. The Canvas color is mapped to the Window system color.
    // https://www.w3.org/TR/css-color-adjust-1/#forced
    if (const auto bg_color = OsSettingsProvider::Get().Color(
            OsSettingsProvider::ColorId::kWindow)) {
      const SkColor srgb_legacy = bg_color.value();
      const auto [r, g, b] = gfx::SRGBLegacyToSRGB(SkColorGetR(srgb_legacy),
                                                   SkColorGetG(srgb_legacy),
                                                   SkColorGetB(srgb_legacy));
      const auto [x, y, z] = gfx::SRGBToXYZD50(r, g, b);
      const float lab_lightness = std::get<0>(gfx::XYZD50ToLab(x, y, z));
      if (lab_lightness < 33.0f) {
        return PreferredColorScheme::kDark;
      }
      if (lab_lightness > 67.0f) {
        return PreferredColorScheme::kLight;
      }
    }
  }

  return in_dark_mode_ ? PreferredColorScheme::kDark
                       : PreferredColorScheme::kLight;
}

NativeThemeWin::NativeThemeWin() {
  // By default UI should not use the system accent color.
  set_should_use_system_accent_color(false);

  // The below code attempts calls to user32.dll, so avoid it if those calls are
  // not possible.
  if (base::win::IsUser32AndGdi32Available()) {
    set_user_color(AccentColorObserver::Get()->accent_color());
    accent_color_subscription_ = AccentColorObserver::Get()->Subscribe(
        base::BindRepeating(&NativeThemeWin::OnAccentColorMaybeChanged,
                            base::Unretained(this)));

    // If there's no sequenced task runner handle, we can't be called back for
    // registry changes. This generally happens in tests.
    const bool observers_can_operate =
        base::SequencedTaskRunner::HasCurrentDefault();

    hkcu_themes_regkey_ = OpenThemeRegKey(KEY_READ | KEY_NOTIFY);
    if (hkcu_themes_regkey_.Valid()) {
      if (!IsForcedDarkMode() && !IsForcedHighContrast()) {
        UpdateDarkModeStatus();
      }
      if (observers_can_operate) {
        RegisterThemeRegkeyObserver();
      }
    }
  }

  set_preferred_color_scheme(CalculatePreferredColorScheme());

  // Histogram high contrast state.
  // NOTE: Reported in metrics; do not reorder, add additional values at end.
  enum class HighContrastColorScheme {
    kNone = 0,
    kDark = 1,
    kLight = 2,
    kMaxValue = kLight,
  };
  auto color_scheme = HighContrastColorScheme::kNone;
  if (OsSettingsProvider::Get().PreferredContrast() ==
      NativeTheme::PreferredContrast::kMore) {
    color_scheme = (preferred_color_scheme() == PreferredColorScheme::kDark)
                       ? HighContrastColorScheme::kDark
                       : HighContrastColorScheme::kLight;
  }
  base::UmaHistogramEnumeration("Accessibility.WinHighContrastTheme",
                                color_scheme);
}

NativeThemeWin::~NativeThemeWin() {
  // TODO(crbug.com/40551168): Calling CloseHandles() here breaks
  // certain tests and the reliability bots.
  // CloseHandles();
}

void NativeThemeWin::OnToolkitSettingsChanged(bool force_notify) {
  CloseHandles();
  NativeTheme::OnToolkitSettingsChanged(force_notify);
}

void NativeThemeWin::CloseHandlesInternal() {
  HANDLE* theme_handles = GetThemeHandles();
  for (int i = 0; i < LAST; ++i) {
    if (theme_handles[i]) {
      CloseThemeData(theme_handles[i]);
      theme_handles[i] = nullptr;
    }
  }
}

void NativeThemeWin::OnAccentColorMaybeChanged() {
  const auto accent_color = AccentColorObserver::Get()->accent_color();
  if (user_color() != accent_color) {
    set_user_color(accent_color);
    NotifyOnNativeThemeUpdated();
  }
}

void NativeThemeWin::RegisterThemeRegkeyObserver() {
  DCHECK(hkcu_themes_regkey_.Valid());
  DCHECK(base::SequencedTaskRunner::HasCurrentDefault());
  hkcu_themes_regkey_.StartWatching(base::BindOnce(
      [](NativeThemeWin* native_theme) {
        native_theme->UpdateDarkModeStatus();
        // RegKey::StartWatching only provides one notification. Reregistration
        // is required to get future notifications.
        native_theme->RegisterThemeRegkeyObserver();
      },
      base::Unretained(this)));
}

void NativeThemeWin::UpdateDarkModeStatus() {
  in_dark_mode_ = false;
  if (hkcu_themes_regkey_.Valid()) {
    DWORD apps_use_light_theme = 1;
    hkcu_themes_regkey_.ReadValueDW(L"AppsUseLightTheme",
                                    &apps_use_light_theme);
    in_dark_mode_ = (apps_use_light_theme == 0);
  }
  set_preferred_color_scheme(CalculatePreferredColorScheme());
  CloseHandlesInternal();
  NotifyOnNativeThemeUpdated();
}

}  // namespace ui
