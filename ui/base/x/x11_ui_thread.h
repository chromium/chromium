// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_X11_UI_THREAD_H_
#define UI_BASE_X_X11_UI_THREAD_H_

#include "base/component_export.h"
#include "base/threading/thread.h"

namespace x11 {
class Connection;
}

namespace ui {

class X11EventSource;

// A thread-local x11::Connection may be used on this thread, stored in TLS and
// obtained via x11::Connection::Get(). X11 events for the connection may also
// dispatch events on this thread.  This thread must be started as TYPE_UI.
// TODO(thomasanderson): This can be removed once Linux switches to ozone.
class COMPONENT_EXPORT(UI_BASE_X) X11UiThread : public base::Thread {
 public:
  explicit X11UiThread(const std::string& thread_name);

  ~X11UiThread() override;

  X11UiThread(const X11UiThread&) = delete;
  X11UiThread& operator=(const X11UiThread&) = delete;

  // Sets the global connection which will have its ownership transferred to the
  // next X11UiThread created.
  static void SetConnection(x11::Connection* connection);

 protected:
  // base::Thread:
  void Init() override;
  void CleanUp() override;

 private:
  std::unique_ptr<x11::Connection> connection_;
  std::unique_ptr<X11EventSource> event_source_;
};

}  // namespace ui

#endif  // UI_BASE_X_X11_UI_THREAD_H_
