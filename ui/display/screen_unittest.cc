// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/screen.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"
#include "ui/display/scoped_display_for_new_windows.h"
#include "ui/display/test/test_screen.h"
#include "ui/gfx/native_widget_types.h"

namespace display {

namespace {

const int DEFAULT_DISPLAY_ID = 0x1337;
const int DEFAULT_DISPLAY_WIDTH = 2560;
const int DEFAULT_DISPLAY_HEIGHT = 1440;

const int DISPLAY_2_ID = 0xc001;

}  // namespace

class ScreenTest : public testing::Test {
 protected:
  ScreenTest() {
    const Display test_display = test_screen_.GetPrimaryDisplay();
    Display display(test_display);
    display.set_id(DEFAULT_DISPLAY_ID);
    display.set_bounds(
        gfx::Rect(0, 0, DEFAULT_DISPLAY_WIDTH, DEFAULT_DISPLAY_HEIGHT));
    test_screen_.display_list().RemoveDisplay(test_display.id());
    test_screen_.display_list().AddDisplay(display, DisplayList::Type::PRIMARY);
    test_screen_.display_list().AddDisplay(Display(DISPLAY_2_ID),
                                           DisplayList::Type::NOT_PRIMARY);
    Screen::SetScreenInstance(&test_screen_);
  }

  ~ScreenTest() override { Screen::SetScreenInstance(nullptr); }

 private:
  test::TestScreen test_screen_;

  DISALLOW_COPY_AND_ASSIGN(ScreenTest);
};

TEST_F(ScreenTest, GetPrimaryDisplaySize) {
  const gfx::Size size = Screen::GetScreen()->GetPrimaryDisplay().size();
  EXPECT_EQ(DEFAULT_DISPLAY_WIDTH, size.width());
  EXPECT_EQ(DEFAULT_DISPLAY_HEIGHT, size.height());
}

TEST_F(ScreenTest, GetNumDisplays) {
  EXPECT_EQ(Screen::GetScreen()->GetNumDisplays(), 2);
}

TEST_F(ScreenTest, GetDisplayWithDisplayId) {
  Display display;
  EXPECT_TRUE(Screen::GetScreen()->GetDisplayWithDisplayId(DEFAULT_DISPLAY_ID,
                                                           &display));
  EXPECT_EQ(DEFAULT_DISPLAY_ID, display.id());
  EXPECT_EQ(DEFAULT_DISPLAY_WIDTH, display.size().width());
  EXPECT_EQ(DEFAULT_DISPLAY_HEIGHT, display.size().height());
}

TEST_F(ScreenTest, GetDisplayForNewWindows) {
  Screen* screen = Screen::GetScreen();

  // Display for new windows defaults to the primary display.
  EXPECT_EQ(screen->GetPrimaryDisplay().id(),
            screen->GetDisplayForNewWindows().id());
}

TEST_F(ScreenTest, ScopedDisplayForNewWindows) {
  Screen* screen = Screen::GetScreen();

  // Set primary as default;
  screen->SetDisplayForNewWindows(DEFAULT_DISPLAY_ID);
  EXPECT_EQ(DEFAULT_DISPLAY_ID, screen->GetDisplayForNewWindows().id());

  // ScopedDisplayForNewWindows overrides while it is in scope.
  {
    ScopedDisplayForNewWindows scoped(DISPLAY_2_ID);
    EXPECT_EQ(DISPLAY_2_ID, screen->GetDisplayForNewWindows().id());
  }

  EXPECT_EQ(DEFAULT_DISPLAY_ID, screen->GetDisplayForNewWindows().id());
}

TEST_F(ScreenTest, GetDisplayListNearestWindowWithFallbacks) {
  Screen* screen = Screen::GetScreen();
  DisplayList display_list =
      screen->GetDisplayListNearestWindowWithFallbacks(gfx::kNullNativeWindow);
  ASSERT_FALSE(display_list.displays().empty());
  EXPECT_EQ(screen->GetPrimaryDisplay().id(),
            display_list.GetPrimaryDisplay().id());
  EXPECT_EQ(screen->GetPrimaryDisplay().id(),
            display_list.GetCurrentDisplay().id());
}

}  // namespace display
