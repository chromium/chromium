// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/test/keyboard_layout.h"

#include "base/check_op.h"
#include "base/strings/sys_string_conversions.h"

namespace ui {

PlatformKeyboardLayout GetPlatformKeyboardLayout(KeyboardLayout layout) {
  // Right now tests only need US English.  If other layouts need to be
  // supported in the future this code should be extended.
  DCHECK_EQ(KEYBOARD_LAYOUT_ENGLISH_US, layout);

  const char kUsInputSourceId[] = "com.apple.keylayout.US";

  base::apple::ScopedCFTypeRef<CFMutableDictionaryRef> input_source_list_filter(
      CFDictionaryCreateMutable(kCFAllocatorDefault, 1,
                                &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks));
  base::apple::ScopedCFTypeRef<CFStringRef> input_source_id_ref =
      base::SysUTF8ToCFStringRef(kUsInputSourceId);
  CFDictionaryAddValue(input_source_list_filter.get(),
                       kTISPropertyInputSourceID, input_source_id_ref.get());
  base::apple::ScopedCFTypeRef<CFArrayRef> input_source_list(
      TISCreateInputSourceList(input_source_list_filter.get(), true));
  if (CFArrayGetCount(input_source_list.get()) != 1) {
    return PlatformKeyboardLayout();
  }

  return base::apple::ScopedCFTypeRef<TISInputSourceRef>(
      (TISInputSourceRef)CFArrayGetValueAtIndex(input_source_list.get(), 0),
      base::scoped_policy::RETAIN);
}

PlatformKeyboardLayout ScopedKeyboardLayout::GetActiveLayout() {
  return PlatformKeyboardLayout(TISCopyCurrentKeyboardInputSource());
}

void ScopedKeyboardLayout::ActivateLayout(PlatformKeyboardLayout layout) {
  DCHECK(layout);
  // According to the documentation in HIToolbox's TextInputSources.h
  // (recommended reading), TISSelectInputSource() can fail if the input source
  // isn't "selectable" or "enabled".
  //
  // On the bots, for some reason, sometimes the US keyboard layout isn't
  // "enabled" even though it is present - we aren't sure why this happens,
  // perhaps if input sources have never been switched on this bot before? In
  // any case, it's harmless to re-enable it here if it's already enabled.
  OSStatus result = TISEnableInputSource(layout.get());
  DCHECK_EQ(noErr, result);
  result = TISSelectInputSource(layout.get());
  DCHECK_EQ(noErr, result);
}

}  // namespace ui
