// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_LINUX_LINUX_UI_H_
#define UI_LINUX_LINUX_UI_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/observer_list.h"
#include "base/scoped_observation_traits.h"
#include "build/buildflag.h"
#include "build/chromecast_buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "ui/display/types/display_config.h"

// The main entrypoint into Linux toolkit specific code. GTK/QT code should only
// be executed behind this interface.

using SkColor = uint32_t;
// TODO(thomasanderson): Remove Profile forward declaration.
class Profile;

namespace aura {
class Window;
}

namespace base {
class TimeDelta;
}

namespace gfx {
struct FontRenderParams;
class Image;
class Size;
}  // namespace gfx

namespace printing {
class PrintingContextLinux;
class PrintDialogLinuxInterface;
}  // namespace printing

namespace ui {

class CursorThemeManagerObserver;
class DeviceScaleFactorObserver;
class Event;
class LinuxInputMethodContext;
class LinuxInputMethodContextDelegate;
class LinuxUiTheme;
class NativeTheme;
class NavButtonProvider;
class SelectFileDialog;
class SelectFilePolicy;
class TextEditCommandAuraLinux;
class WindowButtonOrderObserver;
class WindowFrameProvider;

// Adapter class with targets to render like different toolkits. Set by any
// project that wants to do linux desktop native rendering.
class COMPONENT_EXPORT(LINUX_UI) LinuxUi {
 public:
  // Describes the window management actions that could be taken in response to
  // a middle click in the non client area.
  enum class WindowFrameAction {
    kNone,
    kLower,
    kMinimize,
    kToggleMaximize,
    kMenu,
  };

  // The types of clicks that might invoke a WindowFrameAction.
  enum class WindowFrameActionSource {
    kDoubleClick,
    kMiddleClick,
    kRightClick,
  };

  struct FontSettings {
    std::string family;
    int size_pixels = 0;
    // Holds a bitfield of gfx::Font::Style values.
    int style = 0;
    // A standard font weight as used in Pango.  Must be a value in [1, 999].
    int weight = 0;
  };

  LinuxUi(const LinuxUi&) = delete;
  LinuxUi& operator=(const LinuxUi&) = delete;
  virtual ~LinuxUi();

  // Sets the dynamically loaded singleton that draws the desktop native UI.
  // Returns the old instance if any.
  static LinuxUi* SetInstance(LinuxUi* instance);

  // Returns a LinuxUI instance for the toolkit used in the user's desktop
  // environment.
  //
  // Can return NULL, in case no toolkit has been set. (For example, if we're
  // running with the "--ash" flag.)
  static LinuxUi* instance();

  // Registers |observer| to be notified about changes to the device
  // scale factor.
  void AddDeviceScaleFactorObserver(DeviceScaleFactorObserver* observer);

  // Unregisters |observer| from receiving changes to the device scale
  // factor.
  void RemoveDeviceScaleFactorObserver(DeviceScaleFactorObserver* observer);

  // Adds |observer| and makes initial OnCursorThemNameChanged() and/or
  // OnCursorThemeSizeChanged() calls if the respective settings were set.
  void AddCursorThemeObserver(CursorThemeManagerObserver* observer);

  void RemoveCursorThemeObserver(CursorThemeManagerObserver* observer);

  // Returns details about the default UI font.
  FontSettings GetDefaultFontDescription();

  // Determines the device scale factor for all screens.
  const display::DisplayConfig& display_config() const {
    return display_config_;
  }

  // Returns true on success.  If false is returned, this instance shouldn't
  // be used and the behavior of all functions is undefined.
  [[nodiscard]] virtual bool Initialize() = 0;

  // Caches the default font render parameters.  This doesn't need to be called
  // explicitly since the first call to get the font settings will implicitly
  // initialize the default front render parameters.
  virtual void InitializeFontSettings() = 0;

  virtual base::TimeDelta GetCursorBlinkInterval() const = 0;

  // Returns the icon for a given content type from the icon theme.
  // TODO(davidben): Add an observer for the theme changing, so we can drop the
  // caches.
  virtual gfx::Image GetIconForContentType(const std::string& content_type,
                                           int size,
                                           float scale) const = 0;

  // Returns a map of KeyboardEvent code to KeyboardEvent key values.
  virtual base::flat_map<std::string, std::string> GetKeyboardLayoutMap() = 0;

#if BUILDFLAG(ENABLE_PRINTING)
  virtual printing::PrintDialogLinuxInterface* CreatePrintDialog(
      printing::PrintingContextLinux* context) = 0;

  virtual gfx::Size GetPdfPaperSize(
      printing::PrintingContextLinux* context) = 0;
#endif

  // Returns a native file selection dialog.  `listener` is of type
  // SelectFileDialog::Listener.  TODO(thomasanderson): Move
  // SelectFileDialog::Listener to SelectFileDialogListener so that it can be
  // forward declared.
  virtual SelectFileDialog* CreateSelectFileDialog(
      void* listener,
      std::unique_ptr<SelectFilePolicy> policy) const = 0;

  // Returns the prefererd theme name for cursor loading.
  virtual std::string GetCursorThemeName() = 0;

  // Returns the preferred size for cursor bitmaps.  A value of 64 indicates
  // that 64x64 px bitmaps are preferred.
  virtual int GetCursorThemeSize() = 0;

  // Returns a platform specific input method context.
  virtual std::unique_ptr<LinuxInputMethodContext> CreateInputMethodContext(
      LinuxInputMethodContextDelegate* delegate) const = 0;

  // Matches a key event against the users' platform specific key bindings,
  // false will be returned if the key event doesn't correspond to a predefined
  // key binding.  Edit commands matched with |event| will be stored in
  // |edit_commands|, if |edit_commands| is non-nullptr.
  //
  // |text_falgs| is the current ui::TextInputFlags if available.
  virtual bool GetTextEditCommandsForEvent(
      const ui::Event& event,
      int text_flags,
      std::vector<TextEditCommandAuraLinux>* commands) = 0;

  // Returns the default font rendering settings.
  virtual gfx::FontRenderParams GetDefaultFontRenderParams() = 0;

  // Indicates if animations are enabled by the toolkit.
  virtual bool AnimationsEnabled() const = 0;

  // Notifies the observer about changes about how window buttons should be
  // laid out.
  virtual void AddWindowButtonOrderObserver(
      WindowButtonOrderObserver* observer) = 0;

  // Removes the observer from the LinuxUI's list.
  virtual void RemoveWindowButtonOrderObserver(
      WindowButtonOrderObserver* observer) = 0;

  // What action we should take when the user clicks on the non-client area.
  // |source| describes the type of click.
  virtual WindowFrameAction GetWindowFrameAction(
      WindowFrameActionSource source) = 0;

 protected:
  struct CmdLineArgs {
    CmdLineArgs();
    CmdLineArgs(CmdLineArgs&&);
    CmdLineArgs& operator=(CmdLineArgs&&);
    ~CmdLineArgs();

    // `argc` is modified by toolkits, so store it explicitly.
    int argc = 0;

    // Contains C-strings that point into `args`.  `argv.size()` >= `argc`.
    std::vector<char*> argv;

    // `argv` concatenated with NUL characters.
    std::vector<char> args;
  };

  LinuxUi();

  static CmdLineArgs CopyCmdLine(const base::CommandLine& command_line);

  base::ObserverList<DeviceScaleFactorObserver>::Unchecked&
  device_scale_factor_observer_list() {
    return device_scale_factor_observer_list_;
  }

  base::ObserverList<CursorThemeManagerObserver>& cursor_theme_observers() {
    return cursor_theme_observer_list_;
  }

  display::DisplayConfig& display_config() { return display_config_; }

  void set_default_font_settings(
      const std::optional<FontSettings>& default_font_settings) {
    default_font_settings_ = default_font_settings;
  }

 private:
  // Objects to notify when the device scale factor changes.
  base::ObserverList<DeviceScaleFactorObserver>::Unchecked
      device_scale_factor_observer_list_;

  // Objects to notify when the cursor theme or size changes.
  base::ObserverList<CursorThemeManagerObserver> cursor_theme_observer_list_;

  display::DisplayConfig display_config_;

  std::optional<FontSettings> default_font_settings_;
};

class COMPONENT_EXPORT(LINUX_UI) LinuxUiTheme {
 public:
  LinuxUiTheme(const LinuxUiTheme&) = delete;
  LinuxUiTheme& operator=(const LinuxUiTheme&) = delete;
  virtual ~LinuxUiTheme();

  // Returns the LinuxUi instance for the given window.
  static LinuxUiTheme* GetForWindow(aura::Window* window);

  // Returns the LinuxUi instance for the given profile.
  static LinuxUiTheme* GetForProfile(Profile* profile);

  // Returns the native theme for this toolkit.
  virtual ui::NativeTheme* GetNativeTheme() const = 0;

  virtual bool GetColor(int id,
                        SkColor* color,
                        bool use_custom_frame) const = 0;
  virtual bool GetDisplayProperty(int id, int* result) const = 0;

  // Returns the preferences that we pass to Blink.
  virtual void GetFocusRingColor(SkColor* color) const = 0;
  virtual void GetActiveSelectionBgColor(SkColor* color) const = 0;
  virtual void GetActiveSelectionFgColor(SkColor* color) const = 0;
  virtual void GetInactiveSelectionBgColor(SkColor* color) const = 0;
  virtual void GetInactiveSelectionFgColor(SkColor* color) const = 0;

  // Only used on GTK to indicate if the dark GTK theme variant is
  // preferred.
  virtual bool PreferDarkTheme() const = 0;

  // Override the toolkit's dark mode preference.  Used when the dark mode
  // setting is provided by org.freedesktop.appearance instead of the toolkit.
  virtual void SetDarkTheme(bool dark) = 0;

  // Override the toolkit's accent color.
  virtual void SetAccentColor(std::optional<SkColor> accent_color) = 0;

  // Returns a new NavButtonProvider, or nullptr if the underlying
  // toolkit does not support drawing client-side navigation buttons.
  virtual std::unique_ptr<NavButtonProvider> CreateNavButtonProvider() = 0;

  // Returns a WindowFrameProvider, or nullptr if the underlying toolkit does
  // not support drawing client-side window decorations. |solid_frame| indicates
  // if transparency is unsupported and the frame should be rendered opaque.
  // The returned object is not owned by the caller and will remain alive until
  // the process ends.
  virtual WindowFrameProvider* GetWindowFrameProvider(bool solid_frame,
                                                      bool tiled) = 0;

 protected:
  LinuxUiTheme();
};

// This is used internally by LinuxUi implementations and linux_ui_factory to
// allow converting a LinuxUi to a LinuxUiTheme.  Users should not use (and have
// no way of obtaining) an instance of this class.
class LinuxUiAndTheme : public LinuxUi, public LinuxUiTheme {
 public:
  ~LinuxUiAndTheme() override = default;
};

}  // namespace ui

namespace base {

template <>
struct ScopedObservationTraits<ui::LinuxUi, ui::CursorThemeManagerObserver> {
  static void AddObserver(ui::LinuxUi* source,
                          ui::CursorThemeManagerObserver* observer) {
    source->AddCursorThemeObserver(observer);
  }
  static void RemoveObserver(ui::LinuxUi* source,
                             ui::CursorThemeManagerObserver* observer) {
    source->RemoveCursorThemeObserver(observer);
  }
};

template <>
struct ScopedObservationTraits<ui::LinuxUi, ui::DeviceScaleFactorObserver> {
  static void AddObserver(ui::LinuxUi* source,
                          ui::DeviceScaleFactorObserver* observer) {
    source->AddDeviceScaleFactorObserver(observer);
  }
  static void RemoveObserver(ui::LinuxUi* source,
                             ui::DeviceScaleFactorObserver* observer) {
    source->RemoveDeviceScaleFactorObserver(observer);
  }
};

template <>
struct ScopedObservationTraits<ui::LinuxUi, ui::WindowButtonOrderObserver> {
  static void AddObserver(ui::LinuxUi* source,
                          ui::WindowButtonOrderObserver* observer) {
    source->AddWindowButtonOrderObserver(observer);
  }
  static void RemoveObserver(ui::LinuxUi* source,
                             ui::WindowButtonOrderObserver* observer) {
    source->RemoveWindowButtonOrderObserver(observer);
  }
};

}  // namespace base

#endif  // UI_LINUX_LINUX_UI_H_
