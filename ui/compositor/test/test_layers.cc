// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/test/test_layers.h"

#include "ui/compositor/layer.h"

namespace ui {
namespace test {

std::string ChildLayerNamesAsString(const ui::Layer& parent) {
  std::string names;
  for (auto it = parent.children().begin(); it != parent.children().end();
       ++it) {
    if (!names.empty())
      names += " ";
    names += (*it)->name();
  }
  return names;
}

}  // namespace test
}  // namespace ui
