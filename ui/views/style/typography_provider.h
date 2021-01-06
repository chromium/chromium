// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_STYLE_TYPOGRAPHY_PROVIDER_H_
#define UI_VIEWS_STYLE_TYPOGRAPHY_PROVIDER_H_

#include "base/macros.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/font.h"
#include "ui/views/views_export.h"

namespace gfx {
class FontList;
}

namespace views {

class View;

// Provides a default provider of fonts to use in toolkit-views UI.
class VIEWS_EXPORT TypographyProvider {
 public:
  TypographyProvider() = default;
  virtual ~TypographyProvider() = default;

  // Gets the FontDetails for the given |context| and |style|.
  virtual ui::ResourceBundle::FontDetails GetFontDetails(int context,
                                                         int style) const;

  // Convenience wrapper that gets a FontList for |context| and |style|.
  const gfx::FontList& GetFont(int context, int style) const;

  // Gets the color for the given |context| and |style|. |view| is the View
  // requesting the color.
  virtual SkColor GetColor(const views::View& view,
                           int context,
                           int style) const;

  // Gets the line spacing.  By default this is the font height.
  virtual int GetLineHeight(int context, int style) const;

  // Returns the weight that will result in the ResourceBundle returning an
  // appropriate "medium" weight for UI. This caters for systems that are known
  // to be unable to provide a system font with weight other than NORMAL or BOLD
  // and for user configurations where the NORMAL font is already BOLD. In both
  // of these cases, NORMAL is returned instead.
  static gfx::Font::Weight MediumWeightForUI();

 private:
  DISALLOW_COPY_AND_ASSIGN(TypographyProvider);
};

}  // namespace views

#endif  // UI_VIEWS_STYLE_TYPOGRAPHY_PROVIDER_H_
