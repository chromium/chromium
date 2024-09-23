// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/base/cocoa/defaults_utils.h"

#include <AppKit/AppKit.h>

#include "base/time/time.h"
#import "ui/base/test/cocoa_helper.h"

namespace ui::cocoa {
namespace {

const int k750MS = 750;
const int k250MS = 250;
const int kTwoHoursInMS = 2 * 60 * base::Time::kMillisecondsPerSecond;
const base::TimeDelta kInfiniteBlinkTime = base::Milliseconds(0);

NSString* const kInsertionPointBlinkPeriodOn =
    @"NSTextInsertionPointBlinkPeriodOn";
NSString* const kInsertionPointBlinkPeriodOff =
    @"NSTextInsertionPointBlinkPeriodOff";
NSArray<NSString*>* blink_period_keys =
    @[ kInsertionPointBlinkPeriodOn, kInsertionPointBlinkPeriodOff ];

class DefaultsUtilsTest : public CocoaTest {
 public:
  void SetUp() override {
    CocoaTest::SetUp();

    // Capture the (static) flag's value after the first time the function gets
    // called.
    if (!refresh_flag_initial_value_) {
      refresh_flag_initial_value_ = BlinkPeriodRefreshFlagForTesting();
    }

    // Preserve the environment before clearing it out (in case we're running on
    // a personal machine).
    int i = 0;
    for (NSString* next_key in blink_period_keys) {
      orig_blink_period_values_[i++] =
          [NSUserDefaults.standardUserDefaults integerForKey:next_key];
      [NSUserDefaults.standardUserDefaults removeObjectForKey:next_key];
    }

    // Make sure the test's blink period changes get picked up.
    BlinkPeriodRefreshFlagForTesting() = true;
  }

  bool BlinkPeriodRefreshFlagInitialValue() {
    return *refresh_flag_initial_value_;
  }

  bool WillRefreshBlinkPeriod() { return BlinkPeriodRefreshFlagForTesting(); }

  void RefreshBlinkPeriod() { BlinkPeriodRefreshFlagForTesting() = true; }

  // Sets the blink period to `milliseconds`. Removes all values from defaults.
  void SetBlinkPeriod(const int milliseconds) {
    NSUserDefaults* defaults = NSUserDefaults.standardUserDefaults;

    [defaults removeObjectForKey:kInsertionPointBlinkPeriodOn];
    [defaults removeObjectForKey:kInsertionPointBlinkPeriodOff];

    [defaults setInteger:milliseconds forKey:kInsertionPointBlinkPeriodOn];
    [defaults setInteger:milliseconds forKey:kInsertionPointBlinkPeriodOff];
    RefreshBlinkPeriod();
    EXPECT_EQ(base::Milliseconds(milliseconds),
              *TextInsertionCaretBlinkPeriodFromDefaults());

    [defaults removeObjectForKey:kInsertionPointBlinkPeriodOn];
    [defaults removeObjectForKey:kInsertionPointBlinkPeriodOff];
  }

  void TearDown() override {
    int i = 0;
    for (NSString* next_key in blink_period_keys) {
      if (orig_blink_period_values_[i]) {
        [NSUserDefaults.standardUserDefaults
            setInteger:orig_blink_period_values_[i]
                forKey:next_key];
      } else {
        [NSUserDefaults.standardUserDefaults removeObjectForKey:next_key];
      }
      i++;
    }

    CocoaTest::TearDown();
  }

 private:
  std::optional<bool> refresh_flag_initial_value_;
  NSInteger orig_blink_period_values_[2];
};

// Tests that the flag which tells
// TextInsertionCaretBlinkPeriodFromDefaults() to refresh the blink period is
// set to true the first time BlinkPeriodRefreshFlagForTesting() gets called.
TEST_F(DefaultsUtilsTest, RefreshFlagTrueAfterFirstCall) {
  EXPECT_TRUE(BlinkPeriodRefreshFlagInitialValue());
}

// Tests that the flag which tells
// TextInsertionCaretBlinkPeriodFromDefaults() to refresh the blink period is
// set to true whenever the app becomes active.
TEST_F(DefaultsUtilsTest, RefreshFlagResetsOnAppActivate) {
  // Confirm that after retrieving the blink period we aren't set to refresh.
  TextInsertionCaretBlinkPeriodFromDefaults();
  EXPECT_FALSE(WillRefreshBlinkPeriod());

  // Simulate the app becoming active, as if the user switched away and back.
  [NSNotificationCenter.defaultCenter
      postNotificationName:NSApplicationWillBecomeActiveNotification
                    object:nil];

  EXPECT_TRUE(WillRefreshBlinkPeriod());

  TextInsertionCaretBlinkPeriodFromDefaults();
  EXPECT_FALSE(WillRefreshBlinkPeriod());
}

// Tests that we don't return a blink period if there's nothing set in defaults.
TEST_F(DefaultsUtilsTest, InsertionPointBlinkPeriodNoDefaults) {
  EXPECT_FALSE(TextInsertionCaretBlinkPeriodFromDefaults());
}

// Tests returning the blink period from defaults.
TEST_F(DefaultsUtilsTest, InsertionPointBlinkPeriodFromDefaults) {
  [NSUserDefaults.standardUserDefaults setInteger:k750MS
                                           forKey:kInsertionPointBlinkPeriodOn];
  [NSUserDefaults.standardUserDefaults
      setInteger:k750MS
          forKey:kInsertionPointBlinkPeriodOff];

  EXPECT_EQ(base::Milliseconds(k750MS),
            *TextInsertionCaretBlinkPeriodFromDefaults());
}

// Tests returning the blink period when a double is stored in defaults.
TEST_F(DefaultsUtilsTest, InsertionPointBlinkPeriodFromDefaultsDouble) {
  const double k750WithFractionalMS = 750.6;
  [NSUserDefaults.standardUserDefaults setDouble:k750WithFractionalMS
                                          forKey:kInsertionPointBlinkPeriodOn];
  [NSUserDefaults.standardUserDefaults setDouble:k750WithFractionalMS
                                          forKey:kInsertionPointBlinkPeriodOff];

  EXPECT_EQ(base::Milliseconds(k750MS),
            *TextInsertionCaretBlinkPeriodFromDefaults());
}

// Tests returning the blink period derived from just the on time setting in
// defaults.
TEST_F(DefaultsUtilsTest, InsertionPointBlinkPeriodFromOnTime) {
  [NSUserDefaults.standardUserDefaults setInteger:k750MS
                                           forKey:kInsertionPointBlinkPeriodOn];

  EXPECT_EQ(base::Milliseconds((k750MS + 0) / 2),
            *TextInsertionCaretBlinkPeriodFromDefaults());
}

// Tests returning the blink period derived from just the off time setting in
// defaults.
TEST_F(DefaultsUtilsTest, InsertionPointBlinkPeriodFromOffTime) {
  [NSUserDefaults.standardUserDefaults
      setInteger:k250MS
          forKey:kInsertionPointBlinkPeriodOff];

  EXPECT_EQ(base::Milliseconds((0 + k250MS) / 2),
            *TextInsertionCaretBlinkPeriodFromDefaults());
}

// Tests returning the blink period derived from the on and off times in
// defaults.
TEST_F(DefaultsUtilsTest, InsertionPointBlinkPeriodFromOnOffTime) {
  [NSUserDefaults.standardUserDefaults setInteger:k750MS
                                           forKey:kInsertionPointBlinkPeriodOn];
  [NSUserDefaults.standardUserDefaults
      setInteger:k250MS
          forKey:kInsertionPointBlinkPeriodOff];

  EXPECT_EQ(base::Milliseconds((k750MS + k250MS) / 2),
            *TextInsertionCaretBlinkPeriodFromDefaults());
}

// Tests returning "infinite" blink period for a long on time in defaults.
TEST_F(DefaultsUtilsTest, InsertionPointBlinkPeriodFromLongOnTime) {
  [NSUserDefaults.standardUserDefaults setInteger:kTwoHoursInMS
                                           forKey:kInsertionPointBlinkPeriodOn];

  EXPECT_EQ(kInfiniteBlinkTime, *TextInsertionCaretBlinkPeriodFromDefaults());
}

// Tests handling of bad blink period times from defaults.
TEST_F(DefaultsUtilsTest, InsertionPointBlinkPeriodNegativeTimes) {
  const int kNegativeMS = -500;
  NSUserDefaults* defaults = NSUserDefaults.standardUserDefaults;

  // By setting the blink period we cause
  // TextInsertionCaretBlinkPeriodFromDefaults() to evaluate to true so
  // we're sure that setting a negative value causes it to evaluate to false.
  SetBlinkPeriod(k250MS);

  [defaults setInteger:kNegativeMS forKey:kInsertionPointBlinkPeriodOn];
  RefreshBlinkPeriod();
  EXPECT_FALSE(TextInsertionCaretBlinkPeriodFromDefaults());

  SetBlinkPeriod(k250MS);

  [defaults setInteger:k750MS forKey:kInsertionPointBlinkPeriodOn];
  [defaults setInteger:kNegativeMS forKey:kInsertionPointBlinkPeriodOff];
  RefreshBlinkPeriod();
  EXPECT_FALSE(TextInsertionCaretBlinkPeriodFromDefaults());
}

}  // namespace
}  // namespace ui::cocoa
