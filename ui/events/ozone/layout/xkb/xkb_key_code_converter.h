// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_LAYOUT_XKB_XKB_KEY_CODE_CONVERTER_H_
#define UI_EVENTS_OZONE_LAYOUT_XKB_XKB_KEY_CODE_CONVERTER_H_

#include <xkbcommon/xkbcommon.h>

namespace ui {

enum class DomCode : uint32_t;

// XKB scan code values are platform-dependent; this provides the layout engine
// with the mapping from DomCode to xkb_keycode_t. (This mapping is in principle
// derivable from the XKB keyboard layout, but xkbcommon does not provide a
// practical interface to do so.)
class COMPONENT_EXPORT(EVENTS_OZONE_LAYOUT) XkbKeyCodeConverter {
 public:
  XkbKeyCodeConverter();
  virtual ~XkbKeyCodeConverter();
  xkb_keycode_t InvalidXkbKeyCode() const { return invalid_xkb_keycode_; }
  virtual xkb_keycode_t DomCodeToXkbKeyCode(DomCode dom_code) const = 0;

 protected:
  xkb_keycode_t invalid_xkb_keycode_;
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_LAYOUT_XKB_XKB_KEY_CODE_CONVERTER_H_
