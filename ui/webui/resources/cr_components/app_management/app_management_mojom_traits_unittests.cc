// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/permission.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/webui/resources/cr_components/app_management/app_management.mojom.h"
#include "ui/webui/resources/cr_components/app_management/app_management_mojom_traits.h"

TEST(AppManagementMojomTraitsTest, RoundTripAppType) {
  static constexpr apps::AppType kTestAppType[] = {
      apps::AppType::kUnknown,
      apps::AppType::kArc,
      apps::AppType::kBuiltIn,
      apps::AppType::kCrostini,
      apps::AppType::kChromeApp,
      apps::AppType::kWeb,
      apps::AppType::kMacOs,
      apps::AppType::kPluginVm,
      apps::AppType::kStandaloneBrowser,
      apps::AppType::kRemote,
      apps::AppType::kBorealis,
      apps::AppType::kSystemWeb,
      apps::AppType::kStandaloneBrowserChromeApp,
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
        apps::PermissionType::kUnknown,
        std::make_unique<apps::PermissionValue>(true),
        /*is_managed=*/false);
    apps::PermissionPtr output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<app_management::mojom::Permission>(
            permission, output));
    EXPECT_EQ(*permission, *output);
  }
  {
    auto permission = std::make_unique<apps::Permission>(
        apps::PermissionType::kCamera,
        std::make_unique<apps::PermissionValue>(true),
        /*is_managed=*/true);
    apps::PermissionPtr output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<app_management::mojom::Permission>(
            permission, output));
    EXPECT_EQ(*permission, *output);
  }
  {
    auto permission = std::make_unique<apps::Permission>(
        apps::PermissionType::kLocation,
        std::make_unique<apps::PermissionValue>(apps::TriState::kAllow),
        /*is_managed=*/false);
    apps::PermissionPtr output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<app_management::mojom::Permission>(
            permission, output));
    EXPECT_EQ(*permission, *output);
  }
  {
    auto permission = std::make_unique<apps::Permission>(
        apps::PermissionType::kMicrophone,
        std::make_unique<apps::PermissionValue>(apps::TriState::kBlock),
        /*is_managed=*/true);
    apps::PermissionPtr output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<app_management::mojom::Permission>(
            permission, output));
    EXPECT_EQ(*permission, *output);
  }
  {
    auto permission = std::make_unique<apps::Permission>(
        apps::PermissionType::kNotifications,
        std::make_unique<apps::PermissionValue>(apps::TriState::kAsk),
        /*is_managed=*/false);
    apps::PermissionPtr output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<app_management::mojom::Permission>(
            permission, output));
    EXPECT_EQ(*permission, *output);
  }
  {
    auto permission = std::make_unique<apps::Permission>(
        apps::PermissionType::kContacts,
        std::make_unique<apps::PermissionValue>(apps::TriState::kAllow),
        /*is_managed=*/true);
    apps::PermissionPtr output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<app_management::mojom::Permission>(
            permission, output));
    EXPECT_EQ(*permission, *output);
  }
  {
    auto permission = std::make_unique<apps::Permission>(
        apps::PermissionType::kStorage,
        std::make_unique<apps::PermissionValue>(apps::TriState::kBlock),
        /*is_managed=*/false);
    apps::PermissionPtr output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<app_management::mojom::Permission>(
            permission, output));
    EXPECT_EQ(*permission, *output);
  }
  {
    auto permission = std::make_unique<apps::Permission>(
        apps::PermissionType::kPrinting,
        std::make_unique<apps::PermissionValue>(apps::TriState::kBlock),
        /*is_managed=*/false);
    apps::PermissionPtr output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<app_management::mojom::Permission>(
            permission, output));
    EXPECT_EQ(*permission, *output);
  }
}
