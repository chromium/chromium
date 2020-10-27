// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "tools/json_schema_compiler/test/idl_basics.h"
#include "tools/json_schema_compiler/test/idl_object_types.h"
#include "tools/json_schema_compiler/test/idl_properties.h"

using test::api::idl_basics::MyType1;
using test::api::idl_object_types::BarType;
using test::api::idl_object_types::FooType;

namespace Function2 = test::api::idl_basics::Function2;
namespace Function3 = test::api::idl_basics::Function3;
namespace Function4 = test::api::idl_basics::Function4;
namespace Function5 = test::api::idl_basics::Function5;
namespace Function6 = test::api::idl_basics::Function6;
namespace Function7 = test::api::idl_basics::Function7;
namespace Function8 = test::api::idl_basics::Function8;
namespace Function9 = test::api::idl_basics::Function9;
namespace Function10 = test::api::idl_basics::Function10;
namespace Function11 = test::api::idl_basics::Function11;
namespace ObjectFunction1 = test::api::idl_object_types::ObjectFunction1;

TEST(IdlCompiler, Basics) {
  // Test MyType1.
  MyType1 a;
  a.x = 5;
  a.y = std::string("foo");
  std::unique_ptr<base::DictionaryValue> serialized = a.ToValue();
  MyType1 b;
  EXPECT_TRUE(MyType1::Populate(*serialized.get(), &b));
  EXPECT_EQ(a.x, b.x);
  EXPECT_EQ(a.y, b.y);

  // Test Function2, which takes an integer parameter.
  base::ListValue list;
  list.AppendInteger(5);
  std::unique_ptr<Function2::Params> f2_params =
      Function2::Params::Create(list);
  EXPECT_EQ(5, f2_params->x);

  // Test Function3, which takes a MyType1 parameter.
  list.Clear();
  std::unique_ptr<base::DictionaryValue> tmp(new base::DictionaryValue());
  tmp->SetInteger("x", 17);
  tmp->SetString("y", "hello");
  tmp->SetString("z", "zstring");
  tmp->SetString("a", "astring");
  tmp->SetString("b", "bstring");
  tmp->SetString("c", "cstring");
  list.Append(std::move(tmp));
  std::unique_ptr<Function3::Params> f3_params =
      Function3::Params::Create(list);
  EXPECT_EQ(17, f3_params->arg.x);
  EXPECT_EQ("hello", f3_params->arg.y);

  // Test functions that take a callback function as a parameter, with varying
  // callback signatures.
  base::Value f4_results =
      base::Value::FromUniquePtrValue(Function4::Results::Create());
  base::ListValue expected;
  EXPECT_EQ(expected, f4_results);

  base::Value f5_results =
      base::Value::FromUniquePtrValue(Function5::Results::Create(13));
  ASSERT_TRUE(f5_results.is_list());
  ASSERT_EQ(1u, f5_results.GetList().size());
  EXPECT_TRUE(f5_results.GetList()[0].is_int());

  base::Value f6_results =
      base::Value::FromUniquePtrValue(Function6::Results::Create(a));
  ASSERT_TRUE(f6_results.is_list());
  ASSERT_EQ(1u, f6_results.GetList().size());
  MyType1 c;
  EXPECT_TRUE(MyType1::Populate(f6_results.GetList()[0], &c));
  EXPECT_EQ(a.x, c.x);
  EXPECT_EQ(a.y, c.y);
}

TEST(IdlCompiler, OptionalArguments) {
  // Test a function that takes one optional argument, both without and with
  // that argument.
  base::ListValue list;
  std::unique_ptr<Function7::Params> f7_params =
      Function7::Params::Create(list);
  EXPECT_EQ(NULL, f7_params->arg.get());
  list.AppendInteger(7);
  f7_params = Function7::Params::Create(list);
  EXPECT_EQ(7, *(f7_params->arg));

  // Similar to above, but a function with one required and one optional
  // argument.
  list.Clear();
  list.AppendInteger(8);
  std::unique_ptr<Function8::Params> f8_params =
      Function8::Params::Create(list);
  EXPECT_EQ(8, f8_params->arg1);
  EXPECT_EQ(NULL, f8_params->arg2.get());
  list.AppendString("foo");
  f8_params = Function8::Params::Create(list);
  EXPECT_EQ(8, f8_params->arg1);
  EXPECT_EQ("foo", *(f8_params->arg2));

  // Test a function with an optional argument of custom type.
  list.Clear();
  std::unique_ptr<Function9::Params> f9_params =
      Function9::Params::Create(list);
  EXPECT_EQ(NULL, f9_params->arg.get());
  list.Clear();
  std::unique_ptr<base::DictionaryValue> tmp(new base::DictionaryValue());
  tmp->SetInteger("x", 17);
  tmp->SetString("y", "hello");
  tmp->SetString("z", "zstring");
  tmp->SetString("a", "astring");
  tmp->SetString("b", "bstring");
  tmp->SetString("c", "cstring");
  list.Append(std::move(tmp));
  f9_params = Function9::Params::Create(list);
  ASSERT_TRUE(f9_params->arg.get() != NULL);
  MyType1* t1 = f9_params->arg.get();
  EXPECT_EQ(17, t1->x);
  EXPECT_EQ("hello", t1->y);
}

TEST(IdlCompiler, ArrayTypes) {
  // Tests of a function that takes an integer and an array of integers. First
  // use an empty array.
  base::ListValue list;
  list.AppendInteger(33);
  list.Append(std::make_unique<base::ListValue>());
  std::unique_ptr<Function10::Params> f10_params =
      Function10::Params::Create(list);
  ASSERT_TRUE(f10_params != NULL);
  EXPECT_EQ(33, f10_params->x);
  EXPECT_TRUE(f10_params->y.empty());

  // Same function, but this time with 2 values in the array.
  list.Clear();
  list.AppendInteger(33);
  std::unique_ptr<base::ListValue> sublist(new base::ListValue);
  sublist->AppendInteger(34);
  sublist->AppendInteger(35);
  list.Append(std::move(sublist));
  f10_params = Function10::Params::Create(list);
  ASSERT_TRUE(f10_params != NULL);
  EXPECT_EQ(33, f10_params->x);
  ASSERT_EQ(2u, f10_params->y.size());
  EXPECT_EQ(34, f10_params->y[0]);
  EXPECT_EQ(35, f10_params->y[1]);

  // Now test a function which takes an array of a defined type.
  list.Clear();
  MyType1 a;
  MyType1 b;
  a.x = 5;
  b.x = 6;
  a.y = std::string("foo");
  b.y = std::string("bar");
  std::unique_ptr<base::ListValue> sublist2(new base::ListValue);
  sublist2->Append(a.ToValue());
  sublist2->Append(b.ToValue());
  list.Append(std::move(sublist2));
  std::unique_ptr<Function11::Params> f11_params =
      Function11::Params::Create(list);
  ASSERT_TRUE(f11_params != NULL);
  ASSERT_EQ(2u, f11_params->arg.size());
  EXPECT_EQ(5, f11_params->arg[0].x);
  EXPECT_EQ("foo", f11_params->arg[0].y);
  EXPECT_EQ(6, f11_params->arg[1].x);
  EXPECT_EQ("bar", f11_params->arg[1].y);
}

TEST(IdlCompiler, ObjectTypes) {
  // Test the FooType type.
  FooType f1;
  f1.x = 3;
  std::unique_ptr<base::DictionaryValue> serialized_foo = f1.ToValue();
  FooType f2;
  EXPECT_TRUE(FooType::Populate(*serialized_foo.get(), &f2));
  EXPECT_EQ(f1.x, f2.x);

  // Test the BarType type.
  BarType b1;
  b1.x.reset(new base::Value(7));
  std::unique_ptr<base::DictionaryValue> serialized_bar = b1.ToValue();
  BarType b2;
  EXPECT_TRUE(BarType::Populate(*serialized_bar.get(), &b2));
  int tmp_int = 0;
  EXPECT_TRUE(b2.x->GetAsInteger(&tmp_int));
  EXPECT_EQ(7, tmp_int);

  // Test the params to the ObjectFunction1 function.
  std::unique_ptr<base::DictionaryValue> icon_props(
      new base::DictionaryValue());
  icon_props->SetString("hello", "world");
  ObjectFunction1::Params::Icon icon;
  EXPECT_TRUE(ObjectFunction1::Params::Icon::Populate(*(icon_props.get()),
                                                      &icon));
  base::ListValue list;
  list.Append(std::move(icon_props));
  std::unique_ptr<ObjectFunction1::Params> params =
      ObjectFunction1::Params::Create(list);
  ASSERT_TRUE(params.get() != NULL);
  std::string tmp;
  EXPECT_TRUE(params->icon.additional_properties.GetString("hello", &tmp));
  EXPECT_EQ("world", tmp);
}

TEST(IdlCompiler, PropertyValues) {
  EXPECT_EQ(42, test::api::idl_properties::first);
  EXPECT_EQ(42.1, test::api::idl_properties::second);
  EXPECT_STREQ("hello world", test::api::idl_properties::third);
}
