// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_win.h"

#include <windows.h>
#include <stddef.h>
#include <uxtheme.h>
#include <vsstyle.h>
#include <vssym32.h>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
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
#include "ui/base/ui_base_switches.h"
#include "ui/display/win/screen_win.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/gdi_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/skia_util.h"
#include "ui/native_theme/common_theme.h"

// This was removed from Winvers.h but is still used.
#if !defined(COLOR_MENUHIGHLIGHT)
#define COLOR_MENUHIGHLIGHT 29
#endif

namespace {

// Windows system color IDs cached and updated by the native theme.
const int kSystemColors[] = {
  COLOR_3DFACE,
  COLOR_BTNFACE,
  COLOR_BTNTEXT,
  COLOR_GRAYTEXT,
  COLOR_HIGHLIGHT,
  COLOR_HIGHLIGHTTEXT,
  COLOR_HOTLIGHT,
  COLOR_MENUHIGHLIGHT,
  COLOR_SCROLLBAR,
  COLOR_WINDOW,
  COLOR_WINDOWTEXT,
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
  paint->setShader(
      SkShader::MakeBitmapShader(bitmap, SkShader::kRepeat_TileMode,
                                 SkShader::kRepeat_TileMode, &local_matrix));
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

  DISALLOW_COPY_AND_ASSIGN(ScopedCreateDCWithBitmap);
};

}  // namespace

namespace ui {

NativeTheme* NativeTheme::GetInstanceForNativeUi() {
  return NativeThemeWin::instance();
}

// static
void NativeThemeWin::CloseHandles() {
  instance()->CloseHandlesInternal();
}

// static
NativeThemeWin* NativeThemeWin::instance() {
  static base::NoDestructor<NativeThemeWin> s_native_theme;
  return s_native_theme.get();
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

  base::win::ScopedGetDC screen_dc(NULL);
  SIZE size;
  if (SUCCEEDED(GetThemePartSize(GetThemeName(part), screen_dc, part_id,
                                 state_id, NULL, TS_TRUE, &size)))
    return gfx::Size(size.cx, size.cy);

  // TODO(rogerta): For now, we need to support radio buttons and checkboxes
  // when theming is not enabled.  Support for other parts can be added
  // if/when needed.
  return (part == kCheckbox || part == kRadio) ?
      gfx::Size(13, 13) : gfx::Size();
}

void NativeThemeWin::Paint(cc::PaintCanvas* canvas,
                           Part part,
                           State state,
                           const gfx::Rect& rect,
                           const ExtraParams& extra) const {
  if (rect.IsEmpty())
    return;

  switch (part) {
    case kMenuPopupGutter:
      PaintMenuGutter(canvas, rect);
      return;
    case kMenuPopupSeparator:
      PaintMenuSeparator(canvas, extra.menu_separator);
      return;
    case kMenuPopupBackground:
      PaintMenuBackground(canvas, rect);
      return;
    case kMenuItemBackground:
      CommonThemePaintMenuItemBackground(this, canvas, state, rect,
                                         extra.menu_item);
      return;
    default:
      PaintIndirect(canvas, part, state, rect, extra);
      return;
  }
}

NativeThemeWin::NativeThemeWin()
    : draw_theme_(NULL),
      draw_theme_ex_(NULL),
      get_theme_content_rect_(NULL),
      get_theme_part_size_(NULL),
      open_theme_(NULL),
      close_theme_(NULL),
      theme_dll_(LoadLibrary(L"uxtheme.dll")),
      color_change_listener_(this),
      is_using_high_contrast_(false),
      is_using_high_contrast_valid_(false) {
  if (theme_dll_) {
    draw_theme_ = reinterpret_cast<DrawThemeBackgroundPtr>(
        GetProcAddress(theme_dll_, "DrawThemeBackground"));
    draw_theme_ex_ = reinterpret_cast<DrawThemeBackgroundExPtr>(
        GetProcAddress(theme_dll_, "DrawThemeBackgroundEx"));
    get_theme_content_rect_ = reinterpret_cast<GetThemeContentRectPtr>(
        GetProcAddress(theme_dll_, "GetThemeBackgroundContentRect"));
    get_theme_part_size_ = reinterpret_cast<GetThemePartSizePtr>(
        GetProcAddress(theme_dll_, "GetThemePartSize"));
    open_theme_ = reinterpret_cast<OpenThemeDataPtr>(
        GetProcAddress(theme_dll_, "OpenThemeData"));
    close_theme_ = reinterpret_cast<CloseThemeDataPtr>(
        GetProcAddress(theme_dll_, "CloseThemeData"));
  }
  memset(theme_handles_, 0, sizeof(theme_handles_));

  // Initialize the cached system colors.
  UpdateSystemColors();
}

NativeThemeWin::~NativeThemeWin() {
  if (theme_dll_) {
    // TODO(https://crbug.com/787692): Calling CloseHandles() here breaks
    // certain tests and the reliability bots.
    // CloseHandles();
    FreeLibrary(theme_dll_);
  }
}

bool NativeThemeWin::IsUsingHighContrastThemeInternal() const {
  if (is_using_high_contrast_valid_)
    return is_using_high_contrast_;
  HIGHCONTRAST result;
  result.cbSize = sizeof(HIGHCONTRAST);
  is_using_high_contrast_ =
      SystemParametersInfo(SPI_GETHIGHCONTRAST, result.cbSize, &result, 0) &&
      (result.dwFlags & HCF_HIGHCONTRASTON) == HCF_HIGHCONTRASTON;
  is_using_high_contrast_valid_ = true;
  return is_using_high_contrast_;
}

void NativeThemeWin::CloseHandlesInternal() {
  if (!close_theme_)
    return;

  for (int i = 0; i < LAST; ++i) {
    if (theme_handles_[i]) {
      close_theme_(theme_handles_[i]);
      theme_handles_[i] = nullptr;
    }
  }
}

void NativeThemeWin::OnSysColorChange() {
  UpdateSystemColors();
  is_using_high_contrast_valid_ = false;
  NotifyObservers();
}

void NativeThemeWin::UpdateSystemColors() {
  for (int kSystemColor : kSystemColors)
    system_colors_[kSystemColor] = color_utils::GetSysSkColor(kSystemColor);
}

void NativeThemeWin::PaintMenuSeparator(
    cc::PaintCanvas* canvas,
    const MenuSeparatorExtraParams& params) const {
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
  flags.setColor(GetSystemColor(NativeTheme::kColorId_MenuSeparatorColor));
  canvas->drawLine(start.x(), start.y(), end.x(), end.y(), flags);
}

void NativeThemeWin::PaintMenuGutter(cc::PaintCanvas* canvas,
                                     const gfx::Rect& rect) const {
  cc::PaintFlags flags;
  flags.setColor(GetSystemColor(NativeTheme::kColorId_MenuSeparatorColor));
  int position_x = rect.x() + rect.width() / 2;
  canvas->drawLine(position_x, rect.y(), position_x, rect.bottom(), flags);
}

void NativeThemeWin::PaintMenuBackground(cc::PaintCanvas* canvas,
                                         const gfx::Rect& rect) const {
  cc::PaintFlags flags;
  flags.setColor(GetSystemColor(NativeTheme::kColorId_MenuBackgroundColor));
  canvas->drawRect(gfx::RectToSkRect(rect), flags);
}

void NativeThemeWin::PaintDirect(SkCanvas* destination_canvas,
                                 HDC hdc,
                                 Part part,
                                 State state,
                                 const gfx::Rect& rect,
                                 const ExtraParams& extra) const {
  switch (part) {
    case kCheckbox:
      PaintCheckbox(hdc, part, state, rect, extra.button);
      return;
    case kInnerSpinButton:
      PaintSpinButton(hdc, part, state, rect, extra.inner_spin);
      return;
    case kMenuList:
      PaintMenuList(hdc, state, rect, extra.menu_list);
      return;
    case kMenuCheck:
      PaintMenuCheck(hdc, state, rect, extra.menu_check);
      return;
    case kMenuCheckBackground:
      PaintMenuCheckBackground(hdc, state, rect);
      return;
    case kMenuPopupArrow:
      PaintMenuArrow(hdc, state, rect, extra.menu_arrow);
      return;
    case kProgressBar:
      PaintProgressBar(hdc, rect, extra.progress_bar);
      return;
    case kPushButton:
      PaintPushButton(hdc, part, state, rect, extra.button);
      return;
    case kRadio:
      PaintRadioButton(hdc, part, state, rect, extra.button);
      return;
    case kScrollbarDownArrow:
    case kScrollbarUpArrow:
    case kScrollbarLeftArrow:
    case kScrollbarRightArrow:
      PaintScrollbarArrow(hdc, part, state, rect, extra.scrollbar_arrow);
      return;
    case kScrollbarHorizontalThumb:
    case kScrollbarVerticalThumb:
    case kScrollbarHorizontalGripper:
    case kScrollbarVerticalGripper:
      PaintScrollbarThumb(hdc, part, state, rect, extra.scrollbar_thumb);
      return;
    case kScrollbarHorizontalTrack:
    case kScrollbarVerticalTrack:
      PaintScrollbarTrack(destination_canvas, hdc, part, state, rect,
                          extra.scrollbar_track);
      return;
    case kScrollbarCorner:
      destination_canvas->drawColor(SK_ColorWHITE, SkBlendMode::kSrc);
      return;
    case kTabPanelBackground:
      PaintTabPanelBackground(hdc, rect);
      return;
    case kTextField:
      PaintTextField(hdc, part, state, rect, extra.text_field);
      return;
    case kTrackbarThumb:
    case kTrackbarTrack:
      PaintTrackbar(destination_canvas, hdc, part, state, rect, extra.trackbar);
      return;
    case kWindowResizeGripper:
      PaintWindowResizeGripper(hdc, rect);
      return;
    case kMenuPopupBackground:
    case kMenuPopupGutter:
    case kMenuPopupSeparator:
    case kMenuItemBackground:
    case kSliderTrack:
    case kSliderThumb:
    case kMaxPart:
      NOTREACHED();
  }
}

SkColor NativeThemeWin::GetSystemColor(ColorId color_id) const {
  // TODO: Obtain the correct colors for these using GetSysColor.
  // Button:
  constexpr SkColor kButtonHoverColor = SkColorSetRGB(6, 45, 117);
  constexpr SkColor kProminentButtonColorInvert = gfx::kGoogleBlue300;
  // MenuItem:
  constexpr SkColor kMenuSchemeHighlightBackgroundColorInvert =
      SkColorSetRGB(0x30, 0x30, 0x30);
  // Label:
  constexpr SkColor kLabelTextSelectionBackgroundFocusedColor =
      gfx::kGoogleBlue700;

  switch (color_id) {
    // Windows
    case kColorId_WindowBackground:
      return system_colors_[COLOR_WINDOW];

    // Dialogs
    case kColorId_DialogBackground:
    case kColorId_BubbleBackground:
      break;

    // FocusableBorder
    case kColorId_FocusedBorderColor:
    case kColorId_UnfocusedBorderColor:
      break;

    // Button
    case kColorId_ButtonEnabledColor:
      return system_colors_[COLOR_BTNTEXT];
    case kColorId_ButtonHoverColor:
      return kButtonHoverColor;

    // Label
    case kColorId_LabelEnabledColor:
      return system_colors_[COLOR_BTNTEXT];
    case kColorId_LabelDisabledColor:
      return system_colors_[COLOR_GRAYTEXT];
    case kColorId_LabelTextSelectionColor:
      return system_colors_[COLOR_HIGHLIGHTTEXT];
    case kColorId_LabelTextSelectionBackgroundFocused:
      return kLabelTextSelectionBackgroundFocusedColor;

    // Textfield
    case kColorId_TextfieldDefaultColor:
      return system_colors_[COLOR_WINDOWTEXT];
    case kColorId_TextfieldDefaultBackground:
      return system_colors_[COLOR_WINDOW];
    case kColorId_TextfieldReadOnlyColor:
      return system_colors_[COLOR_GRAYTEXT];
    case kColorId_TextfieldReadOnlyBackground:
      return system_colors_[COLOR_3DFACE];
    case kColorId_TextfieldSelectionColor:
      return system_colors_[COLOR_HIGHLIGHTTEXT];
    case kColorId_TextfieldSelectionBackgroundFocused:
      return system_colors_[COLOR_HIGHLIGHT];

    // Tooltip
    case kColorId_TooltipBackground:
    case kColorId_TooltipText:
      NOTREACHED();
      return gfx::kPlaceholderColor;

    // Tree
    // NOTE: these aren't right for all themes, but as close as I could get.
    case kColorId_TreeBackground:
      return system_colors_[COLOR_WINDOW];
    case kColorId_TreeText:
      return system_colors_[COLOR_WINDOWTEXT];
    case kColorId_TreeSelectedText:
      return system_colors_[COLOR_HIGHLIGHTTEXT];
    case kColorId_TreeSelectedTextUnfocused:
      return system_colors_[COLOR_BTNTEXT];
    case kColorId_TreeSelectionBackgroundFocused:
      return system_colors_[COLOR_HIGHLIGHT];
    case kColorId_TreeSelectionBackgroundUnfocused:
      return system_colors_[UsesHighContrastColors() ? COLOR_MENUHIGHLIGHT
                                                     : COLOR_BTNFACE];

    // Table
    case kColorId_TableBackground:
      return system_colors_[COLOR_WINDOW];
    case kColorId_TableText:
      return system_colors_[COLOR_WINDOWTEXT];
    case kColorId_TableSelectedText:
      return system_colors_[COLOR_HIGHLIGHTTEXT];
    case kColorId_TableSelectedTextUnfocused:
      return system_colors_[COLOR_BTNTEXT];
    case kColorId_TableSelectionBackgroundFocused:
      return system_colors_[COLOR_HIGHLIGHT];
    case kColorId_TableSelectionBackgroundUnfocused:
      return system_colors_[UsesHighContrastColors() ? COLOR_MENUHIGHLIGHT
                                                     : COLOR_BTNFACE];
    case kColorId_TableGroupingIndicatorColor:
      return system_colors_[COLOR_GRAYTEXT];

    // Results Tables
    case kColorId_ResultsTableNormalBackground:
      return system_colors_[COLOR_WINDOW];
    case kColorId_ResultsTableHoveredBackground:
      return color_utils::AlphaBlend(system_colors_[COLOR_HIGHLIGHT],
                                     system_colors_[COLOR_WINDOW], 0x40);
    case kColorId_ResultsTableNormalText:
      return system_colors_[COLOR_WINDOWTEXT];
    case kColorId_ResultsTableDimmedText:
      return color_utils::AlphaBlend(system_colors_[COLOR_WINDOWTEXT],
                                     system_colors_[COLOR_WINDOW], 0x80);
    default:
      break;
  }

  if (color_utils::IsInvertedColorScheme()) {
    switch (color_id) {
      case NativeTheme::kColorId_FocusedMenuItemBackgroundColor:
        return kMenuSchemeHighlightBackgroundColorInvert;
      case NativeTheme::kColorId_ProminentButtonColor:
        return kProminentButtonColorInvert;
      default:
        return color_utils::InvertColor(GetAuraColor(color_id, this));
    }
  }

  return GetAuraColor(color_id, this);
}

bool NativeThemeWin::SupportsNinePatch(Part part) const {
  // The only nine-patch resources currently supported (overlay scrollbar) are
  // painted by NativeThemeAura on Windows.
  return false;
}

gfx::Size NativeThemeWin::GetNinePatchCanvasSize(Part part) const {
  NOTREACHED() << "NativeThemeWin doesn't support nine-patch resources.";
  return gfx::Size();
}

gfx::Rect NativeThemeWin::GetNinePatchAperture(Part part) const {
  NOTREACHED() << "NativeThemeWin doesn't support nine-patch resources.";
  return gfx::Rect();
}

bool NativeThemeWin::UsesHighContrastColors() const {
  bool force_enabled = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kForceHighContrast);
  return force_enabled || IsUsingHighContrastThemeInternal();
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

  if (!offscreen_hdc.SelectBitmap(skia::CreateHBitmap(
          rect.width(), rect.height(), false, nullptr, nullptr))) {
    return;
  }

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
  ExtraParams adjusted_extra(extra);
  switch (part) {
    case kProgressBar:
      adjusted_extra.progress_bar.value_rect_x = 0;
      adjusted_extra.progress_bar.value_rect_y = 0;
      break;
    case kScrollbarHorizontalTrack:
    case kScrollbarVerticalTrack:
      adjusted_extra.scrollbar_track.track_x = 0;
      adjusted_extra.scrollbar_track.track_y = 0;
      break;
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

HRESULT NativeThemeWin::GetThemePartSize(ThemeName theme_name,
                                         HDC hdc,
                                         int part_id,
                                         int state_id,
                                         RECT* rect,
                                         int ts,
                                         SIZE* size) const {
  HANDLE handle = GetThemeHandle(theme_name);
  return (handle && get_theme_part_size_) ?
      get_theme_part_size_(handle, hdc, part_id, state_id, rect, ts, size) :
      E_NOTIMPL;
}

HRESULT NativeThemeWin::PaintButton(HDC hdc,
                                    State state,
                                    const ButtonExtraParams& extra,
                                    int part_id,
                                    int state_id,
                                    RECT* rect) const {
  HANDLE handle = GetThemeHandle(BUTTON);
  if (handle && draw_theme_)
    return draw_theme_(handle, hdc, part_id, state_id, rect, NULL);

  // Adjust classic_state based on part, state, and extras.
  int classic_state = extra.classic_state;
  switch (part_id) {
    case BP_CHECKBOX:
      classic_state |= DFCS_BUTTONCHECK;
      break;
    case BP_RADIOBUTTON:
      classic_state |= DFCS_BUTTONRADIO;
      break;
    case BP_PUSHBUTTON:
      classic_state |= DFCS_BUTTONPUSH;
      break;
    default:
      NOTREACHED();
      break;
  }

  switch (state) {
    case kDisabled:
      classic_state |= DFCS_INACTIVE;
      break;
    case kHovered:
    case kNormal:
      break;
    case kPressed:
      classic_state |= DFCS_PUSHED;
      break;
    case kNumStates:
      NOTREACHED();
      break;
  }

  if (extra.checked)
    classic_state |= DFCS_CHECKED;

  // Draw it manually.
  // All pressed states have both low bits set, and no other states do.
  const bool focused = ((state_id & ETS_FOCUSED) == ETS_FOCUSED);
  const bool pressed = ((state_id & PBS_PRESSED) == PBS_PRESSED);
  if ((BP_PUSHBUTTON == part_id) && (pressed || focused)) {
    // BP_PUSHBUTTON has a focus rect drawn around the outer edge, and the
    // button itself is shrunk by 1 pixel.
    HBRUSH brush = GetSysColorBrush(COLOR_3DDKSHADOW);
    if (brush) {
      FrameRect(hdc, rect, brush);
      InflateRect(rect, -1, -1);
    }
  }
  DrawFrameControl(hdc, rect, DFC_BUTTON, classic_state);

  // Draw the focus rectangle (the dotted line box) only on buttons.  For radio
  // and checkboxes, we let webkit draw the focus rectangle (orange glow).
  if ((BP_PUSHBUTTON == part_id) && focused) {
    // The focus rect is inside the button.  The exact number of pixels depends
    // on whether we're in classic mode or using uxtheme.
    if (handle && get_theme_content_rect_) {
      get_theme_content_rect_(handle, hdc, part_id, state_id, rect, rect);
    } else {
      InflateRect(rect, -GetSystemMetrics(SM_CXEDGE),
                  -GetSystemMetrics(SM_CYEDGE));
    }
    DrawFocusRect(hdc, rect);
  }

  // Classic theme doesn't support indeterminate checkboxes.  We draw
  // a recangle inside a checkbox like IE10 does.
  if (part_id == BP_CHECKBOX && extra.indeterminate) {
    RECT inner_rect = *rect;
    // "4 / 13" is same as IE10 in classic theme.
    int padding = (inner_rect.right - inner_rect.left) * 4 / 13;
    InflateRect(&inner_rect, -padding, -padding);
    int color_index = state == kDisabled ? COLOR_GRAYTEXT : COLOR_WINDOWTEXT;
    FillRect(hdc, &inner_rect, GetSysColorBrush(color_index));
  }

  return S_OK;
}

HRESULT NativeThemeWin::PaintMenuArrow(
    HDC hdc,
    State state,
    const gfx::Rect& rect,
    const MenuArrowExtraParams& extra) const {
  int state_id = MSM_NORMAL;
  if (state == kDisabled)
    state_id = MSM_DISABLED;

  HANDLE handle = GetThemeHandle(MENU);
  RECT rect_win = rect.ToRECT();
  if (handle && draw_theme_) {
    if (extra.pointing_right) {
      return draw_theme_(handle, hdc, MENU_POPUPSUBMENU, state_id, &rect_win,
                         NULL);
    }
    // There is no way to tell the uxtheme API to draw a left pointing arrow; it
    // doesn't have a flag equivalent to DFCS_MENUARROWRIGHT.  But they are
    // needed for RTL locales on Vista.  So use a memory DC and mirror the
    // region with GDI's StretchBlt.
    gfx::Rect r(rect);
    base::win::ScopedCreateDC mem_dc(CreateCompatibleDC(hdc));
    base::win::ScopedBitmap mem_bitmap(CreateCompatibleBitmap(hdc, r.width(),
                                                              r.height()));
    base::win::ScopedSelectObject select_bitmap(mem_dc.Get(), mem_bitmap.get());
    // Copy and horizontally mirror the background from hdc into mem_dc. Use
    // a negative-width source rect, starting at the rightmost pixel.
    StretchBlt(mem_dc.Get(), 0, 0, r.width(), r.height(),
               hdc, r.right()-1, r.y(), -r.width(), r.height(), SRCCOPY);
    // Draw the arrow.
    RECT theme_rect = {0, 0, r.width(), r.height()};
    HRESULT result = draw_theme_(handle, mem_dc.Get(), MENU_POPUPSUBMENU,
                                  state_id, &theme_rect, NULL);
    // Copy and mirror the result back into mem_dc.
    StretchBlt(hdc, r.x(), r.y(), r.width(), r.height(),
               mem_dc.Get(), r.width()-1, 0, -r.width(), r.height(), SRCCOPY);
    return result;
  }

  // For some reason, Windows uses the name DFCS_MENUARROWRIGHT to indicate a
  // left pointing arrow. This makes the following statement counterintuitive.
  UINT pfc_state = extra.pointing_right ? DFCS_MENUARROW : DFCS_MENUARROWRIGHT;
  return PaintFrameControl(hdc, rect, DFC_MENU, pfc_state, extra.is_selected,
                           state);
}

HRESULT NativeThemeWin::PaintMenuCheck(
    HDC hdc,
    State state,
    const gfx::Rect& rect,
    const MenuCheckExtraParams& extra) const {
  HANDLE handle = GetThemeHandle(MENU);
  if (handle && draw_theme_) {
    const int state_id = extra.is_radio ?
        ((state == kDisabled) ? MC_BULLETDISABLED : MC_BULLETNORMAL) :
        ((state == kDisabled) ? MC_CHECKMARKDISABLED : MC_CHECKMARKNORMAL);
    RECT rect_win = rect.ToRECT();
    return draw_theme_(handle, hdc, MENU_POPUPCHECK, state_id, &rect_win, NULL);
  }

  return PaintFrameControl(hdc, rect, DFC_MENU,
                           extra.is_radio ? DFCS_MENUBULLET : DFCS_MENUCHECK,
                           extra.is_selected, state);
}

HRESULT NativeThemeWin::PaintMenuCheckBackground(HDC hdc,
                                                 State state,
                                                 const gfx::Rect& rect) const {
  HANDLE handle = GetThemeHandle(MENU);
  if (!handle || !draw_theme_)
    return S_OK;  // Nothing to do for background.

  int state_id = state == kDisabled ? MCB_DISABLED : MCB_NORMAL;
  RECT rect_win = rect.ToRECT();
  return draw_theme_(handle, hdc, MENU_POPUPCHECKBACKGROUND, state_id,
                     &rect_win, NULL);
}

HRESULT NativeThemeWin::PaintPushButton(HDC hdc,
                                        Part part,
                                        State state,
                                        const gfx::Rect& rect,
                                        const ButtonExtraParams& extra) const {
  int state_id = extra.is_default ? PBS_DEFAULTED : PBS_NORMAL;
  switch (state) {
    case kDisabled:
      state_id = PBS_DISABLED;
      break;
    case kHovered:
      state_id = PBS_HOT;
      break;
    case kNormal:
      break;
    case kPressed:
      state_id = PBS_PRESSED;
      break;
    case kNumStates:
      NOTREACHED();
      break;
  }

  RECT rect_win = rect.ToRECT();
  return PaintButton(hdc, state, extra, BP_PUSHBUTTON, state_id, &rect_win);
}

HRESULT NativeThemeWin::PaintRadioButton(HDC hdc,
                                         Part part,
                                         State state,
                                         const gfx::Rect& rect,
                                         const ButtonExtraParams& extra) const {
  int state_id = extra.checked ? RBS_CHECKEDNORMAL : RBS_UNCHECKEDNORMAL;
  switch (state) {
    case kDisabled:
      state_id = extra.checked ? RBS_CHECKEDDISABLED : RBS_UNCHECKEDDISABLED;
      break;
    case kHovered:
      state_id = extra.checked ? RBS_CHECKEDHOT : RBS_UNCHECKEDHOT;
      break;
    case kNormal:
      break;
    case kPressed:
      state_id = extra.checked ? RBS_CHECKEDPRESSED : RBS_UNCHECKEDPRESSED;
      break;
    case kNumStates:
      NOTREACHED();
      break;
  }

  RECT rect_win = rect.ToRECT();
  return PaintButton(hdc, state, extra, BP_RADIOBUTTON, state_id, &rect_win);
}

HRESULT NativeThemeWin::PaintCheckbox(HDC hdc,
                                      Part part,
                                      State state,
                                      const gfx::Rect& rect,
                                      const ButtonExtraParams& extra) const {
  int state_id = extra.checked ?
      CBS_CHECKEDNORMAL :
      (extra.indeterminate ? CBS_MIXEDNORMAL : CBS_UNCHECKEDNORMAL);
  switch (state) {
    case kDisabled:
      state_id = extra.checked ?
          CBS_CHECKEDDISABLED :
          (extra.indeterminate ? CBS_MIXEDDISABLED : CBS_UNCHECKEDDISABLED);
      break;
    case kHovered:
      state_id = extra.checked ?
          CBS_CHECKEDHOT :
          (extra.indeterminate ? CBS_MIXEDHOT : CBS_UNCHECKEDHOT);
      break;
    case kNormal:
      break;
    case kPressed:
      state_id = extra.checked ?
          CBS_CHECKEDPRESSED :
          (extra.indeterminate ? CBS_MIXEDPRESSED : CBS_UNCHECKEDPRESSED);
      break;
    case kNumStates:
      NOTREACHED();
      break;
  }

  RECT rect_win = rect.ToRECT();
  return PaintButton(hdc, state, extra, BP_CHECKBOX, state_id, &rect_win);
}

HRESULT NativeThemeWin::PaintMenuList(HDC hdc,
                                      State state,
                                      const gfx::Rect& rect,
                                      const MenuListExtraParams& extra) const {
  HANDLE handle = GetThemeHandle(MENULIST);
  RECT rect_win = rect.ToRECT();
  int state_id = CBXS_NORMAL;
  switch (state) {
    case kDisabled:
      state_id = CBXS_DISABLED;
      break;
    case kHovered:
      state_id = CBXS_HOT;
      break;
    case kNormal:
      break;
    case kPressed:
      state_id = CBXS_PRESSED;
      break;
    case kNumStates:
      NOTREACHED();
      break;
  }

  if (handle && draw_theme_)
    return draw_theme_(handle, hdc, CP_DROPDOWNBUTTON, state_id, &rect_win,
                       NULL);

  // Draw it manually.
  DrawFrameControl(hdc, &rect_win, DFC_SCROLL,
                   DFCS_SCROLLCOMBOBOX | extra.classic_state);
  return S_OK;
}

HRESULT NativeThemeWin::PaintScrollbarArrow(
    HDC hdc,
    Part part,
    State state,
    const gfx::Rect& rect,
    const ScrollbarArrowExtraParams& extra) const {
  static const int state_id_matrix[4][kNumStates] = {
      {ABS_DOWNDISABLED, ABS_DOWNHOT, ABS_DOWNNORMAL, ABS_DOWNPRESSED},
      {ABS_LEFTDISABLED, ABS_LEFTHOT, ABS_LEFTNORMAL, ABS_LEFTPRESSED},
      {ABS_RIGHTDISABLED, ABS_RIGHTHOT, ABS_RIGHTNORMAL, ABS_RIGHTPRESSED},
      {ABS_UPDISABLED, ABS_UPHOT, ABS_UPNORMAL, ABS_UPPRESSED},
  };
  HANDLE handle = GetThemeHandle(SCROLLBAR);
  RECT rect_win = rect.ToRECT();
  if (handle && draw_theme_) {
    int index = part - kScrollbarDownArrow;
    DCHECK_GE(index, 0);
    DCHECK_LT(static_cast<size_t>(index), arraysize(state_id_matrix));
    int state_id = state_id_matrix[index][state];

    // Hovering means that the cursor is over the scroolbar, but not over the
    // specific arrow itself.  We don't want to show it "hot" mode, but only
    // in "hover" mode.
    if (state == kHovered && extra.is_hovering) {
      switch (part) {
        case kScrollbarDownArrow:
          state_id = ABS_DOWNHOVER;
          break;
        case kScrollbarLeftArrow:
          state_id = ABS_LEFTHOVER;
          break;
        case kScrollbarRightArrow:
          state_id = ABS_RIGHTHOVER;
          break;
        case kScrollbarUpArrow:
          state_id = ABS_UPHOVER;
          break;
        default:
          NOTREACHED();
          break;
      }
    }
    return PaintScaledTheme(handle, hdc, SBP_ARROWBTN, state_id, rect);
  }

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
      NOTREACHED();
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
      NOTREACHED();
      break;
  }
  DrawFrameControl(hdc, &rect_win, DFC_SCROLL, classic_state);
  return S_OK;
}

HRESULT NativeThemeWin::PaintScrollbarThumb(
    HDC hdc,
    Part part,
    State state,
    const gfx::Rect& rect,
    const ScrollbarThumbExtraParams& extra) const {
  HANDLE handle = GetThemeHandle(SCROLLBAR);
  RECT rect_win = rect.ToRECT();

  int part_id = SBP_THUMBBTNVERT;
  switch (part) {
    case kScrollbarHorizontalThumb:
      part_id = SBP_THUMBBTNHORZ;
      break;
    case kScrollbarVerticalThumb:
      break;
    case kScrollbarHorizontalGripper:
      part_id = SBP_GRIPPERHORZ;
      break;
    case kScrollbarVerticalGripper:
      part_id = SBP_GRIPPERVERT;
      break;
    default:
      NOTREACHED();
      break;
  }

  int state_id = SCRBS_NORMAL;
  switch (state) {
    case kDisabled:
      state_id = SCRBS_DISABLED;
      break;
    case kHovered:
      state_id = extra.is_hovering ? SCRBS_HOVER : SCRBS_HOT;
      break;
    case kNormal:
      break;
    case kPressed:
      state_id = SCRBS_PRESSED;
      break;
    case kNumStates:
      NOTREACHED();
      break;
  }

  if (handle && draw_theme_)
    return PaintScaledTheme(handle, hdc, part_id, state_id, rect);

  // Draw it manually.
  if ((part_id == SBP_THUMBBTNHORZ) || (part_id == SBP_THUMBBTNVERT))
    DrawEdge(hdc, &rect_win, EDGE_RAISED, BF_RECT | BF_MIDDLE);
  // Classic mode doesn't have a gripper.
  return S_OK;
}

HRESULT NativeThemeWin::PaintScrollbarTrack(
    SkCanvas* canvas,
    HDC hdc,
    Part part,
    State state,
    const gfx::Rect& rect,
    const ScrollbarTrackExtraParams& extra) const {
  HANDLE handle = GetThemeHandle(SCROLLBAR);
  RECT rect_win = rect.ToRECT();

  const int part_id = extra.is_upper ?
      ((part == kScrollbarHorizontalTrack) ?
          SBP_UPPERTRACKHORZ : SBP_UPPERTRACKVERT) :
      ((part == kScrollbarHorizontalTrack) ?
          SBP_LOWERTRACKHORZ : SBP_LOWERTRACKVERT);

  int state_id = SCRBS_NORMAL;
  switch (state) {
    case kDisabled:
      state_id = SCRBS_DISABLED;
      break;
    case kHovered:
      state_id = SCRBS_HOVER;
      break;
    case kNormal:
      break;
    case kPressed:
      state_id = SCRBS_PRESSED;
      break;
    case kNumStates:
      NOTREACHED();
      break;
  }

  if (handle && draw_theme_)
    return draw_theme_(handle, hdc, part_id, state_id, &rect_win, NULL);

  // Draw it manually.
  if ((system_colors_[COLOR_SCROLLBAR] != system_colors_[COLOR_3DFACE]) &&
      (system_colors_[COLOR_SCROLLBAR] != system_colors_[COLOR_WINDOW])) {
    FillRect(hdc, &rect_win, reinterpret_cast<HBRUSH>(COLOR_SCROLLBAR + 1));
  } else {
    SkPaint paint;
    RECT align_rect = gfx::Rect(extra.track_x, extra.track_y, extra.track_width,
                                extra.track_height).ToRECT();
    SetCheckerboardShader(&paint, align_rect);
    canvas->drawIRect(skia::RECTToSkIRect(rect_win), paint);
  }
  if (extra.classic_state & DFCS_PUSHED)
    InvertRect(hdc, &rect_win);
  return S_OK;
}

HRESULT NativeThemeWin::PaintSpinButton(
    HDC hdc,
    Part part,
    State state,
    const gfx::Rect& rect,
    const InnerSpinButtonExtraParams& extra) const {
  HANDLE handle = GetThemeHandle(SPIN);
  RECT rect_win = rect.ToRECT();
  int part_id = extra.spin_up ? SPNP_UP : SPNP_DOWN;
  int state_id = extra.spin_up ? UPS_NORMAL : DNS_NORMAL;
  switch (state) {
    case kDisabled:
      state_id = extra.spin_up ? UPS_DISABLED : DNS_DISABLED;
      break;
    case kHovered:
      state_id = extra.spin_up ? UPS_HOT : DNS_HOT;
      break;
    case kNormal:
      break;
    case kPressed:
      state_id = extra.spin_up ? UPS_PRESSED : DNS_PRESSED;
      break;
    case kNumStates:
      NOTREACHED();
      break;
  }

  if (handle && draw_theme_)
    return draw_theme_(handle, hdc, part_id, state_id, &rect_win, NULL);
  DrawFrameControl(hdc, &rect_win, DFC_SCROLL, extra.classic_state);
  return S_OK;
}

HRESULT NativeThemeWin::PaintTrackbar(SkCanvas* canvas,
                                      HDC hdc,
                                      Part part,
                                      State state,
                                      const gfx::Rect& rect,
                                      const TrackbarExtraParams& extra) const {
  const int part_id = extra.vertical ?
      ((part == kTrackbarTrack) ? TKP_TRACKVERT : TKP_THUMBVERT) :
      ((part == kTrackbarTrack) ? TKP_TRACK : TKP_THUMBBOTTOM);

  int state_id = TUS_NORMAL;
  switch (state) {
    case kDisabled:
      state_id = TUS_DISABLED;
      break;
    case kHovered:
      state_id = TUS_HOT;
      break;
    case kNormal:
      break;
    case kPressed:
      state_id = TUS_PRESSED;
      break;
    case kNumStates:
      NOTREACHED();
      break;
  }

  // Make the channel be 4 px thick in the center of the supplied rect.  (4 px
  // matches what XP does in various menus; GetThemePartSize() doesn't seem to
  // return good values here.)
  RECT rect_win = rect.ToRECT();
  RECT channel_rect = rect.ToRECT();
  const int channel_thickness = 4;
  if (part_id == TKP_TRACK) {
    channel_rect.top +=
        ((channel_rect.bottom - channel_rect.top - channel_thickness) / 2);
    channel_rect.bottom = channel_rect.top + channel_thickness;
  } else if (part_id == TKP_TRACKVERT) {
    channel_rect.left +=
        ((channel_rect.right - channel_rect.left - channel_thickness) / 2);
    channel_rect.right = channel_rect.left + channel_thickness;
  }  // else this isn't actually a channel, so |channel_rect| == |rect|.

  HANDLE handle = GetThemeHandle(TRACKBAR);
  if (handle && draw_theme_)
    return draw_theme_(handle, hdc, part_id, state_id, &channel_rect, NULL);

  // Classic mode, draw it manually.
  if ((part_id == TKP_TRACK) || (part_id == TKP_TRACKVERT)) {
    DrawEdge(hdc, &channel_rect, EDGE_SUNKEN, BF_RECT);
  } else if (part_id == TKP_THUMBVERT) {
    DrawEdge(hdc, &rect_win, EDGE_RAISED, BF_RECT | BF_SOFT | BF_MIDDLE);
  } else {
    // Split rect into top and bottom pieces.
    RECT top_section = rect.ToRECT();
    RECT bottom_section = rect.ToRECT();
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
      SetCheckerboardShader(&paint, rect_win);

      // Fill all three pieces with the pattern.
      canvas->drawIRect(skia::RECTToSkIRect(top_section), paint);

      SkScalar left_triangle_top = SkIntToScalar(left_half.top);
      SkScalar left_triangle_right = SkIntToScalar(left_half.right);
      SkPath left_triangle;
      left_triangle.moveTo(SkIntToScalar(left_half.left), left_triangle_top);
      left_triangle.lineTo(left_triangle_right, left_triangle_top);
      left_triangle.lineTo(left_triangle_right,
                           SkIntToScalar(left_half.bottom));
      left_triangle.close();
      canvas->drawPath(left_triangle, paint);

      SkScalar right_triangle_left = SkIntToScalar(right_half.left);
      SkScalar right_triangle_top = SkIntToScalar(right_half.top);
      SkPath right_triangle;
      right_triangle.moveTo(right_triangle_left, right_triangle_top);
      right_triangle.lineTo(SkIntToScalar(right_half.right),
                            right_triangle_top);
      right_triangle.lineTo(right_triangle_left,
                            SkIntToScalar(right_half.bottom));
      right_triangle.close();
      canvas->drawPath(right_triangle, paint);
    }
  }
  return S_OK;
}

HRESULT NativeThemeWin::PaintProgressBar(
    HDC hdc,
    const gfx::Rect& rect,
    const ProgressBarExtraParams& extra) const {
  // There is no documentation about the animation speed, frame-rate, nor
  // size of moving overlay of the indeterminate progress bar.
  // So we just observed real-world programs and guessed following parameters.
  const int kDeterminateOverlayPixelsPerSecond = 300;
  const int kDeterminateOverlayWidth = 120;
  const int kIndeterminateOverlayPixelsPerSecond =  175;
  const int kIndeterminateOverlayWidth = 120;

  RECT bar_rect = rect.ToRECT();
  RECT value_rect = gfx::Rect(extra.value_rect_x,
                              extra.value_rect_y,
                              extra.value_rect_width,
                              extra.value_rect_height).ToRECT();

  HANDLE handle = GetThemeHandle(PROGRESS);
  if (!handle || !draw_theme_ || !draw_theme_ex_) {
    FillRect(hdc, &bar_rect, GetSysColorBrush(COLOR_BTNFACE));
    FillRect(hdc, &value_rect, GetSysColorBrush(COLOR_BTNSHADOW));
    DrawEdge(hdc, &bar_rect, EDGE_SUNKEN, BF_RECT | BF_ADJUST);
    return S_OK;
  }

  draw_theme_(handle, hdc, PP_BAR, 0, &bar_rect, NULL);

  int bar_width = bar_rect.right - bar_rect.left;
  if (!extra.determinate) {
    // The glossy overlay for the indeterminate progress bar has a small pause
    // after each animation. We emulate this by adding an invisible margin the
    // animation has to traverse.
    int width_with_margin = bar_width + kIndeterminateOverlayPixelsPerSecond;
    int overlay_width = kIndeterminateOverlayWidth;
    RECT overlay_rect = bar_rect;
    overlay_rect.left += ComputeAnimationProgress(
        width_with_margin, overlay_width, kIndeterminateOverlayPixelsPerSecond,
        extra.animated_seconds);
    overlay_rect.right = overlay_rect.left + overlay_width;
    draw_theme_(handle, hdc, PP_MOVEOVERLAY, 0, &overlay_rect, &bar_rect);
    return S_OK;
  }

  // We care about the direction here because PP_CHUNK painting is asymmetric.
  // TODO(morrita): This RTL guess can be wrong.  We should pass in the
  // direction from WebKit.
  const DTBGOPTS value_draw_options = {
    sizeof(DTBGOPTS),
    (bar_rect.right == value_rect.right && bar_rect.left != value_rect.left) ?
        DTBG_MIRRORDC : 0u,
    bar_rect
  };

  // On Vista or later, the progress bar part has a single-block value part
  // and a glossy effect. The value part has exactly same height as the bar
  // part, so we don't need to shrink the rect.
  draw_theme_ex_(handle, hdc, PP_FILL, 0, &value_rect, &value_draw_options);

  RECT overlay_rect = value_rect;
  overlay_rect.left += ComputeAnimationProgress(
      bar_width, kDeterminateOverlayWidth, kDeterminateOverlayPixelsPerSecond,
      extra.animated_seconds);
  overlay_rect.right = overlay_rect.left + kDeterminateOverlayWidth;
  draw_theme_(handle, hdc, PP_MOVEOVERLAY, 0, &overlay_rect, &value_rect);
  return S_OK;
}

HRESULT NativeThemeWin::PaintWindowResizeGripper(HDC hdc,
                                                 const gfx::Rect& rect) const {
  HANDLE handle = GetThemeHandle(STATUS);
  RECT rect_win = rect.ToRECT();
  if (handle && draw_theme_) {
    // Paint the status bar gripper.  There doesn't seem to be a standard
    // gripper in Windows for the space between scrollbars.  This is pretty
    // close, but it's supposed to be painted over a status bar.
    return draw_theme_(handle, hdc, SP_GRIPPER, 0, &rect_win, NULL);
  }

  // Draw a windows classic scrollbar gripper.
  DrawFrameControl(hdc, &rect_win, DFC_SCROLL, DFCS_SCROLLSIZEGRIP);
  return S_OK;
}

HRESULT NativeThemeWin::PaintTabPanelBackground(HDC hdc,
                                                const gfx::Rect& rect) const {
  HANDLE handle = GetThemeHandle(TAB);
  RECT rect_win = rect.ToRECT();
  if (handle && draw_theme_)
    return draw_theme_(handle, hdc, TABP_BODY, 0, &rect_win, NULL);

  // Classic just renders a flat color background.
  FillRect(hdc, &rect_win, reinterpret_cast<HBRUSH>(COLOR_3DFACE + 1));
  return S_OK;
}

HRESULT NativeThemeWin::PaintTextField(
    HDC hdc,
    Part part,
    State state,
    const gfx::Rect& rect,
    const TextFieldExtraParams& extra) const {
  int state_id = ETS_NORMAL;
  switch (state) {
    case kDisabled:
      state_id = ETS_DISABLED;
      break;
    case kHovered:
      state_id = ETS_HOT;
      break;
    case kNormal:
      if (extra.is_read_only)
        state_id = ETS_READONLY;
      else if (extra.is_focused)
        state_id = ETS_FOCUSED;
      break;
    case kPressed:
      state_id = ETS_SELECTED;
      break;
    case kNumStates:
      NOTREACHED();
      break;
  }

  RECT rect_win = rect.ToRECT();
  return PaintTextField(hdc, EP_EDITTEXT, state_id, extra.classic_state,
                        &rect_win,
                        skia::SkColorToCOLORREF(extra.background_color),
                        extra.fill_content_area, extra.draw_edges);
}

HRESULT NativeThemeWin::PaintTextField(HDC hdc,
                                       int part_id,
                                       int state_id,
                                       int classic_state,
                                       RECT* rect,
                                       COLORREF color,
                                       bool fill_content_area,
                                       bool draw_edges) const {
  // TODO(ojan): http://b/1210017 Figure out how to give the ability to
  // exclude individual edges from being drawn.

  HANDLE handle = GetThemeHandle(TEXTFIELD);
  // TODO(mpcomplete): can we detect if the color is specified by the user,
  // and if not, just use the system color?
  // CreateSolidBrush() accepts a RGB value but alpha must be 0.
  base::win::ScopedGDIObject<HBRUSH> bg_brush(CreateSolidBrush(color));
  // DrawThemeBackgroundEx was introduced in XP SP2, so that it's possible
  // draw_theme_ex_ is NULL and draw_theme_ is non-null.
  if (!handle || (!draw_theme_ex_ && (!draw_theme_ || !draw_edges))) {
    // Draw it manually.
    if (draw_edges)
      DrawEdge(hdc, rect, EDGE_SUNKEN, BF_RECT | BF_ADJUST);

    if (fill_content_area) {
      FillRect(hdc, rect, (classic_state & DFCS_INACTIVE)
                              ? reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1)
                              : bg_brush.get());
    }
    return S_OK;
  }

  static const DTBGOPTS omit_border_options = {
    sizeof(DTBGOPTS),
    DTBG_OMITBORDER,
    { 0, 0, 0, 0 }
  };
  HRESULT hr = draw_theme_ex_ ?
    draw_theme_ex_(handle, hdc, part_id, state_id, rect,
                   draw_edges ? NULL : &omit_border_options) :
    draw_theme_(handle, hdc, part_id, state_id, rect, NULL);

  // TODO(maruel): Need to be fixed if get_theme_content_rect_ is NULL.
  if (fill_content_area && get_theme_content_rect_) {
    RECT content_rect;
    hr = get_theme_content_rect_(handle, hdc, part_id, state_id, rect,
                                  &content_rect);
    FillRect(hdc, &content_rect, bg_brush.get());
  }
  return hr;
}

HRESULT NativeThemeWin::PaintScaledTheme(HANDLE theme,
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
      HRESULT result = draw_theme_(theme, hdc, part_id, state_id, &bounds,
                                   NULL);
      SetWorldTransform(hdc, &save_transform);
      return result;
    }
  }
  RECT bounds = rect.ToRECT();
  return draw_theme_(theme, hdc, part_id, state_id, &bounds, NULL);
}

// static
NativeThemeWin::ThemeName NativeThemeWin::GetThemeName(Part part) {
  switch (part) {
    case kCheckbox:
    case kPushButton:
    case kRadio:
      return BUTTON;
    case kInnerSpinButton:
      return SPIN;
    case kMenuList:
    case kMenuCheck:
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
    case kScrollbarHorizontalThumb:
    case kScrollbarVerticalThumb:
    case kScrollbarHorizontalTrack:
    case kScrollbarVerticalTrack:
      return SCROLLBAR;
    case kSliderTrack:
    case kSliderThumb:
      return TRACKBAR;
    case kTextField:
      return TEXTFIELD;
    case kWindowResizeGripper:
      return STATUS;
    case kMenuCheckBackground:
    case kMenuPopupBackground:
    case kMenuItemBackground:
    case kScrollbarHorizontalGripper:
    case kScrollbarVerticalGripper:
    case kScrollbarCorner:
    case kTabPanelBackground:
    case kTrackbarThumb:
    case kTrackbarTrack:
    case kMaxPart:
      NOTREACHED();
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
    case kMenuCheck:
      return MENU_POPUPCHECK;
    case kMenuPopupArrow:
      return MENU_POPUPSUBMENU;
    case kMenuPopupGutter:
      return MENU_POPUPGUTTER;
    case kMenuPopupSeparator:
      return MENU_POPUPSEPARATOR;
    case kPushButton:
      return BP_PUSHBUTTON;
    case kRadio:
      return BP_RADIOBUTTON;
    case kScrollbarDownArrow:
    case kScrollbarLeftArrow:
    case kScrollbarRightArrow:
    case kScrollbarUpArrow:
      return SBP_ARROWBTN;
    case kScrollbarHorizontalThumb:
      return SBP_THUMBBTNHORZ;
    case kScrollbarVerticalThumb:
      return SBP_THUMBBTNVERT;
    case kWindowResizeGripper:
      return SP_GRIPPER;
    case kInnerSpinButton:
    case kMenuList:
    case kMenuCheckBackground:
    case kMenuPopupBackground:
    case kMenuItemBackground:
    case kProgressBar:
    case kScrollbarHorizontalTrack:
    case kScrollbarVerticalTrack:
    case kScrollbarHorizontalGripper:
    case kScrollbarVerticalGripper:
    case kScrollbarCorner:
    case kSliderTrack:
    case kSliderThumb:
    case kTabPanelBackground:
    case kTextField:
    case kTrackbarThumb:
    case kTrackbarTrack:
    case kMaxPart:
      NOTREACHED();
  }
  return 0;
}

int NativeThemeWin::GetWindowsState(Part part,
                                    State state,
                                    const ExtraParams& extra) {
  switch (part) {
    case kCheckbox:
      switch (state) {
        case kDisabled:
          return CBS_UNCHECKEDDISABLED;
        case kHovered:
          return CBS_UNCHECKEDHOT;
        case kNormal:
          return CBS_UNCHECKEDNORMAL;
        case kPressed:
          return CBS_UNCHECKEDPRESSED;
        case kNumStates:
          NOTREACHED();
          return 0;
      }
    case kMenuCheck:
      switch (state) {
        case kDisabled:
          return extra.menu_check.is_radio ?
              MC_BULLETDISABLED : MC_CHECKMARKDISABLED;
        case kHovered:
        case kNormal:
        case kPressed:
          return extra.menu_check.is_radio ?
              MC_BULLETNORMAL : MC_CHECKMARKNORMAL;
        case kNumStates:
          NOTREACHED();
          return 0;
      }
    case kMenuPopupArrow:
    case kMenuPopupGutter:
    case kMenuPopupSeparator:
      switch (state) {
        case kDisabled:
          return MBI_DISABLED;
        case kHovered:
          return MBI_HOT;
        case kNormal:
          return MBI_NORMAL;
        case kPressed:
          return MBI_PUSHED;
        case kNumStates:
          NOTREACHED();
          return 0;
      }
    case kPushButton:
      switch (state) {
        case kDisabled:
          return PBS_DISABLED;
        case kHovered:
          return PBS_HOT;
        case kNormal:
          return PBS_NORMAL;
        case kPressed:
          return PBS_PRESSED;
        case kNumStates:
          NOTREACHED();
          return 0;
      }
    case kRadio:
      switch (state) {
        case kDisabled:
          return RBS_UNCHECKEDDISABLED;
        case kHovered:
          return RBS_UNCHECKEDHOT;
        case kNormal:
          return RBS_UNCHECKEDNORMAL;
        case kPressed:
          return RBS_UNCHECKEDPRESSED;
        case kNumStates:
          NOTREACHED();
          return 0;
      }
    case kScrollbarDownArrow:
      switch (state) {
        case kDisabled:
          return ABS_DOWNDISABLED;
        case kHovered:
          return ABS_DOWNHOVER;
        case kNormal:
          return ABS_DOWNNORMAL;
        case kPressed:
          return ABS_DOWNPRESSED;
        case kNumStates:
          NOTREACHED();
          return 0;
      }
    case kScrollbarLeftArrow:
      switch (state) {
        case kDisabled:
          return ABS_LEFTDISABLED;
        case kHovered:
          return ABS_LEFTHOVER;
        case kNormal:
          return ABS_LEFTNORMAL;
        case kPressed:
          return ABS_LEFTPRESSED;
        case kNumStates:
          NOTREACHED();
          return 0;
      }
    case kScrollbarRightArrow:
      switch (state) {
        case kDisabled:
          return ABS_RIGHTDISABLED;
        case kHovered:
          return ABS_RIGHTHOVER;
        case kNormal:
          return ABS_RIGHTNORMAL;
        case kPressed:
          return ABS_RIGHTPRESSED;
        case kNumStates:
          NOTREACHED();
          return 0;
      }
      break;
    case kScrollbarUpArrow:
      switch (state) {
        case kDisabled:
          return ABS_UPDISABLED;
        case kHovered:
          return ABS_UPHOVER;
        case kNormal:
          return ABS_UPNORMAL;
        case kPressed:
          return ABS_UPPRESSED;
        case kNumStates:
          NOTREACHED();
          return 0;
      }
      break;
    case kScrollbarHorizontalThumb:
    case kScrollbarVerticalThumb:
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
          NOTREACHED();
          return 0;
      }
    case kWindowResizeGripper:
      switch (state) {
        case kDisabled:
        case kHovered:
        case kNormal:
        case kPressed:
          return 1;  // gripper has no windows state
        case kNumStates:
          NOTREACHED();
          return 0;
      }
    case kInnerSpinButton:
    case kMenuList:
    case kMenuCheckBackground:
    case kMenuPopupBackground:
    case kMenuItemBackground:
    case kProgressBar:
    case kScrollbarHorizontalTrack:
    case kScrollbarVerticalTrack:
    case kScrollbarHorizontalGripper:
    case kScrollbarVerticalGripper:
    case kScrollbarCorner:
    case kSliderTrack:
    case kSliderThumb:
    case kTabPanelBackground:
    case kTextField:
    case kTrackbarThumb:
    case kTrackbarTrack:
    case kMaxPart:
      NOTREACHED();
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
      NOTREACHED();
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
  if (!open_theme_ || theme_name < 0 || theme_name >= LAST)
    return 0;

  if (theme_handles_[theme_name])
    return theme_handles_[theme_name];

  // Not found, try to load it.
  HANDLE handle = 0;
  switch (theme_name) {
  case BUTTON:
    handle = open_theme_(NULL, L"Button");
    break;
  case LIST:
    handle = open_theme_(NULL, L"Listview");
    break;
  case MENU:
    handle = open_theme_(NULL, L"Menu");
    break;
  case MENULIST:
    handle = open_theme_(NULL, L"Combobox");
    break;
  case SCROLLBAR:
    handle = open_theme_(NULL, L"Scrollbar");
    break;
  case STATUS:
    handle = open_theme_(NULL, L"Status");
    break;
  case TAB:
    handle = open_theme_(NULL, L"Tab");
    break;
  case TEXTFIELD:
    handle = open_theme_(NULL, L"Edit");
    break;
  case TRACKBAR:
    handle = open_theme_(NULL, L"Trackbar");
    break;
  case WINDOW:
    handle = open_theme_(NULL, L"Window");
    break;
  case PROGRESS:
    handle = open_theme_(NULL, L"Progress");
    break;
  case SPIN:
    handle = open_theme_(NULL, L"Spin");
    break;
  case LAST:
    NOTREACHED();
    break;
  }
  theme_handles_[theme_name] = handle;
  return handle;
}

}  // namespace ui
