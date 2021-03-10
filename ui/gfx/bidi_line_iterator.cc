// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/bidi_line_iterator.h"

#include "base/check.h"
#include "base/notreached.h"

namespace ui {
namespace gfx {

namespace {

UBiDiLevel GetParagraphLevelForDirection(base::i18n::TextDirection direction) {
  switch (direction) {
    case base::i18n::UNKNOWN_DIRECTION:
      return UBIDI_DEFAULT_LTR;
    case base::i18n::RIGHT_TO_LEFT:
      return 1;  // Highest RTL level.
    case base::i18n::LEFT_TO_RIGHT:
      return 0;  // Highest LTR level.
    default:
      NOTREACHED();
      return 0;
  }
}

}  // namespace

BiDiLineIterator::BiDiLineIterator() : bidi_(nullptr) {}

BiDiLineIterator::~BiDiLineIterator() {
  if (bidi_) {
    ubidi_close(bidi_);
    bidi_ = nullptr;
  }
}

bool BiDiLineIterator::Open(const base::string16& text,
                            base::i18n::TextDirection direction) {
  DCHECK(!bidi_);
  UErrorCode error = U_ZERO_ERROR;
  bidi_ = ubidi_openSized(static_cast<int>(text.length()), 0, &error);
  if (U_FAILURE(error))
    return false;

  ubidi_setPara(bidi_, text.data(), static_cast<int>(text.length()),
                GetParagraphLevelForDirection(direction), nullptr, &error);
  return (U_SUCCESS(error));
}

int BiDiLineIterator::CountRuns() const {
  DCHECK(bidi_ != nullptr);
  UErrorCode error = U_ZERO_ERROR;
  const int runs = ubidi_countRuns(bidi_, &error);
  return U_SUCCESS(error) ? runs : 0;
}

UBiDiDirection BiDiLineIterator::GetVisualRun(int index,
                                              int* start,
                                              int* length) const {
  DCHECK(bidi_ != nullptr);
  return ubidi_getVisualRun(bidi_, index, start, length);
}

void BiDiLineIterator::GetLogicalRun(int start,
                                     int* end,
                                     UBiDiLevel* level) const {
  DCHECK(bidi_ != nullptr);
  ubidi_getLogicalRun(bidi_, start, end, level);
}

}  // namespace gfx
}  // namespace ui
