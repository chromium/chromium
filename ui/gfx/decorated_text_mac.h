// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_DECORATED_TEXT_MAC_H_
#define UI_GFX_DECORATED_TEXT_MAC_H_

#include "ui/gfx/gfx_export.h"

@class NSAttributedString;

namespace gfx {

struct DecoratedText;

// Returns a NSAttributedString from |decorated_text|.
GFX_EXPORT NSAttributedString* GetAttributedStringFromDecoratedText(
    const DecoratedText& decorated_text);

}  // namespace gfx

#endif  // UI_GFX_DECORATED_TEXT_MAC_H_