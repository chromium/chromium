// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/json_schema_compiler/test/objects.h"

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/json/json_writer.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "tools/json_schema_compiler/test/objects_movable.h"
#include "tools/json_schema_compiler/test/objects_movable_json.h"

namespace objects_movable = test::api::objects_movable;

TEST(JsonSchemaCompilerObjectsTest, ObjectParamParamsCreate) {
  {
    base::Value::List strings;
    strings.Append("one");
    strings.Append("two");
    base::Value::Dict info_value;
    info_value.Set("strings", std::move(strings));
    info_value.Set("integer", 5);
    info_value.Set("boolean", true);

    base::Value::List params_value;
    params_value.Append(std::move(info_value));
    std::optional<test::api::objects::ObjectParam::Params> params(
        test::api::objects::ObjectParam::Params::Create(params_value));
    EXPECT_TRUE(params.has_value());
    EXPECT_EQ((size_t) 2, params->info.strings.size());
    EXPECT_EQ("one", params->info.strings[0]);
    EXPECT_EQ("two", params->info.strings[1]);
    EXPECT_EQ(5, params->info.integer);
    EXPECT_TRUE(params->info.boolean);
  }
  {
    base::Value::List strings;
    strings.Append("one");
    strings.Append("two");
    base::Value::Dict info_value;
    info_value.Set("strings", std::move(strings));
    info_value.Set("integer", 5);

    base::Value::List params_value;
    params_value.Append(std::move(info_value));
    std::optional<test::api::objects::ObjectParam::Params> params(
        test::api::objects::ObjectParam::Params::Create(params_value));
    EXPECT_FALSE(params.has_value());
  }
}

TEST(JsonSchemaCompilerObjectsTest, ReturnsObjectResultCreate) {
  test::api::objects::ReturnsObject::Results::Info info;
  info.state = test::api::objects::FirstState::kFoo;
  base::Value::List results =
      test::api::objects::ReturnsObject::Results::Create(info);
  ASSERT_EQ(1u, results.size());

  base::Value::Dict expected;
  expected.Set("state", "foo");
  EXPECT_EQ(expected, results[0]);
}

TEST(JsonSchemaCompilerObjectsTest, OnObjectFiredCreate) {
  test::api::objects::OnObjectFired::SomeObject object;
  object.state = test::api::objects::FirstState::kBar;
  base::Value::List results = test::api::objects::OnObjectFired::Create(object);
  ASSERT_EQ(1u, results.size());

  base::Value::Dict expected;
  expected.Set("state", "bar");
  EXPECT_EQ(expected, results[0]);
}

TEST(JsonSchemaCompilerMovableObjectsTest, MovableObjectsTest) {
  std::vector<objects_movable::MovablePod> pods;
  {
    objects_movable::MovablePod pod;
    pod.foo = objects_movable::Foo::kBar;
    pod.str = "str1";
    pod.num = 42;
    pod.b = true;
    pods.push_back(std::move(pod));
  }
  {
    objects_movable::MovablePod pod;
    pod.foo = objects_movable::Foo::kBaz;
    pod.str = "str2";
    pod.num = 45;
    pod.b = false;
    pods.push_back(std::move(pod));
  }
  objects_movable::MovableParent parent;
  parent.pods = std::move(pods);
  parent.strs.push_back("pstr");
  parent.blob.additional_properties.Set("key", "val");
  parent.choice.as_string = "string";

  objects_movable::MovableParent parent2(std::move(parent));
  ASSERT_EQ(2u, parent2.pods.size());
  EXPECT_EQ(objects_movable::Foo::kBar, parent2.pods[0].foo);
  EXPECT_EQ("str1", parent2.pods[0].str);
  EXPECT_EQ(42, parent2.pods[0].num);
  EXPECT_TRUE(parent2.pods[0].b);
  EXPECT_EQ(objects_movable::Foo::kBaz, parent2.pods[1].foo);
  EXPECT_EQ("str2", parent2.pods[1].str);
  EXPECT_EQ(45, parent2.pods[1].num);
  EXPECT_FALSE(parent2.pods[1].b);
  ASSERT_EQ(1u, parent2.strs.size());
  EXPECT_EQ("pstr", parent2.strs[0]);
  EXPECT_FALSE(parent2.choice.as_movable_pod);
  ASSERT_TRUE(parent2.choice.as_string);
  EXPECT_EQ("string", *parent2.choice.as_string);
  const std::string* blob_string =
      parent2.blob.additional_properties.FindString("key");
  EXPECT_TRUE(blob_string);
  EXPECT_EQ("val", *blob_string);

  {
    objects_movable::MovableParent parent_with_pod_choice;
    objects_movable::MovablePod pod;
    pod.foo = objects_movable::Foo::kBaz;
    pod.str = "str";
    pod.num = 10;
    pod.b = false;
    parent_with_pod_choice.choice.as_movable_pod = std::move(pod);
    parent2 = std::move(parent_with_pod_choice);
  }
  EXPECT_TRUE(parent2.pods.empty());
  EXPECT_TRUE(parent2.strs.empty());
  EXPECT_TRUE(parent2.blob.additional_properties.empty());
  EXPECT_FALSE(parent2.choice.as_string);
  ASSERT_TRUE(parent2.choice.as_movable_pod);
  EXPECT_EQ(objects_movable::Foo::kBaz, parent2.choice.as_movable_pod->foo);
  EXPECT_EQ("str", parent2.choice.as_movable_pod->str);
  EXPECT_EQ(10, parent2.choice.as_movable_pod->num);
  EXPECT_FALSE(parent2.choice.as_movable_pod->b);

  test::api::objects_movable_json::MovableWithAdditional with_additional;
  with_additional.str = "str";
  std::vector<std::string> vals1;
  vals1.push_back("vals1a");
  vals1.push_back("vals1b");
  with_additional.additional_properties["key1"] = vals1;
  std::vector<std::string> vals2;
  vals2.push_back("vals2a");
  vals2.push_back("vals2b");
  with_additional.additional_properties["key2"] = vals2;

  test::api::objects_movable_json::MovableWithAdditional with_additional2(
      std::move(with_additional));
  EXPECT_EQ("str", with_additional2.str);
  EXPECT_EQ(2u, with_additional2.additional_properties.size());
  EXPECT_EQ(vals1, with_additional2.additional_properties["key1"]);
  EXPECT_EQ(vals2, with_additional2.additional_properties["key2"]);
}
