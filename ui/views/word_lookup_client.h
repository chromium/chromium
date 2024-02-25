// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WORD_LOOKUP_CLIENT_H_
#define UI_VIEWS_WORD_LOOKUP_CLIENT_H_

#include "ui/views/views_export.h"

namespace gfx {
struct DecoratedText;
class Point;
}  // namespace gfx

namespace views {

// An interface implemented by a view which supports word lookups.
class VIEWS_EXPORT WordLookupClient {
 public:
  // Retrieves the word displayed at the given |point| along with its styling
  // information. |point| is in the coordinate system of the view. If no word is
  // displayed at the point, returns a nearby word. |rect| should correspond to
  // the space used by the leftmost glyph of the |word| in the view's
  // coordinates. Returns false, if no word can be retrieved.
  virtual bool GetWordLookupDataAtPoint(const gfx::Point& point,
                                        gfx::DecoratedText* decorated_word,
                                        gfx::Rect* rect) = 0;

  virtual bool GetWordLookupDataFromSelection(
      gfx::DecoratedText* decorated_text,
      gfx::Rect* rect) = 0;

 protected:
  virtual ~WordLookupClient() = default;
};

}  // namespace views

#endif  // UI_VIEWS_WORD_LOOKUP_CLIENT_H_
