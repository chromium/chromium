// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/json_schema_compiler/test/generated_schemas.h"

#include <optional>

#include "base/json/json_reader.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Ensure 'manifest_keys' are excluded from the generated schema.
TEST(GeneratedSchemaTest, ManifestKeysExcluded) {
  using GeneratedSchemas = ::test::api::GeneratedSchemas;
  constexpr char kApiName[] = "simple_api";

  ASSERT_TRUE(GeneratedSchemas::IsGenerated(kApiName));

  // The schema string must be in json format.
  std::optional<base::Value::Dict> json_schema =
      base::JSONReader::ReadDict(GeneratedSchemas::Get(kApiName));
  ASSERT_TRUE(json_schema);
  EXPECT_FALSE(json_schema->Find("manifest_keys"));
}

}  // namespace
