// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_NATIVE_THEME_H_
#define UI_NATIVE_THEME_NATIVE_THEME_H_

#include <stddef.h>

#include <optional>
#include <variant>

#include "base/callback_list.h"
#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/models/menu_separator_types.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/system_theme.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"

namespace cc {
class PaintCanvas;
}

namespace gfx {
class Rect;
}

namespace ui {

class ColorProvider;
class NativeThemeObserver;

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
class COMPONENT_EXPORT(NATIVE_THEME) NativeTheme {
 public:
  // A part being sized or painted.
  enum Part {
    kCheckbox,
#if BUILDFLAG(IS_LINUX)
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

    // The order of these enums is important, do not change without also
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

  // The state of some part being sized or painted.
  enum State {
    // CAUTION: These values are used as array indexes.
    kDisabled = 0,
    kHovered = 1,
    kNormal = 2,
    kPressed = 3,
    kNumStates = kPressed + 1,
  };

  enum class PreferredColorScheme {
    kNoPreference = 0,
    kLight = 1,
    kDark = 2,
    kMaxValue = kDark,
  };

  enum class PreferredContrast {
    kNoPreference = 0,
    kMore = 1,
    kLess = 2,
    kCustom = 3,  // E.g. forced colors outside of a contrast-related setting.
    kMaxValue = kCustom,
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
    ColorId color_id = kColorMenuSeparator;
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

  struct COMPONENT_EXPORT(NATIVE_THEME) MenuListExtraParams {
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
    std::optional<SkColor> track_color;
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

  struct COMPONENT_EXPORT(NATIVE_THEME) TextFieldExtraParams {
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

  using ExtraParams = std::variant<ButtonExtraParams,
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

  // Creating an instance of this class prevents `NotifyOnNativeThemeUpdated()`
  // from having an effect in any `NativeTheme` instance until no scopers
  // remain. When the last scoper is destroyed, any such delayed notifications
  // will be fired.
  class [[maybe_unused, nodiscard]] COMPONENT_EXPORT(NATIVE_THEME)
      UpdateNotificationDelayScoper {
   public:
    UpdateNotificationDelayScoper();
    UpdateNotificationDelayScoper(const UpdateNotificationDelayScoper&);
    UpdateNotificationDelayScoper(UpdateNotificationDelayScoper&&);
    UpdateNotificationDelayScoper& operator=(
        const UpdateNotificationDelayScoper&) = default;
    UpdateNotificationDelayScoper& operator=(UpdateNotificationDelayScoper&&) =
        default;
    ~UpdateNotificationDelayScoper();

    static bool exists(base::PassKey<NativeTheme>) { return !!num_instances_; }

    static base::CallbackListSubscription RegisterCallback(
        base::PassKey<NativeTheme>,
        base::OnceClosure cb);

   private:
    static base::OnceClosureList& GetDelayedNotifications();

    static size_t num_instances_;
  };

  NativeTheme(const NativeTheme&) = delete;
  NativeTheme& operator=(const NativeTheme&) = delete;

  // Returns shared instances of the default native theme for native UI or the
  // web, respectively.
  static NativeTheme* GetInstanceForNativeUi();
  static NativeTheme* GetInstanceForWeb();

  // Convenience methods to scale a width/radius by a zoom factor.
  static float AdjustBorderWidthByZoom(float border_width, float zoom_level);
  static float AdjustBorderRadiusByZoom(Part part,
                                        float border_radius,
                                        float zoom_level);

  virtual gfx::Size GetPartSize(Part part,
                                State state,
                                const ExtraParams& extra_params) const;

  virtual int GetPaintedScrollbarTrackInset() const;

  virtual gfx::Insets GetScrollbarSolidColorThumbInsets(Part part) const;

  virtual float GetBorderRadiusForPart(Part part,
                                       float width,
                                       float height) const;

  // Returns whether the theme uses a nine-patch resource for the given part.
  // If true, calling code should always paint into a canvas the size of which
  // can be gotten from GetNinePatchCanvasSize.
  virtual bool SupportsNinePatch(Part part) const;

  // If the part paints into a nine-patch resource, the size of the canvas
  // which should be painted into.
  virtual gfx::Size GetNinePatchCanvasSize(Part part) const;

  // If the part paints into a nine-patch resource, the rect in the canvas
  // which defines the center tile. This is the tile that should be resized out
  // when the part is resized.
  virtual gfx::Rect GetNinePatchAperture(Part part) const;

  // The scrollbar thumb color, if the theme uses a solid color for the
  // scrollbar thumb.
  virtual SkColor GetScrollbarThumbColor(
      const ColorProvider* color_provider,
      State state,
      const ScrollbarThumbExtraParams& extra_params) const;

  // Returns the color the toolkit would use for a pressed button that has an
  // unpressed color of `base_color`.
  virtual SkColor GetSystemButtonPressedColor(SkColor base_color) const;

  // Registers this instance as an observer of `OsSettingsProvider` changes.
  // This should not be called on an instance marked as the "associated web
  // instance" of another theme, since in that case the other theme should
  // notify about setting changes as necessary.
  void BeginObservingOsSettingChanges();

  // Adds or removes observers to be notified when the native theme changes.
  void AddObserver(NativeThemeObserver* observer);
  void RemoveObserver(NativeThemeObserver* observer);

  // Notifies observers that something has changed and they should reload
  // settings if needed. This also resets the color provider cache.
  // CAUTION: This is expensive; minimize unnecessary calls.
  void NotifyOnNativeThemeUpdated();

  // TODO(pkasting): Consider combining this with
  // `NotifyOnNativeThemeUpdated()`. This would make it easy to move the
  // underpinnings to the `OsSettingsProvider`, as well as replace
  // `NativeThemeObserver` with a `CallbackList`.
  void NotifyOnCaptionStyleUpdated();

  // Paints the provided `part`/`state`. This is largely a wrapper around
  // `PaintImpl()`.
  void Paint(
      cc::PaintCanvas* canvas,
      const ColorProvider* color_provider,
      Part part,
      State state,
      const gfx::Rect& rect,
      const ExtraParams& extra_params,
      bool forced_colors = false,
      PreferredColorScheme color_scheme = PreferredColorScheme::kNoPreference,
      PreferredContrast contrast = PreferredContrast::kNoPreference,
      std::optional<SkColor> accent_color = std::nullopt) const;

  // Returns the key corresponding to this native theme object.
  // Use `use_custom_frame` == true when Chromium renders the titlebar.
  // False when the window manager renders the titlebar (currently GTK only).
  ColorProviderKey GetColorProviderKey(
      scoped_refptr<ColorProviderKey::ThemeInitializerSupplier> custom_theme,
      bool use_custom_frame = true) const;

  // Accessors.
  //
  // NOTE: Be very cautious about using the setters here.
  //   * Tests generally should not modify `NativeTheme` state; if the goal is
  //     to pretend the underlying system is in a particular state, use
  //     `MockOsSettingsProvider` instead.
  //
  //   * The values set below may be overwritten automatically, e.g. when system
  //     settings change; so if the goal is to override system-native behavior,
  //     "fire and forget" usage is insufficient.
  //
  //   * To avoid jank from repeated notifications, these do not automatically
  //     call `NotifyOnNativeThemeUpdated()`. Failing to call that manually
  //     after using them typically results in cryptic bugs.
  //
  // TODO(pkasting): To address the third point, consider using
  // `UpdateNotificationDelayScoper` everywhere that currently calls these
  // setters or writes directly to the underlying members. Then make the setters
  // call `NotifyOnNativeThemeUpdated()` whenever the actual value changes and
  // change all direct writes to use the setters. At that point forgetting to
  // notify will be impossible, but we will only get one such notification.

  SystemTheme system_theme() const { return system_theme_; }

  bool use_overlay_scrollbar() const { return use_overlay_scrollbar_; }
  void set_use_overlay_scrollbar(bool use_overlay_scrollbar) {
    use_overlay_scrollbar_ = use_overlay_scrollbar;
  }

  ColorProviderKey::ForcedColors forced_colors() const {
    return forced_colors_;
  }
  void set_forced_colors(ColorProviderKey::ForcedColors forced_colors) {
    forced_colors_ = forced_colors;
  }

  PreferredColorScheme preferred_color_scheme() const {
    return preferred_color_scheme_;
  }
  void set_preferred_color_scheme(PreferredColorScheme preferred_color_scheme) {
    preferred_color_scheme_ = preferred_color_scheme;
  }

  PreferredContrast preferred_contrast() const { return preferred_contrast_; }
  void set_preferred_contrast(PreferredContrast preferred_contrast) {
    preferred_contrast_ = preferred_contrast;
  }

  bool prefers_reduced_transparency() const {
    return prefers_reduced_transparency_;
  }

  bool inverted_colors() const { return inverted_colors_; }

  std::optional<SkColor> user_color() const { return user_color_; }
  void set_user_color(std::optional<SkColor> user_color) {
    user_color_ = user_color;
  }

  std::optional<ColorProviderKey::SchemeVariant> scheme_variant() const {
    return scheme_variant_;
  }

  base::TimeDelta caret_blink_interval() const { return caret_blink_interval_; }
  void set_caret_blink_interval(base::TimeDelta caret_blink_interval) {
    caret_blink_interval_ = caret_blink_interval;
  }

 protected:
  explicit NativeTheme(SystemTheme system_theme = SystemTheme::kDefault);
  virtual ~NativeTheme();

  // Whether dark mode is forced via command-line flag.
  static bool IsForcedDarkMode();

  // Whether high contrast is forced via command-line flag.
  static bool IsForcedHighContrast();

  // Paints the provided `part`/`state`. Subclasses must override this if they
  // want visible output.
  virtual void PaintImpl(cc::PaintCanvas* canvas,
                         const ColorProvider* color_provider,
                         Part part,
                         State state,
                         const gfx::Rect& rect,
                         const ExtraParams& extra_params,
                         bool forced_colors,
                         bool dark_mode,
                         PreferredContrast contrast,
                         std::optional<SkColor> accent_color) const {}

  // Common implementation used by several subclasses.
  virtual void PaintMenuItemBackground(
      cc::PaintCanvas* canvas,
      const ColorProvider* color_provider,
      State state,
      const gfx::Rect& rect,
      const MenuItemExtraParams& extra_params) const;

  // Called when toolkit settings change. Updates affected variables. If
  // anything changes or `force_notify` is set, notifies observers.
  virtual void OnToolkitSettingsChanged(bool force_notify);

  // Instructs this theme instance to mirror various appearance settings to
  // `associated_web_instance` when they change.
  void SetAssociatedWebInstance(NativeTheme* associated_web_instance);

  // Updates the settings of any `associated_web_instance_` to match this
  // instance's current settings. Returns whether anything was changed.
  bool UpdateWebInstance() const;

 private:
  // Updates web instance and notifies observers something has changed.
  void NotifyOnNativeThemeUpdatedImpl();

  // Updates variables affected by toolkit settings and returns whether anything
  // changed as a result.
  bool UpdateVariablesForToolkitSettings();

  // Calculates and returns appropriate values based on flags and toolkit.
  ColorProviderKey::ForcedColors CalculateForcedColors() const;
  PreferredColorScheme CalculatePreferredColorScheme() const;
  PreferredContrast CalculatePreferredContrast() const;

  base::CallbackListSubscription os_settings_changed_subscription_;
  base::CallbackListSubscription update_delay_subscription_;
  base::ObserverList<NativeThemeObserver> native_theme_observers_;
  SystemTheme system_theme_;
  bool use_overlay_scrollbar_ = false;
  ColorProviderKey::ForcedColors forced_colors_ =
      ColorProviderKey::ForcedColors::kNone;
  PreferredColorScheme preferred_color_scheme_ =
      PreferredColorScheme::kNoPreference;
  PreferredContrast preferred_contrast_ = PreferredContrast::kNoPreference;
  bool prefers_reduced_transparency_ = false;
  bool inverted_colors_ = false;
  std::optional<SkColor> user_color_;
  std::optional<ColorProviderKey::SchemeVariant> scheme_variant_;
  ColorProviderKey::UserColorSource preferred_color_source_ =
      ColorProviderKey::UserColorSource::kAccent;
  base::TimeDelta caret_blink_interval_;

  raw_ptr<NativeTheme> associated_web_instance_ = nullptr;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace ui

#endif  // UI_NATIVE_THEME_NATIVE_THEME_H_
