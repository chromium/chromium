#include "third_party/inspector_protocol/crdtp/test_platform.h"

#include "base/test/values_test_util.h"
#include "third_party/inspector_protocol/crdtp/chromium/protocol_traits.h"

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
  Binary binary = Binary::fromSpan(data, sizeof data);
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
  Binary binary = Binary::fromSpan(data, sizeof data);

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
std::vector<base::Value> MakeList(Args&&... args) {
  std::vector<base::Value> res;
  (res.push_back(base::Value(std::forward<Args>(args))), ...);
  return res;
}

TEST(ProtocolTraits, ListValueSerialization) {
  EXPECT_THAT(
      ConvertTo<std::vector<int>>(base::Value(std::vector<base::Value>())),
      Eq(std::vector<int>()));
  EXPECT_THAT(RoundTrip(base::Value(std::vector<base::Value>())),
              IsJson(base::Value(base::Value::Type::LIST)));

  std::vector<base::Value> list = MakeList(2, 3, 5);
  EXPECT_THAT(ConvertTo<std::vector<int>>(base::Value(list)),
              Eq(std::vector<int>{2, 3, 5}));
  EXPECT_THAT(ConvertTo<base::Value>(list), IsJson(base::Value(list)));
  EXPECT_THAT(RoundTrip(base::Value(list)), IsJson(base::Value(list)));

  std::vector<base::Value> list_of_lists =
      MakeList(list, MakeList("foo", "bar", "bazz"), MakeList(base::Value()));
  EXPECT_THAT(RoundTrip(base::Value(list_of_lists)),
              IsJson(base::Value(list_of_lists)));
}

TEST(ProtocolTraits, DictValueSerialization) {
  base::flat_map<std::string, base::Value> dict;
  EXPECT_THAT(RoundTrip(base::Value(dict)),
              IsJson(base::Value(base::Value::Type::DICTIONARY)));
  dict.insert(std::make_pair("int", base::Value(42)));
  dict.insert(std::make_pair("double", base::Value(2.718281828459045)));
  dict.insert(std::make_pair("string", base::Value("foo")));
  dict.insert(std::make_pair("list", base::Value(MakeList("bar", 42))));
  dict.insert(std::make_pair("null", base::Value()));
  dict.insert(std::make_pair("dict", base::Value(dict)));
  EXPECT_THAT(ConvertTo<base::Value>(dict), IsJson(base::Value(dict)));
  EXPECT_THAT(RoundTrip(base::Value(dict)), IsJson(base::Value(dict)));
}

}  // namespace

}  // namespace crdtp
