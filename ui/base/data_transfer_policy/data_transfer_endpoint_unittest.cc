// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace ui {

namespace {
constexpr char kExampleUrl[] = "https://wwww.example.com";
}  // namespace

// Tests that cloning DataTransferEndpoint object will clone all of its data
// members.
TEST(DataTransferEndpointTest, Clone) {
  DataTransferEndpoint original1(EndpointType::kClipboardHistory,
                                 /*notify_if_restricted=*/true);
  DataTransferEndpoint clone1(original1);

  EXPECT_EQ(original1.type(), clone1.type());
  EXPECT_EQ(original1.notify_if_restricted(), clone1.notify_if_restricted());

  DataTransferEndpoint original2(url::Origin::Create(GURL(kExampleUrl)),
                                 /*notify_if_restricted=*/false);
  DataTransferEndpoint clone2(original2);

  EXPECT_EQ(original2.type(), clone2.type());
  EXPECT_TRUE(clone2.origin()->IsSameOriginWith(*original2.origin()));
  EXPECT_EQ(original2.notify_if_restricted(), clone2.notify_if_restricted());
}

// Tests that two DataTransferEndpoint objects won't be equal unless they have
// the same values for all of their data members.
TEST(DataTransferEndpointTest, Equal) {
  DataTransferEndpoint default_endpoint1(EndpointType::kDefault,
                                         /*notify_if_restricted=*/true);
  DataTransferEndpoint default_endpoint2(EndpointType::kDefault,
                                         /*notify_if_restricted=*/false);

  EXPECT_FALSE(default_endpoint1 == default_endpoint2);

  DataTransferEndpoint url_endpoint1(url::Origin::Create(GURL(kExampleUrl)),
                                     /*notify_if_restricted=*/true);
  DataTransferEndpoint url_endpoint2(url::Origin::Create(GURL(kExampleUrl)),
                                     /*notify_if_restricted=*/true);

  EXPECT_TRUE(url_endpoint1 == url_endpoint2);
}

}  // namespace ui
