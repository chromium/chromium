// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/x/x11_event_translation.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/test/events_test_utils.h"
#include "ui/events/test/events_test_utils_x11.h"

namespace ui {

// Ensure DomKey extraction happens lazily in Ozone X11, while in non-Ozone
// path it is set right away in XEvent => ui::Event translation. This prevents
// regressions such as crbug.com/1007389.
TEST(XEventTranslationTest, KeyEventDomKeyExtraction) {
  ScopedXI2Event xev;
  xev.InitKeyEvent(ET_KEY_PRESSED, VKEY_RETURN, EF_NONE);

  auto keyev = ui::BuildKeyEventFromXEvent(*xev);
  EXPECT_TRUE(keyev);

  KeyEventTestApi test(keyev.get());
#if defined(USE_OZONE)
  EXPECT_EQ(ui::DomKey::NONE, test.dom_key());
#else
  EXPECT_EQ(ui::DomKey::ENTER, test.dom_key());
#endif

  EXPECT_EQ(13, keyev->GetCharacter());
  EXPECT_EQ("Enter", keyev->GetCodeString());
}

}  // namespace ui
