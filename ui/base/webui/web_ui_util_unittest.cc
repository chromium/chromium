// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/webui/web_ui_util.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

TEST(WebUIUtilTest, ParsePathAndImageSpec) {
  std::string path;

  float factor = 0;
  int index = -2;
  GURL url("http://[::192.9.5.5]/some/random/username@email/and/more");
  webui::ParsePathAndImageSpec(url, &path, &factor, &index);
  EXPECT_EQ("some/random/username@email/and/more", path);
  EXPECT_EQ(1.0f, factor);
  EXPECT_EQ(-1, index);

  factor = 0;
  index = -2;
  GURL url2("http://host/some/random/username/and/more");
  webui::ParsePathAndImageSpec(url2, &path, &factor, &index);
  EXPECT_EQ("some/random/username/and/more", path);
  EXPECT_EQ(1.0f, factor);
  EXPECT_EQ(-1, index);

  factor = 0;
  index = -2;
  GURL url3("http://host/some/random/username/and/more[0]@2x");
  webui::ParsePathAndImageSpec(url3, &path, &factor, &index);
  EXPECT_EQ("some/random/username/and/more", path);
  EXPECT_EQ(2.0f, factor);
  EXPECT_EQ(0, index);

  factor = 0;
  index = -2;
  GURL url4("http://[::192.9.5.5]/some/random/username@email/and/more[1]@1.5x");
  webui::ParsePathAndImageSpec(url4, &path, &factor, &index);
  EXPECT_EQ("some/random/username@email/and/more", path);
  EXPECT_EQ(1.5f, factor);
  EXPECT_EQ(1, index);
}

TEST(WebUIUtilTest, GetPngDataUrl_Basic) {
  // The input doesn't have to be a valid image.
  std::vector<unsigned char> in = {1, 2, 3, 4};
  std::string out = webui::GetPngDataUrl(in.data(), in.size());
  EXPECT_EQ("data:image/png;base64,AQIDBA==", out);
}

TEST(WebUIUtilTest, GetPngDataUrl_EmptyInput) {
  std::vector<unsigned char> in;
  webui::GetPngDataUrl(in.data(), in.size());
  // No crash.
}

TEST(WebUIUtilTest, ParseScaleFactor) {
  float scale_factor = 0;

  // Invalid input.
  EXPECT_FALSE(webui::ParseScaleFactor("", &scale_factor));
  EXPECT_FALSE(webui::ParseScaleFactor("0", &scale_factor));
  EXPECT_FALSE(webui::ParseScaleFactor("a", &scale_factor));
  EXPECT_FALSE(webui::ParseScaleFactor("x", &scale_factor));
  EXPECT_FALSE(webui::ParseScaleFactor("ax", &scale_factor));
  EXPECT_FALSE(webui::ParseScaleFactor("5", &scale_factor));
  EXPECT_FALSE(webui::ParseScaleFactor("-5", &scale_factor));
  EXPECT_FALSE(webui::ParseScaleFactor("-5x", &scale_factor));
  EXPECT_FALSE(webui::ParseScaleFactor("0x", &scale_factor));
  EXPECT_FALSE(webui::ParseScaleFactor("2000x", &scale_factor));

  // Valid input.
  EXPECT_TRUE(webui::ParseScaleFactor("2x", &scale_factor));
  EXPECT_EQ(2.0f, scale_factor);

  EXPECT_TRUE(webui::ParseScaleFactor("1.5x", &scale_factor));
  EXPECT_EQ(1.5f, scale_factor);

  EXPECT_TRUE(webui::ParseScaleFactor("1000x", &scale_factor));
  EXPECT_EQ(1000.0f, scale_factor);
}
