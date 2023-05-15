// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/events.h"

namespace ui {
namespace {

// If this property is present, then it means the input field should suppress
// key autorepeat.
constexpr char kPropertySuppressAutoRepeat[] = "suppress_auto_repeat";

}  // namespace

bool HasKeyEventSuppressAutoRepeat(const ui::Event::Properties& properties) {
  return properties.count(kPropertySuppressAutoRepeat);
}

void SetKeyEventSuppressAutoRepeat(ui::Event::Properties& properties) {
  properties[kPropertySuppressAutoRepeat] = {};
}

}  // namespace ui
