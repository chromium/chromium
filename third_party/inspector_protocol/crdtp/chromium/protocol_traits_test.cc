// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/inspector_protocol/crdtp/chromium/protocol_traits.h"

#include "base/json/json_reader.h"
#include "base/test/values_test_util.h"
#include "third_party/inspector_protocol/crdtp/json.h"
#include "third_party/inspector_protocol/crdtp/test_platform.h"

namespace crdtp {

namespace {

using base::test::IsJson;
using testing::Eq;

template <typename T, typename F>
T ConvertTo(const F& from) {
  std::vector<uint8_t> bytes;
  ProtocolTypeTraits<F>::Serialize(from, &bytes);
  DeserializerState deserializer(std::move(bytes));
  T to;
  bool rc = ProtocolTypeTraits<T>::Deserialize(&deserializer, &to);
  EXPECT_TRUE(rc) << deserializer.ErrorMessage(
      crdtp::MakeSpan("Error deserializing"));
  return to;
}

template <typename T>
T RoundTrip(const T& from) {
  return ConvertTo<T>(from);
}

TEST(ProtocolTraits, String) {
  const std::string kEmpty = "";
  EXPECT_THAT(RoundTrip(kEmpty), Eq(kEmpty));

  const std::string kHelloWorld = "Hello, world!";
  EXPECT_THAT(RoundTrip(kHelloWorld), Eq(kHelloWorld));

  std::string all_values(256, ' ');
  for (size_t i = 0; i < all_values.size(); ++i)
    all_values[i] = i;
  EXPECT_THAT(RoundTrip(all_values), Eq(all_values));
}

std::vector<uint8_t> MakeVector(const Binary& b) {
  return std::vector<uint8_t>(b.data(), b.data() + b.size());
}

TEST(ProtocolTraits, BinaryBasic) {
  const Binary empty;
  EXPECT_THAT(empty.size(), Eq(0UL));

  constexpr uint8_t data[] = {'H', 'e', 'l', 'l', 'o', ',', 0,
                              'w', 'o', 'r', 'l', 'd', '!', 0x80};
  const std::vector<uint8_t> data_vec(std::cbegin(data), std::cend(data));
  Binary binary = Binary::fromSpan(data);
  EXPECT_THAT(binary.toBase64(), Eq("SGVsbG8sAHdvcmxkIYA="));
  EXPECT_THAT(MakeVector(binary), Eq(data_vec));
  EXPECT_THAT(MakeVector(Binary::fromVector(data_vec)), Eq(data_vec));
  EXPECT_THAT(MakeVector(Binary::fromString(std::string(
                  reinterpret_cast<const char*>(data), sizeof data))),
              Eq(data_vec));
  bool success = false;
  EXPECT_THAT(MakeVector(Binary::fromBase64("SGVsbG8sAHdvcmxkIYA=", &success)),
              Eq(data_vec));
  Binary::fromBase64("SGVsbG8sAHdvcmxkIYA", &success);
  EXPECT_FALSE(success);
}

TEST(ProtocolTraits, BinarySerialization) {
  constexpr uint8_t data[] = {'H', 'e', 'l', 'l', 'o', ',', 0,
                              'w', 'o', 'r', 'l', 'd', '!', 0x80};
  Binary binary = Binary::fromSpan(data);

  EXPECT_THAT(MakeVector(RoundTrip(binary)), Eq(MakeVector(binary)));
}

TEST(ProtocolTraits, BinaryInvalidBase64) {
  std::vector<uint8_t> bytes;
  ProtocolTypeTraits<std::string>::Serialize("@#%&", &bytes);
  DeserializerState deserializer(std::move(bytes));
  Binary binary;
  bool rc = ProtocolTypeTraits<Binary>::Deserialize(&deserializer, &binary);
  EXPECT_THAT(rc, Eq(false));
  EXPECT_THAT(deserializer.status().ok(), Eq(false));
  EXPECT_THAT(deserializer.ErrorMessage(MakeSpan("error")),
              Eq("Failed to deserialize error - BINDINGS: invalid base64 "
                 "string at position 0"));
}

TEST(ProtocolTraits, PrimitiveValueSerialization) {
  EXPECT_THAT(RoundTrip(base::Value()), IsJson(base::Value()));

  EXPECT_THAT(ConvertTo<bool>(base::Value(false)), Eq(false));
  EXPECT_THAT(ConvertTo<bool>(base::Value(true)), Eq(true));
  EXPECT_THAT(RoundTrip(base::Value(false)), IsJson(base::Value(false)));
  EXPECT_THAT(RoundTrip(base::Value(true)), IsJson(base::Value(true)));

  EXPECT_THAT(ConvertTo<int>(base::Value(42)), 42);
  EXPECT_THAT(RoundTrip(base::Value(42)), IsJson(base::Value(42)));

  constexpr char kMessage[] = "We apologise for the inconvenience";
  EXPECT_THAT(ConvertTo<std::string>(base::Value(kMessage)), Eq(kMessage));
  EXPECT_THAT(RoundTrip(base::Value(kMessage)), IsJson(base::Value(kMessage)));

  EXPECT_THAT(ConvertTo<double>(base::Value(6.62607015e-34)),
              Eq(6.62607015e-34));
  EXPECT_THAT(RoundTrip(base::Value(6.62607015e-34)),
              IsJson(base::Value(6.62607015e-34)));
}

template <typename... Args>
base::Value::List MakeList(Args&&... args) {
  base::Value::List res;
  (res.Append(std::forward<Args>(args)), ...);
  return res;
}

TEST(ProtocolTraits, ListValueSerialization) {
  EXPECT_THAT(ConvertTo<std::vector<int>>(base::Value(base::Value::List())),
              Eq(std::vector<int>()));
  EXPECT_THAT(RoundTrip(base::Value(base::Value::List())),
              IsJson(base::Value(base::Value::List())));

  base::Value::List list = MakeList(2, 3, 5);
  base::Value list_value = base::Value(list.Clone());
  EXPECT_THAT(ConvertTo<std::vector<int>>(list_value),
              Eq(std::vector<int>{2, 3, 5}));
  EXPECT_THAT(ConvertTo<base::Value>(list), IsJson(list_value));
  EXPECT_THAT(RoundTrip(list_value), IsJson(list_value));

  base::Value list_of_lists_value =
      base::Value(MakeList(list_value.Clone(), MakeList("foo", "bar", "bazz"),
                           MakeList(base::Value())));
  EXPECT_THAT(RoundTrip(list_of_lists_value), IsJson(list_of_lists_value));
}

TEST(ProtocolTraits, DictValueSerialization) {
  base::Value::Dict dict;
  EXPECT_THAT(RoundTrip(base::Value(dict.Clone())),
              IsJson(base::Value(base::Value::Type::DICT)));
  dict.Set("int", 42);
  dict.Set("double", 2.718281828459045);
  dict.Set("string", "foo");
  dict.Set("list", base::Value(MakeList("bar", 42)));
  dict.Set("null", base::Value());
  dict.Set("dict", dict.Clone());
  EXPECT_THAT(ConvertTo<base::Value>(dict.Clone()),
              IsJson(base::Value(dict.Clone())));
  EXPECT_THAT(RoundTrip(base::Value(dict.Clone())),
              IsJson(base::Value(dict.Clone())));
}

TEST(ProtocolTraits, DictValueJSONConversion) {
  base::Value::Dict dict;

  dict.Set("int", 42);
  dict.Set("double", 2.718281828459045);
  dict.Set("string", "foo");
  dict.Set("list", base::Value(MakeList("bar", 42)));
  dict.Set("null", base::Value());
  dict.Set("dict", dict.Clone());

  std::vector<uint8_t> bytes;
  ProtocolTypeTraits<base::Value::Dict>::Serialize(dict, &bytes);

  std::string json;
  json::ConvertCBORToJSON(SpanFrom(bytes), &json);

  auto result = base::JSONReader::ReadDict(json);
  ASSERT_TRUE(result.has_value());
  EXPECT_THAT(*result, base::test::DictionaryHasValues(dict));
}

}  // namespace

}  // namespace crdtp
