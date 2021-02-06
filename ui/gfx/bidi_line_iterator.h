// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_BIDI_LINE_ITERATOR_H_
#define UI_GFX_BIDI_LINE_ITERATOR_H_

#include "base/i18n/rtl.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "third_party/icu/source/common/unicode/ubidi.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "ui/gfx/gfx_export.h"

namespace ui {
namespace gfx {

// A simple wrapper class for the bidirectional iterator of ICU.
// This class uses the bidirectional iterator of ICU to split a line of
// bidirectional texts into visual runs in its display order.
class GFX_EXPORT BiDiLineIterator {
 public:
  BiDiLineIterator();
  ~BiDiLineIterator();

  // Initializes the bidirectional iterator with the specified text.  Returns
  // whether initialization succeeded.
  bool Open(const base::string16& text, base::i18n::TextDirection direction);

  // Returns the number of visual runs in the text, or zero on error.
  int CountRuns() const;

  // Gets the logical offset, length, and direction of the specified visual run.
  UBiDiDirection GetVisualRun(int index, int* start, int* length) const;

  // Given a start position, figure out where the run ends (and the BiDiLevel).
  void GetLogicalRun(int start, int* end, UBiDiLevel* level) const;

 private:
  UBiDi* bidi_;

  DISALLOW_COPY_AND_ASSIGN(BiDiLineIterator);
};

}  // namespace gfx
}  // namespace ui

#endif  // UI_GFX_BIDI_LINE_ITERATOR_H_
