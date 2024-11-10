// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_DECORATED_TEXT_MAC_H_
#define UI_GFX_DECORATED_TEXT_MAC_H_

#include "base/component_export.h"

@class NSAttributedString;

namespace gfx {

struct DecoratedText;

// Returns a NSAttributedString from |decorated_text|.
COMPONENT_EXPORT(GFX)
NSAttributedString* GetAttributedStringFromDecoratedText(
    const DecoratedText& decorated_text);

}  // namespace gfx

#endif  // UI_GFX_DECORATED_TEXT_MAC_H_