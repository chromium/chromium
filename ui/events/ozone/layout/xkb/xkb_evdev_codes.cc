// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/layout/xkb/xkb_evdev_codes.h"

#include "ui/events/keycodes/dom/keycode_converter.h"

namespace ui {

XkbEvdevCodes::XkbEvdevCodes() {
  invalid_xkb_keycode_ =
      static_cast<xkb_keycode_t>(KeycodeConverter::InvalidNativeKeycode());
}

XkbEvdevCodes::~XkbEvdevCodes() {
}

xkb_keycode_t XkbEvdevCodes::DomCodeToXkbKeyCode(DomCode dom_code) const {
  // This assumes KeycodeConverter has been built with evdev/xkb codes.
  return static_cast<xkb_keycode_t>(
      KeycodeConverter::DomCodeToNativeKeycode(dom_code));
}

}  // namespace ui
