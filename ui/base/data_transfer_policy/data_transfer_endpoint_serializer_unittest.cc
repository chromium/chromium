// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/data_transfer_policy/data_transfer_endpoint_serializer.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "url/gurl.h"

namespace ui {

namespace {

constexpr char kExampleUrl[] = "https://www.google.com";
constexpr char kExampleJsonUrlType[] =
    "{\"endpoint_type\":\"url\","
    "\"off_the_record\":true,"
    "\"url\":\"https://www.google.com/\"}";
constexpr char kExampleJsonUrlTypeNoUrl[] = R"({"endpoint_type":"url"})";

}  // namespace

TEST(DataTransferEndpointSerializerTest, JsonToDataTransferEndpointUrl) {
  DataTransferEndpoint expected(
      GURL(kExampleUrl),
      {.notify_if_restricted = true, .off_the_record = true});
  std::unique_ptr<DataTransferEndpoint> actual =
      ConvertJsonToDataTransferEndpoint(kExampleJsonUrlType);

  ASSERT_TRUE(actual);
  EXPECT_EQ(expected.type(), actual->type());
  EXPECT_EQ(*expected.GetURL(), *actual->GetURL());
}

TEST(DataTransferEndpointSerializerTest,
     JsonToDataTransferEndpointUrlTypeNoUrl) {
  std::unique_ptr<DataTransferEndpoint> actual =
      ConvertJsonToDataTransferEndpoint(kExampleJsonUrlTypeNoUrl);

  EXPECT_EQ(nullptr, actual);
}

}  // namespace ui
