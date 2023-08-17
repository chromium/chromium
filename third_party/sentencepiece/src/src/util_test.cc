// Copyright 2016 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.!

#include <map>

#include "absl/strings/str_cat.h"
#include "filesystem.h"
#include "testharness.h"
#include "util.h"

namespace sentencepiece {
namespace {
constexpr int kMaxUnicode = 0x10FFFF;
}

TEST(UtilTest, LexicalCastTest) {
  bool b = false;
  EXPECT_TRUE(string_util::lexical_cast<bool>("true", &b));
  EXPECT_TRUE(b);
  EXPECT_TRUE(string_util::lexical_cast<bool>("false", &b));
  EXPECT_FALSE(b);
  EXPECT_FALSE(string_util::lexical_cast<bool>("UNK", &b));

  int32 n = 0;
  EXPECT_TRUE(string_util::lexical_cast<int32>("123", &n));
  EXPECT_EQ(123, n);
  EXPECT_TRUE(string_util::lexical_cast<int32>("-123", &n));
  EXPECT_EQ(-123, n);
  EXPECT_FALSE(string_util::lexical_cast<int32>("UNK", &n));

  double d = 0.0;
  EXPECT_TRUE(string_util::lexical_cast<double>("123.4", &d));
  EXPECT_NEAR(123.4, d, 0.001);
  EXPECT_FALSE(string_util::lexical_cast<double>("UNK", &d));

  std::string s;
  EXPECT_TRUE(string_util::lexical_cast<std::string>("123.4", &s));
  EXPECT_EQ("123.4", s);
}

TEST(UtilTest, Hex) {
  for (char32 a = 0; a < 100000; ++a) {
    const std::string s = string_util::IntToHex<char32>(a);
    CHECK_EQ(a, string_util::HexToInt<char32>(s));
  }

  const int n = 151414;
  CHECK_EQ("24F76", string_util::IntToHex(n));
  CHECK_EQ(n, string_util::HexToInt<int>("24F76"));
}

TEST(UtilTest, StringViewTest) {
  absl::string_view s;
  EXPECT_EQ(0, s.find("", 0));
}

TEST(UtilTest, EncodePODTet) {
  std::string tmp;
  {
    float v = 0.0;
    tmp = string_util::EncodePOD<float>(10.0);
    EXPECT_TRUE(string_util::DecodePOD<float>(tmp, &v));
    EXPECT_EQ(10.0, v);
  }

  {
    double v = 0.0;
    tmp = string_util::EncodePOD<double>(10.0);
    EXPECT_TRUE(string_util::DecodePOD<double>(tmp, &v));
    EXPECT_EQ(10.0, v);
  }

  {
    int32 v = 0;
    tmp = string_util::EncodePOD<int32>(10);
    EXPECT_TRUE(string_util::DecodePOD<int32>(tmp, &v));
    EXPECT_EQ(10, v);
  }

  {
    int16 v = 0;
    tmp = string_util::EncodePOD<int16>(10);
    EXPECT_TRUE(string_util::DecodePOD<int16>(tmp, &v));
    EXPECT_EQ(10, v);
  }

  {
    int64 v = 0;
    tmp = string_util::EncodePOD<int64>(10);
    EXPECT_TRUE(string_util::DecodePOD<int64>(tmp, &v));
    EXPECT_EQ(10, v);
  }

  // Invalid data
  {
    int32 v = 0;
    tmp = string_util::EncodePOD<int64>(10);
    EXPECT_FALSE(string_util::DecodePOD<int32>(tmp, &v));
  }
}

TEST(UtilTest, ItoaTest) {
  auto Itoa = [](int v) {
    char buf[16];
    string_util::Itoa(v, buf);
    return std::string(buf);
  };

  EXPECT_EQ("0", Itoa(0));
  EXPECT_EQ("10", Itoa(10));
  EXPECT_EQ("-10", Itoa(-10));
  EXPECT_EQ("718", Itoa(718));
  EXPECT_EQ("-522", Itoa(-522));
}

TEST(UtilTest, OneCharLenTest) {
  EXPECT_EQ(1, string_util::OneCharLen("abc"));
  EXPECT_EQ(3, string_util::OneCharLen("テスト"));
}

TEST(UtilTest, DecodeUTF8Test) {
  size_t mblen = 0;

  {
    const std::string input = "";
    EXPECT_EQ(0, string_util::DecodeUTF8(input, &mblen));
    EXPECT_EQ(1, mblen);  // mblen always returns >= 1
  }

  {
    EXPECT_EQ(1, string_util::DecodeUTF8("\x01", &mblen));
    EXPECT_EQ(1, mblen);
  }

  {
    EXPECT_EQ(0x7F, string_util::DecodeUTF8("\x7F", &mblen));
    EXPECT_EQ(1, mblen);
  }

  {
    EXPECT_EQ(0x80, string_util::DecodeUTF8("\xC2\x80 ", &mblen));
    EXPECT_EQ(2, mblen);
  }

  {
    EXPECT_EQ(0x7FF, string_util::DecodeUTF8("\xDF\xBF ", &mblen));
    EXPECT_EQ(2, mblen);
  }

  {
    EXPECT_EQ(0x800, string_util::DecodeUTF8("\xE0\xA0\x80 ", &mblen));
    EXPECT_EQ(3, mblen);
  }

  {
    EXPECT_EQ(0x10000, string_util::DecodeUTF8("\xF0\x90\x80\x80 ", &mblen));
    EXPECT_EQ(4, mblen);
  }

  // Invalid UTF8
  {
    EXPECT_EQ(kUnicodeError,
              string_util::DecodeUTF8("\xF7\xBF\xBF\xBF ", &mblen));
    EXPECT_EQ(1, mblen);
  }

  {
    EXPECT_EQ(kUnicodeError,
              string_util::DecodeUTF8("\xF8\x88\x80\x80\x80 ", &mblen));
    EXPECT_EQ(1, mblen);
  }

  {
    EXPECT_EQ(kUnicodeError,
              string_util::DecodeUTF8("\xFC\x84\x80\x80\x80\x80 ", &mblen));
    EXPECT_EQ(1, mblen);
  }

  {
    const char *kInvalidData[] = {
        "\xC2",      // must be 2byte.
        "\xE0\xE0",  // must be 3byte.
        "\xFF",      // BOM
        "\xFE"       // BOM
    };

    for (size_t i = 0; i < 4; ++i) {
      // return values of string_util::DecodeUTF8 is not defined.
      // TODO(taku) implement an workaround.
      EXPECT_EQ(kUnicodeError,
                string_util::DecodeUTF8(
                    kInvalidData[i], kInvalidData[i] + strlen(kInvalidData[i]),
                    &mblen));
      EXPECT_FALSE(string_util::IsStructurallyValid(kInvalidData[i]));
      EXPECT_EQ(1, mblen);
    }
  }

  {
    EXPECT_EQ(kUnicodeError, string_util::DecodeUTF8("\xDF\xDF ", &mblen));
    EXPECT_EQ(1, mblen);
  }

  {
    EXPECT_EQ(kUnicodeError, string_util::DecodeUTF8("\xE0\xE0\xE0 ", &mblen));
    EXPECT_EQ(1, mblen);
  }

  {
    EXPECT_EQ(kUnicodeError,
              string_util::DecodeUTF8("\xF0\xF0\xF0\xFF ", &mblen));
    EXPECT_EQ(1, mblen);
  }
}

TEST(UtilTest, EncodeUTF8Test) {
  char buf[16];
  for (char32 cp = 1; cp <= kMaxUnicode; ++cp) {
    if (!string_util::IsValidCodepoint(cp)) continue;
    const size_t mblen = string_util::EncodeUTF8(cp, buf);
    size_t mblen2;
    const char32 c = string_util::DecodeUTF8(buf, buf + 16, &mblen2);
    EXPECT_EQ(mblen2, mblen);
    EXPECT_EQ(cp, c);
  }

  EXPECT_EQ(1, string_util::EncodeUTF8(0, buf));
  EXPECT_EQ('\0', buf[0]);

  // non UCS4
  size_t mblen;
  EXPECT_EQ(3, string_util::EncodeUTF8(0x7000000, buf));
  EXPECT_EQ(kUnicodeError, string_util::DecodeUTF8(buf, buf + 16, &mblen));
  EXPECT_EQ(3, mblen);

  EXPECT_EQ(3, string_util::EncodeUTF8(0x8000001, buf));
  EXPECT_EQ(kUnicodeError, string_util::DecodeUTF8(buf, buf + 16, &mblen));
  EXPECT_EQ(3, mblen);
}

TEST(UtilTest, UnicodeCharToUTF8Test) {
  for (char32 cp = 1; cp <= kMaxUnicode; ++cp) {
    if (!string_util::IsValidCodepoint(cp)) continue;
    const auto s = string_util::UnicodeCharToUTF8(cp);
    const auto ut = string_util::UTF8ToUnicodeText(s);
    EXPECT_EQ(1, ut.size());
    EXPECT_EQ(cp, ut[0]);
  }
}

TEST(UtilTest, IsStructurallyValidTest) {
  EXPECT_TRUE(string_util::IsStructurallyValid("abcd"));
  EXPECT_TRUE(
      string_util::IsStructurallyValid(absl::string_view("a\0cd", 4)));  // NUL
  EXPECT_TRUE(string_util::IsStructurallyValid("ab\xc3\x81"));        // 2-byte
  EXPECT_TRUE(string_util::IsStructurallyValid("a\xe3\x81\x81"));     // 3-byte
  EXPECT_TRUE(string_util::IsStructurallyValid("\xf2\x82\x81\x84"));  // 4
  EXPECT_FALSE(string_util::IsStructurallyValid("abc\x80"));
  EXPECT_FALSE(string_util::IsStructurallyValid("abc\xc3"));
  EXPECT_FALSE(string_util::IsStructurallyValid("ab\xe3\x81"));
  EXPECT_FALSE(string_util::IsStructurallyValid("a\xf3\x81\x81"));
  EXPECT_FALSE(string_util::IsStructurallyValid("ab\xc0\x82"));
  EXPECT_FALSE(string_util::IsStructurallyValid("a\xe0\x82\x81"));
  EXPECT_FALSE(string_util::IsStructurallyValid("\xf0\x82\x83\x84"));
  EXPECT_FALSE(string_util::IsStructurallyValid("\xf4\xbd\xbe\xbf"));
  EXPECT_FALSE(string_util::IsStructurallyValid("\xED\xA0\x80"));
  EXPECT_FALSE(string_util::IsStructurallyValid("\xED\xBF\xBF"));
  EXPECT_FALSE(string_util::IsStructurallyValid("\xc0\x81"));
  EXPECT_FALSE(string_util::IsStructurallyValid("\xc1\xbf"));
  EXPECT_FALSE(string_util::IsStructurallyValid("\xe0\x81\x82"));
  EXPECT_FALSE(string_util::IsStructurallyValid("\xe0\x9f\xbf"));
  EXPECT_FALSE(string_util::IsStructurallyValid("\xf0\x80\x81\x82"));
  EXPECT_FALSE(string_util::IsStructurallyValid("\xf0\x83\xbe\xbd"));
}

TEST(UtilTest, UnicodeTextToUTF8Test) {
  string_util::UnicodeText ut;

  ut = string_util::UTF8ToUnicodeText("test");
  EXPECT_EQ("test", string_util::UnicodeTextToUTF8(ut));

  ut = string_util::UTF8ToUnicodeText("テスト");
  EXPECT_EQ("テスト", string_util::UnicodeTextToUTF8(ut));

  ut = string_util::UTF8ToUnicodeText("これはtest");
  EXPECT_EQ("これはtest", string_util::UnicodeTextToUTF8(ut));
}

TEST(UtilTest, MapUtilTest) {
  const std::map<std::string, std::string> kMap = {
      {"a", "A"}, {"b", "B"}, {"c", "C"}};

  EXPECT_TRUE(port::ContainsKey(kMap, "a"));
  EXPECT_TRUE(port::ContainsKey(kMap, "b"));
  EXPECT_FALSE(port::ContainsKey(kMap, ""));
  EXPECT_FALSE(port::ContainsKey(kMap, "x"));

  EXPECT_EQ("A", port::FindOrDie(kMap, "a"));
  EXPECT_EQ("B", port::FindOrDie(kMap, "b"));
  EXPECT_DEATH(port::FindOrDie(kMap, "x"), "");

  EXPECT_EQ("A", port::FindWithDefault(kMap, "a", "x"));
  EXPECT_EQ("B", port::FindWithDefault(kMap, "b", "x"));
  EXPECT_EQ("x", port::FindWithDefault(kMap, "d", "x"));

  EXPECT_EQ("A", port::FindOrDie(kMap, "a"));
  EXPECT_DEATH(port::FindOrDie(kMap, "d"), "");
}

TEST(UtilTest, MapUtilVecTest) {
  const std::map<std::vector<int>, std::string> kMap = {{{0, 1}, "A"}};
  EXPECT_DEATH(port::FindOrDie(kMap, {0, 2}), "");
}

TEST(UtilTest, InputOutputBufferTest) {
  const std::vector<std::string> kData = {
      "This"
      "is"
      "a"
      "test"};

  {
    auto output = filesystem::NewWritableFile(
        util::JoinPath(absl::GetFlag(FLAGS_test_tmpdir), "test_file"));
    for (size_t i = 0; i < kData.size(); ++i) {
      output->WriteLine(kData[i]);
    }
  }

  {
    auto input = filesystem::NewReadableFile(
        util::JoinPath(absl::GetFlag(FLAGS_test_tmpdir), "test_file"));
    std::string line;
    for (size_t i = 0; i < kData.size(); ++i) {
      EXPECT_TRUE(input->ReadLine(&line));
      EXPECT_EQ(kData[i], line);
    }
    EXPECT_FALSE(input->ReadLine(&line));
  }
}

TEST(UtilTest, InputOutputBufferInvalidFileTest) {
  auto input = filesystem::NewReadableFile("__UNKNOWN__FILE__");
  EXPECT_FALSE(input->status().ok());
}

TEST(UtilTest, STLDeleteELementsTest) {
  class Item {
   public:
    explicit Item(int *counter) : counter_(counter) {}
    ~Item() { ++*counter_; }

   private:
    int *counter_;
  };

  std::vector<Item *> data;
  int counter = 0;
  for (int i = 0; i < 10; ++i) {
    data.push_back(new Item(&counter));
  }
  port::STLDeleteElements(&data);
  CHECK_EQ(10, counter);
  EXPECT_EQ(0, data.size());
}

TEST(UtilTest, StatusTest) {
  const util::Status ok;
  EXPECT_TRUE(ok.ok());
  EXPECT_EQ(util::StatusCode::kOk, ok.code());
  EXPECT_EQ(std::string(""), ok.message());

  const util::Status s1(util::StatusCode::kUnknown, "unknown");
  const util::Status s2(util::StatusCode::kUnknown, std::string("unknown"));

  EXPECT_EQ(util::StatusCode::kUnknown, s1.code());
  EXPECT_EQ(util::StatusCode::kUnknown, s2.code());
  EXPECT_EQ(std::string("unknown"), s1.message());
  EXPECT_EQ(std::string("unknown"), s2.message());

  auto ok2 = util::OkStatus();
  EXPECT_TRUE(ok2.ok());
  EXPECT_EQ(util::StatusCode::kOk, ok2.code());
  EXPECT_EQ(std::string(""), ok2.message());

  util::OkStatus().IgnoreError();
  for (int i = 1; i <= 16; ++i) {
    util::Status s(static_cast<util::StatusCode>(i), "message");
    EXPECT_TRUE(s.ToString().find("message") != std::string::npos)
        << s.ToString();
  }
}

TEST(UtilTest, JoinPathTest) {
#ifdef OS_WIN
  EXPECT_EQ("foo\\bar\\buz", util::JoinPath("foo", "bar", "buz"));
  EXPECT_EQ("foo\\\\buz", util::JoinPath("foo", "", "buz"));
#else
  EXPECT_EQ("foo/bar/buz", util::JoinPath("foo", "bar", "buz"));
  EXPECT_EQ("foo//buz", util::JoinPath("foo", "", "buz"));
#endif
  EXPECT_EQ("foo", util::JoinPath("foo"));
  EXPECT_EQ("", util::JoinPath(""));
}

TEST(UtilTest, ReservoirSamplerTest) {
  std::vector<int> sampled;
  random::ReservoirSampler<int> sampler(&sampled, 100);
  for (int i = 0; i < 10000; ++i) {
    sampler.Add(i);
  }
  EXPECT_EQ(100, sampled.size());
  EXPECT_EQ(10000, sampler.total_size());
}

TEST(UtilTest, StrSplitAsCSVTest) {
  {
    const auto v = util::StrSplitAsCSV("foo,bar,buz");
    EXPECT_EQ(3, v.size());
    EXPECT_EQ("foo", v[0]);
    EXPECT_EQ("bar", v[1]);
    EXPECT_EQ("buz", v[2]);
  }

  {
    const auto v = util::StrSplitAsCSV("foo,\"\"\"bar\"\"\",buzz");
    EXPECT_EQ(3, v.size());
    EXPECT_EQ("foo", v[0]);
    EXPECT_EQ("\"bar\"", v[1]);
    EXPECT_EQ("buzz", v[2]);
  }

  {
    const auto v = util::StrSplitAsCSV("foo,\"1,2,3,4\"");
    EXPECT_EQ(2, v.size());
    EXPECT_EQ("foo", v[0]);
    EXPECT_EQ("1,2,3,4", v[1]);
  }
}
}  // namespace sentencepiece
