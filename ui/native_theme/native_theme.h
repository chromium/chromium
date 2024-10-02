// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_NATIVE_THEME_H_
#define UI_NATIVE_THEME_NATIVE_THEME_H_

#include <map>
#include <optional>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/models/menu_separator_types.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider_key.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/native_theme/caption_style.h"
#include "ui/native_theme/native_theme_export.h"
#include "ui/native_theme/native_theme_observer.h"

namespace cc {
class PaintCanvas;
}

namespace gfx {
class Insets;
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
// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
    kFrameTopArea,
#endif
    kInnerSpinButton,
    kMenuList,
    kMenuPopupBackground,
#if BUILDFLAG(IS_WIN)
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

  // Enum used for kPageColors pref. Page Colors is a browser setting that can
  // be used to simulate forced colors mode. This enum should match its React
  // counterpart.
  enum PageColors {
    kOff = 0,
    kDusk = 1,
    kDesert = 2,
    kNightSky = 3,
    kWhite = 4,
    kHighContrast = 5,
    kMaxValue = kHighContrast,
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
    kCustom = 3,
    kMaxValue = kCustom,
  };

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

  // Each structure below holds extra information needed when painting a given
  // part.

  struct ButtonExtraParams {
    bool checked = false;
    bool indeterminate = false;  // Whether the button state is indeterminate.
    bool is_default = false;     // Whether the button is default button.
    bool is_focused = false;
    bool has_border = false;
    int classic_state = 0;  // Used on Windows when uxtheme is not available.
    SkColor background_color = gfx::kPlaceholderColor;
    float zoom = 0;
  };

  struct FrameTopAreaExtraParams {
    // Distinguishes between active (foreground) and inactive
    // (background) window frame styles.
    bool is_active = false;
    // True when Chromium renders the titlebar.  False when the window
    // manager renders the titlebar.
    bool use_custom_frame = false;
    // If the NativeTheme will paint a solid color, it should use
    // |default_background_color|.
    SkColor default_background_color = gfx::kPlaceholderColor;
  };

  enum class SpinArrowsDirection : int {
    kLeftRight,
    kUpDown,
  };

  struct InnerSpinButtonExtraParams {
    bool spin_up = false;
    bool read_only = false;
    SpinArrowsDirection spin_arrows_direction = SpinArrowsDirection::kUpDown;
    int classic_state = 0;  // Used on Windows when uxtheme is not available.
  };

  struct MenuArrowExtraParams {
    bool pointing_right = false;
    // Used for the disabled state to indicate if the item is both disabled and
    // selected.
    bool is_selected = false;
  };

  struct MenuCheckExtraParams {
    bool is_radio = false;
    // Used for the disabled state to indicate if the item is both disabled and
    // selected.
    bool is_selected = false;
  };

  struct MenuSeparatorExtraParams {
    raw_ptr<const gfx::Rect> paint_rect = nullptr;
    ui::ColorId color_id = ui::kColorMenuSeparator;
    MenuSeparatorType type = MenuSeparatorType::NORMAL_SEPARATOR;
  };

  struct MenuItemExtraParams {
    bool is_selected = false;
    int corner_radius = 0;
  };

  enum class ArrowDirection : int {
    kDown,
    kLeft,
    kRight,
  };

  struct NATIVE_THEME_EXPORT MenuListExtraParams {
    bool has_border = false;
    bool has_border_radius = false;
    int arrow_x = 0;
    int arrow_y = 0;
    int arrow_size = 0;
    ArrowDirection arrow_direction = ArrowDirection::kDown;
    SkColor arrow_color = gfx::kPlaceholderColor;
    SkColor background_color = gfx::kPlaceholderColor;
    int classic_state = 0;  // Used on Windows when uxtheme is not available.
    float zoom = 0;

    MenuListExtraParams();
    MenuListExtraParams(const MenuListExtraParams&);
    MenuListExtraParams& operator=(const MenuListExtraParams&);
  };

  struct MenuBackgroundExtraParams {
    int corner_radius = 0;
  };

  struct ProgressBarExtraParams {
    double animated_seconds = 0;
    bool determinate = false;
    int value_rect_x = 0;
    int value_rect_y = 0;
    int value_rect_width = 0;
    int value_rect_height = 0;
    float zoom = 0;
    bool is_horizontal = false;
  };

  struct ScrollbarArrowExtraParams {
    bool is_hovering = false;
    float zoom = 0;
    bool needs_rounded_corner = false;
    bool right_to_left = false;
    // These allow clients to directly override the color values to support
    // element-specific web platform CSS.
    std::optional<SkColor> thumb_color;
    std::optional<SkColor> track_color;
  };

  struct ScrollbarTrackExtraParams {
    bool is_upper = false;
    int track_x = 0;
    int track_y = 0;
    int track_width = 0;
    int track_height = 0;
    int classic_state = 0;  // Used on Windows when uxtheme is not available.
    // This allows clients to directly override the color values to support
    // element-specific web platform CSS.
    std::optional<SkColor> track_color;
  };

  struct ScrollbarThumbExtraParams {
    bool is_hovering = false;
    // This allows clients to directly override the color values to support
    // element-specific web platform CSS.
    std::optional<SkColor> thumb_color;
    bool is_thumb_minimal_mode = false;
    bool is_web_test = false;
  };

#if BUILDFLAG(IS_APPLE)
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
    bool is_hovering = false;
    bool is_overlay = false;
    ScrollbarOrientation orientation =
        ScrollbarOrientation::kVerticalOnRight;  // Used on Mac for drawing
                                                 // gradients.
    float scale_from_dip = 0;
    // These allow clients to directly override the color values to support
    // element-specific web platform CSS.
    std::optional<SkColor> thumb_color;
    std::optional<SkColor> track_color;
  };
#endif

  struct SliderExtraParams {
    bool vertical = false;
    bool in_drag = false;
    int thumb_x = 0;
    int thumb_y = 0;
    float zoom = 0;
    bool right_to_left = false;
  };

  struct NATIVE_THEME_EXPORT TextFieldExtraParams {
    bool is_text_area = false;
    bool is_listbox = false;
    SkColor background_color = gfx::kPlaceholderColor;
    bool is_read_only = false;
    bool is_focused = false;
    bool fill_content_area = false;
    bool draw_edges = false;
    int classic_state = 0;  // Used on Windows when uxtheme is not available.
    bool has_border = false;
    bool auto_complete_active = false;
    float zoom = 0;

    TextFieldExtraParams();
    TextFieldExtraParams(const TextFieldExtraParams&);
    TextFieldExtraParams& operator=(const TextFieldExtraParams&);
  };

  struct TrackbarExtraParams {
    bool vertical = false;
    int classic_state = 0;  // Used on Windows when uxtheme is not available.
  };

  using ExtraParams = absl::variant<ButtonExtraParams,
                                    FrameTopAreaExtraParams,
                                    InnerSpinButtonExtraParams,
                                    MenuArrowExtraParams,
                                    MenuCheckExtraParams,
                                    MenuItemExtraParams,
                                    MenuSeparatorExtraParams,
                                    MenuListExtraParams,
                                    MenuBackgroundExtraParams,
                                    ProgressBarExtraParams,
                                    ScrollbarArrowExtraParams,
#if BUILDFLAG(IS_APPLE)
                                    ScrollbarExtraParams,
#endif
                                    ScrollbarTrackExtraParams,
                                    ScrollbarThumbExtraParams,
                                    SliderExtraParams,
                                    TextFieldExtraParams,
                                    TrackbarExtraParams>;

  NativeTheme(const NativeTheme&) = delete;
  NativeTheme& operator=(const NativeTheme&) = delete;

  // Return the size of the part.
  virtual gfx::Size GetPartSize(Part part,
                                State state,
                                const ExtraParams& extra) const = 0;
  virtual int GetPaintedScrollbarTrackInset() const;

  virtual gfx::Insets GetScrollbarSolidColorThumbInsets(Part part) const;

  // Called if the theme uses solid color for scrollbar thumb.
  virtual SkColor4f GetScrollbarThumbColor(
      const ui::ColorProvider& color_provider,
      State state,
      const ScrollbarThumbExtraParams& extra_params) const;

  virtual float GetBorderRadiusForPart(Part part,
                                       float width,
                                       float height) const;

  // Paint the part to the canvas.
  virtual void Paint(
      cc::PaintCanvas* canvas,
      const ui::ColorProvider* color_provider,
      Part part,
      State state,
      const gfx::Rect& rect,
      const ExtraParams& extra,
      ColorScheme color_scheme = ColorScheme::kDefault,
      bool in_forced_colors = false,
      const std::optional<SkColor>& accent_color = std::nullopt) const = 0;

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

  // Returns the key corresponding to this native theme object.
  // Use `use_custom_frame` == true when Chromium renders the titlebar.
  // False when the window manager renders the titlebar (currently GTK only).
  ColorProviderKey GetColorProviderKey(
      scoped_refptr<ColorProviderKey::ThemeInitializerSupplier> custom_theme,
      bool use_custom_frame = true) const;

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
  virtual void NotifyOnNativeThemeUpdated();

  // Notify observers of caption style changes.
  virtual void NotifyOnCaptionStyleUpdated();

  // Notify observers of preferred contrast changes.
  virtual void NotifyOnPreferredContrastUpdated();

  // Returns whether the user has an explicit contrast preference.
  virtual bool UserHasContrastPreference() const;

  // Returns whether we are in forced colors mode, controlled by system
  // accessibility settings. Currently, Windows high contrast is the only system
  // setting that triggers forced colors mode.
  bool InForcedColorsMode() const;

  // Returns the PlatformHighContrastColorScheme used by the OS. Returns a value
  // other than kNone only if the default system color scheme is
  // kPlatformHighContrast.
  PlatformHighContrastColorScheme GetPlatformHighContrastColorScheme() const;

  // Returns true when the NativeTheme uses a light-on-dark color scheme. If
  // you're considering using this function to choose between two hard-coded
  // colors, you probably shouldn't. Instead, use ColorProvider::GetColor().
  virtual bool ShouldUseDarkColors() const;

  // Returns the user's current page colors.
  virtual PageColors GetPageColors() const;

  // Calculates and returns the current user preferred color scheme. The
  // base behavior is to set preferred color scheme to light or dark depending
  // on the state of dark mode.
  virtual PreferredColorScheme CalculatePreferredColorScheme() const;

  // Returns the OS-level user preferred color scheme. See the comment for
  // CalculatePreferredColorScheme() for details on how preferred color scheme
  // is calculated.
  virtual PreferredColorScheme GetPreferredColorScheme() const;

  // Returns the OS-level user preferred contrast.
  virtual PreferredContrast GetPreferredContrast() const;

  // Returns the OS-level user preferred transparency.
  virtual bool GetPrefersReducedTransparency() const;

  // Returns the OS-level inverted colors setting. (Classic invert NOT smart
  // invert)
  virtual bool GetInvertedColors() const;

  // Returns the system's caption style.
  virtual std::optional<CaptionStyle> GetSystemCaptionStyle() const;

  virtual ColorScheme GetDefaultSystemColorScheme() const;

  // Updates contrast-related theme states such as `forced_colors_`,
  // `page_colors_`, `preferred_contrast_` and `prefers_reduced_transparency_`
  // based on the `observed_theme`. Returns true if there's an update to any of
  // these states.
  bool UpdateContrastRelatedStates(const NativeTheme& observed_theme);

  virtual const std::map<SystemThemeColor, SkColor>& GetSystemColors() const;

  std::optional<SkColor> GetSystemThemeColor(
      SystemThemeColor theme_color) const;

  bool HasDifferentSystemColors(
      const std::map<SystemThemeColor, SkColor>& colors) const;

  void set_use_dark_colors(bool should_use_dark_colors) {
    should_use_dark_colors_ = should_use_dark_colors;
  }
  void set_forced_colors(bool forced_colors) { forced_colors_ = forced_colors; }
  void set_page_colors(PageColors page_colors) { page_colors_ = page_colors; }
  void set_preferred_color_scheme(PreferredColorScheme preferred_color_scheme) {
    preferred_color_scheme_ = preferred_color_scheme;
  }
  void set_prefers_reduced_transparency(bool prefers_reduced_transparency) {
    prefers_reduced_transparency_ = prefers_reduced_transparency;
  }
  void set_inverted_colors(bool inverted_colors) {
    inverted_colors_ = inverted_colors;
  }
  void SetPreferredContrast(PreferredContrast preferred_contrast);
  void set_system_colors(const std::map<SystemThemeColor, SkColor>& colors);
  ui::SystemTheme system_theme() const { return system_theme_; }

  // Set the user_color for ColorProviderKey.
  void set_user_color(std::optional<SkColor> user_color) {
    user_color_ = user_color;
  }
  std::optional<SkColor> user_color() const { return user_color_; }

  void set_scheme_variant(
      std::optional<ui::ColorProviderKey::SchemeVariant> scheme_variant) {
    scheme_variant_ = scheme_variant;
  }
  std::optional<ui::ColorProviderKey::SchemeVariant> scheme_variant() const {
    return scheme_variant_;
  }

  void set_should_use_system_accent_color(bool should_use_system_accent_color) {
    should_use_system_accent_color_ = should_use_system_accent_color;
  }
  bool should_use_system_accent_color() const {
    return should_use_system_accent_color_;
  }

  // On certain platforms, currently only Mac, there is a unique visual for
  // pressed states.
  virtual SkColor GetSystemButtonPressedColor(SkColor base_color) const;

  // Assign the focus-ring-appropriate alpha value to the provided base_color.
  virtual SkColor4f FocusRingColorForBaseColor(SkColor4f base_color) const;

  float AdjustBorderWidthByZoom(float border_width, float zoom_level) const;

  float AdjustBorderRadiusByZoom(Part part,
                                 float border_width,
                                 float zoom_level) const;

  // Returns the rate at which the text caret should blink. If 0, the caret
  // will not blink.
  base::TimeDelta GetCaretBlinkInterval() const;

  // Sets the rate at which the text caret should blink. Overrides any
  // platform values.
  void set_caret_blink_interval(
      std::optional<base::TimeDelta> caret_blink_interval) {
    caret_blink_interval_ = std::move(caret_blink_interval);
  }

  // Whether high contrast is forced via command-line flag.
  static bool IsForcedHighContrast();

  // Whether dark mode is forced via command-line flag.
  static bool IsForcedDarkMode();

 protected:
  explicit NativeTheme(
      bool should_only_use_dark_colors,
      ui::SystemTheme system_theme = ui::SystemTheme::kDefault);
  virtual ~NativeTheme();

  // Calculates and returns the current user preferred contrast.
  virtual PreferredContrast CalculatePreferredContrast() const;

  // A function to be called by native theme instances that need to set state
  // or listeners with the webinstance in order to provide correct native
  // platform behaviors.
  virtual void ConfigureWebInstance() {}

  // Gets the platform caret blink interval if it exists.
  virtual std::optional<base::TimeDelta> GetPlatformCaretBlinkInterval() const;

  // Allows one native theme to observe changes in another. For example, the
  // web native theme for Windows observes the corresponding ui native theme in
  // order to receive changes regarding the state of dark mode, forced colors
  // mode, preferred color scheme and preferred contrast.
  class NATIVE_THEME_EXPORT ColorSchemeNativeThemeObserver
      : public NativeThemeObserver {
   public:
    ColorSchemeNativeThemeObserver(NativeTheme* theme_to_update);

    ColorSchemeNativeThemeObserver(const ColorSchemeNativeThemeObserver&) =
        delete;
    ColorSchemeNativeThemeObserver& operator=(
        const ColorSchemeNativeThemeObserver&) = delete;

    ~ColorSchemeNativeThemeObserver() override;

   private:
    // ui::NativeThemeObserver:
    void OnNativeThemeUpdated(ui::NativeTheme* observed_theme) override;

    // The theme that gets updated when OnNativeThemeUpdated() is called.
    const raw_ptr<NativeTheme> theme_to_update_;
  };

  mutable std::map<SystemThemeColor, SkColor> system_colors_;

 private:
  // Observers to notify when the native theme changes.
  base::ObserverList<NativeThemeObserver>::UncheckedAndDanglingUntriaged
      native_theme_observers_;

  // User's primary color. Included in the `ColorProvider::Key` as the basis of
  // all generated colors.
  std::optional<SkColor> user_color_;

  // System color scheme variant. Used in `ColorProvider::Key` to specify the
  // transforms of `user_color_` which generate colors.
  std::optional<ui::ColorProviderKey::SchemeVariant> scheme_variant_;

  // Determines whether generated colors should express the system's accent
  // color if present.
  bool should_use_system_accent_color_ = true;

  bool should_use_dark_colors_ = false;
  const ui::SystemTheme system_theme_;
  bool forced_colors_ = false;
  PageColors page_colors_ = PageColors::kOff;
  bool prefers_reduced_transparency_ = false;
  bool inverted_colors_ = false;
  PreferredColorScheme preferred_color_scheme_ = PreferredColorScheme::kLight;
  PreferredContrast preferred_contrast_ = PreferredContrast::kNoPreference;
  std::optional<base::TimeDelta> caret_blink_interval_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace ui

#endif  // UI_NATIVE_THEME_NATIVE_THEME_H_
