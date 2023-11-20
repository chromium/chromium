// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_idle_query.h"

#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/future.h"
#include "ui/gfx/x/screensaver.h"

namespace ui {

IdleQueryX11::IdleQueryX11() : connection_(x11::Connection::Get()) {}

IdleQueryX11::~IdleQueryX11() = default;

int IdleQueryX11::IdleTime() {
  if (auto reply = connection_->screensaver()
                       .QueryInfo(connection_->default_root())
                       .Sync()) {
    return reply->ms_since_user_input / 1000;
  }
  return 0;
}

}  // namespace ui
