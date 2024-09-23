// Copyright 2006-2008 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/test/perf_time_logger.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_canon.h"
#include "url/url_canon_stdstring.h"

namespace {

TEST(URLParse, FullURL) {
  constexpr std::string_view kUrl =
      "http://me:pass@host/foo/bar.html;param?query=yes#ref";

  url::Parsed parsed;
  base::PerfTimeLogger timer("Full_URL_Parse_AMillion");

  for (int i = 0; i < 1000000; i++)
    parsed = url::ParseStandardURL(kUrl);
  timer.Done();
}

constexpr std::string_view kTypicalUrl1 =
    "http://www.google.com/"
    "search?q=url+parsing&ie=utf-8&oe=utf-8&aq=t&rls=org.mozilla:en-US:"
    "official&client=firefox-a";

constexpr std::string_view kTypicalUrl2 =
    "http://www.amazon.com/Stephen-King-Thrillers-Horror-People/dp/0766012336/"
    "ref=sr_1_2/133-4144931-4505264?ie=UTF8&s=books&qid=2144880915&sr=8-2";

constexpr std::string_view kTypicalUrl3 =
    "http://store.apple.com/1-800-MY-APPLE/WebObjects/AppleStore.woa/wa/"
    "RSLID?nnmm=browse&mco=578E9744&node=home/desktop/mac_pro";

TEST(URLParse, TypicalURLParse) {
  url::Parsed parsed1;
  url::Parsed parsed2;
  url::Parsed parsed3;

  // Do this 1/3 of a million times since we do 3 different URLs.
  base::PerfTimeLogger parse_timer("Typical_URL_Parse_AMillion");
  for (int i = 0; i < 333333; i++) {
    parsed1 = url::ParseStandardURL(kTypicalUrl1);
    parsed2 = url::ParseStandardURL(kTypicalUrl2);
    parsed3 = url::ParseStandardURL(kTypicalUrl3);
  }
  parse_timer.Done();
}

// Includes both parsing and canonicalization with no mallocs.
TEST(URLParse, TypicalURLParseCanon) {
  base::PerfTimeLogger canon_timer("Typical_Parse_Canon_AMillion");
  url::Parsed out_parsed;
  url::RawCanonOutput<1024> output;
  for (int i = 0; i < 333333; i++) {  // divide by 3 so we get 1M
    output.set_length(0);
    url::CanonicalizeStandardURL(
        kTypicalUrl1.data(), url::ParseStandardURL(kTypicalUrl1),
        url::SCHEME_WITH_HOST_PORT_AND_USER_INFORMATION, nullptr, &output,
        &out_parsed);

    output.set_length(0);
    url::CanonicalizeStandardURL(
        kTypicalUrl2.data(), url::ParseStandardURL(kTypicalUrl2),
        url::SCHEME_WITH_HOST_PORT_AND_USER_INFORMATION, nullptr, &output,
        &out_parsed);

    output.set_length(0);
    url::CanonicalizeStandardURL(
        kTypicalUrl3.data(), url::ParseStandardURL(kTypicalUrl3),
        url::SCHEME_WITH_HOST_PORT_AND_USER_INFORMATION, nullptr, &output,
        &out_parsed);
  }
  canon_timer.Done();
}

// Includes both parsing and canonicalization, and mallocs for the output.
TEST(URLParse, TypicalURLParseCanonStdString) {
  base::PerfTimeLogger canon_timer("Typical_Parse_Canon_AMillion");
  url::Parsed out_parsed;
  for (int i = 0; i < 333333; i++) {  // divide by 3 so we get 1M
    std::string out1;
    url::StdStringCanonOutput output1(&out1);
    url::CanonicalizeStandardURL(
        kTypicalUrl1.data(), url::ParseStandardURL(kTypicalUrl1),
        url::SCHEME_WITH_HOST_PORT_AND_USER_INFORMATION, nullptr, &output1,
        &out_parsed);

    std::string out2;
    url::StdStringCanonOutput output2(&out2);
    url::CanonicalizeStandardURL(
        kTypicalUrl2.data(), url::ParseStandardURL(kTypicalUrl2),
        url::SCHEME_WITH_HOST_PORT_AND_USER_INFORMATION, nullptr, &output2,
        &out_parsed);

    std::string out3;
    url::StdStringCanonOutput output3(&out3);
    url::CanonicalizeStandardURL(
        kTypicalUrl3.data(), url::ParseStandardURL(kTypicalUrl3),
        url::SCHEME_WITH_HOST_PORT_AND_USER_INFORMATION, nullptr, &output3,
        &out_parsed);
  }
  canon_timer.Done();
}

TEST(URLParse, GURL) {
  base::PerfTimeLogger gurl_timer("Typical_GURL_AMillion");
  for (int i = 0; i < 333333; i++) {  // divide by 3 so we get 1M
    GURL gurl1(kTypicalUrl1);
    GURL gurl2(kTypicalUrl2);
    GURL gurl3(kTypicalUrl3);
  }
  gurl_timer.Done();
}

}  // namespace
