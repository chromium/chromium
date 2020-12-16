// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/x/xproto_util.h"

namespace x11 {

void SetStringProperty(x11::Window window,
                       x11::Atom property,
                       x11::Atom type,
                       const std::string& value,
                       x11::Connection* connection) {
  std::vector<char> str(value.begin(), value.end());
  SetArrayProperty(window, property, type, str, connection);
}

Window CreateDummyWindow(const std::string& name, x11::Connection* connection) {
  auto window = connection->GenerateId<x11::Window>();
  connection->CreateWindow(x11::CreateWindowRequest{
      .wid = window,
      .parent = connection->default_root(),
      .x = -100,
      .y = -100,
      .width = 10,
      .height = 10,
      .c_class = x11::WindowClass::InputOnly,
      .override_redirect = x11::Bool32(true),
  });
  if (!name.empty())
    SetStringProperty(window, x11::Atom::WM_NAME, x11::Atom::STRING, name);
  return window;
}

}  // namespace x11
