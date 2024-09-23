// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/354829279): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/gfx/win/text_analysis_source.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/win/direct_write.h"

namespace mswr = Microsoft::WRL;

namespace gfx {
namespace win {

namespace {

DWRITE_READING_DIRECTION kReadingDirection =
    DWRITE_READING_DIRECTION_TOP_TO_BOTTOM;
const wchar_t* kLocale = L"hi-in";
const wchar_t kText[] = L"sample text";
const size_t kTextLength = _countof(kText) - 1;

}  // namespace

class TextAnalysisSourceTest : public testing::Test {
 public:
  TextAnalysisSourceTest() {
    mswr::ComPtr<IDWriteFactory> factory;
    CreateDWriteFactory(&factory);

    mswr::ComPtr<IDWriteNumberSubstitution> number_substitution;
    factory->CreateNumberSubstitution(DWRITE_NUMBER_SUBSTITUTION_METHOD_NONE,
                                      kLocale, true /* ignoreUserOverride */,
                                      &number_substitution);

    TextAnalysisSource::Create(&source_, kText, kLocale,
                               number_substitution.Get(), kReadingDirection);
  }

  TextAnalysisSourceTest(const TextAnalysisSourceTest&) = delete;
  TextAnalysisSourceTest& operator=(const TextAnalysisSourceTest&) = delete;

 protected:
  mswr::ComPtr<IDWriteTextAnalysisSource> source_;
};

TEST_F(TextAnalysisSourceTest, TestGetLocaleName) {
  UINT32 length = 0;
  const wchar_t* locale_name = nullptr;
  HRESULT hr = E_FAIL;
  hr = source_->GetLocaleName(0, &length, &locale_name);

  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(kTextLength, length);
  EXPECT_STREQ(kLocale, locale_name);

  // Attempting to get a locale for a location that does not have a text chunk
  // should fail.
  hr = source_->GetLocaleName(kTextLength, &length, &locale_name);

  EXPECT_TRUE(FAILED(hr));
}

TEST_F(TextAnalysisSourceTest, TestGetNumberSubstitution) {
  UINT32 length = 0;
  mswr::ComPtr<IDWriteNumberSubstitution> number_substitution;
  HRESULT hr = E_FAIL;
  hr = source_->GetNumberSubstitution(0, &length, &number_substitution);

  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(kTextLength, length);
  EXPECT_NE(nullptr, number_substitution.Get());

  // Attempting to get a number substitution for a location that does not have
  // a text chunk should fail.
  hr = source_->GetNumberSubstitution(kTextLength, &length,
                                      &number_substitution);

  EXPECT_TRUE(FAILED(hr));
}

TEST_F(TextAnalysisSourceTest, TestGetParagraphReadingDirection) {
  EXPECT_EQ(kReadingDirection, source_->GetParagraphReadingDirection());
}

TEST_F(TextAnalysisSourceTest, TestGetTextAtPosition) {
  UINT32 length = 0;
  const wchar_t* text = nullptr;
  HRESULT hr = E_FAIL;
  hr = source_->GetTextAtPosition(0, &text, &length);

  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(kTextLength, length);
  EXPECT_STREQ(kText, text);

  hr = source_->GetTextAtPosition(5, &text, &length);

  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(kTextLength - 5, length);
  EXPECT_STREQ(kText + 5, text);

  // Trying to get a text chunk past the end should return null.
  hr = source_->GetTextAtPosition(kTextLength, &text, &length);

  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(nullptr, text);
}

TEST_F(TextAnalysisSourceTest, TestGetTextBeforePosition) {
  UINT32 length = 0;
  const wchar_t* text = nullptr;
  HRESULT hr = E_FAIL;
  // Trying to get a text chunk before the beginning should return null.
  hr = source_->GetTextBeforePosition(0, &text, &length);

  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(nullptr, text);

  hr = source_->GetTextBeforePosition(5, &text, &length);

  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(5u, length);
  EXPECT_STREQ(kText, text);

  hr = source_->GetTextBeforePosition(kTextLength, &text, &length);

  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(kTextLength, length);
  EXPECT_STREQ(kText, text);
}

}  // namespace win
}  // namespace gfx
