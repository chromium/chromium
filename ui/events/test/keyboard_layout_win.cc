// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/test/keyboard_layout.h"

#include "base/check.h"
#include "base/notreached.h"

namespace ui {

// |LoadKeyboardLayout()| ensures the locale to be loaded into the system
// (Similar to temporarily adding a locale in Control Panel), otherwise
// |ToUnicodeEx()| will fall-back to the default locale.
// See MSDN LoadKeyboardLayout():
// https://msdn.microsoft.com/en-us/library/windows/desktop/ms646305(v=vs.85).aspx
// And language constants and strings:
// https://msdn.microsoft.com/en-us/library/windows/desktop/dd318693(v=vs.85).aspx
PlatformKeyboardLayout GetPlatformKeyboardLayout(KeyboardLayout layout) {
  switch (layout) {
    case KEYBOARD_LAYOUT_ENGLISH_US:
      return LoadKeyboardLayout(L"00000409", KLF_ACTIVATE);
    case KEYBOARD_LAYOUT_FRENCH:
      return LoadKeyboardLayout(L"0000040c", KLF_ACTIVATE);
    case KEYBOARD_LAYOUT_GERMAN:
      return LoadKeyboardLayout(L"00000407", KLF_ACTIVATE);
    case KEYBOARD_LAYOUT_GREEK:
      return LoadKeyboardLayout(L"00000408", KLF_ACTIVATE);
    case KEYBOARD_LAYOUT_JAPANESE:
      return LoadKeyboardLayout(L"00000411", KLF_ACTIVATE);
    case KEYBOARD_LAYOUT_KOREAN:
      return LoadKeyboardLayout(L"00000412", KLF_ACTIVATE);
    case KEYBOARD_LAYOUT_RUSSIAN:
      return LoadKeyboardLayout(L"00000419", KLF_ACTIVATE);
  }

  NOTREACHED_IN_MIGRATION();
  return 0;
}

PlatformKeyboardLayout ScopedKeyboardLayout::GetActiveLayout() {
  return GetKeyboardLayout(0);
}

void ScopedKeyboardLayout::ActivateLayout(PlatformKeyboardLayout layout) {
  DCHECK(layout);
  HKL result = ActivateKeyboardLayout(layout, 0);
  DCHECK(!!result);
}

}  // namespace ui
