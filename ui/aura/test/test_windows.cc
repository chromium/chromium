// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/test_windows.h"

#include <stddef.h>

#include <algorithm>

#include "base/strings/string_number_conversions.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rect.h"

namespace aura::test {

std::unique_ptr<Window> CreateTestWindow(WindowBuilderParams params,
                                         std::optional<SkColor> color) {
  TestWindowBuilder builder(params);
  if (color) {
    builder.SetDelegate(new ColorTestWindowDelegate(*color));
  }
  return builder.AllowAllWindowStates().Build();
}

template <typename T>
bool ObjectIsAbove(T* upper, T* lower) {
  DCHECK_EQ(upper->parent(), lower->parent());
  DCHECK_NE(upper, lower);
  const auto& children = upper->parent()->children();
  const size_t upper_i = std::ranges::find(children, upper) - children.begin();
  const size_t lower_i = std::ranges::find(children, lower) - children.begin();
  return upper_i > lower_i;
}

bool WindowIsAbove(Window* upper, Window* lower) {
  return ObjectIsAbove<Window>(upper, lower);
}

bool LayerIsAbove(Window* upper, Window* lower) {
  return ObjectIsAbove<ui::Layer>(upper->layer(), lower->layer());
}

std::string ChildWindowIDsAsString(aura::Window* parent) {
  std::string result;
  for (auto i = parent->children().begin(); i != parent->children().end();
       ++i) {
    if (!result.empty())
      result += " ";
    result += base::NumberToString((*i)->GetId());
  }
  return result;
}

}  // namespace aura::test
