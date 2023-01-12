// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_SHELL_BROWSER_SHELL_H_
#define WEBLAYER_SHELL_BROWSER_SHELL_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "weblayer/public/download_delegate.h"
#include "weblayer/public/navigation_observer.h"
#include "weblayer/public/tab_observer.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"
#elif defined(USE_AURA)
namespace views {
class Widget;
class ViewsDelegate;
}  // namespace views
#if !BUILDFLAG(IS_CHROMEOS)
namespace display {
class Screen;
}
namespace wm {
class WMState;
}
#endif
#endif  // defined(USE_AURA)

class GURL;

namespace weblayer {
class Browser;
class Profile;
class Tab;

// This represents one window of the Web Shell, i.e. all the UI including
// buttons and url bar, as well as the web content area.
class Shell : public TabObserver,
              public NavigationObserver,
              public DownloadDelegate {
 public:
  ~Shell() override;

  void LoadURL(const GURL& url);
  void GoBackOrForward(int offset);
  void Reload();
  void ReloadBypassingCache();
  void Stop();
  void Close();

  // Do one time initialization at application startup.
  static void Initialize();

#if BUILDFLAG(IS_ANDROID)
  static Shell* CreateNewWindow(const GURL& url, const gfx::Size& initial_size);
#else
  static Shell* CreateNewWindow(Profile* web_profile,
                                const GURL& url,
                                const gfx::Size& initial_size);
#endif

  // Returns the currently open windows.
  static std::vector<Shell*>& windows() { return windows_; }

  // Closes all windows, pumps teardown tasks, then returns. The main message
  // loop will be signalled to quit, before the call returns.
  static void CloseAllWindows();

  // Stores the supplied |quit_closure|, to be run when the last Shell
  // instance is destroyed.
  static void SetMainMessageLoopQuitClosure(base::OnceClosure quit_closure);

  Tab* tab();
  Browser* browser();

  gfx::NativeWindow window() { return window_; }

  static gfx::Size GetShellDefaultSize();

 private:
  enum UIControl { BACK_BUTTON, FORWARD_BUTTON, STOP_BUTTON };

  static Shell* CreateNewWindowWithBrowser(std::unique_ptr<Browser> browser,
                                           const GURL& url,
                                           const gfx::Size& initial_size);

  explicit Shell(std::unique_ptr<Browser> browser);

  // TabObserver implementation:
  void DisplayedUrlChanged(const GURL& url) override;

  // NavigationObserver implementation:
  void LoadStateChanged(bool is_loading, bool should_show_loading_ui) override;
  void LoadProgressChanged(double progress) override;

  // DownloadDelegate implementation:
  bool InterceptDownload(const GURL& url,
                         const std::string& user_agent,
                         const std::string& content_disposition,
                         const std::string& mime_type,
                         int64_t content_length) override;
  void AllowDownload(Tab* tab,
                     const GURL& url,
                     const std::string& request_method,
                     absl::optional<url::Origin> request_initiator,
                     AllowDownloadCallback callback) override;

  // Helper to create a new Shell.
  static Shell* CreateShell(std::unique_ptr<Browser> browser,
                            const gfx::Size& initial_size);

  // Helper for one time initialization of application
  static void PlatformInitialize(const gfx::Size& default_window_size);

  // Helper for one time deinitialization of platform specific state.
  static void PlatformExit();

  // Adjust the size when Blink sends 0 for width and/or height.
  // This happens when Blink requests a default-sized window.
  static gfx::Size AdjustWindowSize(const gfx::Size& initial_size);

  // All the methods that begin with Platform need to be implemented by the
  // platform specific Shell implementation.
  // Called from the destructor to let each platform do any necessary cleanup.
  void PlatformCleanUp();

  // Creates the main window GUI.
  void PlatformCreateWindow(int width, int height);

  // Links the WebContents into the newly created window.
  void PlatformSetContents();

  // Resize the content area and GUI.
  void PlatformResizeSubViews();

  // Enable/disable a button.
  void PlatformEnableUIControl(UIControl control, bool is_enabled);

  // Updates the url in the url bar.
  void PlatformSetAddressBarURL(const GURL& url);

  // Sets the load progress indicator in the UI.
  void PlatformSetLoadProgress(double progress);

  // Set the title of shell window
  void PlatformSetTitle(const std::u16string& title);

  std::unique_ptr<Browser> browser_;

  gfx::NativeWindow window_;

  gfx::Size content_size_;

#if BUILDFLAG(IS_ANDROID)
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
#elif defined(USE_AURA)
  static wm::WMState* wm_state_;
  static display::Screen* screen_;
#if defined(TOOLKIT_VIEWS)
  static views::ViewsDelegate* views_delegate_;

  raw_ptr<views::Widget> window_widget_;
#endif  // defined(TOOLKIT_VIEWS)
#endif  // defined(USE_AURA)

  // A container of all the open windows. We use a vector so we can keep track
  // of ordering.
  static std::vector<Shell*> windows_;
};

}  // namespace weblayer

#endif  // WEBLAYER_SHELL_BROWSER_SHELL_H_
