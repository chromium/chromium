// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_LINUX_LINUX_UI_H_
#define UI_LINUX_LINUX_UI_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "build/buildflag.h"
#include "build/chromecast_buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/cursor/cursor_theme_manager.h"
#include "ui/base/ime/linux/linux_input_method_context_factory.h"
#include "ui/base/ime/linux/text_edit_key_bindings_delegate_auralinux.h"
#include "ui/gfx/animation/animation_settings_provider_linux.h"
#include "ui/gfx/skia_font_delegate.h"

#if BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CASTOS)
#include "ui/shell_dialogs/shell_dialog_linux.h"
#endif

#if BUILDFLAG(ENABLE_PRINTING)
#include "printing/printing_context_linux.h"  // nogncheck
#endif

// The main entrypoint into Linux toolkit specific code. GTK/QT code should only
// be executed behind this interface.

namespace aura {
class Window;
}

namespace base {
class TimeDelta;
}

namespace gfx {
class Image;
}

namespace ui {

class DeviceScaleFactorObserver;
class NativeTheme;
class NavButtonProvider;
class WindowButtonOrderObserver;
class WindowFrameProvider;

// Adapter class with targets to render like different toolkits. Set by any
// project that wants to do linux desktop native rendering.
class COMPONENT_EXPORT(LINUX_UI) LinuxUi
    : public ui::LinuxInputMethodContextFactory,
      public gfx::SkiaFontDelegate,
#if BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CASTOS)
      public ui::ShellDialogLinux,
#endif
#if BUILDFLAG(ENABLE_PRINTING)
      public printing::PrintingContextLinuxDelegate,
#endif
      public ui::TextEditKeyBindingsDelegateAuraLinux,
      public ui::CursorThemeManager,
      public gfx::AnimationSettingsProviderLinux {
 public:
  using UseSystemThemeCallback =
      base::RepeatingCallback<bool(aura::Window* window)>;

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

  LinuxUi(const LinuxUi&) = delete;
  LinuxUi& operator=(const LinuxUi&) = delete;
  ~LinuxUi() override;

  // Sets the dynamically loaded singleton that draws the desktop native UI.
  static void SetInstance(std::unique_ptr<LinuxUi> instance);

  // Returns a LinuxUI instance for the toolkit used in the user's desktop
  // environment.
  //
  // Can return NULL, in case no toolkit has been set. (For example, if we're
  // running with the "--ash" flag.)
  static LinuxUi* instance();

  // Notifies the observer about changes about how window buttons should be
  // laid out.
  void AddWindowButtonOrderObserver(WindowButtonOrderObserver* observer);

  // Removes the observer from the LinuxUI's list.
  void RemoveWindowButtonOrderObserver(WindowButtonOrderObserver* observer);

  // Registers |observer| to be notified about changes to the device
  // scale factor.
  void AddDeviceScaleFactorObserver(DeviceScaleFactorObserver* observer);

  // Unregisters |observer| from receiving changes to the device scale
  // factor.
  void RemoveDeviceScaleFactorObserver(DeviceScaleFactorObserver* observer);

  // Returns the NativeTheme that reflects the theme used by `window`.
  ui::NativeTheme* GetNativeTheme(aura::Window* window) const;

  // Returns the classic or system NativeTheme depending on `use_system_theme`.
  ui::NativeTheme* GetNativeTheme(bool use_system_theme) const;

  // Sets a callback that determines whether to use the system theme.
  void SetUseSystemThemeCallback(UseSystemThemeCallback callback);

  // Returns whether we should be using the native theme provided by this
  // object by default.
  bool GetDefaultUsesSystemTheme() const;

  // Returns true on success.  If false is returned, this instance shouldn't
  // be used and the behavior of all functions is undefined.
  [[nodiscard]] virtual bool Initialize() = 0;
  virtual bool GetColor(int id,
                        SkColor* color,
                        bool use_custom_frame) const = 0;
  virtual bool GetDisplayProperty(int id, int* result) const = 0;

  // Returns the preferences that we pass to Blink.
  virtual SkColor GetFocusRingColor() const = 0;
  virtual SkColor GetActiveSelectionBgColor() const = 0;
  virtual SkColor GetActiveSelectionFgColor() const = 0;
  virtual SkColor GetInactiveSelectionBgColor() const = 0;
  virtual SkColor GetInactiveSelectionFgColor() const = 0;
  virtual base::TimeDelta GetCursorBlinkInterval() const = 0;

  // Returns the icon for a given content type from the icon theme.
  // TODO(davidben): Add an observer for the theme changing, so we can drop the
  // caches.
  virtual gfx::Image GetIconForContentType(const std::string& content_type,
                                           int size,
                                           float scale) const = 0;

  // What action we should take when the user clicks on the non-client area.
  // |source| describes the type of click.
  virtual WindowFrameAction GetWindowFrameAction(
      WindowFrameActionSource source) = 0;

  // Determines the device scale factor of the primary screen.
  virtual float GetDeviceScaleFactor() const = 0;

  // Only used on GTK to indicate if the dark GTK theme variant is
  // preferred.
  virtual bool PreferDarkTheme() const = 0;

  // Returns a new NavButtonProvider, or nullptr if the underlying
  // toolkit does not support drawing client-side navigation buttons.
  virtual std::unique_ptr<NavButtonProvider> CreateNavButtonProvider() = 0;

  // Returns a WindowFrameProvider, or nullptr if the underlying toolkit does
  // not support drawing client-side window decorations. |solid_frame| indicates
  // if transparency is unsupported and the frame should be rendered opaque.
  // The returned object is not owned by the caller and will remain alive until
  // the process ends.
  virtual WindowFrameProvider* GetWindowFrameProvider(bool solid_frame) = 0;

  // Returns a map of KeyboardEvent code to KeyboardEvent key values.
  virtual base::flat_map<std::string, std::string> GetKeyboardLayoutMap() = 0;

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

  const base::ObserverList<WindowButtonOrderObserver>::Unchecked&
  window_button_order_observer_list() const {
    return window_button_order_observer_list_;
  }

  const base::ObserverList<DeviceScaleFactorObserver>::Unchecked&
  device_scale_factor_observer_list() const {
    return device_scale_factor_observer_list_;
  }

  virtual ui::NativeTheme* GetNativeTheme() const = 0;

 private:
  // Used to determine whether the system theme should be used for a window.  If
  // no override is provided or the callback returns true, LinuxUI will default
  // to GetNativeTheme().
  UseSystemThemeCallback use_system_theme_callback_;

  // Objects to notify when the window frame button order changes.
  base::ObserverList<WindowButtonOrderObserver>::Unchecked
      window_button_order_observer_list_;

  // Objects to notify when the device scale factor changes.
  base::ObserverList<DeviceScaleFactorObserver>::Unchecked
      device_scale_factor_observer_list_;
};

}  // namespace ui

#endif  // UI_LINUX_LINUX_UI_H_
