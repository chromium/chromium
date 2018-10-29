// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/material_design/material_design_controller_observer.h"
#include "ui/base/ui_base_switches.h"

namespace ui {

using MD = MaterialDesignController;
using MDObserver = MaterialDesignControllerObserver;

namespace {

class TestObserver : public MDObserver {
 public:
  TestObserver() = default;
  ~TestObserver() override = default;

  int touch_ui_changes() const { return touch_ui_changes_; }

 private:
  // MDObserver:
  void OnTouchUiChanged() override { ++touch_ui_changes_; }

  int touch_ui_changes_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

// Test fixture for the MaterialDesignController class.
class MaterialDesignControllerTest : public testing::Test {
 public:
  MaterialDesignControllerTest() = default;
  ~MaterialDesignControllerTest() override = default;

 protected:
  // testing::Test:
  void SetUp() override {
    testing::Test::SetUp();
    MD::Initialize();
  }

  void TearDown() override {
    testing::Test::TearDown();
  }

  void SetCommandLineSwitch(const std::string& value_string) {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kTopChromeTouchUi, value_string);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MaterialDesignControllerTest);
};

class MaterialDesignControllerTestCommandLineTouchUiDisabled
    : public MaterialDesignControllerTest {
 public:
  MaterialDesignControllerTestCommandLineTouchUiDisabled() {
    SetCommandLineSwitch(switches::kTopChromeTouchUiDisabled);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(
      MaterialDesignControllerTestCommandLineTouchUiDisabled);
};

class MaterialDesignControllerTestCommandLineTouchUiEnabled
    : public MaterialDesignControllerTest {
 public:
  MaterialDesignControllerTestCommandLineTouchUiEnabled() {
    SetCommandLineSwitch(switches::kTopChromeTouchUiEnabled);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(
      MaterialDesignControllerTestCommandLineTouchUiEnabled);
};

class MaterialDesignControllerTestCommandLineTouchUiAuto
    : public MaterialDesignControllerTest {
 public:
  MaterialDesignControllerTestCommandLineTouchUiAuto() {
    SetCommandLineSwitch(switches::kTopChromeTouchUiAuto);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MaterialDesignControllerTestCommandLineTouchUiAuto);
};

}  // namespace

// Verifies that non-touch is the default.
TEST_F(MaterialDesignControllerTest, NoCommandLineFlagIsNonTouch) {
  ASSERT_FALSE(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kTopChromeTouchUi));
  EXPECT_FALSE(MD::touch_ui());
}

// Verifies that switches::kTopChromeTouchUiDisabled maps to non-touch (the
// default).
TEST_F(MaterialDesignControllerTestCommandLineTouchUiDisabled,
       CheckApiReturns) {
  EXPECT_FALSE(MD::touch_ui());
}

// Verifies that switches::kTopChromeTouchUiEnabled maps to touch.
TEST_F(MaterialDesignControllerTestCommandLineTouchUiEnabled, CheckApiReturns) {
  EXPECT_TRUE(MD::touch_ui());
}

// Verifies that switches::kTopChromeTouchUiAuto maps to non-touch.
TEST_F(MaterialDesignControllerTestCommandLineTouchUiAuto, CheckApiReturns) {
  EXPECT_FALSE(MD::touch_ui());
}

// Verifies that when the mode is set to non-touch and the tablet mode toggles,
// the touch UI state does not change.
TEST_F(MaterialDesignControllerTestCommandLineTouchUiDisabled,
       TabletOnTouchUiChanged) {
  TestObserver observer;
  ScopedObserver<MD, MDObserver> scoped_observer(&observer);
  scoped_observer.Add(MD::GetInstance());

  MD::OnTabletModeToggled(true);
  EXPECT_FALSE(MD::touch_ui());
  EXPECT_EQ(0, observer.touch_ui_changes());

  MD::OnTabletModeToggled(false);
  EXPECT_FALSE(MD::touch_ui());
  EXPECT_EQ(0, observer.touch_ui_changes());
}

// Verifies that when the mode is set to auto and the tablet mode toggles, the
// touch UI state changes and the observer gets called back.
TEST_F(MaterialDesignControllerTestCommandLineTouchUiAuto,
       TabletOnTouchUiChanged) {
  TestObserver observer;
  ScopedObserver<MD, MDObserver> scoped_observer(&observer);
  scoped_observer.Add(MD::GetInstance());

  MD::OnTabletModeToggled(true);
  EXPECT_TRUE(MD::touch_ui());
  EXPECT_EQ(1, observer.touch_ui_changes());

  MD::OnTabletModeToggled(false);
  EXPECT_FALSE(MD::touch_ui());
  EXPECT_EQ(2, observer.touch_ui_changes());
}

}  // namespace ui
