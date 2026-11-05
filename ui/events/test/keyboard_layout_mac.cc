// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/test/keyboard_layout.h"

#include <cstdlib>

#include "base/check_op.h"

namespace ui {

PlatformKeyboardLayout GetPlatformKeyboardLayout(KeyboardLayout layout) {
  // On macOS, TISSelectInputSource() mutates the global login-session keyboard
  // layout across all processes and persists to com.apple.HIToolbox.plist. Only
  // US English is supported via ScopedKeyboardLayout on macOS.
  CHECK_EQ(KEYBOARD_LAYOUT_ENGLISH_US, layout);

  base::apple::ScopedCFTypeRef<CFMutableDictionaryRef> input_source_list_filter(
      CFDictionaryCreateMutable(kCFAllocatorDefault, 1,
                                &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks));
  CFDictionaryAddValue(input_source_list_filter.get(),
                       kTISPropertyInputSourceID,
                       CFSTR("com.apple.keylayout.US"));
  base::apple::ScopedCFTypeRef<CFArrayRef> input_source_list(
      TISCreateInputSourceList(input_source_list_filter.get(),
                               /*includeAllInstalled=*/true));
  if (!input_source_list || CFArrayGetCount(input_source_list.get()) != 1) {
    return PlatformKeyboardLayout();
  }

  return base::apple::ScopedCFTypeRef<TISInputSourceRef>(
      (TISInputSourceRef)CFArrayGetValueAtIndex(input_source_list.get(), 0),
      base::scoped_policy::RETAIN);
}

PlatformKeyboardLayout ScopedKeyboardLayout::GetActiveLayout() {
  // On Swarming bots, always restore KEYBOARD_LAYOUT_ENGLISH_US on teardown so
  // a bot whose OS keyboard layout was previously poisoned is healed back to US
  // English rather than having the poisoned layout restored.
  if (std::getenv("SWARMING_TASK_ID") != nullptr) {
    return GetPlatformKeyboardLayout(KEYBOARD_LAYOUT_ENGLISH_US);
  }
  return PlatformKeyboardLayout(TISCopyCurrentKeyboardInputSource());
}

void ScopedKeyboardLayout::ActivateLayout(PlatformKeyboardLayout layout) {
  CHECK(layout, base::NotFatalUntil::M163);
  // According to the documentation in HIToolbox's TextInputSources.h
  // (recommended reading), TISSelectInputSource() can fail if the input source
  // isn't "selectable" or "enabled".
  //
  // On the bots, for some reason, sometimes the US keyboard layout isn't
  // "enabled" even though it is present - we aren't sure why this happens,
  // perhaps if input sources have never been switched on this bot before? In
  // any case, it's harmless to re-enable it here if it's already enabled.
  OSStatus result = TISEnableInputSource(layout.get());
  CHECK_EQ(noErr, result, base::NotFatalUntil::M163);
  result = TISSelectInputSource(layout.get());
  CHECK_EQ(noErr, result, base::NotFatalUntil::M163);
}

}  // namespace ui
