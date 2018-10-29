// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_ENV_H_
#define UI_AURA_ENV_H_

#include <memory>
#include <set>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/supports_user_data.h"
#include "build/build_config.h"
#include "mojo/public/cpp/system/buffer.h"
#include "ui/aura/aura_export.h"
#include "ui/base/dragdrop/os_exchange_data_provider_factory.h"
#include "ui/events/event_target.h"
#include "ui/events/system_input_injector.h"
#include "ui/gfx/geometry/point.h"

namespace base {
class UnguessableToken;
}

namespace mojo {
template <typename MojoInterface>
class InterfacePtr;
}

namespace service_manager {
class Connector;
}

namespace ui {
class ContextFactory;
class ContextFactoryPrivate;
class EventObserver;
class GestureRecognizer;
class PlatformEventSource;
}  // namespace ui

namespace ws {
namespace mojom {
class WindowTreeClient;
}
}

namespace aura {
namespace test {
class EnvTestHelper;
class EnvWindowTreeClientSetter;
}

class EnvInputStateController;
class EnvObserver;
class EventObserverAdapter;
class InputStateLookup;
class MouseLocationManager;
class MusMouseLocationUpdater;
class Window;
class WindowEventDispatcherObserver;
class WindowOcclusionTracker;
class WindowPort;
class WindowTreeClient;
class WindowTreeHost;

// A singleton object that tracks general state within Aura.
class AURA_EXPORT Env : public ui::EventTarget,
                        public ui::OSExchangeDataProviderFactory::Factory,
                        public ui::SystemInputInjectorFactory,
                        public base::SupportsUserData {
 public:
  enum class Mode {
    // Classic aura.
    LOCAL,

    // Aura with a backend of mus.
    MUS,
  };

  ~Env() override;

  // Creates a new Env instance.
  // NOTE: if you pass in Mode::MUS it is expected that you call
  // SetWindowTreeClient() before any windows are created.
  static std::unique_ptr<Env> CreateInstance(Mode mode = Mode::LOCAL);

  // Creates a new Env of type LOCAL. This factory function is intended for
  // use when this process is providing the WindowService *and* acting as a
  // client of the WindowService, for example, ash with SingleProcessMash.
  static std::unique_ptr<Env> CreateLocalInstanceForInProcess();

#if defined(USE_OZONE)
  // used to create a new Env that hosts the viz process. |connector| is the
  // connector used to establish outbound connections.
  static std::unique_ptr<Env> CreateInstanceToHostViz(
      service_manager::Connector* connector);
#endif

  // This returns the instance created by CreateInstance() or
  // CreateInstanceToHostViz(). This does *not* return the instance returned
  // by CreateLocalInstanceForInProcess(). The instance returned by
  // CreateLocalInstanceForInProcess() is intended for use when an Env has
  // already been created. For example, in chrome with SingleProcessMash an
  // instance is created by way of CreateInstance() (which is the instance
  // returned by GetInstance()) *and* an instance is created via
  // CreateLocalInstanceForInProcess().
  static Env* GetInstance();
  static bool HasInstance();

  Mode mode() const { return mode_; }

  // Called internally to create the appropriate WindowPort implementation.
  std::unique_ptr<WindowPort> CreateWindowPort(Window* window);

  void AddObserver(EnvObserver* observer);
  void RemoveObserver(EnvObserver* observer);

  void AddWindowEventDispatcherObserver(
      WindowEventDispatcherObserver* observer);
  void RemoveWindowEventDispatcherObserver(
      WindowEventDispatcherObserver* observer);
  base::ObserverList<WindowEventDispatcherObserver>::Unchecked&
  window_event_dispatcher_observers() {
    return window_event_dispatcher_observers_;
  }

  EnvInputStateController* env_controller() const {
    return env_controller_.get();
  }

  int mouse_button_flags() const { return mouse_button_flags_; }
  void set_mouse_button_flags(int mouse_button_flags) {
    mouse_button_flags_ = mouse_button_flags;
  }
  // Returns true if a mouse button is down. This may query the native OS,
  // otherwise it uses |mouse_button_flags_|.
  bool IsMouseButtonDown() const;

  // Gets/sets the last mouse location seen in a mouse event in the screen
  // coordinates.
  const gfx::Point& last_mouse_location() const;
  void SetLastMouseLocation(const gfx::Point& last_mouse_location);

  // Creates the MouseLocationManager if it hasn't been created yet.
  void CreateMouseLocationManager();

  // Returns a read-only handle to the shared memory which contains the global
  // mouse position. Each call returns a new handle. This is only valid if Env
  // was configured to create a MouseLocationManager.
  mojo::ScopedSharedBufferHandle GetLastMouseLocationMemory();

  // Whether any touch device is currently down.
  bool is_touch_down() const { return is_touch_down_; }
  void set_touch_down(bool value) { is_touch_down_ = value; }

  void set_context_factory(ui::ContextFactory* context_factory) {
    context_factory_ = context_factory;
  }
  ui::ContextFactory* context_factory() { return context_factory_; }

  // Sets |initial_throttle_input_on_resize| the next time Env is created. This
  // is only useful in tests that need to disable input resize.
  static void set_initial_throttle_input_on_resize_for_testing(
      bool throttle_input) {
    initial_throttle_input_on_resize_ = throttle_input;
  }
  void set_throttle_input_on_resize_for_testing(bool throttle_input) {
    throttle_input_on_resize_ = throttle_input;
  }
  bool throttle_input_on_resize() const { return throttle_input_on_resize_; }

  void set_context_factory_private(
      ui::ContextFactoryPrivate* context_factory_private) {
    context_factory_private_ = context_factory_private;
  }
  ui::ContextFactoryPrivate* context_factory_private() {
    return context_factory_private_;
  }

  ui::GestureRecognizer* gesture_recognizer() {
    return gesture_recognizer_.get();
  }

  void SetGestureRecognizer(
      std::unique_ptr<ui::GestureRecognizer> gesture_recognizer);

  // See CreateInstance() for description.
  void SetWindowTreeClient(WindowTreeClient* window_tree_client);
  bool HasWindowTreeClient() const { return window_tree_client_ != nullptr; }

  // Schedules an embed of a client. See
  // ws::mojom::WindowTreeClient::ScheduleEmbed() for details.
  void ScheduleEmbed(
      mojo::InterfacePtr<ws::mojom::WindowTreeClient> client,
      base::OnceCallback<void(const base::UnguessableToken&)> callback);

  // Get WindowOcclusionTracker instance. Create one if not yet created.
  WindowOcclusionTracker* GetWindowOcclusionTracker();

  // Pause/unpause window occlusion tracking. It hides the detail of where
  // WindowOcclusionTracker lives. It calls the tracker for LOCAL aura and calls
  // Window Service to access the tracker there for MUS aura.
  void PauseWindowOcclusionTracking();
  void UnpauseWindowOcclusionTracking();

  // Add, remove, or notify EventObservers. EventObservers are essentially
  // pre-target EventHandlers that can not modify the events nor alter dispatch.
  // On Chrome OS, observers receive system-wide events if |target| is this Env.
  // On desktop platforms, observers may only receive events targeting Chrome.
  // Observers must be removed before their target is destroyed.
  void AddEventObserver(ui::EventObserver* observer,
                        ui::EventTarget* target,
                        const std::set<ui::EventType>& types);
  void RemoveEventObserver(ui::EventObserver* observer);
  void NotifyEventObservers(const ui::Event& event);

 private:
  friend class test::EnvTestHelper;
  friend class test::EnvWindowTreeClientSetter;
  friend class EventInjector;
  friend class MusMouseLocationUpdater;
  friend class Window;
  friend class WindowTreeClient;  // For call to WindowTreeClientDestroyed().
  friend class WindowTreeHost;

  explicit Env(Mode mode);

  void Init(service_manager::Connector* connector);

  // After calling this method, all OSExchangeDataProvider instances will be
  // Mus instances. We can't do this work in Init(), because our mode may
  // changed via the EnvTestHelper.
  void EnableMusOSExchangeDataProvider();

  // After calling this method, all SystemInputInjectors will go through mus
  // instead of ozone.
  void EnableMusOverrideInputInjector();

  // Called by the Window when it is initialized. Notifies observers.
  void NotifyWindowInitialized(Window* window);

  // Called by the WindowTreeHost when it is initialized. Notifies observers.
  void NotifyHostInitialized(WindowTreeHost* host);

  void WindowTreeClientDestroyed(WindowTreeClient* client);

  // Overridden from ui::EventTarget:
  bool CanAcceptEvent(const ui::Event& event) override;
  ui::EventTarget* GetParentTarget() override;
  std::unique_ptr<ui::EventTargetIterator> GetChildIterator() const override;
  ui::EventTargeter* GetEventTargeter() override;

  // Overridden from ui::OSExchangeDataProviderFactory::Factory:
  std::unique_ptr<ui::OSExchangeData::Provider> BuildProvider() override;

  // Overridden from SystemInputInjectorFactory:
  std::unique_ptr<ui::SystemInputInjector> CreateSystemInputInjector() override;

  // This is not const for tests, which may share Env across tests and so needs
  // to reset the value.
  Mode mode_;

  // Intentionally not exposed publicly. Someday we might want to support
  // multiple WindowTreeClients. Use EnvTestHelper in tests. This is set to null
  // during shutdown.
  WindowTreeClient* window_tree_client_ = nullptr;

  base::ObserverList<EnvObserver>::Unchecked observers_;

  // Code wanting to observe WindowEventDispatcher typically wants to observe
  // all WindowEventDispatchers. This is made easier by having Env own all the
  // observers.
  base::ObserverList<WindowEventDispatcherObserver>::Unchecked
      window_event_dispatcher_observers_;

  // The ObserverList and set of owned EventObserver adapters.
  base::ObserverList<EventObserverAdapter> event_observer_adapter_list_;
  std::set<std::unique_ptr<EventObserverAdapter>> event_observer_adapters_;

  std::unique_ptr<EnvInputStateController> env_controller_;
  int mouse_button_flags_;
  // Location of last mouse event, in screen coordinates.
  mutable gfx::Point last_mouse_location_;
  bool is_touch_down_;
  bool get_last_mouse_location_from_mus_;
  // This may be set to true in tests to force using |last_mouse_location_|
  // rather than querying WindowTreeClient.
  bool always_use_last_mouse_location_ = false;
  // Whether we set ourselves as the OSExchangeDataProviderFactory.
  bool is_os_exchange_data_provider_factory_ = false;
  // Whether we set ourselves as the SystemInputInjectorFactory.
  bool is_override_input_injector_factory_ = false;

  std::unique_ptr<ui::GestureRecognizer> gesture_recognizer_;

  std::unique_ptr<InputStateLookup> input_state_lookup_;
  std::unique_ptr<ui::PlatformEventSource> event_source_;

  ui::ContextFactory* context_factory_;
  ui::ContextFactoryPrivate* context_factory_private_;

  // This is set to true when the WindowTreeClient is destroyed. It triggers
  // creating a different WindowPort implementation.
  bool in_mus_shutdown_ = false;

  static bool initial_throttle_input_on_resize_;
  bool throttle_input_on_resize_ = initial_throttle_input_on_resize_;

  // Only created if CreateMouseLocationManager() was called.
  std::unique_ptr<MouseLocationManager> mouse_location_manager_;

  // Lazily created for LOCAL aura.
  std::unique_ptr<WindowOcclusionTracker> window_occlusion_tracker_;

  DISALLOW_COPY_AND_ASSIGN(Env);
};

}  // namespace aura

#endif  // UI_AURA_ENV_H_
