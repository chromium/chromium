// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/test/test_layers.h"

#include "ui/compositor/layer.h"

namespace ui {
namespace test {

std::string ChildLayerNamesAsString(const ui::Layer& parent) {
  std::string names;
  for (const Layer* child : parent.children()) {
    if (!names.empty())
      names += " ";
    names += child->name();
  }
  return names;
}

}  // namespace test
}  // namespace ui
