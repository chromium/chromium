// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "url/url_util.h"

#include <stddef.h>

#include <optional>
#include <string_view>

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest-message.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_canon.h"
#include "url/url_canon_stdstring.h"
#include "url/url_features.h"
#include "url/url_test_utils.h"

namespace url {

class URLUtilTest : public testing::Test {
 public:
  URLUtilTest() = default;

  URLUtilTest(const URLUtilTest&) = delete;
  URLUtilTest& operator=(const URLUtilTest&) = delete;

  ~URLUtilTest() override = default;

 private:
  ScopedSchemeRegistryForTests scoped_registry_;
};

TEST_F(URLUtilTest, FindAndCompareScheme) {
  Component found_scheme;

  // Simple case where the scheme is found and matches.
  const char kStr1[] = "http://www.com/";
  EXPECT_TRUE(FindAndCompareScheme(kStr1, static_cast<int>(strlen(kStr1)),
                                   "http", nullptr));
  EXPECT_TRUE(FindAndCompareScheme(
      kStr1, static_cast<int>(strlen(kStr1)), "http", &found_scheme));
  EXPECT_TRUE(found_scheme == Component(0, 4));

  // A case where the scheme is found and doesn't match.
  EXPECT_FALSE(FindAndCompareScheme(
      kStr1, static_cast<int>(strlen(kStr1)), "https", &found_scheme));
  EXPECT_TRUE(found_scheme == Component(0, 4));

  // A case where there is no scheme.
  const char kStr2[] = "httpfoobar";
  EXPECT_FALSE(FindAndCompareScheme(
      kStr2, static_cast<int>(strlen(kStr2)), "http", &found_scheme));
  EXPECT_TRUE(found_scheme == Component());

  // When there is an empty scheme, it should match the empty scheme.
  const char kStr3[] = ":foo.com/";
  EXPECT_TRUE(FindAndCompareScheme(
      kStr3, static_cast<int>(strlen(kStr3)), "", &found_scheme));
  EXPECT_TRUE(found_scheme == Component(0, 0));

  // But when there is no scheme, it should fail.
  EXPECT_FALSE(FindAndCompareScheme("", 0, "", &found_scheme));
  EXPECT_TRUE(found_scheme == Component());

  // When there is a whitespace char in scheme, it should canonicalize the URL
  // before comparison.
  const char whtspc_str[] = " \r\n\tjav\ra\nscri\tpt:alert(1)";
  EXPECT_TRUE(FindAndCompareScheme(whtspc_str,
                                   static_cast<int>(strlen(whtspc_str)),
                                   "javascript", &found_scheme));
  EXPECT_TRUE(found_scheme == Component(1, 10));

  // Control characters should be stripped out on the ends, and kept in the
  // middle.
  const char ctrl_str[] = "\02jav\02scr\03ipt:alert(1)";
  EXPECT_FALSE(FindAndCompareScheme(ctrl_str,
                                    static_cast<int>(strlen(ctrl_str)),
                                    "javascript", &found_scheme));
  EXPECT_TRUE(found_scheme == Component(1, 11));
}

TEST_F(URLUtilTest, IsStandard) {
  const char kHTTPScheme[] = "http";
  EXPECT_TRUE(IsStandard(kHTTPScheme, Component(0, strlen(kHTTPScheme))));

  const char kFooScheme[] = "foo";
  EXPECT_FALSE(IsStandard(kFooScheme, Component(0, strlen(kFooScheme))));
}

TEST_F(URLUtilTest, IsReferrerScheme) {
  const char kHTTPScheme[] = "http";
  EXPECT_TRUE(IsReferrerScheme(kHTTPScheme, Component(0, strlen(kHTTPScheme))));

  const char kFooScheme[] = "foo";
  EXPECT_FALSE(IsReferrerScheme(kFooScheme, Component(0, strlen(kFooScheme))));
}

TEST_F(URLUtilTest, AddReferrerScheme) {
  static const char kFooScheme[] = "foo";
  EXPECT_FALSE(IsReferrerScheme(kFooScheme, Component(0, strlen(kFooScheme))));

  url::ScopedSchemeRegistryForTests scoped_registry;
  AddReferrerScheme(kFooScheme, url::SCHEME_WITH_HOST);
  EXPECT_TRUE(IsReferrerScheme(kFooScheme, Component(0, strlen(kFooScheme))));
}

TEST_F(URLUtilTest, ShutdownCleansUpSchemes) {
  static const char kFooScheme[] = "foo";
  EXPECT_FALSE(IsReferrerScheme(kFooScheme, Component(0, strlen(kFooScheme))));

  {
    url::ScopedSchemeRegistryForTests scoped_registry;
    AddReferrerScheme(kFooScheme, url::SCHEME_WITH_HOST);
    EXPECT_TRUE(IsReferrerScheme(kFooScheme, Component(0, strlen(kFooScheme))));
  }

  EXPECT_FALSE(IsReferrerScheme(kFooScheme, Component(0, strlen(kFooScheme))));
}

TEST_F(URLUtilTest, GetStandardSchemeType) {
  url::SchemeType scheme_type;

  const char kHTTPScheme[] = "http";
  scheme_type = url::SCHEME_WITHOUT_AUTHORITY;
  EXPECT_TRUE(GetStandardSchemeType(kHTTPScheme,
                                    Component(0, strlen(kHTTPScheme)),
                                    &scheme_type));
  EXPECT_EQ(url::SCHEME_WITH_HOST_PORT_AND_USER_INFORMATION, scheme_type);

  const char kFilesystemScheme[] = "filesystem";
  scheme_type = url::SCHEME_WITH_HOST_PORT_AND_USER_INFORMATION;
  EXPECT_TRUE(GetStandardSchemeType(kFilesystemScheme,
                                    Component(0, strlen(kFilesystemScheme)),
                                    &scheme_type));
  EXPECT_EQ(url::SCHEME_WITHOUT_AUTHORITY, scheme_type);

  const char kFooScheme[] = "foo";
  scheme_type = url::SCHEME_WITH_HOST_PORT_AND_USER_INFORMATION;
  EXPECT_FALSE(GetStandardSchemeType(kFooScheme,
                                     Component(0, strlen(kFooScheme)),
                                     &scheme_type));
}

TEST_F(URLUtilTest, GetStandardSchemes) {
  std::vector<std::string> expected = {
      kHttpsScheme, kHttpScheme, kFileScheme,       kFtpScheme,
      kWssScheme,   kWsScheme,   kFileSystemScheme, "foo",
  };
  AddStandardScheme("foo", url::SCHEME_WITHOUT_AUTHORITY);
  EXPECT_EQ(expected, GetStandardSchemes());
}

TEST_F(URLUtilTest, ReplaceComponents) {
  Parsed parsed;
  RawCanonOutputT<char> output;
  Parsed new_parsed;

  // Check that the following calls do not cause crash
  Replacements<char> replacements;
  replacements.SetRef("test", Component(0, 4));
  ReplaceComponents(nullptr, 0, parsed, replacements, nullptr, &output,
                    &new_parsed);
  ReplaceComponents("", 0, parsed, replacements, nullptr, &output, &new_parsed);
  replacements.ClearRef();
  replacements.SetHost("test", Component(0, 4));
  ReplaceComponents(nullptr, 0, parsed, replacements, nullptr, &output,
                    &new_parsed);
  ReplaceComponents("", 0, parsed, replacements, nullptr, &output, &new_parsed);

  replacements.ClearHost();
  ReplaceComponents(nullptr, 0, parsed, replacements, nullptr, &output,
                    &new_parsed);
  ReplaceComponents("", 0, parsed, replacements, nullptr, &output, &new_parsed);
  ReplaceComponents(nullptr, 0, parsed, replacements, nullptr, &output,
                    &new_parsed);
  ReplaceComponents("", 0, parsed, replacements, nullptr, &output, &new_parsed);
}

static std::string CheckReplaceScheme(const char* base_url,
                                      const char* scheme) {
  // Make sure the input is canonicalized.
  RawCanonOutput<32> original;
  Parsed original_parsed;
  Canonicalize(base_url, strlen(base_url), true, nullptr, &original,
               &original_parsed);

  Replacements<char> replacements;
  replacements.SetScheme(scheme, Component(0, strlen(scheme)));

  std::string output_string;
  StdStringCanonOutput output(&output_string);
  Parsed output_parsed;
  ReplaceComponents(original.data(), original.length(), original_parsed,
                    replacements, nullptr, &output, &output_parsed);

  output.Complete();
  return output_string;
}

TEST_F(URLUtilTest, ReplaceScheme) {
  EXPECT_EQ("https://google.com/",
            CheckReplaceScheme("http://google.com/", "https"));
  EXPECT_EQ("file://google.com/",
            CheckReplaceScheme("http://google.com/", "file"));
  EXPECT_EQ("http://home/Build",
            CheckReplaceScheme("file:///Home/Build", "http"));
  EXPECT_EQ("javascript:foo",
            CheckReplaceScheme("about:foo", "javascript"));
  EXPECT_EQ("://google.com/",
            CheckReplaceScheme("http://google.com/", ""));
  EXPECT_EQ("http://google.com/",
            CheckReplaceScheme("about:google.com", "http"));
  EXPECT_EQ("http:", CheckReplaceScheme("", "http"));

#ifdef WIN32
  // Magic Windows drive letter behavior when converting to a file URL.
  EXPECT_EQ("file:///E:/foo/",
            CheckReplaceScheme("http://localhost/e:foo/", "file"));
#endif

  // This will probably change to "about://google.com/" when we fix
  // http://crbug.com/160 which should also be an acceptable result.
  EXPECT_EQ("about://google.com/",
            CheckReplaceScheme("http://google.com/", "about"));

  EXPECT_EQ("http://example.com/%20hello%20#%20world",
            CheckReplaceScheme("myscheme:example.com/ hello # world ", "http"));
}

TEST_F(URLUtilTest, DecodeURLEscapeSequences) {
  struct DecodeCase {
    const char* input;
    const char* output;
  } decode_cases[] = {
      {"hello, world", "hello, world"},
      {"%01%02%03%04%05%06%07%08%09%0a%0B%0C%0D%0e%0f/",
       "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0B\x0C\x0D\x0e\x0f/"},
      {"%10%11%12%13%14%15%16%17%18%19%1a%1B%1C%1D%1e%1f/",
       "\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1B\x1C\x1D\x1e\x1f/"},
      {"%20%21%22%23%24%25%26%27%28%29%2a%2B%2C%2D%2e%2f/",
       " !\"#$%&'()*+,-.//"},
      {"%30%31%32%33%34%35%36%37%38%39%3a%3B%3C%3D%3e%3f/",
       "0123456789:;<=>?/"},
      {"%40%41%42%43%44%45%46%47%48%49%4a%4B%4C%4D%4e%4f/",
       "@ABCDEFGHIJKLMNO/"},
      {"%50%51%52%53%54%55%56%57%58%59%5a%5B%5C%5D%5e%5f/",
       "PQRSTUVWXYZ[\\]^_/"},
      {"%60%61%62%63%64%65%66%67%68%69%6a%6B%6C%6D%6e%6f/",
       "`abcdefghijklmno/"},
      {"%70%71%72%73%74%75%76%77%78%79%7a%7B%7C%7D%7e%7f/",
       "pqrstuvwxyz{|}~\x7f/"},
      {"%e4%bd%a0%e5%a5%bd", "\xe4\xbd\xa0\xe5\xa5\xbd"},
      // U+FFFF (Noncharacter) should not be replaced with U+FFFD (Replacement
      // Character) (http://crbug.com/1416021)
      {"%ef%bf%bf", "\xef\xbf\xbf"},
      // U+FDD0 (Noncharacter)
      {"%ef%b7%90", "\xef\xb7\x90"},
      // U+FFFD (Replacement Character)
      {"%ef%bf%bd", "\xef\xbf\xbd"},
  };

  for (const auto& decode_case : decode_cases) {
    RawCanonOutputT<char16_t> output;
    DecodeURLEscapeSequences(decode_case.input,
                             DecodeURLMode::kUTF8OrIsomorphic, &output);
    EXPECT_EQ(decode_case.output, base::UTF16ToUTF8(std::u16string(
                                      output.data(), output.length())));

    RawCanonOutputT<char16_t> output_utf8;
    DecodeURLEscapeSequences(decode_case.input, DecodeURLMode::kUTF8,
                             &output_utf8);
    EXPECT_EQ(decode_case.output,
              base::UTF16ToUTF8(
                  std::u16string(output_utf8.data(), output_utf8.length())));
  }

  // Our decode should decode %00
  const char zero_input[] = "%00";
  RawCanonOutputT<char16_t> zero_output;
  DecodeURLEscapeSequences(zero_input, DecodeURLMode::kUTF8, &zero_output);
  EXPECT_NE("%00", base::UTF16ToUTF8(std::u16string(zero_output.data(),
                                                    zero_output.length())));

  // Test the error behavior for invalid UTF-8.
  struct Utf8DecodeCase {
    const char* input;
    std::vector<char16_t> expected_iso;
    std::vector<char16_t> expected_utf8;
  } utf8_decode_cases[] = {
      // %e5%a5%bd is a valid UTF-8 sequence. U+597D
      {"%e4%a0%e5%a5%bd",
       {0x00e4, 0x00a0, 0x00e5, 0x00a5, 0x00bd, 0},
       {0xfffd, 0x597d, 0}},
      {"%e5%a5%bd%e4%a0",
       {0x00e5, 0x00a5, 0x00bd, 0x00e4, 0x00a0, 0},
       {0x597d, 0xfffd, 0}},
      {"%e4%a0%e5%bd",
       {0x00e4, 0x00a0, 0x00e5, 0x00bd, 0},
       {0xfffd, 0xfffd, 0}},
  };

  for (const auto& utf8_decode_case : utf8_decode_cases) {
    RawCanonOutputT<char16_t> output_iso;
    DecodeURLEscapeSequences(utf8_decode_case.input,
                             DecodeURLMode::kUTF8OrIsomorphic, &output_iso);
    EXPECT_EQ(std::u16string(utf8_decode_case.expected_iso.data()),
              std::u16string(output_iso.data(), output_iso.length()));

    RawCanonOutputT<char16_t> output_utf8;
    DecodeURLEscapeSequences(utf8_decode_case.input, DecodeURLMode::kUTF8,
                             &output_utf8);
    EXPECT_EQ(std::u16string(utf8_decode_case.expected_utf8.data()),
              std::u16string(output_utf8.data(), output_utf8.length()));
  }
}

TEST_F(URLUtilTest, TestEncodeURIComponent) {
  struct EncodeCase {
    const char* input;
    const char* output;
  } encode_cases[] = {
    {"hello, world", "hello%2C%20world"},
    {"\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F",
     "%01%02%03%04%05%06%07%08%09%0A%0B%0C%0D%0E%0F"},
    {"\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F",
     "%10%11%12%13%14%15%16%17%18%19%1A%1B%1C%1D%1E%1F"},
    {" !\"#$%&'()*+,-./",
     "%20!%22%23%24%25%26%27()*%2B%2C-.%2F"},
    {"0123456789:;<=>?",
     "0123456789%3A%3B%3C%3D%3E%3F"},
    {"@ABCDEFGHIJKLMNO",
     "%40ABCDEFGHIJKLMNO"},
    {"PQRSTUVWXYZ[\\]^_",
     "PQRSTUVWXYZ%5B%5C%5D%5E_"},
    {"`abcdefghijklmno",
     "%60abcdefghijklmno"},
    {"pqrstuvwxyz{|}~\x7f",
     "pqrstuvwxyz%7B%7C%7D~%7F"},
  };

  for (const auto& encode_case : encode_cases) {
    RawCanonOutputT<char> buffer;
    EncodeURIComponent(encode_case.input, &buffer);
    std::string output(buffer.data(), buffer.length());
    EXPECT_EQ(encode_case.output, output);
  }
}

TEST_F(URLUtilTest, PotentiallyDanglingMarkup) {
  struct ResolveRelativeCase {
    const char* base;
    const char* rel;
    bool potentially_dangling_markup;
    const char* out;
  } cases[] = {
      {"https://example.com/", "/path<", false, "https://example.com/path%3C"},
      {"https://example.com/", "\n/path<", true, "https://example.com/path%3C"},
      {"https://example.com/", "\r/path<", true, "https://example.com/path%3C"},
      {"https://example.com/", "\t/path<", true, "https://example.com/path%3C"},
      {"https://example.com/", "/pa\nth<", true, "https://example.com/path%3C"},
      {"https://example.com/", "/pa\rth<", true, "https://example.com/path%3C"},
      {"https://example.com/", "/pa\tth<", true, "https://example.com/path%3C"},
      {"https://example.com/", "/path\n<", true, "https://example.com/path%3C"},
      {"https://example.com/", "/path\r<", true, "https://example.com/path%3C"},
      {"https://example.com/", "/path\r<", true, "https://example.com/path%3C"},
      {"https://example.com/", "\n/<path", true, "https://example.com/%3Cpath"},
      {"https://example.com/", "\r/<path", true, "https://example.com/%3Cpath"},
      {"https://example.com/", "\t/<path", true, "https://example.com/%3Cpath"},
      {"https://example.com/", "/<pa\nth", true, "https://example.com/%3Cpath"},
      {"https://example.com/", "/<pa\rth", true, "https://example.com/%3Cpath"},
      {"https://example.com/", "/<pa\tth", true, "https://example.com/%3Cpath"},
      {"https://example.com/", "/<path\n", true, "https://example.com/%3Cpath"},
      {"https://example.com/", "/<path\r", true, "https://example.com/%3Cpath"},
      {"https://example.com/", "/<path\r", true, "https://example.com/%3Cpath"},
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(::testing::Message() << test.base << ", " << test.rel);
    Parsed base_parsed = ParseStandardURL(test.base);

    std::string resolved;
    StdStringCanonOutput output(&resolved);
    Parsed resolved_parsed;
    bool valid =
        ResolveRelative(test.base, strlen(test.base), base_parsed, test.rel,
                        strlen(test.rel), nullptr, &output, &resolved_parsed);
    ASSERT_TRUE(valid);
    output.Complete();

    EXPECT_EQ(test.potentially_dangling_markup,
              resolved_parsed.potentially_dangling_markup);
    EXPECT_EQ(test.out, resolved);
  }
}

TEST_F(URLUtilTest, PotentiallyDanglingMarkupAfterReplacement) {
  // Parse a URL with potentially dangling markup.
  Parsed original_parsed;
  RawCanonOutput<32> original;
  const char* url = "htt\nps://example.com/<path";
  Canonicalize(url, strlen(url), false, nullptr, &original, &original_parsed);
  ASSERT_TRUE(original_parsed.potentially_dangling_markup);

  // Perform a replacement, and validate that the potentially_dangling_markup
  // flag carried over to the new Parsed object.
  Replacements<char> replacements;
  replacements.ClearRef();
  Parsed replaced_parsed;
  RawCanonOutput<32> replaced;
  ReplaceComponents(original.data(), original.length(), original_parsed,
                    replacements, nullptr, &replaced, &replaced_parsed);
  EXPECT_TRUE(replaced_parsed.potentially_dangling_markup);
}

TEST_F(URLUtilTest, PotentiallyDanglingMarkupAfterSchemeOnlyReplacement) {
  // Parse a URL with potentially dangling markup.
  Parsed original_parsed;
  RawCanonOutput<32> original;
  const char* url = "http://example.com/\n/<path";
  Canonicalize(url, strlen(url), false, nullptr, &original, &original_parsed);
  ASSERT_TRUE(original_parsed.potentially_dangling_markup);

  // Perform a replacement, and validate that the potentially_dangling_markup
  // flag carried over to the new Parsed object.
  Replacements<char> replacements;
  const char* new_scheme = "https";
  replacements.SetScheme(new_scheme, Component(0, strlen(new_scheme)));
  Parsed replaced_parsed;
  RawCanonOutput<32> replaced;
  ReplaceComponents(original.data(), original.length(), original_parsed,
                    replacements, nullptr, &replaced, &replaced_parsed);
  EXPECT_TRUE(replaced_parsed.potentially_dangling_markup);
}

TEST_F(URLUtilTest, TestDomainIs) {
  const struct {
    const char* canonicalized_host;
    const char* lower_ascii_domain;
    bool expected_domain_is;
  } kTestCases[] = {
      {"google.com", "google.com", true},
      {"www.google.com", "google.com", true},      // Subdomain is ignored.
      {"www.google.com.cn", "google.com", false},  // Different TLD.
      {"www.google.comm", "google.com", false},
      {"www.iamnotgoogle.com", "google.com", false},  // Different hostname.
      {"www.google.com", "Google.com", false},  // The input is not lower-cased.

      // If the host ends with a dot, it matches domains with or without a dot.
      {"www.google.com.", "google.com", true},
      {"www.google.com.", "google.com.", true},
      {"www.google.com.", ".com", true},
      {"www.google.com.", ".com.", true},

      // But, if the host doesn't end with a dot and the input domain does, then
      // it's considered to not match.
      {"www.google.com", "google.com.", false},

      // If the host ends with two dots, it doesn't match.
      {"www.google.com..", "google.com", false},

      // Empty parameters.
      {"www.google.com", "", false},
      {"", "www.google.com", false},
      {"", "", false},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message() << "(host, domain): ("
                                    << test_case.canonicalized_host << ", "
                                    << test_case.lower_ascii_domain << ")");

    EXPECT_EQ(
        test_case.expected_domain_is,
        DomainIs(test_case.canonicalized_host, test_case.lower_ascii_domain));
  }
}

namespace {
std::optional<std::string> CanonicalizeSpec(std::string_view spec,
                                            bool trim_path_end) {
  std::string canonicalized;
  StdStringCanonOutput output(&canonicalized);
  Parsed parsed;
  if (!Canonicalize(spec.data(), spec.size(), trim_path_end,
                    /*charset_converter=*/nullptr, &output, &parsed)) {
    return {};
  }
  output.Complete();  // Must be called before string is used.
  return canonicalized;
}
}  // namespace

#if BUILDFLAG(IS_WIN)
// Regression test for https://crbug.com/1252658.
TEST_F(URLUtilTest, TestCanonicalizeWindowsPathWithLeadingNUL) {
  auto PrefixWithNUL = [](std::string&& s) -> std::string { return '\0' + s; };
  EXPECT_EQ(CanonicalizeSpec(PrefixWithNUL("w:"), /*trim_path_end=*/false),
            std::make_optional("file:///W:"));
  EXPECT_EQ(CanonicalizeSpec(PrefixWithNUL("\\\\server\\share"),
                             /*trim_path_end=*/false),
            std::make_optional("file://server/share"));
}
#endif

TEST_F(URLUtilTest, TestCanonicalizeIdempotencyWithLeadingControlCharacters) {
  std::string spec = "_w:";
  // Loop over all C0 control characters and the space character.
  for (char c = '\0'; c <= ' '; c++) {
    SCOPED_TRACE(testing::Message() << "c: " << c);

    // Overwrite the first character of `spec`. Note that replacing the first
    // character with NUL will not change the length!
    spec[0] = c;

    for (bool trim_path_end : {false, true}) {
      SCOPED_TRACE(testing::Message() << "trim_path_end: " << trim_path_end);

      std::optional<std::string> canonicalized =
          CanonicalizeSpec(spec, trim_path_end);
      ASSERT_TRUE(canonicalized);
      EXPECT_EQ(canonicalized, CanonicalizeSpec(*canonicalized, trim_path_end));
    }
  }
}

TEST_F(URLUtilTest, TestHasInvalidURLEscapeSequences) {
  struct TestCase {
    const char* input;
    bool is_invalid;
  } cases[] = {
      // Edge cases.
      {"", false},
      {"%", true},

      // Single regular chars with no escaping are valid.
      {"a", false},
      {"g", false},
      {"A", false},
      {"G", false},
      {":", false},
      {"]", false},
      {"\x00", false},      // ASCII 'NUL' char
      {"\x01", false},      // ASCII 'SOH' char
      {"\xC2\xA3", false},  // UTF-8 encoded '£'.

      // Longer strings without escaping are valid.
      {"Hello world", false},
      {"Here: [%25] <-- a percent-encoded percent character.", false},

      // Valid %-escaped sequences ('%' followed by two hex digits).
      {"%00", false},
      {"%20", false},
      {"%02", false},
      {"%ff", false},
      {"%FF", false},
      {"%0a", false},
      {"%0A", false},
      {"abc%FF", false},
      {"%FFabc", false},
      {"abc%FFabc", false},
      {"hello %FF world", false},
      {"%20hello%20world", false},
      {"%25", false},
      {"%25%25", false},
      {"%250", false},
      {"%259", false},
      {"%25A", false},
      {"%25F", false},
      {"%0a:", false},

      // '%' followed by a single character is never a valid sequence.
      {"%%", true},
      {"%2", true},
      {"%a", true},
      {"%A", true},
      {"%g", true},
      {"%G", true},
      {"%:", true},
      {"%[", true},
      {"%F", true},
      {"%\xC2\xA3", true},  //% followed by UTF-8 encoded '£'.

      // String ends on a potential escape sequence but without two hex-digits
      // is invalid.
      {"abc%", true},
      {"abc%%", true},
      {"abc%%%", true},
      {"abc%a", true},

      // One hex and one non-hex digit is invalid.
      {"%a:", true},
      {"%:a", true},
      {"%::", true},
      {"%ag", true},
      {"%ga", true},
      {"%-1", true},
      {"%1-", true},
      {"%0\xC2\xA3", true},  // %0£.
  };

  for (TestCase test_case : cases) {
    const char* input = test_case.input;
    bool result = HasInvalidURLEscapeSequences(input);
    EXPECT_EQ(test_case.is_invalid, result)
        << "Invalid result for '" << input << "'";
  }
}

class URLUtilTypedTest : public ::testing::TestWithParam<bool> {
 public:
  URLUtilTypedTest()
      : use_standard_compliant_non_special_scheme_url_parsing_(GetParam()) {
    if (use_standard_compliant_non_special_scheme_url_parsing_) {
      scoped_feature_list_.InitAndEnableFeature(
          kStandardCompliantNonSpecialSchemeURLParsing);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          kStandardCompliantNonSpecialSchemeURLParsing);
    }
  }

 protected:
  struct URLCase {
    const std::string_view input;
    const std::string_view expected;
    bool expected_success;
  };

  struct ResolveRelativeCase {
    const std::string_view base;
    const std::string_view rel;
    std::optional<std::string_view> expected;
  };

  void TestCanonicalize(const URLCase& url_case) {
    std::string canonicalized;
    StdStringCanonOutput output(&canonicalized);
    Parsed parsed;
    bool success =
        Canonicalize(url_case.input.data(), url_case.input.size(),
                     /*trim_path_end=*/false,
                     /*charset_converter=*/nullptr, &output, &parsed);
    output.Complete();
    EXPECT_EQ(success, url_case.expected_success);
    EXPECT_EQ(output.view(), url_case.expected);
  }

  void TestResolveRelative(const ResolveRelativeCase& test) {
    SCOPED_TRACE(testing::Message()
                 << "base: " << test.base << ", rel: " << test.rel);

    Parsed base_parsed =
        url::IsUsingStandardCompliantNonSpecialSchemeURLParsing()
            ? ParseNonSpecialURL(test.base)
            : ParsePathURL(test.base, /*trim_path_end=*/true);

    std::string resolved;
    StdStringCanonOutput output(&resolved);

    Parsed resolved_parsed;
    bool valid = ResolveRelative(test.base.data(), test.base.size(),
                                 base_parsed, test.rel.data(), test.rel.size(),
                                 nullptr, &output, &resolved_parsed);
    output.Complete();

    if (valid) {
      ASSERT_TRUE(test.expected);
      EXPECT_EQ(resolved, *test.expected);
    } else {
      EXPECT_FALSE(test.expected);
    }
  }

  bool use_standard_compliant_non_special_scheme_url_parsing_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(URLUtilTypedTest, TestResolveRelativeWithNonStandardBase) {
  // This tests non-standard (in the sense that IsStandard() == false)
  // hierarchical schemes.
  struct ResolveRelativeCase {
    const char* base;
    const char* rel;
    bool is_valid;
    const char* out;
    // Optional expected output when the feature is enabled.
    // If the result doesn't change, you can omit this field.
    const char* out_when_non_special_url_feature_is_enabled;
  } resolve_non_standard_cases[] = {
      // Resolving a relative path against a non-hierarchical URL should fail.
      {"scheme:opaque_data", "/path", false, ""},
      // Resolving a relative path against a non-standard authority-based base
      // URL doesn't alter the authority section.
      {"scheme://Authority/", "../path", true, "scheme://Authority/path"},
      // A non-standard hierarchical base is resolved with path URL
      // canonicalization rules.
      {"data:/Blah:Blah/", "file.html", true, "data:/Blah:Blah/file.html"},
      {"data:/Path/../part/part2", "file.html", true,
       "data:/Path/../part/file.html"},
      {"data://text/html,payload", "//user:pass@host:33////payload22", true,
       "data://user:pass@host:33////payload22"},
      // Path URL canonicalization rules also apply to non-standard authority-
      // based URLs.
      {"custom://Authority/", "file.html", true,
       "custom://Authority/file.html"},
      {"custom://Authority/", "other://Auth/", true, "other://Auth/"},
      {"custom://Authority/", "../../file.html", true,
       "custom://Authority/file.html"},
      {"custom://Authority/path/", "file.html", true,
       "custom://Authority/path/file.html"},
      {"custom://Authority:NoCanon/path/", "file.html", true,
       "custom://Authority:NoCanon/path/file.html"},
      // A path with an authority section gets canonicalized under standard URL
      // rules, even though the base was non-standard.
      {"content://content.Provider/", "//other.Provider", true,
       "content://other.provider/",
       // With the feature enabled:
       // - Host case sensitivity should be preserved.
       // - Trailing slash after a host is no longer necessary.
       "content://other.Provider"},
      // Resolving an absolute URL doesn't cause canonicalization of the
      // result.
      {"about:blank", "custom://Authority", true, "custom://Authority"},
      // Fragment URLs can be resolved against a non-standard base.
      {"scheme://Authority/path", "#fragment", true,
       "scheme://Authority/path#fragment"},
      {"scheme://Authority/", "#fragment", true,
       "scheme://Authority/#fragment"},
      // Test resolving a fragment (only) against any kind of base-URL.
      {"about:blank", "#id42", true, "about:blank#id42"},
      {"about:blank", " #id42", true, "about:blank#id42"},
      {"about:blank#oldfrag", "#newfrag", true, "about:blank#newfrag"},
      {"about:blank", " #id:42", true, "about:blank#id:42"},
      // A surprising side effect of allowing fragments to resolve against
      // any URL scheme is we might break javascript: URLs by doing so...
      {"javascript:alert('foo#bar')", "#badfrag", true,
       "javascript:alert('foo#badfrag"},
  };

  for (const auto& test : resolve_non_standard_cases) {
    SCOPED_TRACE(testing::Message()
                 << "base: " << test.base << ", rel: " << test.rel);

    Parsed base_parsed = use_standard_compliant_non_special_scheme_url_parsing_
                             ? ParseNonSpecialURL(test.base)
                             : ParsePathURL(test.base, /*trim_path_end=*/true);

    std::string resolved;
    StdStringCanonOutput output(&resolved);
    Parsed resolved_parsed;
    bool valid =
        ResolveRelative(test.base, strlen(test.base), base_parsed, test.rel,
                        strlen(test.rel), nullptr, &output, &resolved_parsed);
    output.Complete();

    EXPECT_EQ(test.is_valid, valid);
    if (test.is_valid && valid) {
      if (use_standard_compliant_non_special_scheme_url_parsing_ &&
          test.out_when_non_special_url_feature_is_enabled) {
        EXPECT_EQ(test.out_when_non_special_url_feature_is_enabled, resolved);
      } else {
        EXPECT_EQ(test.out, resolved);
      }
    }
  }
}

TEST_P(URLUtilTypedTest, TestNoRefComponent) {
  // This test was originally written before full support for non-special URLs
  // became available. We need a flag-dependent test here because the test uses
  // an internal parse function. See http://crbug.com/40063064 for details.
  //
  // The test case corresponds to the following user scenario:
  //
  // > const url = new URL("any#body", "mailto://to/");
  // > assertEquals(url.href, "mailto://to/any#body");
  //
  // TODO(crbug.com/40063064): Remove this test once the flag is enabled.
  const std::string_view base = "mailto://to/";
  const std::string_view rel = "any#body";
  if (use_standard_compliant_non_special_scheme_url_parsing_) {
    // We probably don't need to test with the flag enabled, however, including
    // a test with the flag enabled would be beneficial for comparison purposes,
    // at least until we enable the flag by default.
    Parsed base_parsed = ParseNonSpecialURL(base);

    std::string resolved;
    StdStringCanonOutput output(&resolved);
    Parsed resolved_parsed;

    bool valid =
        ResolveRelative(base.data(), base.size(), base_parsed, rel.data(),
                        rel.size(), nullptr, &output, &resolved_parsed);
    EXPECT_TRUE(valid);
    // Note: If the flag is enabled and the correct parsing function is used,
    // resolved_parsed.ref becomes valid correctly.
    EXPECT_TRUE(resolved_parsed.ref.is_valid());
    output.Complete();
    EXPECT_EQ(resolved, "mailto://to/any#body");
  } else {
    // Note: See the description of https://codereview.chromium.org/767713002/
    // for the intention of this test, which added this test to record a wrong
    // result if a wrong parser function is used. I kept the following original
    // comment as is:
    //
    // The hash-mark must be ignored when mailto: scheme is parsed,
    // even if the URL has a base and relative part.
    std::string resolved;
    StdStringCanonOutput output(&resolved);
    Parsed resolved_parsed;

    bool valid = ResolveRelative(
        base.data(), base.size(), ParsePathURL(base, false), rel.data(),
        rel.size(), nullptr, &output, &resolved_parsed);
    EXPECT_TRUE(valid);
    EXPECT_FALSE(resolved_parsed.ref.is_valid());
  }
}

TEST_P(URLUtilTypedTest, Cannolicalize) {
  // Verify that the feature flag changes canonicalization behavior,
  // focusing on key cases here as comprehesive testing is covered in other unit
  // tests.
  if (use_standard_compliant_non_special_scheme_url_parsing_) {
    URLCase cases[] = {
        {"git://host/..", "git://host/", true},
        {"git:// /", "git:///", false},
        {"git:/..", "git:/", true},
        {"mailto:/..", "mailto:/", true},
    };
    for (const auto& i : cases) {
      TestCanonicalize(i);
    }
  } else {
    // Every non-special URL is considered as an opaque path if the feature is
    // disabled.
    URLCase cases[] = {
        {"git://host/..", "git://host/..", true},
        {"git:// /", "git:// /", true},
        {"git:/..", "git:/..", true},
        {"mailto:/..", "mailto:/..", true},
    };
    for (const auto& i : cases) {
      TestCanonicalize(i);
    }
  }
}

TEST_P(URLUtilTypedTest, TestResolveRelativeWithNonSpecialBase) {
  // Test flag-dependent behaviors. Existing tests in
  // URLUtilTest::TestResolveRelativeWithNonStandardBase cover common cases.
  //
  // TODO(crbug.com/40063064): Test common cases in this typed test too.
  if (use_standard_compliant_non_special_scheme_url_parsing_) {
    ResolveRelativeCase cases[] = {
        {"scheme://Authority", "path", "scheme://Authority/path"},
    };
    for (const auto& i : cases) {
      TestResolveRelative(i);
    }
  } else {
    ResolveRelativeCase cases[] = {
        // It's still possible to get an invalid path URL.
        //
        // Note: If the flag is enabled, "custom://Invalid:!#Auth/" is an
        // invalid URL.
        // ResolveRelative() should be never called.
        {"custom://Invalid:!#Auth/", "file.html", std::nullopt},

        // Resolving should fail if the base URL is authority-based but is
        // missing a path component (the '/' at the end).
        {"scheme://Authority", "path", std::nullopt},

        // In this case, the backslashes will not be canonicalized because it's
        // a non-standard URL, but they will be treated as a path separators,
        // giving the base URL here a path of "\".
        //
        // The result here is somewhat arbitrary. One could argue it should be
        // either "aaa://a\" or "aaa://a/" since the path is being replaced with
        // the "current directory". But in the context of resolving on data
        // URLs, adding the requested dot doesn't seem wrong either.
        //
        // Note: If the flag is enabled, "aaa://a\\" is an invalid URL.
        // ResolveRelative() should be never called.
        {"aaa://a\\", "aaa:.", "aaa://a\\."}};
    for (const auto& i : cases) {
      TestResolveRelative(i);
    }
  }
}

TEST_P(URLUtilTypedTest, OpaqueNonSpecialScheme) {
  // Ensure that the behavior of "android:" scheme URL is preserved, which is
  // not URL Standard compliant.
  //
  // URL Standard-wise, "android://a b" is an invalid URL because the host part
  // includes a space character, which is not allowed.
  std::optional<std::string> res = CanonicalizeSpec("android://a b", false);
  ASSERT_TRUE(res);
  EXPECT_EQ(*res, "android://a b");

  // Test a "git:" scheme URL for comparison.
  res = CanonicalizeSpec("git://a b", false);
  if (use_standard_compliant_non_special_scheme_url_parsing_) {
    // This is correct behavior because "git://a b" is an invalid URL.
    EXPECT_FALSE(res);
  } else {
    ASSERT_TRUE(res);
    EXPECT_EQ(*res, "git://a b");
  }
}

INSTANTIATE_TEST_SUITE_P(All, URLUtilTypedTest, ::testing::Bool());

}  // namespace url
