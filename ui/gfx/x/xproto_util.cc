// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/x/xproto_util.h"

namespace x11 {

void DeleteProperty(x11::Window window, x11::Atom name) {
  x11::Connection::Get()->DeleteProperty({
      .window = static_cast<x11::Window>(window),
      .property = name,
  });
}

void SetStringProperty(Window window,
                       Atom property,
                       Atom type,
                       const std::string& value,
                       Connection* connection) {
  std::vector<char> str(value.begin(), value.end());
  SetArrayProperty(window, property, type, str, connection);
}

Window CreateDummyWindow(const std::string& name, Connection* connection) {
  auto window = connection->GenerateId<Window>();
  connection->CreateWindow(CreateWindowRequest{
      .wid = window,
      .parent = connection->default_root(),
      .x = -100,
      .y = -100,
      .width = 10,
      .height = 10,
      .c_class = WindowClass::InputOnly,
      .override_redirect = Bool32(true),
  });
  if (!name.empty())
    SetStringProperty(window, Atom::WM_NAME, Atom::STRING, name);
  return window;
}

}  // namespace x11
