// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_ui_thread.h"

#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/x/connection.h"

namespace ui {

namespace {

x11::Connection* g_connection = nullptr;

}

X11UiThread::X11UiThread(const std::string& thread_name)
    : base::Thread(thread_name) {
  connection_.reset(g_connection);
  g_connection = nullptr;
  // When using in-process GPU, g_connection doesn't get set. But we can just
  // open a new connection now since there's no GPU sandbox in place.
  if (!connection_)
    connection_ = x11::Connection::Get()->Clone();
  connection_->DetachFromSequence();
}

X11UiThread::~X11UiThread() = default;

void X11UiThread::SetConnection(x11::Connection* connection) {
  DCHECK(!g_connection);
  g_connection = connection;
}

void X11UiThread::Init() {
  // Connection and X11EventSource make use of TLS, so these calls must be made
  // on the thread, not in the constructor/destructor.
  auto* connection = connection_.get();
  x11::Connection::Set(std::move(connection_));
  event_source_ = std::make_unique<X11EventSource>(connection);
}

void X11UiThread::CleanUp() {
  event_source_.reset();
  connection_.reset();
}

}  // namespace ui