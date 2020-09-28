// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/platform/x11/x11_event_source.h"

#include <algorithm>
#include <memory>
#include <type_traits>

#include "base/auto_reset.h"
#include "base/logging.h"
#include "base/memory/free_deleter.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram_macros.h"
#include "ui/events/devices/x11/device_data_manager_x11.h"
#include "ui/events/devices/x11/touch_factory_x11.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/keyboard_code_conversion_x.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/events/platform/x11/x11_hotplug_event_handler.h"
#include "ui/events/x/events_x_utils.h"
#include "ui/events/x/x11_event_translation.h"
#include "ui/events/x/x11_window_event_manager.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/extension_manager.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/gfx/x/xkb.h"
#include "ui/gfx/x/xproto.h"

#if defined(USE_GLIB)
#include "ui/events/platform/x11/x11_event_watcher_glib.h"
#else
#include "ui/events/platform/x11/x11_event_watcher_fdwatch.h"
#endif

#if defined(OS_CHROMEOS)
#include "ui/events/ozone/chromeos/cursor_controller.h"
#endif

#if defined(USE_OZONE)
#include "ui/base/ui_base_features.h"
#endif

namespace ui {

namespace {

void InitializeXkb(x11::Connection* connection) {
  if (!connection)
    return;

  auto& xkb = connection->xkb();

  xkb.UseExtension({x11::Xkb::major_version, x11::Xkb::minor_version})
      .OnResponse(base::BindOnce([](x11::Xkb::UseExtensionResponse response) {
        if (!response || !response->supported)
          DVLOG(1) << "Xkb extension not available.";
      }));

  // Ask the server not to send KeyRelease event when the user holds down a key.
  // crbug.com/138092
  xkb
      .PerClientFlags({
          .deviceSpec =
              static_cast<x11::Xkb::DeviceSpec>(x11::Xkb::Id::UseCoreKbd),
          .change = x11::Xkb::PerClientFlag::DetectableAutoRepeat,
          .value = x11::Xkb::PerClientFlag::DetectableAutoRepeat,
      })
      .OnResponse(base::BindOnce([](x11::Xkb::PerClientFlagsResponse response) {
        if (!response ||
            !static_cast<bool>(response->supported &
                               x11::Xkb::PerClientFlag::DetectableAutoRepeat)) {
          DVLOG(1) << "Could not set XKB auto repeat flag.";
        }
      }));

  constexpr auto kXkbAllMapPartMask = static_cast<x11::Xkb::MapPart>(0xff);
  xkb.SelectEvents({
      .deviceSpec = static_cast<x11::Xkb::DeviceSpec>(x11::Xkb::Id::UseCoreKbd),
      .affectWhich = x11::Xkb::EventType::NewKeyboardNotify,
      .selectAll = x11::Xkb::EventType::NewKeyboardNotify,
      .affectMap = kXkbAllMapPartMask,
  });
}

x11::Time ExtractTimeFromXEvent(const x11::Event& xev) {
  if (auto* key = xev.As<x11::KeyEvent>())
    return key->time;
  if (auto* button = xev.As<x11::ButtonEvent>())
    return button->time;
  if (auto* motion = xev.As<x11::MotionNotifyEvent>())
    return motion->time;
  if (auto* crossing = xev.As<x11::CrossingEvent>())
    return crossing->time;
  if (auto* prop = xev.As<x11::PropertyNotifyEvent>())
    return prop->time;
  if (auto* sel_clear = xev.As<x11::SelectionClearEvent>())
    return sel_clear->time;
  if (auto* sel_req = xev.As<x11::SelectionRequestEvent>())
    return sel_req->time;
  if (auto* sel_notify = xev.As<x11::SelectionNotifyEvent>())
    return sel_notify->time;
  if (auto* dev_changed = xev.As<x11::Input::DeviceChangedEvent>())
    return dev_changed->time;
  if (auto* device = xev.As<x11::Input::DeviceEvent>())
    return device->time;
  if (auto* xi_crossing = xev.As<x11::Input::CrossingEvent>())
    return xi_crossing->time;
  return x11::Time::CurrentTime;
}

void UpdateDeviceList() {
  auto* connection = x11::Connection::Get();
  DeviceListCacheX11::GetInstance()->UpdateDeviceList(connection);
  TouchFactory::GetInstance()->UpdateDeviceList(connection);
  DeviceDataManagerX11::GetInstance()->UpdateDeviceList(connection);
}

}  // namespace

#if defined(USE_GLIB)
using X11EventWatcherImpl = X11EventWatcherGlib;
#else
using X11EventWatcherImpl = X11EventWatcherFdWatch;
#endif

X11EventSource::X11EventSource(x11::Connection* connection)
    : watcher_(std::make_unique<X11EventWatcherImpl>(this)),
      connection_(connection),
      dispatching_event_(nullptr),
      dummy_initialized_(false),
      distribution_(0, 999) {
  DCHECK(connection_);
  DeviceDataManagerX11::CreateInstance();
  InitializeXkb(connection_);

  watcher_->StartWatching();
}

X11EventSource::~X11EventSource() {
  if (dummy_initialized_)
    connection_->DestroyWindow({dummy_window_});
}

// static
bool X11EventSource::HasInstance() {
  return GetInstance();
}

// static
X11EventSource* X11EventSource::GetInstance() {
  return static_cast<X11EventSource*>(PlatformEventSource::GetInstance());
}

////////////////////////////////////////////////////////////////////////////////
// X11EventSource, public

void X11EventSource::DispatchXEvents() {
  continue_stream_ = true;
  connection_->Dispatch(this);
}

x11::Time X11EventSource::GetCurrentServerTime() {
  DCHECK(connection_);

  if (!dummy_initialized_) {
    // Create a new Window and Atom that will be used for the property change.
    dummy_window_ = connection_->GenerateId<x11::Window>();
    connection_->CreateWindow({
        .wid = dummy_window_,
        .parent = connection_->default_root(),
        .width = 1,
        .height = 1,
        .override_redirect = x11::Bool32(true),
    });
    dummy_atom_ = gfx::GetAtom("CHROMIUM_TIMESTAMP");
    dummy_window_events_ = std::make_unique<XScopedEventSelector>(
        dummy_window_, x11::EventMask::PropertyChange);
    dummy_initialized_ = true;
  }

  // No need to measure Linux.X11.ServerRTT on every call.
  // base::TimeTicks::Now() itself has non-trivial overhead.
  bool measure_rtt = distribution_(generator_) == 0;

  base::TimeTicks start;
  if (measure_rtt)
    start = base::TimeTicks::Now();

  // Make a no-op property change on |dummy_window_|.
  std::vector<uint8_t> data{0};
  connection_->ChangeProperty({
      .window = static_cast<x11::Window>(dummy_window_),
      .property = dummy_atom_,
      .type = x11::Atom::STRING,
      .format = CHAR_BIT,
      .data_len = 1,
      .data = base::RefCountedBytes::TakeVector(&data),
  });

  // Observe the resulting PropertyNotify event to obtain the timestamp.
  connection_->Sync();
  if (measure_rtt) {
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "Linux.X11.ServerRTT",
        (base::TimeTicks::Now() - start).InMicroseconds(), 1,
        base::TimeDelta::FromMilliseconds(50).InMicroseconds(), 50);
  }
  connection_->ReadResponses();

  auto time = x11::Time::CurrentTime;
  auto pred = [&](const x11::Event& event) {
    auto* prop = event.As<x11::PropertyNotifyEvent>();
    if (prop && prop->window == dummy_window_) {
      time = prop->time;
      return true;
    }
    return false;
  };

  auto& events = connection_->events();
  events.erase(std::remove_if(events.begin(), events.end(), pred),
               events.end());
  return time;
}

x11::Time X11EventSource::GetTimestamp() {
  if (dispatching_event_) {
    auto timestamp = ExtractTimeFromXEvent(*dispatching_event_);
    if (timestamp != x11::Time::CurrentTime)
      return timestamp;
  }
  DVLOG(1) << "Making a round trip to get a recent server timestamp.";
  return GetCurrentServerTime();
}

base::Optional<gfx::Point>
X11EventSource::GetRootCursorLocationFromCurrentEvent() const {
  if (!dispatching_event_)
    return base::nullopt;

  DCHECK(dispatching_event_);
  x11::Event* event = dispatching_event_;

  auto* device = event->As<x11::Input::DeviceEvent>();
  auto* crossing = event->As<x11::Input::CrossingEvent>();
  auto* touch_factory = ui::TouchFactory::GetInstance();

  bool is_valid_event = false;
  if (event->As<x11::ButtonEvent>() || event->As<x11::MotionNotifyEvent>() ||
      event->As<x11::CrossingEvent>()) {
    is_valid_event = true;
  } else if (device &&
             (device->opcode == x11::Input::DeviceEvent::ButtonPress ||
              device->opcode == x11::Input::DeviceEvent::ButtonRelease ||
              device->opcode == x11::Input::DeviceEvent::Motion)) {
    is_valid_event = touch_factory->ShouldProcessDeviceEvent(*device);
  } else if (crossing &&
             (crossing->opcode == x11::Input::CrossingEvent::Enter ||
              crossing->opcode == x11::Input::CrossingEvent::Leave)) {
    is_valid_event = touch_factory->ShouldProcessCrossingEvent(*crossing);
  }

  if (is_valid_event)
    return ui::EventSystemLocationFromXEvent(*dispatching_event_);
  return base::nullopt;
}

void X11EventSource::AddXEventDispatcher(XEventDispatcher* dispatcher) {
  dispatchers_xevent_.AddObserver(dispatcher);
  PlatformEventDispatcher* event_dispatcher =
      dispatcher->GetPlatformEventDispatcher();
  if (event_dispatcher)
    AddPlatformEventDispatcher(event_dispatcher);
}

void X11EventSource::RemoveXEventDispatcher(XEventDispatcher* dispatcher) {
  dispatchers_xevent_.RemoveObserver(dispatcher);
  PlatformEventDispatcher* event_dispatcher =
      dispatcher->GetPlatformEventDispatcher();
  if (event_dispatcher)
    RemovePlatformEventDispatcher(event_dispatcher);
}

void X11EventSource::AddXEventObserver(XEventObserver* observer) {
  CHECK(observer);
  observers_.AddObserver(observer);
}

void X11EventSource::RemoveXEventObserver(XEventObserver* observer) {
  CHECK(observer);
  observers_.RemoveObserver(observer);
}

std::unique_ptr<ScopedXEventDispatcher>
X11EventSource::OverrideXEventDispatcher(XEventDispatcher* dispatcher) {
  CHECK(dispatcher);
  overridden_dispatcher_restored_ = false;
  return std::make_unique<ScopedXEventDispatcher>(&overridden_dispatcher_,
                                                  dispatcher);
}

void X11EventSource::RestoreOverridenXEventDispatcher() {
  CHECK(overridden_dispatcher_);
  overridden_dispatcher_restored_ = true;
}

void X11EventSource::DispatchPlatformEvent(const PlatformEvent& event,
                                           x11::Event* xevent) {
  DCHECK(event);

  // First, tell the XEventDispatchers, which can have PlatformEventDispatcher,
  // an ui::Event is going to be sent next. It must make a promise to handle
  // next translated |event| sent by PlatformEventSource based on a XID in
  // |xevent| tested in CheckCanDispatchNextPlatformEvent(). This is needed
  // because it is not possible to access |event|'s associated NativeEvent* and
  // check if it is the event's target window (XID).
  for (XEventDispatcher& dispatcher : dispatchers_xevent_)
    dispatcher.CheckCanDispatchNextPlatformEvent(xevent);

  DispatchEvent(event);

  // Explicitly reset a promise to handle next translated event.
  for (XEventDispatcher& dispatcher : dispatchers_xevent_)
    dispatcher.PlatformEventDispatchFinished();
}

void X11EventSource::DispatchXEventToXEventDispatchers(x11::Event* xevent) {
  bool stop_dispatching = false;

  for (auto& observer : observers_)
    observer.WillProcessXEvent(xevent);

  if (overridden_dispatcher_) {
    stop_dispatching = overridden_dispatcher_->DispatchXEvent(xevent);
  }

  if (!stop_dispatching) {
    for (XEventDispatcher& dispatcher : dispatchers_xevent_) {
      if (dispatcher.DispatchXEvent(xevent))
        break;
    }
  }

  for (auto& observer : observers_)
    observer.DidProcessXEvent(xevent);

  // If an overridden dispatcher has been destroyed, then the event source
  // should halt dispatching the current stream of events, and wait until the
  // next message-loop iteration for dispatching events. This lets any nested
  // message-loop to unwind correctly and any new dispatchers to receive the
  // correct sequence of events.
  if (overridden_dispatcher_restored_)
    StopCurrentEventStream();

  overridden_dispatcher_restored_ = false;
}

void XEventDispatcher::CheckCanDispatchNextPlatformEvent(x11::Event* xev) {}

void XEventDispatcher::PlatformEventDispatchFinished() {}

PlatformEventDispatcher* XEventDispatcher::GetPlatformEventDispatcher() {
  return nullptr;
}

void X11EventSource::ProcessXEvent(x11::Event* xevent) {
  auto translated_event = ui::BuildEventFromXEvent(*xevent);
  // Ignore native platform-events only if they correspond to mouse events.
  // Allow other types of events to still be handled
  if (ui::PlatformEventSource::ShouldIgnoreNativePlatformEvents() &&
      translated_event && translated_event->IsMouseEvent()) {
    return;
  }
  if (translated_event && translated_event->type() != ET_UNKNOWN) {
#if defined(OS_CHROMEOS)
    if (translated_event->IsLocatedEvent()) {
      ui::CursorController::GetInstance()->SetCursorLocation(
          translated_event->AsLocatedEvent()->location_f());
    }
#endif
    DispatchPlatformEvent(translated_event.get(), xevent);
  } else {
    // Only if we can't translate XEvent into ui::Event, try to dispatch XEvent
    // directly to XEventDispatchers.
    DispatchXEventToXEventDispatchers(xevent);
  }
}

////////////////////////////////////////////////////////////////////////////////
// X11EventSource, protected

void X11EventSource::PostDispatchEvent(x11::Event* x11_event) {
  bool should_update_device_list = false;

  if (x11_event->As<x11::Input::HierarchyEvent>()) {
    should_update_device_list = true;
  } else if (auto* device = x11_event->As<x11::Input::DeviceChangedEvent>()) {
    if (device->reason == x11::Input::ChangeReason::DeviceChange) {
      should_update_device_list = true;
    } else if (device->reason == x11::Input::ChangeReason::SlaveSwitch) {
      ui::DeviceDataManagerX11::GetInstance()->InvalidateScrollClasses(
          device->sourceid);
    }
  }

  if (should_update_device_list) {
    UpdateDeviceList();
    hotplug_event_handler_->OnHotplugEvent();
  }

  auto* crossing = x11_event->As<x11::CrossingEvent>();
  if (crossing && crossing->opcode == x11::CrossingEvent::EnterNotify &&
      crossing->detail != x11::NotifyDetail::Inferior &&
      crossing->mode != x11::NotifyMode::Ungrab) {
    // Clear stored scroll data
    ui::DeviceDataManagerX11::GetInstance()->InvalidateScrollClasses(
        DeviceDataManagerX11::kAllDevices);
  }

  auto* mapping = x11_event->As<x11::MappingNotifyEvent>();
  if (mapping && mapping->request == x11::Mapping::Pointer)
    DeviceDataManagerX11::GetInstance()->UpdateButtonMap();
}

void X11EventSource::StopCurrentEventStream() {
  continue_stream_ = false;
}

void X11EventSource::OnDispatcherListChanged() {
  watcher_->StartWatching();

  if (!hotplug_event_handler_) {
    hotplug_event_handler_ = std::make_unique<X11HotplugEventHandler>();
    // Force the initial device query to have an update list of active devices.
    hotplug_event_handler_->OnHotplugEvent();
  }
}

bool X11EventSource::ShouldContinueStream() const {
  return continue_stream_;
}

void X11EventSource::DispatchXEvent(x11::Event* event) {
  // NB: The event should be reset to nullptr when this function
  // returns, not to its initial value, otherwise nested message loops
  // will incorrectly think that the current event being dispatched is
  // an old event.  This means base::AutoReset should not be used.
  dispatching_event_ = event;

  ProcessXEvent(event);
  PostDispatchEvent(event);

  dispatching_event_ = nullptr;
}

// ScopedXEventDispatcher implementation
ScopedXEventDispatcher::ScopedXEventDispatcher(
    XEventDispatcher** scoped_dispatcher,
    XEventDispatcher* new_dispatcher)
    : original_(*scoped_dispatcher),
      restore_(scoped_dispatcher, new_dispatcher) {}

ScopedXEventDispatcher::~ScopedXEventDispatcher() {
  DCHECK(X11EventSource::HasInstance());
  X11EventSource::GetInstance()->RestoreOverridenXEventDispatcher();
}

// static
#if defined(USE_X11)
std::unique_ptr<PlatformEventSource> PlatformEventSource::CreateDefault() {
#if defined(USE_OZONE)
  if (features::IsUsingOzonePlatform())
    return nullptr;
#endif
  return std::make_unique<X11EventSource>(x11::Connection::Get());
}
#endif

}  // namespace ui
