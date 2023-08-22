// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_TEST_KEYBOARD_LAYOUT_H_
#define UI_EVENTS_TEST_KEYBOARD_LAYOUT_H_

#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#elif BUILDFLAG(IS_MAC)
#include <Carbon/Carbon.h>
#include "base/apple/scoped_cftyperef.h"
#elif BUILDFLAG(IS_OZONE)
#include "ui/events/ozone/layout/scoped_keyboard_layout_engine.h"  // nogncheck
#endif

namespace ui {

enum KeyboardLayout {
  KEYBOARD_LAYOUT_ENGLISH_US,
#if BUILDFLAG(IS_WIN)
  KEYBOARD_LAYOUT_FRENCH,
  KEYBOARD_LAYOUT_GERMAN,
  KEYBOARD_LAYOUT_GREEK,
  KEYBOARD_LAYOUT_JAPANESE,
  KEYBOARD_LAYOUT_KOREAN,
  KEYBOARD_LAYOUT_RUSSIAN,
#endif
};

#if BUILDFLAG(IS_WIN)
using PlatformKeyboardLayout = HKL;
#elif BUILDFLAG(IS_MAC)
using PlatformKeyboardLayout = base::apple::ScopedCFTypeRef<TISInputSourceRef>;
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
PlatformKeyboardLayout GetPlatformKeyboardLayout(KeyboardLayout layout);
#endif

// Changes the active keyboard layout for the scope of this object.
class ScopedKeyboardLayout {
 public:
  explicit ScopedKeyboardLayout(KeyboardLayout layout);

  ScopedKeyboardLayout(const ScopedKeyboardLayout&) = delete;
  ScopedKeyboardLayout& operator=(const ScopedKeyboardLayout&) = delete;

  ~ScopedKeyboardLayout();

 private:
#if BUILDFLAG(IS_OZONE)
  std::unique_ptr<ScopedKeyboardLayoutEngine> scoped_keyboard_layout_engine_;
#endif
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  static PlatformKeyboardLayout GetActiveLayout();
  static void ActivateLayout(PlatformKeyboardLayout layout);

  PlatformKeyboardLayout original_layout_;
#endif
};

}  // namespace ui

#endif  // UI_EVENTS_TEST_KEYBOARD_LAYOUT_H_
