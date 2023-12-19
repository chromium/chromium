// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/permission.h"
#include "components/services/app_service/public/cpp/run_on_os_login_types.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/webui/resources/cr_components/app_management/app_management.mojom.h"
#include "ui/webui/resources/cr_components/app_management/app_management_mojom_traits.h"

TEST(AppManagementMojomTraitsTest, RoundTripAppType) {
  static constexpr apps::AppType kTestAppType[] = {
      apps::AppType::kUnknown,   apps::AppType::kArc,
      apps::AppType::kBuiltIn,   apps::AppType::kCrostini,
      apps::AppType::kChromeApp, apps::AppType::kWeb,
      apps::AppType::kPluginVm,  apps::AppType::kStandaloneBrowser,
      apps::AppType::kRemote,    apps::AppType::kBorealis,
      apps::AppType::kSystemWeb, apps::AppType::kStandaloneBrowserChromeApp,
      apps::AppType::kExtension};

  for (auto app_type_in : kTestAppType) {
    apps::AppType app_type_out;

    app_management::mojom::AppType serialized_app_type =
        mojo::EnumTraits<app_management::mojom::AppType,
                         apps::AppType>::ToMojom(app_type_in);
    ASSERT_TRUE((mojo::EnumTraits<app_management::mojom::AppType,
                                  apps::AppType>::FromMojom(serialized_app_type,
                                                            &app_type_out)));
    EXPECT_EQ(app_type_in, app_type_out);
  }
}

TEST(AppManagementMojomTraitsTest, RoundTripPermissions) {
  {
    auto permission = std::make_unique<apps::Permission>(
        apps::PermissionType::kUnknown, /*value=*/true,
        /*is_managed=*/false);
    apps::PermissionPtr output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<app_management::mojom::Permission>(
            permission, output));
    EXPECT_EQ(*permission, *output);
  }
  {
    auto permission = std::make_unique<apps::Permission>(
        apps::PermissionType::kCamera, /*value=*/true,
        /*is_managed=*/true);
    apps::PermissionPtr output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<app_management::mojom::Permission>(
            permission, output));
    EXPECT_EQ(*permission, *output);
  }
  {
    auto permission = std::make_unique<apps::Permission>(
        apps::PermissionType::kLocation, /*value=*/apps::TriState::kAllow,
        /*is_managed=*/false);
    apps::PermissionPtr output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<app_management::mojom::Permission>(
            permission, output));
    EXPECT_EQ(*permission, *output);
  }
  {
    auto permission = std::make_unique<apps::Permission>(
        apps::PermissionType::kMicrophone, /*value=*/apps::TriState::kBlock,
        /*is_managed=*/true);
    apps::PermissionPtr output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<app_management::mojom::Permission>(
            permission, output));
    EXPECT_EQ(*permission, *output);
  }
  {
    auto permission = std::make_unique<apps::Permission>(
        apps::PermissionType::kNotifications, /*value=*/apps::TriState::kAsk,
        /*is_managed=*/false);
    apps::PermissionPtr output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<app_management::mojom::Permission>(
            permission, output));
    EXPECT_EQ(*permission, *output);
  }
  {
    auto permission = std::make_unique<apps::Permission>(
        apps::PermissionType::kContacts, /*value=*/apps::TriState::kAllow,
        /*is_managed=*/true);
    apps::PermissionPtr output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<app_management::mojom::Permission>(
            permission, output));
    EXPECT_EQ(*permission, *output);
  }
  {
    auto permission = std::make_unique<apps::Permission>(
        apps::PermissionType::kStorage, /*value=*/apps::TriState::kBlock,
        /*is_managed=*/false);
    apps::PermissionPtr output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<app_management::mojom::Permission>(
            permission, output));
    EXPECT_EQ(*permission, *output);
  }
  {
    auto permission = std::make_unique<apps::Permission>(
        apps::PermissionType::kPrinting, /*value=*/apps::TriState::kBlock,
        /*is_managed=*/false);
    apps::PermissionPtr output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<app_management::mojom::Permission>(
            permission, output));
    EXPECT_EQ(*permission, *output);
  }
  {
    auto permission = std::make_unique<apps::Permission>(
        apps::PermissionType::kLocation, /*value=*/true,
        /*is_managed=*/false, /*details=*/"While in use");
    apps::PermissionPtr output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<app_management::mojom::Permission>(
            permission, output));
    EXPECT_EQ(*permission, *output);
  }
}

TEST(AppManagementMojomTraitsTest, RoundTripInstallReason) {
  static constexpr apps::InstallReason kTestInstallReason[] = {
      apps::InstallReason::kUnknown, apps::InstallReason::kSystem,
      apps::InstallReason::kPolicy,  apps::InstallReason::kOem,
      apps::InstallReason::kDefault, apps::InstallReason::kSync,
      apps::InstallReason::kUser,    apps::InstallReason::kSubApp,
      apps::InstallReason::kKiosk,   apps::InstallReason::kCommandLine};

  for (auto install_reason_in : kTestInstallReason) {
    apps::InstallReason install_reason_out;

    app_management::mojom::InstallReason serialized_install_reason =
        mojo::EnumTraits<app_management::mojom::InstallReason,
                         apps::InstallReason>::ToMojom(install_reason_in);
    ASSERT_TRUE((mojo::EnumTraits<
                 app_management::mojom::InstallReason,
                 apps::InstallReason>::FromMojom(serialized_install_reason,
                                                 &install_reason_out)));
    EXPECT_EQ(install_reason_in, install_reason_out);
  }
}

TEST(AppManagementMojomTraitsTest, RoundTripInstallSource) {
  static constexpr apps::InstallSource kTestInstallSource[] = {
      apps::InstallSource::kUnknown,        apps::InstallSource::kSystem,
      apps::InstallSource::kSync,           apps::InstallSource::kPlayStore,
      apps::InstallSource::kChromeWebStore, apps::InstallSource::kBrowser};

  for (auto install_source_in : kTestInstallSource) {
    apps::InstallSource install_source_out;

    app_management::mojom::InstallSource serialized_install_source =
        mojo::EnumTraits<app_management::mojom::InstallSource,
                         apps::InstallSource>::ToMojom(install_source_in);
    ASSERT_TRUE((mojo::EnumTraits<
                 app_management::mojom::InstallSource,
                 apps::InstallSource>::FromMojom(serialized_install_source,
                                                 &install_source_out)));
    EXPECT_EQ(install_source_in, install_source_out);
  }
}

TEST(AppManagementMojomTraitsTest, RoundTripWindowMode) {
  static constexpr apps::WindowMode kTestWindowMode[] = {
      apps::WindowMode::kUnknown, apps::WindowMode::kWindow,
      apps::WindowMode::kBrowser, apps::WindowMode::kTabbedWindow};

  for (auto window_mode_in : kTestWindowMode) {
    apps::WindowMode window_mode_out;

    app_management::mojom::WindowMode serialized_window_mode =
        mojo::EnumTraits<app_management::mojom::WindowMode,
                         apps::WindowMode>::ToMojom(window_mode_in);
    ASSERT_TRUE(
        (mojo::EnumTraits<app_management::mojom::WindowMode,
                          apps::WindowMode>::FromMojom(serialized_window_mode,
                                                       &window_mode_out)));
    EXPECT_EQ(window_mode_in, window_mode_out);
  }
}

TEST(AppManagementMojomTraitsTest, RoundTripRunOnOsLogin) {
  {
    auto run_on_os_login =
        std::make_unique<apps::RunOnOsLogin>(apps::RunOnOsLoginMode::kUnknown,
                                             /*is_managed=*/false);
    apps::RunOnOsLoginPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<
                app_management::mojom::RunOnOsLogin>(run_on_os_login, output));
    EXPECT_EQ(*run_on_os_login, *output);
  }
  {
    auto run_on_os_login =
        std::make_unique<apps::RunOnOsLogin>(apps::RunOnOsLoginMode::kNotRun,
                                             /*is_managed=*/true);
    apps::RunOnOsLoginPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<
                app_management::mojom::RunOnOsLogin>(run_on_os_login, output));
    EXPECT_EQ(*run_on_os_login, *output);
  }
  {
    auto run_on_os_login =
        std::make_unique<apps::RunOnOsLogin>(apps::RunOnOsLoginMode::kWindowed,
                                             /*is_managed=*/false);
    apps::RunOnOsLoginPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<
                app_management::mojom::RunOnOsLogin>(run_on_os_login, output));
    EXPECT_EQ(*run_on_os_login, *output);
  }
}
