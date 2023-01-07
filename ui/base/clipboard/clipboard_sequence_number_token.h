// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_CLIPBOARD_SEQUENCE_NUMBER_TOKEN_H_
#define UI_BASE_CLIPBOARD_CLIPBOARD_SEQUENCE_NUMBER_TOKEN_H_

#include "base/types/token_type.h"

namespace ui {

// Identifies a unique clipboard state. This can be used to version the data on
// the clipboard and determine whether it has changed.
using ClipboardSequenceNumberToken =
    base::TokenType<class ClipboardSequenceNumberTokenTypeMarker>;

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_CLIPBOARD_SEQUENCE_NUMBER_TOKEN_H_
