// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_MOUSE_WATCHER_H_
#define UI_VIEWS_MOUSE_WATCHER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/views_export.h"

namespace gfx {
class Point;
}

namespace views {

// MouseWatcherListener is notified when the mouse moves outside the host.
class VIEWS_EXPORT MouseWatcherListener {
 public:
  virtual void MouseMovedOutOfHost() = 0;

 protected:
  virtual ~MouseWatcherListener();
};

// The MouseWatcherHost determines what region is to be monitored.
class VIEWS_EXPORT MouseWatcherHost {
 public:
  // The type of mouse event.
  enum class EventType { kMove, kExit, kPress };

  virtual ~MouseWatcherHost();

  // Return false when the mouse has moved outside the monitored region.
  virtual bool Contains(const gfx::Point& screen_point, EventType type) = 0;
};

// MouseWatcher is used to watch mouse movement and notify its listener when the
// mouse moves outside the bounds of a MouseWatcherHost.
class VIEWS_EXPORT MouseWatcher {
 public:
  // Creates a new MouseWatcher. The |listener| will be notified when the |host|
  // determines that the mouse has moved outside its monitored region.
  // |host| will be owned by the watcher and deleted upon completion, while the
  // listener must remain alive for the lifetime of this object.
  MouseWatcher(std::unique_ptr<MouseWatcherHost> host,
               MouseWatcherListener* listener);

  MouseWatcher(const MouseWatcher&) = delete;
  MouseWatcher& operator=(const MouseWatcher&) = delete;

  ~MouseWatcher();

  // Sets the amount to delay before notifying the listener when the mouse exits
  // the host by way of going to another window.
  void set_notify_on_exit_time(base::TimeDelta time) {
    notify_on_exit_time_ = time;
  }

  // Starts watching mouse movements. When the mouse moves outside the bounds of
  // the host the listener is notified. |Start| may be invoked any number of
  // times. If the mouse moves outside the bounds of the host the listener is
  // notified and watcher stops watching events. |window| must be a window in
  // the hierarchy related to the host. |window| is used to setup initial state,
  // and may be deleted before MouseWatcher.
  void Start(gfx::NativeWindow window);

  // Stops watching mouse events.
  void Stop();

 private:
  class Observer;

  // Are we currently observing events?
  bool is_observing() const { return observer_.get() != nullptr; }

  // Notifies the listener and stops watching events.
  void NotifyListener();

  // Host we're listening for events over.
  std::unique_ptr<MouseWatcherHost> host_;

  // Our listener.
  raw_ptr<MouseWatcherListener> listener_;

  // Does the actual work of listening for mouse events.
  std::unique_ptr<Observer> observer_;

  // See description above setter.
  base::TimeDelta notify_on_exit_time_;
};

}  // namespace views

#endif  // UI_VIEWS_MOUSE_WATCHER_H_
