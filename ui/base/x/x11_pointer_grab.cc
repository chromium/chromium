// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_pointer_grab.h"

#include "base/cancelable_callback.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/devices/x11/device_data_manager_x11.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/xinput.h"
#include "ui/gfx/x/xproto.h"

namespace ui {

namespace {

// The grab window. None if there are no active pointer grabs.
x11::Window g_grab_window = x11::Window::None;

// The "owner events" parameter used to grab the pointer.
bool g_owner_events = false;

base::CancelableOnceCallback<void(x11::Cursor)>& GetGrabCallback() {
  static base::NoDestructor<base::CancelableOnceCallback<void(x11::Cursor)>>
      callback;
  return *callback;
}

x11::GrabStatus GrabPointerImpl(x11::Window window,
                                bool owner_events,
                                x11::Cursor cursor) {
  GetGrabCallback().Cancel();
  auto result = x11::GrabStatus::InvalidTime;
  auto* connection = x11::Connection::Get();
  if (ui::IsXInput2Available()) {
    // Do an xinput pointer grab. If there is an active xinput pointer grab
    // as a result of normal button press, GrabPointer() will fail.
    auto mask = x11::Input::XIEventMask::ButtonPress |
                x11::Input::XIEventMask::ButtonRelease |
                x11::Input::XIEventMask::Motion |
                x11::Input::XIEventMask::TouchBegin |
                x11::Input::XIEventMask::TouchUpdate |
                x11::Input::XIEventMask::TouchEnd;
    static_assert(sizeof(mask) == 4);

    for (auto master_pointer :
         ui::DeviceDataManagerX11::GetInstance()->master_pointers()) {
      x11::Input::XIGrabDeviceRequest req{
          .window = window,
          .time = x11::Time::CurrentTime,
          .cursor = cursor,
          .deviceid = master_pointer,
          .mode = x11::GrabMode::Async,
          .paired_device_mode = x11::GrabMode::Async,
          .owner_events = owner_events ? x11::Input::GrabOwner::Owner
                                       : x11::Input::GrabOwner::NoOwner,
          .mask = {static_cast<uint32_t>(mask)},
      };
      if (auto reply = connection->xinput().XIGrabDevice(req).Sync())
        result = reply->status;

      // Assume that the grab will succeed on either all or none of the master
      // pointers.
      if (result != x11::GrabStatus::Success) {
        // Try core pointer grab.
        break;
      }
    }
  }

  if (result != x11::GrabStatus::Success) {
    auto mask = x11::EventMask::PointerMotion | x11::EventMask::ButtonRelease |
                x11::EventMask::ButtonPress;
    x11::GrabPointerRequest req{
        .owner_events = owner_events,
        .grab_window = window,
        .event_mask = mask,
        .pointer_mode = x11::GrabMode::Async,
        .keyboard_mode = x11::GrabMode::Async,
        .confine_to = x11::Window::None,
        .cursor = cursor,
        .time = x11::Time::CurrentTime,
    };
    if (auto reply = connection->GrabPointer(req).Sync())
      result = reply->status;
  }

  if (result == x11::GrabStatus::Success) {
    g_grab_window = window;
    g_owner_events = owner_events;
  }
  return result;
}

}  // namespace

x11::GrabStatus GrabPointer(x11::Window window,
                            bool owner_events,
                            scoped_refptr<ui::X11Cursor> cursor) {
  if (!cursor)
    return GrabPointerImpl(window, owner_events, x11::Cursor::None);
  if (cursor->loaded())
    return GrabPointerImpl(window, owner_events, cursor->xcursor());

  auto result = GrabPointerImpl(window, owner_events, x11::Cursor::None);
  GetGrabCallback().Reset(base::BindOnce(base::IgnoreResult(GrabPointerImpl),
                                         window, owner_events));
  cursor->OnCursorLoaded(GetGrabCallback().callback());
  return result;
}

void ChangeActivePointerGrabCursor(scoped_refptr<ui::X11Cursor> cursor) {
  if (g_grab_window != x11::Window::None)
    GrabPointer(g_grab_window, g_owner_events, cursor);
}

void UngrabPointer() {
  GetGrabCallback().Cancel();
  g_grab_window = x11::Window::None;
  auto* connection = x11::Connection::Get();
  if (ui::IsXInput2Available()) {
    for (auto master_pointer :
         ui::DeviceDataManagerX11::GetInstance()->master_pointers()) {
      connection->xinput()
          .XIUngrabDevice({x11::Time::CurrentTime, master_pointer})
          .IgnoreError();
    }
  }
  // Try core pointer ungrab in case the XInput2 pointer ungrab failed.
  connection->UngrabPointer().IgnoreError();
}

}  // namespace ui
