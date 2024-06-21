// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ui {

namespace {
constexpr char kExample1Url[] = "https://wwww.example1.com";
constexpr char kExample2Url[] = "https://wwww.example2.com";
}  // namespace

// Tests that cloning DataTransferEndpoint object will clone all of its data
// members.
TEST(DataTransferEndpointTest, Clone) {
  DataTransferEndpoint original1(EndpointType::kClipboardHistory,
                                 {.notify_if_restricted = true});
  DataTransferEndpoint clone1(original1);

  EXPECT_EQ(original1.type(), clone1.type());
  EXPECT_EQ(original1.notify_if_restricted(), clone1.notify_if_restricted());

  DataTransferEndpoint original2(GURL(kExample1Url),
                                 {
                                     .notify_if_restricted = false,
                                     .off_the_record = true,
                                 });
  DataTransferEndpoint clone2(original2);

  EXPECT_EQ(original2.type(), clone2.type());
  EXPECT_EQ(*clone2.GetURL(), *original2.GetURL());
  EXPECT_EQ(original2.off_the_record(), clone2.off_the_record());
  EXPECT_EQ(original2.notify_if_restricted(), clone2.notify_if_restricted());
}

// Tests that two DataTransferEndpoint objects won't be equal unless they have
// the same values for all of their data members.
TEST(DataTransferEndpointTest, Equal) {
  DataTransferEndpoint default_endpoint1(EndpointType::kDefault,
                                         {.notify_if_restricted = true});
  DataTransferEndpoint default_endpoint2(EndpointType::kDefault,
                                         {.notify_if_restricted = false});

  EXPECT_FALSE(default_endpoint1 == default_endpoint2);

  DataTransferEndpoint url_endpoint1(GURL(kExample1Url),
                                     {
                                         .notify_if_restricted = true,
                                         .off_the_record = true,
                                     });
  DataTransferEndpoint url_endpoint2(GURL(kExample1Url),
                                     {
                                         .notify_if_restricted = true,
                                         .off_the_record = true,
                                     });
  DataTransferEndpoint url_endpoint3(GURL(kExample1Url),
                                     {
                                         .notify_if_restricted = true,
                                         .off_the_record = false,
                                     });

  EXPECT_TRUE(url_endpoint1 == url_endpoint2);
  EXPECT_FALSE(url_endpoint1 == url_endpoint3);
}

// Tests DataTransferEndpoint::IsSameOriginWith.
TEST(DataTransferEndpointTest, IsSameURLWith) {
  DataTransferEndpoint default_endpoint(EndpointType::kDefault);
  DataTransferEndpoint url_endpoint1(GURL(kExample1Url),
                                     {
                                         .notify_if_restricted = false,
                                         .off_the_record = true,
                                     });
  DataTransferEndpoint url_endpoint2(GURL(kExample2Url),
                                     {
                                         .notify_if_restricted = true,
                                         .off_the_record = true,
                                     });
  DataTransferEndpoint url_endpoint3(GURL(kExample1Url),
                                     {
                                         .notify_if_restricted = true,
                                         .off_the_record = false,
                                     });

  EXPECT_FALSE(url_endpoint2.IsSameURLWith(default_endpoint));
  EXPECT_FALSE(url_endpoint1.IsSameURLWith(url_endpoint2));
  EXPECT_TRUE(url_endpoint1.IsSameURLWith(url_endpoint3));
}

}  // namespace ui
