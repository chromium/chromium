// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/350788890): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "url/url_canon.h"

#include <errno.h>
#include <stddef.h>
#include <string_view>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_canon_internal.h"
#include "url/url_canon_stdstring.h"
#include "url/url_features.h"
#include "url/url_test_utils.h"

namespace url {

namespace {

struct ComponentCase {
  const char* input;
  const char* expected;
  Component expected_component;
  bool expected_success;
};

// ComponentCase but with dual 8-bit/16-bit input. Generally, the unit tests
// treat each input as optional, and will only try processing if non-NULL.
// The output is always 8-bit.
struct DualComponentCase {
  const char* input8;
  const wchar_t* input16;
  const char* expected;
  Component expected_component;
  bool expected_success;
};

// Test cases for CanonicalizeIPAddress(). The inputs are identical to
// DualComponentCase, but the output has extra CanonHostInfo fields.
struct IPAddressCase {
  const char* input8;
  const wchar_t* input16;
  const char* expected;
  Component expected_component;

  // CanonHostInfo fields, for verbose output.
  CanonHostInfo::Family expected_family;
  int expected_num_ipv4_components;
  const char* expected_address_hex;  // Two hex chars per IP address byte.
};

struct ReplaceCase {
  const char* base;
  const char* scheme;
  const char* username;
  const char* password;
  const char* host;
  const char* port;
  const char* path;
  const char* query;
  const char* ref;
  const char* expected;
};

// Magic string used in the replacements code that tells SetupReplComp to
// call the clear function.
const char kDeleteComp[] = "|";

// Sets up a replacement for a single component. This is given pointers to
// the set and clear function for the component being replaced, and will
// either set the component (if it exists) or clear it (if the replacement
// string matches kDeleteComp).
//
// This template is currently used only for the 8-bit case, and the strlen
// causes it to fail in other cases. It is left a template in case we have
// tests for wide replacements.
template<typename CHAR>
void SetupReplComp(
    void (Replacements<CHAR>::*set)(const CHAR*, const Component&),
    void (Replacements<CHAR>::*clear)(),
    Replacements<CHAR>* rep,
    const CHAR* str) {
  if (str && str[0] == kDeleteComp[0]) {
    (rep->*clear)();
  } else if (str) {
    (rep->*set)(str, Component(0, static_cast<int>(strlen(str))));
  }
}

bool CanonicalizeSpecialPath(const char* spec,
                             const Component& path,
                             CanonOutput* output,
                             Component* out_path) {
  return CanonicalizePath(spec, path, CanonMode::kSpecialURL, output, out_path);
}

bool CanonicalizeSpecialPath(const char16_t* spec,
                             const Component& path,
                             CanonOutput* output,
                             Component* out_path) {
  return CanonicalizePath(spec, path, CanonMode::kSpecialURL, output, out_path);
}

bool CanonicalizeNonSpecialPath(const char* spec,
                                const Component& path,
                                CanonOutput* output,
                                Component* out_path) {
  return CanonicalizePath(spec, path, CanonMode::kNonSpecialURL, output,
                          out_path);
}

bool CanonicalizeNonSpecialPath(const char16_t* spec,
                                const Component& path,
                                CanonOutput* output,
                                Component* out_path) {
  return CanonicalizePath(spec, path, CanonMode::kNonSpecialURL, output,
                          out_path);
}

}  // namespace

class URLCanonTest : public ::testing::Test {
 public:
  URLCanonTest() {
    scoped_feature_list_.InitAndEnableFeature(
        url::kDisallowSpaceCharacterInURLHostParsing);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(URLCanonTest, DoAppendUTF8) {
  struct UTF8Case {
    unsigned input;
    const char* output;
  } utf_cases[] = {
    // Valid code points.
    {0x24, "\x24"},
    {0xA2, "\xC2\xA2"},
    {0x20AC, "\xE2\x82\xAC"},
    {0x24B62, "\xF0\xA4\xAD\xA2"},
    {0x10FFFF, "\xF4\x8F\xBF\xBF"},
  };
  std::string out_str;
  for (const auto& utf_case : utf_cases) {
    out_str.clear();
    StdStringCanonOutput output(&out_str);
    AppendUTF8Value(utf_case.input, &output);
    output.Complete();
    EXPECT_EQ(utf_case.output, out_str);
  }
}

TEST_F(URLCanonTest, DoAppendUTF8Invalid) {
  std::string out_str;
  StdStringCanonOutput output(&out_str);
  // Invalid code point (too large).
  EXPECT_DCHECK_DEATH({
    AppendUTF8Value(0x110000, &output);
    output.Complete();
  });
}

TEST_F(URLCanonTest, UTF) {
  // Low-level test that we handle reading, canonicalization, and writing
  // UTF-8/UTF-16 strings properly.
  struct UTFCase {
    const char* input8;
    const wchar_t* input16;
    bool expected_success;
    const char* output;
  } utf_cases[] = {
      // Valid canonical input should get passed through & escaped.
      {"\xe4\xbd\xa0\xe5\xa5\xbd", L"\x4f60\x597d", true, "%E4%BD%A0%E5%A5%BD"},
      // Test a character that takes > 16 bits (U+10300 = old italic letter A)
      {"\xF0\x90\x8C\x80", L"\xd800\xdf00", true, "%F0%90%8C%80"},
      // Non-shortest-form UTF-8 characters are invalid. The bad bytes should
      // each be replaced with the invalid character (EF BF DB in UTF-8).
      {"\xf0\x84\xbd\xa0\xe5\xa5\xbd", nullptr, false,
       "%EF%BF%BD%EF%BF%BD%EF%BF%BD%EF%BF%BD%E5%A5%BD"},
      // Invalid UTF-8 sequences should be marked as invalid (the first
      // sequence is truncated).
      {"\xe4\xa0\xe5\xa5\xbd", L"\xd800\x597d", false, "%EF%BF%BD%E5%A5%BD"},
      // Character going off the end.
      {"\xe4\xbd\xa0\xe5\xa5", L"\x4f60\xd800", false, "%E4%BD%A0%EF%BF%BD"},
      // ...same with low surrogates with no high surrogate.
      {nullptr, L"\xdc00", false, "%EF%BF%BD"},
      // Test a UTF-8 encoded surrogate value is marked as invalid.
      // ED A0 80 = U+D800
      {"\xed\xa0\x80", nullptr, false, "%EF%BF%BD%EF%BF%BD%EF%BF%BD"},
      // ...even when paired.
      {"\xed\xa0\x80\xed\xb0\x80", nullptr, false,
       "%EF%BF%BD%EF%BF%BD%EF%BF%BD%EF%BF%BD%EF%BF%BD%EF%BF%BD"},
  };

  std::string out_str;
  for (const auto& utf_case : utf_cases) {
    if (utf_case.input8) {
      out_str.clear();
      StdStringCanonOutput output(&out_str);

      size_t input_len = strlen(utf_case.input8);
      bool success = true;
      for (size_t ch = 0; ch < input_len; ch++) {
        success &=
            AppendUTF8EscapedChar(utf_case.input8, &ch, input_len, &output);
      }
      output.Complete();
      EXPECT_EQ(utf_case.expected_success, success);
      EXPECT_EQ(utf_case.output, out_str);
    }
    if (utf_case.input16) {
      out_str.clear();
      StdStringCanonOutput output(&out_str);

      std::u16string input_str(
          test_utils::TruncateWStringToUTF16(utf_case.input16));
      size_t input_len = input_str.length();
      bool success = true;
      for (size_t ch = 0; ch < input_len; ch++) {
        success &= AppendUTF8EscapedChar(input_str.c_str(), &ch, input_len,
                                         &output);
      }
      output.Complete();
      EXPECT_EQ(utf_case.expected_success, success);
      EXPECT_EQ(utf_case.output, out_str);
    }

    if (utf_case.input8 && utf_case.input16 && utf_case.expected_success) {
      // Check that the UTF-8 and UTF-16 inputs are equivalent.

      // UTF-16 -> UTF-8
      std::string input8_str(utf_case.input8);
      std::u16string input16_str(
          test_utils::TruncateWStringToUTF16(utf_case.input16));
      EXPECT_EQ(input8_str, base::UTF16ToUTF8(input16_str));

      // UTF-8 -> UTF-16
      EXPECT_EQ(input16_str, base::UTF8ToUTF16(input8_str));
    }
  }
}

TEST_F(URLCanonTest, Scheme) {
  // Here, we're mostly testing that unusual characters are handled properly.
  // The canonicalizer doesn't do any parsing or whitespace detection. It will
  // also do its best on error, and will escape funny sequences (these won't be
  // valid schemes and it will return error).
  //
  // Note that the canonicalizer will append a colon to the output to separate
  // out the rest of the URL, which is not present in the input. We check,
  // however, that the output range includes everything but the colon.
  ComponentCase scheme_cases[] = {
    {"http", "http:", Component(0, 4), true},
    {"HTTP", "http:", Component(0, 4), true},
    {" HTTP ", "%20http%20:", Component(0, 10), false},
    {"htt: ", "htt%3A%20:", Component(0, 9), false},
    {"\xe4\xbd\xa0\xe5\xa5\xbdhttp", "%E4%BD%A0%E5%A5%BDhttp:", Component(0, 22), false},
      // Don't re-escape something already escaped. Note that it will
      // "canonicalize" the 'A' to 'a', but that's OK.
    {"ht%3Atp", "ht%3atp:", Component(0, 7), false},
    {"", ":", Component(0, 0), false},
  };

  std::string out_str;

  for (const auto& scheme_case : scheme_cases) {
    int url_len = static_cast<int>(strlen(scheme_case.input));
    Component in_comp(0, url_len);
    Component out_comp;

    out_str.clear();
    StdStringCanonOutput output1(&out_str);
    bool success =
        CanonicalizeScheme(scheme_case.input, in_comp, &output1, &out_comp);
    output1.Complete();

    EXPECT_EQ(scheme_case.expected_success, success);
    EXPECT_EQ(scheme_case.expected, out_str);
    EXPECT_EQ(scheme_case.expected_component.begin, out_comp.begin);
    EXPECT_EQ(scheme_case.expected_component.len, out_comp.len);

    // Now try the wide version.
    out_str.clear();
    StdStringCanonOutput output2(&out_str);

    std::u16string wide_input(base::UTF8ToUTF16(scheme_case.input));
    in_comp.len = static_cast<int>(wide_input.length());
    success = CanonicalizeScheme(wide_input.c_str(), in_comp, &output2,
                                 &out_comp);
    output2.Complete();

    EXPECT_EQ(scheme_case.expected_success, success);
    EXPECT_EQ(scheme_case.expected, out_str);
    EXPECT_EQ(scheme_case.expected_component.begin, out_comp.begin);
    EXPECT_EQ(scheme_case.expected_component.len, out_comp.len);
  }

  // Test the case where the scheme is declared nonexistent, it should be
  // converted into an empty scheme.
  Component out_comp;
  out_str.clear();
  StdStringCanonOutput output(&out_str);

  EXPECT_FALSE(CanonicalizeScheme("", Component(0, -1), &output, &out_comp));
  output.Complete();

  EXPECT_EQ(":", out_str);
  EXPECT_EQ(0, out_comp.begin);
  EXPECT_EQ(0, out_comp.len);
}

// IDNA mode to use in CanonHost tests.
enum class IDNAMode { kTransitional, kNonTransitional };

class URLCanonHostTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<IDNAMode> {
 public:
  URLCanonHostTest() {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    if (GetParam() == IDNAMode::kNonTransitional) {
      enabled_features.push_back(kUseIDNA2008NonTransitional);
    } else {
      disabled_features.push_back(kUseIDNA2008NonTransitional);
    }

    enabled_features.push_back(url::kDisallowSpaceCharacterInURLHostParsing);
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         URLCanonHostTest,
                         ::testing::Values(IDNAMode::kTransitional,
                                           IDNAMode::kNonTransitional));

TEST_P(URLCanonHostTest, Host) {
  bool use_idna_non_transitional = IsUsingIDNA2008NonTransitional();

  // clang-format off
  IPAddressCase host_cases[] = {
      // Basic canonicalization, uppercase should be converted to lowercase.
      {"GoOgLe.CoM", L"GoOgLe.CoM", "google.com", Component(0, 10),
       CanonHostInfo::NEUTRAL, -1, ""},
      {"Goo%20 goo.com", L"Goo%20 goo.com", "goo%20%20goo.com",
       Component(0, 16), CanonHostInfo::BROKEN, -1, ""},
      // TODO(crbug.com/40256677): Update the test after ASTERISK is
      // correctly handled.
      {"Goo%2a*goo.com", L"Goo%2a*goo.com", "goo%2A%2Agoo.com",
       Component(0, 16), CanonHostInfo::NEUTRAL, -1, ""},
      // Exciting different types of spaces!
      {nullptr, L"GOO\x00a0\x3000goo.com", "goo%20%20goo.com", Component(0, 16),
       CanonHostInfo::BROKEN, -1, ""},
      // Other types of space (no-break, zero-width, zero-width-no-break) are
      // name-prepped away to nothing.
      {nullptr, L"GOO\x200b\x2060\xfeffgoo.com", "googoo.com", Component(0, 10),
       CanonHostInfo::NEUTRAL, -1, ""},
      // Ideographic full stop (full-width period for Chinese, etc.) should be
      // treated as a dot.
      {nullptr,
       L"www.foo\x3002"
       L"bar.com",
       "www.foo.bar.com", Component(0, 15), CanonHostInfo::NEUTRAL, -1, ""},
      // Invalid unicode characters should fail...
      {"\xef\xb7\x90zyx.com", L"\xfdd0zyx.com", "%EF%B7%90zyx.com",
       Component(0, 16), CanonHostInfo::BROKEN, -1, ""},
      // ...This is the same as previous but with with escaped.
      {"%ef%b7%90zyx.com", L"%ef%b7%90zyx.com", "%EF%B7%90zyx.com",
       Component(0, 16), CanonHostInfo::BROKEN, -1, ""},
      // Test name prepping, fullwidth input should be converted to ASCII and
      // NOT
      // IDN-ized. This is "Go" in fullwidth UTF-8/UTF-16.
      {"\xef\xbc\xa7\xef\xbd\x8f.com", L"\xff27\xff4f.com", "go.com",
       Component(0, 6), CanonHostInfo::NEUTRAL, -1, ""},
      // Test that fullwidth escaped values are properly name-prepped,
      // then converted or rejected.
      // ...%41 in fullwidth = 'A' (also as escaped UTF-8 input)
      {"\xef\xbc\x85\xef\xbc\x94\xef\xbc\x91.com", L"\xff05\xff14\xff11.com",
       "a.com", Component(0, 5), CanonHostInfo::NEUTRAL, -1, ""},
      {"%ef%bc%85%ef%bc%94%ef%bc%91.com", L"%ef%bc%85%ef%bc%94%ef%bc%91.com",
       "a.com", Component(0, 5), CanonHostInfo::NEUTRAL, -1, ""},
      // ...%00 in fullwidth should fail (also as escaped UTF-8 input)
      {"\xef\xbc\x85\xef\xbc\x90\xef\xbc\x90.com", L"\xff05\xff10\xff10.com",
       "%00.com", Component(0, 7), CanonHostInfo::BROKEN, -1, ""},
      {"%ef%bc%85%ef%bc%90%ef%bc%90.com", L"%ef%bc%85%ef%bc%90%ef%bc%90.com",
       "%00.com", Component(0, 7), CanonHostInfo::BROKEN, -1, ""},
      // ICU will convert weird percents into ASCII percents, but not unescape
      // further. A weird percent is U+FE6A (EF B9 AA in UTF-8) which is a
      // "small percent". At this point we should be within our rights to mark
      // anything as invalid since the URL is corrupt or malicious. The code
      // happens to allow ASCII characters (%41 = "A" -> 'a') to be unescaped
      // and kept as valid, so we validate that behavior here, but this level
      // of fixing the input shouldn't be seen as required. "%81" is invalid.
      {"\xef\xb9\xaa"
       "41.com",
       L"\xfe6a"
       L"41.com",
       "a.com", Component(0, 5), CanonHostInfo::NEUTRAL, -1, ""},
      {"%ef%b9%aa"
       "41.com",
       L"\xfe6a"
       L"41.com",
       "a.com", Component(0, 5), CanonHostInfo::NEUTRAL, -1, ""},
      {"\xef\xb9\xaa"
       "81.com",
       L"\xfe6a"
       L"81.com",
       "%81.com", Component(0, 7), CanonHostInfo::BROKEN, -1, ""},
      {"%ef%b9%aa"
       "81.com",
       L"\xfe6a"
       L"81.com",
       "%81.com", Component(0, 7), CanonHostInfo::BROKEN, -1, ""},
      // Basic IDN support, UTF-8 and UTF-16 input should be converted to IDN
      {"\xe4\xbd\xa0\xe5\xa5\xbd\xe4\xbd\xa0\xe5\xa5\xbd",
       L"\x4f60\x597d\x4f60\x597d", "xn--6qqa088eba", Component(0, 14),
       CanonHostInfo::NEUTRAL, -1, ""},
      // See http://unicode.org/cldr/utility/idna.jsp for other
      // examples/experiments and http://goo.gl/7yG11o
      // for the full list of characters handled differently by
      // IDNA 2003, UTS 46 (http://unicode.org/reports/tr46/ ) and IDNA 2008.

      // 4 Deviation characters are mapped/ignored in UTS 46 transitional
      // mechansm. UTS 46, table 4 row (g).
      // Sharp-s is mapped to 'ss' in IDNA 2003, not in IDNA 2008 or UTF 46
      // after transitional period.
      // Previously, it'd be "fussball.de".
      {"fu\xc3\x9f"
       "ball.de",
       L"fu\x00df"
       L"ball.de",
       use_idna_non_transitional ? "xn--fuball-cta.de" : "fussball.de",
       use_idna_non_transitional ? Component(0, 17) : Component(0, 11),
       CanonHostInfo::NEUTRAL, -1, ""},

      // Final-sigma (U+03C3) was mapped to regular sigma (U+03C2).
      // Previously, it'd be "xn--wxaikc9b".
      {"\xcf\x83\xcf\x8c\xce\xbb\xce\xbf\xcf\x82", L"\x3c3\x3cc\x3bb\x3bf\x3c2",
       use_idna_non_transitional ? "xn--wxaijb9b" : "xn--wxaikc6b",
       Component(0, 12), CanonHostInfo::NEUTRAL, -1, ""},

      // ZWNJ (U+200C) and ZWJ (U+200D) are mapped away in UTS 46 transitional
      // handling as well as in IDNA 2003, but not thereafter.
      {"a\xe2\x80\x8c"
       "b\xe2\x80\x8d"
       "c",
       L"a\x200c"
       L"b\x200d"
       L"c",
       use_idna_non_transitional ? "xn--abc-9m0ag" : "abc",
       use_idna_non_transitional ? Component(0, 13) : Component(0, 3),
       CanonHostInfo::NEUTRAL, -1, ""},

      // ZWJ between Devanagari characters was still mapped away in UTS 46
      // transitional handling. IDNA 2008 gives xn--11bo0mv54g.
      // Previously "xn--11bo0m".
      {"\xe0\xa4\x95\xe0\xa5\x8d\xe2\x80\x8d\xe0\xa4\x9c",
       L"\x915\x94d\x200d\x91c",
       use_idna_non_transitional ? "xn--11bo0mv54g" : "xn--11bo0m",
       use_idna_non_transitional ? Component(0, 14) : Component(0, 10),
       CanonHostInfo::NEUTRAL, -1, ""},

      // Fullwidth exclamation mark is disallowed. UTS 46, table 4, row (b)
      // However, we do allow this at the moment because we don't use
      // STD3 rules and canonicalize full-width ASCII to ASCII.
      {"wow\xef\xbc\x81", L"wow\xff01", "wow!", Component(0, 4),
       CanonHostInfo::NEUTRAL, -1, ""},
      // U+2132 (turned capital F) is disallowed. UTS 46, table 4, row (c)
      // Allowed in IDNA 2003, but the mapping changed after Unicode 3.2
      {"\xe2\x84\xb2oo", L"\x2132oo", "%E2%84%B2oo", Component(0, 11),
       CanonHostInfo::BROKEN, -1, ""},
      // U+2F868 (CJK Comp) is disallowed. UTS 46, table 4, row (d)
      // Allowed in IDNA 2003, but the mapping changed after Unicode 3.2
      {"\xf0\xaf\xa1\xa8\xe5\xa7\xbb.cn", L"\xd87e\xdc68\x59fb.cn",
       "%F0%AF%A1%A8%E5%A7%BB.cn", Component(0, 24), CanonHostInfo::BROKEN, -1,
       ""},
      // Maps uppercase letters to lower case letters. UTS 46 table 4 row (e)
      {"M\xc3\x9cNCHEN", L"M\xdcNCHEN", "xn--mnchen-3ya", Component(0, 14),
       CanonHostInfo::NEUTRAL, -1, ""},
      // An already-IDNA host is not modified.
      {"xn--mnchen-3ya", L"xn--mnchen-3ya", "xn--mnchen-3ya", Component(0, 14),
       CanonHostInfo::NEUTRAL, -1, ""},
      // Symbol/punctuations are allowed in IDNA 2003/UTS46.
      // Not allowed in IDNA 2008. UTS 46 table 4 row (f).
      {"\xe2\x99\xa5ny.us", L"\x2665ny.us", "xn--ny-s0x.us", Component(0, 13),
       CanonHostInfo::NEUTRAL, -1, ""},
      // U+11013 is new in Unicode 6.0 and is allowed. UTS 46 table 4, row (h)
      // We used to allow it because we passed through unassigned code points.
      {"\xf0\x91\x80\x93.com", L"\xd804\xdc13.com", "xn--n00d.com",
       Component(0, 12), CanonHostInfo::NEUTRAL, -1, ""},
      // U+0602 is disallowed in UTS46/IDNA 2008. UTS 46 table 4, row(i)
      // Used to be allowed in INDA 2003.
      {"\xd8\x82.eg", L"\x602.eg", "%D8%82.eg", Component(0, 9),
       CanonHostInfo::BROKEN, -1, ""},
      // U+20B7 is new in Unicode 5.2 (not a part of IDNA 2003 based
      // on Unicode 3.2). We did allow it in the past because we let unassigned
      // code point pass. We continue to allow it even though it's a
      // "punctuation and symbol" blocked in IDNA 2008.
      // UTS 46 table 4, row (j)
      {"\xe2\x82\xb7.com", L"\x20b7.com", "xn--wzg.com", Component(0, 11),
       CanonHostInfo::NEUTRAL, -1, ""},
      // Maps uppercase letters to lower case letters.
      // In IDNA 2003, it's allowed without case-folding
      // ( xn--bc-7cb.com ) because it's not defined in Unicode 3.2
      // (added in Unicode 4.1). UTS 46 table 4 row (k)
      {"bc\xc8\xba.com", L"bc\x23a.com", "xn--bc-is1a.com", Component(0, 15),
       CanonHostInfo::NEUTRAL, -1, ""},
      // Maps U+FF43 (Full Width Small Letter C) to 'c'.
      {"ab\xef\xbd\x83.xyz", L"ab\xff43.xyz", "abc.xyz", Component(0, 7),
       CanonHostInfo::NEUTRAL, -1, ""},
      // Maps U+1D68C (Math Monospace Small C) to 'c'.
      // U+1D68C = \xD835\xDE8C in UTF-16
      {"ab\xf0\x9d\x9a\x8c.xyz", L"ab\xd835\xde8c.xyz", "abc.xyz",
       Component(0, 7), CanonHostInfo::NEUTRAL, -1, ""},
      // BiDi check test
      // "Divehi" in Divehi (Thaana script) ends with BidiClass=NSM.
      // Disallowed in IDNA 2003 but now allowed in UTS 46/IDNA 2008.
      {"\xde\x8b\xde\xa8\xde\x88\xde\xac\xde\x80\xde\xa8",
       L"\x78b\x7a8\x788\x7ac\x780\x7a8", "xn--hqbpi0jcw", Component(0, 13),
       CanonHostInfo::NEUTRAL, -1, ""},
      // Disallowed in both IDNA 2003 and 2008 with BiDi check.
      // Labels starting with a RTL character cannot end with a LTR character.
      {"\xd8\xac\xd8\xa7\xd8\xb1xyz", L"\x62c\x627\x631xyz",
       "%D8%AC%D8%A7%D8%B1xyz", Component(0, 21), CanonHostInfo::BROKEN, -1,
       ""},
      // Labels starting with a RTL character can end with BC=EN (European
      // number). Disallowed in IDNA 2003 but now allowed.
      {"\xd8\xac\xd8\xa7\xd8\xb1"
       "2",
       L"\x62c\x627\x631"
       L"2",
       "xn--2-ymcov", Component(0, 11), CanonHostInfo::NEUTRAL, -1, ""},
      // Labels starting with a RTL character cannot have "L" characters
      // even if it ends with an BC=EN. Disallowed in both IDNA 2003/2008.
      {"\xd8\xac\xd8\xa7\xd8\xb1xy2", L"\x62c\x627\x631xy2",
       "%D8%AC%D8%A7%D8%B1xy2", Component(0, 21), CanonHostInfo::BROKEN, -1,
       ""},
      // Labels starting with a RTL character can end with BC=AN (Arabic number)
      // Disallowed in IDNA 2003, but now allowed.
      {"\xd8\xac\xd8\xa7\xd8\xb1\xd9\xa2", L"\x62c\x627\x631\x662",
       "xn--mgbjq0r", Component(0, 11), CanonHostInfo::NEUTRAL, -1, ""},
      // Labels starting with a RTL character cannot have "L" characters
      // even if it ends with an BC=AN (Arabic number).
      // Disallowed in both IDNA 2003/2008.
      {"\xd8\xac\xd8\xa7\xd8\xb1xy\xd9\xa2", L"\x62c\x627\x631xy\x662",
       "%D8%AC%D8%A7%D8%B1xy%D9%A2", Component(0, 26), CanonHostInfo::BROKEN,
       -1, ""},
      // Labels starting with a RTL character cannot mix BC=EN and BC=AN
      {"\xd8\xac\xd8\xa7\xd8\xb1xy2\xd9\xa2", L"\x62c\x627\x631xy2\x662",
       "%D8%AC%D8%A7%D8%B1xy2%D9%A2", Component(0, 27), CanonHostInfo::BROKEN,
       -1, ""},
      // As of Unicode 6.2, U+20CF is not assigned. We do not allow it.
      {"\xe2\x83\x8f.com", L"\x20cf.com", "%E2%83%8F.com", Component(0, 13),
       CanonHostInfo::BROKEN, -1, ""},
      // U+0080 is not allowed.
      {"\xc2\x80.com", L"\x80.com", "%C2%80.com", Component(0, 10),
       CanonHostInfo::BROKEN, -1, ""},
      // Mixed UTF-8 and escaped UTF-8 (narrow case) and UTF-16 and escaped
      // Mixed UTF-8 and escaped UTF-8 (narrow case) and UTF-16 and escaped
      // UTF-8 (wide case). The output should be equivalent to the true wide
      // character input above).
      {"%E4%BD%A0%E5%A5%BD\xe4\xbd\xa0\xe5\xa5\xbd",
       L"%E4%BD%A0%E5%A5%BD\x4f60\x597d", "xn--6qqa088eba", Component(0, 14),
       CanonHostInfo::NEUTRAL, -1, ""},
      // Invalid escaped characters should fail and the percents should be
      // escaped.
      {"%zz%66%a", L"%zz%66%a", "%25zzf%25a", Component(0, 10),
       CanonHostInfo::BROKEN, -1, ""},
      // If we get an invalid character that has been escaped.
      {"%25", L"%25", "%25", Component(0, 3), CanonHostInfo::BROKEN, -1, ""},
      {"hello%00", L"hello%00", "hello%00", Component(0, 8),
       CanonHostInfo::BROKEN, -1, ""},
      // Escaped numbers should be treated like IP addresses if they are.
      {"%30%78%63%30%2e%30%32%35%30.01", L"%30%78%63%30%2e%30%32%35%30.01",
       "192.168.0.1", Component(0, 11), CanonHostInfo::IPV4, 3, "C0A80001"},
      {"%30%78%63%30%2e%30%32%35%30.01%2e",
       L"%30%78%63%30%2e%30%32%35%30.01%2e", "192.168.0.1", Component(0, 11),
       CanonHostInfo::IPV4, 3, "C0A80001"},
      // Invalid escaping should trigger the regular host error handling.
      {"%3g%78%63%30%2e%30%32%35%30%2E.01",
       L"%3g%78%63%30%2e%30%32%35%30%2E.01", "%253gxc0.0250..01",
       Component(0, 17), CanonHostInfo::BROKEN, -1, ""},
      // Something that isn't exactly an IP should get treated as a host and
      // spaces treated as invalid.
      {"192.168.0.1 hello", L"192.168.0.1 hello", "192.168.0.1%20hello",
       Component(0, 19), CanonHostInfo::BROKEN, -1, ""},
      // Fullwidth and escaped UTF-8 fullwidth should still be treated as IP.
      // These are "0Xc0.0250.01" in fullwidth.
      {"\xef\xbc\x90%Ef%bc\xb8%ef%Bd%83\xef\xbc\x90%EF%BC%"
       "8E\xef\xbc\x90\xef\xbc\x92\xef\xbc\x95\xef\xbc\x90\xef\xbc%"
       "8E\xef\xbc\x90\xef\xbc\x91",
       L"\xff10\xff38\xff43\xff10\xff0e\xff10\xff12\xff15\xff10\xff0e\xff10"
       L"\xff11",
       "192.168.0.1", Component(0, 11), CanonHostInfo::IPV4, 3, "C0A80001"},
      // Broken IP addresses get marked as such.
      {"192.168.0.257", L"192.168.0.257", "192.168.0.257", Component(0, 13),
       CanonHostInfo::BROKEN, -1, ""},
      {"[google.com]", L"[google.com]", "[google.com]", Component(0, 12),
       CanonHostInfo::BROKEN, -1, ""},
      // Cyrillic letter followed by '(' should return punycode for '(' escaped
      // before punycode string was created. I.e.
      // if '(' is escaped after punycode is created we would get xn--%28-8tb
      // (incorrect).
      {"\xd1\x82(", L"\x0442(", "xn--(-8tb", Component(0, 9),
       CanonHostInfo::NEUTRAL, -1, ""},
      // Address with all hexadecimal characters with leading number of 1<<32
      // or greater and should return NEUTRAL rather than BROKEN if not all
      // components are numbers.
      {"12345678912345.de", L"12345678912345.de", "12345678912345.de",
       Component(0, 17), CanonHostInfo::NEUTRAL, -1, ""},
      {"1.12345678912345.de", L"1.12345678912345.de", "1.12345678912345.de",
       Component(0, 19), CanonHostInfo::NEUTRAL, -1, ""},
      {"12345678912345.12345678912345.de", L"12345678912345.12345678912345.de",
       "12345678912345.12345678912345.de", Component(0, 32),
       CanonHostInfo::NEUTRAL, -1, ""},
      {"1.2.0xB3A73CE5B59.de", L"1.2.0xB3A73CE5B59.de", "1.2.0xb3a73ce5b59.de",
       Component(0, 20), CanonHostInfo::NEUTRAL, -1, ""},
      {"12345678912345.0xde", L"12345678912345.0xde", "12345678912345.0xde",
       Component(0, 19), CanonHostInfo::BROKEN, -1, ""},
      // A label that starts with "xn--" but contains non-ASCII characters
      // should
      // be an error. Escape the invalid characters.
      {"xn--m\xc3\xbcnchen", L"xn--m\xfcnchen", "xn--m%C3%BCnchen",
       Component(0, 16), CanonHostInfo::BROKEN, -1, ""},
  };
  // clang-format on

  // CanonicalizeHost() non-verbose.
  std::string out_str;
  for (const auto& host_case : host_cases) {
    // Narrow version.
    if (host_case.input8) {
      int host_len = static_cast<int>(strlen(host_case.input8));
      Component in_comp(0, host_len);
      Component out_comp;

      out_str.clear();
      StdStringCanonOutput output(&out_str);

      bool success =
          CanonicalizeHost(host_case.input8, in_comp, &output, &out_comp);
      output.Complete();

      EXPECT_EQ(host_case.expected_family != CanonHostInfo::BROKEN, success)
          << "for input: " << host_case.input8;
      EXPECT_EQ(host_case.expected, out_str)
          << "for input: " << host_case.input8;
      EXPECT_EQ(host_case.expected_component.begin, out_comp.begin)
          << "for input: " << host_case.input8;
      EXPECT_EQ(host_case.expected_component.len, out_comp.len)
          << "for input: " << host_case.input8;
    }

    // Wide version.
    if (host_case.input16) {
      std::u16string input16(
          test_utils::TruncateWStringToUTF16(host_case.input16));
      int host_len = static_cast<int>(input16.length());
      Component in_comp(0, host_len);
      Component out_comp;

      out_str.clear();
      StdStringCanonOutput output(&out_str);

      bool success = CanonicalizeHost(input16.c_str(), in_comp, &output,
                                      &out_comp);
      output.Complete();

      EXPECT_EQ(host_case.expected_family != CanonHostInfo::BROKEN, success);
      EXPECT_EQ(host_case.expected, out_str);
      EXPECT_EQ(host_case.expected_component.begin, out_comp.begin);
      EXPECT_EQ(host_case.expected_component.len, out_comp.len);
    }
  }

  // CanonicalizeHostVerbose()
  for (const auto& host_case : host_cases) {
    // Narrow version.
    if (host_case.input8) {
      int host_len = static_cast<int>(strlen(host_case.input8));
      Component in_comp(0, host_len);

      out_str.clear();
      StdStringCanonOutput output(&out_str);
      CanonHostInfo host_info;

      CanonicalizeHostVerbose(host_case.input8, in_comp, &output, &host_info);
      output.Complete();

      EXPECT_EQ(host_case.expected_family, host_info.family);
      EXPECT_EQ(host_case.expected, out_str);
      EXPECT_EQ(host_case.expected_component.begin, host_info.out_host.begin);
      EXPECT_EQ(host_case.expected_component.len, host_info.out_host.len);
      EXPECT_EQ(
          host_case.expected_address_hex,
          base::HexEncode(host_info.address,
                          static_cast<size_t>(host_info.AddressLength())));
      if (host_case.expected_family == CanonHostInfo::IPV4) {
        EXPECT_EQ(host_case.expected_num_ipv4_components,
                  host_info.num_ipv4_components);
      }
    }

    // Wide version.
    if (host_case.input16) {
      std::u16string input16(
          test_utils::TruncateWStringToUTF16(host_case.input16));
      int host_len = static_cast<int>(input16.length());
      Component in_comp(0, host_len);

      out_str.clear();
      StdStringCanonOutput output(&out_str);
      CanonHostInfo host_info;

      CanonicalizeHostVerbose(input16.c_str(), in_comp, &output, &host_info);
      output.Complete();

      EXPECT_EQ(host_case.expected_family, host_info.family);
      EXPECT_EQ(host_case.expected, out_str);
      EXPECT_EQ(host_case.expected_component.begin, host_info.out_host.begin);
      EXPECT_EQ(host_case.expected_component.len, host_info.out_host.len);
      EXPECT_EQ(
          host_case.expected_address_hex,
          base::HexEncode(host_info.address,
                          static_cast<size_t>(host_info.AddressLength())));
      if (host_case.expected_family == CanonHostInfo::IPV4) {
        EXPECT_EQ(host_case.expected_num_ipv4_components,
                  host_info.num_ipv4_components);
      }
    }
  }
}

TEST_F(URLCanonTest, SpecialHostPuncutationChar) {
  // '%' is not tested here. '%' is used for percent-escaping.
  const std::string_view allowed_host_chars[] = {"!", "\"", "$", "&", "'", "(",
                                                 ")", "+",  ",", "-", ".", ";",
                                                 "=", "_",  "`", "{", "}", "~"};

  const std::string_view forbidden_host_chars[] = {
      " ", "#", "/", ":", "<", ">", "?", "@", "[", "\\", "]", "^", "|",
  };

  // Standard non-compliant characters which are escaped. See
  // https://crbug.com/1416013.
  struct EscapedCharTestCase {
    std::string_view input;
    std::string_view expected;
  } escaped_host_chars[] = {{"*", "%2A"}};

  for (const std::string_view input : allowed_host_chars) {
    std::string out_str;
    Component in_comp(0, input.size());
    Component out_comp;
    StdStringCanonOutput output(&out_str);
    bool success =
        CanonicalizeSpecialHost(input.data(), in_comp, output, out_comp);
    EXPECT_TRUE(success) << "Input: " << input;
    output.Complete();
    EXPECT_EQ(out_str, input) << "Input: " << input;
  }

  for (const std::string_view input : forbidden_host_chars) {
    std::string out_str;
    Component in_comp(0, input.size());
    Component out_comp;
    StdStringCanonOutput output(&out_str);
    EXPECT_FALSE(
        CanonicalizeSpecialHost(input.data(), in_comp, output, out_comp))
        << "Input: " << input;
  }

  for (const auto& c : escaped_host_chars) {
    std::string out_str;
    Component in_comp(0, c.input.size());
    Component out_comp;
    StdStringCanonOutput output(&out_str);
    bool success =
        CanonicalizeSpecialHost(c.input.data(), in_comp, output, out_comp);
    EXPECT_TRUE(success) << "Input: " << c.input;
    output.Complete();
    EXPECT_EQ(out_str, c.expected) << "Input: " << c.input;
  }
}

TEST_F(URLCanonTest, ForbiddenHostCodePoint) {
  // Test only CanonicalizeNonSpecialHost.
  // CanonicalizeSpecialHost is not standard compliant yet.
  // See URLCanonTest::SpecialHostPuncutationChar.

  // https://url.spec.whatwg.org/#forbidden-host-code-point
  const std::string_view forbidden_host_chars[] = {
      "\x09", "\x0A", "\x0D", " ", "#",  "/", ":", "<",
      ">",    "?",    "@",    "[", "\\", "]", "^", "|",
  };

  for (const std::string_view input : forbidden_host_chars) {
    std::string out_str;
    Component in_comp(0, input.size());
    Component out_comp;
    StdStringCanonOutput output(&out_str);
    EXPECT_FALSE(
        CanonicalizeNonSpecialHost(input.data(), in_comp, output, out_comp))
        << "Input: " << input;
  }

  // Test NULL manually.
  const char host_with_null[] = "a\0b";
  std::string out_str;
  Component in_comp(0, 3);
  Component out_comp;
  StdStringCanonOutput output(&out_str);
  EXPECT_FALSE(
      CanonicalizeNonSpecialHost(host_with_null, in_comp, output, out_comp));
}

TEST_F(URLCanonTest, IPv4) {
  // clang-format off
  IPAddressCase cases[] = {
    // Empty is not an IP address.
    {"", L"", "", Component(), CanonHostInfo::NEUTRAL, -1, ""},
    {".", L".", "", Component(), CanonHostInfo::NEUTRAL, -1, ""},
    // Regular IP addresses in different bases.
    {"192.168.0.1", L"192.168.0.1", "192.168.0.1", Component(0, 11), CanonHostInfo::IPV4, 4, "C0A80001"},
    {"0300.0250.00.01", L"0300.0250.00.01", "192.168.0.1", Component(0, 11), CanonHostInfo::IPV4, 4, "C0A80001"},
    {"0xC0.0Xa8.0x0.0x1", L"0xC0.0Xa8.0x0.0x1", "192.168.0.1", Component(0, 11), CanonHostInfo::IPV4, 4, "C0A80001"},
    // Non-IP addresses due to invalid characters.
    {"192.168.9.com", L"192.168.9.com", "", Component(), CanonHostInfo::NEUTRAL, -1, ""},
    // Hostnames with a numeric final component but other components that don't
    // parse as numbers should be considered broken.
    {"19a.168.0.1", L"19a.168.0.1", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"19a.168.0.1.", L"19a.168.0.1.", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"0308.0250.00.01", L"0308.0250.00.01", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"0308.0250.00.01.", L"0308.0250.00.01.", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"0xCG.0xA8.0x0.0x1", L"0xCG.0xA8.0x0.0x1", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"0xCG.0xA8.0x0.0x1.", L"0xCG.0xA8.0x0.0x1.", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    // Non-numeric terminal compeonent should be considered not IPv4 hostnames, but valid.
    {"19.168.0.1a", L"19.168.0.1a", "", Component(), CanonHostInfo::NEUTRAL, -1, ""},
    {"0xC.0xA8.0x0.0x1G", L"0xC.0xA8.0x0.0x1G", "", Component(), CanonHostInfo::NEUTRAL, -1, ""},
    // Hostnames that would be considered broken IPv4 hostnames should be considered valid non-IPv4 hostnames if they end with two dots instead of 0 or 1.
    {"19a.168.0.1..", L"19a.168.0.1..", "", Component(), CanonHostInfo::NEUTRAL, -1, ""},
    {"0308.0250.00.01..", L"0308.0250.00.01..", "", Component(), CanonHostInfo::NEUTRAL, -1, ""},
    {"0xCG.0xA8.0x0.0x1..", L"0xCG.0xA8.0x0.0x1..", "", Component(), CanonHostInfo::NEUTRAL, -1, ""},
    // Hosts with components that aren't considered valid IPv4 numbers but are entirely numeric should be considered invalid.
    {"1.2.3.08", L"1.2.3.08", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"1.2.3.08.", L"1.2.3.08.", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    // If there are not enough components, the last one should fill them out.
    {"192", L"192", "0.0.0.192", Component(0, 9), CanonHostInfo::IPV4, 1, "000000C0"},
    {"0xC0a80001", L"0xC0a80001", "192.168.0.1", Component(0, 11), CanonHostInfo::IPV4, 1, "C0A80001"},
    {"030052000001", L"030052000001", "192.168.0.1", Component(0, 11), CanonHostInfo::IPV4, 1, "C0A80001"},
    {"000030052000001", L"000030052000001", "192.168.0.1", Component(0, 11), CanonHostInfo::IPV4, 1, "C0A80001"},
    {"192.168", L"192.168", "192.0.0.168", Component(0, 11), CanonHostInfo::IPV4, 2, "C00000A8"},
    {"192.0x00A80001", L"192.0x000A80001", "192.168.0.1", Component(0, 11), CanonHostInfo::IPV4, 2, "C0A80001"},
    {"0xc0.052000001", L"0xc0.052000001", "192.168.0.1", Component(0, 11), CanonHostInfo::IPV4, 2, "C0A80001"},
    {"192.168.1", L"192.168.1", "192.168.0.1", Component(0, 11), CanonHostInfo::IPV4, 3, "C0A80001"},
    // Hostnames with too many components, but a numeric final numeric component are invalid.
    {"192.168.0.0.1", L"192.168.0.0.1", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    // We allow a single trailing dot.
    {"192.168.0.1.", L"192.168.0.1.", "192.168.0.1", Component(0, 11), CanonHostInfo::IPV4, 4, "C0A80001"},
    {"192.168.0.1. hello", L"192.168.0.1. hello", "", Component(), CanonHostInfo::NEUTRAL, -1, ""},
    {"192.168.0.1..", L"192.168.0.1..", "", Component(), CanonHostInfo::NEUTRAL, -1, ""},
    // Hosts with two dots in a row with a final numeric component are considered invalid.
    {"192.168..1", L"192.168..1", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"192.168..1.", L"192.168..1.", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    // Any numerical overflow should be marked as BROKEN.
    {"0x100.0", L"0x100.0", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"0x100.0.0", L"0x100.0.0", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"0x100.0.0.0", L"0x100.0.0.0", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"0.0x100.0.0", L"0.0x100.0.0", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"0.0.0x100.0", L"0.0.0x100.0", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"0.0.0.0x100", L"0.0.0.0x100", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"0.0.0x10000", L"0.0.0x10000", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"0.0x1000000", L"0.0x1000000", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"0x100000000", L"0x100000000", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    // Repeat the previous tests, minus 1, to verify boundaries.
    {"0xFF.0", L"0xFF.0", "255.0.0.0", Component(0, 9), CanonHostInfo::IPV4, 2, "FF000000"},
    {"0xFF.0.0", L"0xFF.0.0", "255.0.0.0", Component(0, 9), CanonHostInfo::IPV4, 3, "FF000000"},
    {"0xFF.0.0.0", L"0xFF.0.0.0", "255.0.0.0", Component(0, 9), CanonHostInfo::IPV4, 4, "FF000000"},
    {"0.0xFF.0.0", L"0.0xFF.0.0", "0.255.0.0", Component(0, 9), CanonHostInfo::IPV4, 4, "00FF0000"},
    {"0.0.0xFF.0", L"0.0.0xFF.0", "0.0.255.0", Component(0, 9), CanonHostInfo::IPV4, 4, "0000FF00"},
    {"0.0.0.0xFF", L"0.0.0.0xFF", "0.0.0.255", Component(0, 9), CanonHostInfo::IPV4, 4, "000000FF"},
    {"0.0.0xFFFF", L"0.0.0xFFFF", "0.0.255.255", Component(0, 11), CanonHostInfo::IPV4, 3, "0000FFFF"},
    {"0.0xFFFFFF", L"0.0xFFFFFF", "0.255.255.255", Component(0, 13), CanonHostInfo::IPV4, 2, "00FFFFFF"},
    {"0xFFFFFFFF", L"0xFFFFFFFF", "255.255.255.255", Component(0, 15), CanonHostInfo::IPV4, 1, "FFFFFFFF"},
    // Old trunctations tests. They're all "BROKEN" now.
    {"276.256.0xf1a2.077777", L"276.256.0xf1a2.077777", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"192.168.0.257", L"192.168.0.257", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"192.168.0xa20001", L"192.168.0xa20001", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"192.015052000001", L"192.015052000001", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"0X12C0a80001", L"0X12C0a80001", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"276.1.2", L"276.1.2", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    // Too many components should be rejected, in valid ranges or not.
    {"255.255.255.255.255", L"255.255.255.255.255", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"256.256.256.256.256", L"256.256.256.256.256", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    // Spaces should be rejected.
    {"192.168.0.1 hello", L"192.168.0.1 hello", "", Component(), CanonHostInfo::NEUTRAL, -1, ""},
    // Very large numbers.
    {"0000000000000300.0x00000000000000fF.00000000000000001", L"0000000000000300.0x00000000000000fF.00000000000000001", "192.255.0.1", Component(0, 11), CanonHostInfo::IPV4, 3, "C0FF0001"},
    {"0000000000000300.0xffffffffFFFFFFFF.3022415481470977", L"0000000000000300.0xffffffffFFFFFFFF.3022415481470977", "", Component(0, 11), CanonHostInfo::BROKEN, -1, ""},
    // A number has no length limit, but long numbers can still overflow.
    {"00000000000000000001", L"00000000000000000001", "0.0.0.1", Component(0, 7), CanonHostInfo::IPV4, 1, "00000001"},
    {"0000000000000000100000000000000001", L"0000000000000000100000000000000001", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    // If a long component is non-numeric, it's a hostname, *not* a broken IP.
    {"0.0.0.000000000000000000z", L"0.0.0.000000000000000000z", "", Component(), CanonHostInfo::NEUTRAL, -1, ""},
    {"0.0.0.100000000000000000z", L"0.0.0.100000000000000000z", "", Component(), CanonHostInfo::NEUTRAL, -1, ""},
    // Truncation of all zeros should still result in 0.
    {"0.00.0x.0x0", L"0.00.0x.0x0", "0.0.0.0", Component(0, 7), CanonHostInfo::IPV4, 4, "00000000"},
    // Non-ASCII characters in final component should return NEUTRAL.
    {"1.2.3.\xF0\x9F\x92\xA9", L"1.2.3.\xD83D\xDCA9", "", Component(), CanonHostInfo::NEUTRAL, -1, ""},
    {"1.2.3.4\xF0\x9F\x92\xA9", L"1.2.3.4\xD83D\xDCA9", "", Component(), CanonHostInfo::NEUTRAL, -1, ""},
    {"1.2.3.0x\xF0\x9F\x92\xA9", L"1.2.3.0x\xD83D\xDCA9", "", Component(), CanonHostInfo::NEUTRAL, -1, ""},
    {"1.2.3.0\xF0\x9F\x92\xA9", L"1.2.3.0\xD83D\xDCA9", "", Component(), CanonHostInfo::NEUTRAL, -1, ""},
    // Non-ASCII characters in other components should result in broken IPs when final component is numeric.
    {"1.2.\xF0\x9F\x92\xA9.4", L"1.2.\xD83D\xDCA9.4", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"1.2.3\xF0\x9F\x92\xA9.4", L"1.2.3\xD83D\xDCA9.4", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"1.2.0x\xF0\x9F\x92\xA9.4", L"1.2.0x\xD83D\xDCA9.4", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"1.2.0\xF0\x9F\x92\xA9.4", L"1.2.0\xD83D\xDCA9.4", "", Component(), CanonHostInfo::BROKEN, -1, ""},
    {"\xF0\x9F\x92\xA9.2.3.4", L"\xD83D\xDCA9.2.3.4", "", Component(), CanonHostInfo::BROKEN, -1, ""},
  };
  // clang-format on

  for (const auto& test_case : cases) {
    SCOPED_TRACE(test_case.input8);

    // 8-bit version.
    Component component(0, static_cast<int>(strlen(test_case.input8)));

    std::string out_str1;
    StdStringCanonOutput output1(&out_str1);
    CanonHostInfo host_info;
    CanonicalizeIPAddress(test_case.input8, component, &output1, &host_info);
    output1.Complete();

    EXPECT_EQ(test_case.expected_family, host_info.family);
    EXPECT_EQ(test_case.expected_address_hex,
              base::HexEncode(host_info.address,
                              static_cast<size_t>(host_info.AddressLength())));
    if (host_info.family == CanonHostInfo::IPV4) {
      EXPECT_STREQ(test_case.expected, out_str1.c_str());
      EXPECT_EQ(test_case.expected_component.begin, host_info.out_host.begin);
      EXPECT_EQ(test_case.expected_component.len, host_info.out_host.len);
      EXPECT_EQ(test_case.expected_num_ipv4_components,
                host_info.num_ipv4_components);
    }

    // 16-bit version.
    std::u16string input16(
        test_utils::TruncateWStringToUTF16(test_case.input16));
    component = Component(0, static_cast<int>(input16.length()));

    std::string out_str2;
    StdStringCanonOutput output2(&out_str2);
    CanonicalizeIPAddress(input16.c_str(), component, &output2, &host_info);
    output2.Complete();

    EXPECT_EQ(test_case.expected_family, host_info.family);
    EXPECT_EQ(test_case.expected_address_hex,
              base::HexEncode(host_info.address,
                              static_cast<size_t>(host_info.AddressLength())));
    if (host_info.family == CanonHostInfo::IPV4) {
      EXPECT_STREQ(test_case.expected, out_str2.c_str());
      EXPECT_EQ(test_case.expected_component.begin, host_info.out_host.begin);
      EXPECT_EQ(test_case.expected_component.len, host_info.out_host.len);
      EXPECT_EQ(test_case.expected_num_ipv4_components,
                host_info.num_ipv4_components);
    }
  }
}

TEST_F(URLCanonTest, IPv6) {
  IPAddressCase cases[] = {
      // Empty is not an IP address.
      {"", L"", "", Component(), CanonHostInfo::NEUTRAL, -1, ""},
      // Non-IPs with [:] characters are marked BROKEN.
      {":", L":", "", Component(), CanonHostInfo::BROKEN, -1, ""},
      {"[", L"[", "", Component(), CanonHostInfo::BROKEN, -1, ""},
      {"[:", L"[:", "", Component(), CanonHostInfo::BROKEN, -1, ""},
      {"]", L"]", "", Component(), CanonHostInfo::BROKEN, -1, ""},
      {":]", L":]", "", Component(), CanonHostInfo::BROKEN, -1, ""},
      {"[]", L"[]", "", Component(), CanonHostInfo::BROKEN, -1, ""},
      {"[:]", L"[:]", "", Component(), CanonHostInfo::BROKEN, -1, ""},
      // Regular IP address is invalid without bounding '[' and ']'.
      {"2001:db8::1", L"2001:db8::1", "", Component(), CanonHostInfo::BROKEN,
       -1, ""},
      {"[2001:db8::1", L"[2001:db8::1", "", Component(), CanonHostInfo::BROKEN,
       -1, ""},
      {"2001:db8::1]", L"2001:db8::1]", "", Component(), CanonHostInfo::BROKEN,
       -1, ""},
      // Regular IP addresses.
      {"[::]", L"[::]", "[::]", Component(0, 4), CanonHostInfo::IPV6, -1,
       "00000000000000000000000000000000"},
      {"[::1]", L"[::1]", "[::1]", Component(0, 5), CanonHostInfo::IPV6, -1,
       "00000000000000000000000000000001"},
      {"[1::]", L"[1::]", "[1::]", Component(0, 5), CanonHostInfo::IPV6, -1,
       "00010000000000000000000000000000"},

      // Leading zeros should be stripped.
      {"[000:01:02:003:004:5:6:007]", L"[000:01:02:003:004:5:6:007]",
       "[0:1:2:3:4:5:6:7]", Component(0, 17), CanonHostInfo::IPV6, -1,
       "00000001000200030004000500060007"},

      // Upper case letters should be lowercased.
      {"[A:b:c:DE:fF:0:1:aC]", L"[A:b:c:DE:fF:0:1:aC]", "[a:b:c:de:ff:0:1:ac]",
       Component(0, 20), CanonHostInfo::IPV6, -1,
       "000A000B000C00DE00FF0000000100AC"},

      // The same address can be written with different contractions, but should
      // get canonicalized to the same thing.
      {"[1:0:0:2::3:0]", L"[1:0:0:2::3:0]", "[1::2:0:0:3:0]", Component(0, 14),
       CanonHostInfo::IPV6, -1, "00010000000000020000000000030000"},
      {"[1::2:0:0:3:0]", L"[1::2:0:0:3:0]", "[1::2:0:0:3:0]", Component(0, 14),
       CanonHostInfo::IPV6, -1, "00010000000000020000000000030000"},

      // Addresses with embedded IPv4.
      {"[::192.168.0.1]", L"[::192.168.0.1]", "[::c0a8:1]", Component(0, 10),
       CanonHostInfo::IPV6, -1, "000000000000000000000000C0A80001"},
      {"[::ffff:192.168.0.1]", L"[::ffff:192.168.0.1]", "[::ffff:c0a8:1]",
       Component(0, 15), CanonHostInfo::IPV6, -1,
       "00000000000000000000FFFFC0A80001"},
      {"[::eeee:192.168.0.1]", L"[::eeee:192.168.0.1]", "[::eeee:c0a8:1]",
       Component(0, 15), CanonHostInfo::IPV6, -1,
       "00000000000000000000EEEEC0A80001"},
      {"[2001::192.168.0.1]", L"[2001::192.168.0.1]", "[2001::c0a8:1]",
       Component(0, 14), CanonHostInfo::IPV6, -1,
       "200100000000000000000000C0A80001"},
      {"[1:2:192.168.0.1:5:6]", L"[1:2:192.168.0.1:5:6]", "", Component(),
       CanonHostInfo::BROKEN, -1, ""},

      // IPv4 embedded IPv6 addresses
      {"[::ffff:192.1.2]", L"[::ffff:192.1.2]", "[::ffff:c001:2]", Component(),
       CanonHostInfo::BROKEN, -1, ""},
      {"[::ffff:192.1]", L"[::ffff:192.1]", "[::ffff:c000:1]", Component(),
       CanonHostInfo::BROKEN, -1, ""},
      {"[::ffff:192.1.2.3.4]", L"[::ffff:192.1.2.3.4]", "", Component(),
       CanonHostInfo::BROKEN, -1, ""},

      // IPv4 using hex.
      // TODO(eroman): Should this format be disallowed?
      {"[::ffff:0xC0.0Xa8.0x0.0x1]", L"[::ffff:0xC0.0Xa8.0x0.0x1]",
       "[::ffff:c0a8:1]", Component(0, 15), CanonHostInfo::IPV6, -1,
       "00000000000000000000FFFFC0A80001"},

      // There may be zeros surrounding the "::" contraction.
      {"[0:0::0:0:8]", L"[0:0::0:0:8]", "[::8]", Component(0, 5),
       CanonHostInfo::IPV6, -1, "00000000000000000000000000000008"},

      {"[2001:db8::1]", L"[2001:db8::1]", "[2001:db8::1]", Component(0, 13),
       CanonHostInfo::IPV6, -1, "20010DB8000000000000000000000001"},

      // Can only have one "::" contraction in an IPv6 string literal.
      {"[2001::db8::1]", L"[2001::db8::1]", "", Component(),
       CanonHostInfo::BROKEN, -1, ""},
      // No more than 2 consecutive ':'s.
      {"[2001:db8:::1]", L"[2001:db8:::1]", "", Component(),
       CanonHostInfo::BROKEN, -1, ""},
      {"[:::]", L"[:::]", "", Component(), CanonHostInfo::BROKEN, -1, ""},
      // Non-IP addresses due to invalid characters.
      {"[2001::.com]", L"[2001::.com]", "", Component(), CanonHostInfo::BROKEN,
       -1, ""},
      // If there are not enough components, the last one should fill them out.
      // ... omitted at this time ...
      // Too many components means not an IP address. Similarly, with too few
      // if using IPv4 compat or mapped addresses.
      {"[::192.168.0.0.1]", L"[::192.168.0.0.1]", "", Component(),
       CanonHostInfo::BROKEN, -1, ""},
      {"[::ffff:192.168.0.0.1]", L"[::ffff:192.168.0.0.1]", "", Component(),
       CanonHostInfo::BROKEN, -1, ""},
      {"[1:2:3:4:5:6:7:8:9]", L"[1:2:3:4:5:6:7:8:9]", "", Component(),
       CanonHostInfo::BROKEN, -1, ""},
      // Too many bits (even though 8 components, the last one holds 32 bits).
      {"[0:0:0:0:0:0:0:192.168.0.1]", L"[0:0:0:0:0:0:0:192.168.0.1]", "",
       Component(), CanonHostInfo::BROKEN, -1, ""},

      // Too many bits specified -- the contraction would have to be zero-length
      // to not exceed 128 bits.
      {"[1:2:3:4:5:6::192.168.0.1]", L"[1:2:3:4:5:6::192.168.0.1]", "",
       Component(), CanonHostInfo::BROKEN, -1, ""},

      // The contraction is for 16 bits of zero.
      {"[1:2:3:4:5:6::8]", L"[1:2:3:4:5:6::8]", "[1:2:3:4:5:6:0:8]",
       Component(0, 17), CanonHostInfo::IPV6, -1,
       "00010002000300040005000600000008"},

      // Cannot have a trailing colon.
      {"[1:2:3:4:5:6:7:8:]", L"[1:2:3:4:5:6:7:8:]", "", Component(),
       CanonHostInfo::BROKEN, -1, ""},
      {"[1:2:3:4:5:6:192.168.0.1:]", L"[1:2:3:4:5:6:192.168.0.1:]", "",
       Component(), CanonHostInfo::BROKEN, -1, ""},

      // Cannot have negative numbers.
      {"[-1:2:3:4:5:6:7:8]", L"[-1:2:3:4:5:6:7:8]", "", Component(),
       CanonHostInfo::BROKEN, -1, ""},

      // Scope ID -- the URL may contain an optional ["%" <scope_id>] section.
      // The scope_id should be included in the canonicalized URL, and is an
      // unsigned decimal number.

      // Invalid because no ID was given after the percent.

      // Don't allow scope-id
      {"[1::%1]", L"[1::%1]", "", Component(), CanonHostInfo::BROKEN, -1, ""},
      {"[1::%eth0]", L"[1::%eth0]", "", Component(), CanonHostInfo::BROKEN, -1,
       ""},
      {"[1::%]", L"[1::%]", "", Component(), CanonHostInfo::BROKEN, -1, ""},
      {"[%]", L"[%]", "", Component(), CanonHostInfo::BROKEN, -1, ""},
      {"[::%:]", L"[::%:]", "", Component(), CanonHostInfo::BROKEN, -1, ""},

      // Don't allow leading or trailing colons.
      {"[:0:0::0:0:8]", L"[:0:0::0:0:8]", "", Component(),
       CanonHostInfo::BROKEN, -1, ""},
      {"[0:0::0:0:8:]", L"[0:0::0:0:8:]", "", Component(),
       CanonHostInfo::BROKEN, -1, ""},
      {"[:0:0::0:0:8:]", L"[:0:0::0:0:8:]", "", Component(),
       CanonHostInfo::BROKEN, -1, ""},

      // We allow a single trailing dot.
      // ... omitted at this time ...
      // Two dots in a row means not an IP address.
      {"[::192.168..1]", L"[::192.168..1]", "", Component(),
       CanonHostInfo::BROKEN, -1, ""},
      // Any non-first components get truncated to one byte.
      // ... omitted at this time ...
      // Spaces should be rejected.
      {"[::1 hello]", L"[::1 hello]", "", Component(), CanonHostInfo::BROKEN,
       -1, ""},
  };

  for (size_t i = 0; i < std::size(cases); i++) {
    // 8-bit version.
    Component component(0, static_cast<int>(strlen(cases[i].input8)));

    std::string out_str1;
    StdStringCanonOutput output1(&out_str1);
    CanonHostInfo host_info;
    CanonicalizeIPAddress(cases[i].input8, component, &output1, &host_info);
    output1.Complete();

    EXPECT_EQ(cases[i].expected_family, host_info.family);
    EXPECT_EQ(cases[i].expected_address_hex,
              base::HexEncode(host_info.address,
                              static_cast<size_t>(host_info.AddressLength())))
        << "iter " << i << " host " << cases[i].input8;
    if (host_info.family == CanonHostInfo::IPV6) {
      EXPECT_STREQ(cases[i].expected, out_str1.c_str());
      EXPECT_EQ(cases[i].expected_component.begin,
                host_info.out_host.begin);
      EXPECT_EQ(cases[i].expected_component.len, host_info.out_host.len);
    }

    // 16-bit version.
    std::u16string input16(
        test_utils::TruncateWStringToUTF16(cases[i].input16));
    component = Component(0, static_cast<int>(input16.length()));

    std::string out_str2;
    StdStringCanonOutput output2(&out_str2);
    CanonicalizeIPAddress(input16.c_str(), component, &output2, &host_info);
    output2.Complete();

    EXPECT_EQ(cases[i].expected_family, host_info.family);
    EXPECT_EQ(cases[i].expected_address_hex,
              base::HexEncode(host_info.address,
                              static_cast<size_t>(host_info.AddressLength())));
    if (host_info.family == CanonHostInfo::IPV6) {
      EXPECT_STREQ(cases[i].expected, out_str2.c_str());
      EXPECT_EQ(cases[i].expected_component.begin, host_info.out_host.begin);
      EXPECT_EQ(cases[i].expected_component.len, host_info.out_host.len);
    }
  }
}

TEST_F(URLCanonTest, IPEmpty) {
  std::string out_str1;
  StdStringCanonOutput output1(&out_str1);
  CanonHostInfo host_info;

  // This tests tests.
  const char spec[] = "192.168.0.1";
  CanonicalizeIPAddress(spec, Component(), &output1, &host_info);
  EXPECT_FALSE(host_info.IsIPAddress());

  CanonicalizeIPAddress(spec, Component(0, 0), &output1, &host_info);
  EXPECT_FALSE(host_info.IsIPAddress());
}

// Verifies that CanonicalizeHostSubstring produces the expected output and
// does not "fix" IP addresses. Because this code is a subset of
// CanonicalizeHost, the shared functionality is not tested.
TEST_F(URLCanonTest, CanonicalizeHostSubstring) {
  // Basic sanity check.
  {
    std::string out_str;
    StdStringCanonOutput output(&out_str);
    EXPECT_TRUE(CanonicalizeHostSubstring("M\xc3\x9cNCHEN.com",
                                          Component(0, 12), &output));
    output.Complete();
    EXPECT_EQ("xn--mnchen-3ya.com", out_str);
  }

  // Failure case.
  {
    std::string out_str;
    StdStringCanonOutput output(&out_str);
    EXPECT_FALSE(CanonicalizeHostSubstring(
        test_utils::TruncateWStringToUTF16(L"\xfdd0zyx.com").c_str(),
        Component(0, 8), &output));
    output.Complete();
    EXPECT_EQ("%EF%B7%90zyx.com", out_str);
  }

  // Should return true for empty input strings.
  {
    std::string out_str;
    StdStringCanonOutput output(&out_str);
    EXPECT_TRUE(CanonicalizeHostSubstring("", Component(0, 0), &output));
    output.Complete();
    EXPECT_EQ(std::string(), out_str);
  }

  // Numbers that look like IP addresses should not be changed.
  {
    std::string out_str;
    StdStringCanonOutput output(&out_str);
    EXPECT_TRUE(
        CanonicalizeHostSubstring("01.02.03.04", Component(0, 11), &output));
    output.Complete();
    EXPECT_EQ("01.02.03.04", out_str);
  }
}

TEST_F(URLCanonTest, UserInfo) {
  // Note that the canonicalizer should escape and treat empty components as
  // not being there.

  // We actually parse a full input URL so we can get the initial components.
  struct UserComponentCase {
    const char* input;
    const char* expected;
    Component expected_username;
    Component expected_password;
    bool expected_success;
  } user_info_cases[] = {
    {"http://user:pass@host.com/", "user:pass@", Component(0, 4), Component(5, 4), true},
    {"http://@host.com/", "", Component(0, -1), Component(0, -1), true},
    {"http://:@host.com/", "", Component(0, -1), Component(0, -1), true},
    {"http://foo:@host.com/", "foo@", Component(0, 3), Component(0, -1), true},
    {"http://:foo@host.com/", ":foo@", Component(0, 0), Component(1, 3), true},
    {"http://^ :$\t@host.com/", "%5E%20:$%09@", Component(0, 6), Component(7, 4), true},
    {"http://user:pass@/", "user:pass@", Component(0, 4), Component(5, 4), true},
    {"http://%2540:bar@domain.com/", "%2540:bar@", Component(0, 5), Component(6, 3), true },

      // IE7 compatibility: old versions allowed backslashes in usernames, but
      // IE7 does not. We disallow it as well.
    {"ftp://me\\mydomain:pass@foo.com/", "", Component(0, -1), Component(0, -1), true},
  };

  for (const auto& user_info_case : user_info_cases) {
    Parsed parsed = ParseStandardURL(user_info_case.input);
    Component out_user, out_pass;
    std::string out_str;
    StdStringCanonOutput output1(&out_str);

    bool success = CanonicalizeUserInfo(user_info_case.input, parsed.username,
                                        user_info_case.input, parsed.password,
                                        &output1, &out_user, &out_pass);
    output1.Complete();

    EXPECT_EQ(user_info_case.expected_success, success);
    EXPECT_EQ(user_info_case.expected, out_str);
    EXPECT_EQ(user_info_case.expected_username.begin, out_user.begin);
    EXPECT_EQ(user_info_case.expected_username.len, out_user.len);
    EXPECT_EQ(user_info_case.expected_password.begin, out_pass.begin);
    EXPECT_EQ(user_info_case.expected_password.len, out_pass.len);

    // Now try the wide version
    out_str.clear();
    StdStringCanonOutput output2(&out_str);
    std::u16string wide_input(base::UTF8ToUTF16(user_info_case.input));
    success = CanonicalizeUserInfo(wide_input.c_str(),
                                   parsed.username,
                                   wide_input.c_str(),
                                   parsed.password,
                                   &output2,
                                   &out_user,
                                   &out_pass);
    output2.Complete();

    EXPECT_EQ(user_info_case.expected_success, success);
    EXPECT_EQ(user_info_case.expected, out_str);
    EXPECT_EQ(user_info_case.expected_username.begin, out_user.begin);
    EXPECT_EQ(user_info_case.expected_username.len, out_user.len);
    EXPECT_EQ(user_info_case.expected_password.begin, out_pass.begin);
    EXPECT_EQ(user_info_case.expected_password.len, out_pass.len);
  }
}

TEST_F(URLCanonTest, Port) {
  // We only need to test that the number gets properly put into the output
  // buffer. The parser unit tests will test scanning the number correctly.
  //
  // Note that the CanonicalizePort will always prepend a colon to the output
  // to separate it from the colon that it assumes precedes it.
  struct PortCase {
    const char* input;
    int default_port;
    const char* expected;
    Component expected_component;
    bool expected_success;
  } port_cases[] = {
      // Invalid input should be copied w/ failure.
    {"as df", 80, ":as%20df", Component(1, 7), false},
    {"-2", 80, ":-2", Component(1, 2), false},
      // Default port should be omitted.
    {"80", 80, "", Component(0, -1), true},
    {"8080", 80, ":8080", Component(1, 4), true},
      // PORT_UNSPECIFIED should mean always keep the port.
    {"80", PORT_UNSPECIFIED, ":80", Component(1, 2), true},
  };

  for (const auto& port_case : port_cases) {
    int url_len = static_cast<int>(strlen(port_case.input));
    Component in_comp(0, url_len);
    Component out_comp;
    std::string out_str;
    StdStringCanonOutput output1(&out_str);
    bool success = CanonicalizePort(
        port_case.input, in_comp, port_case.default_port, &output1, &out_comp);
    output1.Complete();

    EXPECT_EQ(port_case.expected_success, success);
    EXPECT_EQ(port_case.expected, out_str);
    EXPECT_EQ(port_case.expected_component.begin, out_comp.begin);
    EXPECT_EQ(port_case.expected_component.len, out_comp.len);

    // Now try the wide version
    out_str.clear();
    StdStringCanonOutput output2(&out_str);
    std::u16string wide_input(base::UTF8ToUTF16(port_case.input));
    success = CanonicalizePort(wide_input.c_str(), in_comp,
                               port_case.default_port, &output2, &out_comp);
    output2.Complete();

    EXPECT_EQ(port_case.expected_success, success);
    EXPECT_EQ(port_case.expected, out_str);
    EXPECT_EQ(port_case.expected_component.begin, out_comp.begin);
    EXPECT_EQ(port_case.expected_component.len, out_comp.len);
  }
}

DualComponentCase kCommonPathCases[] = {
    // ----- path collapsing tests -----
    {"/././foo", L"/././foo", "/foo", Component(0, 4), true},
    {"/./.foo", L"/./.foo", "/.foo", Component(0, 5), true},
    {"/foo/.", L"/foo/.", "/foo/", Component(0, 5), true},
    {"/foo/./", L"/foo/./", "/foo/", Component(0, 5), true},
    // double dots followed by a slash or the end of the string count
    {"/foo/bar/..", L"/foo/bar/..", "/foo/", Component(0, 5), true},
    {"/foo/bar/../", L"/foo/bar/../", "/foo/", Component(0, 5), true},
    // don't count double dots when they aren't followed by a slash
    {"/foo/..bar", L"/foo/..bar", "/foo/..bar", Component(0, 10), true},
    // some in the middle
    {"/foo/bar/../ton", L"/foo/bar/../ton", "/foo/ton", Component(0, 8), true},
    {"/foo/bar/../ton/../../a", L"/foo/bar/../ton/../../a", "/a",
     Component(0, 2), true},
    // we should not be able to go above the root
    {"/foo/../../..", L"/foo/../../..", "/", Component(0, 1), true},
    {"/foo/../../../ton", L"/foo/../../../ton", "/ton", Component(0, 4), true},
    // escaped dots should be unescaped and treated the same as dots
    {"/foo/%2e", L"/foo/%2e", "/foo/", Component(0, 5), true},
    {"/foo/%2e%2", L"/foo/%2e%2", "/foo/.%2", Component(0, 8), true},
    {"/foo/%2e./%2e%2e/.%2e/%2e.bar", L"/foo/%2e./%2e%2e/.%2e/%2e.bar",
     "/..bar", Component(0, 6), true},
    // Multiple slashes in a row should be preserved and treated like empty
    // directory names.
    {"////../..", L"////../..", "//", Component(0, 2), true},

    // ----- escaping tests -----
    {"/foo", L"/foo", "/foo", Component(0, 4), true},
    // Valid escape sequence
    {"/%20foo", L"/%20foo", "/%20foo", Component(0, 7), true},
    // Invalid escape sequence we should pass through unchanged.
    {"/foo%", L"/foo%", "/foo%", Component(0, 5), true},
    {"/foo%2", L"/foo%2", "/foo%2", Component(0, 6), true},
    // Invalid escape sequence: bad characters should be treated the same as
    // the surrounding text, not as escaped (in this case, UTF-8).
    {"/foo%2zbar", L"/foo%2zbar", "/foo%2zbar", Component(0, 10), true},
    {"/foo%2\xc2\xa9zbar", nullptr, "/foo%2%C2%A9zbar", Component(0, 16), true},
    {nullptr, L"/foo%2\xc2\xa9zbar", "/foo%2%C3%82%C2%A9zbar", Component(0, 22),
     true},
    // Regular characters that are escaped should remain escaped
    {"/foo%41%7a", L"/foo%41%7a", "/foo%41%7a", Component(0, 10), true},
    // Funny characters that are unescaped should be escaped
    {"/foo\x09\x91%91", nullptr, "/foo%09%91%91", Component(0, 13), true},
    {nullptr, L"/foo\x09\x91%91", "/foo%09%C2%91%91", Component(0, 16), true},
    // %00 should not cause failures.
    {"/foo%00%51", L"/foo%00%51", "/foo%00%51", Component(0, 10), true},
    // Some characters should be passed through unchanged regardless of esc.
    {"/(%28:%3A%29)", L"/(%28:%3A%29)", "/(%28:%3A%29)", Component(0, 13),
     true},
    // Characters that are properly escaped should not have the case changed
    // of hex letters.
    {"/%3A%3a%3C%3c", L"/%3A%3a%3C%3c", "/%3A%3a%3C%3c", Component(0, 13),
     true},
    // Funny characters that are unescaped should be escaped
    {"/foo\tbar", L"/foo\tbar", "/foo%09bar", Component(0, 10), true},
    // Hashes found in paths (possibly only when the caller explicitly sets
    // the path on an already-parsed URL) should be escaped.
    {"/foo#bar", L"/foo#bar", "/foo%23bar", Component(0, 10), true},
    // %7f should be allowed and %3D should not be unescaped (these were wrong
    // in a previous version).
    {"/%7Ffp3%3Eju%3Dduvgw%3Dd", L"/%7Ffp3%3Eju%3Dduvgw%3Dd",
     "/%7Ffp3%3Eju%3Dduvgw%3Dd", Component(0, 24), true},
    // @ should be passed through unchanged (escaped or unescaped).
    {"/@asdf%40", L"/@asdf%40", "/@asdf%40", Component(0, 9), true},
    // Nested escape sequences no longer happen. See https://crbug.com/1252531.
    {"/%A%42", L"/%A%42", "/%A%42", Component(0, 6), true},
    {"/%%41B", L"/%%41B", "/%%41B", Component(0, 6), true},
    {"/%%41%42", L"/%%41%42", "/%%41%42", Component(0, 8), true},
    // Make sure truncated "nested" escapes don't result in reading off the
    // string end.
    {"/%%41", L"/%%41", "/%%41", Component(0, 5), true},
    // Don't unescape the leading '%' if unescaping doesn't result in a valid
    // new escape sequence.
    {"/%%470", L"/%%470", "/%%470", Component(0, 6), true},
    {"/%%2D%41", L"/%%2D%41", "/%%2D%41", Component(0, 8), true},
    // Don't erroneously downcast a UTF-16 character in a way that makes it
    // look like part of an escape sequence.
    {nullptr, L"/%%41\x0130", "/%%41%C4%B0", Component(0, 11), true},

    // ----- encoding tests -----
    // Basic conversions
    {"/\xe4\xbd\xa0\xe5\xa5\xbd\xe4\xbd\xa0\xe5\xa5\xbd",
     L"/\x4f60\x597d\x4f60\x597d", "/%E4%BD%A0%E5%A5%BD%E4%BD%A0%E5%A5%BD",
     Component(0, 37), true},
    // Unicode Noncharacter (U+FDD0) should not fail.
    {"/\xef\xb7\x90zyx", nullptr, "/%EF%B7%90zyx", Component(0, 13), true},
    {nullptr, L"/\xfdd0zyx", "/%EF%B7%90zyx", Component(0, 13), true},
};

typedef bool (*CanonFunc8Bit)(const char*,
                              const Component&,
                              CanonOutput*,
                              Component*);
typedef bool (*CanonFunc16Bit)(const char16_t*,
                               const Component&,
                               CanonOutput*,
                               Component*);

void DoPathTest(const DualComponentCase* path_cases,
                size_t num_cases,
                CanonFunc8Bit canon_func_8,
                CanonFunc16Bit canon_func_16) {
  for (size_t i = 0; i < num_cases; i++) {
    testing::Message scope_message;
    scope_message << path_cases[i].input8 << "," << path_cases[i].input16;
    SCOPED_TRACE(scope_message);
    if (path_cases[i].input8) {
      int len = static_cast<int>(strlen(path_cases[i].input8));
      Component in_comp(0, len);
      Component out_comp;
      std::string out_str;
      StdStringCanonOutput output(&out_str);
      bool success =
          canon_func_8(path_cases[i].input8, in_comp, &output, &out_comp);
      output.Complete();

      EXPECT_EQ(path_cases[i].expected_success, success);
      EXPECT_EQ(path_cases[i].expected_component.begin, out_comp.begin);
      EXPECT_EQ(path_cases[i].expected_component.len, out_comp.len);
      EXPECT_EQ(path_cases[i].expected, out_str);
    }

    if (path_cases[i].input16) {
      std::u16string input16(
          test_utils::TruncateWStringToUTF16(path_cases[i].input16));
      int len = static_cast<int>(input16.length());
      Component in_comp(0, len);
      Component out_comp;
      std::string out_str;
      StdStringCanonOutput output(&out_str);

      bool success =
          canon_func_16(input16.c_str(), in_comp, &output, &out_comp);
      output.Complete();

      EXPECT_EQ(path_cases[i].expected_success, success);
      EXPECT_EQ(path_cases[i].expected_component.begin, out_comp.begin);
      EXPECT_EQ(path_cases[i].expected_component.len, out_comp.len);
      EXPECT_EQ(path_cases[i].expected, out_str);
    }
  }
}

TEST_F(URLCanonTest, SpecialPath) {
  // Common test cases
  DoPathTest(kCommonPathCases, std::size(kCommonPathCases),
             CanonicalizeSpecialPath, CanonicalizeSpecialPath);

  // Manual test: embedded NULLs should be escaped and the URL should be marked
  // as valid.
  const char path_with_null[] = "/ab\0c";
  Component in_comp(0, 5);
  Component out_comp;

  std::string out_str;
  StdStringCanonOutput output(&out_str);
  bool success =
      CanonicalizeSpecialPath(path_with_null, in_comp, &output, &out_comp);
  output.Complete();
  EXPECT_TRUE(success);
  EXPECT_EQ("/ab%00c", out_str);

  // Test cases specific on special URLs.
  DualComponentCase special_path_cases[] = {
      // Canonical path for empty path is a slash.
      {"", L"", "/", Component(0, 1), true},
      // Backslashes should be used as path separators.
      {"\\a\\b", L"\\a\\b", "/a/b", Component(0, 4), true},
      {"/a\\..\\b", L"/a\\..\\b", "/b", Component(0, 2), true},
      {"/a\\.\\b", L"/a\\.\\b", "/a/b", Component(0, 4), true},
  };

  DoPathTest(special_path_cases, std::size(special_path_cases),
             CanonicalizeSpecialPath, CanonicalizePath);
}

TEST_F(URLCanonTest, NonSpecialPath) {
  // Common test cases
  DoPathTest(kCommonPathCases, std::size(kCommonPathCases),
             CanonicalizeNonSpecialPath, CanonicalizeNonSpecialPath);

  // Test cases specific on non-special URLs.
  DualComponentCase non_special_path_cases[] = {
      // Empty.
      {"", L"", "", Component(0, 0), true},
      // Backslashes.
      {"/a\\..\\b", L"/a\\..\\b", "/a\\..\\b", Component(0, 7), true},
      {"/a\\./b", L"/a\\./b", "/a\\./b", Component(0, 6), true},
  };

  DoPathTest(non_special_path_cases, std::size(non_special_path_cases),
             CanonicalizeNonSpecialPath, CanonicalizeNonSpecialPath);
}

TEST_F(URLCanonTest, PartialPath) {
  DualComponentCase partial_path_cases[] = {
      {".html", L".html", ".html", Component(0, 5), true},
      {"", L"", "", Component(0, 0), true},
  };

  DoPathTest(kCommonPathCases, std::size(kCommonPathCases),
             CanonicalizePartialPath, CanonicalizePartialPath);
  DoPathTest(partial_path_cases, std::size(partial_path_cases),
             CanonicalizePartialPath, CanonicalizePartialPath);
}

TEST_F(URLCanonTest, Query) {
  struct QueryCase {
    const char* input8;
    const wchar_t* input16;
    const char* expected;
  } query_cases[] = {
      // Regular ASCII case.
    {"foo=bar", L"foo=bar", "?foo=bar"},
      // Allow question marks in the query without escaping
    {"as?df", L"as?df", "?as?df"},
      // Always escape '#' since it would mark the ref.
    {"as#df", L"as#df", "?as%23df"},
      // Escape some questionable 8-bit characters, but never unescape.
    {"\x02hello\x7f bye", L"\x02hello\x7f bye", "?%02hello%7F%20bye"},
    {"%40%41123", L"%40%41123", "?%40%41123"},
      // Chinese input/output
    {"q=\xe4\xbd\xa0\xe5\xa5\xbd", L"q=\x4f60\x597d", "?q=%E4%BD%A0%E5%A5%BD"},
      // Invalid UTF-8/16 input should be replaced with invalid characters.
    {"q=\xed\xed", L"q=\xd800\xd800", "?q=%EF%BF%BD%EF%BF%BD"},
      // Don't allow < or > because sometimes they are used for XSS if the
      // URL is echoed in content. Firefox does this, IE doesn't.
    {"q=<asdf>", L"q=<asdf>", "?q=%3Casdf%3E"},
      // Escape double quotemarks in the query.
    {"q=\"asdf\"", L"q=\"asdf\"", "?q=%22asdf%22"},
  };

  for (const auto& query_case : query_cases) {
    Component out_comp;

    if (query_case.input8) {
      int len = static_cast<int>(strlen(query_case.input8));
      Component in_comp(0, len);
      std::string out_str;

      StdStringCanonOutput output(&out_str);
      CanonicalizeQuery(query_case.input8, in_comp, nullptr, &output,
                        &out_comp);
      output.Complete();

      EXPECT_EQ(query_case.expected, out_str);
    }

    if (query_case.input16) {
      std::u16string input16(
          test_utils::TruncateWStringToUTF16(query_case.input16));
      int len = static_cast<int>(input16.length());
      Component in_comp(0, len);
      std::string out_str;

      StdStringCanonOutput output(&out_str);
      CanonicalizeQuery(input16.c_str(), in_comp, nullptr, &output, &out_comp);
      output.Complete();

      EXPECT_EQ(query_case.expected, out_str);
    }
  }

  // Extra test for input with embedded NULL;
  std::string out_str;
  StdStringCanonOutput output(&out_str);
  Component out_comp;
  CanonicalizeQuery("a \x00z\x01", Component(0, 5), nullptr, &output,
                    &out_comp);
  output.Complete();
  EXPECT_EQ("?a%20%00z%01", out_str);
}

TEST_F(URLCanonTest, Ref) {
  // Refs are trivial, it just checks the encoding.
  DualComponentCase ref_cases[] = {
      {"hello!", L"hello!", "#hello!", Component(1, 6), true},
      // We should escape spaces, double-quotes, angled braces, and backtics.
      {"hello, world", L"hello, world", "#hello,%20world", Component(1, 14),
       true},
      {"hello,\"world", L"hello,\"world", "#hello,%22world", Component(1, 14),
       true},
      {"hello,<world", L"hello,<world", "#hello,%3Cworld", Component(1, 14),
       true},
      {"hello,>world", L"hello,>world", "#hello,%3Eworld", Component(1, 14),
       true},
      {"hello,`world", L"hello,`world", "#hello,%60world", Component(1, 14),
       true},
      // UTF-8/wide input should be preserved
      {"\xc2\xa9", L"\xa9", "#%C2%A9", Component(1, 6), true},
      // Test a characer that takes > 16 bits (U+10300 = old italic letter A)
      {"\xF0\x90\x8C\x80ss", L"\xd800\xdf00ss", "#%F0%90%8C%80ss",
       Component(1, 14), true},
      // Escaping should be preserved unchanged, even invalid ones
      {"%41%a", L"%41%a", "#%41%a", Component(1, 5), true},
      // Invalid UTF-8/16 input should be flagged and the input made valid
      {"\xc2", nullptr, "#%EF%BF%BD", Component(1, 9), true},
      {nullptr, L"\xd800\x597d", "#%EF%BF%BD%E5%A5%BD", Component(1, 18), true},
      // Test a Unicode invalid character.
      {"a\xef\xb7\x90", L"a\xfdd0", "#a%EF%B7%90", Component(1, 10), true},
      // Refs can have # signs and we should preserve them.
      {"asdf#qwer", L"asdf#qwer", "#asdf#qwer", Component(1, 9), true},
      {"#asdf", L"#asdf", "##asdf", Component(1, 5), true},
  };

  for (const auto& ref_case : ref_cases) {
    // 8-bit input
    if (ref_case.input8) {
      int len = static_cast<int>(strlen(ref_case.input8));
      Component in_comp(0, len);
      Component out_comp;

      std::string out_str;
      StdStringCanonOutput output(&out_str);
      CanonicalizeRef(ref_case.input8, in_comp, &output, &out_comp);
      output.Complete();

      EXPECT_EQ(ref_case.expected_component.begin, out_comp.begin);
      EXPECT_EQ(ref_case.expected_component.len, out_comp.len);
      EXPECT_EQ(ref_case.expected, out_str);
    }

    // 16-bit input
    if (ref_case.input16) {
      std::u16string input16(
          test_utils::TruncateWStringToUTF16(ref_case.input16));
      int len = static_cast<int>(input16.length());
      Component in_comp(0, len);
      Component out_comp;

      std::string out_str;
      StdStringCanonOutput output(&out_str);
      CanonicalizeRef(input16.c_str(), in_comp, &output, &out_comp);
      output.Complete();

      EXPECT_EQ(ref_case.expected_component.begin, out_comp.begin);
      EXPECT_EQ(ref_case.expected_component.len, out_comp.len);
      EXPECT_EQ(ref_case.expected, out_str);
    }
  }

  // Try one with an embedded NULL. It should be stripped.
  const char null_input[5] = "ab\x00z";
  Component null_input_component(0, 4);
  Component out_comp;

  std::string out_str;
  StdStringCanonOutput output(&out_str);
  CanonicalizeRef(null_input, null_input_component, &output, &out_comp);
  output.Complete();

  EXPECT_EQ(1, out_comp.begin);
  EXPECT_EQ(6, out_comp.len);
  EXPECT_EQ("#ab%00z", out_str);
}

TEST_F(URLCanonTest, CanonicalizeStandardURL) {
  // The individual component canonicalize tests should have caught the cases
  // for each of those components. Here, we just need to test that the various
  // parts are included or excluded properly, and have the correct separators.
  // clang-format off
  struct URLCase {
    const char* input;
    const char* expected;
    bool expected_success;
  } cases[] = {
    {"http://www.google.com/foo?bar=baz#", "http://www.google.com/foo?bar=baz#",
     true},

      // Backslashes should get converted to forward slashes.
      {"http:\\\\www.google.com\\foo", "http://www.google.com/foo", true},

      // Busted refs shouldn't make the whole thing fail.
      {"http://www.google.com/asdf#\xc2",
       "http://www.google.com/asdf#%EF%BF%BD", true},

      // Basic port tests.
      {"http://foo:80/", "http://foo/", true},
      {"http://foo:81/", "http://foo:81/", true},
      {"httpa://foo:80/", "httpa://foo:80/", true},
      {"http://foo:-80/", "http://foo:-80/", false},

      {"https://foo:443/", "https://foo/", true},
      {"https://foo:80/", "https://foo:80/", true},
      {"ftp://foo:21/", "ftp://foo/", true},
      {"ftp://foo:80/", "ftp://foo:80/", true},
      {"gopher://foo:70/", "gopher://foo:70/", true},
      {"gopher://foo:443/", "gopher://foo:443/", true},
      {"ws://foo:80/", "ws://foo/", true},
      {"ws://foo:81/", "ws://foo:81/", true},
      {"ws://foo:443/", "ws://foo:443/", true},
      {"ws://foo:815/", "ws://foo:815/", true},
      {"wss://foo:80/", "wss://foo:80/", true},
      {"wss://foo:81/", "wss://foo:81/", true},
      {"wss://foo:443/", "wss://foo/", true},
      {"wss://foo:815/", "wss://foo:815/", true},

      // This particular code path ends up "backing up" to replace an invalid
      // host ICU generated with an escaped version. Test that in the context
      // of a full URL to make sure the backing up doesn't mess up the non-host
      // parts of the URL. "EF B9 AA" is U+FE6A which is a type of percent that
      // ICU will convert to an ASCII one, generating "%81".
      {"ws:)W\x1eW\xef\xb9\xaa"
       "81:80/",
       "ws://)w%1ew%81/", false},
      // Regression test for the last_invalid_percent_index bug described in
      // https://crbug.com/1080890#c10.
      {R"(HTTP:S/5%\../>%41)", "http://s/%3E%41", true},
  };
  // clang-format on

  for (const auto& i : cases) {
    Parsed parsed = ParseStandardURL(i.input);

    Parsed out_parsed;
    std::string out_str;
    StdStringCanonOutput output(&out_str);
    bool success = CanonicalizeStandardURL(
        i.input, parsed, SCHEME_WITH_HOST_PORT_AND_USER_INFORMATION, nullptr,
        &output, &out_parsed);
    output.Complete();

    EXPECT_EQ(i.expected_success, success);
    EXPECT_EQ(i.expected, out_str);
  }
}

TEST_F(URLCanonTest, CanonicalizeNonSpecialURL) {
  // The individual component canonicalize tests should have caught the cases
  // for each of those components. Here, we just need to test that the various
  // parts are included or excluded properly, and have the correct separators.
  struct URLCase {
    const std::string_view input;
    const std::string_view expected;
    bool expected_success;
  } cases[] = {
      // Basic cases.
      {"git://host:80/path?a=b#ref", "git://host:80/path?a=b#ref", true},
      {"git://host", "git://host", true},
      {"git://host/", "git://host/", true},
      {"git://HosT/", "git://HosT/", true},
      {"git://..", "git://..", true},
      {"git://../", "git://../", true},
      {"git://../..", "git://../", true},

      // Empty hosts.
      {"git://", "git://", true},
      {"git:///", "git:///", true},
      {"git:////", "git:////", true},
      {"git:///a", "git:///a", true},
      {"git:///a/../b", "git:///b", true},
      {"git:///..", "git:///", true},

      // No hosts.
      {"git:/", "git:/", true},
      {"git:/a", "git:/a", true},
      {"git:/a/../b", "git:/b", true},
      {"git:/..", "git:/", true},
      {"git:/../", "git:/", true},
      {"git:/../..", "git:/", true},
      {"git:/.//a", "git:/.//a", true},

      // Users.
      {"git://@host", "git://host", true},
      {"git:// @host", "git://%20@host", true},
      {"git://\\@host", "git://%5C@host", true},

      // Paths.
      {"git://host/path", "git://host/path", true},
      {"git://host/p ath", "git://host/p%20ath", true},
      {"git://host/a/../b", "git://host/b", true},
      {"git://host/..", "git://host/", true},
      {"git://host/../", "git://host/", true},
      {"git://host/../..", "git://host/", true},
      {"git://host/.", "git://host/", true},
      {"git://host/./", "git://host/", true},
      {"git://host/./.", "git://host/", true},
      // Backslashes.
      {"git://host/a\\..\\b", "git://host/a\\..\\b", true},

      // IPv6.
      {"git://[1:2:0:0:5:0:0:0]", "git://[1:2:0:0:5::]", true},
      {"git://[1:2:0:0:5:0:0:0]/", "git://[1:2:0:0:5::]/", true},
      {"git://[1:2:0:0:5:0:0:0]/path", "git://[1:2:0:0:5::]/path", true},

      // IPv4 is unsupported.
      {"git://127.00.0.1", "git://127.00.0.1", true},
      {"git://127.1000.0.1", "git://127.1000.0.1", true},

      // Invalid URLs.
      {"git://@", "git://", false},
      // Forbidden host code points.
      {"git://<", "git://", false},
      {"git:// /", "git:///", false},
      // Backslashes cannot be used as host terminators.
      {"git://host\\a/../b", "git://host/b", false},

      // Opaque paths.
      {"git:", "git:", true},
      {"git:opaque", "git:opaque", true},
      {"git:o p a q u e", "git:o p a q u e", true},
      {"git: <", "git: <", true},
      {"git:opaque/a/../b", "git:opaque/a/../b", true},
      {"git:opaque\\a\\..\\b", "git:opaque\\a\\..\\b", true},
      {"git:\\a", "git:\\a", true},
      // Like URNs.
      {"git:a:b:c:123", "git:a:b:c:123", true},
  };

  for (const auto& i : cases) {
    SCOPED_TRACE(i.input);
    Parsed parsed = ParseNonSpecialURL(i.input);
    Parsed out_parsed;
    std::string out_str;
    StdStringCanonOutput output(&out_str);
    bool success = CanonicalizeNonSpecialURL(
        i.input.data(), i.input.size(), parsed,
        /*query_converter=*/nullptr, output, out_parsed);
    output.Complete();
    EXPECT_EQ(success, i.expected_success);
    EXPECT_EQ(out_str, i.expected);
  }
}

TEST_F(URLCanonTest, CanonicalizeNonSpecialURLOutputParsed) {
  // Test that out_parsed is correctly set.
  struct URLCase {
    const std::string_view input;
    // Currently, test only host and length.
    Component expected_output_parsed_host;
    int expected_output_parsed_length;
  } cases[] = {
      {"git:", Component(), 4},
      {"git:opaque", Component(), 10},
      {"git:/", Component(), 5},
      {"git://", Component(6, 0), 6},
      {"git:///", Component(6, 0), 7},
      // The length of "[1:2:0:0:5::]" is 13.
      {"git://[1:2:0:0:5:0:0:0]/", Component(6, 13), 20},
  };

  for (const auto& i : cases) {
    SCOPED_TRACE(i.input);
    Parsed parsed = ParseNonSpecialURL(i.input);
    Parsed out_parsed;
    std::string unused_out_str;
    StdStringCanonOutput unused_output(&unused_out_str);
    bool success = CanonicalizeNonSpecialURL(
        i.input.data(), i.input.size(), parsed,
        /*query_converter=*/nullptr, unused_output, out_parsed);
    ASSERT_TRUE(success);
    EXPECT_EQ(out_parsed.host, i.expected_output_parsed_host);
    EXPECT_EQ(out_parsed.Length(), i.expected_output_parsed_length);
  }
}

// The codepath here is the same as for regular canonicalization, so we just
// need to test that things are replaced or not correctly.
TEST_F(URLCanonTest, ReplaceStandardURL) {
  ReplaceCase replace_cases[] = {
      // Common case of truncating the path.
      {"http://www.google.com/foo?bar=baz#ref", nullptr, nullptr, nullptr,
       nullptr, nullptr, "/", kDeleteComp, kDeleteComp,
       "http://www.google.com/"},
      // Replace everything
      {"http://a:b@google.com:22/foo;bar?baz@cat", "https", "me", "pw",
       "host.com", "99", "/path", "query", "ref",
       "https://me:pw@host.com:99/path?query#ref"},
      // Replace nothing
      {"http://a:b@google.com:22/foo?baz@cat", nullptr, nullptr, nullptr,
       nullptr, nullptr, nullptr, nullptr, nullptr,
       "http://a:b@google.com:22/foo?baz@cat"},
      // Replace scheme with filesystem. The result is garbage, but you asked
      // for it.
      {"http://a:b@google.com:22/foo?baz@cat", "filesystem", nullptr, nullptr,
       nullptr, nullptr, nullptr, nullptr, nullptr,
       "filesystem://a:b@google.com:22/foo?baz@cat"},
  };

  for (const auto& replace_case : replace_cases) {
    const ReplaceCase& cur = replace_case;
    Parsed parsed = ParseStandardURL(cur.base);

    Replacements<char> r;
    typedef Replacements<char> R;  // Clean up syntax.

    // Note that for the scheme we pass in a different clear function since
    // there is no function to clear the scheme.
    SetupReplComp(&R::SetScheme, &R::ClearRef, &r, cur.scheme);
    SetupReplComp(&R::SetUsername, &R::ClearUsername, &r, cur.username);
    SetupReplComp(&R::SetPassword, &R::ClearPassword, &r, cur.password);
    SetupReplComp(&R::SetHost, &R::ClearHost, &r, cur.host);
    SetupReplComp(&R::SetPort, &R::ClearPort, &r, cur.port);
    SetupReplComp(&R::SetPath, &R::ClearPath, &r, cur.path);
    SetupReplComp(&R::SetQuery, &R::ClearQuery, &r, cur.query);
    SetupReplComp(&R::SetRef, &R::ClearRef, &r, cur.ref);

    std::string out_str;
    StdStringCanonOutput output(&out_str);
    Parsed out_parsed;
    ReplaceStandardURL(replace_case.base, parsed, r,
                       SCHEME_WITH_HOST_PORT_AND_USER_INFORMATION, nullptr,
                       &output, &out_parsed);
    output.Complete();

    EXPECT_EQ(replace_case.expected, out_str);
  }

  // The path pointer should be ignored if the address is invalid.
  {
    const char src[] = "http://www.google.com/here_is_the_path";
    Parsed parsed = ParseStandardURL(src);

    // Replace the path to 0 length string. By using 1 as the string address,
    // the test should get an access violation if it tries to dereference it.
    Replacements<char> r;
    r.SetPath(reinterpret_cast<char*>(0x00000001), Component(0, 0));
    std::string out_str1;
    StdStringCanonOutput output1(&out_str1);
    Parsed new_parsed;
    ReplaceStandardURL(src, parsed, r,
                       SCHEME_WITH_HOST_PORT_AND_USER_INFORMATION, nullptr,
                       &output1, &new_parsed);
    output1.Complete();
    EXPECT_STREQ("http://www.google.com/", out_str1.c_str());

    // Same with an "invalid" path.
    r.SetPath(reinterpret_cast<char*>(0x00000001), Component());
    std::string out_str2;
    StdStringCanonOutput output2(&out_str2);
    ReplaceStandardURL(src, parsed, r,
                       SCHEME_WITH_HOST_PORT_AND_USER_INFORMATION, nullptr,
                       &output2, &new_parsed);
    output2.Complete();
    EXPECT_STREQ("http://www.google.com/", out_str2.c_str());
  }
}

TEST_F(URLCanonTest, ReplaceFileURL) {
  ReplaceCase replace_cases[] = {
      // Replace everything
      {"file:///C:/gaba?query#ref", nullptr, nullptr, nullptr, "filer", nullptr,
       "/foo", "b", "c", "file://filer/foo?b#c"},
      // Replace nothing
      {"file:///C:/gaba?query#ref", nullptr, nullptr, nullptr, nullptr, nullptr,
       nullptr, nullptr, nullptr, "file:///C:/gaba?query#ref"},
      {"file:///Y:", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
       nullptr, nullptr, "file:///Y:"},
      {"file:///Y:/", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
       nullptr, nullptr, "file:///Y:/"},
      {"file:///./Y", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
       nullptr, nullptr, "file:///Y"},
      {"file:///./Y:", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
       nullptr, nullptr, "file:///Y:"},
      // Clear non-path components (common)
      {"file:///C:/gaba?query#ref", nullptr, nullptr, nullptr, nullptr, nullptr,
       nullptr, kDeleteComp, kDeleteComp, "file:///C:/gaba"},
      // Replace path with something that doesn't begin with a slash and make
      // sure it gets added properly.
      {"file:///C:/gaba", nullptr, nullptr, nullptr, nullptr, nullptr,
       "interesting/", nullptr, nullptr, "file:///interesting/"},
      {"file:///home/gaba?query#ref", nullptr, nullptr, nullptr, "filer",
       nullptr, "/foo", "b", "c", "file://filer/foo?b#c"},
      {"file:///home/gaba?query#ref", nullptr, nullptr, nullptr, nullptr,
       nullptr, nullptr, nullptr, nullptr, "file:///home/gaba?query#ref"},
      {"file:///home/gaba?query#ref", nullptr, nullptr, nullptr, nullptr,
       nullptr, nullptr, kDeleteComp, kDeleteComp, "file:///home/gaba"},
      {"file:///home/gaba", nullptr, nullptr, nullptr, nullptr, nullptr,
       "interesting/", nullptr, nullptr, "file:///interesting/"},
      // Replace scheme -- shouldn't do anything.
      {"file:///C:/gaba?query#ref", "http", nullptr, nullptr, nullptr, nullptr,
       nullptr, nullptr, nullptr, "file:///C:/gaba?query#ref"},
  };

  for (const auto& replace_case : replace_cases) {
    const ReplaceCase& cur = replace_case;
    SCOPED_TRACE(cur.base);
    Parsed parsed = ParseFileURL(cur.base);

    Replacements<char> r;
    typedef Replacements<char> R;  // Clean up syntax.
    SetupReplComp(&R::SetScheme, &R::ClearRef, &r, cur.scheme);
    SetupReplComp(&R::SetUsername, &R::ClearUsername, &r, cur.username);
    SetupReplComp(&R::SetPassword, &R::ClearPassword, &r, cur.password);
    SetupReplComp(&R::SetHost, &R::ClearHost, &r, cur.host);
    SetupReplComp(&R::SetPort, &R::ClearPort, &r, cur.port);
    SetupReplComp(&R::SetPath, &R::ClearPath, &r, cur.path);
    SetupReplComp(&R::SetQuery, &R::ClearQuery, &r, cur.query);
    SetupReplComp(&R::SetRef, &R::ClearRef, &r, cur.ref);

    std::string out_str;
    StdStringCanonOutput output(&out_str);
    Parsed out_parsed;
    ReplaceFileURL(cur.base, parsed, r, nullptr, &output, &out_parsed);
    output.Complete();

    EXPECT_EQ(replace_case.expected, out_str);
  }
}

TEST_F(URLCanonTest, ReplaceFileSystemURL) {
  ReplaceCase replace_cases[] = {
      // Replace everything in the outer URL.
      {"filesystem:file:///temporary/gaba?query#ref", nullptr, nullptr, nullptr,
       nullptr, nullptr, "/foo", "b", "c",
       "filesystem:file:///temporary/foo?b#c"},
      // Replace nothing
      {"filesystem:file:///temporary/gaba?query#ref", nullptr, nullptr, nullptr,
       nullptr, nullptr, nullptr, nullptr, nullptr,
       "filesystem:file:///temporary/gaba?query#ref"},
      // Clear non-path components (common)
      {"filesystem:file:///temporary/gaba?query#ref", nullptr, nullptr, nullptr,
       nullptr, nullptr, nullptr, kDeleteComp, kDeleteComp,
       "filesystem:file:///temporary/gaba"},
      // Replace path with something that doesn't begin with a slash and make
      // sure it gets added properly.
      {"filesystem:file:///temporary/gaba?query#ref", nullptr, nullptr, nullptr,
       nullptr, nullptr, "interesting/", nullptr, nullptr,
       "filesystem:file:///temporary/interesting/?query#ref"},
      // Replace scheme -- shouldn't do anything except canonicalize.
      {"filesystem:http://u:p@bar.com/t/gaba?query#ref", "http", nullptr,
       nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
       "filesystem:http://bar.com/t/gaba?query#ref"},
      // Replace username -- shouldn't do anything except canonicalize.
      {"filesystem:http://u:p@bar.com/t/gaba?query#ref", nullptr, "u2", nullptr,
       nullptr, nullptr, nullptr, nullptr, nullptr,
       "filesystem:http://bar.com/t/gaba?query#ref"},
      // Replace password -- shouldn't do anything except canonicalize.
      {"filesystem:http://u:p@bar.com/t/gaba?query#ref", nullptr, nullptr,
       "pw2", nullptr, nullptr, nullptr, nullptr, nullptr,
       "filesystem:http://bar.com/t/gaba?query#ref"},
      // Replace host -- shouldn't do anything except canonicalize.
      {"filesystem:http://u:p@bar.com:80/t/gaba?query#ref", nullptr, nullptr,
       nullptr, "foo.com", nullptr, nullptr, nullptr, nullptr,
       "filesystem:http://bar.com/t/gaba?query#ref"},
      // Replace port -- shouldn't do anything except canonicalize.
      {"filesystem:http://u:p@bar.com:40/t/gaba?query#ref", nullptr, nullptr,
       nullptr, nullptr, "41", nullptr, nullptr, nullptr,
       "filesystem:http://bar.com:40/t/gaba?query#ref"},
  };

  for (const auto& replace_case : replace_cases) {
    const ReplaceCase& cur = replace_case;
    Parsed parsed = ParseFileSystemURL(cur.base);

    Replacements<char> r;
    typedef Replacements<char> R;  // Clean up syntax.
    SetupReplComp(&R::SetScheme, &R::ClearRef, &r, cur.scheme);
    SetupReplComp(&R::SetUsername, &R::ClearUsername, &r, cur.username);
    SetupReplComp(&R::SetPassword, &R::ClearPassword, &r, cur.password);
    SetupReplComp(&R::SetHost, &R::ClearHost, &r, cur.host);
    SetupReplComp(&R::SetPort, &R::ClearPort, &r, cur.port);
    SetupReplComp(&R::SetPath, &R::ClearPath, &r, cur.path);
    SetupReplComp(&R::SetQuery, &R::ClearQuery, &r, cur.query);
    SetupReplComp(&R::SetRef, &R::ClearRef, &r, cur.ref);

    std::string out_str;
    StdStringCanonOutput output(&out_str);
    Parsed out_parsed;
    ReplaceFileSystemURL(cur.base, parsed, r, nullptr, &output, &out_parsed);
    output.Complete();

    EXPECT_EQ(replace_case.expected, out_str);
  }
}

TEST_F(URLCanonTest, ReplacePathURL) {
  ReplaceCase replace_cases[] = {
      // Replace everything
      {"data:foo", "javascript", nullptr, nullptr, nullptr, nullptr,
       "alert('foo?');", nullptr, nullptr, "javascript:alert('foo?');"},
      // Replace nothing
      {"data:foo", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
       nullptr, nullptr, "data:foo"},
      // Replace one or the other
      {"data:foo", "javascript", nullptr, nullptr, nullptr, nullptr, nullptr,
       nullptr, nullptr, "javascript:foo"},
      {"data:foo", nullptr, nullptr, nullptr, nullptr, nullptr, "bar", nullptr,
       nullptr, "data:bar"},
      {"data:foo", nullptr, nullptr, nullptr, nullptr, nullptr, kDeleteComp,
       nullptr, nullptr, "data:"},
  };

  for (const auto& replace_case : replace_cases) {
    const ReplaceCase& cur = replace_case;

    Replacements<char> r;
    typedef Replacements<char> R;  // Clean up syntax.
    SetupReplComp(&R::SetScheme, &R::ClearRef, &r, cur.scheme);
    SetupReplComp(&R::SetUsername, &R::ClearUsername, &r, cur.username);
    SetupReplComp(&R::SetPassword, &R::ClearPassword, &r, cur.password);
    SetupReplComp(&R::SetHost, &R::ClearHost, &r, cur.host);
    SetupReplComp(&R::SetPort, &R::ClearPort, &r, cur.port);
    SetupReplComp(&R::SetPath, &R::ClearPath, &r, cur.path);
    SetupReplComp(&R::SetQuery, &R::ClearQuery, &r, cur.query);
    SetupReplComp(&R::SetRef, &R::ClearRef, &r, cur.ref);

    std::string out_str;
    StdStringCanonOutput output(&out_str);
    Parsed out_parsed;
    ReplacePathURL(cur.base, ParsePathURL(cur.base, false), r, &output,
                   &out_parsed);
    output.Complete();

    EXPECT_EQ(replace_case.expected, out_str);
  }
}

TEST_F(URLCanonTest, ReplaceMailtoURL) {
  ReplaceCase replace_cases[] = {
      // Replace everything
      {"mailto:jon@foo.com?body=sup", "mailto", nullptr, nullptr, nullptr,
       nullptr, "addr1", "to=tony", nullptr, "mailto:addr1?to=tony"},
      // Replace nothing
      {"mailto:jon@foo.com?body=sup", nullptr, nullptr, nullptr, nullptr,
       nullptr, nullptr, nullptr, nullptr, "mailto:jon@foo.com?body=sup"},
      // Replace the path
      {"mailto:jon@foo.com?body=sup", nullptr, nullptr, nullptr, nullptr,
       nullptr, "jason", nullptr, nullptr, "mailto:jason?body=sup"},
      // Replace the query
      {"mailto:jon@foo.com?body=sup", nullptr, nullptr, nullptr, nullptr,
       nullptr, nullptr, "custom=1", nullptr, "mailto:jon@foo.com?custom=1"},
      // Replace the path and query
      {"mailto:jon@foo.com?body=sup", nullptr, nullptr, nullptr, nullptr,
       nullptr, "jason", "custom=1", nullptr, "mailto:jason?custom=1"},
      // Set the query to empty (should leave trailing question mark)
      {"mailto:jon@foo.com?body=sup", nullptr, nullptr, nullptr, nullptr,
       nullptr, nullptr, "", nullptr, "mailto:jon@foo.com?"},
      // Clear the query
      {"mailto:jon@foo.com?body=sup", nullptr, nullptr, nullptr, nullptr,
       nullptr, nullptr, "|", nullptr, "mailto:jon@foo.com"},
      // Clear the path
      {"mailto:jon@foo.com?body=sup", nullptr, nullptr, nullptr, nullptr,
       nullptr, "|", nullptr, nullptr, "mailto:?body=sup"},
      // Clear the path + query
      {"mailto:", nullptr, nullptr, nullptr, nullptr, nullptr, "|", "|",
       nullptr, "mailto:"},
      // Setting the ref should have no effect
      {"mailto:addr1", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
       nullptr, "BLAH", "mailto:addr1"},
  };

  for (const auto& replace_case : replace_cases) {
    const ReplaceCase& cur = replace_case;
    Parsed parsed = ParseMailtoURL(cur.base);

    Replacements<char> r;
    typedef Replacements<char> R;
    SetupReplComp(&R::SetScheme, &R::ClearRef, &r, cur.scheme);
    SetupReplComp(&R::SetUsername, &R::ClearUsername, &r, cur.username);
    SetupReplComp(&R::SetPassword, &R::ClearPassword, &r, cur.password);
    SetupReplComp(&R::SetHost, &R::ClearHost, &r, cur.host);
    SetupReplComp(&R::SetPort, &R::ClearPort, &r, cur.port);
    SetupReplComp(&R::SetPath, &R::ClearPath, &r, cur.path);
    SetupReplComp(&R::SetQuery, &R::ClearQuery, &r, cur.query);
    SetupReplComp(&R::SetRef, &R::ClearRef, &r, cur.ref);

    std::string out_str;
    StdStringCanonOutput output(&out_str);
    Parsed out_parsed;
    ReplaceMailtoURL(cur.base, parsed, r, &output, &out_parsed);
    output.Complete();

    EXPECT_EQ(replace_case.expected, out_str);
  }
}

TEST_F(URLCanonTest, CanonicalizeFileURL) {
  struct URLCase {
    const char* input;
    const char* expected;
    bool expected_success;
    Component expected_host;
    Component expected_path;
  } cases[] = {
#ifdef _WIN32
      // Windows-style paths
      {"file:c:\\foo\\bar.html", "file:///C:/foo/bar.html", true, Component(),
       Component(7, 16)},
      {"  File:c|////foo\\bar.html", "file:///C:////foo/bar.html", true,
       Component(), Component(7, 19)},
      {"file:", "file:///", true, Component(), Component(7, 1)},
      {"file:UNChost/path", "file://unchost/path", true, Component(7, 7),
       Component(14, 5)},
      // CanonicalizeFileURL supports absolute Windows style paths for IE
      // compatibility. Note that the caller must decide that this is a file
      // URL itself so it can call the file canonicalizer. This is usually
      // done automatically as part of relative URL resolving.
      {"c:\\foo\\bar", "file:///C:/foo/bar", true, Component(),
       Component(7, 11)},
      {"C|/foo/bar", "file:///C:/foo/bar", true, Component(), Component(7, 11)},
      {"/C|\\foo\\bar", "file:///C:/foo/bar", true, Component(),
       Component(7, 11)},
      {"//C|/foo/bar", "file:///C:/foo/bar", true, Component(),
       Component(7, 11)},
      {"//server/file", "file://server/file", true, Component(7, 6),
       Component(13, 5)},
      {"\\\\server\\file", "file://server/file", true, Component(7, 6),
       Component(13, 5)},
      {"/\\server/file", "file://server/file", true, Component(7, 6),
       Component(13, 5)},
      // We should preserve the number of slashes after the colon for IE
      // compatibility, except when there is none, in which case we should
      // add one.
      {"file:c:foo/bar.html", "file:///C:/foo/bar.html", true, Component(),
       Component(7, 16)},
      {"file:/\\/\\C:\\\\//foo\\bar.html", "file:///C:////foo/bar.html", true,
       Component(), Component(7, 19)},
      // Three slashes should be non-UNC, even if there is no drive spec (IE
      // does this, which makes the resulting request invalid).
      {"file:///foo/bar.txt", "file:///foo/bar.txt", true, Component(),
       Component(7, 12)},
      // TODO(brettw) we should probably fail for invalid host names, which
      // would change the expected result on this test. We also currently allow
      // colon even though it's probably invalid, because its currently the
      // "natural" result of the way the canonicalizer is written. There doesn't
      // seem to be a strong argument for why allowing it here would be bad, so
      // we just tolerate it and the load will fail later.
      {"FILE:/\\/\\7:\\\\//foo\\bar.html", "file://7:////foo/bar.html", false,
       Component(7, 2), Component(9, 16)},
      {"file:filer/home\\me", "file://filer/home/me", true, Component(7, 5),
       Component(12, 8)},
      // Make sure relative paths can't go above the "C:"
      {"file:///C:/foo/../../../bar.html", "file:///C:/bar.html", true,
       Component(), Component(7, 12)},
      // Busted refs shouldn't make the whole thing fail.
      {"file:///C:/asdf#\xc2", "file:///C:/asdf#%EF%BF%BD", true, Component(),
       Component(7, 8)},
      {"file:///./s:", "file:///S:", true, Component(), Component(7, 3)},
#else
      // Unix-style paths
      {"file:///home/me", "file:///home/me", true, Component(),
       Component(7, 8)},
      // Windowsy ones should get still treated as Unix-style.
      {"file:c:\\foo\\bar.html", "file:///c:/foo/bar.html", true, Component(),
       Component(7, 16)},
      {"file:c|//foo\\bar.html", "file:///c%7C//foo/bar.html", true,
       Component(), Component(7, 19)},
      {"file:///./s:", "file:///s:", true, Component(), Component(7, 3)},
      // file: tests from WebKit (LayoutTests/fast/loader/url-parse-1.html)
      {"//", "file:///", true, Component(), Component(7, 1)},
      {"///", "file:///", true, Component(), Component(7, 1)},
      {"///test", "file:///test", true, Component(), Component(7, 5)},
      {"file://test", "file://test/", true, Component(7, 4), Component(11, 1)},
      {"file://localhost", "file://localhost/", true, Component(7, 9),
       Component(16, 1)},
      {"file://localhost/", "file://localhost/", true, Component(7, 9),
       Component(16, 1)},
      {"file://localhost/test", "file://localhost/test", true, Component(7, 9),
       Component(16, 5)},
#endif  // _WIN32
  };

  for (const auto& i : cases) {
    Parsed parsed = ParseFileURL(i.input);

    Parsed out_parsed;
    std::string out_str;
    StdStringCanonOutput output(&out_str);
    bool success =
        CanonicalizeFileURL(i.input, static_cast<int>(strlen(i.input)), parsed,
                            nullptr, &output, &out_parsed);
    output.Complete();

    EXPECT_EQ(i.expected_success, success);
    EXPECT_EQ(i.expected, out_str);

    // Make sure the spec was properly identified, the file canonicalizer has
    // different code for writing the spec.
    EXPECT_EQ(0, out_parsed.scheme.begin);
    EXPECT_EQ(4, out_parsed.scheme.len);

    EXPECT_EQ(i.expected_host.begin, out_parsed.host.begin);
    EXPECT_EQ(i.expected_host.len, out_parsed.host.len);

    EXPECT_EQ(i.expected_path.begin, out_parsed.path.begin);
    EXPECT_EQ(i.expected_path.len, out_parsed.path.len);
  }
}

TEST_F(URLCanonTest, CanonicalizeFileSystemURL) {
  struct URLCase {
    const char* input;
    const char* expected;
    bool expected_success;
  } cases[] = {
      {"Filesystem:htTp://www.Foo.com:80/tempoRary",
       "filesystem:http://www.foo.com/tempoRary/", true},
      {"filesystem:httpS://www.foo.com/temporary/",
       "filesystem:https://www.foo.com/temporary/", true},
      {"filesystem:http://www.foo.com//", "filesystem:http://www.foo.com//",
       false},
      {"filesystem:http://www.foo.com/persistent/bob?query#ref",
       "filesystem:http://www.foo.com/persistent/bob?query#ref", true},
      {"filesystem:fIle://\\temporary/", "filesystem:file:///temporary/", true},
      {"filesystem:fiLe:///temporary", "filesystem:file:///temporary/", true},
      {"filesystem:File:///temporary/Bob?qUery#reF",
       "filesystem:file:///temporary/Bob?qUery#reF", true},
      {"FilEsysteM:htTp:E=/.", "filesystem:http://e=//", false},
  };

  for (const auto& i : cases) {
    Parsed parsed = ParseFileSystemURL(i.input);

    Parsed out_parsed;
    std::string out_str;
    StdStringCanonOutput output(&out_str);
    bool success = CanonicalizeFileSystemURL(i.input, parsed, nullptr, &output,
                                             &out_parsed);
    output.Complete();

    EXPECT_EQ(i.expected_success, success);
    EXPECT_EQ(i.expected, out_str);

    // Make sure the spec was properly identified, the filesystem canonicalizer
    // has different code for writing the spec.
    EXPECT_EQ(0, out_parsed.scheme.begin);
    EXPECT_EQ(10, out_parsed.scheme.len);
    if (success)
      EXPECT_GT(out_parsed.path.len, 0);
  }
}

TEST_F(URLCanonTest, CanonicalizePathURL) {
  // Path URLs should get canonicalized schemes but nothing else.
  struct PathCase {
    const char* input;
    const char* expected;
  } path_cases[] = {
      {"javascript:", "javascript:"},
      {"JavaScript:Foo", "javascript:Foo"},
      {"Foo:\":This /is interesting;?#", "foo:\":This /is interesting;?#"},

      // Unicode invalid characters should not cause failure. See
      // https://crbug.com/925614.
      {"javascript:\uFFFF", "javascript:%EF%BF%BF"},
  };

  for (const auto& path_case : path_cases) {
    int url_len = static_cast<int>(strlen(path_case.input));

    Parsed out_parsed;
    std::string out_str;
    StdStringCanonOutput output(&out_str);
    bool success = CanonicalizePathURL(path_case.input, url_len,
                                       ParsePathURL(path_case.input, true),
                                       &output, &out_parsed);
    output.Complete();

    EXPECT_TRUE(success);
    EXPECT_EQ(path_case.expected, out_str);

    EXPECT_EQ(0, out_parsed.host.begin);
    EXPECT_EQ(-1, out_parsed.host.len);

    // When we end with a colon at the end, there should be no path.
    if (path_case.input[url_len - 1] == ':') {
      EXPECT_EQ(0, out_parsed.GetContent().begin);
      EXPECT_EQ(-1, out_parsed.GetContent().len);
    }
  }
}

TEST_F(URLCanonTest, CanonicalizePathURLPath) {
  struct PathCase {
    std::string input;
    std::wstring input16;
    std::string expected;
  } path_cases[] = {
      {"Foo", L"Foo", "Foo"},
      {"\":This /is interesting;?#", L"\":This /is interesting;?#",
       "\":This /is interesting;?#"},
      {"\uFFFF", L"\uFFFF", "%EF%BF%BF"},
  };

  for (const auto& path_case : path_cases) {
    // 8-bit string input
    std::string out_str;
    StdStringCanonOutput output(&out_str);
    url::Component out_component;
    CanonicalizePathURLPath(path_case.input.data(),
                            Component(0, path_case.input.size()), &output,
                            &out_component);
    output.Complete();

    EXPECT_EQ(path_case.expected, out_str);

    EXPECT_EQ(0, out_component.begin);
    EXPECT_EQ(path_case.expected.size(),
              static_cast<size_t>(out_component.len));

    // 16-bit string input
    std::string out_str16;
    StdStringCanonOutput output16(&out_str16);
    url::Component out_component16;
    std::u16string input16(
        test_utils::TruncateWStringToUTF16(path_case.input16.data()));
    CanonicalizePathURLPath(input16.c_str(),
                            Component(0, path_case.input16.size()), &output16,
                            &out_component16);
    output16.Complete();

    EXPECT_EQ(path_case.expected, out_str16);

    EXPECT_EQ(0, out_component16.begin);
    EXPECT_EQ(path_case.expected.size(),
              static_cast<size_t>(out_component16.len));
  }
}

TEST_F(URLCanonTest, CanonicalizeMailtoURL) {
  struct URLCase {
    const char* input;
    const char* expected;
    bool expected_success;
    Component expected_path;
    Component expected_query;
  } cases[] = {
    // Null character should be escaped to %00.
    // Keep this test first in the list as it is handled specially below.
    {"mailto:addr1\0addr2?foo",
     "mailto:addr1%00addr2?foo",
     true, Component(7, 13), Component(21, 3)},
    {"mailto:addr1",
     "mailto:addr1",
     true, Component(7, 5), Component()},
    {"mailto:addr1@foo.com",
     "mailto:addr1@foo.com",
     true, Component(7, 13), Component()},
    // Trailing whitespace is stripped.
    {"MaIlTo:addr1 \t ",
     "mailto:addr1",
     true, Component(7, 5), Component()},
    {"MaIlTo:addr1?to=jon",
     "mailto:addr1?to=jon",
     true, Component(7, 5), Component(13,6)},
    {"mailto:addr1,addr2",
     "mailto:addr1,addr2",
     true, Component(7, 11), Component()},
    // Embedded spaces must be encoded.
    {"mailto:addr1, addr2",
     "mailto:addr1,%20addr2",
     true, Component(7, 14), Component()},
    {"mailto:addr1, addr2?subject=one two ",
     "mailto:addr1,%20addr2?subject=one%20two",
     true, Component(7, 14), Component(22, 17)},
    {"mailto:addr1%2caddr2",
     "mailto:addr1%2caddr2",
     true, Component(7, 13), Component()},
    {"mailto:\xF0\x90\x8C\x80",
     "mailto:%F0%90%8C%80",
     true, Component(7, 12), Component()},
    // Invalid -- UTF-8 encoded surrogate value.
    {"mailto:\xed\xa0\x80",
     "mailto:%EF%BF%BD%EF%BF%BD%EF%BF%BD",
     false, Component(7, 27), Component()},
    {"mailto:addr1?",
     "mailto:addr1?",
     true, Component(7, 5), Component(13, 0)},
    // Certain characters have special meanings and must be encoded.
    {"mailto:! \x22$&()+,-./09:;<=>@AZ[\\]&_`az{|}~\x7f?Query! \x22$&()+,-./09:;<=>@AZ[\\]&_`az{|}~",
     "mailto:!%20%22$&()+,-./09:;%3C=%3E@AZ[\\]&_%60az%7B%7C%7D~%7F?Query!%20%22$&()+,-./09:;%3C=%3E@AZ[\\]&_`az{|}~",
     true, Component(7, 53), Component(61, 47)},
  };

  // Define outside of loop to catch bugs where components aren't reset
  Parsed out_parsed;

  for (size_t i = 0; i < std::size(cases); i++) {
    int url_len = static_cast<int>(strlen(cases[i].input));
    if (i == 0) {
      // The first test case purposely has a '\0' in it -- don't count it
      // as the string terminator.
      url_len = 22;
    }

    std::string out_str;
    StdStringCanonOutput output(&out_str);
    bool success = CanonicalizeMailtoURL(
        cases[i].input, url_len,
        ParseMailtoURL(std::string_view(cases[i].input, url_len)), &output,
        &out_parsed);
    output.Complete();

    EXPECT_EQ(cases[i].expected_success, success);
    EXPECT_EQ(cases[i].expected, out_str);

    // Make sure the spec was properly identified
    EXPECT_EQ(0, out_parsed.scheme.begin);
    EXPECT_EQ(6, out_parsed.scheme.len);

    EXPECT_EQ(cases[i].expected_path.begin, out_parsed.path.begin);
    EXPECT_EQ(cases[i].expected_path.len, out_parsed.path.len);

    EXPECT_EQ(cases[i].expected_query.begin, out_parsed.query.begin);
    EXPECT_EQ(cases[i].expected_query.len, out_parsed.query.len);
  }
}

#ifndef WIN32

TEST_F(URLCanonTest, _itoa_s) {
  // We fill the buffer with 0xff to ensure that it's getting properly
  // null-terminated. We also allocate one byte more than what we tell
  // _itoa_s about, and ensure that the extra byte is untouched.
  char buf[6];
  memset(buf, 0xff, sizeof(buf));
  EXPECT_EQ(0, _itoa_s(12, buf, sizeof(buf) - 1, 10));
  EXPECT_STREQ("12", buf);
  EXPECT_EQ('\xFF', buf[3]);

  // Test the edge cases - exactly the buffer size and one over
  memset(buf, 0xff, sizeof(buf));
  EXPECT_EQ(0, _itoa_s(1234, buf, sizeof(buf) - 1, 10));
  EXPECT_STREQ("1234", buf);
  EXPECT_EQ('\xFF', buf[5]);

  memset(buf, 0xff, sizeof(buf));
  EXPECT_EQ(EINVAL, _itoa_s(12345, buf, sizeof(buf) - 1, 10));
  EXPECT_EQ('\xFF', buf[5]);  // should never write to this location

  // Test the template overload (note that this will see the full buffer)
  memset(buf, 0xff, sizeof(buf));
  EXPECT_EQ(0, _itoa_s(12, buf, 10));
  EXPECT_STREQ("12", buf);
  EXPECT_EQ('\xFF', buf[3]);

  memset(buf, 0xff, sizeof(buf));
  EXPECT_EQ(0, _itoa_s(12345, buf, 10));
  EXPECT_STREQ("12345", buf);

  EXPECT_EQ(EINVAL, _itoa_s(123456, buf, 10));

  // Test that radix 16 is supported.
  memset(buf, 0xff, sizeof(buf));
  EXPECT_EQ(0, _itoa_s(1234, buf, sizeof(buf) - 1, 16));
  EXPECT_STREQ("4d2", buf);
  EXPECT_EQ('\xFF', buf[5]);
}

TEST_F(URLCanonTest, _itow_s) {
  // We fill the buffer with 0xff to ensure that it's getting properly
  // null-terminated. We also allocate one byte more than what we tell
  // _itoa_s about, and ensure that the extra byte is untouched.
  char16_t buf[6];
  const char fill_mem = 0xff;
  const char16_t fill_char = 0xffff;
  memset(buf, fill_mem, sizeof(buf));
  EXPECT_EQ(0, _itow_s(12, buf, sizeof(buf) / 2 - 1, 10));
  EXPECT_EQ(u"12", std::u16string(buf));
  EXPECT_EQ(fill_char, buf[3]);

  // Test the edge cases - exactly the buffer size and one over
  EXPECT_EQ(0, _itow_s(1234, buf, sizeof(buf) / 2 - 1, 10));
  EXPECT_EQ(u"1234", std::u16string(buf));
  EXPECT_EQ(fill_char, buf[5]);

  memset(buf, fill_mem, sizeof(buf));
  EXPECT_EQ(EINVAL, _itow_s(12345, buf, sizeof(buf) / 2 - 1, 10));
  EXPECT_EQ(fill_char, buf[5]);  // should never write to this location

  // Test the template overload (note that this will see the full buffer)
  memset(buf, fill_mem, sizeof(buf));
  EXPECT_EQ(0, _itow_s(12, buf, 10));
  EXPECT_EQ(u"12", std::u16string(buf));
  EXPECT_EQ(fill_char, buf[3]);

  memset(buf, fill_mem, sizeof(buf));
  EXPECT_EQ(0, _itow_s(12345, buf, 10));
  EXPECT_EQ(u"12345", std::u16string(buf));

  EXPECT_EQ(EINVAL, _itow_s(123456, buf, 10));
}

#endif  // !WIN32

// Returns true if the given two structures are the same.
static bool ParsedIsEqual(const Parsed& a, const Parsed& b) {
  return a.scheme.begin == b.scheme.begin && a.scheme.len == b.scheme.len &&
         a.username.begin == b.username.begin && a.username.len == b.username.len &&
         a.password.begin == b.password.begin && a.password.len == b.password.len &&
         a.host.begin == b.host.begin && a.host.len == b.host.len &&
         a.port.begin == b.port.begin && a.port.len == b.port.len &&
         a.path.begin == b.path.begin && a.path.len == b.path.len &&
         a.query.begin == b.query.begin && a.query.len == b.query.len &&
         a.ref.begin == b.ref.begin && a.ref.len == b.ref.len;
}

TEST_F(URLCanonTest, ResolveRelativeURL) {
  struct RelativeCase {
    const char* base;      // Input base URL: MUST BE CANONICAL
    bool is_base_hier;     // Is the base URL hierarchical
    bool is_base_file;     // Tells us if the base is a file URL.
    const char* test;      // Input URL to test against.
    bool succeed_relative; // Whether we expect IsRelativeURL to succeed
    bool is_rel;           // Whether we expect |test| to be relative or not.
    bool succeed_resolve;  // Whether we expect ResolveRelativeURL to succeed.
    const char* resolved;  // What we expect in the result when resolving.
  } rel_cases[] = {
      // Basic absolute input.
      {"http://host/a", true, false, "http://another/", true, false, false,
       nullptr},
      {"http://host/a", true, false, "http:////another/", true, false, false,
       nullptr},
      // Empty relative URLs should only remove the ref part of the URL,
      // leaving the rest unchanged.
      {"http://foo/bar", true, false, "", true, true, true, "http://foo/bar"},
      {"http://foo/bar#ref", true, false, "", true, true, true,
       "http://foo/bar"},
      {"http://foo/bar#", true, false, "", true, true, true, "http://foo/bar"},
      // Spaces at the ends of the relative path should be ignored.
      {"http://foo/bar", true, false, "  another  ", true, true, true,
       "http://foo/another"},
      {"http://foo/bar", true, false, "  .  ", true, true, true, "http://foo/"},
      {"http://foo/bar", true, false, " \t ", true, true, true,
       "http://foo/bar"},
      // Matching schemes without two slashes are treated as relative.
      {"http://host/a", true, false, "http:path", true, true, true,
       "http://host/path"},
      {"http://host/a/", true, false, "http:path", true, true, true,
       "http://host/a/path"},
      {"http://host/a", true, false, "http:/path", true, true, true,
       "http://host/path"},
      {"http://host/a", true, false, "HTTP:/path", true, true, true,
       "http://host/path"},
      // Nonmatching schemes are absolute.
      {"http://host/a", true, false, "https:host2", true, false, false,
       nullptr},
      {"http://host/a", true, false, "htto:/host2", true, false, false,
       nullptr},
      // Absolute path input
      {"http://host/a", true, false, "/b/c/d", true, true, true,
       "http://host/b/c/d"},
      {"http://host/a", true, false, "\\b\\c\\d", true, true, true,
       "http://host/b/c/d"},
      {"http://host/a", true, false, "/b/../c", true, true, true,
       "http://host/c"},
      {"http://host/a?b#c", true, false, "/b/../c", true, true, true,
       "http://host/c"},
      {"http://host/a", true, false, "\\b/../c?x#y", true, true, true,
       "http://host/c?x#y"},
      {"http://host/a?b#c", true, false, "/b/../c?x#y", true, true, true,
       "http://host/c?x#y"},
      // Relative path input
      {"http://host/a", true, false, "b", true, true, true, "http://host/b"},
      {"http://host/a", true, false, "bc/de", true, true, true,
       "http://host/bc/de"},
      {"http://host/a/", true, false, "bc/de?query#ref", true, true, true,
       "http://host/a/bc/de?query#ref"},
      {"http://host/a/", true, false, ".", true, true, true, "http://host/a/"},
      {"http://host/a/", true, false, "..", true, true, true, "http://host/"},
      {"http://host/a/", true, false, "./..", true, true, true, "http://host/"},
      {"http://host/a/", true, false, "../.", true, true, true, "http://host/"},
      {"http://host/a/", true, false, "././.", true, true, true,
       "http://host/a/"},
      {"http://host/a?query#ref", true, false, "../../../foo", true, true, true,
       "http://host/foo"},
      // Query input
      {"http://host/a", true, false, "?foo=bar", true, true, true,
       "http://host/a?foo=bar"},
      {"http://host/a?x=y#z", true, false, "?", true, true, true,
       "http://host/a?"},
      {"http://host/a?x=y#z", true, false, "?foo=bar#com", true, true, true,
       "http://host/a?foo=bar#com"},
      // Ref input
      {"http://host/a", true, false, "#ref", true, true, true,
       "http://host/a#ref"},
      {"http://host/a#b", true, false, "#", true, true, true, "http://host/a#"},
      {"http://host/a?foo=bar#hello", true, false, "#bye", true, true, true,
       "http://host/a?foo=bar#bye"},
      // Non-hierarchical base: no relative handling. Relative input should
      // error, and if a scheme is present, it should be treated as absolute.
      {"data:foobar", false, false, "baz.html", false, false, false, nullptr},
      {"data:foobar", false, false, "data:baz", true, false, false, nullptr},
      {"data:foobar", false, false, "data:/base", true, false, false, nullptr},
      // Non-hierarchical base: absolute input should succeed.
      {"data:foobar", false, false, "http://host/", true, false, false,
       nullptr},
      {"data:foobar", false, false, "http:host", true, false, false, nullptr},
      // Non-hierarchical base: empty URL should give error.
      {"data:foobar", false, false, "", false, false, false, nullptr},
      // Invalid schemes should be treated as relative.
      {"http://foo/bar", true, false, "./asd:fgh", true, true, true,
       "http://foo/asd:fgh"},
      {"http://foo/bar", true, false, ":foo", true, true, true,
       "http://foo/:foo"},
      {"http://foo/bar", true, false, " hello world", true, true, true,
       "http://foo/hello%20world"},
      {"data:asdf", false, false, ":foo", false, false, false, nullptr},
      {"data:asdf", false, false, "bad(':foo')", false, false, false, nullptr},
      // We should treat semicolons like any other character in URL resolving
      {"http://host/a", true, false, ";foo", true, true, true,
       "http://host/;foo"},
      {"http://host/a;", true, false, ";foo", true, true, true,
       "http://host/;foo"},
      {"http://host/a", true, false, ";/../bar", true, true, true,
       "http://host/bar"},
      // Relative URLs can also be written as "//foo/bar" which is relative to
      // the scheme. In this case, it would take the old scheme, so for http
      // the example would resolve to "http://foo/bar".
      {"http://host/a", true, false, "//another", true, true, true,
       "http://another/"},
      {"http://host/a", true, false, "//another/path?query#ref", true, true,
       true, "http://another/path?query#ref"},
      {"http://host/a", true, false, "///another/path", true, true, true,
       "http://another/path"},
      {"http://host/a", true, false, "//Another\\path", true, true, true,
       "http://another/path"},
      {"http://host/a", true, false, "//", true, true, false, "http:"},
      // IE will also allow one or the other to be a backslash to get the same
      // behavior.
      {"http://host/a", true, false, "\\/another/path", true, true, true,
       "http://another/path"},
      {"http://host/a", true, false, "/\\Another\\path", true, true, true,
       "http://another/path"},
#ifdef WIN32
      // Resolving against Windows file base URLs.
      {"file:///C:/foo", true, true, "http://host/", true, false, false,
       nullptr},
      {"file:///C:/foo", true, true, "bar", true, true, true, "file:///C:/bar"},
      {"file:///C:/foo", true, true, "../../../bar.html", true, true, true,
       "file:///C:/bar.html"},
      {"file:///C:/foo", true, true, "/../bar.html", true, true, true,
       "file:///C:/bar.html"},
      // But two backslashes on Windows should be UNC so should be treated
      // as absolute.
      {"http://host/a", true, false, "\\\\another\\path", true, false, false,
       nullptr},
      // IE doesn't support drive specs starting with two slashes. It fails
      // immediately and doesn't even try to load. We fix it up to either
      // an absolute path or UNC depending on what it looks like.
      {"file:///C:/something", true, true, "//c:/foo", true, true, true,
       "file:///C:/foo"},
      {"file:///C:/something", true, true, "//localhost/c:/foo", true, true,
       true, "file:///C:/foo"},
      // Windows drive specs should be allowed and treated as absolute.
      {"file:///C:/foo", true, true, "c:", true, false, false, nullptr},
      {"file:///C:/foo", true, true, "c:/foo", true, false, false, nullptr},
      {"http://host/a", true, false, "c:\\foo", true, false, false, nullptr},
      // Relative paths with drive letters should be allowed when the base is
      // also a file.
      {"file:///C:/foo", true, true, "/z:/bar", true, true, true,
       "file:///Z:/bar"},
      // Treat absolute paths as being off of the drive.
      {"file:///C:/foo", true, true, "/bar", true, true, true,
       "file:///C:/bar"},
      {"file://localhost/C:/foo", true, true, "/bar", true, true, true,
       "file://localhost/C:/bar"},
      {"file:///C:/foo/com/", true, true, "/bar", true, true, true,
       "file:///C:/bar"},
      // On Windows, two slashes without a drive letter when the base is a file
      // means that the path is UNC.
      {"file:///C:/something", true, true, "//somehost/path", true, true, true,
       "file://somehost/path"},
      {"file:///C:/something", true, true, "/\\//somehost/path", true, true,
       true, "file://somehost/path"},
#else
      // On Unix we fall back to relative behavior since there's nothing else
      // reasonable to do.
      {"http://host/a", true, false, "\\\\Another\\path", true, true, true,
       "http://another/path"},
#endif
      // Even on Windows, we don't allow relative drive specs when the base
      // is not file.
      {"http://host/a", true, false, "/c:\\foo", true, true, true,
       "http://host/c:/foo"},
      {"http://host/a", true, false, "//c:\\foo", true, true, true,
       "http://c/foo"},
      // Cross-platform relative file: resolution behavior.
      {"file://host/a", true, true, "/", true, true, true, "file://host/"},
      {"file://host/a", true, true, "//", true, true, true, "file:///"},
      {"file://host/a", true, true, "/b", true, true, true, "file://host/b"},
      {"file://host/a", true, true, "//b", true, true, true, "file://b/"},
      // Ensure that ports aren't allowed for hosts relative to a file url.
      // Although the result string shows a host:port portion, the call to
      // resolve the relative URL returns false, indicating parse failure,
      // which is what is required.
      {"file:///foo.txt", true, true, "//host:80/bar.txt", true, true, false,
       "file://host:80/bar.txt"},
      // Filesystem URL tests; filesystem URLs are only valid and relative if
      // they have no scheme, e.g. "./index.html". There's no valid equivalent
      // to http:index.html.
      {"filesystem:http://host/t/path", true, false,
       "filesystem:http://host/t/path2", true, false, false, nullptr},
      {"filesystem:http://host/t/path", true, false,
       "filesystem:https://host/t/path2", true, false, false, nullptr},
      {"filesystem:http://host/t/path", true, false, "http://host/t/path2",
       true, false, false, nullptr},
      {"http://host/t/path", true, false, "filesystem:http://host/t/path2",
       true, false, false, nullptr},
      {"filesystem:http://host/t/path", true, false, "./path2", true, true,
       true, "filesystem:http://host/t/path2"},
      {"filesystem:http://host/t/path/", true, false, "path2", true, true, true,
       "filesystem:http://host/t/path/path2"},
      {"filesystem:http://host/t/path", true, false, "filesystem:http:path2",
       true, false, false, nullptr},
      // Absolute URLs are still not relative to a non-standard base URL.
      {"about:blank", false, false, "http://X/A", true, false, true, ""},
      {"about:blank", false, false, "content://content.Provider/", true, false,
       true, ""},
  };

  for (const auto& cur_case : rel_cases) {
    Parsed parsed;
    if (cur_case.is_base_file)
      parsed = ParseFileURL(cur_case.base);
    else if (cur_case.is_base_hier)
      parsed = ParseStandardURL(cur_case.base);
    else
      parsed = ParsePathURL(cur_case.base, false);

    // First see if it is relative.
    int test_len = static_cast<int>(strlen(cur_case.test));
    bool is_relative;
    Component relative_component;
    bool succeed_is_rel = IsRelativeURL(
        cur_case.base, parsed, cur_case.test, test_len, cur_case.is_base_hier,
        &is_relative, &relative_component);

    EXPECT_EQ(cur_case.succeed_relative, succeed_is_rel) <<
        "succeed is rel failure on " << cur_case.test;
    EXPECT_EQ(cur_case.is_rel, is_relative) <<
        "is rel failure on " << cur_case.test;
    // Now resolve it.
    if (succeed_is_rel && is_relative && cur_case.is_rel) {
      std::string resolved;
      StdStringCanonOutput output(&resolved);
      Parsed resolved_parsed;

      bool succeed_resolve = ResolveRelativeURL(
          cur_case.base, parsed, cur_case.is_base_file, cur_case.test,
          relative_component, nullptr, &output, &resolved_parsed);
      output.Complete();

      EXPECT_EQ(cur_case.succeed_resolve, succeed_resolve);
      EXPECT_EQ(cur_case.resolved, resolved) << " on " << cur_case.test;

      // Verify that the output parsed structure is the same as parsing a
      // the URL freshly.
      Parsed ref_parsed;
      if (cur_case.is_base_file) {
        ref_parsed = ParseFileURL(resolved);
      } else if (cur_case.is_base_hier) {
        ref_parsed = ParseStandardURL(resolved);
      } else {
        ref_parsed = ParsePathURL(resolved, false);
      }
      EXPECT_TRUE(ParsedIsEqual(ref_parsed, resolved_parsed));
    }
  }
}

class URLCanonTypedTest : public ::testing::TestWithParam<bool> {
 public:
  URLCanonTypedTest()
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

  struct ResolveRelativeURLCase {
    const std::string_view base;
    const std::string_view rel;
    const bool is_base_hier;
    const bool expected_base_is_valid;
    const bool expected_is_relative;
    const bool expected_succeed_resolve;
    const std::string_view expected_resolved_url;
  };

  void TestNonSpecialResolveRelativeURL(
      const ResolveRelativeURLCase& relative_case) {
    // The following test is similar to URLCanonTest::ResolveRelativeURL, but
    // simplified.
    Parsed parsed = use_standard_compliant_non_special_scheme_url_parsing_
                        ? ParseNonSpecialURL(relative_case.base)
                        : ParsePathURL(relative_case.base,
                                       /*trim_path_end=*/true);

    // First see if it is relative.
    bool is_relative;
    Component relative_component;
    bool succeed_is_rel = IsRelativeURL(
        relative_case.base.data(), parsed, relative_case.rel.data(),
        relative_case.rel.size(), relative_case.is_base_hier, &is_relative,
        &relative_component);

    EXPECT_EQ(is_relative, relative_case.expected_is_relative);
    if (succeed_is_rel && is_relative) {
      std::string resolved_url;
      StdStringCanonOutput output(&resolved_url);
      Parsed resolved_parsed;

      bool succeed_resolve = ResolveRelativeURL(
          relative_case.base.data(), parsed, relative_case.is_base_hier,
          relative_case.rel.data(), relative_component, nullptr, &output,
          &resolved_parsed);
      output.Complete();

      EXPECT_EQ(succeed_resolve, relative_case.expected_succeed_resolve);
      EXPECT_EQ(resolved_url, relative_case.expected_resolved_url);
    }
  }

  bool use_standard_compliant_non_special_scheme_url_parsing_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(URLCanonTypedTest, NonSpecialResolveRelativeURL) {
  // Test flag-dependent behaviors of non-special URLs.
  if (use_standard_compliant_non_special_scheme_url_parsing_) {
    ResolveRelativeURLCase cases[] = {
        {"git://host", "path", true, true, true, true, "git://host/path"},
    };
    for (const auto& i : cases) {
      TestNonSpecialResolveRelativeURL(i);
    }
  } else {
    ResolveRelativeURLCase cases[] = {
        {"git://host", "path", true, true, true, true, "git://path"},
    };
    for (const auto& i : cases) {
      TestNonSpecialResolveRelativeURL(i);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(All, URLCanonTypedTest, ::testing::Bool());

// It used to be the case that when we did a replacement with a long buffer of
// UTF-16 characters, we would get invalid data in the URL. This is because the
// buffer that it used to hold the UTF-8 data was resized, while some pointers
// were still kept to the old buffer that was removed.
TEST_F(URLCanonTest, ReplacementOverflow) {
  const char src[] = "file:///C:/foo/bar";
  Parsed parsed = ParseFileURL(src);

  // Override two components, the path with something short, and the query with
  // something long enough to trigger the bug.
  Replacements<char16_t> repl;
  std::u16string new_query;
  for (int i = 0; i < 4800; i++)
    new_query.push_back('a');

  std::u16string new_path(test_utils::TruncateWStringToUTF16(L"/foo"));
  repl.SetPath(new_path.c_str(), Component(0, 4));
  repl.SetQuery(new_query.c_str(),
                Component(0, static_cast<int>(new_query.length())));

  // Call ReplaceComponents on the string. It doesn't matter if we call it for
  // standard URLs, file URLs, etc, since they will go to the same replacement
  // function that was buggy.
  Parsed repl_parsed;
  std::string repl_str;
  StdStringCanonOutput repl_output(&repl_str);
  ReplaceFileURL(src, parsed, repl, nullptr, &repl_output, &repl_parsed);
  repl_output.Complete();

  // Generate the expected string and check.
  std::string expected("file:///foo?");
  for (size_t i = 0; i < new_query.length(); i++)
    expected.push_back('a');
  EXPECT_TRUE(expected == repl_str);
}

TEST_F(URLCanonTest, DefaultPortForScheme) {
  struct TestCases {
    const char* scheme;
    const int expected_port;
  } cases[]{
      {"http", 80},
      {"https", 443},
      {"ftp", 21},
      {"ws", 80},
      {"wss", 443},
      {"fake-scheme", PORT_UNSPECIFIED},
      {"HTTP", PORT_UNSPECIFIED},
      {"HTTPS", PORT_UNSPECIFIED},
      {"FTP", PORT_UNSPECIFIED},
      {"WS", PORT_UNSPECIFIED},
      {"WSS", PORT_UNSPECIFIED},
  };

  for (const auto& test_case : cases) {
    SCOPED_TRACE(test_case.scheme);
    EXPECT_EQ(test_case.expected_port,
              DefaultPortForScheme(std::string_view(test_case.scheme,
                                                    strlen(test_case.scheme))));
  }
}

TEST_F(URLCanonTest, FindWindowsDriveLetter) {
  struct TestCase {
    std::string_view spec;
    int begin;
    int end;  // -1 for end of spec
    int expected_drive_letter_pos;
  } cases[] = {
      {"/", 0, -1, -1},

      {"c:/foo", 0, -1, 0},
      {"/c:/foo", 0, -1, 1},
      {"//c:/foo", 0, -1, -1},  // "//" does not canonicalize to "/"
      {"\\C|\\foo", 0, -1, 1},
      {"/cd:/foo", 0, -1, -1},  // "/c" does not canonicalize to "/"
      {"/./c:/foo", 0, -1, 3},
      {"/.//c:/foo", 0, -1, -1},  // "/.//" does not canonicalize to "/"
      {"/././c:/foo", 0, -1, 5},
      {"/abc/c:/foo", 0, -1, -1},  // "/abc/" does not canonicalize to "/"
      {"/abc/./../c:/foo", 0, -1, 10},

      {"/c:/c:/foo", 3, -1, 4},  // actual input is "/c:/foo"
      {"/c:/foo", 3, -1, -1},    // actual input is "/foo"
      {"/c:/foo", 0, 1, -1},     // actual input is "/"
  };

  for (const auto& c : cases) {
    int end = c.end;
    if (end == -1)
      end = c.spec.size();

    EXPECT_EQ(c.expected_drive_letter_pos,
              FindWindowsDriveLetter(c.spec.data(), c.begin, end))
        << "for " << c.spec << "[" << c.begin << ":" << end << "] (UTF-8)";

    std::u16string spec16 = base::ASCIIToUTF16(c.spec);
    EXPECT_EQ(c.expected_drive_letter_pos,
              FindWindowsDriveLetter(spec16.data(), c.begin, end))
        << "for " << c.spec << "[" << c.begin << ":" << end << "] (UTF-16)";
  }
}

TEST_F(URLCanonTest, IDNToASCII) {
  RawCanonOutputW<1024> output;

  // Basic ASCII test.
  std::u16string str = u"hello";
  EXPECT_TRUE(IDNToASCII(str, &output));
  EXPECT_EQ(u"hello", std::u16string(output.data()));
  output.set_length(0);

  // Mixed ASCII/non-ASCII.
  str = u"hellö";
  EXPECT_TRUE(IDNToASCII(str, &output));
  EXPECT_EQ(u"xn--hell-8qa", std::u16string(output.data()));
  output.set_length(0);

  // All non-ASCII.
  str = u"你好";
  EXPECT_TRUE(IDNToASCII(str, &output));
  EXPECT_EQ(u"xn--6qq79v", std::u16string(output.data()));
  output.set_length(0);

  // Characters that need mapping (the resulting Punycode is the encoding for
  // "1⁄4").
  str = u"¼";
  EXPECT_TRUE(IDNToASCII(str, &output));
  EXPECT_EQ(u"xn--14-c6t", std::u16string(output.data()));
  output.set_length(0);

  // String to encode already starts with "xn--", and all ASCII. Should not
  // modify the string.
  str = u"xn--hell-8qa";
  EXPECT_TRUE(IDNToASCII(str, &output));
  EXPECT_EQ(u"xn--hell-8qa", std::u16string(output.data()));
  output.set_length(0);

  // String to encode already starts with "xn--", and mixed ASCII/non-ASCII.
  // Should fail, due to a special case: if the label starts with "xn--", it
  // should be parsed as Punycode, which must be all ASCII.
  str = u"xn--hellö";
  EXPECT_FALSE(IDNToASCII(str, &output));
  output.set_length(0);

  // String to encode already starts with "xn--", and mixed ASCII/non-ASCII.
  // This tests that there is still an error for the character '⁄' (U+2044),
  // which would be a valid ASCII character, U+0044, if the high byte were
  // ignored.
  str = u"xn--1⁄4";
  EXPECT_FALSE(IDNToASCII(str, &output));
  output.set_length(0);
}

void ComponentCaseMatches(bool success,
                          std::string_view out_str,
                          const Component& out_comp,
                          const DualComponentCase& expected) {
  EXPECT_EQ(success, expected.expected_success);
  EXPECT_STREQ(out_str.data(), expected.expected);
  EXPECT_EQ(out_comp, expected.expected_component);
}

TEST_F(URLCanonTest, OpaqueHost) {
  DualComponentCase host_cases[] = {
      {"", L"", "", Component(), true},
      {"google.com", L"google.com", "google.com", Component(0, 10), true},
      // Upper case letters should be preserved.
      {"gooGle.com", L"gooGle.com", "gooGle.com", Component(0, 10), true},
      {"\x41", L"\x41", "A", Component(0, 1), true},
      {"\x61", L"\x61", "a", Component(0, 1), true},
      // Percent encode.
      {"\x10", L"\x10", "%10", Component(0, 3), true},
      // A valid percent encoding should be preserved.
      {"%41", L"%41", "%41", Component(0, 3), true},
      // An invalid percent encoding should be preserved too.
      {"%zz", L"%zz", "%zz", Component(0, 3), true},
      // UTF-16 HIRAGANA LETTER A (codepoint U+3042, "\xe3\x81\x82" in UTF-8).
      {"\xe3\x81\x82", L"\x3042", "%E3%81%82", Component(0, 9), true},
  };

  for (const auto& host_case : host_cases) {
    SCOPED_TRACE(testing::Message() << "url: \"" << host_case.input8 << "\"");
    std::string out_str;
    StdStringCanonOutput output(&out_str);
    Component out_comp;
    bool success = CanonicalizeNonSpecialHost(
        host_case.input8,
        Component(0, static_cast<int>(strlen(host_case.input8))), output,
        out_comp);
    output.Complete();
    ComponentCaseMatches(success, out_str, out_comp, host_case);
  }

  // UTF-16 version.
  for (const auto& host_case : host_cases) {
    SCOPED_TRACE(testing::Message() << "url: \"" << host_case.input16 << "\"");
    std::u16string input16(
        test_utils::TruncateWStringToUTF16(host_case.input16));
    std::string out_str;
    StdStringCanonOutput output(&out_str);
    Component out_comp;
    bool success = CanonicalizeNonSpecialHost(
        input16.c_str(), Component(0, static_cast<int>(input16.length())),
        output, out_comp);
    output.Complete();
    ComponentCaseMatches(success, out_str, out_comp, host_case);
  }
}

void IPAddressCaseMatches(std::string_view out_str,
                          const CanonHostInfo& host_info,
                          const IPAddressCase& expected) {
  EXPECT_EQ(host_info.family, expected.expected_family);
  EXPECT_STREQ(out_str.data(), expected.expected);
  EXPECT_EQ(base::HexEncode(host_info.address,
                            static_cast<size_t>(host_info.AddressLength())),
            expected.expected_address_hex);
  if (expected.expected_family == CanonHostInfo::IPV4) {
    EXPECT_EQ(host_info.num_ipv4_components,
              expected.expected_num_ipv4_components);
  }
}

TEST_F(URLCanonTest, NonSpecialHostIPv6Address) {
  IPAddressCase ip_address_cases[] = {
      // Non-special URLs don't support IPv4. Family must be NEUTRAL.
      {"192.168.0.1", L"192.168.0.1", "192.168.0.1", Component(0, 11),
       CanonHostInfo::NEUTRAL, 0, ""},
      {"192", L"192", "192", Component(0, 3), CanonHostInfo::NEUTRAL, 0, ""},
      // "257" is allowed since the number is not considered as a part of IPv4.
      {"192.168.0.257", L"192.168.0.257", "192.168.0.257", Component(0, 13),
       CanonHostInfo::NEUTRAL, 0, ""},
      // IPv6.
      {"[1:0:0:2::3:0]", L"[1:0:0:2::3:0]", "[1::2:0:0:3:0]", Component(0, 14),
       CanonHostInfo::IPV6, -1, "00010000000000020000000000030000"},
      {"[::]", L"[::]", "[::]", Component(0, 4), CanonHostInfo::IPV6, -1,
       "00000000000000000000000000000000"},
      // Invalid hosts.
      {"#[::]", L"#[::]", "", Component(), CanonHostInfo::BROKEN, -1, ""},
      {"[]", L"[]", "[]", Component(), CanonHostInfo::BROKEN, -1, ""},
      {"a]", L"a]", "a]", Component(), CanonHostInfo::BROKEN, -1, ""},
      {"[a", L"[a", "[a", Component(), CanonHostInfo::BROKEN, -1, ""},
      {"a[]", L"a[]", "a[]", Component(), CanonHostInfo::BROKEN, -1, ""},
      {"[]a", L"[]a", "[]a", Component(), CanonHostInfo::BROKEN, -1, ""},
  };

  for (const auto& ip_address_case : ip_address_cases) {
    SCOPED_TRACE(testing::Message()
                 << "url: \"" << ip_address_case.input8 << "\"");
    std::string out_str;
    StdStringCanonOutput output(&out_str);
    CanonHostInfo host_info;
    CanonicalizeNonSpecialHostVerbose(
        ip_address_case.input8,
        Component(0, static_cast<int>(strlen(ip_address_case.input8))), output,
        host_info);
    output.Complete();
    IPAddressCaseMatches(out_str, host_info, ip_address_case);
  }

  // UTF-16 version.
  for (const auto& ip_address_case : ip_address_cases) {
    SCOPED_TRACE(testing::Message()
                 << "url: \"" << ip_address_case.input16 << "\"");
    std::u16string input16(
        test_utils::TruncateWStringToUTF16(ip_address_case.input16));
    std::string out_str;
    StdStringCanonOutput output(&out_str);
    CanonHostInfo host_info;
    CanonicalizeNonSpecialHostVerbose(
        input16.c_str(), Component(0, static_cast<int>(input16.length())),
        output, host_info);
    output.Complete();
    IPAddressCaseMatches(out_str, host_info, ip_address_case);
  }
}

}  // namespace url
