// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_INPUT_STATE_LOOKUP_WIN_H_
#define UI_AURA_INPUT_STATE_LOOKUP_WIN_H_

#include "ui/aura/input_state_lookup.h"

namespace aura {

// Windows implementation of InputStateLookup.
class AURA_EXPORT InputStateLookupWin : public InputStateLookup {
 public:
  InputStateLookupWin();

  InputStateLookupWin(const InputStateLookupWin&) = delete;
  InputStateLookupWin& operator=(const InputStateLookupWin&) = delete;

  ~InputStateLookupWin() override;

  // InputStateLookup overrides:
  bool IsMouseButtonDown() const override;
};

}  // namespace aura

#endif  // UI_AURA_INPUT_STATE_LOOKUP_WIN_H_
