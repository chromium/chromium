// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <type_traits>

#include "base/auto_reset.h"
#include "base/logging.h"
#include "base/memory/free_deleter.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "build/chromeos_buildflags.h"
#include "ui/events/devices/x11/device_data_manager_x11.h"
#include "ui/events/devices/x11/touch_factory_x11.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/keyboard_code_conversion_x.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/events/platform/x11/x11_hotplug_event_handler.h"
#include "ui/events/x/events_x_utils.h"
#include "ui/events/x/x11_event_translation.h"
#include "ui/gfx/x/atom_cache.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/extension_manager.h"
#include "ui/gfx/x/future.h"
#include "ui/gfx/x/window_event_manager.h"
#include "ui/gfx/x/xkb.h"
#include "ui/gfx/x/xproto.h"

#if defined(USE_GLIB)
#include "ui/events/platform/x11/x11_event_watcher_glib.h"
#else
#include "ui/events/platform/x11/x11_event_watcher_fdwatch.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ui/events/ozone/chromeos/cursor_controller.h"
#endif

namespace ui {

namespace {

void InitializeXkb(x11::Connection* connection) {
  auto& xkb = connection->xkb();

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
  xkb.SelectEvents(x11::Xkb::SelectEventsRequest{
      .deviceSpec = static_cast<x11::Xkb::DeviceSpec>(x11::Xkb::Id::UseCoreKbd),
      .affectWhich = x11::Xkb::EventType::NewKeyboardNotify,
      .selectAll = x11::Xkb::EventType::NewKeyboardNotify,
      .affectMap = kXkbAllMapPartMask,
  });
}

x11::Time ExtractTimeFromXEvent(const x11::Event& xev) {
  if (auto* key = xev.As<x11::KeyEvent>()) {
    return key->time;
  }
  if (auto* button = xev.As<x11::ButtonEvent>()) {
    return button->time;
  }
  if (auto* motion = xev.As<x11::MotionNotifyEvent>()) {
    return motion->time;
  }
  if (auto* crossing = xev.As<x11::CrossingEvent>()) {
    return crossing->time;
  }
  if (auto* prop = xev.As<x11::PropertyNotifyEvent>()) {
    return prop->time;
  }
  if (auto* sel_clear = xev.As<x11::SelectionClearEvent>()) {
    return sel_clear->time;
  }
  if (auto* sel_req = xev.As<x11::SelectionRequestEvent>()) {
    return sel_req->time;
  }
  if (auto* sel_notify = xev.As<x11::SelectionNotifyEvent>()) {
    return sel_notify->time;
  }
  if (auto* dev_changed = xev.As<x11::Input::DeviceChangedEvent>()) {
    return dev_changed->time;
  }
  if (auto* device = xev.As<x11::Input::DeviceEvent>()) {
    return device->time;
  }
  if (auto* xi_crossing = xev.As<x11::Input::CrossingEvent>()) {
    return xi_crossing->time;
  }
  return x11::Time::CurrentTime;
}

void UpdateDeviceList() {
  auto* connection = x11::Connection::Get();
  DeviceListCacheX11::GetInstance()->UpdateDeviceList(connection);
  TouchFactory::GetInstance()->UpdateDeviceList(connection);
  DeviceDataManagerX11::GetInstance()->UpdateDeviceList(connection);
}

std::optional<gfx::Point> GetRootCursorLocationFromEvent(
    const x11::Event& event) {
  auto* device = event.As<x11::Input::DeviceEvent>();
  auto* crossing = event.As<x11::Input::CrossingEvent>();
  auto* touch_factory = ui::TouchFactory::GetInstance();

  bool is_valid_event = false;
  if (event.As<x11::ButtonEvent>() || event.As<x11::MotionNotifyEvent>() ||
      event.As<x11::CrossingEvent>()) {
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

  if (is_valid_event) {
    return ui::EventSystemLocationFromXEvent(event);
  }
  return std::nullopt;
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
      dummy_initialized_(false) {
  DCHECK(connection_);
  connection_->AddEventObserver(this);

  DeviceDataManagerX11::CreateInstance();
  InitializeXkb(connection_);

  watcher_->StartWatching();
}

X11EventSource::~X11EventSource() {
  if (dummy_initialized_) {
    connection_->DestroyWindow({dummy_window_});
  }
  connection_->RemoveEventObserver(this);
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

x11::Time X11EventSource::GetCurrentServerTime() {
  DCHECK(connection_);

  if (!dummy_initialized_) {
    // Create a new Window and Atom that will be used for the property change.
    dummy_window_ = connection_->GenerateId<x11::Window>();
    connection_->CreateWindow(x11::CreateWindowRequest{
        .wid = dummy_window_,
        .parent = connection_->default_root(),
        .width = 1,
        .height = 1,
        .override_redirect = x11::Bool32(true),
    });
    dummy_atom_ = x11::GetAtom("CHROMIUM_TIMESTAMP");
    dummy_window_events_ = connection_->ScopedSelectEvent(
        dummy_window_, x11::EventMask::PropertyChange);
    dummy_initialized_ = true;
  }

  // Make a no-op property change on |dummy_window_|.
  std::vector<uint8_t> data({0});
  connection_->ChangeProperty(x11::ChangePropertyRequest{
      .window = static_cast<x11::Window>(dummy_window_),
      .property = dummy_atom_,
      .type = x11::Atom::STRING,
      .format = CHAR_BIT,
      .data_len = base::checked_cast<uint32_t>(data.size()),
      .data = base::MakeRefCounted<base::RefCountedBytes>(std::move(data)),
  });

  // Observe the resulting PropertyNotify event to obtain the timestamp.
  connection_->Sync();
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
  auto it = base::ranges::find_if(events, pred);
  if (it != events.end()) {
    *it = x11::Event();
  }
  return time;
}

x11::Time X11EventSource::GetTimestamp() {
  if (auto* dispatching_event = connection_->dispatching_event()) {
    auto timestamp = ExtractTimeFromXEvent(*dispatching_event);
    if (timestamp != x11::Time::CurrentTime) {
      return timestamp;
    }
  }
  DVLOG(1) << "Making a round trip to get a recent server timestamp.";
  return GetCurrentServerTime();
}

void X11EventSource::ClearLastCursorLocation() {
  last_cursor_location_.reset();
}

////////////////////////////////////////////////////////////////////////////////
// X11EventSource, protected

void X11EventSource::OnEvent(const x11::Event& x11_event) {
  auto cursor_location = GetRootCursorLocationFromEvent(x11_event);
  if (cursor_location.has_value()) {
    last_cursor_location_ = cursor_location;
  }

  bool should_update_device_list = false;

  if (x11_event.As<x11::Input::HierarchyEvent>()) {
    should_update_device_list = true;
  } else if (auto* device = x11_event.As<x11::Input::DeviceChangedEvent>()) {
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

  auto* crossing = x11_event.As<x11::CrossingEvent>();
  if (crossing && crossing->opcode == x11::CrossingEvent::EnterNotify &&
      crossing->detail != x11::NotifyDetail::Inferior &&
      crossing->mode != x11::NotifyMode::Ungrab) {
    // Clear stored scroll data
    ui::DeviceDataManagerX11::GetInstance()->InvalidateScrollClasses(
        DeviceDataManagerX11::kAllDevices);
  }

  auto* mapping = x11_event.As<x11::MappingNotifyEvent>();
  if (mapping && mapping->request == x11::Mapping::Pointer) {
    DeviceDataManagerX11::GetInstance()->UpdateButtonMap();
  }

  auto translated_event = ui::BuildEventFromXEvent(x11_event);
  // Ignore native platform-events only if they correspond to mouse events.
  // Allow other types of events to still be handled
  if (ui::PlatformEventSource::ShouldIgnoreNativePlatformEvents() &&
      translated_event && translated_event->IsMouseEvent()) {
    return;
  }
  if (translated_event && translated_event->type() != EventType::kUnknown) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    if (translated_event->IsLocatedEvent()) {
      ui::CursorController::GetInstance()->SetCursorLocation(
          translated_event->AsLocatedEvent()->location_f());
    }
#endif
    DispatchEvent(translated_event.get());
  }
}

void X11EventSource::OnDispatcherListChanged() {
  watcher_->StartWatching();

  if (!hotplug_event_handler_) {
    hotplug_event_handler_ = std::make_unique<X11HotplugEventHandler>();
    // Force the initial device query to have an update list of active devices.
    hotplug_event_handler_->OnHotplugEvent();
  }
}

}  // namespace ui
