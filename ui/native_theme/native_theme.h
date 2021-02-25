// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_NATIVE_THEME_H_
#define UI_NATIVE_THEME_NATIVE_THEME_H_

#include <map>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "cc/paint/paint_canvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/models/menu_separator_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/native_theme/caption_style.h"
#include "ui/native_theme/native_theme_color_id.h"
#include "ui/native_theme/native_theme_export.h"
#include "ui/native_theme/native_theme_observer.h"

namespace gfx {
class Rect;
class Size;
}

namespace ui {

// This class supports drawing UI controls (like buttons, text fields, lists,
// comboboxes, etc) that look like the native UI controls of the underlying
// platform, such as Windows or Linux. It also supplies default colors for
// dialog box backgrounds, etc., which are obtained from the system theme where
// possible.
//
// The supported control types are listed in the Part enum.  These parts can be
// in any state given by the State enum, where the actual definition of the
// state is part-specific. The supported colors are listed in the ColorId enum.
//
// Some parts require more information than simply the state in order to be
// drawn correctly, and this information is given to the Paint() method via the
// ExtraParams union.  Each part that requires more information has its own
// field in the union.
//
// NativeTheme also supports getting the default size of a given part with
// the GetPartSize() method.
class NATIVE_THEME_EXPORT NativeTheme {
 public:
  // The part to be painted / sized.
  enum Part {
    kCheckbox,
// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
    kFrameTopArea,
#endif
    kInnerSpinButton,
    kMenuList,
    kMenuPopupBackground,
#if defined(OS_WIN)
    kMenuCheck,
    kMenuCheckBackground,
    kMenuPopupArrow,
    kMenuPopupGutter,
#endif
    kMenuPopupSeparator,
    kMenuItemBackground,
    kProgressBar,
    kPushButton,
    kRadio,

    // The order of the arrow enums is important, do not change without also
    // changing the code in platform implementations.
    kScrollbarDownArrow,
    kScrollbarLeftArrow,
    kScrollbarRightArrow,
    kScrollbarUpArrow,

    kScrollbarHorizontalThumb,
    kScrollbarVerticalThumb,
    kScrollbarHorizontalTrack,
    kScrollbarVerticalTrack,
    kScrollbarHorizontalGripper,
    kScrollbarVerticalGripper,
    // The corner is drawn when there is both a horizontal and vertical
    // scrollbar.
    kScrollbarCorner,
    kSliderTrack,
    kSliderThumb,
    kTabPanelBackground,
    kTextField,
    kTrackbarThumb,
    kTrackbarTrack,
    kWindowResizeGripper,
    kMaxPart,
  };

  // The state of the part.
  enum State {
    // IDs defined as specific values for use in arrays.
    kDisabled = 0,
    kHovered  = 1,
    kNormal   = 2,
    kPressed  = 3,
    kNumStates = kPressed + 1,
  };

  // OS-level preferred color scheme. (Ex. high contrast or dark mode color
  // preference.)
  enum class PreferredColorScheme {
    kDark = 0,
    kLight = 1,
    kMaxValue = kLight,
  };

  // OS-level preferred contrast. (Ex. high contrast or increased contrast.)
  enum class PreferredContrast {
    kNoPreference = 0,
    kMore = 1,
    kLess = 2,
    kMaxValue = kLess,
  };

  // IMPORTANT!
  // This enum is reporting in metrics. Do not reorder; add additional values at
  // the end.
  //
  // This represents the OS-level high contrast theme. kNone unless the default
  // system color scheme is kPlatformHighContrast.
  enum class PlatformHighContrastColorScheme {
    kNone = 0,
    kDark = 1,
    kLight = 2,
    kMaxValue = kLight,
  };

  // The color scheme used for painting the native controls.
  enum class ColorScheme {
    kDefault,
    kLight,
    kDark,
    kPlatformHighContrast,  // When the platform is providing HC colors (eg.
                            // Win)
  };

  // This enum represents the available unique security chip color states.
  enum class SecurityChipColorId {
    DEFAULT,
    SECURE,
    SECURE_WITH_CERT,
    DANGEROUS,
  };

  // Each structure below holds extra information needed when painting a given
  // part.

  struct ButtonExtraParams {
    bool checked;
    bool indeterminate;  // Whether the button state is indeterminate.
    bool is_default;  // Whether the button is default button.
    bool is_focused;
    bool has_border;
    int classic_state;  // Used on Windows when uxtheme is not available.
    SkColor background_color;
    float zoom;
  };

  struct FrameTopAreaExtraParams {
    // Distinguishes between active (foreground) and inactive
    // (background) window frame styles.
    bool is_active;
    // True when Chromium renders the titlebar.  False when the window
    // manager renders the titlebar.
    bool use_custom_frame;
    // If the NativeTheme will paint a solid color, it should use
    // |default_background_color|.
    SkColor default_background_color;
  };

  struct InnerSpinButtonExtraParams {
    bool spin_up;
    bool read_only;
    int classic_state;  // Used on Windows when uxtheme is not available.
  };

  struct MenuArrowExtraParams {
    bool pointing_right;
    // Used for the disabled state to indicate if the item is both disabled and
    // selected.
    bool is_selected;
  };

  struct MenuCheckExtraParams {
    bool is_radio;
    // Used for the disabled state to indicate if the item is both disabled and
    // selected.
    bool is_selected;
  };

  struct MenuSeparatorExtraParams {
    const gfx::Rect* paint_rect;
    MenuSeparatorType type;
  };

  struct MenuItemExtraParams {
    bool is_selected;
    int corner_radius;
  };

  struct MenuListExtraParams {
    bool has_border;
    bool has_border_radius;
    int arrow_x;
    int arrow_y;
    int arrow_size;
    SkColor arrow_color;
    SkColor background_color;
    int classic_state;  // Used on Windows when uxtheme is not available.
    float zoom;
  };

  struct MenuBackgroundExtraParams {
    int corner_radius;
  };

  struct ProgressBarExtraParams {
    double animated_seconds;
    bool determinate;
    int value_rect_x;
    int value_rect_y;
    int value_rect_width;
    int value_rect_height;
    float zoom;
  };

  struct ScrollbarArrowExtraParams {
    bool is_hovering;
    float zoom;
    bool right_to_left;
  };

  struct ScrollbarTrackExtraParams {
    bool is_upper;
    int track_x;
    int track_y;
    int track_width;
    int track_height;
    int classic_state;  // Used on Windows when uxtheme is not available.
  };

  enum ScrollbarOverlayColorTheme {
    ScrollbarOverlayColorThemeDark,
    ScrollbarOverlayColorThemeLight
  };

  struct ScrollbarThumbExtraParams {
    bool is_hovering;
    ScrollbarOverlayColorTheme scrollbar_theme;
  };

#if defined(OS_APPLE)
  enum ScrollbarOrientation {
    // Vertical scrollbar on the right side of content.
    kVerticalOnRight,
    // Vertical scrollbar on the left side of content.
    kVerticalOnLeft,
    // Horizontal scrollbar (on the bottom of content).
    kHorizontal,
  };

  // A unique set of scrollbar params. Currently needed for Mac.
  struct ScrollbarExtraParams {
    bool is_hovering;
    bool is_overlay;
    ScrollbarOverlayColorTheme scrollbar_theme;
    ScrollbarOrientation orientation;  // Used on Mac for drawing gradients.
  };
#endif

  struct SliderExtraParams {
    bool vertical;
    bool in_drag;
    int thumb_x;
    int thumb_y;
    float zoom;
    bool right_to_left;
  };

  struct TextFieldExtraParams {
    bool is_text_area;
    bool is_listbox;
    SkColor background_color;
    bool is_read_only;
    bool is_focused;
    bool fill_content_area;
    bool draw_edges;
    int classic_state;  // Used on Windows when uxtheme is not available.
    bool has_border;
    bool auto_complete_active;
    float zoom;
  };

  struct TrackbarExtraParams {
    bool vertical;
    int classic_state;  // Used on Windows when uxtheme is not available.
  };

  union NATIVE_THEME_EXPORT ExtraParams {
    ExtraParams();
    ExtraParams(const ExtraParams& other);

    ButtonExtraParams button;
    FrameTopAreaExtraParams frame_top_area;
    InnerSpinButtonExtraParams inner_spin;
    MenuArrowExtraParams menu_arrow;
    MenuCheckExtraParams menu_check;
    MenuItemExtraParams menu_item;
    MenuSeparatorExtraParams menu_separator;
    MenuListExtraParams menu_list;
    MenuBackgroundExtraParams menu_background;
    ProgressBarExtraParams progress_bar;
    ScrollbarArrowExtraParams scrollbar_arrow;
#if defined(OS_APPLE)
    ScrollbarExtraParams scrollbar_extra;
#endif
    ScrollbarTrackExtraParams scrollbar_track;
    ScrollbarThumbExtraParams scrollbar_thumb;
    SliderExtraParams slider;
    TextFieldExtraParams text_field;
    TrackbarExtraParams trackbar;
  };

  // Return the size of the part.
  virtual gfx::Size GetPartSize(Part part,
                                State state,
                                const ExtraParams& extra) const = 0;

  virtual float GetBorderRadiusForPart(Part part,
                                       float width,
                                       float height) const;

  // Paint the part to the canvas.
  virtual void Paint(
      cc::PaintCanvas* canvas,
      Part part,
      State state,
      const gfx::Rect& rect,
      const ExtraParams& extra,
      ColorScheme color_scheme = ColorScheme::kDefault) const = 0;

  // Paint part during state transition, used for overlay scrollbar state
  // transition animation.
  virtual void PaintStateTransition(cc::PaintCanvas* canvas,
                                    Part part,
                                    State startState,
                                    State endState,
                                    double progress,
                                    const gfx::Rect& rect,
                                    ScrollbarOverlayColorTheme theme) const {}

  // Returns whether the theme uses a nine-patch resource for the given part.
  // If true, calling code should always paint into a canvas the size of which
  // can be gotten from GetNinePatchCanvasSize.
  virtual bool SupportsNinePatch(Part part) const = 0;

  // If the part paints into a nine-patch resource, the size of the canvas
  // which should be painted into.
  virtual gfx::Size GetNinePatchCanvasSize(Part part) const = 0;

  // If the part paints into a nine-patch resource, the rect in the canvas
  // which defines the center tile. This is the tile that should be resized out
  // when the part is resized.
  virtual gfx::Rect GetNinePatchAperture(Part part) const = 0;

  // Colors for GetSystemColor().
  enum ColorId {
#define OP(enum_name) enum_name
    NATIVE_THEME_COLOR_IDS,
#undef OP

    kColorId_NumColors,
  };

  enum class SystemThemeColor {
    kNotSupported,
    kButtonFace,
    kButtonText,
    kGrayText,
    kHighlight,
    kHighlightText,
    kHotlight,
    kMenuHighlight,
    kScrollbar,
    kWindow,
    kWindowText,
    kMaxValue = kWindowText,
  };

  // Return a color from the system theme.
  virtual SkColor GetSystemColor(
      ColorId color_id,
      ColorScheme color_scheme = ColorScheme::kDefault) const;

  // Returns a shared instance of the native theme that should be used for web
  // rendering. Do not use it in a normal application context (i.e. browser).
  // The returned object should not be deleted by the caller. This function is
  // not thread safe and should only be called from the UI thread. Each port of
  // NativeTheme should provide its own implementation of this function,
  // returning the port's subclass.
  static NativeTheme* GetInstanceForWeb();

  // Returns a shared instance of the default native theme for native UI.
  static NativeTheme* GetInstanceForNativeUi();

  // Returns a shared instance of the native theme for incognito UI.
  static NativeTheme* GetInstanceForDarkUI();

  // Whether OS-level dark mode is available in the current OS.
  static bool SystemDarkModeSupported();

  // Add or remove observers to be notified when the native theme changes.
  void AddObserver(NativeThemeObserver* observer);
  void RemoveObserver(NativeThemeObserver* observer);

  // Notify observers of native theme changes.
  virtual void NotifyObservers();

  // Returns whether the user has an explicit contrast preference, i.e. whether
  // we are in forced colors mode or PreferredContrast is set.
  virtual bool UserHasContrastPreference() const;

  // Returns whether we are in forced colors mode, controlled by system
  // accessibility settings. Currently, Windows high contrast is the only system
  // setting that triggers forced colors mode.
  virtual bool InForcedColorsMode() const;

  // Returns the PlatformHighContrastColorScheme used by the OS. Returns a value
  // other than kNone only if the default system color scheme is
  // kPlatformHighContrast.
  PlatformHighContrastColorScheme GetPlatformHighContrastColorScheme() const;

  // Returns true when the NativeTheme uses a light-on-dark color scheme. If
  // you're considering using this function to choose between two hard-coded
  // colors, you probably shouldn't. Instead, use GetSystemColor().
  virtual bool ShouldUseDarkColors() const;

  // Returns the OS-level user preferred color scheme. See the comment for
  // CalculatePreferredColorScheme() for details on how preferred color scheme
  // is calculated.
  virtual PreferredColorScheme GetPreferredColorScheme() const;

  // Returns the OS-level user preferred contrast.
  virtual PreferredContrast GetPreferredContrast() const;

  // Returns the system's caption style.
  virtual base::Optional<CaptionStyle> GetSystemCaptionStyle() const;

  virtual ColorScheme GetDefaultSystemColorScheme() const;

  virtual const std::map<SystemThemeColor, SkColor>& GetSystemColors() const;

  base::Optional<SkColor> GetSystemThemeColor(
      SystemThemeColor theme_color) const;

  bool HasDifferentSystemColors(
      const std::map<SystemThemeColor, SkColor>& colors) const;

  void set_use_dark_colors(bool should_use_dark_colors) {
    should_use_dark_colors_ = should_use_dark_colors;
  }
  void set_forced_colors(bool forced_colors) { forced_colors_ = forced_colors; }
  void set_preferred_color_scheme(PreferredColorScheme preferred_color_scheme) {
    preferred_color_scheme_ = preferred_color_scheme;
  }
  void set_preferred_contrast(PreferredContrast preferred_contrast) {
    preferred_contrast_ = preferred_contrast;
  }
  void set_system_colors(const std::map<SystemThemeColor, SkColor>& colors);

  // Updates the state of dark mode, forced colors mode, and the map of system
  // colors. Returns true if NativeTheme was updated as a result, or false if
  // the state of NativeTheme was untouched.
  bool UpdateSystemColorInfo(
      bool is_dark_mode,
      bool forced_colors,
      const base::flat_map<SystemThemeColor, uint32_t>& colors);

  // On certain platforms, currently only Mac, there is a unique visual for
  // pressed states.
  virtual SkColor GetSystemButtonPressedColor(SkColor base_color) const;

  // Assign the focus-ring-appropriate alpha value to the provided base_color.
  virtual SkColor FocusRingColorForBaseColor(SkColor base_color) const;

 protected:
  explicit NativeTheme(bool should_only_use_dark_colors);
  virtual ~NativeTheme();

  // Whether high contrast is forced via command-line flag.
  bool IsForcedHighContrast() const;
  // Whether dark mode is forced via command-line flag.
  bool IsForcedDarkMode() const;

  // Calculates and returns the current user preferred color scheme. The
  // base behavior is to set preferred color scheme to light or dark depending
  // on the state of dark mode.
  //
  // Some platforms override this behavior. On Windows, for example, we also
  // look at the high contrast setting. If high contrast is enabled, the
  // preferred color scheme calculation will ignore the state of dark mode.
  // Instead, preferred color scheme will be light, or dark depending on the OS
  // high contrast theme. If high contrast is off, the preferred color scheme
  // calculation will follow the default behavior.
  virtual PreferredColorScheme CalculatePreferredColorScheme() const;

  // Calculates and returns the current user preferred contrast.
  virtual PreferredContrast CalculatePreferredContrast() const;

  // A function to be called by native theme instances that need to set state
  // or listeners with the webinstance in order to provide correct native
  // platform behaviors.
  virtual void ConfigureWebInstance() {}

  // Allows one native theme to observe changes in another. For example, the
  // web native theme for Windows observes the corresponding ui native theme in
  // order to receive changes regarding the state of dark mode, forced colors
  // mode, preferred color scheme and preferred contrast.
  class NATIVE_THEME_EXPORT ColorSchemeNativeThemeObserver
      : public NativeThemeObserver {
   public:
    ColorSchemeNativeThemeObserver(NativeTheme* theme_to_update);
    ~ColorSchemeNativeThemeObserver() override;

   private:
    // ui::NativeThemeObserver:
    void OnNativeThemeUpdated(ui::NativeTheme* observed_theme) override;

    // The theme that gets updated when OnNativeThemeUpdated() is called.
    NativeTheme* const theme_to_update_;

    DISALLOW_COPY_AND_ASSIGN(ColorSchemeNativeThemeObserver);
  };

  mutable std::map<SystemThemeColor, SkColor> system_colors_;

 private:
  // Observers to notify when the native theme changes.
  base::ObserverList<NativeThemeObserver>::Unchecked native_theme_observers_;

  bool should_use_dark_colors_ = false;
  bool forced_colors_ = false;
  PreferredColorScheme preferred_color_scheme_ = PreferredColorScheme::kLight;
  PreferredContrast preferred_contrast_ = PreferredContrast::kNoPreference;

  DISALLOW_COPY_AND_ASSIGN(NativeTheme);
};

}  // namespace ui

#endif  // UI_NATIVE_THEME_NATIVE_THEME_H_
