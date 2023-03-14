// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_util_win.h"

#include "testing/platform_test.h"

namespace ui {
namespace {

using ClipboardUtilWinTest = PlatformTest;

TEST_F(ClipboardUtilWinTest, EmptyHtmlToCFHtml) {
  const std::string result_cfhtml = clipboard_util::HtmlToCFHtml(
      std::string(), "www.example.com", ClipboardContentType::kSanitized);
  EXPECT_TRUE(result_cfhtml.empty());
  EXPECT_TRUE(result_cfhtml.empty());
}

TEST_F(ClipboardUtilWinTest, ConversionFromSanitizedHtmlToCFHtml) {
  const std::string sanitized_html =
      "<p style=\"color: blue; font-size: medium; font-style: normal; "
      "font-variant-ligatures: normal; font-variant-caps: normal; font-weight: "
      "400; letter-spacing: normal; orphans: 2; text-align: start; "
      "text-indent: 0px; text-transform: none; white-space: normal; widows: 2; "
      "word-spacing: 0px; -webkit-text-stroke-width: 0px; "
      "text-decoration-thickness: initial; text-decoration-style: initial; "
      "text-decoration-color: initial;\">Hello World</p>";
  const std::string url = "www.example.com";
  const std::string expected_cfhtml =
      "Version:0.9\r\n"
      "StartHTML:0000000132\r\n"
      "EndHTML:0000000637\r\n"
      "StartFragment:0000000168\r\n"
      "EndFragment:0000000601\r\n"
      "SourceURL:" +
      url +
      "\r\n"
      "<html>\r\n"
      "<body>\r\n"
      "<!--StartFragment-->" +
      sanitized_html +
      "<!--EndFragment-->\r\n"
      "</body>\r\n"
      "</html>";
  const std::string actual_cfhtml = clipboard_util::HtmlToCFHtml(
      sanitized_html, url, ClipboardContentType::kSanitized);
  EXPECT_EQ(expected_cfhtml, actual_cfhtml);
}

TEST_F(ClipboardUtilWinTest, ConversionFromUnsanitizedCompleteHtmlToCFHtml) {
  const std::string unsanitized_html =
      "<html><head><style>p {color:blue}</style></head><body><p>Hello "
      "World</p></body></html>";
  const std::string url = "www.example.com";
  const std::string expected_cfhtml =
      "Version:0.9\r\n"
      "StartHTML:0000000132\r\n"
      "EndHTML:0000000256\r\n"
      "StartFragment:0000000152\r\n"
      "EndFragment:0000000238\r\n"
      "SourceURL:" +
      url +
      "\r\n"
      "<!--StartFragment-->" +
      unsanitized_html + "<!--EndFragment-->";
  const std::string actual_cfhtml = clipboard_util::HtmlToCFHtml(
      unsanitized_html, url, ClipboardContentType::kUnsanitized);
  EXPECT_EQ(expected_cfhtml, actual_cfhtml);
}

TEST_F(ClipboardUtilWinTest, ConversionFromUnsanitizedIncompleteHtmlToCFHtml) {
  const std::string unsanitized_html =
      "<head><style>p {color:blue}</style></head><p>Hello World</p>";
  const std::string url = "www.example.com";
  const std::string expected_cfhtml =
      "Version:0.9\r\n"
      "StartHTML:0000000132\r\n"
      "EndHTML:0000000230\r\n"
      "StartFragment:0000000152\r\n"
      "EndFragment:0000000212\r\n"
      "SourceURL:" +
      url +
      "\r\n"
      "<!--StartFragment-->" +
      unsanitized_html + "<!--EndFragment-->";
  const std::string actual_cfhtml = clipboard_util::HtmlToCFHtml(
      unsanitized_html, url, ClipboardContentType::kUnsanitized);
  EXPECT_EQ(expected_cfhtml, actual_cfhtml);
}

TEST_F(ClipboardUtilWinTest,
       ConversionFromUnsanitizedHtmlWithDoubleTagsToCFHtml) {
  const std::string unsanitized_html =
      "<!--StartFragment--><head><style>p {color:blue}</style></head><p>Hello "
      "World</p><!--EndFragment-->";
  const std::string url = "www.example.com";
  const std::string expected_cfhtml =
      "Version:0.9\r\n"
      "StartHTML:0000000132\r\n"
      "EndHTML:0000000268\r\n"
      "StartFragment:0000000152\r\n"
      "EndFragment:0000000250\r\n"
      "SourceURL:" +
      url +
      "\r\n"
      "<!--StartFragment-->" +
      unsanitized_html + "<!--EndFragment-->";
  const std::string actual_cfhtml = clipboard_util::HtmlToCFHtml(
      unsanitized_html, url, ClipboardContentType::kUnsanitized);
  EXPECT_EQ(expected_cfhtml, actual_cfhtml);
}

}  // namespace
}  // namespace ui