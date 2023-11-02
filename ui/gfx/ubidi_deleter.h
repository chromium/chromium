// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_UBIDI_DELETER_H_
#define UI_GFX_UBIDI_DELETER_H_

#include "third_party/icu/source/common/unicode/ubidi.h"

namespace ui::gfx {

struct UBiDiDeleter {
  void operator()(UBiDi* bidi) {
    if (bidi)
      ubidi_close(bidi);
  }
};

}  // namespace ui::gfx

#endif  // UI_GFX_UBIDI_DELETER_H_
