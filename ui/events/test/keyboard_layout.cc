// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/test/keyboard_layout.h"

#include "base/check_op.h"
#include "base/notreached.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/events/ozone/layout/stub/stub_keyboard_layout_engine.h"  // nogncheck
#endif

namespace ui {

ScopedKeyboardLayout::ScopedKeyboardLayout(KeyboardLayout layout) {
#if BUILDFLAG(IS_OZONE)
  CHECK_EQ(layout, KEYBOARD_LAYOUT_ENGLISH_US);
  auto keyboard_layout_engine = std::make_unique<StubKeyboardLayoutEngine>();
  scoped_keyboard_layout_engine_ = std::make_unique<ScopedKeyboardLayoutEngine>(
      std::move(keyboard_layout_engine));
#elif BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  original_layout_ = GetActiveLayout();
  ActivateLayout(GetPlatformKeyboardLayout(layout));
#else
  NOTIMPLEMENTED();
#endif
}

ScopedKeyboardLayout::~ScopedKeyboardLayout() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  ActivateLayout(original_layout_);
#endif
}

}  // namespace ui
