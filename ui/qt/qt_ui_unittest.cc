// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/qt/qt_ui.h"

#include <memory>

#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/scoped_environment_variable_override.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/color/color_provider_manager.h"
#include "ui/display/types/display_config.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/linux/linux_ui.h"

namespace qt {

class QtUiTest : public testing::Test {
 protected:
  void TearDown() override {
    qt_ui_.reset();
    ui::ColorProviderManager::ResetForTesting();
  }

  // Creates and initializes a QtUi using QT's offscreen platform plugin, which
  // has a single 800x600 screen. Returns false if QT is unavailable.
  bool InitializeQtUi() {
    qt_ui_ = CreateQtUi(nullptr);
    return qt_ui_->Initialize();
  }

  const display::DisplayConfig& display_config() const {
    const ui::LinuxUi& linux_ui = *qt_ui_;
    return linux_ui.display_config();
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  base::ScopedEnvironmentVariableOverride qt_platform_{"QT_QPA_PLATFORM",
                                                       "offscreen"};
  base::ScopedEnvironmentVariableOverride qt_scale_factors_{
      "QT_SCREEN_SCALE_FACTORS", "2"};
  std::unique_ptr<ui::LinuxUiAndTheme> qt_ui_;
};

// Tests that QtUi reports the geometry and scale of each QT screen, in physical
// pixels, so that X11 displays can use per-monitor scale factors.
TEST_F(QtUiTest, DisplayGeometries) {
  if (!InitializeQtUi()) {
    GTEST_SKIP() << "QT is not available";
  }

  const display::DisplayConfig& config = display_config();
  ASSERT_EQ(config.display_geometries.size(), 1u);
  const display::DisplayGeometry& geometry = config.display_geometries[0];
  // QT reports the screen size in DIPs, so the bounds must be scaled back to
  // physical pixels.
  EXPECT_EQ(geometry.bounds_px, gfx::Rect(0, 0, 800, 600));
  // The exact scale also depends on the offscreen platform's logical DPI, so
  // only check that the screen scale factor is applied.
  EXPECT_GT(geometry.scale, 1.0f);
  EXPECT_EQ(geometry.scale, config.primary_scale);
}

// Tests that QT is kept from loading the GTK2 style or platform theme while
// Chrome uses GTK3, since GTK2 and GTK3 symbols collide.
TEST(QtUiGtkCollisionTest, OverridesGtk2StyleAndPlatformTheme) {
  base::ScopedEnvironmentVariableOverride style("QT_STYLE_OVERRIDE", "gtk2");
  base::ScopedEnvironmentVariableOverride theme("QT_QPA_PLATFORMTHEME", "gtk2");
  auto env = base::Environment::Create();
  {
    auto overrides = GetGtkQtCollisionPreventionOverrides(
        /*gtk_loaded=*/true, /*prevent_gtk3=*/false);
    EXPECT_EQ(env->GetVar("QT_STYLE_OVERRIDE"), "Fusion");
    EXPECT_EQ(env->GetVar("QT_QPA_PLATFORMTHEME"), "chromium-fallback");
  }
  EXPECT_EQ(env->GetVar("QT_STYLE_OVERRIDE"), "gtk2");

  // Without GTK in the process, GTK2 can be loaded safely.
  auto overrides = GetGtkQtCollisionPreventionOverrides(
      /*gtk_loaded=*/false, /*prevent_gtk3=*/false);
  EXPECT_EQ(env->GetVar("QT_STYLE_OVERRIDE"), "gtk2");
}

// Tests that a GTK style chosen in the LXQt config, rather than in the
// environment, is also overridden.
TEST(QtUiGtkCollisionTest, OverridesGtk2StyleFromLxqtConfig) {
  base::ScopedTempDir config_dir;
  ASSERT_TRUE(config_dir.CreateUniqueTempDir());
  ASSERT_TRUE(base::CreateDirectory(config_dir.GetPath().Append("lxqt")));
  base::ScopedEnvironmentVariableOverride config_home(
      "XDG_CONFIG_HOME", config_dir.GetPath().value());
  base::ScopedEnvironmentVariableOverride style("QT_STYLE_OVERRIDE");
  base::ScopedEnvironmentVariableOverride theme("QT_QPA_PLATFORMTHEME", "lxqt");
  auto env = base::Environment::Create();

  const base::FilePath lxqt_conf =
      config_dir.GetPath().Append("lxqt/lxqt.conf");
  ASSERT_TRUE(
      base::WriteFile(lxqt_conf, "[General]\nstyle=x\n[Qt]\nstyle=gtk2\n"));
  {
    auto overrides = GetGtkQtCollisionPreventionOverrides(
        /*gtk_loaded=*/true, /*prevent_gtk3=*/false);
    EXPECT_EQ(env->GetVar("QT_STYLE_OVERRIDE"), "Fusion");
    EXPECT_EQ(env->GetVar("QT_QPA_PLATFORMTHEME"), "lxqt");
  }

  // Other styles are left alone.
  ASSERT_TRUE(base::WriteFile(lxqt_conf, "[Qt]\nstyle=Breeze\n"));
  auto overrides = GetGtkQtCollisionPreventionOverrides(
      /*gtk_loaded=*/true, /*prevent_gtk3=*/false);
  EXPECT_FALSE(env->HasVar("QT_STYLE_OVERRIDE"));
}

}  // namespace qt
