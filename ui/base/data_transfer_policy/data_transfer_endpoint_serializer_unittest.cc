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

#if BUILDFLAG(IS_CHROMEOS)
constexpr char kExampleJsonNonUrlType[] = R"({"endpoint_type":"crostini"})";
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace

TEST(DataTransferEndpointSerializerTest, DataTransferEndpointToJsonUrl) {
  const DataTransferEndpoint example(
      GURL(kExampleUrl),
      {.notify_if_restricted = true, .off_the_record = true});
  std::string actual = ConvertDataTransferEndpointToJson(example);
  EXPECT_EQ(kExampleJsonUrlType, actual);
}

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

#if BUILDFLAG(IS_CHROMEOS)
TEST(DataTransferEndpointSerializerTest, DataTransferEndpointToJsonNonUrl) {
  const DataTransferEndpoint example(EndpointType::kCrostini,
                                     {.notify_if_restricted = true});
  std::string actual = ConvertDataTransferEndpointToJson(example);

  EXPECT_EQ(kExampleJsonNonUrlType, actual);
}

TEST(DataTransferEndpointSerializerTest, JsonToDataTransferEndpointNonUrl) {
  std::unique_ptr<DataTransferEndpoint> actual =
      ConvertJsonToDataTransferEndpoint(kExampleJsonNonUrlType);

  ASSERT_TRUE(actual);
  EXPECT_EQ(EndpointType::kCrostini, actual->type());
  EXPECT_EQ(nullptr, actual->GetURL());
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace ui
