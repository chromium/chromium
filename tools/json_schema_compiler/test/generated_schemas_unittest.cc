// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/json_schema_compiler/test/generated_schemas.h"

#include "base/json/json_reader.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

// Ensure 'manifest_keys' are excluded from the generated schema.
TEST(GeneratedSchemaTest, ManifestKeysExcluded) {
  using GeneratedSchemas = ::test::api::GeneratedSchemas;
  constexpr char kApiName[] = "simple_api";

  ASSERT_TRUE(GeneratedSchemas::IsGenerated(kApiName));

  // The schema string must be in json format.
  absl::optional<base::Value> json_schema =
      base::JSONReader::Read(GeneratedSchemas::Get(kApiName));
  ASSERT_TRUE(json_schema);
  ASSERT_TRUE(json_schema->is_dict());

  EXPECT_FALSE(json_schema->FindPath("manifest_keys"));
}

}  // namespace
