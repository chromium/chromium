// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_X11_IDLE_QUERY_H_
#define UI_BASE_X_X11_IDLE_QUERY_H_

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"

namespace x11 {
class Connection;
}

namespace ui {

class COMPONENT_EXPORT(UI_BASE_X) IdleQueryX11 {
 public:
  IdleQueryX11();

  IdleQueryX11(const IdleQueryX11&) = delete;
  IdleQueryX11& operator=(const IdleQueryX11&) = delete;

  ~IdleQueryX11();

  int IdleTime();

 private:
  raw_ptr<x11::Connection> connection_;
};

}  // namespace ui

#endif  // UI_BASE_X_X11_IDLE_QUERY_H_
