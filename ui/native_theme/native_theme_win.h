// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_NATIVE_THEME_WIN_H_
#define UI_NATIVE_THEME_NATIVE_THEME_WIN_H_

// A wrapper class for working with custom XP/Vista themes provided in
// uxtheme.dll.  This is a singleton class that can be grabbed using
// NativeThemeWin::instance().
// For more information on visual style parts and states, see:
// http://msdn.microsoft.com/library/default.asp?url=/library/en-us/shellcc/platform/commctls/userex/topics/partsandstates.asp

#include <map>

#include <windows.h>
#include <uxtheme.h>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/sys_color_change_listener.h"
#include "ui/native_theme/native_theme.h"

class SkCanvas;

namespace ui {

// Windows implementation of native theme class.
//
// At the moment, this class in in transition from an older API that consists
// of several PaintXXX methods to an API, inherited from the NativeTheme base
// class, that consists of a single Paint() method with a argument to indicate
// what kind of part to paint.
class NATIVE_THEME_EXPORT NativeThemeWin : public NativeTheme,
                                           public gfx::SysColorChangeListener {
 public:
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

  // Closes cached theme handles so we can unload the DLL or update our UI
  // for a theme change.
  static void CloseHandles();

  HRESULT PaintTextField(HDC hdc,
                         int part_id,
                         int state_id,
                         int classic_state,
                         RECT* rect,
                         COLORREF color,
                         bool fill_content_area,
                         bool draw_edges) const;

  // NativeTheme implementation:
  gfx::Size GetPartSize(Part part,
                        State state,
                        const ExtraParams& extra) const override;
  void Paint(cc::PaintCanvas* canvas,
             Part part,
             State state,
             const gfx::Rect& rect,
             const ExtraParams& extra) const override;
  SkColor GetSystemColor(ColorId color_id) const override;
  bool SupportsNinePatch(Part part) const override;
  gfx::Size GetNinePatchCanvasSize(Part part) const override;
  gfx::Rect GetNinePatchAperture(Part part) const override;
  bool UsesHighContrastColors() const override;

 protected:
  friend class NativeTheme;
  friend class base::NoDestructor<NativeThemeWin>;
  // Gets our singleton instance.
  static NativeThemeWin* instance();

  NativeThemeWin();
  ~NativeThemeWin() override;

 private:
  bool IsUsingHighContrastThemeInternal() const;
  void CloseHandlesInternal();

  // gfx::SysColorChangeListener implementation:
  void OnSysColorChange() override;

  // Update the locally cached set of system colors.
  void UpdateSystemColors();

  // Painting functions that paint to PaintCanvas.
  void PaintMenuSeparator(cc::PaintCanvas* canvas,
                          const MenuSeparatorExtraParams& params) const;
  void PaintMenuGutter(cc::PaintCanvas* canvas, const gfx::Rect& rect) const;
  void PaintMenuBackground(cc::PaintCanvas* canvas,
                           const gfx::Rect& rect) const;

  // Paint directly to canvas' HDC.
  void PaintDirect(SkCanvas* destination_canvas,
                   HDC hdc,
                   Part part,
                   State state,
                   const gfx::Rect& rect,
                   const ExtraParams& extra) const;

  // Create a temporary HDC, paint to that, clean up the alpha values in the
  // temporary HDC, and then blit the result to canvas.  This is to work around
  // the fact that Windows XP and some classic themes give bogus alpha values.
  void PaintIndirect(cc::PaintCanvas* destination_canvas,
                     Part part,
                     State state,
                     const gfx::Rect& rect,
                     const ExtraParams& extra) const;

  HRESULT GetThemePartSize(ThemeName themeName,
                           HDC hdc,
                           int part_id,
                           int state_id,
                           RECT* rect,
                           int ts,
                           SIZE* size) const;

  HRESULT PaintButton(HDC hdc,
                      State state,
                      const ButtonExtraParams& extra,
                      int part_id,
                      int state_id,
                      RECT* rect) const;

  // |arrow_direction| determines whether the arrow is pointing to the left or
  // to the right. In RTL locales, sub-menus open from right to left and
  // therefore the menu arrow should point to the left and not to the right.
  HRESULT PaintMenuArrow(HDC hdc,
                         State state,
                         const gfx::Rect& rect,
                         const MenuArrowExtraParams& extra) const;

  HRESULT PaintMenuCheck(HDC hdc,
                         State state,
                         const gfx::Rect& rect,
                         const MenuCheckExtraParams& extra) const;

  HRESULT PaintMenuCheckBackground(HDC hdc,
                                   State state,
                                   const gfx::Rect& rect) const;

  HRESULT PaintPushButton(HDC hdc,
                          Part part,
                          State state,
                          const gfx::Rect& rect,
                          const ButtonExtraParams& extra) const;

  HRESULT PaintRadioButton(HDC hdc,
                           Part part,
                           State state,
                           const gfx::Rect& rect,
                           const ButtonExtraParams& extra) const;

  HRESULT PaintCheckbox(HDC hdc,
                        Part part,
                        State state,
                        const gfx::Rect& rect,
                        const ButtonExtraParams& extra) const;

  HRESULT PaintMenuList(HDC hdc,
                        State state,
                        const gfx::Rect& rect,
                        const MenuListExtraParams& extra) const;

  // Paints a scrollbar arrow.  |classic_state| should have the appropriate
  // classic part number ORed in already.
  HRESULT PaintScrollbarArrow(HDC hdc,
                              Part part,
                              State state,
                              const gfx::Rect& rect,
                              const ScrollbarArrowExtraParams& extra) const;

  HRESULT PaintScrollbarThumb(HDC hdc,
                              Part part,
                              State state,
                              const gfx::Rect& rect,
                              const ScrollbarThumbExtraParams& extra) const;

  // This method is deprecated and will be removed in the near future.
  // Paints a scrollbar track section.  |align_rect| is only used in classic
  // mode, and makes sure the checkerboard pattern in |target_rect| is aligned
  // with one presumed to be in |align_rect|.
  HRESULT PaintScrollbarTrack(SkCanvas* canvas,
                              HDC hdc,
                              Part part,
                              State state,
                              const gfx::Rect& rect,
                              const ScrollbarTrackExtraParams& extra) const;

  HRESULT PaintSpinButton(HDC hdc,
                          Part part,
                          State state,
                          const gfx::Rect& rect,
                          const InnerSpinButtonExtraParams& extra) const;

  HRESULT PaintTrackbar(SkCanvas* canvas,
                        HDC hdc,
                        Part part,
                        State state,
                        const gfx::Rect& rect,
                        const TrackbarExtraParams& extra) const;

  HRESULT PaintProgressBar(HDC hdc,
                           const gfx::Rect& rect,
                           const ProgressBarExtraParams& extra) const;

  HRESULT PaintWindowResizeGripper(HDC hdc, const gfx::Rect& rect) const;

  HRESULT PaintTabPanelBackground(HDC hdc, const gfx::Rect& rect) const;

  HRESULT PaintTextField(HDC hdc,
                         Part part,
                         State state,
                         const gfx::Rect& rect,
                         const TextFieldExtraParams& extra) const;

  // Paints a theme part, with support for scene scaling in high-DPI mode.
  // |theme| is the theme handle. |hdc| is the handle for the device context.
  // |part_id| is the identifier for the part (e.g. thumb gripper). |state_id|
  // is the identifier for the rendering state of the part (e.g. hover). |rect|
  // is the bounds for rendering, expressed in logical coordinates.
  HRESULT PaintScaledTheme(HANDLE theme,
                           HDC hdc,
                           int part_id,
                           int state_id,
                           const gfx::Rect& rect) const;

  // Get the windows theme name/part/state.  These three helper functions are
  // used only by GetPartSize(), as each of the corresponding PaintXXX()
  // methods do further validation of the part and state that is required for
  // getting the size.
  static ThemeName GetThemeName(Part part);
  static int GetWindowsPart(Part part, State state, const ExtraParams& extra);
  static int GetWindowsState(Part part, State state, const ExtraParams& extra);

  HRESULT PaintFrameControl(HDC hdc,
                            const gfx::Rect& rect,
                            UINT type,
                            UINT state,
                            bool is_selected,
                            State control_state) const;

  // Returns a handle to the theme data.
  HANDLE GetThemeHandle(ThemeName theme_name) const;

  typedef HRESULT (WINAPI* DrawThemeBackgroundPtr)(HANDLE theme,
                                                   HDC hdc,
                                                   int part_id,
                                                   int state_id,
                                                   const RECT* rect,
                                                   const RECT* clip_rect);
  typedef HRESULT (WINAPI* DrawThemeBackgroundExPtr)(HANDLE theme,
                                                     HDC hdc,
                                                     int part_id,
                                                     int state_id,
                                                     const RECT* rect,
                                                     const DTBGOPTS* opts);
  typedef HRESULT (WINAPI* GetThemeColorPtr)(HANDLE hTheme,
                                             int part_id,
                                             int state_id,
                                             int prop_id,
                                             COLORREF* color);
  typedef HRESULT (WINAPI* GetThemeContentRectPtr)(HANDLE hTheme,
                                                   HDC hdc,
                                                   int part_id,
                                                   int state_id,
                                                   const RECT* rect,
                                                   RECT* content_rect);
  typedef HRESULT (WINAPI* GetThemePartSizePtr)(HANDLE hTheme,
                                                HDC hdc,
                                                int part_id,
                                                int state_id,
                                                RECT* rect,
                                                int ts,
                                                SIZE* size);
  typedef HANDLE (WINAPI* OpenThemeDataPtr)(HWND window,
                                            LPCWSTR class_list);
  typedef HRESULT (WINAPI* CloseThemeDataPtr)(HANDLE theme);

  typedef void (WINAPI* SetThemeAppPropertiesPtr) (DWORD flags);
  typedef BOOL (WINAPI* IsThemeActivePtr)();
  typedef HRESULT (WINAPI* GetThemeIntPtr)(HANDLE hTheme,
                                           int part_id,
                                           int state_id,
                                           int prop_id,
                                           int *value);

  // Function pointers into uxtheme.dll.
  DrawThemeBackgroundPtr draw_theme_;
  DrawThemeBackgroundExPtr draw_theme_ex_;
  GetThemeColorPtr get_theme_color_;
  GetThemeContentRectPtr get_theme_content_rect_;
  GetThemePartSizePtr get_theme_part_size_;
  OpenThemeDataPtr open_theme_;
  CloseThemeDataPtr close_theme_;

  // Handle to uxtheme.dll.
  HMODULE theme_dll_;

  // A cache of open theme handles.
  mutable HANDLE theme_handles_[LAST];

  // The system color change listener and the updated cache of system colors.
  gfx::ScopedSysColorChangeListener color_change_listener_;
  mutable std::map<int, SkColor> system_colors_;

  // Is a high contrast theme active?
  mutable bool is_using_high_contrast_;

  // Is |is_using_high_contrast_| valid?
  mutable bool is_using_high_contrast_valid_;

  DISALLOW_COPY_AND_ASSIGN(NativeThemeWin);
};

}  // namespace ui

#endif  // UI_NATIVE_THEME_NATIVE_THEME_WIN_H_
