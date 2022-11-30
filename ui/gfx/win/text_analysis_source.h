// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_WIN_TEXT_ANALYSIS_SOURCE_H_
#define UI_GFX_WIN_TEXT_ANALYSIS_SOURCE_H_

#include <dwrite.h>
#include <wrl.h>

#include <string>

#include "ui/gfx/gfx_export.h"

namespace gfx {
namespace win {

// Implements an IDWriteTextAnalysisSource, describing a single pre-defined
// chunk of text with a uniform locale, reading direction, and number
// substitution.
class TextAnalysisSource
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IDWriteTextAnalysisSource> {
 public:
  // Factory method to avoid exporting the class and all it derives from.
  static GFX_EXPORT HRESULT
  Create(IDWriteTextAnalysisSource** text_analysis_out,
         const std::wstring& text,
         const std::wstring& locale_name,
         IDWriteNumberSubstitution* number_substitution,
         DWRITE_READING_DIRECTION reading_direction);

  // Use Create() to construct these objects. Direct calls to the constructor
  // are an error - it is only public because a WRL helper function creates the
  // objects.
  TextAnalysisSource();

  TextAnalysisSource& operator=(const TextAnalysisSource&) = delete;

  // IDWriteTextAnalysisSource:
  HRESULT STDMETHODCALLTYPE GetLocaleName(UINT32 text_position,
                                          UINT32* text_length,
                                          const WCHAR** locale_name) override;
  HRESULT STDMETHODCALLTYPE GetNumberSubstitution(
      UINT32 text_position,
      UINT32* text_length,
      IDWriteNumberSubstitution** number_substitution) override;
  DWRITE_READING_DIRECTION STDMETHODCALLTYPE
  GetParagraphReadingDirection() override;
  HRESULT STDMETHODCALLTYPE GetTextAtPosition(UINT32 text_position,
                                              const WCHAR** text_string,
                                              UINT32* text_length) override;
  HRESULT STDMETHODCALLTYPE GetTextBeforePosition(UINT32 text_position,
                                                  const WCHAR** text_string,
                                                  UINT32* text_length) override;

  HRESULT STDMETHODCALLTYPE
  RuntimeClassInitialize(const std::wstring& text,
                         const std::wstring& locale_name,
                         IDWriteNumberSubstitution* number_substitution,
                         DWRITE_READING_DIRECTION reading_direction);

 protected:
  ~TextAnalysisSource() override;

 private:
  std::wstring text_;
  std::wstring locale_name_;
  Microsoft::WRL::ComPtr<IDWriteNumberSubstitution> number_substitution_;
  DWRITE_READING_DIRECTION reading_direction_;
};

}  // namespace win
}  // namespace gfx

#endif  // UI_GFX_WIN_TEXT_ANALYSIS_SOURCE_H_
