// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_util_win.h"

#include "testing/platform_test.h"

namespace ui {
namespace {

using ClipboardUtilWinTest = PlatformTest;

TEST_F(ClipboardUtilWinTest, EmptyHtmlToCFHtml) {
  const std::string result_cfhtml =
      clipboard_util::HtmlToCFHtml(std::string(), "www.example.com");
  EXPECT_TRUE(result_cfhtml.empty());
  EXPECT_TRUE(result_cfhtml.empty());
}

TEST_F(ClipboardUtilWinTest, ConversionFromWellFormedHtmlToCFHtml) {
  const std::string well_formed_html =
      "<html><head><style>p {color:blue}</style></head><body><p>Hello "
      "World</p></body></html>";
  const std::string url = "www.example.com";
  const std::string expected_cfhtml =
      "Version:0.9\r\n"
      "StartHTML:0000000132\r\n"
      "EndHTML:0000000290\r\n"
      "StartFragment:0000000168\r\n"
      "EndFragment:0000000254\r\n"
      "SourceURL:" +
      url +
      "\r\n"
      "<html>\r\n"
      "<body>\r\n"
      "<!--StartFragment-->" +
      well_formed_html + "<!--EndFragment-->" + "\r\n</body>\r\n</html>";
  const std::string actual_cfhtml =
      clipboard_util::HtmlToCFHtml(well_formed_html, url);
  EXPECT_EQ(expected_cfhtml, actual_cfhtml);
}

}  // namespace
}  // namespace ui