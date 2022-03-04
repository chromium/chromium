// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "components/services/app_service/public/cpp/permission.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/webui/resources/cr_components/app_management/app_management.mojom.h"
#include "ui/webui/resources/cr_components/app_management/app_management_mojom_traits.h"

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
