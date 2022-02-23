// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/data_transfer_policy/data_transfer_endpoint_serializer.h"

#include "build/chromeos_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace ui {

namespace {

constexpr char kExampleUrl[] = "https://www.google.com";
constexpr char kExampleJsonUrlType[] =
    R"({"endpoint_type":"url","url_origin":"https://www.google.com"})";
constexpr char kExampleJsonUrlTypeNoOrigin[] = R"({"endpoint_type":"url"})";

#if BUILDFLAG(IS_CHROMEOS_ASH)
// TODO(crbug.com/1280545): Enable test when VM DataTransferEndpoint endpoint
// types are built in Lacros.
constexpr char kExampleJsonNonUrlType[] = R"({"endpoint_type":"crostini"})";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

TEST(DataTransferEndpointSerializerTest, DataTransferEndpointToJsonUrl) {
  const DataTransferEndpoint example(url::Origin::Create(GURL(kExampleUrl)),
                                     /*notify_if_restricted=*/true);
  std::string actual = ConvertDataTransferEndpointToJson(example);
  EXPECT_EQ(kExampleJsonUrlType, actual);
}

TEST(DataTransferEndpointSerializerTest, JsonToDataTransferEndpointUrl) {
  DataTransferEndpoint expected(url::Origin::Create(GURL(kExampleUrl)),
                                /*notify_if_restricted=*/true);
  std::unique_ptr<DataTransferEndpoint> actual =
      ConvertJsonToDataTransferEndpoint(kExampleJsonUrlType);

  ASSERT_TRUE(actual);
  EXPECT_EQ(expected.type(), actual->type());
  EXPECT_TRUE(expected.GetOrigin()->IsSameOriginWith(*actual->GetOrigin()));
}

TEST(DataTransferEndpointSerializerTest,
     JsonToDataTransferEndpointUrlTypeNoOrigin) {
  std::unique_ptr<DataTransferEndpoint> actual =
      ConvertJsonToDataTransferEndpoint(kExampleJsonUrlTypeNoOrigin);

  EXPECT_EQ(nullptr, actual);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// TODO(crbug.com/1280545): Enable test when VM DataTransferEndpoint endpoint
// types are built in Lacros.
TEST(DataTransferEndpointSerializerTest, DataTransferEndpointToJsonNonUrl) {
  const DataTransferEndpoint example(EndpointType::kCrostini,
                                     /*notify_if_restricted=*/true);
  std::string actual = ConvertDataTransferEndpointToJson(example);

  EXPECT_EQ(kExampleJsonNonUrlType, actual);
}

TEST(DataTransferEndpointSerializerTest, JsonToDataTransferEndpointNonUrl) {
  std::unique_ptr<DataTransferEndpoint> actual =
      ConvertJsonToDataTransferEndpoint(kExampleJsonNonUrlType);

  ASSERT_TRUE(actual);
  EXPECT_EQ(EndpointType::kCrostini, actual->type());
  EXPECT_EQ(nullptr, actual->GetOrigin());
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace ui
