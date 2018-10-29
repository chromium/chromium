// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_MUS_MUS_CLIENT_H_
#define UI_VIEWS_MUS_MUS_CLIENT_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "services/service_manager/public/cpp/identity.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/mus/window_tree_client_delegate.h"
#include "ui/views/mus/mus_export.h"
#include "ui/views/mus/screen_mus_delegate.h"
#include "ui/views/widget/widget.h"

namespace aura {
class PropertyConverter;
class Window;
class WindowTreeClient;
}

namespace base {
class SingleThreadTaskRunner;
}

namespace service_manager {
class Connector;
}

namespace ui {
class CursorDataFactoryOzone;
}

namespace wm {
class WMState;
}

namespace ws {
class InputDeviceClient;
}

namespace views {

class AXRemoteHost;
class DesktopNativeWidgetAura;
class MusClientObserver;
class MusPropertyMirror;
class ScreenMus;

namespace internal {
class NativeWidgetDelegate;
}

// MusClient establishes a connection to mus and sets up necessary state so that
// aura and views target mus. This class is useful for typical clients, not the
// WindowManager. Most clients don't create this directly, rather use AuraInit.
class VIEWS_MUS_EXPORT MusClient : public aura::WindowTreeClientDelegate,
                                   public ScreenMusDelegate {
 public:
  struct VIEWS_MUS_EXPORT InitParams {
    InitParams();
    ~InitParams();

    // Production code should provide |connector|, |identity|, and an
    // |io_task_runner| if the process already has one. Test code may skip these
    // parameters (e.g. a unit test that does not need to connect to the window
    // service does not need to provide a connector).
    service_manager::Connector* connector = nullptr;
    service_manager::Identity identity;
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner = nullptr;

    // Create a wm::WMState. Some processes (e.g. the browser) may already
    // have one.
    bool create_wm_state = true;

    // Tests may need to control objects owned by MusClient.
    bool create_cursor_factory = true;

    // If provided, MusClient will not create the WindowTreeClient. Not owned.
    // Must outlive MusClient.
    aura::WindowTreeClient* window_tree_client = nullptr;

    // Connect to the accessibility host service in the browser (e.g. to support
    // ChromeVox).
    bool use_accessibility_host = false;

    // Set to true if the WindowService is running in the same process and on
    // the same thread as MusClient.
    bool running_in_ws_process = false;
  };

  // Most clients should use AuraInit, which creates a MusClient.
  explicit MusClient(const InitParams& params);
  ~MusClient() override;

  static MusClient* Get() { return instance_; }
  static bool Exists() { return instance_ != nullptr; }

  // Returns true if a DesktopNativeWidgetAura should be created given the
  // specified params. If this returns false a NativeWidgetAura should be
  // created.
  static bool ShouldCreateDesktopNativeWidgetAura(
      const Widget::InitParams& init_params);

  // Returns true if the windows backing the Widget should be made translucent.
  static bool ShouldMakeWidgetWindowsTranslucent(
      const Widget::InitParams& params);

  // Returns the properties to supply to mus when creating a window.
  static std::map<std::string, std::vector<uint8_t>>
  ConfigurePropertiesFromParams(const Widget::InitParams& init_params);

  aura::PropertyConverter* property_converter() {
    return property_converter_.get();
  }

  aura::WindowTreeClient* window_tree_client() { return window_tree_client_; }

  AXRemoteHost* ax_remote_host() { return ax_remote_host_.get(); }

  // Creates DesktopNativeWidgetAura with DesktopWindowTreeHostMus. This is
  // set as the factory function used for creating NativeWidgets when a
  //  NativeWidget has not been explicitly set.
  NativeWidget* CreateNativeWidget(const Widget::InitParams& init_params,
                                   internal::NativeWidgetDelegate* delegate);
  void OnWidgetInitDone(Widget* widget);

  // Called when the capture client has been set or unset for a window.
  void OnCaptureClientSet(aura::client::CaptureClient* capture_client);
  void OnCaptureClientUnset(aura::client::CaptureClient* capture_client);

  void AddObserver(MusClientObserver* observer);
  void RemoveObserver(MusClientObserver* observer);

  void SetMusPropertyMirror(std::unique_ptr<MusPropertyMirror> mirror);
  MusPropertyMirror* mus_property_mirror() {
    return mus_property_mirror_.get();
  }

  // Close all widgets this client knows.
  void CloseAllWidgets();

 private:
  friend class AuraInit;
  friend class MusClientTestApi;

  // Creates a DesktopWindowTreeHostMus. This is set as the factory function
  // ViewsDelegate such that if DesktopNativeWidgetAura is created without a
  // DesktopWindowTreeHost this is created.
  std::unique_ptr<DesktopWindowTreeHost> CreateDesktopWindowTreeHost(
      const Widget::InitParams& init_params,
      internal::NativeWidgetDelegate* delegate,
      DesktopNativeWidgetAura* desktop_native_widget_aura);

  // aura::WindowTreeClientDelegate:
  void OnEmbed(
      std::unique_ptr<aura::WindowTreeHostMus> window_tree_host) override;
  void OnLostConnection(aura::WindowTreeClient* client) override;
  void OnEmbedRootDestroyed(aura::WindowTreeHostMus* window_tree_host) override;
  aura::PropertyConverter* GetPropertyConverter() override;
  void OnDisplaysChanged(std::vector<ws::mojom::WsDisplayPtr> ws_displays,
                         int64_t primary_display_id,
                         int64_t internal_display_id,
                         int64_t display_id_for_new_windows) override;

  // ScreenMusDelegate:
  void OnWindowManagerFrameValuesChanged() override;
  aura::Window* GetWindowAtScreenPoint(const gfx::Point& point) override;

  static MusClient* instance_;

  service_manager::Identity identity_;

  base::ObserverList<MusClientObserver>::Unchecked observer_list_;

#if defined(USE_OZONE)
  std::unique_ptr<ui::CursorDataFactoryOzone> cursor_factory_ozone_;
#endif

  // NOTE: this may be null (creation is based on argument supplied to
  // constructor).
  std::unique_ptr<wm::WMState> wm_state_;

  std::unique_ptr<ScreenMus> screen_;

  std::unique_ptr<aura::PropertyConverter> property_converter_;
  std::unique_ptr<MusPropertyMirror> mus_property_mirror_;

  // Non-null if MusClient created the WindowTreeClient.
  std::unique_ptr<aura::WindowTreeClient> owned_window_tree_client_;

  // Never null.
  aura::WindowTreeClient* window_tree_client_;

  // Gives services transparent remote access the InputDeviceManager.
  std::unique_ptr<ws::InputDeviceClient> input_device_client_;

  // Forwards accessibility events to extensions in the browser. Can be null for
  // apps that do not need accessibility support and for the browser itself
  // under OopAsh.
  std::unique_ptr<AXRemoteHost> ax_remote_host_;

  DISALLOW_COPY_AND_ASSIGN(MusClient);
};

}  // namespace views

#endif  // UI_VIEWS_MUS_MUS_CLIENT_H_
