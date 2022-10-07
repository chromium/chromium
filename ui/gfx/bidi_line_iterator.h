// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_BIDI_LINE_ITERATOR_H_
#define UI_GFX_BIDI_LINE_ITERATOR_H_

#include <memory>
#include <string>

#include "base/i18n/rtl.h"
#include "third_party/icu/source/common/unicode/ubidi.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "ui/gfx/gfx_export.h"
#include "ui/gfx/ubidi_deleter.h"

namespace ui {
namespace gfx {

// A simple wrapper class for the bidirectional iterator of ICU.
// This class uses the bidirectional iterator of ICU to split a line of
// bidirectional texts into visual runs in its display order.
class GFX_EXPORT BiDiLineIterator {
 public:
  BiDiLineIterator();

  BiDiLineIterator(const BiDiLineIterator&) = delete;
  BiDiLineIterator& operator=(const BiDiLineIterator&) = delete;

  ~BiDiLineIterator();

  // Initializes the bidirectional iterator with the specified text.  Returns
  // whether initialization succeeded.
  bool Open(const std::u16string& text, base::i18n::TextDirection direction);

  // Returns the number of visual runs in the text, or zero on error.
  int CountRuns() const;

  // Gets the logical offset, length, and direction of the specified visual run.
  UBiDiDirection GetVisualRun(int index, int* start, int* length) const;

  // Given a start position, figure out where the run ends (and the BiDiLevel).
  void GetLogicalRun(int start, int* end, UBiDiLevel* level) const;

 private:
  std::unique_ptr<UBiDi, UBiDiDeleter> bidi_;
};

}  // namespace gfx
}  // namespace ui

#endif  // UI_GFX_BIDI_LINE_ITERATOR_H_
